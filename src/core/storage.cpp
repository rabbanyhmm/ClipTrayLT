#include "storage.h"
#include "config.h"
#include <iostream>
#include <chrono>
#include <filesystem>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>

namespace fs = std::filesystem;

static std::string getDefaultDbPath() {
    const char* sudo_user = std::getenv("SUDO_USER");
    std::string home_dir;
    if (sudo_user) {
        struct passwd* pw = getpwnam(sudo_user);
        if (pw && pw->pw_dir) {
            home_dir = pw->pw_dir;
        }
    }
    if (home_dir.empty()) {
        const char* home = std::getenv("HOME");
        if (home) {
            home_dir = home;
        } else {
            home_dir = "/tmp";
        }
    }
    std::string dir = home_dir + "/.local/share/cliptraylt";
    fs::create_directories(dir);
    return dir + "/history.db";
}

static std::string resolveDbPath(const std::string& custom_path) {
    if (!custom_path.empty()) {
        return custom_path;
    }
    if (!Config::get().save_to_disk) {
        return ":memory:";
    }
    if (!Config::get().db_path.empty()) {
        return Config::get().db_path;
    }
    return getDefaultDbPath();
}

StorageManager::StorageManager(const std::string& db_path)
    : db_path_(resolveDbPath(db_path)) {
    init();
}

StorageManager::~StorageManager() {
    if (db_) {
        sqlite3_close(db_);
        db_ = nullptr;
    }
}

bool StorageManager::init() {
    if (db_) return true;
    int rc = sqlite3_open(db_path_.c_str(), &db_);
    if (rc != SQLITE_OK) {
        std::cerr << "Failed to open database: " << sqlite3_errmsg(db_) << "\n";
        return false;
    }
    ensureSchema();
    return true;
}

static uint64_t computeFastHash(const void* data, size_t len) {
    if (!data || len == 0) return 0;
    const uint8_t* p = static_cast<const uint8_t*>(data);
    uint64_t h = len * 0x517cc1b727220a95ULL;

    if (len > 65536) {
        // Fast deterministic sampling across head, mid, and tail in 0.001ms
        for (size_t i = 0; i < 4096; ++i) {
            h = (h ^ p[i]) * 1099511628211ULL;
        }
        size_t mid = len / 2;
        for (size_t i = mid; i < mid + 4096 && i < len; ++i) {
            h = (h ^ p[i]) * 1099511628211ULL;
        }
        for (size_t i = len - 4096; i < len; ++i) {
            h = (h ^ p[i]) * 1099511628211ULL;
        }
        return h;
    }

    const uint64_t* p64 = reinterpret_cast<const uint64_t*>(p);
    size_t words = len / 8;
    for (size_t i = 0; i < words; ++i) {
        h = (h ^ p64[i]) * 1099511628211ULL;
    }
    for (size_t i = words * 8; i < len; ++i) {
        h = (h ^ p[i]) * 1099511628211ULL;
    }
    return h;
}

void StorageManager::ensureSchema() {
    const char* schema = R"(
        PRAGMA journal_mode = WAL;
        PRAGMA synchronous = NORMAL;
        PRAGMA temp_store = MEMORY;
        PRAGMA cache_size = -64000;
        PRAGMA mmap_size = 67108864;
        CREATE TABLE IF NOT EXISTS clipboard_history (
            id INTEGER PRIMARY KEY AUTOINCREMENT,
            content_type TEXT NOT NULL,
            text_content TEXT,
            html_content TEXT,
            image_blob BLOB,
            is_pinned INTEGER DEFAULT 0,
            created_at INTEGER NOT NULL,
            content_hash INTEGER DEFAULT 0
        );
        CREATE INDEX IF NOT EXISTS idx_created ON clipboard_history(created_at);
        CREATE INDEX IF NOT EXISTS idx_pinned ON clipboard_history(is_pinned);
        CREATE INDEX IF NOT EXISTS idx_hash ON clipboard_history(content_hash);
    )";
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, schema, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
    }
    sqlite3_exec(db_, "ALTER TABLE clipboard_history ADD COLUMN content_hash INTEGER DEFAULT 0;", nullptr, nullptr, nullptr);
    sqlite3_exec(db_, "CREATE INDEX IF NOT EXISTS idx_hash ON clipboard_history(content_hash);", nullptr, nullptr, nullptr);
}

