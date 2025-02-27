
#ifndef PROCESS_HPP_
#define PROCESS_HPP_

#include <chrono>
#include <map>
#include <memory>
#include <unordered_map>

#include <sys/types.h>

class Thread;

class Process {
	friend class Debugger;
	friend class Thread;

public:
	Process(pid_t pid);
	~Process();

public:
	pid_t pid() const { return pid_; }

public:
	int64_t read_memory(uint64_t address, void *buffer, size_t n);
	int64_t write_memory(uint64_t address, const void *buffer, size_t n);

public:
	void kill();
	void step();
	void resume();
	void stop();
	void detach();
	bool next_debug_event(std::chrono::milliseconds timeout);

public:
	void report() const;

public:
	std::unordered_map<pid_t, std::shared_ptr<Thread>> threads() const { return threads_; }

private:
	pid_t pid_ = 0;
	int memfd_ = -1;
	std::unordered_map<pid_t, std::shared_ptr<Thread>> threads_;
	std::shared_ptr<Thread> active_thread_;
};

#endif
