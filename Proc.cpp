
#include "Proc.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>

#include <dirent.h>

namespace {

template <class Callback>
void proc_enumerator(const char *path, Callback callback) {
	DIR *const dir = opendir(path);
	if (!dir) {
		perror("opendir");
		exit(0);
	}

	while (struct dirent *entry = readdir(dir)) {

		if (entry->d_type != DT_DIR) {
			continue;
		}

		errno          = 0;
		char *end_ptr  = nullptr;
		const long num = strtol(entry->d_name, &end_ptr, 10);

		if (errno == 0 && *end_ptr == '\0') {
			callback(num);
		}
	}

	if (closedir(dir) == -1) {
		perror("closedir");
		exit(0);
	}
}

}

/**
 * @brief
 *
 * @return std::vector<pid_t>
 */
std::vector<pid_t> enumerate_threads(pid_t pid) {
	std::vector<pid_t> threads;

	char path[PATH_MAX];
	::snprintf(path, sizeof(path), "/proc/%d/task/", pid);

	proc_enumerator(path, [&threads](long tid) {
		threads.push_back(static_cast<pid_t>(tid));
	});

	return threads;
}

/**
 * @brief
 *
 * @return std::vector<pid_t>
 */
std::vector<pid_t> enumerate_processes() {

	std::vector<pid_t> processes;

	proc_enumerator("/proc/", [&processes](long pid) {
		processes.push_back(static_cast<pid_t>(pid));
	});

	return processes;
}
