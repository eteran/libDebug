
#ifndef REGISTER_REF_HPP_
#define REGISTER_REF_HPP_

#include "DebuggerError.hpp"
#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>

class RegisterRef {
public:
	explicit RegisterRef(std::string_view name, void *ptr, size_t size)
		: name_(name), ptr_(ptr), size_(size) {}

	RegisterRef()  = default;
	~RegisterRef() = default;
	// non-copyable and non-movable: RegisterRef aliases external memory.
	// Copying/moving would rebind the alias; use make_register() at the call site instead.
	// .assign() can be used to copy values between RegisterRefs without changing the reference itself.
	RegisterRef &operator=(RegisterRef &&)      = delete;
	RegisterRef &operator=(const RegisterRef &) = delete;
	RegisterRef(RegisterRef &&)                 = delete;
	RegisterRef(const RegisterRef &)            = delete;

public:
	[[nodiscard]] const std::string &name() const noexcept { return name_; }
	[[nodiscard]] bool is_valid() const noexcept { return ptr_ != nullptr; }
	[[nodiscard]] size_t size() const noexcept { return size_; }
	[[nodiscard]] const void *data() const noexcept { return ptr_; }
	[[nodiscard]] void *data() noexcept { return ptr_; }

public:
	[[nodiscard]] bool operator==(const RegisterRef &rhs) const {
		if (size_ != rhs.size_) {
			return false;
		}

		if (ptr_ == rhs.ptr_) {
			return true;
		}

		if (!ptr_ || !rhs.ptr_) {
			return false;
		}

		return std::memcmp(ptr_, rhs.ptr_, size_) == 0;
	}

	[[nodiscard]] bool operator!=(const RegisterRef &rhs) const { return !(*this == rhs); }

public:
	void assign(const RegisterRef &rhs) {
		if (!is_valid() || !rhs.is_valid()) {
			throw DebuggerError("RegisterRef: invalid assignment between '%s' and '%s'", name_.c_str(), rhs.name().c_str());
		}

		if (this != &rhs) {
			std::memset(ptr_, 0, size_);
			std::memcpy(ptr_, rhs.data(), std::min(size_, rhs.size()));
		}
	}

public:
	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	Integer as() const {

		if (!is_valid()) {
			throw DebuggerError("RegisterRef: invalid read from '%s'", name_.c_str());
		}

		// NOTE(eteran): effectively zero-extend the value to the size of the integer being read into
		// NOTE(eteran): this assumes little-endian, which is true for x86 for now.
		Integer value;
		std::memset(&value, 0, sizeof(Integer));
		std::memcpy(&value, ptr_, std::min(size_, sizeof(Integer)));
		return value;
	}

	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	RegisterRef &operator=(Integer value) {

		if (!is_valid()) {
			throw DebuggerError("RegisterRef: invalid write to '%s'", name_.c_str());
		}

		// NOTE(eteran): effectively zero-extend the value to the size of the register
		// NOTE(eteran): this assumes little-endian, which is true for x86 for now.
		std::memset(ptr_, 0, size_);
		std::memcpy(ptr_, &value, std::min(size_, sizeof(Integer)));
		return *this;
	}

private:
	template <class Op>
	void apply_op(Op op) {
		if (!is_valid()) {
			throw DebuggerError("RegisterRef: cannot perform operation on invalid register reference '%s'", name_.c_str());
		}

		union {
			uint8_t u8;
			uint16_t u16;
			uint32_t u32;
			uint64_t u64;
		};

		switch (size_) {
		case 1:
			std::memcpy(&u8, ptr_, 1);
			op(u8);
			std::memcpy(ptr_, &u8, 1);
			break;
		case 2:
			std::memcpy(&u16, ptr_, 2);
			op(u16);
			std::memcpy(ptr_, &u16, 2);
			break;
		case 4:
			std::memcpy(&u32, ptr_, 4);
			op(u32);
			std::memcpy(ptr_, &u32, 4);
			break;
		case 8:
			std::memcpy(&u64, ptr_, 8);
			op(u64);
			std::memcpy(ptr_, &u64, 8);
			break;
		default:
			assert(false && "Invalid size");
		}
	}

public:
	/**
	 * @brief Pre-increment operator.
	 *
	 * @return A reference to this register.
	 */
	RegisterRef &operator++() {
		apply_op([](auto &v) { ++v; });
		return *this;
	}

	/**
	 * @brief Pre-decrement operator.
	 *
	 * @return A reference to this register.
	 */
	RegisterRef &operator--() {
		apply_op([](auto &v) { --v; });
		return *this;
	}

	/**
	 * @brief Addition assignment operator.
	 *
	 * @param value The value to add to this register.
	 * @return A reference to this register.
	 */
	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	RegisterRef &operator+=(Integer value) {
		apply_op([value](auto &v) { v += static_cast<std::decay_t<decltype(v)>>(value); });
		return *this;
	}

	/**
	 * @brief Subtraction assignment operator.
	 *
	 * @param value The value to subtract from this register.
	 * @return A reference to this register.
	 */
	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	RegisterRef &operator-=(Integer value) {
		apply_op([value](auto &v) { v -= static_cast<std::decay_t<decltype(v)>>(value); });
		return *this;
	}

private:
	std::string name_;
	void *ptr_   = nullptr;
	size_t size_ = 0;
};

/**
 * @brief Creates a register reference.
 *
 * @param name The name of the register.
 * @param var The variable containing the register data.
 * @param offset The offset into the variable where the register data starts.
 * @return A register reference.
 */
template <class T>
RegisterRef make_register(std::string_view name, T &var, size_t offset) {
	assert(offset < sizeof(T));
	auto ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(&var) + offset);
	return RegisterRef(name, ptr, sizeof(T) - offset);
}

/**
 * @brief Creates a register reference.
 *
 * @param name The name of the register.
 * @param var The variable containing the register data.
 * @param offset The offset into the variable where the register data starts.
 * @param size The size of the register.
 * @return A register reference.
 */
template <class T>
RegisterRef make_register(std::string_view name, T &var, size_t offset, size_t size) {
	assert(offset < sizeof(T));
	assert(size <= sizeof(T) - offset);
	auto ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(&var) + offset);
	return RegisterRef(name, ptr, size);
}

#endif
