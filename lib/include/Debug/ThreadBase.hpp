
#ifndef THREAD_BASE_HPP_
#define THREAD_BASE_HPP_

#include "Debug/Process.hpp"
#include <csignal>
#include <optional>

class ThreadBase {
protected:
	enum class State {
		Stopped,
		Running,
	};

public:
	using Flag                             = uint32_t;
	static constexpr Flag Attach           = 1u << 0;
	static constexpr Flag NoAttach         = 1u << 1;
	static constexpr Flag KillOnTracerExit = 1u << 2;

public:
	ThreadBase(Process *process, pid_t tid);
	virtual ~ThreadBase() = default;

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
	void kill() const;
	void step();
	void resume();
	void resume_until_syscall();
	void step(int signal);
	void resume(int signal);
	void resume_until_syscall(int signal);
	void stop() const;
	void detach();
	void wait();

public:
	virtual uint64_t get_instruction_pointer() const        = 0;
	virtual void set_instruction_pointer(uint64_t ip) const = 0;

public:
	bool load_signal_info();

protected:
	void resume_internal(int signal, bool until_syscall);

protected:
	static long create_ptrace_options(Flag f);

protected:
	Process *process_                                = nullptr;
	pid_t tid_                                       = 0;
	int wstatus_                                     = 0;
	int pending_signal_                              = 0;
	std::optional<uint64_t> pending_step_breakpoint_ = {};
	State state_                                     = State::Running;
	bool is_64_bit_                                  = false;
	siginfo_t siginfo_                               = {};
};

#endif
