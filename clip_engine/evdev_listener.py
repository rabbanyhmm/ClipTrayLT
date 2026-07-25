import os
import glob
import threading
import logging
from PySide6.QtCore import QObject, Signal

logger = logging.getLogger("EvdevListener")

class EvdevListener(QObject):
    toggle_signal = Signal()

    def __init__(self):
        super().__init__()
        self._running = False
        self._threads = []
        self._devices = []

    def start(self):
        try:
            import evdev
            from evdev import ecodes

            devices = [evdev.InputDevice(path) for path in glob.glob('/dev/input/event*')]
            keyboard_devices = []

            for dev in devices:
                try:
                    caps = dev.capabilities()
                    if ecodes.EV_KEY in caps:
                        keys = caps[ecodes.EV_KEY]
                        if ecodes.KEY_V in keys and (ecodes.KEY_LEFTMETA in keys or ecodes.KEY_RIGHTMETA in keys or ecodes.KEY_A in keys):
                            keyboard_devices.append(dev)
                except Exception:
                    pass

            if not keyboard_devices:
                logger.info("No evdev keyboard devices accessible (standard user mode).")
                return

            self._running = True
            self._devices = keyboard_devices
            logger.info(f"Kernel evdev listener active on {len(keyboard_devices)} keyboard device(s).")

            for dev in keyboard_devices:
                t = threading.Thread(target=self._device_loop, args=(dev,), daemon=True)
                t.start()
                self._threads.append(t)

        except PermissionError:
            logger.info("Evdev requires root/sudo permissions for direct kernel keyboard hooking. Fallbacks active.")
        except Exception as e:
            logger.debug(f"Evdev init note: {e}")

    def _device_loop(self, dev):
        from evdev import ecodes
        pressed_keys = set()

        try:
            for event in dev.read_loop():
                if not self._running:
                    break
                if event.type == ecodes.EV_KEY:
                    val = event.value  # 0 = release, 1 = press, 2 = hold
                    code = event.code

                    if val in (1, 2):
                        pressed_keys.add(code)
                        has_super = ecodes.KEY_LEFTMETA in pressed_keys or ecodes.KEY_RIGHTMETA in pressed_keys
                        has_ctrl = ecodes.KEY_LEFTCTRL in pressed_keys or ecodes.KEY_RIGHTCTRL in pressed_keys
                        has_alt = ecodes.KEY_LEFTALT in pressed_keys or ecodes.KEY_RIGHTALT in pressed_keys
                        has_v = code == ecodes.KEY_V

                        if val == 1 and has_v and (has_super or (has_ctrl and has_alt)):
                            logger.info(f"Kernel evdev shortcut detected! (Device: {dev.name})")
                            pressed_keys.discard(ecodes.KEY_V)
                            self.toggle_signal.emit()

                    elif val == 0:
                        pressed_keys.discard(code)
        except Exception:
            pass

    def stop(self):
        self._running = False
