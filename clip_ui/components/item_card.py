import os
import datetime
from PySide6.QtWidgets import (QFrame, QWidget, QHBoxLayout, QVBoxLayout, QLabel, 
                             QPushButton, QMenu, QGraphicsDropShadowEffect)
from PySide6.QtGui import QPixmap, QImage, QIcon, QAction, QColor
from PySide6.QtCore import Signal, Qt

class ItemCard(QFrame):
    item_clicked = Signal(dict)
    pin_toggled = Signal(int)
    delete_requested = Signal(int)

    def __init__(self, item_data: dict, parent=None):
        super().__init__(parent)
        self.item_data = item_data
        self.setObjectName("ItemCard")
        self.setProperty("pinned", "true" if item_data.get('is_pinned') else "false")
        self.setCursor(Qt.PointingHandCursor)
        self._init_ui()

    def _init_ui(self):
        # Card Layout: Main Horizontal layout (Left content + Right actions column)
        main_layout = QHBoxLayout(self)
        main_layout.setContentsMargins(10, 10, 10, 10)
        main_layout.setSpacing(12)

        content_type = self.item_data.get('content_type', 'text')

        # 1. Left Content Area (Image thumbnail OR Text snippet preview)
        left_container = QWidget()
        left_layout = QVBoxLayout(left_container)
        left_layout.setContentsMargins(0, 0, 0, 0)
        left_layout.setSpacing(4)

        if content_type == 'image':
            img_path = self.item_data.get('image_path')
            if img_path and os.path.exists(img_path):
                img_label = QLabel()
                pixmap = QPixmap(img_path)
                if not pixmap.isNull():
                    # Thumbnail scaled nicely on left side (max 180px width, max 110px height)
                    scaled_pixmap = pixmap.scaled(180, 110, Qt.KeepAspectRatio, Qt.SmoothTransformation)
                    img_label.setPixmap(scaled_pixmap)
                    img_label.setStyleSheet("border-radius: 6px;")
                    left_layout.addWidget(img_label)
        else:
            text_content = self.item_data.get('text_content', '')
            lines = text_content.split('\n')
            preview_text = '\n'.join(lines[:4])
            if len(lines) > 4:
                preview_text += '...'
                
            text_label = QLabel(preview_text)
            text_label.setObjectName("ItemText")
            text_label.setWordWrap(True)
            text_label.setTextInteractionFlags(Qt.NoTextInteraction)
            left_layout.addWidget(text_label)

        # Meta timestamp badge at bottom left
        timestamp = self.item_data.get('last_used_at', 0)
        time_str = datetime.datetime.fromtimestamp(timestamp).strftime("%H:%M") if timestamp else ""
        if time_str:
            meta_label = QLabel(f"Copy • {time_str}")
            meta_label.setObjectName("ItemMeta")
            left_layout.addWidget(meta_label)

        # 2. Right Action Column (Top: `···` Options menu, Bottom: `📌` Pin icon)
        right_column = QVBoxLayout()
        right_column.setContentsMargins(0, 0, 0, 0)

        # Top Right: `···` Three Dots Options Button
        self.options_btn = QPushButton("•••")
        self.options_btn.setObjectName("CardActionButton")
        self.options_btn.setToolTip("More options")
        self.options_btn.clicked.connect(self._show_options_menu)

        # Bottom Right: `📌` Pin Button
        self.pin_btn = QPushButton("📌")
        self.pin_btn.setObjectName("CardActionButton")
        self.pin_btn.setProperty("pinned", "true" if self.item_data.get('is_pinned') else "false")
        self.pin_btn.setToolTip("Unpin item" if self.item_data.get('is_pinned') else "Pin item")
        self.pin_btn.clicked.connect(self._on_pin_clicked)

        right_column.addWidget(self.options_btn, alignment=Qt.AlignTop | Qt.AlignRight)
        right_column.addStretch()
        right_column.addWidget(self.pin_btn, alignment=Qt.AlignBottom | Qt.AlignRight)

        main_layout.addWidget(left_container, stretch=1)
        main_layout.addLayout(right_column)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            self.item_clicked.emit(self.item_data)
            event.accept()
            return
        super().mousePressEvent(event)

    def _show_options_menu(self):
        menu = QMenu(self)
        menu.setStyleSheet("""
            QMenu {
                background-color: #2b2b2b;
                color: #ffffff;
                border: 1px solid #444444;
                border-radius: 6px;
                padding: 4px;
            }
            QMenu::item {
                padding: 6px 16px;
                border-radius: 4px;
            }
            QMenu::item:selected {
                background-color: #383838;
            }
        """)

        copy_action = QAction("📋 Copy to clipboard", self)
        copy_action.triggered.connect(lambda: self.item_clicked.emit(self.item_data))

        pin_label = "📌 Unpin from top" if self.item_data.get('is_pinned') else "📌 Pin to top"
        pin_action = QAction(pin_label, self)
        pin_action.triggered.connect(self._on_pin_clicked)

        del_action = QAction("🗑️ Delete item", self)
        del_action.triggered.connect(self._on_delete_clicked)

        menu.addAction(copy_action)
        menu.addAction(pin_action)
        menu.addSeparator()
        menu.addAction(del_action)
        menu.exec(self.options_btn.mapToGlobal(self.options_btn.rect().bottomLeft()))

    def _on_pin_clicked(self):
        item_id = self.item_data['id']
        self.pin_toggled.emit(item_id)

    def _on_delete_clicked(self):
        item_id = self.item_data['id']
        self.delete_requested.emit(item_id)
