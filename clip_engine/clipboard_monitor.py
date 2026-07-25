import sys
import os
import logging
from PySide6.QtCore import QObject, Signal, QTimer, QMimeData, QByteArray
from PySide6.QtGui import QClipboard, QGuiApplication
from clip_engine.format_handler import FormatHandler
from clip_engine.storage import StorageManager

logger = logging.getLogger("ClipboardMonitor")

class ClipboardMonitor(QObject):
    item_added = Signal(dict)

    def __init__(self, storage: StorageManager, clipboard: QClipboard):
        super().__init__()
        self.storage = storage
        self.clipboard = clipboard
        self.format_handler = FormatHandler()
        self.ignore_next = False
        
        # Connect PySide6 low-level clipboard changed event signal
        self.clipboard.dataChanged.connect(self._on_data_changed)
        
        # Debounce timer to prevent rapid duplicate events
        self._debounce_timer = QTimer()
        self._debounce_timer.setSingleShot(True)
        self._debounce_timer.setInterval(150)
        self._debounce_timer.timeout.connect(self._process_clipboard)

    def _on_data_changed(self):
        if self.ignore_next:
            self.ignore_next = False
            return
        self._debounce_timer.start()

    def _process_clipboard(self):
        mime_data = self.clipboard.mimeData()
        if not mime_data:
            return

        content_type, text_content, html_content, image_path, raw_mime_map = self.format_handler.extract_mime_data(mime_data)
        
        if not content_type and not raw_mime_map:
            return

        item = self.storage.add_item(
            content_type=content_type or 'raw',
            text_content=text_content,
            html_content=html_content,
            image_path=image_path,
            raw_mime_map=raw_mime_map
        )
        
        if item:
            self.item_added.emit(item)

    def set_clipboard_content(self, item: dict):
        """
        Reconstructs 100% bit-exact QMimeData with raw binary streams for all MIME formats.
        """
        self.ignore_next = True
        raw_blob = item.get('raw_mime_blob')
        raw_mime_map = self.storage.deserialize_raw_mime(raw_blob) if raw_blob else {}

        if raw_mime_map:
            mime_data = QMimeData()
            for fmt, raw_bytes in raw_mime_map.items():
                try:
                    mime_data.setData(fmt, QByteArray(raw_bytes))
                except Exception as e:
                    logger.warning(f"Could not set raw mime format '{fmt}': {e}")
            self.clipboard.setMimeData(mime_data)
            logger.info(f"Restored raw QMimeData with {len(raw_mime_map)} format channels.")
        else:
            # Fallback for plain text or simple image
            content_type = item.get('content_type', 'text')
            if content_type in ('text', 'html'):
                text = item.get('text_content', '')
                self.clipboard.setText(text)
            elif content_type == 'image':
                image_path = item.get('image_path')
                if image_path and os.path.exists(image_path):
                    from PySide6.QtGui import QImage
                    img = QImage(image_path)
                    if not img.isNull():
                        self.clipboard.setImage(img)
