
#ifndef DEFER_HPP_
#define DEFER_HPP_

#include <utility>

template <class F>
class defer_type {
public:
	defer_type(F &&func)
		: func_(std::forward<F>(func)) {
	}

	~defer_type() {
		func_();
	}

private:
	F func_;
};

template <class F>
defer_type<F> make_defer(F &&f) {
	return defer_type<F>{std::forward<F>(f)};
}

#define CONCAT(a, b)  a##b
#define CONCAT2(a, b) CONCAT(a, b)

#define SCOPE_EXIT(...) \
	auto CONCAT2(scope_exit_, __LINE__) = make_defer([&] __VA_ARGS__)

#endif
