#include "World/Level/Storage/LegacyWorldImporter.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Storage/BlockStateUpgrades.h"
#include "World/Level/Storage/LevelStorage.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iterator>
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

    storage.close();
    if (skipped != 0)
        LOG_WARN(LogAreaID::Server, "%d chunks of %s were cut short and were left out", skipped, levelName.c_str());
    LOG_INFO(LogAreaID::Server, "Imported %d chunks of %s", imported, levelName.c_str());
    return true;
}
