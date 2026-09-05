#include <cassert>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include "storage.h"
#include "config.h"

int main() {
    std::string test_db = "/tmp/test_sc_storage_25.db";
    if (std::filesystem::exists(test_db)) {
        std::filesystem::remove(test_db);
    }

    StorageManager storage(test_db);

    // 1. Add 30 items with distinct timestamps
    for (int i = 1; i <= 30; ++i) {
        std::string txt = "Item #" + std::to_string(i);
        storage.addItem("text", txt, "", {}, 25);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    auto items = storage.getItems(50);
    // Should be exactly 25 items max!
    assert(items.size() == 25);

    // Newest item (#30) should be at the very top (index 0)
    assert(items[0].text_content == "Item #30");
    // Oldest surviving unpinned item should be #6 (1..5 pruned)
    assert(items[24].text_content == "Item #6");

    // 2. Pin one item
    int64_t pin_id = items[15].id;
    storage.togglePin(pin_id);

    // 3. Add 10 more items
    for (int i = 31; i <= 40; ++i) {
        std::string txt = "Item #" + std::to_string(i);
        storage.addItem("text", txt, "", {}, 25);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    items = storage.getItems(50);
    // Total should be 25 unpinned + 1 pinned = 26 items!
    assert(items.size() == 26);
    // Pinned item should always be at the very top
    assert(items[0].id == pin_id);
    assert(items[0].is_pinned == true);

    // 4. Test re-copying existing item (should jump to top without duplicate)
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
    storage.addItem("text", "Item #20", "", {}, 25);
    items = storage.getItems(50);
    // First is pinned, second should now be the re-copied Item #20!
    assert(items[1].text_content == "Item #20");

    std::filesystem::remove(test_db);
    std::cout << "All 25-item auto-pruning & deduplication tests passed successfully!\n";

    // 5. Test Config dynamic max_items expansion (e.g. increase to 40)
    Config::get().max_items = 40;
    StorageManager storage2(test_db);
    for (int i = 1; i <= 50; ++i) {
        storage2.addItem("text", "ConfigItem #" + std::to_string(i));
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    auto items_dyn = storage2.getItems();
    assert(items_dyn.size() == 40);
    assert(items_dyn[0].text_content == "ConfigItem #50");
    std::filesystem::remove(test_db);
    std::cout << "Dynamic Config max_items test passed!\n";

    // 6. Test In-Memory mode (save_to_disk = false)
    Config::get().save_to_disk = false;
    StorageManager mem_storage; // resolves to :memory:
    for (int i = 1; i <= 10; ++i) {
        mem_storage.addItem("text", "RamItem #" + std::to_string(i));
    }
    auto mem_items = mem_storage.getItems();
    assert(mem_items.size() == 10);
    assert(mem_items[0].text_content == "RamItem #10");
    std::cout << "High-speed volatile RAM (:memory:) storage test passed!\n";

    return 0;
}
