
#ifndef THREAD_INTEL_HPP_
#define THREAD_INTEL_HPP_

#include "Context.hpp"

#include <sys/types.h>

class Thread {
	friend class Debugger;
	friend class Process;

	enum class State {
		Stopped,
		Running,
	};

public:
	using Flag                             = uint32_t;
	static constexpr Flag Attach           = 0;
	static constexpr Flag NoAttach         = 1;
	static constexpr Flag KillOnTracerExit = 2;

public:
	Thread(pid_t pid, pid_t tid, Flag f);
	~Thread();

public:
	[[nodiscard]] pid_t tid() const { return tid_; }
	[[nodiscard]] int wait_status() const { return wstatus_; }

public:
	[[nodiscard]] bool is_exited() const;
	[[nodiscard]] bool is_signaled() const;
	[[nodiscard]] bool is_stopped() const;
	[[nodiscard]] bool is_continued() const;

public:
	[[nodiscard]] int exit_status() const;
	[[nodiscard]] int signal_status() const;
	[[nodiscard]] int stop_status() const;

public:
	void get_context(Context *ctx) const;
	void set_context(const Context *ctx) const;

	uint64_t get_instruction_pointer() const;
	void set_instruction_pointer(uint64_t ip) const;

public:
	void kill() const;
	void step();
	void resume();
	void stop() const;
	void detach();
	void wait();

private:
	[[nodiscard]] bool detect_64_bit() const;
	[[nodiscard]] uint32_t get_segment_base(Context *ctx, RegisterId reg) const;
	void get_debug_registers(Context *ctx) const;
	void get_debug_registers32(Context *ctx) const;
	void get_debug_registers64(Context *ctx) const;
	void get_registers(Context *ctx) const;
	void get_registers32(Context *ctx) const;
	void get_registers64(Context *ctx) const;
	void get_segment_bases(Context *ctx) const;
	void get_xstate(Context *ctx) const;
	void get_xstate32(Context *ctx) const;
	void get_xstate64(Context *ctx) const;
	void set_debug_registers(const Context *ctx) const;
	void set_debug_registers32(const Context *ctx) const;
	void set_debug_registers64(const Context *ctx) const;
	void set_registers(const Context *ctx) const;
	void set_registers32(const Context *ctx) const;
	void set_registers64(const Context *ctx) const;
	void set_xstate(const Context *ctx) const;
	void set_xstate32(const Context *ctx) const;
	void set_xstate64(const Context *ctx) const;

	int get_xstate32_modern(Context *ctx) const;
	int get_xstate32_legacy(Context *ctx) const;
	int get_xstate32_fallback(Context *ctx) const;
	int set_xstate32_modern(const Context *ctx) const;
	int set_xstate32_legacy(const Context *ctx) const;
	int set_xstate32_fallback(const Context *ctx) const;

private:
	pid_t pid_      = 0;
	pid_t tid_      = 0;
	int wstatus_    = 0;
	State state_    = State::Running;
	bool is_64_bit_ = false;
};

#endif
