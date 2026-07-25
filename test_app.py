import os
import sys
import unittest
from PySide6.QtCore import QMimeData, QByteArray
from PySide6.QtWidgets import QApplication

sys.path.insert(0, os.path.dirname(__file__))

from clip_engine.storage import StorageManager
from clip_engine.format_handler import FormatHandler
from clip_engine.clipboard_monitor import ClipboardMonitor
from clip_ui.main_window import ClipboardFlyoutWindow

class TestWinClipboard(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not QApplication.instance():
            cls.app = QApplication([])
        else:
            cls.app = QApplication.instance()

    def setUp(self):
        self.db_path = "/tmp/test_clipboard_raw.db"
        if os.path.exists(self.db_path):
            os.remove(self.db_path)
        self.storage = StorageManager(db_path=self.db_path)
        self.format_handler = FormatHandler()

    def tearDown(self):
        if os.path.exists(self.db_path):
            os.remove(self.db_path)

    def test_storage_add_and_deduplication(self):
        item1 = self.storage.add_item('text', text_content="Hello World")
        self.assertIsNotNone(item1)
        self.assertEqual(item1['text_content'], "Hello World")
        self.assertEqual(item1['is_pinned'], 0)

        # Re-adding exact content should update timestamp, not duplicate
        item2 = self.storage.add_item('text', text_content="Hello World")
        self.assertEqual(item1['id'], item2['id'])

        items = self.storage.get_items()
        self.assertEqual(len(items), 1)

    def test_raw_mime_preservation(self):
        # Create QMimeData with multiple custom & standard raw formats
        mime = QMimeData()
        mime.setText("Plain text content")
        mime.setHtml("<b>Rich HTML content</b>")
        mime.setData("application/x-custom-blob", QByteArray(b"\x00\xff\xfe\xfd\x12\x34\x56\x78"))

        content_type, text_content, html_content, image_path, raw_mime_map = self.format_handler.extract_mime_data(mime)
        self.assertIn("text/plain", raw_mime_map)
        self.assertIn("text/html", raw_mime_map)
        self.assertIn("application/x-custom-blob", raw_mime_map)
        self.assertEqual(raw_mime_map["application/x-custom-blob"], b"\x00\xff\xfe\xfd\x12\x34\x56\x78")

        # Save to storage
        item = self.storage.add_item('text', text_content=text_content, html_content=html_content, raw_mime_map=raw_mime_map)
        fetched = self.storage.get_item_by_id(item['id'])
        
        # De-serialize raw mime blob
        restored_map = self.storage.deserialize_raw_mime(fetched['raw_mime_blob'])
        self.assertEqual(restored_map["application/x-custom-blob"], b"\x00\xff\xfe\xfd\x12\x34\x56\x78")

    def test_pinning_and_clear_unpinned(self):
        item1 = self.storage.add_item('text', text_content="Item 1")
        item2 = self.storage.add_item('text', text_content="Item 2")
        
        self.storage.toggle_pin(item1['id'])
        self.storage.clear_unpinned()
        
        remaining = self.storage.get_items()
        self.assertEqual(len(remaining), 1)
        self.assertEqual(remaining[0]['id'], item1['id'])

    def test_ui_instantiation(self):
        flyout = ClipboardFlyoutWindow(storage_manager=self.storage)
        flyout.reload_history()
        self.assertEqual(len(flyout.cards_list), 0)

        self.storage.add_item('text', text_content="Sample UI Card")
        flyout.reload_history()
        self.assertEqual(len(flyout.cards_list), 1)

if __name__ == '__main__':
    unittest.main()
