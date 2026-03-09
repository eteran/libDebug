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
	with_attached_child_sync(
		[](int sync_read_fd) {
			child_wait_ready(sync_read_fd);
			std::this_thread::sleep_for(500ms);
			_exit(42);
		},
		[](const AttachedChildContext &ctx) {
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
