#include "flyout_window.h"
#include "style.h"
#include "config.h"
#include <QScreen>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <QMimeData>
#include <QImage>
#include <QScrollBar>
#include <QTimer>
#include <iostream>
#include <thread>

FlyoutWindow::FlyoutWindow(std::shared_ptr<StorageManager> storage,
                           std::shared_ptr<PasteInjector> paste_injector,
                           std::shared_ptr<ClipboardDaemon> clip_daemon,
                           QWidget* parent)
    : QWidget(parent),
      storage_(storage),
      paste_injector_(paste_injector),
      clip_daemon_(clip_daemon),
      last_show_time_(std::chrono::steady_clock::now() - std::chrono::seconds(10)) {

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool | Qt::WindowDoesNotAcceptFocus);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setAttribute(Qt::WA_ShowWithoutActivating, true);
    setAttribute(Qt::WA_X11DoNotAcceptFocus, true);
    setFocusPolicy(Qt::NoFocus);
    setFixedSize(360, 480);
    setStyleSheet(Style::WIN10_FLYOUT_STYLE);

    auto* outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(10, 10, 10, 10);

    container_ = new QWidget(this);
    container_->setObjectName("FlyoutWindow");
    container_->setFocusPolicy(Qt::NoFocus);

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 6);
    container_->setGraphicsEffect(shadow);

    auto* main_layout = new QVBoxLayout(container_);
    main_layout->setContentsMargins(14, 14, 14, 14);
    main_layout->setSpacing(10);

    // Top Header: "Clipboard" + "Clear all"
    auto* header_layout = new QHBoxLayout();
    auto* header_title = new QLabel("Clipboard", container_);
    header_title->setObjectName("HeaderTitle");
    header_layout->addWidget(header_title);

    header_layout->addStretch();

    clear_all_btn_ = new QPushButton("Clear all", container_);
    clear_all_btn_->setObjectName("ClearAllBtn");
    clear_all_btn_->setFocusPolicy(Qt::NoFocus);
    clear_all_btn_->setCursor(Qt::PointingHandCursor);
    connect(clear_all_btn_, &QPushButton::clicked, this, &FlyoutWindow::onClearAllClicked);
    header_layout->addWidget(clear_all_btn_);

    main_layout->addLayout(header_layout);

    // Smooth Kinetic Scroll Area
    scroll_area_ = new SmoothScrollArea(container_);
    scroll_area_->setObjectName("HistoryScroll");
    scroll_area_->setFocusPolicy(Qt::NoFocus);
    scroll_area_->setWidgetResizable(true);
    scroll_area_->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    list_container_ = new QWidget(scroll_area_);
    list_container_->setStyleSheet("background: transparent;");
    list_container_->setFocusPolicy(Qt::NoFocus);
    list_layout_ = new QVBoxLayout(list_container_);
    list_layout_->setContentsMargins(0, 0, 0, 0);
    list_layout_->setSpacing(6);
    list_layout_->setAlignment(Qt::AlignTop);

    scroll_area_->setWidget(list_container_);
    main_layout->addWidget(scroll_area_, 1);

    // Empty state widget
    empty_state_ = new QWidget(container_);
    empty_state_->setFocusPolicy(Qt::NoFocus);
    auto* empty_layout = new QVBoxLayout(empty_state_);
    empty_layout->setAlignment(Qt::AlignCenter);
    empty_layout->setSpacing(6);

    auto* empty_icon = new QLabel("📋", empty_state_);
    empty_icon->setStyleSheet("font-size: 38px;");
    empty_icon->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_icon);

    auto* empty_title = new QLabel("Your clipboard is empty", empty_state_);
    empty_title->setObjectName("EmptyTitle");
    empty_title->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_title);

    auto* empty_subtitle = new QLabel("When you copy or cut text and images, they'll appear here.", empty_state_);
    empty_subtitle->setObjectName("EmptySubtitle");
    empty_subtitle->setWordWrap(true);
    empty_subtitle->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_subtitle);

    main_layout->addWidget(empty_state_, 1);
    empty_state_->hide();

    outer_layout->addWidget(container_);

    if (qApp) {
        qApp->installEventFilter(this);
    }

    // Pre-calculate position and pre-load items in memory
    positionAtBottomRight();
    reloadHistory();
    history_dirty_ = false;

    // Real-time clipboard update
    connect(clip_daemon_.get(), &ClipboardDaemon::historyUpdated, this, [this]() {
        history_dirty_ = true;
        if (isVisible()) {
            reloadHistory();
            history_dirty_ = false;
        }
    });
}

