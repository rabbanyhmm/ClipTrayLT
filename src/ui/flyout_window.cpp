#include "flyout_window.h"
#include "style.h"
#include "config.h"
#include <QScreen>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QGraphicsDropShadowEffect>
#include <QIcon>
#include <QPixmap>
#include <QMimeData>
#include <QImage>
#include <QScrollBar>
#include <QTimer>
#include <iostream>
#include <thread>
#include <cstring>
#include <X11/Xlib.h>
#include <X11/Xatom.h>

static unsigned long getActiveX11Window() {
    Display* display = XOpenDisplay(nullptr);
    if (!display) return 0;
    Atom net_active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;
    unsigned long active_win = 0;
    if (XGetWindowProperty(display, DefaultRootWindow(display), net_active, 0, 1, False,
                           XA_WINDOW, &actual_type, &actual_format, &nitems, &bytes_after, &prop) == Success && prop) {
        if (actual_type == XA_WINDOW && actual_format == 32 && nitems > 0) {
            active_win = *reinterpret_cast<Window*>(prop);
        }
        XFree(prop);
    }
    XCloseDisplay(display);
    return active_win;
}

static void restoreActiveX11Window(unsigned long win) {
    if (win == 0) return;
    Display* display = XOpenDisplay(nullptr);
    if (!display) return;
    XEvent event;
    std::memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = win;
    event.xclient.message_type = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = 1; // 1 = normal application request
    event.xclient.data.l[1] = CurrentTime;
    event.xclient.data.l[2] = 0;
    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XSetInputFocus(display, win, RevertToPointerRoot, CurrentTime);
    XFlush(display);
    XCloseDisplay(display);
}

static void forceX11WindowActive(unsigned long win) {
    if (win == 0) return;
    Display* display = XOpenDisplay(nullptr);
    if (!display) return;

    Atom net_active = XInternAtom(display, "_NET_ACTIVE_WINDOW", False);
    XEvent event;
    std::memset(&event, 0, sizeof(event));
    event.xclient.type = ClientMessage;
    event.xclient.window = win;
    event.xclient.message_type = net_active;
    event.xclient.format = 32;
    event.xclient.data.l[0] = 2; // 2 = user direct action / pager
    event.xclient.data.l[1] = CurrentTime;
    event.xclient.data.l[2] = 0;

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &event);
    XSetInputFocus(display, win, RevertToPointerRoot, CurrentTime);
    XRaiseWindow(display, win);
    XFlush(display);
    XCloseDisplay(display);
}

static void setX11SkipTaskbar(unsigned long win) {
    if (win == 0) return;
    Display* display = XOpenDisplay(nullptr);
    if (!display) return;

    Atom net_state = XInternAtom(display, "_NET_WM_STATE", False);
    Atom skip_taskbar = XInternAtom(display, "_NET_WM_STATE_SKIP_TASKBAR", False);
    Atom skip_pager = XInternAtom(display, "_NET_WM_STATE_SKIP_PAGER", False);
    Atom stay_top = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);

    Atom atoms[3] = { skip_taskbar, skip_pager, stay_top };
    XChangeProperty(display, win, net_state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(atoms), 3);

    // Explicitly set window type to POPUP_MENU / UTILITY so Mutter and extensions never treat it as a dialog
    // and never play window closing / CRT collapse animations
    Atom net_type = XInternAtom(display, "_NET_WM_WINDOW_TYPE", False);
    Atom type_popup = XInternAtom(display, "_NET_WM_WINDOW_TYPE_POPUP_MENU", False);
    Atom type_utility = XInternAtom(display, "_NET_WM_WINDOW_TYPE_UTILITY", False);
    Atom types[2] = { type_popup, type_utility };
    XChangeProperty(display, win, net_type, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<unsigned char*>(types), 2);

    XFlush(display);
    XCloseDisplay(display);
}

// Clean up X11 macro pollution so Qt symbols are unaffected
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef FontChange
#undef None
#undef Bool
#undef Status
#undef CursorShape


