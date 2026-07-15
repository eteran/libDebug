#ifndef SRC_RESULT_HPP_
#define SRC_RESULT_HPP_

#include <cassert>
#include <cerrno>
#include <cstddef>
#include <functional>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

template <class T>
class Ok {
	template <class, class>
	friend class Result;

public:
	constexpr explicit Ok(T value)
		: value_(std::move(value)) {}

private:
	T value_;
};

template <>
class Ok<void> {
public:
	constexpr Ok() = default;
};

Ok() -> Ok<void>;

template <class E>
class Err {
	template <class, class>
	friend class Result;

public:
	constexpr explicit Err(E error)
		: error_(std::move(error)) {}

private:
	E error_;
};

template <class T, class E = int>
class Result;

template <class R, class E>
struct is_result_with_error : std::false_type {};

template <class O, class E>
struct is_result_with_error<Result<O, E>, E> : std::true_type {};

template <class T, class E>
class [[nodiscard]] Result {
public:
	template <class U = T, typename std::enable_if<std::is_convertible_v<U, T>, int>::type = 0>
	Result(Ok<U> &&ok)
		: storage_(std::in_place_index<0>, std::move(ok.value_)) {}

	template <class U = E, typename std::enable_if<std::is_convertible_v<U, E>, int>::type = 0>
	Result(Err<U> &&err)
		: storage_(std::in_place_index<1>, std::move(err.error_)) {}

public:
	// Conversion constructor for compatible types
	template <class U, class V, typename std::enable_if<std::is_convertible_v<U, T> && std::is_convertible_v<V, E>, int>::type = 0>
	Result(const Result<U, V> &other)
		: storage_(other.ok()
					   ? std::variant<T, E>(std::in_place_index<0>, static_cast<T>(other.value()))
					   : std::variant<T, E>(std::in_place_index<1>, static_cast<E>(other.error()))) {}

	// Conversion constructor for compatible rvalue types
	template <class U, class V, typename std::enable_if<std::is_convertible_v<U, T> && std::is_convertible_v<V, E>, int>::type = 0>
	Result(Result<U, V> &&other)
		: storage_(other.ok()
					   ? std::variant<T, E>(std::in_place_index<0>, static_cast<T>(std::move(other).value()))
					   : std::variant<T, E>(std::in_place_index<1>, static_cast<E>(std::move(other).error()))) {}

public:
	[[nodiscard]] bool ok() const noexcept {
		return storage_.index() == 0;
	}

	[[nodiscard]] bool is_err() const noexcept {
		return storage_.index() == 1;
	}

	explicit operator bool() const noexcept {
		return ok();
	}

public:
	Err<E> into_err() const & {
		assert(is_err());
		return Err<E>(std::get<1>(storage_));
	}

	Err<E> into_err() && {
		assert(is_err());
		return Err<E>(std::get<1>(std::move(storage_)));
	}

	Ok<T> into_ok() const & {
		assert(ok());
		return Ok<T>(std::get<0>(storage_));
	}

public:
	[[nodiscard]] const T &value() const & {
		assert(ok());
		return std::get<0>(storage_);
	}

	[[nodiscard]] T &value() & {
		assert(ok());
		return std::get<0>(storage_);
	}

	[[nodiscard]] T &&value() && {
		assert(ok());
		return std::get<0>(std::move(storage_));
	}

public:
	[[nodiscard]] const E &error() const & {
		assert(is_err());
		return std::get<1>(storage_);
	}

	[[nodiscard]] E &error() & {
		assert(is_err());
		return std::get<1>(storage_);
	}

	[[nodiscard]] E &&error() && {
		assert(is_err());
		return std::get<1>(std::move(storage_));
	}

public:
	template <class U>
	[[nodiscard]] T value_or(U &&default_value) const & {
		if (ok()) {
			return value();
		}
		return static_cast<T>(std::forward<U>(default_value));
	}

	template <class U>
	[[nodiscard]] T value_or(U &&default_value) && {
		if (ok()) {
			return std::move(*this).value();
		}
		return static_cast<T>(std::forward<U>(default_value));
	}

public:
	template <class F /*, typename std::enable_if<std::is_invocable_v<F, T>, int>::type = 0*/>
	[[nodiscard]] auto and_then(F &&func) const & {
		using Res = std::invoke_result_t<F, T>;
		static_assert(is_result_with_error<Res, E>::value, "`and_then` requires `F` to return `Result<O, E>`");
		if (ok()) {
			return std::invoke(std::forward<F>(func), value());
		}
		return Res(into_err());
	}

