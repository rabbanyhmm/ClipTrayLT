#include "item_card.h"
#include <QMouseEvent>
#include <QPixmap>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QApplication>

static QIcon createPinIcon(bool pinned) {
    QPixmap pix(24, 24);
    pix.fill(Qt::transparent);
    QPainter p(&pix);
    p.setRenderHint(QPainter::Antialiasing);

    QColor col = pinned ? QColor("#0078d4") : QColor("#999999");
    p.setPen(QPen(col, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(pinned ? QBrush(col) : Qt::NoBrush);

    // Clean Windows-style diagonal thumbtack
    // Needle pointing to bottom-left
    p.drawLine(5, 17, 9, 13);

    // Pin cylinder body
    QPainterPath body;
    body.moveTo(8, 14);
    body.lineTo(13, 9);
    body.lineTo(15, 11);
    body.lineTo(10, 16);
    body.closeSubpath();
    p.drawPath(body);

    // Top cap
    p.drawLine(12, 8, 16, 12);

    return QIcon(pix);
}

static QString formatByteSize(size_t bytes) {
    if (bytes < 1024) return QString("%1 B").arg(bytes);
    if (bytes < 1024 * 1024) return QString("%1 KB").arg(QString::number(bytes / 1024.0, 'f', 1));
    return QString("%1 MB").arg(QString::number(bytes / (1024.0 * 1024.0), 'f', 1));
}

static bool isBinaryPayload(const std::string& data) {
    if (data.empty()) return false;
    size_t check_len = std::min<size_t>(data.size(), 512);
    size_t non_printable = 0;
    for (size_t i = 0; i < check_len; ++i) {
        unsigned char c = static_cast<unsigned char>(data[i]);
        if (c == 0) return true;
        if (c < 32 && c != '\t' && c != '\n' && c != '\r') {
            non_printable++;
        }
    }
    return (non_printable * 100 / check_len) > 15;
}

ItemCard::ItemCard(const ClipboardRecord& record, QWidget* parent)
    : QWidget(parent), record_(record) {
    setObjectName("ItemCard");
    setAttribute(Qt::WA_StyledBackground, true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);

    auto* main_layout = new QHBoxLayout(this);
    main_layout->setContentsMargins(12, 10, 8, 10);
    main_layout->setSpacing(10);

    // Left content area (Text or Image)
    content_label_ = new QLabel(this);
    content_label_->setObjectName("ItemText");
    content_label_->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    content_label_->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    if (record_.content_type == "image" && !record_.image_data.empty()) {
        QPixmap pixmap;
        pixmap.loadFromData(record_.image_data.data(), static_cast<uint>(record_.image_data.size()));
        if (!pixmap.isNull()) {
            content_label_->setPixmap(pixmap.scaledToHeight(70, Qt::SmoothTransformation));
        } else {
            content_label_->setText("[Image content]");
        }
    } else if (record_.content_type == "raw" || isBinaryPayload(record_.text_content)) {
        content_label_->setWordWrap(true);
        size_t effective_size = (record_.full_size > 0) ? record_.full_size : record_.text_content.size();
        QString size_str = formatByteSize(effective_size);
        size_t hex_len = std::min<size_t>(record_.text_content.size(), 14);
        QString hex_preview;
        for (size_t i = 0; i < hex_len; ++i) {
            hex_preview += QString::asprintf("%02X ", static_cast<unsigned char>(record_.text_content[i]));
        }
        if (effective_size > hex_len) {
            hex_preview += "...";
        }
        QString preview_text = QString("📦 Binary Payload (%1)\n%2").arg(size_str).arg(hex_preview.trimmed());
        content_label_->setText(preview_text);
        content_label_->setMaximumHeight(70);
        setToolTip(QString("Raw Binary Data: %1 (%2 bytes)\nClick or press Enter to paste").arg(size_str).arg(effective_size));
    } else {
        content_label_->setWordWrap(true);
        size_t effective_size = (record_.full_size > 0) ? record_.full_size : record_.text_content.size();
        size_t preview_len = std::min<size_t>(record_.text_content.size(), 400);
        QString text = QString::fromUtf8(record_.text_content.data(), static_cast<int>(preview_len)).trimmed();
        if (effective_size > preview_len || text.length() > 240) {
            text = text.left(240) + "...";
        }
        if (text.isEmpty() && !record_.text_content.empty()) {
            text = QString("[%1 raw binary data]").arg(formatByteSize(effective_size));
        }
        content_label_->setText(text);
        content_label_->setMaximumHeight(70);
        if (effective_size > 32 * 1024) {
            setToolTip(QString("Large Text: %1 (%2 bytes)\nClick or press Enter to paste").arg(formatByteSize(effective_size)).arg(effective_size));
        }
    }
    main_layout->addWidget(content_label_, 1);

    // Right action buttons (Pin & Delete)
    auto* btn_layout = new QVBoxLayout();
    btn_layout->setContentsMargins(0, 0, 0, 0);
    btn_layout->setSpacing(4);
    btn_layout->setAlignment(Qt::AlignTop);

    pin_btn_ = new QPushButton(this);
    pin_btn_->setObjectName("CardActionBtn");
    pin_btn_->setFixedSize(24, 24);
    pin_btn_->setFocusPolicy(Qt::NoFocus);
    pin_btn_->setCursor(Qt::PointingHandCursor);
    updatePinUi();

    connect(pin_btn_, &QPushButton::clicked, this, [this]() {
        record_.is_pinned = !record_.is_pinned;
        updatePinUi();
        emit pinToggled(record_.id);
    });
    btn_layout->addWidget(pin_btn_);

    delete_btn_ = new QPushButton("✕", this);
    delete_btn_->setObjectName("CardActionBtn");
    delete_btn_->setToolTip("Delete");
    delete_btn_->setFixedSize(24, 24);
    delete_btn_->setFocusPolicy(Qt::NoFocus);
    delete_btn_->setCursor(Qt::PointingHandCursor);

    connect(delete_btn_, &QPushButton::clicked, this, [this]() {
        emit deleteRequested(record_.id);
    });
    btn_layout->addWidget(delete_btn_);

    main_layout->addLayout(btn_layout);
}

void ItemCard::updatePinUi() {
    pin_btn_->setText("");
    pin_btn_->setIcon(createPinIcon(record_.is_pinned));
    pin_btn_->setIconSize(QSize(16, 16));
    pin_btn_->setProperty("pinned", record_.is_pinned);
    pin_btn_->setToolTip(record_.is_pinned ? "Unpin item" : "Pin item");
    pin_btn_->style()->unpolish(pin_btn_);
    pin_btn_->style()->polish(pin_btn_);
}

void ItemCard::setSelected(bool selected) {
    if (is_selected_ == selected) return;
    is_selected_ = selected;
    setProperty("selected", selected);
    style()->unpolish(this);
    style()->polish(this);
}

void ItemCard::mousePressEvent(QMouseEvent* event) {
    emit cardInteracted();
    if (event->button() == Qt::LeftButton) {
        is_pressed_ = true;
        event->accept();
        return;
    }
    QWidget::mousePressEvent(event);
}

void ItemCard::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && is_pressed_) {
        is_pressed_ = false;
        if (rect().contains(event->pos())) {
            emit clicked(record_.id);
        }
        event->accept();
        return;
    }
    QWidget::mouseReleaseEvent(event);
}

void ItemCard::mouseDoubleClickEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton) {
        emit clicked(record_.id);
        event->accept();
        return;
    }
    QWidget::mouseDoubleClickEvent(event);
}

void ItemCard::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
}

void ItemCard::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
}
