#include "Debug/Process.hpp"
#include "Debug/Event.hpp"
#include "Debug/EventStatus.hpp"
#include "Test.hpp"

#include <chrono>
#include <csignal>
#include <functional>
#include <sys/wait.h>
#include <thread>
#include <cstdio>
#include <unistd.h>

using namespace std::chrono_literals;

TEST(IgnoreSignal) {
    pid_t child = fork();
    if (child == 0) {
        // Child: give parent time to attach, then exit normally with status 42
        std::this_thread::sleep_for(500ms);
        _exit(42);
    }

    // Parent
    Process proc(child, Process::Attach);
    // Let attach complete and resume the child so signals are delivered
    proc.resume();

    // Ignore SIGUSR1 for this process
    proc.ignore_signal(SIGUSR1);

    bool callback_called = false;
    auto cb = [&](const Event &e) -> EventStatus {
        (void)e;
        callback_called = true;
        return EventStatus::Stop;
    };

    // Send the ignored signal
    kill(child, SIGUSR1);

    // Wait for the event loop to process the signal
    proc.next_debug_event(500ms, cb);

    CHECK_MSG(!callback_called, "Ignored signal should not trigger event callback");

    // Cleanup
    proc.detach();
    kill(child, SIGKILL);
    waitpid(child, nullptr, 0);
}
