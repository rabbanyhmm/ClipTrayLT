#pragma once

#include <QObject>
#include <QClipboard>
#include <memory>
#include <atomic>
#include <chrono>
#include "storage.h"

class ClipboardDaemon : public QObject {
    Q_OBJECT
public:
    explicit ClipboardDaemon(std::shared_ptr<StorageManager> storage, QObject* parent = nullptr);

    void setSelfCopying(bool active);
    bool isSelfCopying() const;

signals:
    void historyUpdated();

private slots:
    void onClipboardChanged();

private:
    std::shared_ptr<StorageManager> storage_;
    std::atomic<bool> self_copying_{false};
    std::string last_saved_raw_;
    std::chrono::steady_clock::time_point last_text_copy_time_{};
    QByteArray last_saved_image_bytes_;
    std::chrono::steady_clock::time_point last_image_copy_time_{};
};
