#include "clipboard_daemon.h"
#include <QGuiApplication>
#include <QMimeData>
#include <QImage>
#include <QBuffer>
#include <iostream>

ClipboardDaemon::ClipboardDaemon(std::shared_ptr<StorageManager> storage, QObject* parent)
    : QObject(parent), storage_(storage) {
    QClipboard* clipboard = QGuiApplication::clipboard();
    connect(clipboard, &QClipboard::dataChanged, this, &ClipboardDaemon::onClipboardChanged);
    std::cout << "[ClipboardDaemon] Event-driven clipboard monitor initialized (0% idle CPU).\n";
}

void ClipboardDaemon::setSelfCopying(bool active) {
    self_copying_ = active;
}

bool ClipboardDaemon::isSelfCopying() const {
    return self_copying_;
}

void ClipboardDaemon::onClipboardChanged() {
    if (self_copying_) {
        // Ignored because this copy was initiated by our own paste action
        return;
    }

    QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData* mime = clipboard->mimeData(QClipboard::Clipboard);
    if (!mime) return;

    if (mime->hasText()) {
        QString text = mime->text();
        if (text.trimmed().isEmpty()) return;

        if (text == last_saved_text_) {
            // Rapid duplicate event ignored
            return;
        }
        last_saved_text_ = text;

        QString html = mime->hasHtml() ? mime->html() : "";
        int64_t id = storage_->addItem("text", text.toStdString(), html.toStdString(), {}, 25);
        std::cout << "[ClipboardDaemon] Captured copied text (ID: " << id
                  << ", Preview: " << text.left(40).toStdString() << "...)\n" << std::flush;
        emit historyUpdated();
        return;
    }

    if (mime->hasImage()) {
        QImage image = qvariant_cast<QImage>(mime->imageData());
        if (!image.isNull()) {
            QByteArray bytes;
            QBuffer buffer(&bytes);
            buffer.open(QIODevice::WriteOnly);
            image.save(&buffer, "PNG");

            std::vector<uint8_t> blob(bytes.begin(), bytes.end());
            int64_t id = storage_->addItem("image", "", "", blob, 25);
            std::cout << "[ClipboardDaemon] Captured copied image (ID: " << id << ")\n" << std::flush;
            emit historyUpdated();
        }
    }
}
