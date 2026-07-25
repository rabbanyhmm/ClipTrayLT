import os
import time
import hashlib
from typing import Tuple, Optional, Dict
from PySide6.QtGui import QClipboard, QImage
from PySide6.QtCore import QMimeData, QByteArray

THUMBNAIL_DIR = os.path.expanduser("~/.config/win_clipboard/thumbnails")

class FormatHandler:
    def __init__(self, thumbnail_dir: str = THUMBNAIL_DIR):
        self.thumbnail_dir = thumbnail_dir
        os.makedirs(self.thumbnail_dir, exist_ok=True)

    def extract_raw_mime_map(self, mime_data: QMimeData) -> Dict[str, bytes]:
        """
        Extracts raw binary payloads for all formats present on QMimeData.
        Returns: {mime_format_name: raw_bytes}
        """
        raw_map = {}
        if not mime_data:
            return raw_map

        for fmt in mime_data.formats():
            try:
                data_bytes = bytes(mime_data.data(fmt))
                if data_bytes:
                    raw_map[fmt] = data_bytes
            except Exception:
                pass
        return raw_map

    def extract_mime_data(self, mime_data: QMimeData) -> Tuple[Optional[str], Optional[str], Optional[str], Optional[str], Dict[str, bytes]]:
        """
        Extracts preview data and raw MIME payload map.
        Returns: (content_type, text_content, html_content, image_path, raw_mime_map)
        """
        if mime_data is None:
            return None, None, None, None, {}

        raw_mime_map = self.extract_raw_mime_map(mime_data)

        # 1. Check for Image preview
        image_path = None
        content_type = None
        if mime_data.hasImage():
            image = mime_data.imageData()
            if isinstance(image, QImage) and not image.isNull():
                img_filename = f"img_{int(time.time() * 1000)}.png"
                image_path = os.path.join(self.thumbnail_dir, img_filename)
                image.save(image_path, "PNG")
                content_type = 'image'

        # 2. Check for HTML
        html_content = mime_data.html() if mime_data.hasHtml() else None
        
        # 3. Check for Plain Text
        text_content = mime_data.text() if mime_data.hasText() else None
        
        if not content_type:
            if text_content and text_content.strip():
                content_type = 'text'
            elif html_content:
                content_type = 'html'
            elif raw_mime_map:
                content_type = 'raw'

        if not content_type and not raw_mime_map:
            return None, None, None, None, {}

        return content_type or 'raw', text_content, html_content, image_path, raw_mime_map
