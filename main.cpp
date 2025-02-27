
#include "Debugger.hpp"
#include "Process.hpp"
#include "Thread.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <thread>

void tracee() {
#if 1
	for (int i = 0; i < 5; ++i) {
		auto thr = std::thread([](int n) {
			while (1) {
				printf("In child, doing some work [%d]...\n", n);
				std::this_thread::sleep_for(std::chrono::seconds(1));
			}
		},
							   i);

		thr.detach();
	}
#endif
	while (1) {
		printf("In child, doing some work [%d]...\n", -1);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void tracer(pid_t cpid) {
	auto debugger = std::make_unique<Debugger>();
	auto process  = debugger->attach(cpid);
	process->resume();

	std::this_thread::sleep_for(std::chrono::seconds(5));
	process->stop();

	if (process->next_debug_event(std::chrono::seconds(10))) {
		printf("Stop Complete!\n");
	} else {
		printf("Timeout!\n");
		exit(0);
	}

	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
	// printf("Killing Process\n");
	// process->kill();
}

int main() {

	switch (const pid_t cpid = fork()) {
	case 0:
		tracee();
		break;
	case -1:
		perror("fork");
		exit(1);
	default:
		tracer(cpid);
		break;
	}
}
