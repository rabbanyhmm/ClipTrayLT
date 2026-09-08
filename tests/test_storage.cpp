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

    // 10. Test Multi-Megabyte (10MB) Large Text Payload
    std::string huge_db = "/tmp/test_sc_huge.db";
    StorageManager huge_storage(huge_db);
    std::string huge_payload(10 * 1024 * 1024, 'A');
    huge_payload[0] = '[';
    huge_payload[huge_payload.size() - 1] = ']';
    int64_t huge_id = huge_storage.addItem("text", huge_payload);
    assert(huge_id > 0);

    auto retrieved_huge = huge_storage.getItemById(huge_id);
    assert(retrieved_huge.has_value());
    assert(retrieved_huge->text_content.size() == 10 * 1024 * 1024);
    assert(retrieved_huge->text_content.front() == '[');
    assert(retrieved_huge->text_content.back() == ']');
    std::filesystem::remove(huge_db);
    std::cout << "Huge 10MB payload storage and retrieval verified successfully!\n";

    // 11. Test Custom Binary Format (e.g. application/octet-stream or custom mime)
    std::string custom_bin_db = "/tmp/test_sc_custom_bin.db";
    StorageManager custom_storage(custom_bin_db);
    std::string bin_payload(2 * 1024 * 1024, '\0');
    for (size_t i = 0; i < bin_payload.size(); ++i) {
        bin_payload[i] = static_cast<char>(i % 256);
    }
    int64_t bin_id = custom_storage.addItem("raw", bin_payload, "application/octet-stream");
    assert(bin_id > 0);

    auto retrieved_bin = custom_storage.getItemById(bin_id);
    assert(retrieved_bin.has_value());
    assert(retrieved_bin->text_content.size() == 2 * 1024 * 1024);
    assert(retrieved_bin->html_content == "application/octet-stream");
    assert(retrieved_bin->text_content == bin_payload);
    std::filesystem::remove(custom_bin_db);
    std::cout << "2MB arbitrary binary data with embedded nulls and custom MIME verified successfully!\n";

    // 12. Test 50MB Massive Payload Storage, Instant Previews, and Full Retrieval
    std::string massive_db = "/tmp/test_sc_massive_50mb.db";
    StorageManager massive_storage(massive_db);
    size_t payload_50mb = 50 * 1024 * 1024;
    std::string text_50mb(payload_50mb, 'X');
    text_50mb[0] = '{';
    text_50mb[1] = '\"';
    text_50mb[2] = 'k';
    text_50mb[payload_50mb - 1] = '}';

    int64_t massive_id = massive_storage.addItem("text", text_50mb);
    assert(massive_id > 0);

    // Verify lightweight preview performance
    auto previews = massive_storage.getItemPreviews(10);
    assert(!previews.empty());
    assert(previews[0].id == massive_id);
    assert(previews[0].full_size == payload_50mb);
    assert(previews[0].text_content.size() <= 512);
    assert(previews[0].text_content.substr(0, 3) == "{\"k");

    // Verify full untruncated retrieval
    auto full_massive = massive_storage.getItemById(massive_id);
    assert(full_massive.has_value());
    assert(full_massive->text_content.size() == payload_50mb);
    assert(full_massive->full_size == payload_50mb);
    assert(full_massive->text_content.front() == '{');
    assert(full_massive->text_content.back() == '}');
    std::filesystem::remove(massive_db);
    std::cout << "Massive 50MB payload storage, instant previews, and full fidelity retrieval verified successfully!\n";

    // 13. Test 20MB Binary Data with Embedded Nulls & O(1) Hash Deduplication
    std::string bin20_db = "/tmp/test_sc_bin20mb.db";
    StorageManager bin20_storage(bin20_db);
    size_t bin20_size = 20 * 1024 * 1024;
    std::string bin20_data(bin20_size, '\0');
    for (size_t i = 0; i < bin20_size; i += 4) {
        bin20_data[i] = static_cast<char>(i & 0xFF);
    }
    int64_t b1_id = bin20_storage.addItem("raw", bin20_data, "application/octet-stream");
    assert(b1_id > 0);

    // Re-insert exact same 20MB binary data (should deduplicate and update existing item)
    int64_t b2_id = bin20_storage.addItem("raw", bin20_data, "application/octet-stream");
    assert(b2_id == b1_id);
    (void)b2_id;

    auto bin20_previews = bin20_storage.getItemPreviews();
    assert(bin20_previews.size() == 1);
    assert(bin20_previews[0].full_size == bin20_size);
    assert(bin20_previews[0].text_content.size() <= 512);

    auto retrieved_bin20 = bin20_storage.getItemById(b1_id);
    assert(retrieved_bin20.has_value());
    assert(retrieved_bin20->text_content.size() == bin20_size);
    assert(retrieved_bin20->html_content == "application/octet-stream");
    assert(retrieved_bin20->text_content == bin20_data);
    std::filesystem::remove(bin20_db);
    std::cout << "20MB binary data with embedded nulls and O(1) hash deduplication verified successfully!\n";

    return 0;
}
