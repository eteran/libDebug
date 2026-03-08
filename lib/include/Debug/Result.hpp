
#ifndef DEBUG_RESULT_HPP_
#define DEBUG_RESULT_HPP_

#pragma once

#include <cstddef>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <variant>

template <typename T, typename E>
class Result {
public:
	static Result Ok(T value) {
		return Result(std::in_place_index<0>, std::move(value));
	}

	static Result Err(E err) {
		return Result(std::in_place_index<1>, std::move(err));
	}

public:
	bool ok() const noexcept { return storage_.index() == 0; }
	bool is_err() const noexcept { return storage_.index() == 1; }
	explicit operator bool() const noexcept { return ok(); }

public:
	const T &value() const & { return std::get<0>(storage_); }
	T &value() & { return std::get<0>(storage_); }
	T &&value() && { return std::get<0>(std::move(storage_)); }

public:
	const E &error() const & { return std::get<1>(storage_); }
	E &error() & { return std::get<1>(storage_); }
	E &&error() && { return std::get<1>(std::move(storage_)); }

public:
	template <typename U>
	T value_or(U &&default_value) const {
		if (ok()) return value();
		return static_cast<T>(std::forward<U>(default_value));
	}

private:
	template <std::size_t I, typename... Args>
	explicit Result(std::in_place_index_t<I>, Args &&...args)
		: storage_(std::in_place_index<I>, std::forward<Args>(args)...) {}

	std::variant<T, E> storage_;
};

// Specialization for void result
template <typename E>
class Result<void, E> {
public:
	static Result Ok() {
		Result r;
		r.storage_.template emplace<0>();
		return r;
	}

	static Result Err(E err) {
		Result r;
		r.storage_.template emplace<1>(std::move(err));
		return r;
	}

public:
	bool ok() const noexcept { return storage_.index() == 0; }
	bool is_err() const noexcept { return storage_.index() == 1; }
	explicit operator bool() const noexcept { return ok(); }

public:
	const E &error() const & { return std::get<1>(storage_); }
	E &error() & { return std::get<1>(storage_); }
	E &&error() && { return std::get<1>(std::move(storage_)); }

private:
	std::variant<std::monostate, E> storage_;
};

#endif
