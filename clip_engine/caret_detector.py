import logging
from typing import Optional, Tuple
from PySide6.QtGui import QCursor

logger = logging.getLogger("CaretDetector")

class CaretDetector:
    @staticmethod
    def get_focused_text_input_position() -> Optional[Tuple[int, int]]:
        """
        Detects if an active text input control has focus and queries its caret position.
        Queries the Input Method framework (IBus) via DBus for exact caret coordinates.
        Falls back to active window cursor positioning.
        """
        # 1. Try querying IBus DBus cursor location
        try:
            import dbus
            bus = dbus.SessionBus()
            
            # Retrieve IBus address from DBus session bus
            try:
                ibus_obj = bus.get_object('org.freedesktop.IBus', '/org/freedesktop/IBus')
                ibus_iface = dbus.Interface(ibus_obj, 'org.freedesktop.IBus')
                current_input_context_path = ibus_iface.current_input_context()
                
                if current_input_context_path:
                    ctx_obj = bus.get_object('org.freedesktop.IBus', current_input_context_path)
                    ctx_iface = dbus.Interface(ctx_obj, 'org.freedesktop.IBus.InputContext')
                    
                    # IBus returns (x, y, width, height) of the active cursor/caret
                    loc = ctx_iface.GetCursorLocation()
                    if loc and len(loc) >= 2 and loc[0] > 0 and loc[1] > 0:
                        logger.info(f"Retrieved precise caret location from IBus: X={loc[0]}, Y={loc[1]}")
                        return (int(loc[0]), int(loc[1]))
            except Exception:
                pass
        except Exception:
            pass

        # 2. Fall back to focused window class check and mouse cursor location
        try:
            from Xlib import display as xdisplay
            d = xdisplay.Display()
            
            focus_obj = d.get_input_focus()
            focused_window = focus_obj.focus if focus_obj else None

            if focused_window and not isinstance(focused_window, int):
                wm_class = None
                curr = focused_window
                for _ in range(5):
                    if curr and not isinstance(curr, int):
                        try:
                            wm_class = curr.get_wm_class()
                            if wm_class:
                                break
                            parent_query = curr.query_tree()
                            curr = parent_query.parent if parent_query else None
                        except Exception:
                            break

                class_str = ""
                if wm_class:
                    class_str = (str(wm_class[0]) + " " + str(wm_class[1])).lower()

                # List of application classes that accept text input
                text_input_keywords = (
                    'code', 'sublime_text', 'gedit', 'emacs', 'vim', 'neovim',
                    'terminal', 'xterm', 'konsole', 'alacritty', 'kitty', 'tilix',
                    'firefox', 'chrome', 'chromium', 'opera', 'brave', 'edge',
                    'slack', 'discord', 'office', 'thunderbird', 'mail', 'chat',
                    'editor', 'studio', 'intellij', 'pycharm', 'eclipse', 'text',
                    'entry', 'input', 'search', 'query'
                )

                if any(kw in class_str for kw in text_input_keywords):
                    cursor_pos = QCursor.pos()
                    return (cursor_pos.x(), cursor_pos.y())

            logger.info("No editable text control focused. Placing flyout at bottom-right corner.")
            return None
        except Exception as e:
            logger.debug(f"Xlib focus query note: {e}")
            
        return None
