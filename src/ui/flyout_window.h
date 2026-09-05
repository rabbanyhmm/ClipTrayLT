#pragma once

#include <QWidget>
#include <QVBoxLayout>
#include <QScrollArea>
#include <QLabel>
#include <QPushButton>
#include <QLineEdit>
#include <QPoint>
#include <memory>
#include <vector>
#include <atomic>
#include <chrono>
#include <QPointer>
#include <QParallelAnimationGroup>
#include <QPropertyAnimation>
#include "storage.h"
#include "paste_injector.h"
#include "clipboard_daemon.h"
#include "item_card.h"
#include "smooth_scroll_area.h"

class FlyoutWindow : public QWidget {
    Q_OBJECT
public:
    FlyoutWindow(std::shared_ptr<StorageManager> storage,
                 std::shared_ptr<PasteInjector> paste_injector,
                 std::shared_ptr<ClipboardDaemon> clip_daemon,
                 QWidget* parent = nullptr);

    void toggleFlyout();
    void showFlyout();
    void hideFlyout();
    void reloadHistory(const QString& query = "");
    void handleGlobalClick();

protected:
    void keyPressEvent(QKeyEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

private slots:
    void onCardClicked(int64_t id);
    void onPinToggled(int64_t id);
    void onDeleteRequested(int64_t id);
    void onClearAllClicked();
    void onSearchTextChanged(const QString& text);

private:
    std::shared_ptr<StorageManager> storage_;
    std::shared_ptr<PasteInjector> paste_injector_;
    std::shared_ptr<ClipboardDaemon> clip_daemon_;

    QWidget* container_ = nullptr;
    QLineEdit* search_bar_ = nullptr;
    SmoothScrollArea* scroll_area_ = nullptr;
    QWidget* list_container_ = nullptr;
    QVBoxLayout* list_layout_ = nullptr;
    QWidget* empty_state_ = nullptr;
    QLabel* empty_icon_ = nullptr;
    QLabel* empty_title_ = nullptr;
    QLabel* empty_subtitle_ = nullptr;
    QPushButton* clear_all_btn_ = nullptr;

    std::vector<ItemCard*> cards_;
    int selected_index_ = -1;
    std::atomic<bool> history_dirty_{true};
    std::chrono::steady_clock::time_point last_show_time_;
    QPointer<QParallelAnimationGroup> anim_group_ = nullptr;
    unsigned long target_window_ = 0;

    void updateSelection(int new_index);
    void pasteCardAt(int index);
    void positionAtBottomRight();
    QPoint calculateBottomRightPosition();
};
