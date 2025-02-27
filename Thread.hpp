
#ifndef THREAD_HPP_
#define THREAD_HPP_

#include <memory>
#include <sys/types.h>

class Thread {
	friend class Debugger;
	friend class Process;

	enum class State {
		Stopped,
		Running,
	};

	enum class Flags {
		NoAttach,
		Attach,
	};

public:
	Thread(pid_t pid, pid_t tid, Flags f);
	~Thread();

public:
	pid_t tid() const { return tid_; }
	int wait_status() const { return wstatus_; }

public:
	bool is_exited() const;
	bool is_signaled() const;
	bool is_stopped() const;
	bool is_continued() const;

public:
	int exit_status() const;
	int signal_status() const;
	int stop_status() const;

public:
	void kill();
	void step();
	void resume();
	void stop();
	void detach();
	void wait();

private:
	pid_t pid_   = 0;
	pid_t tid_   = 0;
	int wstatus_ = 0;
	State state_ = State::Running;
};

#endif
