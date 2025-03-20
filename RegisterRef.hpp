
#ifndef REGISTER_REF_HPP_
#define REGISTER_REF_HPP_

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

class RegisterRef {
public:
	explicit RegisterRef(void *ptr, size_t size)
		: ptr_(ptr), size_(size) {}

	RegisterRef()                               = default;
	RegisterRef(const RegisterRef &)            = default;
	RegisterRef &operator=(const RegisterRef &) = default;
	RegisterRef(RegisterRef &&)                 = default;
	RegisterRef &operator=(RegisterRef &&)      = default;
	~RegisterRef()                              = default;

public:
	[[nodiscard]] bool is_valid() const { return ptr_ != nullptr; }
	[[nodiscard]] size_t size() const { return size_; }
	[[nodiscard]] const void *data() const { return ptr_; }
	[[nodiscard]] bool operator==(const RegisterRef &rhs) const { return size_ == rhs.size_ && std::memcmp(ptr_, rhs.ptr_, size_) == 0; }
	[[nodiscard]] bool operator!=(const RegisterRef &rhs) const { return size_ != rhs.size_ || std::memcmp(ptr_, rhs.ptr_, size_) != 0; }

public:
	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	Integer as() const {
		Integer value;
		std::memcpy(&value, ptr_, std::min(size_, sizeof(Integer)));
		return value;
	}

	template <class Integer, std::enable_if_t<std::is_integral_v<Integer>, bool> = true>
	RegisterRef &operator=(Integer value) {
		assert(size_ >= sizeof(Integer));
		// NOTE(eteran): effectively zero-extend the value to the size of the register
		std::memset(ptr_, 0, size_);
		std::memcpy(ptr_, &value, sizeof(Integer));
		return *this;
	}

private:
	void *ptr_   = nullptr;
	size_t size_ = 0;
};

template <class T>
RegisterRef make_register(T &var) {
	return RegisterRef(&var, sizeof(T));
}

#endif
