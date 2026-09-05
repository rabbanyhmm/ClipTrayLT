#pragma once

#include <QString>

namespace Style {

const QString WIN10_FLYOUT_STYLE = R"(
    QWidget#FlyoutWindow {
        background-color: #1f1f1f;
        border: 1px solid #3c3c3c;
        border-radius: 6px;
    }

    QLabel#HeaderTitle {
        color: #f1f1f1;
        font-size: 15px;
        font-weight: 600;
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
    }

    QPushButton#ClearAllBtn {
        background-color: transparent;
        color: #d0d0d0;
        border: 1px solid #444444;
        border-radius: 4px;
        padding: 4px 12px;
        font-size: 12px;
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
    }
    QPushButton#ClearAllBtn:hover {
        background-color: #333333;
        color: #ffffff;
        border-color: #666666;
    }
    QPushButton#ClearAllBtn:pressed {
        background-color: #404040;
    }

    /* Search Bar */
    QLineEdit#SearchBar {
        background-color: #2b2b2b;
        color: #f1f1f1;
        border: 1px solid #3c3c3c;
        border-radius: 4px;
        padding: 6px 10px;
        font-size: 13px;
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
        selection-background-color: #0078d4;
        selection-color: #ffffff;
    }
    QLineEdit#SearchBar:focus {
        border: 1px solid #0078d4;
        background-color: #323232;
    }
    QLineEdit#SearchBar::placeholder {
        color: #888888;
    }

    QScrollArea#HistoryScroll {
        background: transparent;
        border: none;
    }

    QScrollBar:vertical {
        background: transparent;
        width: 6px;
        margin: 0px;
    }
    QScrollBar::handle:vertical {
        background: rgba(255, 255, 255, 0.22);
        min-height: 28px;
        border-radius: 3px;
    }
    QScrollBar::handle:vertical:hover {
        background: rgba(255, 255, 255, 0.45);
    }
    QScrollBar::handle:vertical:pressed {
        background: rgba(255, 255, 255, 0.65);
    }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
        height: 0px;
        background: transparent;
    }
    QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
        background: transparent;
    }

    /* Card Styling */
    QWidget#ItemCard {
        background-color: #2b2b2b;
        border: 1px solid #363636;
        border-radius: 5px;
    }
    QWidget#ItemCard[selected="true"] {
        background-color: #383838;
        border: 1px solid #0078d4;
    }
    QWidget#ItemCard[selected="true"]:hover {
        background-color: #3d3d3d;
        border: 1px solid #1685e0;
    }
    QWidget#ItemCard:hover {
        background-color: #353535;
        border: 1px solid #4d4d4d;
    }

    QLabel#ItemText {
        color: #f0f0f0;
        font-size: 13px;
        line-height: 1.4;
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
    }

    QPushButton#CardActionBtn {
        background: transparent;
        border: none;
        border-radius: 3px;
        color: #999999;
        font-size: 13px;
        padding: 2px;
    }
    QPushButton#CardActionBtn:hover {
        background-color: #444444;
        color: #ffffff;
    }

    QPushButton#CardActionBtn[pinned="true"] {
        color: #0078d4;
    }

    /* Empty state */
    QLabel#EmptyTitle {
        color: #ffffff;
        font-size: 14px;
        font-weight: 600;
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
    }
    QLabel#EmptySubtitle {
        color: #8c8c8c;
        font-size: 12px;
        font-family: 'Segoe UI', 'Ubuntu', 'Cantarell', sans-serif;
    }
)";

} // namespace Style
