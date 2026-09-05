#include "smooth_scroll_area.h"
#include <QScrollBar>
#include <algorithm>

SmoothScrollArea::SmoothScrollArea(QWidget* parent) : QScrollArea(parent) {
    scroll_anim_ = new QPropertyAnimation(verticalScrollBar(), "value", this);
    scroll_anim_->setEasingCurve(QEasingCurve::OutCubic);
    target_value_ = verticalScrollBar()->value();

    if (viewport()) {
        viewport()->installEventFilter(this);
    }
}

void SmoothScrollArea::smoothScrollTo(int target_value, int duration_ms) {
    QScrollBar* bar = verticalScrollBar();
    target_value_ = std::clamp(target_value, bar->minimum(), bar->maximum());

    scroll_anim_->stop();
    scroll_anim_->setDuration(duration_ms);
    scroll_anim_->setStartValue(bar->value());
    scroll_anim_->setEndValue(target_value_);
    scroll_anim_->start();
}

void SmoothScrollArea::wheelEvent(QWheelEvent* event) {
    handleWheel(event);
}

bool SmoothScrollArea::eventFilter(QObject* watched, QEvent* event) {
    if (watched == viewport() && event->type() == QEvent::Wheel) {
        handleWheel(static_cast<QWheelEvent*>(event));
        return true;
    }
    return QScrollArea::eventFilter(watched, event);
}

void SmoothScrollArea::handleWheel(QWheelEvent* event) {
    int degrees = event->angleDelta().y() / 8;
    int steps = degrees / 15;
    if (steps == 0) {
        steps = (event->angleDelta().y() > 0) ? 1 : -1;
    }

    // 84 pixels per notch - smooth kinetic response
    int step_pixels = 84;
    int delta = steps * step_pixels;

    QScrollBar* bar = verticalScrollBar();
    if (scroll_anim_->state() == QAbstractAnimation::Running) {
        target_value_ -= delta;
    } else {
        target_value_ = bar->value() - delta;
    }

    target_value_ = std::clamp(target_value_, bar->minimum(), bar->maximum());

    scroll_anim_->stop();
    // Fast, ultra-smooth 180ms with OutCubic deceleration
    scroll_anim_->setDuration(180);
    scroll_anim_->setStartValue(bar->value());
    scroll_anim_->setEndValue(target_value_);
    scroll_anim_->start();

    event->accept();
}
