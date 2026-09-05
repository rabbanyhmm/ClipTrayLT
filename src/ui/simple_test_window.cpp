#include "simple_test_window.h"
#include <QScreen>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QGraphicsDropShadowEffect>
#include <iostream>

SimpleTestWindow::SimpleTestWindow(QWidget* parent) : QWidget(parent) {
    setWindowFlags(Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint | Qt::Tool);
    setAttribute(Qt::WA_TranslucentBackground, true);
    setFixedSize(340, 190);

    auto* outer_layout = new QVBoxLayout(this);
    outer_layout->setContentsMargins(10, 10, 10, 10);

    container_ = new QWidget(this);
    container_->setStyleSheet(R"(
        QWidget {
            background-color: #1f1f1f;
            border: 2px solid #0078d4;
            border-radius: 8px;
        }
        QLabel {
            border: none;
            color: #f0f0f0;
            font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
        }
    )");

    auto* shadow = new QGraphicsDropShadowEffect(this);
    shadow->setBlurRadius(24);
    shadow->setColor(QColor(0, 0, 0, 180));
    shadow->setOffset(0, 6);
    container_->setGraphicsEffect(shadow);

    auto* layout = new QVBoxLayout(container_);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(8);

    auto* title = new QLabel("📋 ClipTray LT (Corner Anchor)", container_);
    title->setStyleSheet("font-size: 14px; font-weight: bold; color: #ffffff;");
    layout->addWidget(title);

    status_label_ = new QLabel("Status: Active (Single Window Guarantee)", container_);
    status_label_->setStyleSheet("color: #4cc2ff; font-size: 12px;");
    layout->addWidget(status_label_);

    location_label_ = new QLabel("Position: Fixed Bottom-Right Work Area\nSensitive Outside-Click Dismissal: ON", container_);
    location_label_->setStyleSheet("color: #cccccc; font-size: 12px;");
    location_label_->setWordWrap(true);
    layout->addWidget(location_label_);

    auto* hint = new QLabel("Press SUPER + V or Click Outside to close", container_);
    hint->setStyleSheet("color: #888888; font-size: 11px; margin-top: 4px;");
    layout->addWidget(hint);

    outer_layout->addWidget(container_);
}

void SimpleTestWindow::toggleFlyout() {
    if (isVisible()) {
        hideFlyout();
    } else {
        showFlyout();
    }
}

void SimpleTestWindow::showFlyout() {
    QScreen* screen = QGuiApplication::primaryScreen();
    QRect work = screen->availableGeometry();

    // Constant bottom-right corner anchor above taskbar
    int x = work.right() - width() - 16;
    int y = work.bottom() - height() - 16;

    move(x, y);
    show();
    raise();
    activateWindow();

    std::cout << "[Flyout] Displayed single instance at bottom-right corner: (" << x << ", " << y << ")\n" << std::flush;
}

void SimpleTestWindow::hideFlyout() {
    if (!isVisible()) return;
    std::cout << "[Flyout] Window hidden.\n" << std::flush;
    hide();
}

void SimpleTestWindow::handleGlobalClick(const QPoint& pt) {
    if (!isVisible()) return;

    // Check if the mouse click happened outside our window boundaries
    if (!geometry().contains(pt)) {
        std::cout << "[Flyout] Global click outside window at (" << pt.x() << ", " << pt.y() << "). Hiding window.\n" << std::flush;
        hideFlyout();
    }
}

void SimpleTestWindow::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape) {
        hideFlyout();
        event->accept();
        return;
    }
    QWidget::keyPressEvent(event);
}

void SimpleTestWindow::changeEvent(QEvent* event) {
    if (event->type() == QEvent::ActivationChange) {
        if (!isActiveWindow() && isVisible()) {
            hideFlyout();
        }
    }
    QWidget::changeEvent(event);
}
