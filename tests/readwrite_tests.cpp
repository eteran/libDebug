#include "Test.hpp"
#include "TestHelpers.hpp"

#include <sys/mman.h>

namespace {

void run_readwrite_case(bool disable_proc_mem) {
	with_attached_child(
		[](int addr_write_fd, int sync_read_fd) {
			constexpr size_t PageSize = 4096;
			void *mem                 = mmap(nullptr, PageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
			CHECK_MSG(mem != MAP_FAILED, "mmap failed to allocate memory");

			auto ptr = static_cast<uint8_t *>(mem);
			for (int i = 0; i < 32; ++i) {
				ptr[i] = static_cast<uint8_t>(0xaa + i);
			}

			send_address(addr_write_fd, ptr);
			child_wait_ready(sync_read_fd);
		},
		[](const AttachedContext &ctx) {
			uint8_t buffer[32];
			int64_t nread = ctx.process->read_memory(ctx.address, buffer, sizeof(buffer));
			CHECK_MSG(nread == sizeof(buffer), "proc->read_memory returned unexpected size");
			for (int i = 0; i < 32; ++i) {
				CHECK_MSG(buffer[i] == static_cast<uint8_t>(0xaa + i), "initial pattern mismatch in child memory");
			}

			for (int i = 0; i < 32; ++i) {
				buffer[i] = static_cast<uint8_t>(0x55 + i);
			}
			int64_t nwritten = ctx.process->write_memory(ctx.address, buffer, sizeof(buffer));
			CHECK_MSG(nwritten == sizeof(buffer), "proc->write_memory wrote unexpected number of bytes");

			uint8_t verify_buffer[32];
			nread = ctx.process->read_memory(ctx.address, verify_buffer, sizeof(verify_buffer));
			CHECK_MSG(nread == sizeof(verify_buffer), "proc->read_memory returned unexpected size for verification");
			for (int i = 0; i < 32; ++i) {
				CHECK_MSG(verify_buffer[i] == static_cast<uint8_t>(0x55 + i), "verify buffer mismatch after write");
			}

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();
			ctx.process->kill();
			ctx.process->wait();
		},
		false,
		[disable_proc_mem](Debugger &dbg) {
			dbg.set_disable_proc_mem(disable_proc_mem);
		});
}

void run_readwrite_edges_case(bool disable_proc_mem) {
	with_attached_child(
		[](int addr_write_fd, int sync_read_fd) {
			constexpr size_t PageSize = 4096;
			void *mem                 = mmap(nullptr, PageSize, PROT_READ | PROT_WRITE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
			CHECK_MSG(mem != MAP_FAILED, "mmap failed to allocate memory");

			auto ptr = static_cast<uint8_t *>(mem);
			for (int i = 0; i < 32; ++i) {
				ptr[i] = static_cast<uint8_t>(0xa0 + i);
			}

			send_address(addr_write_fd, ptr);
			child_wait_ready(sync_read_fd);
		},
		[](const AttachedContext &ctx) {
			uint8_t edge[4] = {};

			int64_t nread = ctx.process->read_memory(ctx.address, edge, sizeof(edge));
			CHECK_MSG(nread == sizeof(edge), "read_memory failed at start edge");
			for (int i = 0; i < 4; ++i) {
				CHECK_MSG(edge[i] == static_cast<uint8_t>(0xa0 + i), "start edge read mismatch");
			}

			nread = ctx.process->read_memory(ctx.address + 28, edge, sizeof(edge));
			CHECK_MSG(nread == sizeof(edge), "read_memory failed at end edge");
			for (int i = 0; i < 4; ++i) {
				CHECK_MSG(edge[i] == static_cast<uint8_t>(0xa0 + 28 + i), "end edge read mismatch");
			}

			uint8_t write_start[4] = {0x11, 0x22, 0x33, 0x44};
			int64_t nwritten       = ctx.process->write_memory(ctx.address, write_start, sizeof(write_start));
			CHECK_MSG(nwritten == sizeof(write_start), "write_memory failed at start edge");

			uint8_t write_end[4] = {0xaa, 0xbb, 0xcc, 0xdd};
			nwritten             = ctx.process->write_memory(ctx.address + 28, write_end, sizeof(write_end));
			CHECK_MSG(nwritten == sizeof(write_end), "write_memory failed at end edge");

			uint8_t verify[32] = {};
			nread              = ctx.process->read_memory(ctx.address, verify, sizeof(verify));
			CHECK_MSG(nread == sizeof(verify), "verification read failed");

			for (int i = 0; i < 4; ++i) {
				CHECK_MSG(verify[i] == write_start[i], "start edge write verification mismatch");
			}

			for (int i = 4; i < 28; ++i) {
				CHECK_MSG(verify[i] == static_cast<uint8_t>(0xa0 + i), "middle region changed unexpectedly");
			}

			for (int i = 0; i < 4; ++i) {
				CHECK_MSG(verify[28 + i] == write_end[i], "end edge write verification mismatch");
			}

			notify_child_start(ctx.sync_write_fd);
			ctx.process->resume();
			ctx.process->kill();
			ctx.process->wait();
		},
		false,
		[disable_proc_mem](Debugger &dbg) {
			dbg.set_disable_proc_mem(disable_proc_mem);
		});
}

}

TEST(ReadWrite) {
	run_readwrite_case(false);
	run_readwrite_case(true);
}

TEST(ReadWriteEdges) {
	run_readwrite_edges_case(false);
	run_readwrite_edges_case(true);
}
