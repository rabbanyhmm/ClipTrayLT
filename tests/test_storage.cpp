#include <cassert>
#include <iostream>
#include <filesystem>
#include <thread>
#include <chrono>
#include "storage.h"

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
    return 0;
}
