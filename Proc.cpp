
#include "Proc.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>

namespace {

// NOTE(eteran): see https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
// for more details on this algorithm
class hasher {
public:
	void update(const void *ptr, size_t n) {
		auto p    = static_cast<const uint8_t *>(ptr);
		auto last = p + n;

		while (p != last) {
			state_ = (state_ * 1099511628211ULL) ^ *p;
			++p;
		}
	}

	uint64_t digest() const { return state_; }

private:
	uint64_t state_ = 0xcbf29ce484222325ULL;
};

/**
 * @brief enumerates all numeric directories in the given path
 *
 *
 * @tparam Callback
 * @param path where to enumerate, typically either `/proc/` or `/proc/<pid>/task/`
 * @param callback the function to call for each numeric directory,
 * it should return false if it wants to abort enumeration
 */
template <class Callback>
void proc_enumerator(const char *path, Callback callback) {
	DIR *const dir = ::opendir(path);
	if (!dir) {
		::perror("opendir");
		::exit(0);
	}

	while (struct dirent *entry = readdir(dir)) {

		if (entry->d_type != DT_DIR) {
			continue;
		}

		errno          = 0;
		char *end_ptr  = nullptr;
		const long num = ::strtol(entry->d_name, &end_ptr, 10);

		if (errno == 0 && *end_ptr == '\0') {
			if (!callback(static_cast<pid_t>(num))) {
				break;
			}
		}
	}

	if (::closedir(dir) == -1) {
		::perror("closedir");
		::exit(0);
	}
}

}

/**
 * @brief enumerates all the threads of a given pid
 *
 * @param pid
 * @return std::vector<pid_t>
 */
std::vector<pid_t> enumerate_threads(pid_t pid) {
	std::vector<pid_t> threads;

	char path[PATH_MAX];
	::snprintf(path, sizeof(path), "/proc/%d/task/", pid);

	proc_enumerator(path, [&threads](pid_t tid) {
		threads.push_back(tid);
		return true;
	});

	return threads;
}

/**
 * @brief enumerates all running processes in the system
 *
 * @return std::vector<pid_t>
 */
std::vector<pid_t> enumerate_processes() {

	std::vector<pid_t> processes;

	proc_enumerator("/proc/", [&processes](pid_t pid) {
		processes.push_back(pid);
		return true;
	});

	return processes;
}

/**
 * @brief hashes the memory map of a given process
 *
 * @param pid
 * @return uint64_t
 */
uint64_t hash_regions(pid_t pid) {
	hasher h;

	char path[PATH_MAX];
	::snprintf(path, sizeof(path), "/proc/%d/maps", pid);

	int fd = ::open(path, O_RDONLY);
	if (fd == -1) {
		return 0;
	}

	char buffer[4096];
	ssize_t n;
	while ((n = ::read(fd, buffer, sizeof(buffer))) > 0) {
		h.update(buffer, static_cast<size_t>(n));
	}

	close(fd);

	return h.digest();
}

/**
 * @brief
 *
 * @param pid
 * @return std::vector<Region>
 */
std::vector<Region> enumerate_regions(pid_t pid) {
	std::vector<Region> regions;

	char path[PATH_MAX];
	::snprintf(path, sizeof(path), "/proc/%d/maps", pid);

	FILE *fp = ::fopen(path, "r");
	if (!fp) {
		return regions;
	}

	char line[PATH_MAX];
	while (::fgets(line, sizeof(line), fp)) {

		uint64_t start       = 0;
		uint64_t end         = 0;
		uint64_t offset      = 0;
		uint64_t permissions = 0;
		uint32_t dev_maj     = 0;
		uint32_t dev_min     = 0;
		uint32_t inode       = 0;
		char perms[32]       = {};
		char name[PATH_MAX]  = {};

		// address           perms offset  dev   inode       pathname
		// 00400000-00452000 r-xp 00000000 08:02 173521      /usr/bin/dbus-daemon

		if (::sscanf(line, "%lx-%lx %s %lx %x:%x %x %s", &start, &end, perms, &offset, &dev_maj, &dev_min, &inode, name) != -1) {
			if (::strchr(name, 'r')) permissions |= 1;
			if (::strchr(name, 'w')) permissions |= 2;
			if (::strchr(name, 'x')) permissions |= 4;
			if (::strchr(name, 'p')) permissions |= 0x2000;
			if (::strchr(name, 's')) permissions |= 0x1000;

			regions.emplace_back(start, end, permissions, name);
		}
	}

	fclose(fp);

	return regions;
}
