
#include "Debug/Debugger.hpp"
#include "Debug/DebuggerError.hpp"
#include "Debug/Proc.hpp"
#include "Debug/Process.hpp"
#include "Debug/Region.hpp"
#include "Debug/Thread.hpp"

#include <cctype>
#include <cinttypes>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

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
				throw DebuggerError("Failed to read memory at %016" PRIx64 ": %s", first, strerror(errno));
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

	auto debugger = std::make_unique<Debugger>();
	debugger->set_disable_aslr(true);
	debugger->set_disable_lazy_binding(true);

#define TEST64

	const char *argv[] = {
#ifdef TEST64
		"./TestApp64",
#else
		"./TestApp32",
#endif
		nullptr,
	};

	std::shared_ptr<Process> process;

	try {
		process = debugger->spawn(nullptr, argv);
	} catch (const DebuggerError &e) {
		std::printf("Debugger Error: %s\n", e.what());
		return 1;
	}

#if 0
#ifdef TEST64
	process->add_breakpoint(0x40190c); // main of TestApp on my machine (64-bit)
#else
	process->add_breakpoint(0x080498da); // main of TestApp on my machine (32-bit)
#endif
#endif

	uint64_t prev_memory_map_hash = hash_regions(process->pid());
	auto regions                  = enumerate_regions(process->pid());

	dump_regions(regions);
	if (!regions.empty()) {
		dump_memory(process.get(), regions[0].start(), 256);
	}

	process->resume();

	for (int i = 0; i < 10; ++i) {
		if (!process->next_debug_event(std::chrono::seconds(10), [&]([[maybe_unused]] const Event &e) {
				std::printf("Debug Event!\n");

				const uint64_t current_memory_map_hash = hash_regions(process->pid());
				if (current_memory_map_hash != prev_memory_map_hash) {
					prev_memory_map_hash = current_memory_map_hash;
					regions              = enumerate_regions(process->pid());
					std::printf("Memory Map Changed!\n");
				}

				process->report();

				auto current = process->active_thread();

				// EXPERIMENT: copy XMM7 to XMM0
				Context ctx;
				current->get_context(&ctx);
				ctx[RegisterId::YMM0] = ctx[RegisterId::YMM7];
				current->set_context(&ctx);

				printf("Instruction Pointer: %016" PRIx64 "\n", ctx.get(RegisterId::XIP).as<uint64_t>());
				printf("Instruction Pointer (Alt): %016" PRIx64 "\n", current->get_instruction_pointer());
				current->set_instruction_pointer(current->get_instruction_pointer());

				return EventStatus::Stop;
			})) {
			std::printf("Timeout!\n");
			std::exit(0);
		}
	}

	std::printf("Done Stepping\n");
	process->kill();
}
