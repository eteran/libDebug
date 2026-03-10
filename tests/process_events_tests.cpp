#include "Debug/Event.hpp"
#include "Debug/EventStatus.hpp"
#include "Test.hpp"
#include "TestHelpers.hpp"

#include <chrono>
#include <csignal>
#include <sys/wait.h>
#include <thread>

using namespace std::chrono_literals;

TEST(IgnoreSignal) {
	with_attached_child(
		[](int addr_write_fd, int sync_read_fd) {
			send_dummy_address(addr_write_fd);
			child_wait_ready(sync_read_fd);

			std::this_thread::sleep_for(500ms);
			_exit(42);
		},
		[](const AttachedContext &ctx) {
			ctx.process->resume();
			ctx.process->ignore_signal(SIGUSR1);

			notify_child_start(ctx.sync_write_fd);
			kill(ctx.child_pid, SIGUSR1);

			bool callback_called = false;
			auto cb              = [&](const Event &e) -> EventStatus {
                (void)e;
                callback_called = true;
                return EventStatus::Stop;
			};

			ctx.process->next_debug_event(500ms, cb);
			CHECK_MSG(!callback_called, "Ignored signal should not trigger event callback");

			ctx.process->detach();
			kill(ctx.child_pid, SIGKILL);
			waitpid(ctx.child_pid, nullptr, 0);
		});
}

TEST(SigstopNotRedeliveredAfterStop) {
	with_attached_child(
		[](int addr_write_fd, int sync_read_fd) {
			send_dummy_address(addr_write_fd);
			child_wait_ready(sync_read_fd);

			std::this_thread::sleep_for(300ms);
			_exit(0);
		},
		[](const AttachedContext &ctx) {
			ctx.process->resume();
			notify_child_start(ctx.sync_write_fd);

			CHECK_MSG(kill(ctx.child_pid, SIGSTOP) == 0, "failed to send SIGSTOP to child");

			bool first_sigstop_seen = false;
			auto first_cb           = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGSTOP) {
					first_sigstop_seen = true;
					return EventStatus::Stop;
				}

				return EventStatus::Continue;
			};

			const bool first_observed = poll_debug_events_until(
				ctx.process,
				3s,
				std::chrono::milliseconds(100),
				first_cb,
				[&]() { return first_sigstop_seen; });
			CHECK_MSG(first_observed, "did not observe initial SIGSTOP event");

			ctx.process->resume();

			bool second_sigstop_seen = false;
			bool exited              = false;
			auto second_cb           = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGSTOP) {
					second_sigstop_seen = true;
					return EventStatus::Continue;
				}

				if (e.type == Event::Type::Exited || e.type == Event::Type::Terminated) {
					exited = true;
					return EventStatus::Continue;
				}

				return EventStatus::Continue;
			};

			const bool exited_observed = poll_debug_events_until(
				ctx.process,
				3s,
				std::chrono::milliseconds(100),
				second_cb,
				[&]() { return exited; });

			CHECK_MSG(exited_observed, "did not observe child exit after resuming from SIGSTOP");
			CHECK_MSG(!second_sigstop_seen, "SIGSTOP was unexpectedly re-delivered after resume");

			ctx.process->wait();
		});
}
