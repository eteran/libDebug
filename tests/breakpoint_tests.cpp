#include "Debug/Breakpoint.hpp"
#include "Debug/Event.hpp"
#include "Debug/EventStatus.hpp"
#include "Debug/Thread.hpp"
#include "Test.hpp"
#include "TestHelpers.hpp"

#include <array>
#include <sys/wait.h>

using namespace std::chrono_literals;

namespace {

struct AltBreakpointCase {
	Breakpoint::TypeId type;
	const char *name;
	int expected_signal;
	int alternate_expected_signal;
};

struct FaultSignalCase {
	int signal;
	const char *failure_message;
	void (*child_trigger)();
};

void trigger_sigsegv_fault() {
	volatile int *ptr = nullptr;
	*ptr              = 1;
	_exit(1);
}

void trigger_sigill_fault() {
	raise(SIGILL);
	_exit(1);
}

void trigger_sigfpe_fault() {
	raise(SIGFPE);
	_exit(1);
}

/**
 * @brief Maps an executable memory page, writes a simple function that returns immediately, and returns the address of the mapped page.
 */
uint8_t *map_executable_page() {
	const size_t page = 4096;
	void *mem         = mmap(nullptr, page, PROT_READ | PROT_WRITE | PROT_EXEC, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
	CHECK_MSG(mem != MAP_FAILED, "mmap failed to allocate executable page");

	auto code = static_cast<uint8_t *>(mem);
	code[0]   = 0x90; // NOP
	code[1]   = 0x90; // NOP
	code[2]   = 0xc3; // RET

	return code;
}

template <size_t Count = 1>
void child_run_executable_buffer(int addr_write_fd, int sync_read_fd) {

	uint8_t *code = map_executable_page();

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wuseless-cast"
	auto addr = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(code));
#pragma GCC diagnostic pop

	ssize_t wrote = write(addr_write_fd, &addr, sizeof(addr));
	if (wrote != static_cast<ssize_t>(sizeof(addr))) {
		_exit(1);
	}

	child_wait_ready(sync_read_fd);

	int (*fn)() = reinterpret_cast<int (*)()>(code);
	for (size_t i = 0; i < Count; ++i) {
		fn();
	}
	_exit(0);
}

void run_fault_signal_case(const FaultSignalCase &test_case) {
	with_attached_child_sync(
		[&](int sync_read_fd) {
			child_wait_ready(sync_read_fd);

			test_case.child_trigger();
			_exit(1);
		},
		[&](const AttachedChildContext &ctx) {
			ctx.process->resume();
			notify_child_start(ctx.sync_write_fd);

			bool observed_fault = false;
			bool signal_ok      = false;

			auto cb = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Fault) {
					observed_fault = true;
					signal_ok      = (e.siginfo.si_signo == test_case.signal);
					return EventStatus::Stop;
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return observed_fault; });

			CHECK_MSG(observed, test_case.failure_message);
			CHECK_MSG(signal_ok, test_case.failure_message);

			ctx.process->kill();
			ctx.process->wait();
		},
		true);
}

void run_alt_breakpoint_case(const AltBreakpointCase &test_case) {
	with_attached_child_with_address(
		child_run_executable_buffer,
		[&](const AttachedChildAddressContext &ctx) {
			ctx.process->add_breakpoint(ctx.address, test_case.type);

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			bool observed_stop = false;
			bool signal_ok     = false;

			auto cb = [&](const Event &e) -> EventStatus {
				if (e.type == Event::Type::Stopped && e.tid == ctx.child_pid) {
					observed_stop = true;
					if (e.siginfo.si_signo == test_case.expected_signal ||
						(test_case.alternate_expected_signal != 0 && e.siginfo.si_signo == test_case.alternate_expected_signal)) {
						signal_ok = true;
					}
					return EventStatus::Stop;
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return observed_stop; });

			CHECK_MSG(observed, test_case.name);
			CHECK_MSG(signal_ok, test_case.name);

			ctx.process->kill();
			ctx.process->wait();
		},
		true);
}

}

TEST(BreakpointHit) {
	with_attached_child_with_address(
		child_run_executable_buffer,
		[](const AttachedChildAddressContext &ctx) {
			ctx.process->add_breakpoint(ctx.address);
			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			bool hit = false;
			auto cb  = [&](const Event &e) -> EventStatus {
                if (e.type == Event::Type::Stopped) {
                    hit = true;
                }
                return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return hit; });

			CHECK_MSG(observed, "did not observe expected breakpoint event");
			ctx.process->kill();
			ctx.process->wait();
		});
}

