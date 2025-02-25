
#include "Process.hpp"
#include "Proc.hpp"
#include "Thread.hpp"

#include <cerrno>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include <dirent.h>
#include <sys/ptrace.h>
#include <sys/wait.h>

namespace {

timespec duration_to_timespec(std::chrono::milliseconds msecs) {
	struct timespec ts;
	ts.tv_sec  = (msecs.count() / 1000);
	ts.tv_nsec = (msecs.count() % 1000) * 1000000;
	return ts;
}

}

/**
 * @brief Construct a new Process object
 *
 * @param pid
 */
Process::Process(pid_t pid)
	: pid_(pid) {

	while (true) {

		const std::vector<pid_t> threads = enumerate_threads(pid);

		bool inserted = false;
		for (const pid_t &tid : threads) {
			auto it = threads_.find(tid);
			if (it != threads_.end()) {
				continue;
			}

			long ret = ::ptrace(PTRACE_ATTACH, tid, 0L, 0L);
			if (ret == -1) {
				perror("ptrace");
				exit(0);
			}

			auto new_thread = std::make_shared<Thread>(tid);
			new_thread->wait();

			threads_.emplace(tid, new_thread);
			inserted = true;
		}

		if (!inserted) {
			break;
		}
	}

	for (auto [tid, thread] : threads_) {

		if (thread->is_exited()) {
			printf("Thread: %d [EXITED] [%d]\n", thread->tid(), thread->exit_status());
		}

		if (thread->is_signaled()) {
			printf("Thread: %d [SIGNALED] [%d]\n", thread->tid(), thread->signal_status());
		}

		if (thread->is_stopped()) {
			printf("Thread: %d [STOPPED] [%d]\n", thread->tid(), thread->stop_status());
		}

		if (thread->is_continued()) {
			printf("Thread: %d [CONTINUED]\n", thread->tid());
		}
	}
}

/**
 * @brief
 *
 * @param address
 * @param buffer
 * @param n
 */
void Process::read_memory(uint64_t address, void *buffer, size_t n) {
	(void)address;
	(void)buffer;
	(void)n;
}

/**
 * @brief
 *
 * @param address
 * @param buffer
 * @param n
 */
void Process::write_memory(uint64_t address, const void *buffer, size_t n) {
	(void)address;
	(void)buffer;
	(void)n;
}

/**
 * @brief
 *
 */
void Process::step() {
}

/**
 * @brief
 *
 */
void Process::resume() {
	printf("Resuming All Threads\n");
	for (auto [tid, thread] : threads_) {
		thread->resume();
	}
}

/**
 * @brief
 *
 */
void Process::stop() {

	printf("Stopping A Single Thread\n");

	// TODO(eteran): pick the thread more intelligently
	for (auto [tid, thread] : threads_) {
		thread->stop();
		break;
	}
}

/**
 * @brief
 *
 */
void Process::kill() {
}

/**
 * @brief
 *
 */
void Process::detach() {
}

/**
 * @brief
 *
 * @param timeout
 * @return true
 * @return false
 */
bool Process::wait() {

	while (true) {
		int wstatus = 0;
		long ret    = ::waitpid(-1, &wstatus, __WALL | WNOHANG);

		printf("Thread Stopped [%ld]!\n", ret);

		if (ret == -1) {
			perror("waitpid");
			exit(0);
		}

		if (ret == 0) {
			// return false;
			std::this_thread::sleep_for(std::chrono::milliseconds(10));
			continue;
		}

		return true;
	}

	return true;
}

/**
 * @brief
 *
 * @param timeout
 * @return true
 * @return false
 */
bool Process::wait_for_sigchild(std::chrono::milliseconds timeout) {

	const struct timespec ts = duration_to_timespec(timeout);
	siginfo_t info;
	sigset_t mask;
	sigemptyset(&mask);
	sigaddset(&mask, SIGCHLD);
	sigprocmask(SIG_BLOCK, &mask, nullptr);

	const int ret = ::sigtimedwait(&mask, &info, &ts);
	return ret == SIGCHLD;
}
