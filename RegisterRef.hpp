
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
	explicit RegisterRef(std::string_view name, void *ptr, size_t size, size_t offset)
		: name_(name), ptr_(ptr), size_(size), offset_(offset) {}

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
		Integer value;
		// NOTE(eteran): effectively zero-extend the value to the size of the integer being read into
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

private:
	std::string name_;
	void *ptr_     = nullptr;
	size_t size_   = 0;
	size_t offset_ = 0;
};

template <class T>
RegisterRef make_register(std::string_view name, T &var, size_t offset) {
	return RegisterRef(name, &var, sizeof(T), offset);
}

template <class T>
RegisterRef make_register(std::string_view name, T &var, size_t size, size_t offset) {
	assert(size <= sizeof(T));
	return RegisterRef(name, &var, size, offset);
}

#endif
