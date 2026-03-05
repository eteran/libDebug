#include "Debug/Event.hpp"
#include "Debug/EventStatus.hpp"
#include "Test.hpp"
#include "TestHelpers.hpp"

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstring>
#include <pthread.h>
#include <sys/wait.h>
#include <thread>
#include <unistd.h>

using namespace std::chrono_literals;

static void *sleeper(void *) {
	std::this_thread::sleep_for(2s);
	return nullptr;
}

TEST(CloneEvent) {
	with_attached_child_sync(
		[](int sync_read_fd) {
			char buf;
			if (read(sync_read_fd, &buf, 1) != 1) {
				_exit(1);
			}
			std::this_thread::sleep_for(50ms);

			pthread_t thread;
			if (pthread_create(&thread, nullptr, &sleeper, nullptr) != 0) {
				_exit(1);
			}

			pthread_join(thread, nullptr);
			_exit(0);
		},
		[](const AttachedChildContext &ctx) {
			ctx.process->resume();

			ctx.process->next_debug_event(500ms, [&](const Event &e) {
				(void)e;
				return EventStatus::Continue;
			});

			notify_child_start(ctx.sync_write_fd);

			bool saw_clone   = false;
			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				1s,
				[&](const Event &e) {
					if (e.type == Event::Type::Clone) {
						saw_clone = true;
						return EventStatus::Stop;
					}
					return EventStatus::Continue;
				},
				[&]() { return saw_clone; });

			CHECK_MSG(observed, "Expected clone/fork event to be reported to callback");

			ctx.process->detach();
			kill(ctx.child_pid, SIGKILL);
			waitpid(ctx.child_pid, nullptr, 0);
		});
}

TEST(ExitTraceEvent) {
	with_attached_child_sync(
		[](int sync_read_fd) {
			char buf;
			if (read(sync_read_fd, &buf, 1) != 1) {
				_exit(1);
			}
			std::this_thread::sleep_for(50ms);
			_exit(77);
		},
		[](const AttachedChildContext &ctx) {
			ctx.process->resume();

			ctx.process->next_debug_event(500ms, [&](const Event &e) {
				(void)e;
				return EventStatus::Continue;
			});

			notify_child_start(ctx.sync_write_fd);

			bool saw_exit    = false;
			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				1s,
				[&](const Event &e) {
					if ((e.type == Event::Type::Exited || e.type == Event::Type::Terminated) && e.tid == ctx.child_pid) {
						saw_exit = true;
						return EventStatus::Stop;
					}
					return EventStatus::Continue;
				},
				[&]() { return saw_exit; });

			CHECK_MSG(observed, "Expected to observe child exit/termination event");

			ctx.process->detach();
			waitpid(ctx.child_pid, nullptr, 0);
		});
}