	template <class F /*, typename std::enable_if<std::is_invocable_v<F, T>, int>::type = 0 */>
	[[nodiscard]] auto and_then(F &&func) && {
		using Res = std::invoke_result_t<F, T>;
		static_assert(is_result_with_error<Res, E>::value, "`and_then` requires `F` to return `Result<O, E>`");
		if (ok()) {
			return std::invoke(std::forward<F>(func), std::move(*this).value());
		}
		return Res(std::move(*this).into_err());
	}

private:
	template <std::size_t I, class... Args>
	explicit Result(std::in_place_index_t<I>, Args &&...args)
		: storage_(std::in_place_index<I>, std::forward<Args>(args)...) {
	}

	std::variant<T, E> storage_;
};

// Specialization for void result
template <class E>
class [[nodiscard]] Result<void, E> {
public:
	Result()
		: storage_(std::in_place_index<0>) {}

	Result(Ok<void> &&)
		: Result() {}

	template <class U = E, typename std::enable_if<std::is_convertible_v<U, E>, int>::type = 0>
	Result(Err<U> &&err)
		: storage_(std::in_place_index<1>, std::move(err.error_)) {}

public:
	// Conversion constructor for compatible types
	template <class U, typename std::enable_if<std::is_convertible_v<U, E>, int>::type = 0>
	Result(const Result<void, U> &other)
		: storage_(other.ok()
					   ? std::variant<std::monostate, E>(std::in_place_index<0>, std::monostate{})
					   : std::variant<std::monostate, E>(std::in_place_index<1>, static_cast<E>(other.error()))) {}

	// Conversion constructor for compatible rvalue types
	template <class U, typename std::enable_if<std::is_convertible_v<U, E>, int>::type = 0>
	Result(Result<void, U> &&other)
		: storage_(other.ok()
					   ? std::variant<std::monostate, E>(std::in_place_index<0>, std::monostate{})
					   : std::variant<std::monostate, E>(std::in_place_index<1>, static_cast<E>(std::move(other).error()))) {}

public:
	[[nodiscard]] bool ok() const noexcept { return storage_.index() == 0; }
	[[nodiscard]] bool is_err() const noexcept { return storage_.index() == 1; }
	explicit operator bool() const noexcept { return ok(); }

public:
	Err<E> into_err() const & {
		assert(is_err());
		return Err<E>(std::get<1>(storage_));
	}

	Err<E> into_err() && {
		assert(is_err());
		return Err<E>(std::get<1>(std::move(storage_)));
	}

public:
	[[nodiscard]] const E &error() const & {
		assert(is_err());
		return std::get<1>(storage_);
	}

	[[nodiscard]] E &error() & {
		assert(is_err());
		return std::get<1>(storage_);
	}

	[[nodiscard]] E &&error() && {
		assert(is_err());
		return std::get<1>(std::move(storage_));
	}

public:
	template <class F /*, typename std::enable_if<std::is_invocable_v<F>, int>::type = 0*/>
	[[nodiscard]] auto and_then(F &&func) const & {
		using Res = std::invoke_result_t<F>;
		static_assert(is_result_with_error<Res, E>::value, "`and_then` requires `F` to return `Result<O, E>`");
		if (ok()) {
			return std::invoke(std::forward<F>(func));
		}
		return Res(into_err());
	}

	template <class F /*, typename std::enable_if<std::is_invocable_v<F>, int>::type = 0 */>
	[[nodiscard]] auto and_then(F &&func) && {
		using Res = std::invoke_result_t<F>;
		static_assert(is_result_with_error<Res, E>::value, "`and_then` requires `F` to return `Result<O, E>`");
		if (ok()) {
			return std::invoke(std::forward<F>(func));
		}
		return Res(std::move(*this).into_err());
	}

private:
	template <std::size_t I, class... Args>
	explicit Result(std::in_place_index_t<I>, Args &&...args)
		: storage_(std::in_place_index<I>, std::forward<Args>(args)...) {}

	std::variant<std::monostate, E> storage_;
};

#endif
