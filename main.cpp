
#include "Debugger.hpp"
#include "Proc.hpp"
#include "Process.hpp"
#include "Region.hpp"
#include "Thread.hpp"

#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

void tracee() {
#if 1
	for (int i = 0; i < 5; ++i) {
		auto thr = std::thread([](int n) {
			int j = 0;
			while (1) {
				printf("In child, doing some work [%d]...\n", n);
				std::this_thread::sleep_for(std::chrono::seconds(1));

				if (j++ == 3 && n == 1) {
					printf("Exiting Thread!\n");
					return;
				}
			}
		},
							   i);

		thr.detach();
	}
#endif
	while (1) {
		printf("In child, doing some work [%d]...\n", -1);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void tracer(pid_t cpid) {
	auto debugger = std::make_unique<Debugger>();
	auto process  = debugger->attach(cpid);
	process->resume();

	std::this_thread::sleep_for(std::chrono::seconds(5));
	process->stop();

	while (1) {
		if (!process->next_debug_event(std::chrono::seconds(10), []([[maybe_unused]] const Event &e) {
				printf("Debug Event!\n");
				return EventStatus::Stop;
			})) {
			printf("Timeout!\n");
			::exit(0);
		}
	}
}

/**
 * @brief dumps the memory of a given process
 *
 * @param process the process to dump memory from
 * @param address the address to start dumping from
 * @param n the number of bytes to dump
 */
void dump_memory(Process *process, uint64_t address, size_t n) {

	uint64_t first            = address;
	const uint64_t last       = address + n;
	int64_t remaining         = 0;
	size_t buffer_index       = 0;
	constexpr size_t StepSize = 16;

	uint8_t buffer[4096] = {};

	while (first < last) {
		if (remaining <= 0) {
			remaining = process->read_memory(first, buffer, sizeof(buffer));
			if (remaining == -1) {
				printf("Error Reading Memory\n");
				return;
			}
			buffer_index = 0;
		}

		const size_t line_end = std::min(StepSize, last - first);

		::printf("%016lx: ", first);
		for (size_t i = 0; i < line_end; ++i) {
			::printf("%02x ", buffer[buffer_index + i]);
		}

		for (size_t i = 0; i < line_end; ++i) {
			const uint8_t ch = buffer[buffer_index + i];
			::printf("%c", ::isprint(ch) ? ch : '.');
		}

		::printf("\n");

		buffer_index += StepSize;
		first += StepSize;
		remaining -= StepSize;
	}
}

/**
 * @brief main function
 *
 * @return int
 */
int main() {

#if 0
	switch (const pid_t cpid = fork()) {
	case 0:
		tracee();
		break;
	case -1:
		::perror("fork");
		::exit(1);
	default:
		tracer(cpid);
		break;
	}
#else
	auto debugger = std::make_unique<Debugger>();

	const char *argv[] = {
		"/usr/bin/sleep",
		"999",
		nullptr,
	};


	auto process = debugger->spawn("/etc/", argv);
	if(!process) {
		printf("Failed to spawn process\n");
		return 1;
	}

	process->add_breakpoint(0x00007ffff7fe4540);
	uint64_t prev_memory_map_hash = hash_regions(process->pid());
	auto regions                  = enumerate_regions(process->pid());
	for (const auto &region : regions) {
		printf("Region: %016lx - %016lx: %s\n", region.start(), region.end(), region.name().c_str());
	}
	dump_memory(process.get(), 0x00007ffff7fe4500, 256);
	process->resume();

	for (int i = 0; i < 20; ++i) {
		if (!process->next_debug_event(std::chrono::seconds(10), [&]([[maybe_unused]] const Event &e) {
				printf("Debug Event!\n");

				uint64_t current_memory_map_hash = hash_regions(process->pid());
				if (current_memory_map_hash != prev_memory_map_hash) {
					prev_memory_map_hash = current_memory_map_hash;
					regions              = enumerate_regions(process->pid());
					printf("Memory Map Changed!\n");
				}

				return EventStatus::Stop;
			})) {
			printf("Timeout!\n");
			::exit(0);
		}
	}

	printf("Done Stepping\n");
	process->detach();
#endif
}
