#include "Resources/BrArchive.h"

#include <algorithm>

namespace {
    constexpr size_t HEADER_SIZE = 16;
    constexpr size_t ENTRY_SIZE = 256;
    constexpr size_t NAME_FIELD = 247;

    void putU32(std::string &out, uint32_t value) {
        for (int shift = 0; shift < 32; shift += 8)
            out += (char) ((value >> shift) & 0xFF);
    }

    uint32_t getU32(const std::string &in, size_t offset) {
        uint32_t value = 0;
        for (int index = 3; index >= 0; --index)
            value = (value << 8) | (unsigned char) in[offset + (size_t) index];
        return value;
    }
}

bool BrArchive::write(std::vector<Entry> entries, std::string &out) {
    std::sort(entries.begin(), entries.end(),
              [](const Entry &left, const Entry &right) { return left.mName < right.mName; });

    out.clear();
    for (int shift = 0; shift < 64; shift += 8)
        out += (char) ((MAGIC >> shift) & 0xFF);
    putU32(out, (uint32_t) entries.size());
    putU32(out, VERSION);

    uint32_t offset = 0;
    for (const Entry &entry: entries) {
        if (entry.mName.size() > MAX_NAME_LENGTH)
            return false;
        out += (char) entry.mName.size();
        std::string name = entry.mName;
        name.resize(NAME_FIELD, '\0');
        out += name;
        // an empty entry stands for a file kept beside the archive and points at the start
        putU32(out, entry.mData.empty() ? 0 : offset);
        putU32(out, (uint32_t) entry.mData.size());
        offset += (uint32_t) entry.mData.size();
    }
    for (const Entry &entry: entries)
        out += entry.mData;
    return true;
}

bool BrArchive::read(const std::string &archive, std::vector<Entry> &out) {
    out.clear();
    if (archive.size() < HEADER_SIZE)
        return false;
    uint64_t magic = 0;
    for (int index = 7; index >= 0; --index)
        magic = (magic << 8) | (unsigned char) archive[(size_t) index];
    if (magic != MAGIC || getU32(archive, 12) != VERSION)
        return false;

    const uint32_t count = getU32(archive, 8);
    if ((archive.size() - HEADER_SIZE) / ENTRY_SIZE < count)
        return false;
    const size_t dataStart = HEADER_SIZE + (size_t) count * ENTRY_SIZE;
    for (uint32_t index = 0; index < count; ++index) {
        const size_t entryOffset = HEADER_SIZE + (size_t) index * ENTRY_SIZE;
        const size_t nameLength = (unsigned char) archive[entryOffset];
        const uint32_t offset = getU32(archive, entryOffset + 1 + NAME_FIELD);
        const uint32_t size = getU32(archive, entryOffset + 1 + NAME_FIELD + 4);
        if (nameLength > NAME_FIELD || dataStart + offset > archive.size()
            || archive.size() - dataStart - offset < size)
            return false;
        Entry entry;
        entry.mName = archive.substr(entryOffset + 1, nameLength);
        entry.mData = archive.substr(dataStart + offset, size);
        out.push_back(std::move(entry));
    }
    return true;
}
