from PySide6.QtWidgets import QWidget, QHBoxLayout, QTabBar
from PySide6.QtCore import Signal, Qt

class NavigationTabs(QWidget):
    tab_changed = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._init_ui()

    def _init_ui(self):
        layout = QHBoxLayout(self)
        layout.setContentsMargins(12, 0, 12, 0)
        
        self.tab_bar = QTabBar()
        self.tab_bar.setCursor(Qt.PointingHandCursor)

        # Tab order matching official Windows 11 layout (Clipboard History on FAR RIGHT):
        # 0: Emoji (😊), 1: Kaomoji (;-)), 2: Symbols (🔣), 3: Clipboard History (📋 - Far Right!)
        self.tab_bar.addTab("😊")
        self.tab_bar.setTabToolTip(0, "Emojis")

        self.tab_bar.addTab(";-)")
        self.tab_bar.setTabToolTip(1, "Kaomoji")

        self.tab_bar.addTab("🔣")
        self.tab_bar.setTabToolTip(2, "Symbols")

        self.tab_bar.addTab("📋")
        self.tab_bar.setTabToolTip(3, "Clipboard History")

        # Default active tab is Clipboard History on far right (Index 3)
        self.tab_bar.setCurrentIndex(3)
        
        self.tab_bar.currentChanged.connect(self.tab_changed.emit)
        layout.addWidget(self.tab_bar)
