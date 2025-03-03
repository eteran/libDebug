
#ifndef EVENT_HPP_
#define EVENT_HPP_

#include <sys/types.h>

#include <csignal>

class Event {
public:
	enum Type {
		Exited,     // exited normally
		Terminated, // terminated by event
		Stopped,    // normal event
		Unknown,
	};

	enum Reason {
		Stepping,
		Breakpoint,
	};

public:
	siginfo_t siginfo_ = {};
	pid_t pid_         = 0;
	pid_t tid_         = 0;
	int status_        = 0;
};

#endif
