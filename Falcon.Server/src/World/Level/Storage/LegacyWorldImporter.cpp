#include "World/Level/Storage/LegacyWorldImporter.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Storage/BlockStateUpgrades.h"
#include "World/Level/Storage/LevelStorage.h"

#include "LegacyBlockTables.h"

#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <unordered_map>
#include <vector>

namespace {
    // sub_14D1F0F10: chunk coordinates 0 to 15 on both axes, 16 by 128 by 16 blocks each
    const int32_t CHUNKS_PER_SIDE = 16;
    const int32_t HEIGHT = 128;
    const size_t BLOCK_COUNT = 16 * HEIGHT * 16;
    // sub_14109A450: a 4096 byte header of 32 by 32 entries, sector count in the low byte, offset in sectors
    const size_t SECTOR_SIZE = 4096;
    const int32_t TABLE_SIDE = 32;

    std::string readFile(const std::filesystem::path &path) {
        std::ifstream in(path, std::ios::binary);
        if (!in.is_open())
            return std::string();
        return std::string((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    }

    uint32_t readLInt(const std::string &bytes, size_t offset) {
        return (uint32_t) (uint8_t) bytes[offset] | ((uint32_t) (uint8_t) bytes[offset + 1] << 8)
               | ((uint32_t) (uint8_t) bytes[offset + 2] << 16) | ((uint32_t) (uint8_t) bytes[offset + 3] << 24);
    }

    // the ids and metas of one chunk, empty when the chunk was never saved
    std::string chunkPayload(const std::string &file, int32_t chunkX, int32_t chunkZ) {
        const uint32_t entry = readLInt(file, (size_t) (chunkX + chunkZ * TABLE_SIDE) * 4);
        if (entry == 0)
            return std::string();

        const size_t offset = (size_t) (entry >> 8) * SECTOR_SIZE;
        if (offset + 4 > file.size())
            return std::string();

        // the length counts its own four bytes
        const uint32_t length = readLInt(file, offset);
        if (length <= 4 || offset + length > file.size())
            return std::string();
        return file.substr(offset + 4, length - 4);
    }

    // name to numeric id in the embedded table, turned around
    std::unordered_map<int32_t, std::string> idsToNames(const char *bytes, size_t size) {
        std::unordered_map<int32_t, std::string> names;
        const std::unique_ptr<json::Value> root = json::parse(std::string(bytes, size));
        if (root == nullptr || !root->isObject())
            return names;
        for (const auto &entry: root->mObject) {
            if (entry.second != nullptr && entry.second->isNumber())
                names.emplace((int32_t) entry.second->mNumber, entry.first);
        }
        return names;
    }

    const std::unordered_map<int32_t, std::string> &itemNames() {
        static const auto names = idsToNames(FalconLegacyBlockData::kItemIds, FalconLegacyBlockData::kItemIdsSize);
        return names;
    }

    const std::unordered_map<int32_t, std::string> &entityNames() {
        static const auto names = idsToNames(FalconLegacyBlockData::kEntityIds,
                                             FalconLegacyBlockData::kEntityIdsSize);
        return names;
    }

    bool isNumber(const Tag &tag) {
        return tag.getType() == Tag::Type::Byte || tag.getType() == Tag::Type::Short
               || tag.getType() == Tag::Type::Int;
    }

    int32_t numberOf(const Tag &tag) {
        if (tag.getType() == Tag::Type::Byte)
            return tag.asByte();
        if (tag.getType() == Tag::Type::Short)
            return tag.asShort();
        return tag.asInt();
    }

    // an old item is a numeric id with Count and Damage: it gets the name saved items have now, the loader
    // then follows renames and splits by meta
    Tag upgradeItems(const Tag &tag);

    Tag upgradeList(const Tag &list) {
        Tag upgraded = Tag::ofList(list.getListType());
        for (const Tag &entry: list.getList())
            upgraded.addToList(upgradeItems(entry));
        return upgraded;
    }

    Tag upgradeItems(const Tag &tag) {
        if (tag.isList())
            return upgradeList(tag);
        if (!tag.isCompound())
            return tag;

        Tag upgraded = Tag::ofCompound();
        const std::vector<std::string> &keys = tag.getKeys();
        const std::vector<Tag> &values = tag.getValues();
        for (size_t i = 0; i < keys.size(); i++)
            upgraded.put(keys[i], upgradeItems(values[i]));

        const Tag *id = tag.get("id");
        if (id != nullptr && isNumber(*id) && tag.contains("Count") && !tag.contains("Name")) {
            const auto name = itemNames().find(numberOf(*id));
            upgraded.remove("id");
            upgraded.putString("Name", name != itemNames().end() ? name->second : "minecraft:air");
        }
        return upgraded;
    }

    // a sign kept one string per line, and had no back
    void upgradeSign(Tag &sign) {
        if (sign.contains("FrontText") || !sign.contains("Text1"))
            return;
        std::string text;
        for (const char *line: {"Text1", "Text2", "Text3", "Text4"}) {
            if (line[4] != '1')
                text += "\n";
            text += sign.getString(line);
            sign.remove(line);
        }
        while (!text.empty() && text.back() == '\n')
            text.pop_back();
        Tag front = SignBlockActor::emptyText();
        front.putString("Text", text);
        sign.put("FrontText", std::move(front));
        sign.put("BackText", SignBlockActor::emptyText());
    }

    // sub_14D1EE9A0: a small header, then the length of a little-endian compound holding Entities and TileEntities
    bool readEntities(const std::string &file, Tag &root) {
        for (const size_t header: {(size_t) 12, (size_t) 16}) {
            if (file.size() < header)
                continue;
            const uint32_t length = readLInt(file, header - 4);
            if (length == 0 || header + length > file.size() || file[header] != 10)
                continue;
            try {
                ReadOnlyBinaryStream stream(file.substr(header, length));
                root = NbtIo::readTag(stream, NbtVariant::LittleEndian);
                if (root.isCompound())
                    return true;
            } catch (const std::exception &) {
            }
        }
        return false;
    }

    using ChunkKey = std::pair<int32_t, int32_t>;

    // entities by the chunk their position falls in, like sub_14D1EE9A0 sorts them
    int32_t importEntities(const std::filesystem::path &root, LevelStorage &storage, const std::string &levelName) {
        const std::string file = readFile(root / "entities.dat");
        if (file.empty())
            return 0;

        Tag data;
        if (!readEntities(file, data)) {
            LOG_WARN(LogAreaID::Server, "entities.dat of %s could not be read and was left out", levelName.c_str());
            return 0;
        }

        std::map<ChunkKey, std::vector<Tag>> actors;
        std::map<ChunkKey, std::vector<Tag>> blockActors;
        int32_t imported = 0;
        // live ids carry the world's start count in their high half: one it never reaches keeps these apart
        int64_t nextUniqueId = (int64_t) 0x7fffffff << 32;

        const Tag *entities = data.get("Entities");
        if (entities != nullptr && entities->isList()) {
            for (const Tag &entity: entities->getList()) {
                if (!entity.isCompound())
                    continue;
                Tag upgraded = upgradeItems(entity);
                const Tag *id = entity.get("id");
                if (id != nullptr && isNumber(*id)) {
                    // the low byte of the actor type
                    const auto name = entityNames().find(numberOf(*id) & 0xff);
                    if (name == entityNames().end())
                        continue;
                    upgraded.remove("id");
                    upgraded.putString("identifier", name->second);
                }
                if (!upgraded.contains("identifier"))
                    continue;
                if (!upgraded.contains("UniqueID", Tag::Type::Long))
                    upgraded.putLong("UniqueID", ++nextUniqueId);

                const Tag *position = entity.get("Pos");
                if (position == nullptr || !position->isList() || position->getList().size() < 3
                    || position->getListType() != Tag::Type::Float)
                    continue;
                const int32_t chunkX = (int32_t) std::floor(position->getList()[0].asFloat()) >> 4;
                const int32_t chunkZ = (int32_t) std::floor(position->getList()[2].asFloat()) >> 4;
                actors[ChunkKey(chunkX, chunkZ)].push_back(std::move(upgraded));
            }
        }

        const Tag *tiles = data.get("TileEntities");
        if (tiles != nullptr && tiles->isList()) {
            for (const Tag &tile: tiles->getList()) {
                if (!tile.isCompound() || !tile.contains("x") || !tile.contains("z"))
                    continue;
                Tag upgraded = upgradeItems(tile);
                if (upgraded.getString("id") == "Sign")
                    upgradeSign(upgraded);
                const ChunkKey key(tile.getInt("x") >> 4, tile.getInt("z") >> 4);
                blockActors[key].push_back(std::move(upgraded));
            }
        }

        for (const auto &chunk: actors) {
            if (storage.saveEntities(chunk.first.first, chunk.first.second, chunk.second))
                imported += (int32_t) chunk.second.size();
        }
        for (const auto &chunk: blockActors) {
            if (storage.saveBlockEntities(chunk.first.first, chunk.first.second, chunk.second))
                imported += (int32_t) chunk.second.size();
        }
        return imported;
    }

    class StateCache {
    public:
        const BlockState &get(uint8_t id, uint8_t meta) {
            const uint16_t key = (uint16_t) ((id << 4) | meta);
            auto found = mStates.find(key);
            if (found != mStates.end())
                return *found->second;

            const BlockStateData upgraded = BlockStateUpgrades::upgrade(BlockStateUpgrades::fromLegacy(id, meta));
            std::unique_ptr<BlockState> state(new BlockState(upgraded.getName(), upgraded.getStates()));
            return *mStates.emplace(key, std::move(state)).first->second;
        }

    private:
        std::unordered_map<uint16_t, std::unique_ptr<BlockState>> mStates;
    };
}

bool LegacyWorldImporter::isLegacyWorld(const std::string &worldsDirectory, const std::string &levelName) {
    const std::filesystem::path root = std::filesystem::path(worldsDirectory) / levelName;
    std::error_code error;
    return std::filesystem::exists(root / "chunks.dat", error)
           && !std::filesystem::exists(root / "db" / "CURRENT", error);
}

bool LegacyWorldImporter::importIfNeeded(const std::string &worldsDirectory, const std::string &levelName) {
    if (!isLegacyWorld(worldsDirectory, levelName))
        return false;

    const std::filesystem::path root = std::filesystem::path(worldsDirectory) / levelName;
    const std::string chunks = readFile(root / "chunks.dat");
    if (chunks.size() < SECTOR_SIZE) {
        LOG_WARN(LogAreaID::Server, "chunks.dat of %s is too short to be a world", levelName.c_str());
        return false;
    }

    LevelStorage storage;
    if (!storage.open(worldsDirectory, levelName, 0))
        return false;

    LOG_INFO(LogAreaID::Server, "Importing %s, a world from before 0.9", levelName.c_str());

    StateCache states;
    int32_t imported = 0;
    int32_t skipped = 0;
    for (int32_t chunkZ = 0; chunkZ < CHUNKS_PER_SIDE; chunkZ++) {
        for (int32_t chunkX = 0; chunkX < CHUNKS_PER_SIDE; chunkX++) {
            const std::string payload = chunkPayload(chunks, chunkX, chunkZ);
            if (payload.empty())
                continue;
            // sub_14D1F0F10: ids, then metas; the light that follows from storage version 2 is recomputed
            if (payload.size() < BLOCK_COUNT + BLOCK_COUNT / 2) {
                skipped++;
                continue;
            }

            LevelChunk chunk(chunkX, chunkZ);
            const char *ids = payload.data();
            const char *metas = payload.data() + BLOCK_COUNT;
            // sub_145CF6D30: x, then z, then y fastest
            for (int32_t x = 0; x < 16; x++) {
                for (int32_t z = 0; z < 16; z++) {
                    for (int32_t y = 0; y < HEIGHT; y++) {
                        const size_t index = ((size_t) x * 16 + (size_t) z) * HEIGHT + (size_t) y;
                        const uint8_t id = (uint8_t) ids[index];
                        if (id == 0)
                            continue;
                        // sub_1432C9BA0: low nibble for even indices
                        const uint8_t packed = (uint8_t) metas[index >> 1];
                        const uint8_t meta = (index & 1) == 0 ? (packed & 0x0f) : (packed >> 4);
                        chunk.setBlock(x, y, z, states.get(id, meta));
                    }
                }
            }

            // the terrain is finished: the generator must not decorate it again
            chunk.setPopulated(true);
            if (storage.saveChunk(chunk))
                imported++;
        }
    }

    const int32_t entities = importEntities(root, storage, levelName);
    storage.close();
    if (skipped != 0)
        LOG_WARN(LogAreaID::Server, "%d chunks of %s were cut short and were left out", skipped, levelName.c_str());
    LOG_INFO(LogAreaID::Server, "Imported %d chunks and %d entities and block entities of %s", imported, entities,
             levelName.c_str());
    return true;
}
