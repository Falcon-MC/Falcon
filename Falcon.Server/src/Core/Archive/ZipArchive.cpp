#include "Core/Archive/ZipArchive.h"

#include <filesystem>
#include <fstream>

#include <zlib.h>

namespace {
    uint16_t readU16(const std::string &data, size_t offset) {
        return (uint16_t) ((unsigned char) data[offset] | ((unsigned char) data[offset + 1] << 8));
    }

    uint32_t readU32(const std::string &data, size_t offset) {
        return (uint32_t) ((unsigned char) data[offset] | ((unsigned char) data[offset + 1] << 8)
                           | ((unsigned char) data[offset + 2] << 16) | ((unsigned char) data[offset + 3] << 24));
    }

    void writeU16(std::string &out, uint16_t value) {
        out.push_back((char) (value & 0xff));
        out.push_back((char) ((value >> 8) & 0xff));
    }

    void writeU32(std::string &out, uint32_t value) {
        out.push_back((char) (value & 0xff));
        out.push_back((char) ((value >> 8) & 0xff));
        out.push_back((char) ((value >> 16) & 0xff));
        out.push_back((char) ((value >> 24) & 0xff));
    }

    bool inflateRaw(const std::string &input, size_t uncompressedSize, std::string &out) {
        z_stream stream{};
        if (inflateInit2(&stream, -MAX_WBITS) != Z_OK)
            return false;

        out.resize(uncompressedSize);
        stream.next_in = (Bytef *) input.data();
        stream.avail_in = (uInt) input.size();
        stream.next_out = (Bytef *) &out[0];
        stream.avail_out = (uInt) uncompressedSize;

        const int result = inflate(&stream, Z_FINISH);
        inflateEnd(&stream);
        return result == Z_STREAM_END;
    }
}

bool ZipArchive::isZip(const std::string &data) {
    return data.size() >= 4 && readU32(data, 0) == 0x04034b50;
}

bool ZipArchive::listEntries(const std::string &zip, std::vector<ZipEntry> &out) {
    if (zip.size() < 22)
        return false;

    size_t eocd = std::string::npos;
    for (size_t i = zip.size() - 22; i + 4 <= zip.size(); i--) {
        if (readU32(zip, i) == 0x06054b50) {
            eocd = i;
            break;
        }
        if (i == 0)
            break;
    }

    if (eocd == std::string::npos)
        return false;

    const uint16_t entryCount = readU16(zip, eocd + 10);
    // the offsets come from the archive, so the arithmetic is done in 64 bits: a 32-bit sum could wrap past
    // the size check and lead readU32 to read out of bounds.
    size_t offset = readU32(zip, eocd + 16);

    for (uint16_t entry = 0; entry < entryCount; entry++) {
        if (offset + 46 > zip.size() || readU32(zip, offset) != 0x02014b50)
            return false;

        const uint16_t method = readU16(zip, offset + 10);
        const uint32_t compressedSize = readU32(zip, offset + 20);
        const uint32_t uncompressedSize = readU32(zip, offset + 24);
        const uint16_t nameLength = readU16(zip, offset + 28);
        const uint16_t extraLength = readU16(zip, offset + 30);
        const uint16_t commentLength = readU16(zip, offset + 32);
        const size_t localOffset = readU32(zip, offset + 42);

        const std::string name = zip.substr(offset + 46, nameLength);
        offset += (size_t) 46 + nameLength + extraLength + commentLength;

        if (!name.empty() && name.back() == '/')
            continue;

        if (localOffset + 30 > zip.size() || readU32(zip, localOffset) != 0x04034b50)
            return false;

        const uint16_t localNameLength = readU16(zip, localOffset + 26);
        const uint16_t localExtraLength = readU16(zip, localOffset + 28);
        const size_t dataStart = localOffset + 30 + (size_t) localNameLength + localExtraLength;

        if (dataStart + compressedSize > zip.size())
            return false;

        const std::string raw = zip.substr(dataStart, compressedSize);

        ZipEntry result;
        result.mName = name;

        if (method == 0) {
            result.mData = raw;
        } else if (method == 8) {
            if (!inflateRaw(raw, uncompressedSize, result.mData))
                continue;
        } else {
            continue;
        }

        out.push_back(std::move(result));
    }

    return true;
}

