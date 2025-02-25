
#include "Debugger.hpp"
#include "Process.hpp"
#include "Thread.hpp"

#include <signal.h>
#include <sys/wait.h>
#include <thread>

void tracee() {
	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		printf("In child, doing some work...\n");
	}
}

void tracer(pid_t cpid) {
	Debugger debugger;
	auto process = debugger.attach(cpid);
	process->resume();

	std::this_thread::sleep_for(std::chrono::seconds(3));
	process->stop();

	if (!Process::wait_for_sigchild(std::chrono::seconds(5))) {
		printf("Timeout!\n");
	} else {
		process->wait();
		printf("Stop Complete!\n");
	}

	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
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
