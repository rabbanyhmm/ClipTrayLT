from PySide6.QtWidgets import QWidget, QHBoxLayout, QVBoxLayout, QLabel, QLineEdit, QPushButton
from PySide6.QtCore import Signal, Qt

class TopTitleWidget(QWidget):
    close_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setCursor(Qt.SizeAllCursor)
        self._init_ui()

    def _init_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(14, 10, 14, 4)

        title_label = QLabel("Emoji and more ⊞ ◽")
        title_label.setObjectName("TopWindowTitle")
        title_label.setAttribute(Qt.WA_TransparentForMouseEvents, True)

        close_btn = QPushButton("✕")
        close_btn.setObjectName("CloseBtn")
        close_btn.setCursor(Qt.PointingHandCursor)
        close_btn.setToolTip("Close (Esc)")
        close_btn.clicked.connect(self.close_requested.emit)

        layout.addWidget(title_label)
        layout.addStretch()
        layout.addWidget(close_btn)

    def mousePressEvent(self, event):
        if event.button() == Qt.LeftButton:
            win = self.window()
            if win:
                win._is_system_dragging = True
                if win.windowHandle():
                    win.windowHandle().startSystemMove()
                    event.accept()


class ClipboardSubHeaderWidget(QWidget):
    search_changed = Signal(str)
    clear_unpinned_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self._init_ui()

    def _init_ui(self):
        layout = QVBoxLayout(self)
        layout.setContentsMargins(14, 8, 14, 4)
        layout.setSpacing(8)

        section_header = QHBoxLayout()
        section_header.setContentsMargins(0, 0, 0, 0)

        section_title = QLabel("Clipboard")
        section_title.setObjectName("SectionTitle")

        self.clear_btn = QPushButton("Clear all")
        self.clear_btn.setObjectName("ClearAllPillBtn")
        self.clear_btn.setCursor(Qt.PointingHandCursor)
        self.clear_btn.setToolTip("Clear all unpinned clipboard items")
        self.clear_btn.clicked.connect(self.clear_unpinned_requested.emit)

        section_header.addWidget(section_title)
        section_header.addStretch()
        section_header.addWidget(self.clear_btn)

        self.search_input = QLineEdit()
        self.search_input.setObjectName("SearchInput")
        self.search_input.setPlaceholderText("🔍 Search clipboard history...")
        self.search_input.setClearButtonEnabled(True)
        self.search_input.textChanged.connect(self.search_changed.emit)

        layout.addLayout(section_header)
        layout.addWidget(self.search_input)

    def focus_search(self):
        self.search_input.setFocus()
        self.search_input.selectAll()
