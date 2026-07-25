import time
import subprocess
import logging
from pynput.keyboard import Key, Controller

logger = logging.getLogger("PasteInjector")

class PasteInjector:
    def __init__(self):
        self.keyboard = Controller()
        self._ui = None
        
        # Initialize virtual hardware keyboard once at startup to bypass OS device registration delay
        try:
            import evdev
            from evdev import ecodes
            cap = {
                ecodes.EV_KEY: [
                    ecodes.KEY_LEFTCTRL,
                    ecodes.KEY_V,
                    ecodes.KEY_LEFTSHIFT,
                    ecodes.KEY_INSERT
                ]
            }
            self._ui = evdev.UInput(cap, name="virtual-clipboard-keyboard")
            logger.info("Hardware virtual keyboard initialized successfully.")
        except PermissionError:
            logger.debug("uinput requires root/sudo permissions for direct hardware emulation. Fallbacks active.")
        except Exception as e:
            logger.debug(f"uinput initialization note: {e}")

    def paste(self, delay: float = 0.25):
        """
        Waits for window manager focus, clears modifiers, and injects paste instantly using the cached virtual device.
        """
        # 1. Wait for Linux window manager focus transition
        time.sleep(delay)

        # 2. Try instant hardware keyboard injection via cached device
        if self._ui:
            try:
                from evdev import ecodes
                self._ui.write(ecodes.EV_KEY, ecodes.KEY_LEFTCTRL, 1)  # Ctrl Press
                self._ui.syn()
                self._ui.write(ecodes.EV_KEY, ecodes.KEY_V, 1)         # V Press
                self._ui.syn()
                self._ui.write(ecodes.EV_KEY, ecodes.KEY_V, 0)         # V Release
                self._ui.syn()
                self._ui.write(ecodes.EV_KEY, ecodes.KEY_LEFTCTRL, 0)  # Ctrl Release
                self._ui.syn()
                logger.info("Executed paste via cached hardware virtual keyboard (ctrl+v).")
                return
            except Exception as e:
                logger.debug(f"Hardware injection error: {e}")

        # 3. Release user-space modifier keys (Super/Win, Alt, Ctrl, Shift)
        try:
            self.keyboard.release(Key.cmd)
            self.keyboard.release(Key.alt)
            self.keyboard.release(Key.ctrl)
            self.keyboard.release(Key.shift)
        except Exception:
            pass

        time.sleep(0.05)

        # 4. Try xdotool if installed (X11 only)
        try:
            res = subprocess.run(["xdotool", "key", "--clearmodifiers", "ctrl+v"], capture_output=True, timeout=1)
            if res.returncode == 0:
                logger.info("Executed paste via xdotool (ctrl+v).")
                return
        except Exception:
            pass

        # 5. pynput Controller fallback (X11/Wayland User-space emulation)
        try:
            self.keyboard.press(Key.ctrl)
            self.keyboard.press('v')
            self.keyboard.release('v')
            self.keyboard.release(Key.ctrl)
            logger.info("Executed paste via pynput (ctrl+v).")
        except Exception as e:
            logger.error(f"Error during pynput paste injection: {e}")

    def cleanup(self):
        if self._ui:
            try:
                self._ui.close()
            except Exception:
                pass