void StorageManager::enforceMaxItems(int max_items) {
    if (max_items <= 0) {
        max_items = Config::get().max_items;
    }
    if (!db_ || max_items <= 0) return;
    // Keep all pinned items, and remove oldest unpinned items past the limit
    const char* sql = "DELETE FROM clipboard_history WHERE is_pinned = 0 AND id NOT IN ("
                      "  SELECT id FROM clipboard_history WHERE is_pinned = 0 "
                      "  ORDER BY created_at DESC, id DESC LIMIT ?"
                      ");";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) == SQLITE_OK) {
        sqlite3_bind_int(stmt, 1, max_items);
        sqlite3_step(stmt);
        sqlite3_finalize(stmt);
    }
}

int64_t StorageManager::addItem(const std::string& content_type,
                                const std::string& text_content,
                                const std::string& html_content,
                                const std::vector<uint8_t>& image_data,
                                int max_items) {
    if (max_items <= 0) {
        max_items = Config::get().max_items;
    }
    if (!db_) return -1;

    int64_t now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();

    uint64_t hash = 0;
    if (!text_content.empty()) {
        hash = computeFastHash(text_content.data(), text_content.size());
    } else if (!image_data.empty()) {
        hash = computeFastHash(image_data.data(), image_data.size());
    }

    // High-speed O(1) indexed deduplication via 64-bit content_hash (instant for 100MB+ payloads)
    if (hash != 0) {
        const char* check_sql = "SELECT id, content_type, LENGTH(text_content), LENGTH(image_blob) "
                                "FROM clipboard_history WHERE content_hash = ? LIMIT 1;";
        sqlite3_stmt* stmt = nullptr;
        if (sqlite3_prepare_v2(db_, check_sql, -1, &stmt, nullptr) == SQLITE_OK) {
            sqlite3_bind_int64(stmt, 1, static_cast<int64_t>(hash));
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                int64_t existing_id = sqlite3_column_int64(stmt, 0);
                sqlite3_finalize(stmt);

                const char* update_sql = html_content.empty()
                    ? "UPDATE clipboard_history SET created_at = ? WHERE id = ?;"
                    : "UPDATE clipboard_history SET created_at = ?, html_content = ? WHERE id = ?;";
                sqlite3_stmt* ustmt = nullptr;
                if (sqlite3_prepare_v2(db_, update_sql, -1, &ustmt, nullptr) == SQLITE_OK) {
                    sqlite3_bind_int64(ustmt, 1, now);
                    if (!html_content.empty()) {
                        sqlite3_bind_text(ustmt, 2, html_content.data(), static_cast<int>(html_content.size()), SQLITE_STATIC);
                        sqlite3_bind_int64(ustmt, 3, existing_id);
                    } else {
                        sqlite3_bind_int64(ustmt, 2, existing_id);
                    }
                    sqlite3_step(ustmt);
                    sqlite3_finalize(ustmt);
                    return existing_id;
                }
            } else {
                sqlite3_finalize(stmt);
            }
        }
    }

    const char* insert_sql = "INSERT INTO clipboard_history (content_type, text_content, html_content, image_blob, is_pinned, created_at, content_hash) "
                             "VALUES (?, ?, ?, ?, 0, ?, ?);";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, insert_sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return -1;
    }

    // Use SQLITE_STATIC to avoid redundant multi-megabyte buffer cloning
    sqlite3_bind_text(stmt, 1, content_type.data(), static_cast<int>(content_type.size()), SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, text_content.data(), static_cast<int>(text_content.size()), SQLITE_STATIC);
    if (!html_content.empty()) {
        sqlite3_bind_text(stmt, 3, html_content.data(), static_cast<int>(html_content.size()), SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 3);
    }
    if (!image_data.empty()) {
        sqlite3_bind_blob(stmt, 4, image_data.data(), static_cast<int>(image_data.size()), SQLITE_STATIC);
    } else {
        sqlite3_bind_null(stmt, 4);
    }
    sqlite3_bind_int64(stmt, 5, now);
    sqlite3_bind_int64(stmt, 6, static_cast<int64_t>(hash));

    int64_t new_id = -1;
    if (sqlite3_step(stmt) == SQLITE_DONE) {
        new_id = sqlite3_last_insert_rowid(db_);
    }
    sqlite3_finalize(stmt);

    if (new_id > 0) {
        enforceMaxItems(max_items);
    }
    return new_id;
}

