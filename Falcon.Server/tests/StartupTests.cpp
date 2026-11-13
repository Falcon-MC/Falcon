#include "World/Level/Block/BlockPaletteRegistry.h"
#include "BlockPaletteNbt.h"
#include "Core/Archive/Gzip.h"
#include "Core/NBT/NbtIo.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
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
                                             FalconBlockPaletteData::kBlockPaletteNbtSize),
                                 bytes),
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

    void checkPreopenedStorage(const std::filesystem::path &directory) {
        Tag saved = Tag::ofCompound();
        saved.put("value", Tag::ofInt(42));
        LevelStorage::preopen(directory.string(), "Preopened");
        {
            LevelStorage adopted;
            require(adopted.open(directory.string(), "Preopened"), "Could not adopt the preopened database");
            require(adopted.saveSavedData("check", saved), "Preopened database is not writable");
        }

        // another level leaves the preopened one alone; discarding it releases the lock
        LevelStorage::preopen(directory.string(), "Preopened");
        {
            LevelStorage other;
            require(other.open(directory.string(), "Other"), "Preopening blocked another level");
        }
        LevelStorage::discardPreopened();
        LevelStorage reopened;
        require(reopened.open(directory.string(), "Preopened"), "Discarded database kept its lock");
        Tag loaded;
        require(reopened.loadSavedData("check", loaded) && loaded == saved, "Preopened database lost its data");
    }

    Tag unknownBlockActor(const char *id, const Vector3i &position) {
        Tag data = Tag::ofCompound();
        data.putString("id", id);
        data.putInt("x", position.x);
        data.putInt("y", position.y);
        data.putInt("z", position.z);
        data.putByte("isMovable", 1);
        return data;
    }

    const Tag *savedAt(const std::vector<Tag> &saved, const Vector3i &position) {
        for (const Tag &data: saved) {
            if (data.getInt("x") == position.x && data.getInt("y") == position.y && data.getInt("z") == position.z)
                return &data;
        }
        return nullptr;
    }

    void checkUnknownBlockActors(const std::filesystem::path &directory) {
        BlockDefinitionRegistry blocks;
        ItemDefinitionRegistry items;
        PacketCodecContext context(blocks, items);
        const Vector3i spawnerPosition(1, 70, 1);
        const Vector3i skullPosition(2, 70, 2);

        Tag spawner = unknownBlockActor("MobSpawner", spawnerPosition);
        spawner.putString("EntityIdentifier", "minecraft:zombie");
        spawner.putShort("Delay", 20);
        Tag skull = unknownBlockActor("Skull", skullPosition);
        skull.putByte("SkullType", 1);
        skull.putFloat("Rotation", 90.0f);

        Level level("BlockActors", 4, 12345);
        require(level.openStorage(directory.string()), "Could not create block actor world");
        level.getChunk(0, 0);
        level.setBlockState(spawnerPosition.x, spawnerPosition.y, spawnerPosition.z,
                            VanillaBlocks::MOB_SPAWNER().toBlockState());
        level.setBlockState(skullPosition.x, skullPosition.y, skullPosition.z,
                            VanillaBlocks::SKELETON_SKULL().toBlockState());
        level.saveBlockEntities(0, 0, {spawner, skull});

        BlockActorStore &store = level.getBlockActors();
        store.loadChunk(0, 0, level.loadBlockEntities(0, 0), context);
        require(store.find(spawnerPosition) == nullptr, "A block actor of no class got one");
        std::vector<Tag> saved = store.saveChunk(0, 0);
        require(saved.size() == 2, "A block actor of no class was dropped on save");
        require(savedAt(saved, spawnerPosition) != nullptr && *savedAt(saved, spawnerPosition) == spawner,
                "The spawner was not saved back untouched");
        require(savedAt(saved, skullPosition) != nullptr && *savedAt(saved, skullPosition) == skull,
                "The skull was not saved back untouched");
        require(!store.encodeChunkNetwork(0, 0).empty(), "A block actor of no class is not sent");

        level.saveBlockEntities(0, 0, saved);
        store.unloadChunk(0, 0);
        store.loadChunk(0, 0, level.loadBlockEntities(0, 0), context);
        require(store.saveChunk(0, 0).size() == 2, "A block actor of no class did not survive a reload");

        level.setBlockState(skullPosition.x, skullPosition.y, skullPosition.z, VanillaBlocks::AIR().toBlockState());
        saved = store.saveChunk(0, 0);
        require(saved.size() == 1 && savedAt(saved, spawnerPosition) != nullptr,
                "A replaced block kept its unknown block actor");
        level.closeStorage();
    }

    void checkBlockActorChunks() {
        Level level("ChunkIndex", 4, 12345);
        BlockActorStore &store = level.getBlockActors();
        const Vector3i negative(-17, 70, -1);
        const Vector3i positive(33, -40, 47);
        store.getOrCreate<ChestBlockActor>(negative);
        store.getOrCreate<ChestBlockActor>(positive);
        require(store.saveChunk(-2, -1).size() == 1 && store.saveChunk(2, 2).size() == 1,
                "A block actor was not found in its chunk");
        require(store.saveChunk(-1, -1).empty() && store.saveChunk(0, 0).empty(),
                "A block actor was found in another chunk");
        require(!store.encodeSubChunkNetwork(2, -3, 2).empty() && store.encodeSubChunkNetwork(2, 4, 2).empty(),
                "A block actor was sent with the wrong sub-chunk");
        require(store.take(negative) != nullptr && store.saveChunk(-2, -1).empty(),
                "A taken block actor stayed in its chunk");
        store.unloadChunk(2, 2);
        require(store.find(positive) == nullptr && store.saveChunk(2, 2).empty(),
                "Unloading a chunk kept its block actors");
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
                           / ("falcon-startup-test-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
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
                            && data.getInt("SpawnZ") == generatedSpawn.z,
                    "Generated spawn was not persisted");
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
        checkPreopenedStorage(directory);
        checkUnknownBlockActors(directory);
        checkBlockActorChunks();
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
