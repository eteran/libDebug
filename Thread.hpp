
#ifndef THREAD_HPP_
#define THREAD_HPP_

#include "Context.hpp"

#include <memory>
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

public:
	void kill() const;
	void step();
	void resume();
	void stop() const;
	void detach();
	void wait();

private:
	pid_t pid_   = 0;
	pid_t tid_   = 0;
	int wstatus_ = 0;
	State state_ = State::Running;
};

#endif
