#pragma once

#include <atomic>
#include <thread>
#include <string>

class CaretDetector {
public:
    static void initialize();
    static void shutdown();
    static bool isTerminalActive();

private:
    static void runEventLoop();
};