FlyoutWindow::FlyoutWindow(std::shared_ptr<StorageManager> storage,
                           std::shared_ptr<PasteInjector> paste_injector,
                           std::shared_ptr<ClipboardDaemon> clip_daemon,
                           QWidget* parent)
    : QWidget(parent),
      storage_(storage),
      paste_injector_(paste_injector),
      clip_daemon_(clip_daemon),
      last_show_time_(std::chrono::steady_clock::now() - std::chrono::seconds(10)) {

    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFocusPolicy(Qt::StrongFocus);
    setFixedSize(360, 490);
    setStyleSheet(Style::WIN10_FLYOUT_STYLE);
    setWindowIcon(QIcon(":/icons/app_icon.png"));

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
    main_layout->setSpacing(8);

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

    // Search Bar
    search_bar_ = new QLineEdit(container_);
    search_bar_->setObjectName("SearchBar");
    search_bar_->setPlaceholderText("Search history...");
    search_bar_->setClearButtonEnabled(true);
    search_bar_->installEventFilter(this);
    connect(search_bar_, &QLineEdit::textChanged, this, &FlyoutWindow::onSearchTextChanged);
    main_layout->addWidget(search_bar_);

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

    empty_icon_ = new QLabel(empty_state_);
    QPixmap icon_pixmap(":/icons/app_icon.png");
    if (!icon_pixmap.isNull()) {
        empty_icon_->setPixmap(icon_pixmap.scaled(68, 68, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        empty_icon_->setText("📋");
        empty_icon_->setStyleSheet("font-size: 38px;");
    }
    empty_icon_->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_icon_);

    empty_title_ = new QLabel("Your clipboard is empty", empty_state_);
    empty_title_->setObjectName("EmptyTitle");
    empty_title_->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_title_);

    empty_subtitle_ = new QLabel("When you copy or cut text and images, they'll appear here.", empty_state_);
    empty_subtitle_->setObjectName("EmptySubtitle");
    empty_subtitle_->setWordWrap(true);
    empty_subtitle_->setAlignment(Qt::AlignCenter);
    empty_layout->addWidget(empty_subtitle_);

    main_layout->addWidget(empty_state_, 1);
    empty_state_->hide();

    outer_layout->addWidget(container_);

    if (qApp) {
        qApp->installEventFilter(this);
    }

    // Pre-calculate position and pre-load items in memory
    positionAtBottomRight();
    reloadHistory("");
    history_dirty_ = false;

    // Real-time clipboard update: pre-warm history in memory for 0ms open latency
    connect(clip_daemon_.get(), &ClipboardDaemon::historyUpdated, this, [this]() {
        if (isVisible()) {
            reloadHistory(search_bar_ ? search_bar_->text().trimmed() : "");
            history_dirty_ = false;
        } else {
            reloadHistory("");
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

    // Save previous active window for focus restoration on paste
    unsigned long active = getActiveX11Window();
    if (active != 0 && active != static_cast<unsigned long>(winId())) {
        target_window_ = active;
    }

    if (search_bar_) {
        search_bar_->blockSignals(true);
        search_bar_->clear();
        search_bar_->blockSignals(false);
    }
    selected_index_ = -1;

    if (history_dirty_) {
        reloadHistory("");
        history_dirty_ = false;
    }

    QPoint target_pos = calculateBottomRightPosition();
    QPoint start_pos = target_pos + QPoint(0, 10);

    if (anim_group_) {
        anim_group_->stop();
        delete anim_group_;
        anim_group_ = nullptr;
    }
    if (hide_anim_) {
        hide_anim_->stop();
        delete hide_anim_;
        hide_anim_ = nullptr;
    }

    qreal start_opacity = isVisible() ? windowOpacity() : 0.08;
    QPoint curr_pos = isVisible() ? pos() : start_pos;

    move(curr_pos);
    setWindowOpacity(start_opacity);
    show();
    raise();
    activateWindow();
    forceX11WindowActive(winId());
    setX11SkipTaskbar(winId());
    if (search_bar_) {
        search_bar_->setFocus(Qt::OtherFocusReason);
    }

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
        forceX11WindowActive(winId());
        if (search_bar_) {
            search_bar_->setFocus(Qt::OtherFocusReason);
        }
    });

    anim_group_->start(QAbstractAnimation::DeleteWhenStopped);

    std::cout << "[Flyout] Window smoothly opening at bottom-right corner.\n" << std::flush;
}

void FlyoutWindow::hideFlyout() {
    if (!isVisible()) return;

    if (anim_group_) {
        anim_group_->stop();
        delete anim_group_;
        anim_group_ = nullptr;
    }
    if (hide_anim_) {
        hide_anim_->stop();
        delete hide_anim_;
        hide_anim_ = nullptr;
    }

    hide();
    setWindowOpacity(1.0);
    std::cout << "[Flyout] Window dismissed.\n" << std::flush;
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
    if (elapsed < 100) {
        return;
    }

    QPoint mouse_pt = QCursor::pos();
    QRect win_bounds(mapToGlobal(QPoint(0, 0)), size());
    if (win_bounds.contains(mouse_pt)) {
        // Mouse click was inside the flyout window - do not dismiss!
        return;
    }

    std::cout << "[Flyout] Confirmed outside click at (" << mouse_pt.x() << ", " << mouse_pt.y() << "). Dismissing flyout.\n" << std::flush;
    hideFlyout();
}

void FlyoutWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::ActivationChange) {
        if (!isActiveWindow() && isVisible()) {
            auto now = std::chrono::steady_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_show_time_).count();
            if (elapsed > 200) {
                std::cout << "[Flyout] Window lost active focus to another window. Dismissing.\n" << std::flush;
                hideFlyout();
            }
        }
    }
    QWidget::changeEvent(event);
}

