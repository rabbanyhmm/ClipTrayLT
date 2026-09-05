#pragma once

#include <string>
#include <vector>
#include <optional>
#include <cstdint>
#include <sqlite3.h>

struct ClipboardRecord {
    int64_t id = 0;
    std::string content_type; // "text" or "image"
    std::string text_content;
    std::string html_content;
    std::vector<uint8_t> image_data;
    bool is_pinned = false;
    int64_t created_at = 0;
};

class StorageManager {
public:
    explicit StorageManager(const std::string& db_path = "");
    ~StorageManager();

    bool init();
    int64_t addItem(const std::string& content_type,
                    const std::string& text_content,
                    const std::string& html_content = "",
                    const std::vector<uint8_t>& image_data = {},
                    int max_items = -1);

    void enforceMaxItems(int max_items = -1);
    std::vector<ClipboardRecord> getItems(int limit = -1, const std::string& query = "");
    std::optional<ClipboardRecord> getItemById(int64_t id);
    bool togglePin(int64_t id);
    bool deleteItem(int64_t id);
    bool clearUnpinned();

private:
    std::string db_path_;
    sqlite3* db_ = nullptr;
    void ensureSchema();
};
