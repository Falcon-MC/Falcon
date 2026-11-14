#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Item/ItemClassRegistry.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/MobSpawnerBlockActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *const BLAZE = "minecraft:blaze";

    struct Arena {
        ServerNetworkHandler mOwner{"Mob spawner", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -4; x <= 4; x++) {
                for (int32_t z = -4; z <= 4; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }

        // a stone floor under an open room around the spawner
        MobSpawnerBlockActor &spawner(const Vector3i &position, const char *identifier) {
            for (int32_t x = position.x - 10; x <= position.x + 10; ++x) {
                for (int32_t z = position.z - 10; z <= position.z + 10; ++z) {
                    mLevel->setBlockState(x, position.y - 2, z, BlockState("minecraft:stone"));
                    for (int32_t y = position.y - 1; y <= position.y + 3; ++y)
                        mLevel->setBlockState(x, y, z, BlockState("minecraft:air"));
                }
            }
            mLevel->setBlockState(position.x, position.y, position.z, BlockState("minecraft:mob_spawner"));
            MobSpawnerBlockActor &spawner = mLevel->getBlockActors().getOrCreate<MobSpawnerBlockActor>(position);
            if (identifier != nullptr)
                spawner.setEntityIdentifier(identifier);
            spawner.seedRandom(1234);
            return spawner;
        }

        std::vector<ServerActor *> all(const char *identifier) {
            std::vector<ServerActor *> found;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead() && entry.second->isIn(*mLevel))
                    found.push_back(entry.second.get());
            }
            return found;
        }

        void clear() {
            std::vector<int64_t> ids;
            for (auto &entry: mOwner.getActors())
                ids.push_back(entry.first);
            for (const int64_t id: ids)
                mOwner.removeActor(id);
        }
    };

    void defaultsAndSave(Arena &arena) {
        MobSpawnerBlockActor spawner;
        require(spawner.getDelay() == 20 && spawner.getMinSpawnDelay() == 200 && spawner.getMaxSpawnDelay() == 800,
                "the spawner delays are not the game's defaults");
        require(spawner.getSpawnCount() == 4 && spawner.getMaxNearbyEntities() == 6
                        && spawner.getRequiredPlayerRange() == 16 && spawner.getSpawnRange() == 4,
                "the spawner counts are not the game's defaults");
        require(!spawner.hasEntity(), "an empty spawner has a mob");

        spawner.setEntityIdentifier(BLAZE);
        spawner.setDelay(37);
        const Tag saved = spawner.saveNbt();
        require(saved.getString("EntityIdentifier") == BLAZE, "the mob was not saved");
        require(saved.contains("Delay", Tag::Type::Short) && saved.getShort("Delay") == 37, "the delay is not a short");
        require(saved.contains("DisplayEntityHeight", Tag::Type::Float), "the display height is not saved");

        Tag changed = saved;
        changed.putShort("MinSpawnDelay", 10);
        changed.putShort("MaxSpawnDelay", 20);
        changed.putShort("SpawnCount", 2);
        changed.putShort("SpawnRange", 3);
        Tag potentials = Tag::ofList(Tag::Type::Compound);
        Tag entry = Tag::ofCompound();
        entry.putInt("Weight", 1);
        entry.putString("TypeId", "minecraft:zombie");
        potentials.addToList(entry);
        changed.put("SpawnPotentials", potentials);

        MobSpawnerBlockActor loaded;
        loaded.loadNbt(changed, arena.mOwner.getCodecContext());
        require(loaded.hasEntity() && loaded.getEntityIdentifier() == BLAZE && loaded.getDelay() == 37,
                "the saved spawner did not load back");
        require(loaded.getMinSpawnDelay() == 10 && loaded.getMaxSpawnDelay() == 20 && loaded.getSpawnCount() == 2
                        && loaded.getSpawnRange() == 3,
                "the delays and counts were not read");
        require(loaded.getSpawnPotentials().size() == 1
                        && loaded.getSpawnPotentials()[0].mIdentifier == "minecraft:zombie",
                "the spawn potentials were not read");

        // the delay pair is only read as shorts
        Tag ints = saved;
        ints.putInt("MinSpawnDelay", 5);
        MobSpawnerBlockActor ignored;
        ignored.loadNbt(ints, arena.mOwner.getCodecContext());
        require(ignored.getMinSpawnDelay() == 200 && ignored.getMaxSpawnDelay() == 800,
                "delays saved as ints were read");
    }

    void spawnsNearPlayer(Arena &arena) {
        arena.clear();
        const Vector3i position(0, 80, 0);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);

        // nobody around: the delay does not run down
        BlockActorStore &store = arena.mLevel->getBlockActors();
        store.tick(arena.mOwner);
        require(spawner.getDelay() == 20, "the spawner counted down with no player in range");

        ServerPlayer &player = arena.player(9501, Vector3f(30.5f, 79.0f, 0.5f));
        store.tick(arena.mOwner);
        require(spawner.getDelay() == 20, "a player 30 blocks away activated the spawner");

        player.setPosition(Vector3f(8.5f, 79.0f, 0.5f));
        store.tick(arena.mOwner);
        require(spawner.getDelay() == 19, "a player 8 blocks away did not activate the spawner");

        spawner.setDelay(0);
        int32_t turns = 0;
        while (arena.all(BLAZE).empty() && turns++ < 50)
            store.tick(arena.mOwner);
        const std::vector<ServerActor *> blazes = arena.all(BLAZE);
        require(!blazes.empty() && blazes.size() <= 4, "the spawner did not spawn one to four blazes");
        require(spawner.getDelay() >= 200 && spawner.getDelay() < 800, "the delay was not drawn between 200 and 800");

        for (const ServerActor *blaze: blazes) {
            const Vector3f &at = blaze->getPosition();
            const int32_t dx = (int32_t) std::floor(at.x - (float) position.x);
            const int32_t dy = (int32_t) std::floor(at.y - (float) position.y);
            const int32_t dz = (int32_t) std::floor(at.z - (float) position.z);
            require(std::abs(dx) + std::abs(dy) + std::abs(dz) <= 4, "a blaze spawned out of the spawn range");
            require(dy >= -1 && dy <= 1, "a blaze spawned more than a block above or below the spawner");
        }
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void stopsAtCap(Arena &arena) {
        arena.clear();
        const Vector3i position(32, 80, 32);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);
        ServerPlayer &player = arena.player(9502, Vector3f(36.5f, 79.0f, 32.5f));

        // six blazes inside the counted box: the next turn only draws a new delay
        for (int32_t i = 0; i < 6; ++i)
            arena.mOwner.spawnActor(*arena.mLevel, BLAZE, Vector3f(25.5f + (float) i, 79.0f, 28.5f));
        require(spawner.countNearby(arena.mOwner, *arena.mLevel, BLAZE) == 6, "the six blazes were not counted");

        spawner.setDelay(0);
        arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(arena.all(BLAZE).size() == 6, "the spawner spawned past its cap of six");
        require(spawner.getDelay() >= 200, "a capped spawner did not take a new delay");

        // a blaze nine blocks out is outside the box and not counted
        arena.clear();
        arena.mOwner.spawnActor(*arena.mLevel, BLAZE, Vector3f(41.5f, 79.0f, 32.5f));
        require(spawner.countNearby(arena.mOwner, *arena.mLevel, BLAZE) == 0, "a blaze outside the box was counted");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void needsSpace(Arena &arena) {
        arena.clear();
        const Vector3i position(-32, 80, -32);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);
        ServerPlayer &player = arena.player(9503, Vector3f(-28.5f, 79.0f, -32.5f));

        // every spot it can draw is stone: the turn spawns nothing and tries again
        for (int32_t x = position.x - 5; x <= position.x + 5; ++x) {
            for (int32_t z = position.z - 5; z <= position.z + 5; ++z) {
                for (int32_t y = position.y - 1; y <= position.y + 1; ++y) {
                    if (x != position.x || y != position.y || z != position.z)
                        arena.mLevel->setBlockState(x, y, z, BlockState("minecraft:stone"));
                }
            }
        }
        spawner.setDelay(0);
        for (int32_t turn = 0; turn < 5; ++turn)
            arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(arena.all(BLAZE).empty(), "a blaze spawned inside stone");
        require(spawner.getDelay() == 0, "a turn that spawned nothing took a new delay");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void emptySpawner(Arena &arena) {
        arena.clear();
        MobSpawnerBlockActor &spawner = arena.spawner(Vector3i(0, 80, 40), nullptr);
        ServerPlayer &player = arena.player(9504, Vector3f(2.5f, 79.0f, 40.5f));
        spawner.setDelay(0);
        arena.mLevel->getBlockActors().tick(arena.mOwner);
        require(arena.mOwner.getActors().empty() && spawner.getDelay() == 0, "an empty spawner did something");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }

    void spawnEgg(Arena &arena) {
        arena.clear();
        const Vector3i position(40, 80, 0);
        MobSpawnerBlockActor &spawner = arena.spawner(position, BLAZE);
        ServerPlayer &player = arena.player(9505, Vector3f(42.5f, 79.0f, 0.5f));
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:zombie_spawn_egg", 2));

        const std::unique_ptr<Item> egg = ItemClassRegistry::create(VanillaItems::ZOMBIE_SPAWN_EGG());
        require(egg->onUseOnBlock(arena.mOwner, player, player.getInventory().getItemInHand(), position, 1,
                                  Vector3f(0.5f, 1.0f, 0.5f)),
                "the egg was not used on the spawner");
        require(spawner.getEntityIdentifier() == "minecraft:zombie", "the egg did not change the spawner's mob");
        require(arena.mOwner.getActors().empty(), "the egg spawned a zombie on top of the spawner");
        require(player.getInventory().getItemInHand().mCount == 1, "the egg was not used up");
        arena.mOwner.erasePlayer(player.getNetworkIdentifier());
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-mob-spawner-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        defaultsAndSave(arena);
        spawnsNearPlayer(arena);
        stopsAtCap(arena);
        needsSpace(arena);
        emptySpawner(arena);
        spawnEgg(arena);
        std::cout << "mob spawner checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