void FlyoutWindow::mousePressEvent(QMouseEvent* event) {
    if (container_ && !container_->geometry().contains(event->pos())) {
        hideFlyout();
        return;
    }
    QWidget::mousePressEvent(event);
}

void FlyoutWindow::reloadHistory(const QString& query) {
    for (auto* card : cards_) {
        list_layout_->removeWidget(card);
        card->deleteLater();
    }
    cards_.clear();
    selected_index_ = -1;

    // Load items matching search query up to Config limit
    auto items = storage_->getItems(-1, query.toStdString());
    if (items.empty()) {
        scroll_area_->hide();
        clear_all_btn_->setEnabled(false);
        if (query.isEmpty()) {
            if (empty_icon_) {
                QPixmap icon_pixmap(":/icons/app_icon.png");
                if (!icon_pixmap.isNull()) {
                    empty_icon_->setStyleSheet("");
                    empty_icon_->setPixmap(icon_pixmap.scaled(68, 68, Qt::KeepAspectRatio, Qt::SmoothTransformation));
                } else {
                    empty_icon_->setText("📋");
                    empty_icon_->setStyleSheet("font-size: 38px;");
                }
            }
            if (empty_title_) empty_title_->setText("Your clipboard is empty");
            if (empty_subtitle_) empty_subtitle_->setText("When you copy or cut text and images, they'll appear here.");
        } else {
            if (empty_icon_) {
                empty_icon_->setText("🔍");
                empty_icon_->setStyleSheet("font-size: 36px;");
            }
            if (empty_title_) empty_title_->setText("No results found");
            if (empty_subtitle_) empty_subtitle_->setText(QString("No clipboard items match \"%1\"").arg(query));
        }
        empty_state_->show();
        return;
    }

    empty_state_->hide();
    scroll_area_->show();
    clear_all_btn_->setEnabled(true);

    int idx = 0;
    for (const auto& item : items) {
        auto* card = new ItemCard(item, list_container_);
        card->installEventFilter(this);
        if (idx < 9) {
            card->setToolTip(QString("Click or press Enter to paste (Alt+%1)").arg(idx + 1));
        }
        connect(card, &ItemCard::clicked, this, &FlyoutWindow::onCardClicked);
        connect(card, &ItemCard::pinToggled, this, &FlyoutWindow::onPinToggled);
        connect(card, &ItemCard::deleteRequested, this, &FlyoutWindow::onDeleteRequested);

        list_layout_->addWidget(card);
        cards_.push_back(card);
        idx++;
    }

    if (!query.isEmpty() && !cards_.empty()) {
        updateSelection(0);
    }
}

