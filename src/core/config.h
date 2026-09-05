#pragma once

#include <string>

struct Config {
    // Maximum items to keep in clipboard history (easily increase to 50, 100, etc.)
    int max_items = 25;

    // Save history to disk:
    // true  -> High-performance persistent SQLite database on disk (WAL mode, memory mapped)
    // false -> Volatile in-memory only (:memory:) for maximum privacy and speed (wiped on exit)
    bool save_to_disk = true;

    // Flyout appearance and dismissal animation durations (in milliseconds)
    int anim_appear_ms = 200;
    int anim_hide_ms = 160;

    // Custom database path (leave empty for default ~/.local/share/simpleclipboard/history.db)
    std::string db_path = "";

    // Global configuration singleton
    static Config& get();

    // Load configuration from ~/.config/simpleclipboard/config.ini or generate defaults
    void load();

    // Save configuration to ~/.config/simpleclipboard/config.ini
    void save();
};
