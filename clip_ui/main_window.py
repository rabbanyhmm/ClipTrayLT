import sys
from PySide6.QtWidgets import (QMainWindow, QWidget, QVBoxLayout, QScrollArea, 
                             QStackedWidget, QLabel, QGraphicsDropShadowEffect)
from PySide6.QtCore import Qt, Signal, QPoint, QEvent, QTimer
from PySide6.QtGui import QColor, QCursor, QGuiApplication, QKeyEvent

from clip_ui.styles import WIN11_DARK_STYLESHEET
from clip_ui.components.header import TopTitleWidget, ClipboardSubHeaderWidget
from clip_ui.components.tabs import NavigationTabs
from clip_ui.components.item_card import ItemCard
from clip_ui.components.emoji_panel import EmojiPanel

class ClipboardFlyoutWindow(QMainWindow):
    item_selected = Signal(object)
    symbol_selected = Signal(object)

    def __init__(self, storage_manager, parent=None):
        super().__init__(parent)
        self.storage = storage_manager
        self.cards_list = []
        self._is_system_dragging = False
        self._selected_card_index = -1
        
        # Window Flags for Windows 11 Flyout behavior (Frameless, Stay on Top)
        self.setWindowFlags(
            Qt.FramelessWindowHint | 
            Qt.WindowStaysOnTopHint
        )
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self.resize(390, 550)
        self.setStyleSheet(WIN11_DARK_STYLESHEET)
        
        self._init_ui()

    def _init_ui(self):
        self.central_widget = QWidget()
        self.central_widget.setObjectName("FlyoutWindow")
        self.setCentralWidget(self.central_widget)

        shadow = QGraphicsDropShadowEffect(self)
        shadow.setBlurRadius(28)
        shadow.setColor(QColor(0, 0, 0, 180))
        shadow.setOffset(0, 8)
        self.central_widget.setGraphicsEffect(shadow)

        main_layout = QVBoxLayout(self.central_widget)
        main_layout.setContentsMargins(0, 0, 0, 0)
        main_layout.setSpacing(0)

        # 1. Top Window Title Bar ("Emoji and more" + Close button)
        self.top_title_bar = TopTitleWidget()
        self.top_title_bar.close_requested.connect(self.hide)
        main_layout.addWidget(self.top_title_bar)

        # 2. Navigation Icon Tabs Bar (😊, GIF, ;-), 🔣, 📋)
        self.tabs = NavigationTabs()
        self.tabs.tab_changed.connect(self.on_tab_changed)
        main_layout.addWidget(self.tabs)

        # 3. Section Sub-Header ("Clipboard" + "Clear all" Pill + Search input)
        self.sub_header = ClipboardSubHeaderWidget()
        self.sub_header.search_changed.connect(self.on_search_changed)
        self.sub_header.clear_unpinned_requested.connect(self.on_clear_unpinned)
        self.sub_header.search_input.returnPressed.connect(self._on_search_return_pressed)
        main_layout.addWidget(self.sub_header)

        # 4. Stacked Pages for Panels
        self.stack = QStackedWidget()

        # Page 0: Clipboard History Scroll Area
        self.history_scroll = QScrollArea()
        self.history_scroll.setWidgetResizable(True)
        self.history_container = QWidget()
        self.history_layout = QVBoxLayout(self.history_container)
        self.history_layout.setContentsMargins(14, 4, 14, 12)
        self.history_layout.setSpacing(8)
        self.history_scroll.setWidget(self.history_container)

        self.stack.addWidget(self.history_scroll) # Index 0: History

        # Page 1: Emoji Panel
        self.emoji_panel = EmojiPanel(mode="emoji")
        self.emoji_panel.symbol_selected.connect(self._on_symbol_clicked)
        self.stack.addWidget(self.emoji_panel) # Index 1

        # Page 2: Kaomoji Panel
        self.kaomoji_panel = EmojiPanel(mode="kaomoji")
        self.kaomoji_panel.symbol_selected.connect(self._on_symbol_clicked)
        self.stack.addWidget(self.kaomoji_panel) # Index 2

        # Page 3: Symbols Panel
        self.symbols_panel = EmojiPanel(mode="symbols")
        self.symbols_panel.symbol_selected.connect(self._on_symbol_clicked)
        self.stack.addWidget(self.symbols_panel) # Index 3

        main_layout.addWidget(self.stack)

        # Empty State Label
        self.empty_label = QLabel("Your clipboard history is empty.")
        self.empty_label.setAlignment(Qt.AlignCenter)
        self.empty_label.setStyleSheet("color: #888888; font-size: 13px; font-family: 'Segoe UI'; margin: 40px;")

        # Set default tab to Clipboard History on far right (Index 3 in tabs)
        self.tabs.tab_bar.setCurrentIndex(3)
        self.stack.setCurrentIndex(0)

    def reload_history(self, search_query: str = None, filter_pinned: bool = False):
        """Clears and re-renders history list cleanly."""
        for card in self.cards_list:
            card.deleteLater()
        self.cards_list.clear()

        # Clear items in history_layout
        while self.history_layout.count() > 0:
            item = self.history_layout.takeAt(0)
            if item.widget() and item.widget() != self.empty_label:
                item.widget().deleteLater()

        filter_type = 'pinned' if filter_pinned else None
        items = self.storage.get_items(search_query=search_query, filter_type=filter_type)
        
        if not items:
            self.history_layout.addWidget(self.empty_label)
            self.empty_label.show()
            self._selected_card_index = -1
        else:
            self.empty_label.hide()
            for item in items:
                card = ItemCard(item_data=item)
                card.item_clicked.connect(self._on_item_clicked)
                card.pin_toggled.connect(self._on_pin_toggled)
                card.delete_requested.connect(self._on_delete_requested)
                self.history_layout.addWidget(card)
                self.cards_list.append(card)

            self.history_layout.addStretch()
            self._selected_card_index = 0
            self.update_card_highlights()

    def update_card_highlights(self):
        """Visually updates keyboard selection borders and scroll visibility."""
        for idx, card in enumerate(self.cards_list):
            is_selected = (idx == self._selected_card_index)
            card.setProperty("keyboard_selected", "true" if is_selected else "false")
            card.style().polish(card)
            if is_selected:
                self.history_scroll.ensureWidgetVisible(card)

    def _on_search_return_pressed(self):
        """Pastes the currently selected card if Enter is pressed in search bar."""
        if self.cards_list and 0 <= self._selected_card_index < len(self.cards_list):
            selected_card = self.cards_list[self._selected_card_index]
            self._on_item_clicked(selected_card.item_data)

    def show_near_cursor(self):
        """
        Windows 11 Dual-Placement Algorithm:
        - If input is focused: Show popup next to caret / input box.
        - If no input is focused: Show at bottom-right of monitor work area.
        """
        # Save the active window focus before displaying flyout
        try:
            from Xlib import display as xdisplay
            d = xdisplay.Display()
            focus_obj = d.get_input_focus()
            if focus_obj and focus_obj.focus:
                self._last_active_window = focus_obj.focus
        except Exception:
            self._last_active_window = None

        from clip_engine.caret_detector import CaretDetector

        caret_coords = CaretDetector.get_focused_text_input_position()
        screen = QGuiApplication.screenAt(QCursor.pos()) or QGuiApplication.primaryScreen()
        work_area = screen.availableGeometry()

        if caret_coords:
            x, y = caret_coords
            if x + self.width() > work_area.right():
                x = work_area.right() - self.width() - 10
            if y + self.height() > work_area.bottom():
                y = y - self.height() - 10
            else:
                y = y + 16

            x = max(work_area.left() + 10, x)
            y = max(work_area.top() + 10, y)
        else:
            x = work_area.right() - self.width() - 16
            y = work_area.bottom() - self.height() - 16

        self.reload_history()
        self._recently_shown = True
        QTimer.singleShot(400, lambda: setattr(self, '_recently_shown', False))

        self.show()
        self.move(QPoint(x, y))
        self.raise_()
        self.activateWindow()
        self.sub_header.focus_search()

    def hideEvent(self, event):
        # Restore focus back to target window
        if getattr(self, '_last_active_window', None):
            try:
                from Xlib import display as xdisplay
                import Xlib
                d = xdisplay.Display()
                d.set_input_focus(self._last_active_window, Xlib.X.RevertToParent, Xlib.X.CurrentTime)
                d.sync()
            except Exception:
                pass
            self._last_active_window = None
        super().hideEvent(event)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self._is_system_dragging = True
            if self.windowHandle():
                self.windowHandle().startSystemMove()
                event.accept()
                return
        super().mousePressEvent(event)

    def mouseReleaseEvent(self, event):
        super().mouseReleaseEvent(event)

    def mouseMoveEvent(self, event):
        super().mouseMoveEvent(event)

    def toggle_window(self):
        if self.isVisible():
            self.hide()
        else:
            self.show_near_cursor()

    def on_search_changed(self, query: str):
        self.reload_history(search_query=query if query.strip() else None)

    def on_clear_unpinned(self):
        self.storage.clear_unpinned()
        self.reload_history()

    def on_tab_changed(self, tab_index: int):
        # 0: Emoji (😊), 1: Kaomoji (;-)), 2: Symbols (🔣), 3: Clipboard History (📋 - Far Right)
        if tab_index == 3:  # Clipboard History
            self.sub_header.show()
            self.stack.setCurrentIndex(0)
            self.reload_history()
        else:
            self.sub_header.hide()
            self.stack.setCurrentIndex(tab_index + 1)

    def _on_item_clicked(self, item_data: dict):
        was_active = self.isActiveWindow()
        self.hide()
        self.item_selected.emit((item_data, was_active))

    def _on_symbol_clicked(self, symbol: str):
        was_active = self.isActiveWindow()
        self.hide()
        self.symbol_selected.emit((symbol, was_active))

    def _on_pin_toggled(self, item_id: int):
        self.storage.toggle_pin(item_id)
        self.reload_history(self.sub_header.search_input.text())

    def _on_delete_requested(self, item_id: int):
        self.storage.delete_item(item_id)
        self.reload_history(self.sub_header.search_input.text())

    def keyPressEvent(self, event: QKeyEvent):
        if event.key() == Qt.Key_Escape:
            self.hide()
            event.accept()
        elif event.key() == Qt.Key_Down:
            if self.cards_list and self.stack.currentIndex() == 0:
                self._selected_card_index = (self._selected_card_index + 1) % len(self.cards_list)
                self.update_card_highlights()
                event.accept()
        elif event.key() == Qt.Key_Up:
            if self.cards_list and self.stack.currentIndex() == 0:
                self._selected_card_index = (self._selected_card_index - 1 + len(self.cards_list)) % len(self.cards_list)
                self.update_card_highlights()
                event.accept()
        elif event.key() in (Qt.Key_Return, Qt.Key_Enter):
            if self.cards_list and self.stack.currentIndex() == 0:
                if 0 <= self._selected_card_index < len(self.cards_list):
                    selected_card = self.cards_list[self._selected_card_index]
                    self._on_item_clicked(selected_card.item_data)
                    event.accept()
        else:
            super().keyPressEvent(event)

    def changeEvent(self, event):
        if event.type() == QEvent.ActivationChange:
            if self.isActiveWindow():
                # Focus regained: Drag operation has finished
                self._is_system_dragging = False
            else:
                # Focus lost: Hide only if not system dragging
                if not getattr(self, '_recently_shown', False) and not self._is_system_dragging:
                    self.hide()
        super().changeEvent(event)
