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

void run_fault_signal_case(const FaultSignalCase &test_case) {
	with_attached_child_sync(
		[&](int sync_read_fd) {
			char ready;
			if (read(sync_read_fd, &ready, 1) != 1) {
				_exit(1);
			}

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
		child_run_basic_executable_buffer,
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
		child_run_basic_executable_buffer,
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
		child_run_basic_executable_buffer,
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
