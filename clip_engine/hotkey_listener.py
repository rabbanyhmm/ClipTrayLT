import logging
from PySide6.QtCore import QObject, Signal
from pynput import keyboard

logger = logging.getLogger("HotkeyListener")

class HotkeyListener(QObject):
    toggle_signal = Signal()

    def __init__(self):
        super().__init__()
        self.listener = None
        self._pressed_keys = set()

    def start(self):
        try:
            self.listener = keyboard.Listener(
                on_press=self._on_key_press,
                on_release=self._on_key_release
            )
            self.listener.start()
            logger.info("Global hotkey listener active for Super+V (Win+V). No sudo required.")
        except Exception as e:
            logger.error(f"Failed to start hotkey listener: {e}")

    def _normalize_key(self, key):
        # 1. Super / Win / Cmd / Meta keys
        if key in (keyboard.Key.cmd, getattr(keyboard.Key, 'cmd_l', None), getattr(keyboard.Key, 'cmd_r', None)):
            return 'super'

        vk = getattr(key, 'vk', None)
        if vk in (133, 134, 125, 126, 65515, 65516):
            return 'super'

        key_name = str(getattr(key, 'name', '') or '').lower()
        key_str = str(key).lower()

        if any(term in key_name for term in ('super', 'cmd', 'meta', 'win')):
            return 'super'
        if any(term in key_str for term in ('super', 'cmd', 'meta', 'win')):
            return 'super'

        # 2. Ctrl keys
        if key in (keyboard.Key.ctrl, getattr(keyboard.Key, 'ctrl_l', None), getattr(keyboard.Key, 'ctrl_r', None)):
            return 'ctrl'
        if vk in (29, 97, 65507, 65508) or 'ctrl' in key_name or 'ctrl' in key_str:
            return 'ctrl'

        # 3. Alt keys
        if key in (keyboard.Key.alt, getattr(keyboard.Key, 'alt_l', None), getattr(keyboard.Key, 'alt_r', None), getattr(keyboard.Key, 'alt_gr', None)):
            return 'alt'
        if vk in (56, 100, 65513, 65514) or 'alt' in key_name or 'alt' in key_str:
            return 'alt'

        # 4. 'V' key
        try:
            if hasattr(key, 'char') and key.char:
                return key.char.lower()
            if vk and (vk == 86 or vk == 55 or (32 <= vk <= 126 and chr(vk).lower() == 'v')):
                return 'v'
        except Exception:
            pass

        return key_str

    def _on_key_press(self, key):
        norm = self._normalize_key(key)
        self._pressed_keys.add(norm)

        has_super = 'super' in self._pressed_keys
        has_ctrl = 'ctrl' in self._pressed_keys
        has_alt = 'alt' in self._pressed_keys
        has_v = 'v' in self._pressed_keys

        # Trigger on Super+V (or Ctrl+Alt+V fallback)
        if has_v and (has_super or (has_ctrl and has_alt)):
            logger.info(f"Hotkey triggered: {'Super+V (Win+V)' if has_super else 'Ctrl+Alt+V'}")
            self._pressed_keys.discard('v')
            self.toggle_signal.emit()

    def _on_key_release(self, key):
        norm = self._normalize_key(key)
        self._pressed_keys.discard(norm)

    def stop(self):
        if self.listener:
            self.listener.stop()
