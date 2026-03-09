#include "Test.hpp"
#include "TestHelpers.hpp"

#include <sys/mman.h>

TEST(ReadWrite) {
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
		});
}
