#pragma once

#include <QObject>
#include <thread>
#include <atomic>
#include <vector>

class EvdevListener : public QObject {
    Q_OBJECT
public:
    explicit EvdevListener(QObject* parent = nullptr);
    ~EvdevListener();

    bool start();
    void stop();

signals:
    void hotkeyTriggered();
    void escapePressed();
    void globalMousePressed();

private:
    std::atomic<bool> running_{false};
    std::thread worker_thread_;
    std::vector<int> input_fds_;

    void runLoop();
    void discoverDevices();
    void closeDevices();
};
