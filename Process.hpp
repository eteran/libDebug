
#ifndef PROCESS_HPP_
#define PROCESS_HPP_

#include <chrono>
#include <map>
#include <memory>

#include <sys/types.h>

class Thread;

class Process {
	friend class Debugger;
	friend class Thread;

public:
	Process(pid_t pid);

public:
	pid_t pid() const { return pid_; }

public:
	void read_memory(uint64_t address, void *buffer, size_t n);
	void write_memory(uint64_t address, const void *buffer, size_t n);

public:
	void kill();
	void step();
	void resume();
	void stop();
	void detach();
	bool wait();

public:
	static bool wait_for_sigchild(std::chrono::milliseconds timeout);

public:
	std::map<pid_t, std::shared_ptr<Thread>> threads() const { return threads_; }

private:
	pid_t pid_ = 0;
	std::map<pid_t, std::shared_ptr<Thread>> threads_;
};

#endif