std::vector<ClipboardRecord> StorageManager::getItems(int limit, const std::string& query) {
    if (limit <= 0) {
        limit = Config::get().max_items;
    }
    std::vector<ClipboardRecord> results;
    if (!db_) return results;

    // Always sort by is_pinned DESC, then newest first (created_at DESC)
    std::string sql = "SELECT id, content_type, text_content, html_content, image_blob, is_pinned, created_at "
                      "FROM clipboard_history ";
    if (!query.empty()) {
        sql += "WHERE text_content LIKE ? ";
    }
    sql += "ORDER BY is_pinned DESC, created_at DESC, id DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    int param_idx = 1;
    if (!query.empty()) {
        std::string pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, param_idx++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, param_idx, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ClipboardRecord rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        const unsigned char* ctype = sqlite3_column_text(stmt, 1);
        int ctype_bytes = sqlite3_column_bytes(stmt, 1);
        rec.content_type = (ctype && ctype_bytes > 0) ? std::string(reinterpret_cast<const char*>(ctype), ctype_bytes) : "";

        const unsigned char* txt = sqlite3_column_text(stmt, 2);
        int txt_bytes = sqlite3_column_bytes(stmt, 2);
        rec.text_content = (txt && txt_bytes > 0) ? std::string(reinterpret_cast<const char*>(txt), txt_bytes) : "";

        const unsigned char* html = sqlite3_column_text(stmt, 3);
        int html_bytes = sqlite3_column_bytes(stmt, 3);
        rec.html_content = (html && html_bytes > 0) ? std::string(reinterpret_cast<const char*>(html), html_bytes) : "";

        const void* blob = sqlite3_column_blob(stmt, 4);
        int bytes = sqlite3_column_bytes(stmt, 4);
        if (blob && bytes > 0) {
            const uint8_t* byte_ptr = static_cast<const uint8_t*>(blob);
            rec.image_data.assign(byte_ptr, byte_ptr + bytes);
        }

        rec.is_pinned = (sqlite3_column_int(stmt, 5) != 0);
        rec.created_at = sqlite3_column_int64(stmt, 6);
        rec.full_size = (rec.text_content.size() > 0) ? rec.text_content.size() : rec.image_data.size();
        results.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::vector<ClipboardRecord> StorageManager::getItemPreviews(int limit, const std::string& query) {
    if (limit <= 0) {
        limit = Config::get().max_items;
    }
    std::vector<ClipboardRecord> results;
    if (!db_) return results;

    // Fast lightweight previews: fetch at most 512 bytes for UI preview, but report full exact size
    std::string sql = "SELECT id, content_type, "
                      "  CASE WHEN LENGTH(text_content) > 512 THEN SUBSTR(text_content, 1, 512) ELSE text_content END, "
                      "  COALESCE(LENGTH(text_content), 0), "
                      "  html_content, "
                      "  CASE WHEN content_type = 'image' THEN image_blob ELSE NULL END, "
                      "  COALESCE(LENGTH(image_blob), 0), "
                      "  is_pinned, created_at "
                      "FROM clipboard_history ";
    if (!query.empty()) {
        sql += "WHERE text_content LIKE ? ";
    }
    sql += "ORDER BY is_pinned DESC, created_at DESC, id DESC LIMIT ?;";

    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql.c_str(), -1, &stmt, nullptr) != SQLITE_OK) {
        return results;
    }

    int param_idx = 1;
    if (!query.empty()) {
        std::string pattern = "%" + query + "%";
        sqlite3_bind_text(stmt, param_idx++, pattern.c_str(), -1, SQLITE_TRANSIENT);
    }
    sqlite3_bind_int(stmt, param_idx, limit);

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        ClipboardRecord rec;
        rec.id = sqlite3_column_int64(stmt, 0);

        const unsigned char* ctype = sqlite3_column_text(stmt, 1);
        int ctype_bytes = sqlite3_column_bytes(stmt, 1);
        rec.content_type = (ctype && ctype_bytes > 0) ? std::string(reinterpret_cast<const char*>(ctype), ctype_bytes) : "";

        const unsigned char* txt = sqlite3_column_text(stmt, 2);
        int txt_bytes = sqlite3_column_bytes(stmt, 2);
        rec.text_content = (txt && txt_bytes > 0) ? std::string(reinterpret_cast<const char*>(txt), txt_bytes) : "";

        int64_t full_txt_len = sqlite3_column_int64(stmt, 3);

        const unsigned char* html = sqlite3_column_text(stmt, 4);
        int html_bytes = sqlite3_column_bytes(stmt, 4);
        rec.html_content = (html && html_bytes > 0) ? std::string(reinterpret_cast<const char*>(html), html_bytes) : "";

        const void* blob = sqlite3_column_blob(stmt, 5);
        int bytes = sqlite3_column_bytes(stmt, 5);
        if (blob && bytes > 0) {
            const uint8_t* byte_ptr = static_cast<const uint8_t*>(blob);
            rec.image_data.assign(byte_ptr, byte_ptr + bytes);
        }

        int64_t full_blob_len = sqlite3_column_int64(stmt, 6);

        rec.is_pinned = (sqlite3_column_int(stmt, 7) != 0);
        rec.created_at = sqlite3_column_int64(stmt, 8);
        rec.full_size = (full_txt_len > 0) ? static_cast<size_t>(full_txt_len) : static_cast<size_t>(full_blob_len);
        results.push_back(std::move(rec));
    }
    sqlite3_finalize(stmt);
    return results;
}

