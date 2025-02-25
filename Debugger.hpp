
#ifndef DEBUGGER_HPP_
#define DEBUGGER_HPP_

#include <memory>
#include <string>
#include <vector>

#include <sys/types.h>

class Process;

class Debugger {
public:
	std::shared_ptr<Process> attach(pid_t pid);
	std::shared_ptr<Process> spawn(int argc, char *argv[]);

public:
	std::shared_ptr<Process> process() const { return process_; };

private:
	std::shared_ptr<Process> process_;
};

#endif
