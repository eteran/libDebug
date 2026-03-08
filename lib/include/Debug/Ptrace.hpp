
#ifndef PTRACE_HPP_
#define PTRACE_HPP_

#include "Result.hpp"
#include <cerrno>
#include <sys/ptrace.h>

/**
 * @brief A helper function to wrap ptrace calls and return a Result type instead of using errno.
 *
 * @param request The ptrace request to perform.
 * @param pid The process ID to perform the ptrace request on.
 * @param args The arguments to pass to ptrace.
 * @return A Result containing either the return value of ptrace or the errno error code if ptrace failed.
 */
template <class... Args>
Result<long, int> do_ptrace(int request, pid_t pid, Args... args) {

    using R = Result<long, int>;

	errno             = 0;
	const long result = ::ptrace(static_cast<__ptrace_request>(request), pid, std::forward<Args>(args)...);
	if (result == -1 && errno != 0) {
		return R::Err(errno);
	}
	return R::Ok(result);
}

#endif