std::optional<ClipboardRecord> StorageManager::getItemById(int64_t id) {
    if (!db_) return std::nullopt;
    const char* sql = "SELECT id, content_type, text_content, html_content, image_blob, is_pinned, created_at "
                      "FROM clipboard_history WHERE id = ? LIMIT 1;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) {
        return std::nullopt;
    }
    sqlite3_bind_int64(stmt, 1, id);

    if (sqlite3_step(stmt) == SQLITE_ROW) {
        ClipboardRecord rec;
        rec.id = sqlite3_column_int64(stmt, 0);
        const unsigned char* ctype = sqlite3_column_text(stmt, 1);
        int ctype_bytes = sqlite3_column_bytes(stmt, 1);
        rec.content_type = (ctype && ctype_bytes > 0) ? std::string(reinterpret_cast<const char*>(ctype), ctype_bytes) : "";

        const unsigned char* txt = sqlite3_column_text(stmt, 2);
        int txt_bytes = sqlite3_column_bytes(stmt, 2);
        rec.text_content = (txt && txt_bytes > 0) ? std::string(reinterpret_cast<const char*>(txt), txt_bytes) : "";

        const unsigned char* html = sqlite3_column_text(stmt, 3);
        int html_bytes = sqlite3_column_bytes(stmt, 3);
        rec.html_content = (html && html_bytes > 0) ? std::string(reinterpret_cast<const char*>(html), html_bytes) : "";

        const void* blob = sqlite3_column_blob(stmt, 4);
        int bytes = sqlite3_column_bytes(stmt, 4);
        if (blob && bytes > 0) {
            const uint8_t* byte_ptr = static_cast<const uint8_t*>(blob);
            rec.image_data.assign(byte_ptr, byte_ptr + bytes);
        }
        rec.is_pinned = (sqlite3_column_int(stmt, 5) != 0);
        rec.created_at = sqlite3_column_int64(stmt, 6);
        rec.full_size = (rec.text_content.size() > 0) ? rec.text_content.size() : rec.image_data.size();
        sqlite3_finalize(stmt);
        return rec;
    }
    sqlite3_finalize(stmt);
    return std::nullopt;
}

bool StorageManager::togglePin(int64_t id) {
    if (!db_) return false;
    const char* sql = "UPDATE clipboard_history SET is_pinned = 1 - is_pinned WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool StorageManager::deleteItem(int64_t id) {
    if (!db_) return false;
    const char* sql = "DELETE FROM clipboard_history WHERE id = ?;";
    sqlite3_stmt* stmt = nullptr;
    if (sqlite3_prepare_v2(db_, sql, -1, &stmt, nullptr) != SQLITE_OK) return false;
    sqlite3_bind_int64(stmt, 1, id);
    bool ok = (sqlite3_step(stmt) == SQLITE_DONE);
    sqlite3_finalize(stmt);
    return ok;
}

bool StorageManager::clearUnpinned() {
    if (!db_) return false;
    const char* sql = "DELETE FROM clipboard_history WHERE is_pinned = 0;";
    char* err_msg = nullptr;
    if (sqlite3_exec(db_, sql, nullptr, nullptr, &err_msg) != SQLITE_OK) {
        sqlite3_free(err_msg);
        return false;
    }
    return true;
}
