
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

public:
	siginfo_t siginfo = {};
	pid_t pid         = 0;
	pid_t tid         = 0;
	int status        = 0;
	Type type         = Type::Unknown;
};

#endif
