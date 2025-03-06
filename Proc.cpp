
#include "Proc.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <dirent.h>

namespace {

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
