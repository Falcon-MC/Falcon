#include "World/Level/Block/BlockPaletteRegistry.h"
#include "BlockPaletteNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/NBT/NbtIo.h"
#include "World/Level/LevelGen/Biome/BiomeChunkGenDataRegistry.h"
#include "World/Level/Level.h"
#include "World/Level/Storage/LevelStorage.h"
#include "World/Level/World.h"

#include <chrono>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unordered_map>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    bool samePosition(const Vector3i &left, const Vector3i &right) {
        return left.x == right.x && left.y == right.y && left.z == right.z;
    }

    void checkPalette() {
        std::string bytes;
        require(Gzip::decompress(std::string((const char *) FalconBlockPaletteData::kBlockPaletteNbt,
                                            FalconBlockPaletteData::kBlockPaletteNbtSize), bytes),
                "Could not decompress reference palette");
        ReadOnlyBinaryStream stream(bytes);
        EncodingSettings settings;
        settings.mMaxListSize = 65536;
        stream.setEncodingSettings(settings);
        const Tag reference = NbtIo::readTag(stream, NbtVariant::BigEndian);
        const Tag *blocks = reference.get("blocks");
        require(blocks != nullptr, "Reference palette has no blocks");

        const BlockPaletteRegistry &registry = BlockPaletteRegistry::getInstance();
        require(registry.isLoaded(), "Registry did not load");
        std::unordered_map<std::string, size_t> counts;
        std::vector<std::string> names;
        for (const Tag &entry: blocks->getList()) {
            const Tag *name = entry.get("name");
            const Tag *states = entry.get("states");
            if (name == nullptr || states == nullptr)
                continue;
            const std::string &identifier = name->asString();
            size_t &index = counts[identifier];
            const std::vector<Tag> *permutations = registry.getPermutations(identifier);
            require(permutations != nullptr && index < permutations->size(), "Missing block permutation");
            require((*permutations)[index] == *states, "Block permutation changed");
            if (index++ == 0) {
                names.push_back(identifier);
                const Tag *defaults = registry.getDefaultStates(identifier);
                require(defaults != nullptr && *defaults == *states, "Default block states changed");
                const Tag *networkId = entry.get("network_id");
                if (networkId != nullptr) {
                    int32_t id = 0;
                    require(registry.getDefaultNetworkId(identifier, id) && id == networkId->asInt(),
                            "Default network id changed");
                }
            }
        }
        require(registry.getBlockNames() == names, "Block order or count changed");
        for (const auto &entry: counts)
            require(registry.getPermutations(entry.first)->size() == entry.second, "Extra block permutations");
    }

    void checkDimensions(const std::filesystem::path &directory, int64_t seed) {
        World world(7, "World", 4, seed);
        require(world.open(directory.string(), 1, Level::PacketBroadcaster(), nullptr), "Could not open world");
        for (Level *level: world.getLevels()) {
            require(level->getSeed() == 12345, "Dimension did not retain the saved world seed");
            require(level->getWorldId() == 7, "Dimension lost its world id");
            require(&level->getGameRules() == &world.getOverworld().getGameRules(), "Game rules are not shared");
        }
        world.close();
    }
}

int main() {
    const auto directory = std::filesystem::temp_directory_path()
            / ("falcon-startup-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    try {
        BiomeChunkGenDataRegistry::initialize();
        BlockPaletteRegistry::getInstance().initialize();
        checkPalette();

        Vector3i generatedSpawn;
        {
            Level first("World", 4, 12345);
            require(first.openStorage(directory.string()), "Could not create world");
            generatedSpawn = first.getSpawnPosition();
            require(generatedSpawn.y != Level::UNSET_SPAWN_Y, "Initial spawn was not found");
            require(first.isStandable(generatedSpawn.x, generatedSpawn.y, generatedSpawn.z), "Initial spawn is unsafe");
            first.closeStorage();
        }
        {
            LevelStorage storage;
            require(storage.open(directory.string(), "World"), "Could not inspect world storage");
            Tag data;
            require(storage.readLevelDat(data), "Missing level.dat");
            require(data.getInt("SpawnX") == generatedSpawn.x && data.getInt("SpawnY") == generatedSpawn.y
                    && data.getInt("SpawnZ") == generatedSpawn.z, "Generated spawn was not persisted");
        }
        {
            Level reopened("World", 4, 67890);
            require(reopened.openStorage(directory.string()), "Could not reopen world");
            require(reopened.getSeed() == 12345, "Saved seed was overwritten");
            require(samePosition(reopened.getSpawnPosition(), generatedSpawn), "Default spawn changed on restart");
            require(reopened.getLoadedChunkCount() == 0, "Restart regenerated chunks to search for spawn");
            reopened.setSpawnPosition(Vector3i(17, 90, -19));
            reopened.closeStorage();
        }
        {
            Level reopened("World", 4, 12345);
            require(reopened.openStorage(directory.string()), "Could not reopen custom-spawn world");
            require(samePosition(reopened.getSpawnPosition(), Vector3i(17, 90, -19)), "Custom spawn changed");
            require(reopened.getLoadedChunkCount() == 0, "Custom spawn caused an unused land search");
            reopened.closeStorage();
        }
        checkDimensions(directory, 12345);
        checkDimensions(directory, 67890);
        {
            LevelStorage legacy;
            require(legacy.open(directory.string(), "Legacy"), "Could not create legacy world");
            legacy.writeLevelDat("Legacy", 0, Level::UNSET_SPAWN_Y, 0, 0, 1, 12345, 0, false, false, 0);
        }
        {
            Level legacy("Legacy", 4, 12345);
            require(legacy.openStorage(directory.string()), "Could not migrate legacy spawn");
            require(samePosition(legacy.getSpawnPosition(), generatedSpawn), "Legacy default spawn changed");
            legacy.closeStorage();
        }
        std::filesystem::remove_all(directory);
        std::cout << "Startup persistence checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::fprintf(stderr, "Startup persistence checks failed: %s\n", error.what());
        std::filesystem::remove_all(directory);
        return 1;
    }
}
