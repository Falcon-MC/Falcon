#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

struct ZipEntry {
    std::string mName;
    std::string mData;
};

class ZipArchive {
public:
    static constexpr size_t MAX_ENTRY_BYTES = 128 * 1024 * 1024;
    static constexpr size_t MAX_ARCHIVE_BYTES = 512 * 1024 * 1024;

    static bool listEntries(const std::string &zip, std::vector<ZipEntry> &out, size_t budget = MAX_ARCHIVE_BYTES);

    static bool readEntry(std::string_view input, uint16_t method, size_t expected, size_t limit, std::string &out);

    static bool extractToDirectory(const std::string &zip, const std::string &destinationDirectory);

    static bool isZip(const std::string &data);
};

class ZipWriter {
public:
    void addFile(const std::string &name, const std::string &data);

    std::string finish() const;

private:
    struct Record {
        std::string mName;
        std::string mData;
        uint32_t mCrc;
        uint32_t mLocalOffset;
    };

    std::vector<Record> mRecords;
    std::string mBuffer;
};
