#pragma once

#include <QWidget>
#include <QLabel>
#include <QVBoxLayout>
#include <QPoint>

class SimpleTestWindow : public QWidget {
    Q_OBJECT
public:
    explicit SimpleTestWindow(QWidget* parent = nullptr);

    void toggleFlyout();
    void showFlyout();
    void hideFlyout();
    void handleGlobalClick(const QPoint& pt);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void changeEvent(QEvent* event) override;

private:
    QWidget* container_ = nullptr;
    QLabel* status_label_ = nullptr;
    QLabel* location_label_ = nullptr;
};
