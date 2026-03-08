
#ifndef PROCESS_HPP_
#define PROCESS_HPP_

#include "Breakpoint.hpp"
#include "Event.hpp"
#include "EventStatus.hpp"

#include <chrono>
#include <functional>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <sys/types.h>

class Thread;
class Memory;

class Process {
	friend class Debugger;
	friend class Thread;

	struct internal_t {};

public:
	using event_callback = std::function<EventStatus(const Event &)>;

public:
	using Flag                               = uint32_t;
	static constexpr Flag Attach             = 1u << 0;
	static constexpr Flag NoAttach           = 1u << 1;
	static constexpr Flag KillOnTracerExit   = 1u << 2;
	static constexpr Flag DisableAslr        = 1u << 3;
	static constexpr Flag DisableLazyBinding = 1u << 4;

public:
	// The constructor is private to force users to create Process objects through the Debugger interface.
	Process(const internal_t &, pid_t pid, Flag flags);
	Process(const Process &)            = delete;
	Process &operator=(const Process &) = delete;
	~Process();

public:
	[[nodiscard]] pid_t pid() const {
		return pid_;
	}

public:
	// memory APIs
	int64_t read_memory(uint64_t address, void *buffer, size_t n) const;
	int64_t write_memory(uint64_t address, const void *buffer, size_t n) const;

public:
	// event API
	void kill() const;
	void step();
	void resume();
	void stop();
	void detach();
	bool next_debug_event(std::chrono::milliseconds timeout, event_callback callback);
	int wait() const;

public:
	// breakpoint API
	void add_breakpoint(uint64_t address, Breakpoint::TypeId type = Breakpoint::TypeId::Automatic);
	void remove_breakpoint(uint64_t address);
	[[nodiscard]] std::shared_ptr<Breakpoint> find_breakpoint(uint64_t address) const;
	[[nodiscard]] std::shared_ptr<Breakpoint> search_breakpoint(uint64_t address) const;

public:
	void ignore_signal(int signal);
	void unignore_signal(int signal);
	[[nodiscard]] bool is_ignoring_signal(int signal) const;

public:
	void report() const;

public:
	[[nodiscard]] std::shared_ptr<Thread> find_thread(pid_t tid) const;

	[[nodiscard]] const std::unordered_map<pid_t, std::shared_ptr<Thread>> &threads() const {
		return threads_;
	}

	[[nodiscard]] std::shared_ptr<Thread> active_thread() const {
		return active_thread_;
	}

private:
	struct EventContext {
		std::shared_ptr<Thread> current_thread;
		uint64_t address = 0;
		pid_t tid        = 0;
		int wstatus      = 0;
		bool first_stop  = false;
	};

	void handle_exit_event(EventContext &ctx, event_callback callback);
	void handle_exit_trace_event(EventContext &ctx, event_callback callback);
	void handle_clone_event(EventContext &ctx, event_callback callback);
	void handle_signal_event(EventContext &ctx, event_callback callback);
	void handle_continue_event(EventContext &ctx, event_callback callback);
	void handle_trap_event(EventContext &ctx, event_callback callback);
	bool handle_unknown_event(EventContext &ctx, event_callback callback);
	void handle_stop_event(EventContext &ctx, event_callback callback);
	void process_stop_event(EventContext &ctx, event_callback callback, Event::Type stop_type);
	std::vector<pid_t> stop_all_threads(pid_t except_tid);
	void all_stop_barrier(const std::vector<pid_t> &target_tids);

private:
	void filter_breakpoints(uint64_t address, void *buffer, size_t n) const;

private:
	pid_t pid_     = 0;
	bool all_stop_ = true;
	std::shared_ptr<Thread> active_thread_;
	std::unordered_map<pid_t, std::shared_ptr<Thread>> threads_;
	std::unordered_map<uint64_t, std::shared_ptr<Breakpoint>> breakpoints_;
	std::unordered_set<int> ignored_signals_;
	std::unique_ptr<Memory> memory_;
};

#endif