bool ZipArchive::extractToDirectory(const std::string &zip, const std::string &destinationDirectory) {
    std::vector<ZipEntry> entries;
    if (!listEntries(zip, entries))
        return false;

    std::error_code error;
    const std::filesystem::path root(destinationDirectory);
    std::filesystem::create_directories(root, error);
    const std::filesystem::path canonicalRoot = std::filesystem::weakly_canonical(root, error);
    const std::filesystem::path safeRoot = canonicalRoot.empty() ? root : canonicalRoot;

    for (const ZipEntry &entry: entries) {
        // the archive is attacker-controlled, so an entry name must not escape the destination directory.
        // reject separators the host may not treat as such, absolute names and any traversal, then confirm
        // the resolved path stays under the root (zip slip).
        if (entry.mName.empty() || entry.mName.find('\\') != std::string::npos
            || entry.mName.find('\0') != std::string::npos) {
            return false;
        }

        const std::filesystem::path relative(entry.mName);
        if (relative.is_absolute() || relative.has_root_name()) {
            return false;
        }

        bool traverses = false;
        for (const std::filesystem::path &part: relative) {
            if (part == "..") {
                traverses = true;
                break;
            }
        }
        if (traverses) {
            return false;
        }

        const std::filesystem::path target = root / relative;
        const std::filesystem::path canonicalTarget = std::filesystem::weakly_canonical(target, error);
        const std::string targetString = canonicalTarget.empty() ? target.string() : canonicalTarget.string();
        const std::string rootString = safeRoot.string();
        if (targetString.compare(0, rootString.size(), rootString) != 0
            || (targetString.size() > rootString.size()
                && targetString[rootString.size()] != std::filesystem::path::preferred_separator)) {
            return false;
        }

        if (target.has_parent_path())
            std::filesystem::create_directories(target.parent_path(), error);

        std::ofstream file(target, std::ios::binary);
        if (!file.is_open())
            return false;

        file.write(entry.mData.data(), (std::streamsize) entry.mData.size());
    }

    return true;
}

void ZipWriter::addFile(const std::string &name, const std::string &data) {
    Record record;
    record.mName = name;
    record.mData = data;
    record.mCrc = (uint32_t) crc32(0L, (const Bytef *) data.data(), (uInt) data.size());
    record.mLocalOffset = 0;
    mRecords.push_back(std::move(record));
}

std::string ZipWriter::finish() const {
    std::string out;
    std::vector<uint32_t> offsets;
    offsets.reserve(mRecords.size());

    for (const Record &record: mRecords) {
        offsets.push_back((uint32_t) out.size());

        writeU32(out, 0x04034b50);
        writeU16(out, 20);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU32(out, record.mCrc);
        writeU32(out, (uint32_t) record.mData.size());
        writeU32(out, (uint32_t) record.mData.size());
        writeU16(out, (uint16_t) record.mName.size());
        writeU16(out, 0);
        out.append(record.mName);
        out.append(record.mData);
    }

    const uint32_t centralStart = (uint32_t) out.size();

    for (size_t i = 0; i < mRecords.size(); ++i) {
        const Record &record = mRecords[i];

        writeU32(out, 0x02014b50);
        writeU16(out, 20);
        writeU16(out, 20);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU32(out, record.mCrc);
        writeU32(out, (uint32_t) record.mData.size());
        writeU32(out, (uint32_t) record.mData.size());
        writeU16(out, (uint16_t) record.mName.size());
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU16(out, 0);
        writeU32(out, 0);
        writeU32(out, offsets[i]);
        out.append(record.mName);
    }

    const uint32_t centralSize = (uint32_t) out.size() - centralStart;

    writeU32(out, 0x06054b50);
    writeU16(out, 0);
    writeU16(out, 0);
    writeU16(out, (uint16_t) mRecords.size());
    writeU16(out, (uint16_t) mRecords.size());
    writeU32(out, centralSize);
    writeU32(out, centralStart);
    writeU16(out, 0);

    return out;
}
