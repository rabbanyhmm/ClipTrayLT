#pragma once

#include <string>

struct Config {
    // Maximum items to keep in clipboard history (easily increase to 50, 100, etc.)
    int max_items = 25;

    // Save history to disk:
    // false (default) -> Volatile in-memory only (:memory:) for maximum privacy and zero disk footprint
    // true            -> High-performance persistent SQLite database on disk (WAL mode, memory mapped)
    bool save_to_disk = false;

    // Flyout appearance and dismissal animation durations (in milliseconds)
    int anim_appear_ms = 200;
    int anim_hide_ms = 160;

    // Custom database path (leave empty for default ~/.local/share/cliptraylt/history.db)
    std::string db_path = "";

    // Global configuration singleton
    static Config& get();

    // Load configuration from ~/.config/cliptraylt/config.ini or generate defaults
    void load();

    // Save configuration to ~/.config/cliptraylt/config.ini
    void save();

    // Path to config file
    static std::string getConfigFilePath();
};
