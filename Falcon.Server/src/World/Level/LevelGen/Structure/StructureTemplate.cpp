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
    const bool bedrockMobPiece = path.starts_with("bastion/mobs/") && path.ends_with(".mcstructure");
    std::string payload;
    if (bedrockMobPiece) {
        payload = compressed;
    } else if (!Gzip::decompress(compressed, payload)) {
        LOG_WARN(LogAreaID::Server, "Failed to decompress structure template %s", path.c_str());
        return nullptr;
    }

    ReadOnlyBinaryStream stream(payload);
    stream.setEncodingSettings(templateEncodingSettings());
    Tag root;
    try {
        root = NbtIo::readTag(stream, bedrockMobPiece ? NbtVariant::LittleEndian : NbtVariant::BigEndian);
    } catch (const std::exception &exception) {
        LOG_WARN(LogAreaID::Server, "Failed to parse structure template %s: %s", path.c_str(), exception.what());
        return nullptr;
    }

    auto result = std::make_shared<StructureTemplate>();
    if (!(bedrockMobPiece ? result->_readBedrockMobPiece(root) : result->_read(root))) {
        LOG_WARN(LogAreaID::Server, "Structure template %s is incomplete", path.c_str());
        return nullptr;
    }

    cache.emplace(path, result);
    return result;
}

void StructureTemplate::rotate(int32_t quarterTurns, int32_t &x, int32_t &z) {
    for (int32_t i = 0; i < quarterTurns; ++i) {
        const int32_t oldX = x;
        x = -z;
        z = oldX;
    }
}

void StructureTemplate::mirror(int32_t mirror, int32_t &x, int32_t &z) {
    if (mirror == 1)
        z = -z;
    else if (mirror == 2)
        x = -x;
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

    for (const Tag &state: palette->getList()) {
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

    for (const Tag &block: blocks->getList()) {
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

bool StructureTemplate::_readBedrockMobPiece(const Tag &root) {
    const Tag *size = root.get("size");
    const Tag *structure = root.get("structure");
    if (root.getInt("format_version") != 1 || size == nullptr || !size->isList() || structure == nullptr)
        return false;

    const auto &dimensions = size->getList();
    if (dimensions.size() != 3 || dimensions[0].asInt() != 1 || dimensions[2].asInt() != 1)
        return false;

    const Tag *layers = structure->get("block_indices");
    const Tag *palettes = structure->get("palette");
    const Tag *palette = palettes != nullptr ? palettes->get("default") : nullptr;
    const Tag *states = palette != nullptr ? palette->get("block_palette") : nullptr;
    const Tag *positions = palette != nullptr ? palette->get("block_position_data") : nullptr;
    if (layers == nullptr || !layers->isList() || states == nullptr || !states->isList() || positions == nullptr)
        return false;

    const int32_t height = dimensions[1].asInt();
    const auto &indices = layers->getList();
    if (height <= 0 || indices.size() != 2 || !indices[0].isList() || !indices[1].isList())
        return false;

    const auto &primary = indices[0].getList();
    const auto &secondary = indices[1].getList();
    if (primary.size() != (size_t) height || secondary.size() != primary.size())
        return false;

    for (const Tag &state: states->getList()) {
        PaletteEntry entry;
        entry.mName = state.getString("name");
        if (entry.mName == "minecraft:jigsaw") {
            const Tag *properties = state.get("states");
            if (properties == nullptr || properties->getInt("facing_direction", -1) != 0
                || properties->getInt("rotation", -1) != 3)
                return false;
            entry.mProperties["orientation"] = "down_east";
        } else if (entry.mName != "minecraft:air") {
            return false;
        }
        mPalette.push_back(std::move(entry));
    }

    int32_t connectors = 0;
    for (int32_t y = 0; y < height; ++y) {
        const int32_t index = primary[(size_t) y].asInt();
        if (secondary[(size_t) y].asInt() != -1 || index < -1 || index >= (int32_t) mPalette.size())
            return false;
        if (index == -1)
            continue;

        BlockEntry block;
        block.y = y;
        block.mPaletteIndex = index;
        if (const Tag *position = positions->get(std::to_string(y))) {
            if (const Tag *data = position->get("block_entity_data"))
                block.mData = *data;
        }
        if (mPalette[(size_t) index].mName == "minecraft:jigsaw") {
            if (++connectors != 1 || block.mData.getString("name").empty()
                || block.mData.getString("target_pool") != "minecraft:empty")
                return false;
        }
        mBlocks.push_back(std::move(block));
    }
    if (connectors != 1)
        return false;

    mSizeX = 1;
    mSizeY = height;
    mSizeZ = 1;
    return true;
}