void FlyoutWindow::toggleFlyout() {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_show_time_).count();

    if (isVisible()) {
        if (elapsed < 300) {
            return;
        }
        hideFlyout();
    } else {
        showFlyout();
    }
}

void FlyoutWindow::showFlyout() {
    last_show_time_ = std::chrono::steady_clock::now();
    was_clicked_inside_ = false;

    if (history_dirty_.load()) {
        reloadHistory();
        history_dirty_ = false;
    }

    QPoint target_pos = calculateBottomRightPosition();
    QPoint start_pos = target_pos + QPoint(0, 22);

    if (anim_group_) {
        anim_group_->stop();
        delete anim_group_;
    }

    qreal start_opacity = isVisible() ? windowOpacity() : 0.0;
    QPoint curr_pos = isVisible() ? pos() : start_pos;

    move(curr_pos);
    setWindowOpacity(start_opacity);
    show();
    raise();

    anim_group_ = new QParallelAnimationGroup(this);

    int appear_ms = Config::get().anim_appear_ms;
    auto* opacity_anim = new QPropertyAnimation(this, "windowOpacity");
    opacity_anim->setDuration(appear_ms);
    opacity_anim->setStartValue(start_opacity);
    opacity_anim->setEndValue(1.0);
    opacity_anim->setEasingCurve(QEasingCurve::OutCubic);

    auto* pos_anim = new QPropertyAnimation(this, "pos");
    pos_anim->setDuration(appear_ms);
    pos_anim->setStartValue(curr_pos);
    pos_anim->setEndValue(target_pos);
    pos_anim->setEasingCurve(QEasingCurve::OutCubic);

    anim_group_->addAnimation(opacity_anim);
    anim_group_->addAnimation(pos_anim);

    connect(anim_group_, &QParallelAnimationGroup::finished, this, [this, target_pos]() {
        move(target_pos);
        setWindowOpacity(1.0);
    });

    anim_group_->start(QAbstractAnimation::DeleteWhenStopped);

    std::cout << "[Flyout] Window smoothly opening at bottom-right corner.\n" << std::flush;
}

void FlyoutWindow::hideFlyout() {
    if (!isVisible()) return;

    if (anim_group_) {
        anim_group_->stop();
        delete anim_group_;
    }

    qreal start_opacity = windowOpacity();
    QPoint curr_pos = pos();
    QPoint end_pos = curr_pos + QPoint(0, 16);

    anim_group_ = new QParallelAnimationGroup(this);

    int hide_ms = Config::get().anim_hide_ms;
    auto* opacity_anim = new QPropertyAnimation(this, "windowOpacity");
    opacity_anim->setDuration(hide_ms);
    opacity_anim->setStartValue(start_opacity);
    opacity_anim->setEndValue(0.0);
    opacity_anim->setEasingCurve(QEasingCurve::InCubic);

    auto* pos_anim = new QPropertyAnimation(this, "pos");
    pos_anim->setDuration(hide_ms);
    pos_anim->setStartValue(curr_pos);
    pos_anim->setEndValue(end_pos);
    pos_anim->setEasingCurve(QEasingCurve::InCubic);

    anim_group_->addAnimation(opacity_anim);
    anim_group_->addAnimation(pos_anim);

    connect(anim_group_, &QParallelAnimationGroup::finished, this, [this]() {
        hide();
        setWindowOpacity(1.0);
        std::cout << "[Flyout] Window smoothly hidden.\n" << std::flush;
    });

    anim_group_->start(QAbstractAnimation::DeleteWhenStopped);
}

QPoint FlyoutWindow::calculateBottomRightPosition() {
    QScreen* screen = QGuiApplication::screenAt(QCursor::pos());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    if (!screen) return pos();
    QRect work = screen->availableGeometry();
    int x = work.right() - width() - 16;
    int y = work.bottom() - height() - 16;
    return QPoint(x, y);
}

void FlyoutWindow::positionAtBottomRight() {
    move(calculateBottomRightPosition());
}