TEST(DisabledBreakpointDoesNotStop) {
	with_attached_child_with_address(
		child_run_executable_buffer,
		[](const AttachedChildAddressContext &ctx) {
			ctx.process->add_breakpoint(ctx.address);

			auto bp = ctx.process->find_breakpoint(ctx.address);
			CHECK_MSG(bp != nullptr, "failed to find breakpoint after add_breakpoint");
			bp->disable();

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			bool observed_stop = false;
			bool observed_exit = false;

			auto cb = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Stopped || e.type == Event::Type::Fault) {
					observed_stop = true;
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Exited || e.type == Event::Type::Terminated) {
					observed_exit = true;
					return EventStatus::Continue;
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return observed_exit; });

			CHECK_MSG(observed, "did not observe child exit with disabled breakpoint");
			CHECK_MSG(!observed_stop, "disabled breakpoint unexpectedly stopped child");
		});
}

TEST(EnabledBreakpointCanBeSteppedOverThenHitAgain) {
	with_attached_child_with_address(
		child_run_executable_buffer<3>,
		[](const AttachedChildAddressContext &ctx) {
			ctx.process->add_breakpoint(ctx.address);

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			bool first_breakpoint_stop = false;
			auto wait_first_cb         = [&](const Event &e) -> EventStatus {
                if (e.tid != ctx.child_pid) {
                    return EventStatus::Continue;
                }

                if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
                    first_breakpoint_stop = true;
                    return EventStatus::Stop;
                }

                return EventStatus::Continue;
			};

			const bool first_observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				wait_first_cb,
				[&]() { return first_breakpoint_stop; });

			CHECK_MSG(first_observed, "did not observe first breakpoint stop before step");

			ctx.process->step();

			bool second_breakpoint_stop = false;
			bool observed_exit          = false;

			auto wait_second_cb = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Exited || e.type == Event::Type::Terminated) {
					observed_exit = true;
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
					auto thread = ctx.process->find_thread(e.tid);
					if (!thread) {
						return EventStatus::Continue;
					}

					if (thread->get_instruction_pointer() == ctx.address) {
						second_breakpoint_stop = true;
						return EventStatus::Stop;
					}

					return EventStatus::Continue;
				}

				return EventStatus::Continue;
			};

			const bool second_observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				wait_second_cb,
				[&]() { return second_breakpoint_stop || observed_exit; });

			CHECK_MSG(second_observed, "did not observe events after stepping over breakpoint");
			CHECK_MSG(second_breakpoint_stop, "did not hit enabled breakpoint again after step-over");

			ctx.process->kill();
			ctx.process->wait();
		});
}

TEST(EnabledBreakpointCanBeContinuedFromThenHitAgain) {
	with_attached_child_with_address(
		child_run_executable_buffer<3>,
		[](const AttachedChildAddressContext &ctx) {
			ctx.process->add_breakpoint(ctx.address);

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			int breakpoint_stops = 0;
			bool observed_exit   = false;

			auto cb = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Exited || e.type == Event::Type::Terminated) {
					observed_exit = true;
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
					++breakpoint_stops;
					if (breakpoint_stops == 1) {
						return EventStatus::Continue;
					}
					if (breakpoint_stops >= 2) {
						return EventStatus::Stop;
					}
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return breakpoint_stops >= 2 || observed_exit; });

			CHECK_MSG(observed, "did not observe events after continuing from breakpoint");
			CHECK_MSG(breakpoint_stops >= 2, "did not hit enabled breakpoint again after continue");

			ctx.process->kill();
			ctx.process->wait();
		});
}

TEST(SteppingOnDisabledBreakpointDoesNotRearmIt) {
	with_attached_child_with_address(
		child_run_executable_buffer<3>,
		[](const AttachedChildAddressContext &ctx) {
			ctx.process->add_breakpoint(ctx.address);

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			bool first_breakpoint_stop = false;
			auto wait_first_cb         = [&](const Event &e) -> EventStatus {
                if (e.tid != ctx.child_pid) {
                    return EventStatus::Continue;
                }

                if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
                    first_breakpoint_stop = true;
                    return EventStatus::Stop;
                }

                return EventStatus::Continue;
			};

			const bool first_observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				wait_first_cb,
				[&]() { return first_breakpoint_stop; });

			CHECK_MSG(first_observed, "did not observe first breakpoint stop before disabling");

			auto bp = ctx.process->find_breakpoint(ctx.address);
			CHECK_MSG(bp != nullptr, "failed to find breakpoint before disabling");
			bp->disable();

			ctx.process->step();

			bool rehit_disabled_breakpoint = false;
			bool observed_exit             = false;

			auto cb = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Exited || e.type == Event::Type::Terminated) {
					observed_exit = true;
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
					auto thread = ctx.process->find_thread(e.tid);
					if (thread && thread->get_instruction_pointer() == ctx.address) {
						rehit_disabled_breakpoint = true;
						return EventStatus::Stop;
					}
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return rehit_disabled_breakpoint || observed_exit; });

			CHECK_MSG(observed, "did not observe events after stepping disabled breakpoint");
			CHECK_MSG(!rehit_disabled_breakpoint, "disabled breakpoint was re-armed and hit again after step");
			CHECK_MSG(observed_exit, "child did not terminate after stepping over disabled breakpoint");
		});
}

