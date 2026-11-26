#include "CircuitTicks.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/Hostile/ShulkerActor.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::string text(const Vector3i &block) {
        return std::to_string(block.x) + "," + std::to_string(block.y) + "," + std::to_string(block.z);
    }

    bool same(const Vector3i &a, const Vector3i &b) {
        return a.x == b.x && a.y == b.y && a.z == b.z;
    }

    // the spawn chunk keeps its generated terrain: every case stays in the empty chunks around it
    struct Arena {
        ServerNetworkHandler mOwner{"Shulkers", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -4; x <= 4; x++) {
                for (int32_t z = -4; z <= 4; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }
        }

        void stone(int32_t x, int32_t y, int32_t z) {
            mLevel->setBlockState(x, y, z, BlockState("minecraft:stone"));
        }

        void air(int32_t x, int32_t y, int32_t z) {
            mLevel->setBlockState(x, y, z, BlockState("minecraft:air"));
        }

        // stone every third layer: every air cell has a stone face with air across from it
        void lattice(const Vector3i &centre, int32_t reach) {
            for (int32_t x = centre.x - reach; x <= centre.x + reach; ++x) {
                for (int32_t y = centre.y - reach; y <= centre.y + reach; ++y) {
                    for (int32_t z = centre.z - reach; z <= centre.z + reach; ++z) {
                        if (((y % 3) + 3) % 3 == 0)
                            stone(x, y, z);
                    }
                }
            }
        }

        ShulkerActor &shulker(const Vector3f &position) {
            ServerActor *actor = mOwner.spawnActor(*mLevel, ShulkerActor::IDENTIFIER, position);
            auto *shulker = dynamic_cast<ShulkerActor *>(actor);
            require(shulker != nullptr, "a shulker has no class of its own");
            return *shulker;
        }

        void tick(int32_t ticks = 1) {
            for (int32_t tick = 0; tick < ticks; ++tick)
                mOwner.tickActors();
        }

        int32_t shulkers() {
            int32_t count = 0;
            for (const auto &entry: mOwner.getActors())
                count += dynamic_cast<ShulkerActor *>(entry.second.get()) != nullptr ? 1 : 0;
            return count;
        }

        void clear(ShulkerActor &shulker) {
            mOwner.removeActor(shulker.getUniqueId());
        }
    };

    // on a floor it takes its block, faces down and sits on the block's floor, centred, without falling
    void floorAttachment(Arena &arena) {
        arena.stone(20, 99, 20);
        ShulkerActor &shulker = arena.shulker(Vector3f(20.3f, 100.2f, 20.8f));
        arena.tick(5);
        require(shulker.isAttached(), "the shulker took no block");
        require(same(shulker.getAttachPosition(), Vector3i(20, 100, 20)),
                "the shulker took the wrong block: " + text(shulker.getAttachPosition()));
        require(shulker.getAttachFace() == 0, "a shulker on a floor is not stuck down");
        const Vector3f position = shulker.getPosition();
        require(position.x == 20.5f && position.y == 100.0f && position.z == 20.5f,
                "the shulker is not centred on its block's floor");
        require(std::abs(shulker.getRotation().y - 180.0f) < 0.01f, "a new shulker does not face south");
        arena.clear(shulker);
    }

    // with only a wall beside it, it sticks to the wall and hangs there
    void wallAttachment(Arena &arena) {
        arena.stone(31, 100, 20);
        ShulkerActor &shulker = arena.shulker(Vector3f(30.5f, 100.0f, 20.5f));
        arena.tick(5);
        require(shulker.getAttachFace() == 5, "a shulker beside an east wall is stuck to face "
                                                      + std::to_string(shulker.getAttachFace()));
        require(shulker.getPosition().y == 100.0f, "a shulker on a wall fell");
        require(same(shulker.getAttachPosition(), Vector3i(30, 100, 20)), "a shulker on a wall left its block");

        // the wall goes and a ceiling is left: it turns to the ceiling, the first face that holds
        arena.stone(30, 101, 20);
        arena.air(31, 100, 20);
        arena.tick();
        require(shulker.getAttachFace() == 1, "a shulker whose wall went did not stick to the ceiling, face "
                                                      + std::to_string(shulker.getAttachFace()));
        require(same(shulker.getAttachPosition(), Vector3i(30, 100, 20)), "a shulker with a ceiling moved");
        arena.clear(shulker);
    }

    // a face is no good when the cell across from it is solid: the shell has no room to open
    void blockedShell(Arena &arena) {
        arena.stone(40, 99, 20);
        arena.stone(40, 101, 20);
        arena.stone(39, 100, 20);
        ShulkerActor &shulker = arena.shulker(Vector3f(40.5f, 100.0f, 20.5f));
        arena.tick();
        require(shulker.getAttachFace() == 4, "a shulker boxed in floor and ceiling did not take the west wall, face "
                                                      + std::to_string(shulker.getAttachFace()));
        arena.clear(shulker);
    }

    Vector3i attachOrStart(const ShulkerActor &shulker, const Vector3i &start) {
        return shulker.isAttached() ? shulker.getAttachPosition() : start;
    }

    // no face holds anywhere in its block: it teleports to one that does
    void strandedTeleport(Arena &arena) {
        const Vector3i centre(20, 100, -40);
        arena.lattice(centre, 10);
        // a bubble of air around it, so its own block holds nothing
        for (int32_t x = -1; x <= 1; ++x) {
            for (int32_t y = -1; y <= 1; ++y) {
                for (int32_t z = -1; z <= 1; ++z)
                    arena.air(centre.x + x, centre.y + 1 + y, centre.z + z);
            }
        }
        const Vector3i start(centre.x, centre.y + 1, centre.z);
        ShulkerActor &shulker = arena.shulker(Vector3f(20.5f, 101.0f, -39.5f));
        for (int32_t tick = 0; tick < 40 && same(attachOrStart(shulker, start), start); ++tick)
            arena.tick();
        const Vector3i landed = shulker.getAttachPosition();
        require(!same(landed, start), "a shulker with nothing to stick to never teleported");
        require(std::abs(landed.x - start.x) <= 8 && std::abs(landed.y - start.y) <= 8
                        && std::abs(landed.z - start.z) <= 8,
                "the shulker teleported beyond 8 blocks: " + text(landed));
        require(arena.mLevel->getBlockState(landed.x, landed.y, landed.z).mName == "minecraft:air",
                "the shulker teleported into a block");
        require(shulker.getPosition().x == (float) landed.x + 0.5f && shulker.getPosition().y == (float) landed.y,
                "the shulker did not move to its new block");
        arena.clear(shulker);
    }

    // a target opens the shell wide; losing it closes it again
    void peek(Arena &arena) {
        arena.stone(-20, 99, 20);
        arena.stone(-20, 99, 24);
        ShulkerActor &shulker = arena.shulker(Vector3f(-19.5f, 100.0f, 20.5f));
        ServerActor &cow = *arena.mOwner.spawnActor(*arena.mLevel, "minecraft:cow", Vector3f(-19.5f, 100.0f, 24.5f));
        arena.tick();
        require(shulker.setTarget(arena.mOwner, cow.getRuntimeId()), "the shulker could not target the cow");
        arena.tick();
        require(shulker.getPeek() == ShulkerActor::PEEK_OPEN, "a shulker with a target is not open wide, peek "
                                                                      + std::to_string(shulker.getPeek()));

        shulker.clearTarget();
        int32_t closedAfter = -1;
        for (int32_t tick = 0; tick < 200 && closedAfter < 0; ++tick) {
            arena.tick();
            if (shulker.getPeek() == ShulkerActor::PEEK_CLOSED)
                closedAfter = tick;
        }
        require(closedAfter >= 0, "a shulker that lost its target never closed");
        require(closedAfter <= 61, "a shulker closed only after " + std::to_string(closedAfter) + " ticks");
        arena.mOwner.removeActor(cow.getUniqueId());
        arena.clear(shulker);
    }

    // a shut shell turns projectiles aside; an open one does not
    void shellArmour(Arena &arena) {
        arena.stone(-30, 99, 20);
        ShulkerActor &shulker = arena.shulker(Vector3f(-29.5f, 100.0f, 20.5f));
        arena.tick();
        const float full = shulker.getHealth();
        ActorDamageSource arrow = ActorDamageSource::environment("death.attack.arrow", shulker.getName());
        arrow.asProjectile();
        if (shulker.getPeek() == ShulkerActor::PEEK_CLOSED) {
            arena.mOwner.damageActor(shulker, 4.0f, arrow);
            require(shulker.getHealth() == full, "an arrow hurt a shut shulker");
        }

        ServerActor &cow = *arena.mOwner.spawnActor(*arena.mLevel, "minecraft:cow", Vector3f(-29.5f, 100.0f, 24.5f));
        shulker.setTarget(arena.mOwner, cow.getRuntimeId());
        arena.tick();
        require(shulker.getPeek() == ShulkerActor::PEEK_OPEN, "the shulker did not open for its target");
        arena.mOwner.damageActor(shulker, 4.0f, arrow);
        require(shulker.getHealth() < full, "an arrow did not hurt an open shulker");
        arena.mOwner.removeActor(cow.getUniqueId());
        arena.clear(shulker);
    }

    // fire sends it away unharmed
    void fireTeleport(Arena &arena) {
        const Vector3i centre(-30, 100, -40);
        arena.lattice(centre, 10);
        const Vector3i start(centre.x, centre.y + 1, centre.z);
        ShulkerActor &shulker = arena.shulker(Vector3f(-29.5f, 101.0f, -39.5f));
        arena.tick();
        require(same(shulker.getAttachPosition(), start), "the shulker did not settle on the lattice");
        const float full = shulker.getHealth();
        for (int32_t hit = 0; hit < 5 && same(shulker.getAttachPosition(), start); ++hit) {
            arena.mOwner.damageActor(shulker, 1.0f,
                                     ActorDamageSource::environment("death.attack.inFire", shulker.getName()));
        }
        require(shulker.getHealth() == full, "fire hurt a shulker");
        require(!same(shulker.getAttachPosition(), start), "fire did not teleport the shulker");
        arena.clear(shulker);
    }

    // an open shulker struck by a shulker bullet teleports and, alone, leaves a new shulker where it was
    void bulletDuplication(Arena &arena) {
        const Vector3i centre(55, 100, -40);
        arena.lattice(centre, 10);
        ShulkerActor &shulker = arena.shulker(Vector3f(55.5f, 101.0f, -39.5f));
        ServerActor &cow = *arena.mOwner.spawnActor(*arena.mLevel, "minecraft:cow", Vector3f(55.5f, 101.0f, -37.5f));
        arena.tick();
        shulker.setTarget(arena.mOwner, cow.getRuntimeId());
        arena.tick();
        require(shulker.getPeek() == ShulkerActor::PEEK_OPEN, "the shulker did not open for its target");

        const int32_t before = arena.shulkers();
        const Vector3i start = shulker.getAttachPosition();
        ServerActor *bullet =
                arena.mOwner.spawnActor(*arena.mLevel, "minecraft:shulker_bullet", Vector3f(55.5f, 103.5f, -39.5f));
        require(bullet != nullptr, "no shulker bullet");
        ActorDamageSource source = ActorDamageSource::environment("death.attack.bullet", shulker.getName());
        source.asProjectile().withDamager(bullet);
        arena.mOwner.damageActor(shulker, 4.0f, source);
        require(!same(shulker.getAttachPosition(), start), "a bullet did not teleport the open shulker");
        require(arena.shulkers() == before + 1,
                "a lone shulker struck by a bullet left no copy: " + std::to_string(arena.shulkers()) + " shulkers");
        arena.mOwner.removeActor(cow.getUniqueId());
        arena.mOwner.removeActor(bullet->getUniqueId());
    }

    // a piston's arm entering its block moves the block one step on
    void pistonPush(Arena &arena) {
        const Vector3i piston(20, 100, 40);
        arena.mLevel->getBlockActors().loadChunk(1, 2, {}, arena.mOwner.getCodecContext());
        for (int32_t x = 20; x <= 24; ++x)
            arena.stone(x, 99, 40);
        // stored facing_direction 4 is east: the horizontal ones are stored reversed
        const Block *pistonBlock = VanillaBlocks::fromIdentifier("minecraft:piston");
        require(pistonBlock != nullptr, "no piston block");
        BlockState state = pistonBlock->toBlockState();
        state.mStates.putInt("facing_direction", 4);
        arena.mLevel->setBlockState(piston.x, piston.y, piston.z, state);

        ShulkerActor &shulker = arena.shulker(Vector3f(21.5f, 100.0f, 40.5f));
        arena.tick();
        require(same(shulker.getAttachPosition(), Vector3i(21, 100, 40)), "the shulker did not settle by the piston");

        const Block *power = VanillaBlocks::fromIdentifier("minecraft:redstone_block");
        arena.mLevel->setBlockState(piston.x - 1, piston.y, piston.z, power->toBlockState());
        settleCircuit(arena.mOwner, *arena.mLevel);
        for (int32_t tick = 0; tick < 10; ++tick) {
            PistonSystem::tick(arena.mOwner, *arena.mLevel);
            arena.tick();
        }
        require(same(shulker.getAttachPosition(), Vector3i(22, 100, 40)),
                "a piston did not push the shulker one block east: " + text(shulker.getAttachPosition()));
        require(shulker.getPosition().x == 22.5f, "the pushed shulker went back to its old block");
        arena.clear(shulker);
    }

    // the block, face and shell opening survive a save
    void saved(Arena &arena) {
        arena.stone(-41, 100, 30);
        ShulkerActor &shulker = arena.shulker(Vector3f(-39.5f, 100.0f, 30.5f));
        arena.tick();
        require(shulker.getAttachFace() == 4, "the shulker did not take the west wall");
        const Tag data = shulker.saveNbt();
        ShulkerActor &loaded = arena.shulker(Vector3f(0.0f, 0.0f, 0.0f));
        loaded.loadNbt(data);
        require(loaded.isAttached() && same(loaded.getAttachPosition(), Vector3i(-40, 100, 30)),
                "the attach block was not saved: " + text(loaded.getAttachPosition()));
        require(loaded.getAttachFace() == 4, "the attach face was not saved");

        EntityDataMap metadata;
        loaded.fillSpawnMetadata(metadata);
        int32_t found = 0;
        for (const EntityDataEntry &entry: metadata.mEntries) {
            if (entry.mId == ShulkerActor::ATTACH_FACE_DATA_ID && entry.mByteValue == 4)
                ++found;
            if (entry.mId == ShulkerActor::ATTACH_POSITION_DATA_ID && same(entry.mVector3iValue, Vector3i(-40, 100, 30)))
                ++found;
            if (entry.mId == ShulkerActor::ATTACHED_DATA_ID && entry.mShortValue == 1)
                ++found;
            if (entry.mId == ShulkerActor::PEEK_DATA_ID)
                ++found;
        }
        require(found == 4, "the spawn data lacks the shulker's entries");
        arena.clear(shulker);
        arena.clear(loaded);
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-shulkers-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        floorAttachment(arena);
        wallAttachment(arena);
        blockedShell(arena);
        strandedTeleport(arena);
        peek(arena);
        shellArmour(arena);
        fireTeleport(arena);
        bulletDuplication(arena);
        pistonPush(arena);
        saved(arena);
    } catch (const std::exception &error) {
        std::cout << "Shulker regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
