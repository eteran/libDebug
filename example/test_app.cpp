
#include <chrono>
#include <cmath>
#include <cstdio>
#include <thread>

int main() {

	float angle = 0.0f;

	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::printf("Hello, world! Angle: %f\n", std::sin(angle));
		angle += 10.0f;
	}
}
