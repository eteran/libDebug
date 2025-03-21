
#include "Debugger.hpp"
#include "DebuggerError.hpp"
#include "Proc.hpp"
#include "Process.hpp"
#include "Region.hpp"
#include "Thread.hpp"

#include <cctype>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

void tracee() {
#if 1
	for (int i = 0; i < 5; ++i) {
		auto thr = std::thread([](int n) {
			int j = 0;
			while (true) {
				std::printf("In child, doing some work [%d]...\n", n);
				std::this_thread::sleep_for(std::chrono::seconds(1));

				if (j++ == 3 && n == 1) {
					std::printf("Exiting Thread!\n");
					return;
				}
			}
		},
							   i);

		thr.detach();
	}
#endif
	while (true) {
		std::printf("In child, doing some work [%d]...\n", -1);
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void tracer(pid_t cpid) {
	auto debugger = std::make_unique<Debugger>();
	auto process  = debugger->attach(cpid);
	process->resume();

	std::this_thread::sleep_for(std::chrono::seconds(5));
	process->stop();

	while (true) {
		if (!process->next_debug_event(std::chrono::seconds(10), []([[maybe_unused]] const Event &e) {
				std::printf("Debug Event!\n");
				return EventStatus::Stop;
			})) {
			std::printf("Timeout!\n");
			std::exit(0);
		}
	}
}

/**
 * @brief Dumps the regions of a given process.
 *
 * @param regions The regions to dump.
 */
void dump_regions(const std::vector<Region> &regions) {
	for (const auto &region : regions) {
		std::printf("Region: %016" PRIx64 " - %016" PRIx64 ": %s\n", region.start(), region.end(), region.name().c_str());
	}
}

/**
 * @brief Dumps the memory of a given process.
 *
 * @param process The process to dump memory from.
 * @param address The address to start dumping from.
 * @param n The number of bytes to dump.
 */
void dump_memory(Process *process, uint64_t address, size_t n) {

	uint64_t first             = address;
	const uint64_t last        = address + n;
	int64_t remaining          = 0;
	int64_t buffer_index       = 0;
	constexpr int64_t StepSize = 16;

	uint8_t buffer[4096] = {};

	while (first < last) {
		if (remaining <= 0) {
			remaining = process->read_memory(first, buffer, sizeof(buffer));
			if (remaining == -1) {
				std::printf("Error Reading Memory\n");
				return;
			}
			buffer_index = 0;
		}

		const int64_t line_end = std::min(StepSize, static_cast<int64_t>(last - first));

		std::printf("%016" PRIx64 ": ", first);
		for (int64_t i = 0; i < line_end; ++i) {
			std::printf("%02x ", buffer[buffer_index + i]);
		}

		for (int64_t i = 0; i < line_end; ++i) {
			const uint8_t ch = buffer[buffer_index + i];
			std::printf("%c", std::isprint(ch) ? ch : '.');
		}

		std::printf("\n");

		buffer_index += StepSize;
		first += StepSize;
		remaining -= StepSize;
	}
}

/**
 * @brief Main function.
 *
 * @return 0 on success, non-zero on failure.
 */
int main() {

#if 0
	switch (const pid_t cpid = fork()) {
	case 0:
		tracee();
		break;
	case -1:
		std::perror("fork");
		std::exit(1);
	default:
		tracer(cpid);
		break;
	}
#else
	auto debugger = std::make_unique<Debugger>();

	const char *argv[] = {
		"./TestApp32",
		nullptr,
	};

	std::shared_ptr<Process> process;

	try {
		process = debugger->spawn(nullptr, argv);
	} catch (const DebuggerError &e) {
		std::printf("Debugger Error: %s\n", e.what());
		return 1;
	}

#if 1
	process->add_breakpoint(0x56556090); // main of TestApp on my machine (32-bit)
#else
	process->add_breakpoint(0x00005555555551a9); // main of TestApp on my machine (64-bit)
#endif

	uint64_t prev_memory_map_hash = hash_regions(process->pid());
	auto regions                  = enumerate_regions(process->pid());

	dump_regions(regions);
	dump_memory(process.get(), 0x00007ffff7fe4500, 256);
	process->resume();

	for (int i = 0; i < 20; ++i) {
		if (!process->next_debug_event(std::chrono::seconds(10), [&]([[maybe_unused]] const Event &e) {
				std::printf("Debug Event!\n");

				const uint64_t current_memory_map_hash = hash_regions(process->pid());
				if (current_memory_map_hash != prev_memory_map_hash) {
					prev_memory_map_hash = current_memory_map_hash;
					regions              = enumerate_regions(process->pid());
					std::printf("Memory Map Changed!\n");
				}

				process->report();

				return EventStatus::Stop;
			})) {
			std::printf("Timeout!\n");
			std::exit(0);
		}
	}

	std::printf("Done Stepping\n");
	process->kill();
#endif
}
