
#include <chrono>
#include <cstdio>
#include <thread>

int main() {

	while (1) {
		std::this_thread::sleep_for(std::chrono::seconds(1));
		std::printf("Hello, world!\n");
	}

}
