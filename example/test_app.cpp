
#include <chrono>
#include <cmath>
#include <cstdio>
#include <immintrin.h>
#include <thread>

int main() {

	double angle = 0.0;

	for (int i = 0; i < 10; ++i) {
		// Simulate some work, involving the FPU
		std::printf("Hello, world! Angle: %f\n", std::sin(angle));
		angle += 10.0;
	}
#if 0
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
#endif
	while (1) {
		__asm__ __volatile__("int3");
	}
}
