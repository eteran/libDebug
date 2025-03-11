
#ifndef PROCESS_HPP_
#define PROCESS_HPP_

#include "Event.hpp"
#include "EventStatus.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>

#include <sys/types.h>

class Thread;
class Breakpoint;

class Process {
	friend class Debugger;
	friend class Thread;

public:
	using event_callback = std::function<EventStatus(const Event &)>;

public:
	using Flag                               = uint32_t;
	static constexpr Flag Attach             = 0;
	static constexpr Flag NoAttach           = 1;
	static constexpr Flag KillOnTracerExit   = 2;
	static constexpr Flag DisableAslr        = 4;
	static constexpr Flag DisableLazyBinding = 8;

public:
	Process(pid_t pid, Flag flags);
	~Process();

public:
	pid_t pid() const {
		return pid_;
	}

public:
	// memory APIs
	int64_t read_memory(uint64_t address, void *buffer, size_t n) const;
	int64_t write_memory(uint64_t address, const void *buffer, size_t n) const;

public:
	// event API
	void kill();
	void step();
	void resume();
	void stop();
	void detach();
	bool next_debug_event(std::chrono::milliseconds timeout, event_callback callback);

public:
	// breakpoint API
	void add_breakpoint(uint64_t address);
	void remove_breakpoint(uint64_t address);
	std::shared_ptr<Breakpoint> find_breakpoint(uint64_t address) const;

public:
	void report() const;

public:
	std::shared_ptr<Thread> find_thread(pid_t tid) const;

	std::unordered_map<pid_t, std::shared_ptr<Thread>> threads() const {
		return threads_;
	}

	std::shared_ptr<Thread> active_thread() const {
		return active_thread_;
	}

private:
	int64_t read_memory_ptrace(uint64_t address, void *buffer, size_t n) const;
	int64_t write_memory_ptrace(uint64_t address, const void *buffer, size_t n) const;
	void filter_breakpoints(uint64_t address, void *buffer, size_t n) const;

private:
	pid_t pid_ = 0;
	int memfd_ = -1;
	std::shared_ptr<Thread> active_thread_;
	std::unordered_map<pid_t, std::shared_ptr<Thread>> threads_;
	std::unordered_map<uint64_t, std::shared_ptr<Breakpoint>> breakpoints_;
	uint64_t prev_memory_map_hash_ = 0;
};

#endif
