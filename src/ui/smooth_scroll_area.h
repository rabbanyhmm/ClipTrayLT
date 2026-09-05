#pragma once

#include <QScrollArea>
#include <QPropertyAnimation>
#include <QWheelEvent>

class SmoothScrollArea : public QScrollArea {
    Q_OBJECT
public:
    explicit SmoothScrollArea(QWidget* parent = nullptr);

    void smoothScrollTo(int target_value, int duration_ms = 180);

protected:
    void wheelEvent(QWheelEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void handleWheel(QWheelEvent* event);

    QPropertyAnimation* scroll_anim_ = nullptr;
    int target_value_ = 0;
};
