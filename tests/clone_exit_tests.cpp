#include "Debug/Debugger.hpp"
#include "Debug/Event.hpp"
#include "Debug/EventStatus.hpp"
#include "Debug/Process.hpp"
#include "Test.hpp"

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
	Debugger dbg;
	int syncfd[2];
	if (pipe(syncfd) != 0) {
		_exit(1);
	}

	pid_t child = fork();
	if (child == 0) {
		// Child: wait on the pipe until parent signals, then create a thread
		close(syncfd[1]);
		char buf;
		if (read(syncfd[0], &buf, 1) != 1) {
			_exit(1);
		}
		// small delay to allow tracer to finish setup
		std::this_thread::sleep_for(50ms);
		close(syncfd[0]);

		pthread_t t;
		if (pthread_create(&t, nullptr, &sleeper, nullptr) != 0) {
			_exit(1);
		}

		// Wait for the thread to finish
		pthread_join(t, nullptr);
		_exit(0);
	}

	// Parent: signal child to create the thread after attaching/resuming
	close(syncfd[0]);
	auto proc = dbg.attach(child);

	proc->resume();

	// Give the Process a chance to process the initial attach stop and set ptrace options
	proc->next_debug_event(500ms, [&](const Event &e) {
		(void)e;
		return EventStatus::Continue;
	});

	// allow child to proceed and create the thread
	if (write(syncfd[1], "x", 1) != 1) {
		// best-effort; continue test
	}
	close(syncfd[1]);

	// Wait for the clone event reported to the callback
	bool saw_clone   = false;
	const auto start = std::chrono::steady_clock::now();
	while (std::chrono::steady_clock::now() - start < 5s) {
		proc->next_debug_event(1s, [&](const Event &e) {
			if (e.type == Event::Type::Clone) {
				saw_clone = true;
				return EventStatus::Stop;
			}
			return EventStatus::Continue;
		});

		if (saw_clone) break;
	}

	CHECK_MSG(saw_clone, "Expected clone/fork event to be reported to callback");

	// Cleanup
	proc->detach();
	kill(child, SIGKILL);
	waitpid(child, nullptr, 0);
}

TEST(ExitTraceEvent) {
	Debugger dbg;
	int syncfd[2];
	if (pipe(syncfd) != 0) {
		_exit(1);
	}

	pid_t child = fork();
	if (child == 0) {
		// Child: wait for parent to signal, then exit with a specific status
		close(syncfd[1]);
		char buf;
		if (read(syncfd[0], &buf, 1) != 1) {
			_exit(1);
		}
		close(syncfd[0]);
		// slight delay to ensure tracer is ready to observe exit
		std::this_thread::sleep_for(50ms);
		_exit(77);
	}

	// Parent: attach/resume then tell child to exit
	close(syncfd[0]);
	auto proc = dbg.attach(child);
	proc->resume();

	// Give the Process a chance to process the initial attach stop and set ptrace options
	proc->next_debug_event(500ms, [&](const Event &e) {
		(void)e;
		return EventStatus::Continue;
	});

		if (write(syncfd[1], "x", 1) != 1) {
			// best-effort; continue
		}
	close(syncfd[1]);

	bool saw_exit    = false;
	const auto start = std::chrono::steady_clock::now();
	while (std::chrono::steady_clock::now() - start < 5s) {
		proc->next_debug_event(1s, [&](const Event &e) {
			if ((e.type == Event::Type::Exited || e.type == Event::Type::Terminated) && e.tid == child) {
				saw_exit = true;
				return EventStatus::Stop;
			}
			return EventStatus::Continue;
		});
		if (saw_exit) break;
	}

	CHECK_MSG(saw_exit, "Expected to observe child exit/termination event");

	// Cleanup
	proc->detach();
	// child might already be reaped by the event loop; ensure it's dead
	waitpid(child, nullptr, 0);
}
