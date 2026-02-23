

#include "Test.hpp"
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <span>
#include <sys/wait.h>
#include <unistd.h>

constexpr int READ_END  = 0;
constexpr int WRITE_END = 1;

void my_assert_fail(const char *expr, const char *file, int line, const char *msg) {
	if (msg) {
		printf("%s:%d: CHECK '%s' failed: %s\n", file, line, expr, msg);
	} else {
		printf("%s:%d: CHECK '%s' failed.\n", file, line, expr);
	}
	abort();
}

void run_test(test_type &test, bool capture_output = true) {

	int pipefd[2];
	if (pipe(pipefd) == -1) {
		perror("pipe");
		exit(EXIT_FAILURE);
	}

	const pid_t cpid = fork();
	switch (cpid) {
	case 0:
		close(pipefd[READ_END]); // Close unused read end
		if (capture_output) {
			dup2(pipefd[WRITE_END], STDOUT_FILENO); // Redirect stdout to write end of the pipe
			dup2(pipefd[WRITE_END], STDERR_FILENO); // Redirect stderr to write end of the pipe
		}

		test.test_func();
		close(pipefd[WRITE_END]); // Reader will see EOF
		exit(EXIT_SUCCESS);
	case -1:
		perror("fork");
		exit(EXIT_FAILURE);
	default: {
		printf("Running Test %-20s: ", test.name);
		int status;
		int ret = waitpid(cpid, &status, 0);
		if (ret == -1) {
			perror("waitpid");
			exit(EXIT_FAILURE);
		}

		close(pipefd[WRITE_END]); /* Close unused write end */

		test.result = false;
		if (WIFEXITED(status)) {
			const int exit_status = WEXITSTATUS(status);
			if (exit_status == EXIT_SUCCESS) {
				printf("\x1b[32mOK\033[m\n");
				test.result = true;
			} else {
				printf("\x1b[31mFAIL\033[m [Exited, status=%d]\n", WEXITSTATUS(status));
			}
		} else if (WIFSIGNALED(status)) {
			printf("\x1b[31mFAIL\033[m [Killed by signal %d]\n", WTERMSIG(status));
		} else if (WIFSTOPPED(status)) {
			printf("\x1b[33mUNKNOWN\033[m [Stopped by signal %d]\n", WSTOPSIG(status));
		} else if (WIFCONTINUED(status)) {
			printf("\x1b[33mUNKNOWN\033[m [Continued]\n");
		}

		if (!test.result) {
			if (true) {
				printf("Failed Test: [%s] @ %s:%d\n", test.name, test.file, test.line);
			}

			printf("--------------------------------------------------------------------------------\n");
			printf("\x1b[31m");
			char buffer[1];
			while ((read(pipefd[READ_END], buffer, 1)) > 0) {
				putchar(buffer[0]);
			}
			printf("\033[m");
			printf("--------------------------------------------------------------------------------\n");
		}

		close(pipefd[READ_END]);
	}
	}
}

struct Span {
	test_type *test = nullptr;
	size_t count    = 0;

	test_type *begin() const {
		return test;
	}

	test_type *end() const {
		return test + count;
	}
};

Span collect_tests() {
	extern test_type __start_test_array[];
	extern test_type __stop_test_array[];

	const size_t count = (reinterpret_cast<uintptr_t>(__stop_test_array) -
						  reinterpret_cast<uintptr_t>(__start_test_array)) /
						 sizeof(test_type);

#if defined(__cpp_lib_start_lifetime_as) && __cpp_lib_start_lifetime_as >= 202207L
	auto first = std::start_lifetime_as_array<test_type>(__start_test_array, count);
#else
	auto first = &__start_test_array[0];
	__asm__("" : "+r"(first));
#endif

	return Span{first, count};
}

int main(int argc, char *argv[]) {

	bool capture_output = true;
	for (int i = 1; i < argc; ++i) {
		if (std::strcmp(argv[i], "--no-capture-output") == 0) {
			capture_output = false;
		} else {
			printf("Usage: %s [--no-capture-output]\n", argv[0]);
			return EXIT_FAILURE;
		}
	}

	int fail_count = 0;
	for (test_type &test : collect_tests()) {
		run_test(test, capture_output);
		if (!test.result) {
			fail_count++;
		}
	}

	exit(!!fail_count);
}
