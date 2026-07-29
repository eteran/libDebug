
#ifndef BREAKPOINT_ARM_HPP_
#define BREAKPOINT_ARM_HPP_

#include <atomic>
#include <cstdint>
#include <memory>

class Process;

class Breakpoint {
public:
	static constexpr size_t MinBreakpointSize = 4;
	static constexpr size_t MaxBreakpointSize = 4;

public:
	enum class TypeId : int {
		Automatic = 0,
		BRK,

		TYPE_COUNT,
	};

public:
	Breakpoint(const Process *process, uint64_t address, TypeId type = TypeId::Automatic);
	Breakpoint(const Breakpoint &)            = delete;
	Breakpoint &operator=(const Breakpoint &) = delete;
	~Breakpoint();

public:
	[[nodiscard]] uint64_t address() const { return address_; }
	[[nodiscard]] size_t size() const { return size_; }
	void enable();
	void disable();
	void toggle();
	void hit();
	void set_internal(bool value) { internal_ = value; }
	[[nodiscard]] bool is_internal() const { return internal_; }
	[[nodiscard]] bool enabled() const { return enabled_; }
	[[nodiscard]] TypeId type() const { return type_; }
	[[nodiscard]] uint8_t *old_bytes() { return old_bytes_; }
	[[nodiscard]] uint8_t *new_bytes() { return new_bytes_; }
	[[nodiscard]] uint64_t hit_count() const { return hit_count_.load(); }

private:
	std::atomic<uint64_t> hit_count_{0};
	const Process *process_               = nullptr;
	uint64_t address_                     = 0;
	uint8_t old_bytes_[MaxBreakpointSize] = {};
	uint8_t new_bytes_[MaxBreakpointSize] = {};
	size_t size_                          = 0;
	TypeId type_                          = TypeId::Automatic;
	bool enabled_                         = false;
	bool internal_                        = false;
};

#endif