void FlyoutWindow::handleGlobalClick() {
    if (!isVisible()) return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_show_time_).count();
    if (elapsed < 200) {
        return;
    }

    was_clicked_inside_ = false;

    QTimer::singleShot(40, this, [this]() {
        if (!isVisible()) return;
        if (was_clicked_inside_) {
            was_clicked_inside_ = false;
            return;
        }
        std::cout << "[Flyout] Confirmed outside click. Dismissing flyout.\n" << std::flush;
        hideFlyout();
    });
}

void FlyoutWindow::reloadHistory() {
    for (auto* card : cards_) {
        list_layout_->removeWidget(card);
        card->deleteLater();
    }
    cards_.clear();
    selected_index_ = -1;

    // Load items up to Config limit
    auto items = storage_->getItems();
    if (items.empty()) {
        scroll_area_->hide();
        clear_all_btn_->setEnabled(false);
        empty_state_->show();
        return;
    }

    empty_state_->hide();
    scroll_area_->show();
    clear_all_btn_->setEnabled(true);

    for (const auto& item : items) {
        auto* card = new ItemCard(item, list_container_);
        card->installEventFilter(this);
        connect(card, &ItemCard::clicked, this, &FlyoutWindow::onCardClicked);
        connect(card, &ItemCard::cardInteracted, this, [this]() {
            was_clicked_inside_ = true;
        });
        connect(card, &ItemCard::pinToggled, this, &FlyoutWindow::onPinToggled);
        connect(card, &ItemCard::deleteRequested, this, &FlyoutWindow::onDeleteRequested);

        list_layout_->addWidget(card);
        cards_.push_back(card);
    }

    // Do NOT select or highlight any card by default - user must explicitly click!
    selected_index_ = -1;
}

void FlyoutWindow::updateSelection(int new_index) {
    if (new_index < 0 || new_index >= static_cast<int>(cards_.size())) return;

    if (selected_index_ >= 0 && selected_index_ < static_cast<int>(cards_.size())) {
        cards_[selected_index_]->setSelected(false);
    }
    selected_index_ = new_index;
    cards_[selected_index_]->setSelected(true);

    scroll_area_->ensureWidgetVisible(cards_[selected_index_]);
}

void FlyoutWindow::onCardClicked(int64_t id) {
    std::cout << "[Flyout] onCardClicked triggered by user click for ID=" << id << "\n" << std::flush;
    hideFlyout();

    auto item_opt = storage_->getItemById(id);
    if (!item_opt.has_value()) return;

    const auto& item = item_opt.value();

    clip_daemon_->setSelfCopying(true);

    QClipboard* clipboard = QGuiApplication::clipboard();
    if (item.content_type == "image" && !item.image_data.empty()) {
        QPixmap pix;
        pix.loadFromData(item.image_data.data(), static_cast<uint>(item.image_data.size()));
        clipboard->setPixmap(pix);
    } else {
        auto* mime = new QMimeData();
        mime->setText(QString::fromStdString(item.text_content));
        if (!item.html_content.empty()) {
            mime->setHtml(QString::fromStdString(item.html_content));
        }
        clipboard->setMimeData(mime);
    }

    clip_daemon_->setSelfCopying(false);

    auto injector = paste_injector_;
    std::thread([injector]() {
        injector->paste(35);
    }).detach();
}

void FlyoutWindow::onPinToggled(int64_t id) {
    storage_->togglePin(id);
}

void FlyoutWindow::onDeleteRequested(int64_t id) {
    storage_->deleteItem(id);
    reloadHistory();
}

void FlyoutWindow::onClearAllClicked() {
    storage_->clearUnpinned();
    reloadHistory();
}

void FlyoutWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hideFlyout();
        event->accept();
        return;
    }

    if (cards_.empty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Down) {
        int next_idx = (selected_index_ + 1) % static_cast<int>(cards_.size());
        updateSelection(next_idx);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Up) {
        int prev_idx = selected_index_ - 1;
        if (prev_idx < 0) prev_idx = static_cast<int>(cards_.size()) - 1;
        updateSelection(prev_idx);
        event->accept();
        return;
    }

    // No auto-paste on Enter/Return! Pasting only happens on explicit mouse click on a card!
    QWidget::keyPressEvent(event);
}

bool FlyoutWindow::eventFilter(QObject* watched, QEvent* event) {
    if (event->type() == QEvent::MouseButtonPress) {
        was_clicked_inside_ = true;
    }
    return QWidget::eventFilter(watched, event);
}
