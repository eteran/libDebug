// NOTE(eteran): Makes it possible for 32-bit builds to debug 64-bit processes
#define _POSIX_C_SOURCE   200809L
#define _FILE_OFFSET_BITS 64
#define _TIME_BITS        64

#include "Debug/Memory.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Proc.hpp"
#include "Debug/Ptrace.hpp"
#include "Debug/Region.hpp"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstring>

#include <fcntl.h>
#include <linux/limits.h>
#include <unistd.h>

// NOTE(eteran): we don't use `process_vm_writev` here because it doesn't allow for writing to read-only pages
// which is necessary for implementing breakpoints. We try `pwrite` first because it's faster,
// but if it fails we fall back to `ptrace` which is more likely to succeed.

namespace {

/**
 * @brief Validates that `/proc/<pid>/mem` supports both read and write.
 *
 * Performs a one-byte read and write-back on a readable+writable mapping.
 * If no suitable mapping can be validated, throws `DebuggerError` so callers
 * can fall back to ptrace-based access.
 */
void validate_proc_memory_fd(int memfd, pid_t pid) {
	int last_read_errno  = 0;
	int last_write_errno = 0;
	bool attempted       = false;

	const std::vector<Region> regions = enumerate_regions(pid);
	for (const Region &region : regions) {
		if (!region.is_readable() || !region.is_writable() || region.start() >= region.end()) {
			continue;
		}

		attempted = true;

		const uint64_t probe_address = region.start();
		off_t offset                 = static_cast<off_t>(probe_address);

		uint8_t original = 0;
		const ssize_t nread = pread(memfd, &original, sizeof(original), offset);
		if (nread != static_cast<ssize_t>(sizeof(original))) {
			if (nread < 0) {
				last_read_errno = errno;
			}
			continue;
		}

		const ssize_t nwritten = pwrite(memfd, &original, sizeof(original), offset);
		if (nwritten != static_cast<ssize_t>(sizeof(original))) {
			if (nwritten < 0) {
				last_write_errno = errno;
			}
			continue;
		}

		return;
	}

	if (!attempted) {
		throw DebuggerError("Failed to validate /proc/%d/mem for process %d: no readable+writable regions", pid, pid);
	}

	throw DebuggerError("Failed to validate /proc/%d/mem for process %d: last read error=%s, last write error=%s",
						pid,
						pid,
						last_read_errno != 0 ? strerror(last_read_errno) : "none",
						last_write_errno != 0 ? strerror(last_write_errno) : "none");
}

}

/**
 * @brief Opens the memory file descriptor for the given process ID.
 *
 * @param pid The process ID to open the memory file descriptor for.
 */
ProcMemory::ProcMemory(pid_t pid) {
	char path[PATH_MAX];
	std::snprintf(path, sizeof(path), "/proc/%d/mem", pid);
	memfd_ = open(path, O_RDWR);
	if (memfd_ == -1) {
		throw DebuggerError("Failed to open memory file descriptor for process %d: %s", pid, strerror(errno));
	}

	validate_proc_memory_fd(memfd_, pid);
}

/**
 * @brief Destroy the ProcMemory object and closes the memory file descriptor if it was opened successfully.
 */
ProcMemory::~ProcMemory() {
	if (memfd_ != -1) {
		close(memfd_);
	}
}

/**
 * @brief Reads bytes from the attached process using `pread`.
 *
 * @param address The address in the attached process to read from.
 * @param buffer Where to store the bytes, must be at least `size` bytes big.
 * @param size How many bytes to read.
 * @return how many bytes actually read.
 */
int64_t ProcMemory::read(uint64_t address, void *buffer, size_t size) {
	return pread(memfd_, buffer, size, static_cast<off_t>(address));
}

/**
 * @brief Writes bytes to the attached process using `pwrite`.
 *
 * @param address The address in the attached process to write to.
 * @param buffer A buffer containing the bytes to write must be at least `size` bytes big.
 * @param size How many bytes to write.
 * @return how many bytes actually written.
 */
int64_t ProcMemory::write(uint64_t address, const void *buffer, size_t size) {
	return pwrite(memfd_, buffer, size, static_cast<off_t>(address));
}

/**
 * @brief Constructs a new PtraceMemory object for the given process ID.
 */
PtraceMemory::PtraceMemory(pid_t pid) : pid_(pid) {
}

/**
 * @brief Reads bytes from the attached process using ptrace.
 *
 * @param address The address in the attached process to read from.
 * @param buffer Where to store the bytes, must be at least `size` bytes big.
 * @param size How many bytes to read.
 * @return how many bytes actually read.
 */
int64_t PtraceMemory::read(uint64_t address, void *buffer, size_t size) {
	auto ptr      = static_cast<uint8_t *>(buffer);
	int64_t total = 0;

	while (size > 0) {

		auto ret = do_ptrace(PTRACE_PEEKDATA, pid_, reinterpret_cast<void *>(address), nullptr);
		if (ret.is_err()) {
			// we just ignore ESRCH because it means the process
			// was terminated while we were trying to read
			if (ret.error() == ESRCH) {
				return 0;
			}
			throw DebuggerError("Failed to read memory for process %d: %s", pid_, strerror(ret.error()));
		}

		auto value = ret.value();

		const size_t count = std::min(size, sizeof(value));
		std::memcpy(ptr, &value, count);

		address += count;
		ptr += count;
		total += static_cast<int64_t>(count);
		size -= count;
	}

	return total;
}

/**
 * @brief Writes bytes to the attached process using ptrace.
 *
 * @param address The address in the attached process to write to.
 * @param buffer A buffer containing the bytes to write must be at least `size` bytes big.
 * @param size How many bytes to write.
 * @return how many bytes actually written.
 */
int64_t PtraceMemory::write(uint64_t address, const void *buffer, size_t size) {
	auto ptr      = static_cast<const uint8_t *>(buffer);
	int64_t total = 0;

	while (size > 0) {
		const size_t count = std::min(size, sizeof(long));

		alignas(long) uint8_t data[sizeof(long)] = {};
		std::memcpy(data, ptr, count);

		if (count < sizeof(long)) {

			auto ret = do_ptrace(PTRACE_PEEKDATA, pid_, reinterpret_cast<void *>(address), nullptr);
			if (ret.is_err()) {
				// we just ignore ESRCH because it means the process
				// was terminated while we were trying to read
				if (ret.error() == ESRCH) {
					return 0;
				}
				throw DebuggerError("Failed to read memory for process %d: %s", pid_, strerror(ret.error()));
			}

			std::memcpy(
				data + count,
				reinterpret_cast<const uint8_t *>(&ret) + count,
				sizeof(long) - count);
		}

		long data_value;
		std::memcpy(&data_value, data, sizeof(long));

		if (auto ret = do_ptrace(PTRACE_POKEDATA, pid_, reinterpret_cast<void *>(address), data_value); ret.is_err()) {
			throw DebuggerError("Failed to write memory for process %d: %s", pid_, strerror(ret.error()));
		}

		address += count;
		ptr += count;
		total += static_cast<int64_t>(count);
		size -= count;
	}

	return total;
}
