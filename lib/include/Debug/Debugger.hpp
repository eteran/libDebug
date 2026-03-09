
#ifndef DEBUGGER_HPP_
#define DEBUGGER_HPP_

#include <memory>

#include <signal.h>
#include <sys/types.h>
#include <thread>

class Process;

class Debugger {
public:
	Debugger();
	Debugger(const Debugger &)            = delete;
	Debugger &operator=(const Debugger &) = delete;
	~Debugger();

public:
	std::shared_ptr<Process> attach(pid_t pid);
	std::shared_ptr<Process> spawn(const char *cwd, const char *argv[], const char *envp[] = nullptr);

public:
	[[nodiscard]] const std::shared_ptr<Process> &process() const { return process_; }

public:
	void set_disable_lazy_binding(bool value) noexcept;
	void set_disable_aslr(bool value) noexcept;
	void set_disable_proc_mem(bool value) noexcept { disableProcMem_ = value; }

private:
	std::shared_ptr<Process> process_;
	std::thread::id ctor_thread_id_;
	bool disableLazyBinding_ = true;
	bool disableProcMem_     = false;
	bool disableASLR_        = true;
};

#endif
