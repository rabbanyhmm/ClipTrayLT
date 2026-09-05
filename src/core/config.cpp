#include "config.h"
#include <QSettings>
#include <filesystem>
#include <cstdlib>
#include <pwd.h>
#include <unistd.h>
#include <iostream>

namespace fs = std::filesystem;

static std::string getConfigDir() {
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
        home_dir = home ? home : "/tmp";
    }
    std::string dir = home_dir + "/.config/cliptraylt";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir;
}

std::string Config::getConfigFilePath() {
    return getConfigDir() + "/config.ini";
}

Config& Config::get() {
    static Config instance;
    return instance;
}

void Config::load() {
    std::string config_path = getConfigFilePath();
    if (!fs::exists(config_path)) {
        save();
        return;
    }

    QSettings settings(QString::fromStdString(config_path), QSettings::IniFormat);
    max_items = settings.value("Clipboard/max_items", max_items).toInt();
    save_to_disk = settings.value("Clipboard/save_to_disk", save_to_disk).toBool();
    anim_appear_ms = settings.value("Animation/appear_ms", anim_appear_ms).toInt();
    anim_hide_ms = settings.value("Animation/hide_ms", anim_hide_ms).toInt();
    std::string custom_db = settings.value("Storage/db_path", QString::fromStdString(db_path)).toString().toStdString();
    if (!custom_db.empty()) {
        db_path = custom_db;
    }

    if (max_items < 1) max_items = 25;
    if (anim_appear_ms < 0) anim_appear_ms = 200;
    if (anim_hide_ms < 0) anim_hide_ms = 160;

    std::cout << "[Config] Loaded configuration: max_items=" << max_items
              << ", save_to_disk=" << (save_to_disk ? "true" : "false")
              << ", anim_appear=" << anim_appear_ms << "ms"
              << ", anim_hide=" << anim_hide_ms << "ms\n";
}

void Config::save() {
    std::string config_path = getConfigFilePath();
    QSettings settings(QString::fromStdString(config_path), QSettings::IniFormat);

    settings.setValue("Clipboard/max_items", max_items);
    settings.setValue("Clipboard/save_to_disk", save_to_disk);
    settings.setValue("Animation/appear_ms", anim_appear_ms);
    settings.setValue("Animation/hide_ms", anim_hide_ms);
    settings.setValue("Storage/db_path", QString::fromStdString(db_path));
    settings.sync();
}
