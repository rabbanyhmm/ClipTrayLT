#pragma once

#include <QPoint>
#include <optional>
#include <atomic>
#include <chrono>
#include <thread>

struct CaretInfo {
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;
};

class CaretDetector {
public:
    static void initialize();
    static void shutdown();
    static std::optional<CaretInfo> getActiveTextCaret();

private:
    static void runEventLoop();
};
