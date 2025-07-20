
#ifndef DEBUGGER_ERROR_HPP_
#define DEBUGGER_ERROR_HPP_

#include <cstdarg>
#include <stdexcept>

class DebuggerError : public std::exception {
public:
	DebuggerError(const char *fmt, ...) __attribute__((format(printf, 2, 3))) {
		va_list ap;
		va_start(ap, fmt);
		const int n = vsnprintf(buffer_, sizeof(buffer_), fmt, ap);
		va_end(ap);
		if (n < 0 || static_cast<size_t>(n) >= sizeof(buffer_)) {
			throw std::runtime_error("DebuggerError: message too long");
		}
	}

public:
	const char *what() const noexcept override {
		return buffer_;
	}

private:
	char buffer_[1024];
};

#endif
