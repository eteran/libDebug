
#ifndef EVENT_STATUS_HPP_
#define EVENT_STATUS_HPP_

enum class EventStatus {
	Stop,                // do nothing, the debugger will instigate the next event
	Continue,            // the event has been addressed, continue as normal
	ContinueStep,        // the event has been addressed, step as normal
	ExceptionNotHandled, // pass the event unmodified back thread and continue
	NextHandler,         // pass the event to the next handler
};

#endif
