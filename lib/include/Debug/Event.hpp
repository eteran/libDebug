
#ifndef EVENT_HPP_
#define EVENT_HPP_

#include <sys/types.h>

#include <csignal>

struct Event {
	enum Type {
		Exited,     // exited normally
		Terminated, // terminated by event
		Stopped,    // normal event
		Clone,      // clone/fork/vfork event
		Unknown,
	};

	siginfo_t siginfo = {};
	pid_t pid         = 0;
	pid_t tid         = 0;
	int status        = 0;
	Type type         = Type::Unknown;
	pid_t new_tid     = 0; // for clone events, the TID of the new thread/process
};

#endif
