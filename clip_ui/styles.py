WIN11_DARK_STYLESHEET = """
/* Windows 11 Fluent Dark Acrylic Theme - Matched to Win+V Flyout Spec */

QWidget#FlyoutWindow {
    background-color: #191919;
    border: 1px solid #333333;
    border-radius: 12px;
}

QWidget#CentralContainer {
    background-color: transparent;
    border-radius: 12px;
}

/* Top Window Title Bar */
QLabel#TopWindowTitle {
    color: #e3e3e3;
    font-size: 13px;
    font-weight: 500;
    font-family: 'Segoe UI', 'Inter', sans-serif;
}

QPushButton#CloseBtn {
    background: transparent;
    color: #a0a0a0;
    border: none;
    font-size: 15px;
    font-weight: bold;
    border-radius: 4px;
    padding: 2px 6px;
}

QPushButton#CloseBtn:hover {
    background-color: #c42b1c;
    color: #ffffff;
}

/* Section Header (Clipboard & Clear All) */
QLabel#SectionTitle {
    color: #ffffff;
    font-size: 16px;
    font-weight: 600;
    font-family: 'Segoe UI', 'Inter', sans-serif;
}

QPushButton#ClearAllPillBtn {
    background-color: rgba(255, 255, 255, 0.08);
    color: #e0e0e0;
    border: 1px solid rgba(255, 255, 255, 0.12);
    border-radius: 12px;
    padding: 5px 14px;
    font-size: 12px;
    font-family: 'Segoe UI', 'Inter', sans-serif;
}

QPushButton#ClearAllPillBtn:hover {
    background-color: rgba(255, 255, 255, 0.16);
    color: #ffffff;
    border-color: rgba(255, 255, 255, 0.25);
}

QPushButton#ClearAllPillBtn:pressed {
    background-color: rgba(255, 255, 255, 0.05);
}

/* Search Input */
QLineEdit#SearchInput {
    background-color: #242424;
    color: #ffffff;
    border: 1px solid #383838;
    border-bottom: 2px solid #0078d4;
    border-radius: 6px;
    padding: 6px 12px;
    font-size: 13px;
    font-family: 'Segoe UI', 'Inter', sans-serif;
}

QLineEdit#SearchInput:focus {
    background-color: #2b2b2b;
    border-bottom: 2px solid #60cdff;
}

QLineEdit#SearchInput::placeholder {
    color: #888888;
}

/* Navigation Icon Tabs Bar */
QTabBar::tab {
    background: transparent;
    color: #a0a0a0;
    font-size: 16px;
    padding: 8px 14px;
    border: none;
    border-bottom: 3px solid transparent;
    margin-right: 2px;
}

QTabBar::tab:hover {
    color: #ffffff;
    background: rgba(255, 255, 255, 0.06);
    border-radius: 4px;
}

QTabBar::tab:selected {
    color: #ffffff;
    font-weight: bold;
    border-bottom: 3px solid #60cdff;
}

/* Scroll Area & Custom Scrollbars */
QScrollArea {
    border: none;
    background: transparent;
}

QScrollBar:vertical {
    border: none;
    background: transparent;
    width: 6px;
    margin: 0px;
    border-radius: 3px;
}

QScrollBar::handle:vertical {
    background: rgba(255, 255, 255, 0.18);
    min-height: 25px;
    border-radius: 3px;
}

QScrollBar::handle:vertical:hover {
    background: rgba(255, 255, 255, 0.35);
}

QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {
    height: 0px;
}

/* Clipboard Item Card - Matched to Screenshot Spec */
QFrame#ItemCard {
    background-color: #242424;
    border: 1px solid #353535;
    border-radius: 8px;
    margin: 4px 0px;
}

QFrame#ItemCard:hover {
    background-color: #2c2c2c;
    border: 1px solid #484848;
}

QFrame#ItemCard[pinned="true"] {
    background-color: #262c35;
    border: 1px solid #0078d4;
}

QFrame#ItemCard[keyboard_selected="true"] {
    background-color: #2c2c2c;
    border: 1.5px solid #60cdff;
}

QLabel#ItemText {
    color: #e3e3e3;
    font-size: 13px;
    font-family: 'Segoe UI', 'Consolas', 'Inter', sans-serif;
    line-height: 1.4;
}

QLabel#ItemMeta {
    color: #888888;
    font-size: 11px;
    font-family: 'Segoe UI', sans-serif;
}

/* Card Action Buttons (Options menu & Pin icon) */
QPushButton#CardActionButton {
    background: transparent;
    border: none;
    border-radius: 4px;
    padding: 3px 6px;
    color: #a0a0a0;
    font-size: 14px;
}

QPushButton#CardActionButton:hover {
    background-color: rgba(255, 255, 255, 0.12);
    color: #ffffff;
}

QPushButton#PinBtn[pinned="true"] {
    color: #60cdff;
}

/* Emoji / Symbol Grid Item */
QPushButton#EmojiItem {
    background-color: #242424;
    border: 1px solid transparent;
    border-radius: 6px;
    font-size: 22px;
    padding: 6px;
}

QPushButton#EmojiItem:hover {
    background-color: #333333;
    border: 1px solid #444444;
}
"""
