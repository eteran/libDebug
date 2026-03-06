
#ifndef EVENT_HPP_
#define EVENT_HPP_

#include <sys/types.h>

#include <csignal>

struct Event {
	enum class Type {
		Exited,     // exited normally
		Terminated, // terminated by event
		Stopped,    // normal event
		Fault,      // fault event, such as SIGSEGV, SIGFPE, etc.
		Clone,      // clone/fork/vfork event
		Unknown,
	};

	siginfo_t siginfo = {};            // signal info for signal events, empty for non-signal events
	pid_t pid         = 0;             // PID of the process that generated the event
	pid_t tid         = 0;             // TID of the thread that generated the event
	int status        = 0;             // wait status from waitpid, can be used to further inspect the event
	Type type         = Type::Unknown; // type of the event
	pid_t new_tid     = 0;             // for clone events, the TID of the new thread/process
};

#endif
