#include "World/Level/LevelGen/Structure/StructureTemplate.h"

#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "StructuresPack.h"

#include <exception>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace {
    struct ArchivedFile {
        const unsigned char *mData = nullptr;
        size_t mSize = 0;
    };

    uint32_t readLittleEndian(const unsigned char *data, size_t bytes) {
        uint32_t value = 0;
        for (size_t i = 0; i < bytes; ++i)
            value |= (uint32_t) data[i] << (8 * i);

        return value;
    }

    // layout: u32 count, then for each file u16 path length, path, u32 size, contents, all little endian
    std::unordered_map<std::string, ArchivedFile> indexArchive() {
        std::unordered_map<std::string, ArchivedFile> index;
        const unsigned char *data = FalconStructureData::kStructuresPack;
        const size_t size = FalconStructureData::kStructuresPackSize;

        size_t offset = 4;
        const uint32_t count = readLittleEndian(data, 4);
        for (uint32_t i = 0; i < count; ++i) {
            const size_t pathSize = readLittleEndian(data + offset, 2);
            const std::string path((const char *) data + offset + 2, pathSize);
            offset += 2 + pathSize;

            const size_t fileSize = readLittleEndian(data + offset, 4);
            offset += 4;
            index[path] = {data + offset, fileSize};
            offset += fileSize;
        }

        if (offset != size)
            LOG_WARN(LogAreaID::Server, "Embedded structure archive has %zu unread bytes", size - offset);

        return index;
    }

    // the archive ships with the server, and the largest pieces hold more blocks than a client may send in a list
    const EncodingSettings &templateEncodingSettings() {
        static const EncodingSettings settings = []() {
            EncodingSettings embedded;
            embedded.mMaxListSize = 1024 * 1024;
            return embedded;
        }();

        return settings;
    }

    const std::unordered_map<std::string, ArchivedFile> &archive() {
        static const std::unordered_map<std::string, ArchivedFile> index = indexArchive();
        return index;
    }
}

std::shared_ptr<const StructureTemplate> StructureTemplate::load(const std::string &path) {
    // chunk workers ask for the same few pieces around every start
    static std::mutex cacheMutex;
    static std::unordered_map<std::string, std::shared_ptr<const StructureTemplate>> cache;
    const std::lock_guard<std::mutex> lock(cacheMutex);
    const auto cached = cache.find(path);
    if (cached != cache.end())
        return cached->second;

    const auto entry = archive().find(path);
    if (entry == archive().end())
        return nullptr;

    const std::string compressed((const char *) entry->second.mData, entry->second.mSize);
    std::string decompressed;
    if (!Gzip::decompress(compressed, decompressed)) {
        LOG_WARN(LogAreaID::Server, "Failed to decompress structure template %s", path.c_str());
        return nullptr;
    }

    ReadOnlyBinaryStream stream(decompressed);
    stream.setEncodingSettings(templateEncodingSettings());
    Tag root;
    try {
        root = NbtIo::readTag(stream, NbtVariant::BigEndian);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Failed to parse structure template %s: %s", path.c_str(), exception.what());
        return nullptr;
    }

    auto result = std::make_shared<StructureTemplate>();
    if (!result->_read(root)) {
        LOG_WARN(LogAreaID::Server, "Structure template %s is incomplete", path.c_str());
        return nullptr;
    }

    cache.emplace(path, result);
    return result;
}

int32_t StructureTemplate::getSizeX() const {
    return mSizeX;
}

int32_t StructureTemplate::getSizeY() const {
    return mSizeY;
}

int32_t StructureTemplate::getSizeZ() const {
    return mSizeZ;
}

const std::vector<StructureTemplate::PaletteEntry> &StructureTemplate::getPalette() const {
    return mPalette;
}

const std::vector<StructureTemplate::BlockEntry> &StructureTemplate::getBlocks() const {
    return mBlocks;
}

bool StructureTemplate::_read(const Tag &root) {
    const Tag *size = root.get("size");
    const Tag *palette = root.get("palette");
    const Tag *blocks = root.get("blocks");
    if (size == nullptr || palette == nullptr || blocks == nullptr || size->getList().size() != 3)
        return false;

    mSizeX = size->getList()[0].asInt();
    mSizeY = size->getList()[1].asInt();
    mSizeZ = size->getList()[2].asInt();

    for (const Tag &state : palette->getList()) {
        const Tag *name = state.get("Name");
        if (name == nullptr)
            return false;

        PaletteEntry entry;
        entry.mName = name->asString();
        if (const Tag *properties = state.get("Properties")) {
            const std::vector<std::string> &keys = properties->getKeys();
            const std::vector<Tag> &values = properties->getValues();
            for (size_t i = 0; i < keys.size(); ++i)
                entry.mProperties[keys[i]] = values[i].asString();
        }

        mPalette.push_back(std::move(entry));
    }

    for (const Tag &block : blocks->getList()) {
        const Tag *position = block.get("pos");
        const Tag *state = block.get("state");
        if (position == nullptr || state == nullptr || position->getList().size() != 3)
            return false;

        BlockEntry entry;
        entry.x = position->getList()[0].asInt();
        entry.y = position->getList()[1].asInt();
        entry.z = position->getList()[2].asInt();
        entry.mPaletteIndex = state->asInt();
        if (entry.mPaletteIndex < 0 || (size_t) entry.mPaletteIndex >= mPalette.size())
            return false;

        if (const Tag *data = block.get("nbt"))
            entry.mData = *data;

        mBlocks.push_back(std::move(entry));
    }

    return true;
}
