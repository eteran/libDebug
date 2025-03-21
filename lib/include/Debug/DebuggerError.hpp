
#ifndef DEBUGGER_ERROR_HPP_
#define DEBUGGER_ERROR_HPP_

#include <stdexcept>
#include <string_view>

class DebuggerError : public std::runtime_error {
public:
	using std::runtime_error::runtime_error;
};

#endif
