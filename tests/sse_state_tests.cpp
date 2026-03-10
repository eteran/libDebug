#include "Debug/Context.hpp"
#include "Debug/Event.hpp"
#include "Debug/EventStatus.hpp"
#include "Debug/Thread.hpp"
#include "Test.hpp"
#include "TestHelpers.hpp"

#include <chrono>
#include <cstdint>
#include <cstring>

using namespace std::chrono_literals;

TEST(SseStateGetSetRoundTrip) {
	with_attached_child(
		[](int addr_write_fd, int sync_read_fd) {
			send_dummy_address(addr_write_fd);
			child_wait_ready(sync_read_fd);

			alignas(16) uint32_t xmm0_init[4] = {
				0x11111111,
				0x22222222,
				0x33333333,
				0x44444444,
			};

			uint32_t mxcsr_init = 0x5f80;

			__asm__ __volatile__(
				"ldmxcsr (%0)\n"
				"movdqu (%1), %%xmm0\n"
				"int3\n"
				:
				: "r"(&mxcsr_init), "r"(xmm0_init)
				: "memory");

			_exit(0);
		},
		[](const AttachedContext &ctx) {
			ctx.process->resume();
			notify_child_start(ctx.sync_write_fd);

			enum class TestPhase {
				WaitingTrap,
				ManipulatedContext,
				WaitingExit
			};

			TestPhase phase = TestPhase::WaitingTrap;

			auto callback = [&](const Event &e) -> EventStatus {
				if (e.tid != ctx.child_pid) {
					return EventStatus::Continue;
				}

				if (phase == TestPhase::WaitingTrap && e.type == Event::Type::Stopped && e.siginfo.si_signo == SIGTRAP) {
					auto thread = ctx.process->find_thread(ctx.child_pid);
					CHECK_MSG(thread != nullptr, "failed to find stopped thread");

					Context context;
					thread->get_context(&context);
					CHECK_MSG(context.is_set(), "get_context did not populate context");

					const uint32_t mxcsr_read = context[RegisterId::MXCSR].as<uint32_t>();
					CHECK_MSG((mxcsr_read & 0x6000U) == (0x5f80U & 0x6000U), "MXCSR rounding mode mismatch after SSE setup");

					const uint8_t expected_xmm0[16] = {
						0x11, 0x11, 0x11, 0x11,
						0x22, 0x22, 0x22, 0x22,
						0x33, 0x33, 0x33, 0x33,
						0x44, 0x44, 0x44, 0x44,
					};

					auto xmm0_ref = context[RegisterId::XMM0];
					CHECK_MSG(std::memcmp(xmm0_ref.data(), expected_xmm0, sizeof(expected_xmm0)) == 0, "XMM0 mismatch after SSE setup");

					const uint8_t new_xmm0[16] = {
						0xaa, 0xbb, 0xcc, 0xdd,
						0x10, 0x20, 0x30, 0x40,
						0x55, 0x66, 0x77, 0x88,
						0x99, 0x00, 0xfe, 0xef,
					};

					std::memcpy(xmm0_ref.data(), new_xmm0, sizeof(new_xmm0));
					context[RegisterId::MXCSR] = static_cast<uint32_t>(0x1f80);
					thread->set_context(&context);

					Context verify;
					thread->get_context(&verify);
					CHECK_MSG(verify[RegisterId::MXCSR].as<uint32_t>() == 0x1f80U, "MXCSR write/readback mismatch");
					CHECK_MSG(std::memcmp(verify[RegisterId::XMM0].data(), new_xmm0, sizeof(new_xmm0)) == 0, "XMM0 write/readback mismatch");

					phase = TestPhase::ManipulatedContext;

					return EventStatus::Continue;
				}

				if (phase == TestPhase::ManipulatedContext || phase == TestPhase::WaitingExit) {
					if (e.type == Event::Type::Exited || e.type == Event::Type::Terminated) {
						phase = TestPhase::WaitingExit;
						return EventStatus::Continue;
					}
				}

				return EventStatus::Continue;
			};

			const bool observed = poll_debug_events_until(
				ctx.process,
				5s,
				std::chrono::milliseconds(200),
				callback,
				[&]() { return phase == TestPhase::WaitingExit; });
			CHECK_MSG(observed, "did not observe child exit after SSE test");

			ctx.process->wait();
		});
}
