import sqlite3
import os
import time
import hashlib
import zlib
import pickle
from typing import List, Dict, Optional, Tuple

DB_PATH = os.path.expanduser("~/.config/win_clipboard/clipboard.db")

class StorageManager:
    def __init__(self, db_path: str = DB_PATH):
        self.db_path = db_path
        os.makedirs(os.path.dirname(self.db_path), exist_ok=True)
        self._init_db()

    def _get_connection(self) -> sqlite3.Connection:
        conn = sqlite3.connect(self.db_path)
        conn.row_factory = sqlite3.Row
        return conn

    def _init_db(self):
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("""
                CREATE TABLE IF NOT EXISTS clipboard_history (
                    id INTEGER PRIMARY KEY AUTOINCREMENT,
                    content_type TEXT NOT NULL, -- 'text', 'html', 'image', 'file', 'raw'
                    text_content TEXT,
                    html_content TEXT,
                    image_path TEXT,
                    raw_mime_blob BLOB,
                    content_hash TEXT UNIQUE NOT NULL,
                    char_count INTEGER DEFAULT 0,
                    line_count INTEGER DEFAULT 0,
                    is_pinned INTEGER DEFAULT 0,
                    created_at REAL NOT NULL,
                    last_used_at REAL NOT NULL
                )
            """)
            
            # Migration check: add raw_mime_blob if missing in existing table
            cursor.execute("PRAGMA table_info(clipboard_history)")
            columns = [col['name'] for col in cursor.fetchall()]
            if 'raw_mime_blob' not in columns:
                cursor.execute("ALTER TABLE clipboard_history ADD COLUMN raw_mime_blob BLOB")

            cursor.execute("CREATE INDEX IF NOT EXISTS idx_pinned ON clipboard_history(is_pinned)")
            cursor.execute("CREATE INDEX IF NOT EXISTS idx_last_used ON clipboard_history(last_used_at DESC)")
            conn.commit()

    def serialize_raw_mime(self, raw_mime_map: Dict[str, bytes]) -> bytes:
        """Compresses dictionary of {mime_type: raw_bytes} using pickle + zlib."""
        if not raw_mime_map:
            return b""
        return zlib.compress(pickle.dumps(raw_mime_map))

    def deserialize_raw_mime(self, blob: bytes) -> Dict[str, bytes]:
        """Decompresses binary blob back into {mime_type: raw_bytes} dictionary."""
        if not blob:
            return {}
        try:
            return pickle.loads(zlib.decompress(blob))
        except Exception:
            return {}

    def add_item(self, content_type: str, text_content: Optional[str] = None, 
                 html_content: Optional[str] = None, image_path: Optional[str] = None, 
                 raw_mime_map: Optional[Dict[str, bytes]] = None,
                 max_unpinned_items: int = 50) -> Optional[Dict]:
        """
        Adds a new item or updates timestamp if duplicate hash exists.
        Stores raw_mime_map for 100% bit-exact restoration.
        """
        raw_mime_blob = self.serialize_raw_mime(raw_mime_map) if raw_mime_map else b""

        # Hash calculation: if raw_mime_map is provided, hash the sorted raw MIME data
        if raw_mime_map:
            hash_builder = hashlib.sha256()
            for key in sorted(raw_mime_map.keys()):
                hash_builder.update(key.encode('utf-8'))
                hash_builder.update(raw_mime_map[key])
            content_hash = hash_builder.hexdigest()
        else:
            raw_bytes = (text_content or "").encode('utf-8') if content_type in ('text', 'html') else (image_path or "").encode('utf-8')
            if not raw_bytes and not image_path:
                return None
            content_hash = hashlib.sha256(raw_bytes).hexdigest()

        now = time.time()
        char_count = len(text_content) if text_content else 0
        line_count = text_content.count('\n') + 1 if text_content else 0

        with self._get_connection() as conn:
            cursor = conn.cursor()
            
            # Check if hash exists
            cursor.execute("SELECT id, is_pinned FROM clipboard_history WHERE content_hash = ?", (content_hash,))
            existing = cursor.fetchone()
            
            if existing:
                item_id = existing['id']
                cursor.execute("""
                    UPDATE clipboard_history 
                    SET last_used_at = ?, raw_mime_blob = CASE WHEN LENGTH(?) > 0 THEN ? ELSE raw_mime_blob END
                    WHERE id = ?
                """, (now, raw_mime_blob, raw_mime_blob, item_id))
                conn.commit()
                return self.get_item_by_id(item_id)

            # Insert new item
            cursor.execute("""
                INSERT INTO clipboard_history 
                (content_type, text_content, html_content, image_path, raw_mime_blob, content_hash, char_count, line_count, is_pinned, created_at, last_used_at)
                VALUES (?, ?, ?, ?, ?, ?, ?, ?, 0, ?, ?)
            """, (content_type, text_content, html_content, image_path, raw_mime_blob, content_hash, char_count, line_count, now, now))
            
            new_id = cursor.lastrowid
            
            # Cleanup old unpinned items if limit reached
            cursor.execute("""
                DELETE FROM clipboard_history 
                WHERE is_pinned = 0 AND id NOT IN (
                    SELECT id FROM clipboard_history 
                    WHERE is_pinned = 0 
                    ORDER BY last_used_at DESC 
                    LIMIT ?
                )
            """, (max_unpinned_items,))
            
            conn.commit()
            return self.get_item_by_id(new_id)

    def get_items(self, search_query: Optional[str] = None, filter_type: Optional[str] = None) -> List[Dict]:
        with self._get_connection() as conn:
            cursor = conn.cursor()
            query = "SELECT * FROM clipboard_history WHERE 1=1"
            params = []
            
            if filter_type == 'pinned':
                query += " AND is_pinned = 1"
            elif filter_type and filter_type != 'all':
                query += " AND content_type = ?"
                params.append(filter_type)
                
            if search_query:
                query += " AND text_content LIKE ?"
                params.append(f"%{search_query}%")
                
            query += " ORDER BY is_pinned DESC, last_used_at DESC"
            cursor.execute(query, params)
            return [dict(row) for row in cursor.fetchall()]

    def get_item_by_id(self, item_id: int) -> Optional[Dict]:
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT * FROM clipboard_history WHERE id = ?", (item_id,))
            row = cursor.fetchone()
            return dict(row) if row else None

    def toggle_pin(self, item_id: int) -> bool:
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("UPDATE clipboard_history SET is_pinned = 1 - is_pinned WHERE id = ?", (item_id,))
            conn.commit()
            cursor.execute("SELECT is_pinned FROM clipboard_history WHERE id = ?", (item_id,))
            row = cursor.fetchone()
            return bool(row['is_pinned']) if row else False

    def delete_item(self, item_id: int):
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT image_path FROM clipboard_history WHERE id = ?", (item_id,))
            row = cursor.fetchone()
            if row and row['image_path'] and os.path.exists(row['image_path']):
                try:
                    os.remove(row['image_path'])
                except Exception:
                    pass
            cursor.execute("DELETE FROM clipboard_history WHERE id = ?", (item_id,))
            conn.commit()

    def clear_unpinned(self):
        with self._get_connection() as conn:
            cursor = conn.cursor()
            cursor.execute("SELECT image_path FROM clipboard_history WHERE is_pinned = 0 AND image_path IS NOT NULL")
            for row in cursor.fetchall():
                if row['image_path'] and os.path.exists(row['image_path']):
                    try:
                        os.remove(row['image_path'])
                    except Exception:
                        pass
            cursor.execute("DELETE FROM clipboard_history WHERE is_pinned = 0")
            conn.commit()