TEST(ResumingOnDisabledBreakpointDoesNotRearmIt) {
	with_attached_child_with_address(
		child_run_executable_buffer<3>,
		[](const AttachedChildAddressContext &ctx) {
			ctx.process->add_breakpoint(ctx.address);

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			bool first_breakpoint_stop = false;
			auto wait_first_cb         = [&](const Event &e) -> EventStatus {
                if (e.tid != ctx.child_pid) {
                    return EventStatus::Continue;
                }

                if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
                    first_breakpoint_stop = true;
                    return EventStatus::Stop;
                }

                return EventStatus::Continue;
			};

			const bool first_observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				wait_first_cb,
				[&]() { return first_breakpoint_stop; });

			CHECK_MSG(first_observed, "did not observe first breakpoint stop before disabling");

			auto bp = ctx.process->find_breakpoint(ctx.address);
			CHECK_MSG(bp != nullptr, "failed to find breakpoint before disabling");
			bp->disable();

			ctx.process->resume();

			bool rehit_disabled_breakpoint = false;
			bool observed_exit             = false;

			auto cb = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Exited || e.type == Event::Type::Terminated) {
					observed_exit = true;
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
					auto thread = ctx.process->find_thread(e.tid);
					if (thread && thread->get_instruction_pointer() == ctx.address) {
						rehit_disabled_breakpoint = true;
						return EventStatus::Stop;
					}
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return rehit_disabled_breakpoint || observed_exit; });

			CHECK_MSG(observed, "did not observe events after resuming from disabled breakpoint");
			CHECK_MSG(!rehit_disabled_breakpoint, "disabled breakpoint was re-armed and hit again after resume");
			CHECK_MSG(observed_exit, "child did not terminate after resuming from disabled breakpoint");
		});
}

TEST(AltBreakpointTypesReported) {
	constexpr std::array<AltBreakpointCase, 4> TestCases = {
		AltBreakpointCase{Breakpoint::TypeId::INT1, "INT1 did not report expected SIGTRAP", SIGTRAP, 0},
		AltBreakpointCase{Breakpoint::TypeId::UD2, "UD2 did not report expected SIGILL", SIGILL, 0},
		AltBreakpointCase{Breakpoint::TypeId::UD0, "UD0 did not report expected SIGILL", SIGILL, 0},
		AltBreakpointCase{Breakpoint::TypeId::HLT, "HLT did not report expected SIGSEGV/SIGILL", SIGSEGV, SIGILL},
	};

	for (auto const &test_case : TestCases) {
		run_alt_breakpoint_case(test_case);
	}
}

TEST(HardwareExecuteBreakpointHit) {
	with_attached_child_with_address(
		child_run_executable_buffer,
		[](const AttachedChildAddressContext &ctx) {
			auto thread = ctx.process->active_thread();
			CHECK_MSG(thread != nullptr, "proc->active_thread() returned null");

			Context context;
			thread->get_context(&context);
			CHECK_MSG(context.is_set(), "thread->get_context did not populate context");

			context[RegisterId::DR0] = ctx.address;
			context[RegisterId::DR6] = static_cast<uint64_t>(0);

			uint64_t dr7 = context[RegisterId::DR7].as<uint64_t>();
			dr7 &= ~(0xfULL << 16); // RW0/LEN0 for bp0: execute, len=1
			dr7 |= 0x1ULL;          // local enable for bp0
			context[RegisterId::DR7] = dr7;

			thread->set_context(&context);

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();

			bool observed_stop    = false;
			bool observed_hwbkpt  = false;
			bool observed_sigtrap = false;

			auto cb = [&](const Event &e) -> EventStatus {
				if (e.type == Event::Type::Stopped && e.tid == ctx.child_pid) {
					observed_stop    = true;
					observed_sigtrap = (e.siginfo.si_signo == SIGTRAP);
#ifdef TRAP_HWBKPT
					observed_hwbkpt = (e.siginfo.si_code == TRAP_HWBKPT);
#endif
					return EventStatus::Stop;
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				cb,
				[&]() { return observed_stop; });

			CHECK_MSG(observed, "did not observe stop for hardware breakpoint");
			CHECK_MSG(observed_sigtrap, "hardware breakpoint did not report SIGTRAP");
#ifdef TRAP_HWBKPT
			CHECK_MSG(observed_hwbkpt, "hardware breakpoint did not report TRAP_HWBKPT");
#endif

			ctx.process->kill();
			ctx.process->wait();
		});
}

TEST(FaultSignalsReported) {
	constexpr std::array<FaultSignalCase, 3> TestCases = {
		FaultSignalCase{SIGSEGV, "did not observe expected SIGSEGV fault event", trigger_sigsegv_fault},
		FaultSignalCase{SIGILL, "did not observe expected SIGILL fault event", trigger_sigill_fault},
		FaultSignalCase{SIGFPE, "did not observe expected SIGFPE fault event", trigger_sigfpe_fault},
	};

	for (const auto &test_case : TestCases) {
		run_fault_signal_case(test_case);
	}
}
