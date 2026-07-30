
#ifndef THREAD_INTEL_HPP_
#define THREAD_INTEL_HPP_

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
	[[nodiscard]] uint32_t get_segment_base32(Context *ctx, RegisterId reg) const;
	void get_debug_registers(Context *ctx) const;
	void get_debug_registers32(Context *ctx) const;
	void get_debug_registers64(Context *ctx) const;
	void get_registers(Context *ctx) const;
	void get_registers32(Context *ctx) const;
	void get_registers64(Context *ctx) const;
	void get_segment_bases(Context *ctx) const;
	void get_xstate(Context *ctx) const;
	void get_xstate32(Context *ctx) const;
	void get_xstate64(Context *ctx) const;
	void set_debug_registers(const Context *ctx) const;
	void set_debug_registers32(const Context *ctx) const;
	void set_debug_registers64(const Context *ctx) const;
	void set_registers(const Context *ctx) const;
	void set_registers32(const Context *ctx) const;
	void set_registers64(const Context *ctx) const;
	void set_xstate(const Context *ctx) const;
	void set_xstate32(const Context *ctx) const;
	void set_xstate64(const Context *ctx) const;

	int get_xstate32_modern(Context *ctx) const;
	int get_xstate32_legacy(Context *ctx) const;
	int get_xstate32_fallback(Context *ctx) const;
	int set_xstate32_modern(const Context *ctx) const;
	int set_xstate32_legacy(const Context *ctx) const;
	int set_xstate32_fallback(const Context *ctx) const;
};

#endif
