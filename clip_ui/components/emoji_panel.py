from PySide6.QtWidgets import QWidget, QGridLayout, QVBoxLayout, QPushButton, QScrollArea, QLabel
from PySide6.QtCore import Signal, Qt

EMOJIS = [
    "😀", "😃", "😄", "😁", "😆", "😅", "😂", "🤣", "😊", "😇",
    "🙂", "🙃", "😉", "😌", "😍", "🥰", "😘", "😗", "😙", "😚",
    "😋", "😛", "😝", "😜", "🤪", "🤨", "🧐", "🤓", "😎", "🤩",
    "🥳", "😏", "😒", "😞", "😔", "😟", "😕", "🙁", "☹️", "😣",
    "😖", "😫", "😩", "🥺", "😢", "😭", "😤", "😠", "😡", "🤬",
    "🤯", "😳", "🥵", "🥶", "😱", "😨", "😰", "😥", "😓", "🤗",
    "🤔", "🤭", "🤫", "🤥", "😶", "😐", "😑", "😬", "🙄", "😯",
    "😦", "😧", "😮", "😲", "🥱", "😴", "🤤", "😪", "😵", "🤐",
    "🥴", "🤢", "🤮", "🤧", "😷", "🤒", "🤕", "🤑", "🤠", "😈",
    "👍", "👎", "👏", "🙌", "👐", "🤲", "🤝", "🙏", "✌️", "🤟",
    "🔥", "✨", "🎉", "❤️", "💯", "🚀", "💡", "⭐", "⚡", "🎯"
]

KAOMOJI = [
    r"¯\_(ツ)_/¯", "( ͡° ͜ʖ ͡°)", "(╯°□°)╯︵ ┻━┻", "┬─┬ノ( º _ ºノ)", 
    "(•_•)", "( •_•)>⌐■-■", "(⌐■_■)", "(◕‿◕)", "(｡♥‿♥｡)",
    "o(≧▽x)o", "(>_<)", "(T_T)", "(>_<)", "(ಠ_ಠ)", "(¬_¬)"
]

SYMBOLS = [
    "©", "®", "™", "€", "£", "¥", "§", "¶", "†", "‡",
    "α", "β", "γ", "δ", "ε", "θ", "λ", "μ", "π", "σ",
    "±", "≠", "≈", "≤", "≥", "÷", "×", "∞", "√", "∑",
    "←", "↑", "→", "↓", "↔", "⇒", "⇔", "▲", "▼", "★"
]

class EmojiPanel(QWidget):
    symbol_selected = Signal(str)

    def __init__(self, mode: str = "emoji", parent=None):
        super().__init__(parent)
        self.mode = mode
        self._init_ui()

    def _init_ui(self):
        main_layout = QVBoxLayout(self)
        main_layout.setContentsMargins(6, 6, 6, 6)

        scroll = QScrollArea()
        scroll.setWidgetResizable(True)
        container = QWidget()
        grid = QGridLayout(container)
        grid.setContentsMargins(6, 6, 6, 6)
        grid.setSpacing(6)

        items = EMOJIS if self.mode == "emoji" else (KAOMOJI if self.mode == "kaomoji" else SYMBOLS)
        cols = 8 if self.mode in ("emoji", "symbols") else 2

        for idx, text in enumerate(items):
            btn = QPushButton(text)
            btn.setObjectName("EmojiItem")
            btn.setCursor(Qt.PointingHandCursor)
            btn.clicked.connect(lambda _, t=text: self.symbol_selected.emit(t))
            grid.addWidget(btn, idx // cols, idx % cols)

        scroll.setWidget(container)
        main_layout.addWidget(scroll)
