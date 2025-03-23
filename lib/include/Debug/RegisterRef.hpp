
#ifndef REGISTER_REF_HPP_
#define REGISTER_REF_HPP_

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

	RegisterRef()                               = default;
	RegisterRef(const RegisterRef &)            = default;
	RegisterRef &operator=(const RegisterRef &) = default;
	RegisterRef(RegisterRef &&)                 = default;
	RegisterRef &operator=(RegisterRef &&)      = default;
	~RegisterRef()                              = default;

public:
	[[nodiscard]] const std::string &name() const { return name_; }
	[[nodiscard]] bool is_valid() const { return ptr_ != nullptr; }
	[[nodiscard]] size_t size() const { return size_; }
	[[nodiscard]] const void *data() const { return ptr_; }

public:
	[[nodiscard]] bool operator==(const RegisterRef &rhs) const { return size_ == rhs.size_ && std::memcmp(ptr_, rhs.ptr_, size_) == 0; }
	[[nodiscard]] bool operator!=(const RegisterRef &rhs) const { return size_ != rhs.size_ || std::memcmp(ptr_, rhs.ptr_, size_) != 0; }

public:
	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	Integer as() const {
		// NOTE(eteran): effectively zero-extend the value to the size of the integer being read into
		Integer value;
		std::memset(&value, 0, sizeof(Integer));
		std::memcpy(&value, ptr_, std::min(size_, sizeof(Integer)));
		return value;
	}

	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	RegisterRef &operator=(Integer value) {

		// NOTE(eteran): effectively zero-extend the value to the size of the register
		std::memset(ptr_, 0, size_);
		std::memcpy(ptr_, &value, std::min(size_, sizeof(Integer)));
		return *this;
	}

public:
	/**
	 * @brief Pre-increment operator.
	 *
	 * @return A reference to this register.
	 */
	RegisterRef &operator++() {
		union {
			uint8_t u8;
			uint16_t u16;
			uint32_t u32;
			uint64_t u64;
		};

		switch (size_) {
		case 1:
			memcpy(&u8, ptr_, 1);
			++u8;
			memcpy(ptr_, &u8, 1);
			break;
		case 2:
			memcpy(&u16, ptr_, 1);
			++u16;
			memcpy(ptr_, &u16, 1);
			break;
		case 4:
			memcpy(&u32, ptr_, 1);
			++u32;
			memcpy(ptr_, &u32, 1);
			break;
		case 8:
			memcpy(&u64, ptr_, 1);
			++u64;
			memcpy(ptr_, &u64, 1);
			break;
		default:
			assert(false && "Invalid size");
		}

		return *this;
	}

	/**
	 * @brief Pre-decrement operator.
	 *
	 * @return A reference to this register.
	 */
	RegisterRef &operator--() {
		union {
			uint8_t u8;
			uint16_t u16;
			uint32_t u32;
			uint64_t u64;
		};

		switch (size_) {
		case 1:
			memcpy(&u8, ptr_, 1);
			--u8;
			memcpy(ptr_, &u8, 1);
			break;
		case 2:
			memcpy(&u16, ptr_, 1);
			--u16;
			memcpy(ptr_, &u16, 1);
			break;
		case 4:
			memcpy(&u32, ptr_, 1);
			--u32;
			memcpy(ptr_, &u32, 1);
			break;
		case 8:
			memcpy(&u64, ptr_, 1);
			--u64;
			memcpy(ptr_, &u64, 1);
			break;
		default:
			assert(false && "Invalid size");
		}

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

		union {
			uint8_t u8;
			uint16_t u16;
			uint32_t u32;
			uint64_t u64;
		};

		switch (size_) {
		case 1:
			memcpy(&u8, ptr_, 1);
			u8 += static_cast<uint8_t>(value);
			memcpy(ptr_, &u8, 1);
			break;
		case 2:
			memcpy(&u16, ptr_, 1);
			u16 += static_cast<uint16_t>(value);
			memcpy(ptr_, &u16, 1);
			break;
		case 4:
			memcpy(&u32, ptr_, 1);
			u32 += static_cast<uint32_t>(value);
			memcpy(ptr_, &u32, 1);
			break;
		case 8:
			memcpy(&u64, ptr_, 1);
			u64 += static_cast<uint64_t>(value);
			memcpy(ptr_, &u64, 1);
			break;
		default:
			assert(false && "Invalid size");
		}

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

		union {
			uint8_t u8;
			uint16_t u16;
			uint32_t u32;
			uint64_t u64;
		};

		switch (size_) {
		case 1:
			memcpy(&u8, ptr_, 1);
			u8 -= static_cast<uint8_t>(value);
			memcpy(ptr_, &u8, 1);
			break;
		case 2:
			memcpy(&u16, ptr_, 1);
			u16 -= static_cast<uint16_t>(value);
			memcpy(ptr_, &u16, 1);
			break;
		case 4:
			memcpy(&u32, ptr_, 1);
			u32 -= static_cast<uint32_t>(value);
			memcpy(ptr_, &u32, 1);
			break;
		case 8:
			memcpy(&u64, ptr_, 1);
			u64 -= static_cast<uint64_t>(value);
			memcpy(ptr_, &u64, 1);
			break;
		default:
			assert(false && "Invalid size");
		}

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
 * @param size The size of the register.
 * @param offset The offset into the variable where the register data starts.
 * @return A register reference.
 */
template <class T>
RegisterRef make_register(std::string_view name, T &var, size_t size, size_t offset) {
	assert(offset < sizeof(T));
	assert(size <= sizeof(T) - offset);
	auto ptr = reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(&var) + offset);
	return RegisterRef(name, ptr, size);
}

#endif
