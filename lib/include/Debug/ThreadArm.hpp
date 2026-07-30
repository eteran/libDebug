
#ifndef THREAD_ARM_HPP_
#define THREAD_ARM_HPP_

#include "Context.hpp"
#include "ThreadBase.hpp"

#include <cstdint>

class Process;

class Thread : public ThreadBase {
	friend class Debugger;
	friend class Process;

	struct internal_t {};

public:
	// The constructor is private to force users to create Thread objects through the Process interface.
	Thread(const internal_t &, Process *process, pid_t tid, Flag f);
	Thread(const Thread &)            = delete;
	Thread &operator=(const Thread &) = delete;
	~Thread();

public:
	void get_context(Context *ctx) const;
	void set_context(const Context *ctx) const;

	[[nodiscard]] uint64_t get_instruction_pointer() const override;
	void set_instruction_pointer(uint64_t ip) const override;

private:
	[[nodiscard]] bool detect_64_bit() const;
};

#endif
