
#ifndef DEBUGGER_HPP_
#define DEBUGGER_HPP_

#include <memory>

#include <sys/types.h>

class Process;

class Debugger {
public:
	Debugger();
	~Debugger();

public:
	std::shared_ptr<Process> attach(pid_t pid);
	std::shared_ptr<Process> spawn(const char *cwd, const char *argv[]);

public:
	std::shared_ptr<Process> process() const { return process_; };

public:
	void set_disable_lazy_binding(bool value);
	void set_disable_aslr(bool value);

private:
	std::shared_ptr<Process> process_;
	sigset_t prev_mask_;

	bool disableLazyBinding_ = true;
	bool disableASLR_        = true;
};

#endif
