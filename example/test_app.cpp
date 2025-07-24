
#include <chrono>
#include <cmath>
#include <cstdio>
#include <immintrin.h>
#include <thread>

int main() {
	__attribute__((aligned(32))) const uint32_t data[] = {
		0x11111111,
		0x22222222,
		0x33333333,
		0x44444444,
		0x55555555,
		0x66666666,
		0x77777777,
		0x88888888,
	};

	__asm__ __volatile__("vmovaps %0, %%xmm7" : : "m"(data));
	while (1) {
		__asm__ __volatile__("int3");
	}
#if 0
	float angle = 0.0f;

	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::printf("Hello, world! Angle: %f\n", std::sin(angle));
		angle += 10.0f;
	}
#endif
}
