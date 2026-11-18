#include "World/Level/Storage/LevelStorage.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a saved structure holds a block index per block: far more than the list limit of network packets
    const int32_t ENTRIES = 64 * 34 * 64;

    Tag largeData() {
        Tag indices = Tag::ofList(Tag::Type::Int);
        for (int32_t i = 0; i < ENTRIES; ++i)
            indices.addToList(Tag::ofInt(i % 7));
        Tag data = Tag::ofCompound();
        data.put("block_indices", indices);
        return data;
    }

    void checkLargeLists(const std::filesystem::path &directory) {
        {
            LevelStorage storage;
            require(storage.open(directory.string(), "world"), "the world did not open");
            require(storage.saveSavedData("structuretemplate_test:large", largeData(), false),
                    "the data was not saved");
        }

        LevelStorage storage;
        require(storage.open(directory.string(), "world"), "the world did not reopen");
        Tag loaded;
        require(storage.loadSavedData("structuretemplate_test:large", loaded, false), "the large data did not load");
        const Tag *indices = loaded.get("block_indices");
        require(indices != nullptr && indices->getList().size() == (size_t) ENTRIES, "the list came back short");

        const auto entries = storage.loadSavedDataWithPrefix("structuretemplate_test:");
        require(entries.size() == 1, "the prefixed load skipped the large data");
    }
}

int main() {
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-saved-data-test-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int status = 0;
    try {
        std::filesystem::create_directories(directory);
        checkLargeLists(directory);
        std::cout << "saved-data: ok\n";
    } catch (const std::exception &error) {
        std::cerr << "saved-data: " << error.what() << '\n';
        status = 1;
    }
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return status;
}
