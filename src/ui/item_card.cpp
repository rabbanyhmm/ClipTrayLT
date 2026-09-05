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

    if (record_.content_type == "image" && !record_.image_data.empty()) {
        QPixmap pixmap;
        pixmap.loadFromData(record_.image_data.data(), static_cast<uint>(record_.image_data.size()));
        if (!pixmap.isNull()) {
            content_label_->setPixmap(pixmap.scaledToHeight(70, Qt::SmoothTransformation));
        } else {
            content_label_->setText("[Image content]");
        }
    } else {
        content_label_->setWordWrap(true);
        QString text = QString::fromStdString(record_.text_content).trimmed();
        if (text.length() > 240) {
            text = text.left(240) + "...";
        }
        content_label_->setText(text);
        content_label_->setMaximumHeight(70);
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
        emit clicked(record_.id);
    }
    QWidget::mousePressEvent(event);
}

void ItemCard::enterEvent(QEnterEvent* event) {
    QWidget::enterEvent(event);
}

void ItemCard::leaveEvent(QEvent* event) {
    QWidget::leaveEvent(event);
}
