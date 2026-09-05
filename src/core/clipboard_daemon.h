#pragma once

#include <QObject>
#include <QClipboard>
#include <memory>
#include <atomic>
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
    QString last_saved_text_;
};
