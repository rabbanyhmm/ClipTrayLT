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

    // 7. Rigorous No-Duplicate Rule Verification (Text & Images)
    StorageManager dedup_storage("/tmp/test_sc_dedup.db");
    dedup_storage.addItem("text", "Apple");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    dedup_storage.addItem("text", "Banana");
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    dedup_storage.addItem("text", "Cherry");

    auto list1 = dedup_storage.getItems();
    assert(list1.size() == 3);
    assert(list1[0].text_content == "Cherry");
    assert(list1[1].text_content == "Banana");
    assert(list1[2].text_content == "Apple");

    // Re-copy "Apple": should move from bottom (index 2) to top (index 0), NO duplicate created!
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    dedup_storage.addItem("text", "Apple");
    auto list2 = dedup_storage.getItems();
    assert(list2.size() == 3); // Still exactly 3 items!
    assert(list2[0].text_content == "Apple"); // Moved to top
    assert(list2[1].text_content == "Cherry");
    assert(list2[2].text_content == "Banana");

    // Test Image deduplication
    std::vector<uint8_t> imgA = {0x89, 'P', 'N', 'G', 0x01};
    std::vector<uint8_t> imgB = {0x89, 'P', 'N', 'G', 0x02};
    dedup_storage.addItem("image", "", "", imgA);
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    dedup_storage.addItem("image", "", "", imgB);

    auto list3 = dedup_storage.getItems();
    assert(list3.size() == 5); // 3 text + 2 images
    assert(list3[0].image_data == imgB);

    // Re-copy imgA: should jump to very top, NO duplicate created!
    std::this_thread::sleep_for(std::chrono::milliseconds(2));
    dedup_storage.addItem("image", "", "", imgA);
    auto list4 = dedup_storage.getItems();
    assert(list4.size() == 5); // Still exactly 5 items
    assert(list4[0].image_data == imgA); // imgA moved to the very top!
    std::filesystem::remove("/tmp/test_sc_dedup.db");
    std::cout << "No-duplicate rule for text & images verified successfully!\n";

    // 8. Test Large Content (5 MB payload)
    std::string large_db = "/tmp/test_sc_large.db";
    StorageManager large_storage(large_db);
    std::string large_payload(5 * 1024 * 1024, 'X');
    large_payload[0] = 'S';
    large_payload[large_payload.size() - 1] = 'E';
    int64_t large_id = large_storage.addItem("text", large_payload);
    assert(large_id > 0);

    auto retrieved_large = large_storage.getItemById(large_id);
    assert(retrieved_large.has_value());
    assert(retrieved_large->text_content.size() == 5 * 1024 * 1024);
    assert(retrieved_large->text_content.front() == 'S');
    assert(retrieved_large->text_content.back() == 'E');
    std::filesystem::remove(large_db);
    std::cout << "Large 5MB payload storage and retrieval verified successfully!\n";

    // 9. Test Raw Bytes with Embedded Nulls (\0) and Binary Values
    std::string raw_db = "/tmp/test_sc_raw.db";
    StorageManager raw_storage(raw_db);
    std::string raw_data = "PREFIX\0\0BINARY\x01\x02\xFF\xFE\0SUFFIX";
    std::string raw_string(raw_data.data(), 27); // explicitly 27 bytes containing 3 null bytes
    assert(raw_string.size() == 27);

    int64_t raw_id = raw_storage.addItem("raw", raw_string);
    assert(raw_id > 0);

    auto retrieved_raw = raw_storage.getItemById(raw_id);
    assert(retrieved_raw.has_value());
    assert(retrieved_raw->text_content.size() == 27);
    assert(retrieved_raw->text_content == raw_string);

    auto items_raw = raw_storage.getItems(10);
    assert(!items_raw.empty());
    assert(items_raw[0].text_content.size() == 27);
    assert(items_raw[0].text_content == raw_string);
    std::filesystem::remove(raw_db);
    std::cout << "Raw binary payload with embedded nulls verified successfully!\n";

    return 0;
}
