
#include "Debug/Breakpoint.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Process.hpp"

#include <cstring>

/**
 * @brief Disable and then destroy the Breakpoint object.
 */
Breakpoint::~Breakpoint() {
	disable();
}

/**
 * @brief Enables the breakpoint by backing up the bytes at the target address as needed.
 * and then replacing them with bytes representing a breakpoint.
 */
void Breakpoint::enable() {
	if (enabled_) {
		return;
	}

	const int64_t r = process_->read_memory(address_, old_bytes_, size_);
	if (r == -1) {
		throw DebuggerError("Failed to read memory for process %d: %s", process_->pid(), strerror(errno));
	}

	if (static_cast<size_t>(r) != size_) {
		throw DebuggerError("Failed to read memory for process %d", process_->pid());
	}

	const int64_t w = process_->write_memory(address_, new_bytes_, size_);
	if (w == -1) {
		// If the tracee exited, treat as benign during teardown.
		if (errno == ESRCH) {
			enabled_ = false;
			return;
		}
		throw DebuggerError("Failed to write memory for process %d: %s", process_->pid(), strerror(errno));
	}

	if (w == 0) {
		// pwrite may return 0 when the process no longer exists.
		if (kill(process_->pid(), 0) == -1 && errno == ESRCH) {
			enabled_ = false;
			return;
		}
		throw DebuggerError("Failed to write memory for process %d: short write", process_->pid());
	}

	if (static_cast<size_t>(w) != size_) {
		// If the process disappeared, treat as benign.
		if (kill(process_->pid(), 0) == -1 && errno == ESRCH) {
			enabled_ = false;
			return;
		}
		throw DebuggerError("Failed to write memory for process %d: partial write", process_->pid());
	}

	enabled_ = true;
}

/**
 * @brief Disables the breakpoint by restoring the backed up bytes at the target address.
 */
void Breakpoint::disable() {
	if (!enabled_) {
		return;
	}
	const int64_t w = process_->write_memory(address_, old_bytes_, size_);

	if (w == -1) {
		// If the tracee has already exited, treat as benign cleanup.
		if (errno == ESRCH) {
			enabled_ = false;
			return;
		}
		throw DebuggerError("Failed to write memory for process %d: %s", process_->pid(), strerror(errno));
	}

	if (w == 0) {
		if (kill(process_->pid(), 0) == -1 && errno == ESRCH) {
			enabled_ = false;
			return;
		}
		throw DebuggerError("Failed to write memory for process %d: short write", process_->pid());
	}

	if (static_cast<size_t>(w) != size_) {
		if (kill(process_->pid(), 0) == -1 && errno == ESRCH) {
			enabled_ = false;
			return;
		}
		throw DebuggerError("Failed to write memory for process %d: partial write", process_->pid());
	}

	enabled_ = false;
}

/**
 * @brief Increments the hit count for the breakpoint.
 */
void Breakpoint::hit() {
	hit_count_++;
}

/**
 * @brief Toggles the breakpoint between enabled and disabled states.
 */
void Breakpoint::toggle() {
	if (enabled_) {
		disable();
	} else {
		enable();
	}
}
