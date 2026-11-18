#include "Resources/BrPak.h"

#include <algorithm>
#include <cstring>
#include <set>

#include <zlib.h>

namespace {
    // made by: UNIX, ZIP 6.3; needed: 1.0, stored entries only
    constexpr uint16_t VERSION_MADE_BY = 0x033F;
    constexpr uint16_t VERSION_NEEDED = 10;
    constexpr uint16_t FLAG_UTF8 = 0x0800;
    // a regular file, rw-rw-rw-
    constexpr uint32_t EXTERNAL_ATTRIBUTES = 0100666u << 16;

    void putU16(std::string &out, uint16_t value) {
        out += (char) (value & 0xFF);
        out += (char) (value >> 8);
    }

    void putU32(std::string &out, uint32_t value) {
        putU16(out, (uint16_t) (value & 0xFFFF));
        putU16(out, (uint16_t) (value >> 16));
    }
}

bool BrPak::isValidEntryName(const std::string &name) {
    if (name.empty() || name.front() == '/' || name.find('\\') != std::string::npos
        || name.find(':') != std::string::npos)
        return false;
    size_t start = 0;
    while (start <= name.size()) {
        const size_t end = std::min(name.find('/', start), name.size());
        if (name.compare(start, end - start, "..") == 0)
            return false;
        start = end + 1;
    }
    return true;
}

bool BrPak::write(const std::vector<ZipEntry> &entries, uint16_t dosTime, uint16_t dosDate, std::string &out) {
    // past these the archive would need ZIP64 records
    if (entries.size() > 0xFFFF)
        return false;
    std::set<std::string> names;
    std::vector<uint32_t> crcs;
    std::vector<uint32_t> offsets;
    out.clear();
    for (const ZipEntry &entry: entries) {
        if (!isValidEntryName(entry.mName) || entry.mName.size() > 0xFFFF || !names.insert(entry.mName).second
            || out.size() + entry.mData.size() > 0xFFFFFFFFull - 0xFFFF)
            return false;
        const uint32_t crc = (uint32_t) crc32(0L, (const Bytef *) entry.mData.data(), (uInt) entry.mData.size());
        crcs.push_back(crc);
        offsets.push_back((uint32_t) out.size());

        putU32(out, 0x04034B50);
        putU16(out, VERSION_NEEDED);
        putU16(out, FLAG_UTF8);
        putU16(out, 0);
        putU16(out, dosTime);
        putU16(out, dosDate);
        putU32(out, crc);
        putU32(out, (uint32_t) entry.mData.size());
        putU32(out, (uint32_t) entry.mData.size());
        putU16(out, (uint16_t) entry.mName.size());
        putU16(out, 0);
        out += entry.mName;
        out += entry.mData;
    }

    const uint32_t centralStart = (uint32_t) out.size();
    for (size_t index = 0; index < entries.size(); ++index) {
        const ZipEntry &entry = entries[index];
        putU32(out, 0x02014B50);
        putU16(out, VERSION_MADE_BY);
        putU16(out, VERSION_NEEDED);
        putU16(out, FLAG_UTF8);
        putU16(out, 0);
        putU16(out, dosTime);
        putU16(out, dosDate);
        putU32(out, crcs[index]);
        putU32(out, (uint32_t) entry.mData.size());
        putU32(out, (uint32_t) entry.mData.size());
        putU16(out, (uint16_t) entry.mName.size());
        putU16(out, 0);
        putU16(out, 0);
        putU16(out, 0);
        putU16(out, 0);
        putU32(out, EXTERNAL_ATTRIBUTES);
        putU32(out, offsets[index]);
        out += entry.mName;
    }
    const uint32_t centralSize = (uint32_t) out.size() - centralStart;

    putU32(out, 0x06054B50);
    putU16(out, 0);
    putU16(out, 0);
    putU16(out, (uint16_t) entries.size());
    putU16(out, (uint16_t) entries.size());
    putU32(out, centralSize);
    putU32(out, centralStart);
    putU16(out, (uint16_t) std::strlen(COMMENT));
    out += COMMENT;
    return true;
}