void FlyoutWindow::onSearchTextChanged(const QString& text) {
    reloadHistory(text.trimmed());
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

void FlyoutWindow::pasteCardAt(int index) {
    if (index < 0 || index >= static_cast<int>(cards_.size())) return;
    onCardClicked(cards_[index]->recordId());
}

void FlyoutWindow::onCardClicked(int64_t id) {
    std::cout << "[Flyout] onCardClicked triggered for ID=" << id << "\n" << std::flush;
    hideFlyout();

    if (target_window_ != 0) {
        restoreActiveX11Window(target_window_);
        target_window_ = 0;
    }

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
        injector->paste(45);
    }).detach();
}

void FlyoutWindow::onPinToggled(int64_t id) {
    storage_->togglePin(id);
}

void FlyoutWindow::onDeleteRequested(int64_t id) {
    storage_->deleteItem(id);
    reloadHistory(search_bar_ ? search_bar_->text().trimmed() : "");
}

void FlyoutWindow::onClearAllClicked() {
    storage_->clearUnpinned();
    reloadHistory(search_bar_ ? search_bar_->text().trimmed() : "");
}

void FlyoutWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        if (search_bar_ && !search_bar_->text().isEmpty()) {
            search_bar_->clear();
        } else {
            hideFlyout();
        }
        event->accept();
        return;
    }

    if (cards_.empty()) {
        QWidget::keyPressEvent(event);
        return;
    }

    if (event->key() == Qt::Key_Down) {
        int next_idx = (selected_index_ < 0) ? 0 : (selected_index_ + 1) % static_cast<int>(cards_.size());
        updateSelection(next_idx);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Up) {
        int prev_idx = (selected_index_ <= 0) ? static_cast<int>(cards_.size()) - 1 : selected_index_ - 1;
        updateSelection(prev_idx);
        event->accept();
        return;
    }

    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        int idx = (selected_index_ >= 0 && selected_index_ < static_cast<int>(cards_.size())) ? selected_index_ : 0;
        pasteCardAt(idx);
        event->accept();
        return;
    }

    // Direct number keys 1..9 if focus is outside the search bar
    if (event->key() >= Qt::Key_1 && event->key() <= Qt::Key_9) {
        int idx = event->key() - Qt::Key_1;
        if (idx < static_cast<int>(cards_.size())) {
            pasteCardAt(idx);
            event->accept();
            return;
        }
    }

    QWidget::keyPressEvent(event);
}

bool FlyoutWindow::eventFilter(QObject* watched, QEvent* event) {
    if (watched == search_bar_ && event->type() == QEvent::KeyPress) {
        auto* ke = static_cast<QKeyEvent*>(event);
        if (ke->key() == Qt::Key_Down) {
            if (!cards_.empty()) {
                int next_idx = (selected_index_ < 0) ? 0 : (selected_index_ + 1) % static_cast<int>(cards_.size());
                updateSelection(next_idx);
            }
            return true;
        }
        if (ke->key() == Qt::Key_Up) {
            if (!cards_.empty()) {
                int prev_idx = (selected_index_ <= 0) ? static_cast<int>(cards_.size()) - 1 : selected_index_ - 1;
                updateSelection(prev_idx);
            }
            return true;
        }
        if (ke->key() == Qt::Key_Return || ke->key() == Qt::Key_Enter) {
            if (!cards_.empty()) {
                int idx = (selected_index_ >= 0 && selected_index_ < static_cast<int>(cards_.size())) ? selected_index_ : 0;
                pasteCardAt(idx);
            }
            return true;
        }
        if (ke->key() == Qt::Key_Escape) {
            if (!search_bar_->text().isEmpty()) {
                search_bar_->clear();
            } else {
                hideFlyout();
            }
            return true;
        }
        if ((ke->modifiers() & (Qt::AltModifier | Qt::ControlModifier)) &&
            ke->key() >= Qt::Key_1 && ke->key() <= Qt::Key_9) {
            int idx = ke->key() - Qt::Key_1;
            if (idx < static_cast<int>(cards_.size())) {
                pasteCardAt(idx);
            }
            return true;
        }
    }

    return QWidget::eventFilter(watched, event);
}

