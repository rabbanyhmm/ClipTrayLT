#pragma once

#include <QWidget>
#include <QLabel>
#include <QPushButton>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include "storage.h"

class ItemCard : public QWidget {
    Q_OBJECT
public:
    explicit ItemCard(const ClipboardRecord& record, QWidget* parent = nullptr);

    int64_t recordId() const { return record_.id; }
    void setSelected(bool selected);
    bool isSelected() const { return is_selected_; }

signals:
    void clicked(int64_t id);
    void cardInteracted();
    void pinToggled(int64_t id);
    void deleteRequested(int64_t id);

protected:
    void mousePressEvent(QMouseEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    ClipboardRecord record_;
    bool is_selected_ = false;
    QLabel* content_label_ = nullptr;
    QPushButton* pin_btn_ = nullptr;
    QPushButton* delete_btn_ = nullptr;

    void updatePinUi();
};
