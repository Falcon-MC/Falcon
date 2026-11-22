#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ExperienceOrbActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/Actor/SculkCatalystBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const int32_t FLOOR_Y = 99;
    const Vector3i CATALYST(0, FLOOR_Y, 0);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Sculk catalyst", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            mLevel.setOwner(&mOwner);
            std::vector<int64_t> columns;
            for (int32_t x = -3; x <= 3; x++) {
                for (int32_t z = -3; z <= 3; z++) {
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
                    columns.push_back(((int64_t) x << 32) | (int64_t) (uint32_t) z);
                }
            }
            // a cursor only moves through ticking chunks
            mLevel.setActiveColumns(columns);
            const BlockState stone("minecraft:stone");
            for (int32_t x = -24; x < 24; x++) {
                for (int32_t z = -24; z < 24; z++) {
                    for (int32_t y = FLOOR_Y - 3; y <= FLOOR_Y; y++)
                        mLevel.setBlockState(x, y, z, stone);
                }
            }
            const BlockState catalyst = VanillaBlocks::SCULK_CATALYST().toBlockState();
            mLevel.setBlock(CATALYST, catalyst, true);
            mLevel.onBlockPlaced(CATALYST, catalyst);
        }

        SculkCatalystBlockActor &catalyst() {
            auto *actor = mLevel.getBlockActors().find<SculkCatalystBlockActor>(CATALYST);
            require(actor != nullptr, "the catalyst has no block actor");
            return *actor;
        }

        std::string blockAt(int32_t x, int32_t y, int32_t z) {
            return mLevel.getBlockState(x, y, z).mName;
        }

        int32_t count(const char *name) {
            int32_t found = 0;
            for (int32_t x = -24; x < 24; x++) {
                for (int32_t z = -24; z < 24; z++) {
                    for (int32_t y = FLOOR_Y - 3; y <= FLOOR_Y + 3; y++)
                        found += blockAt(x, y, z) == name ? 1 : 0;
                }
            }
            return found;
        }

        int32_t orbs() {
            int32_t found = 0;
            mOwner.forEachActorNear(Vector3f(0.5f, FLOOR_Y + 1.0f, 0.5f), 16.0f, [&found](ServerActor &actor) {
                if (dynamic_cast<ExperienceOrbActor *>(&actor) != nullptr)
                    ++found;
            });
            return found;
        }
    };

    // a zombie dies near: its experience becomes charge at its feet, no orb drops, and the catalyst blooms
    void death() {
        Arena arena;
        auto *zombie = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(arena.mLevel, "minecraft:zombie", Vector3f(5.5f, FLOOR_Y + 1.0f, 0.5f)));
        require(zombie != nullptr, "the zombie did not spawn");
        zombie->kill(arena.mOwner);

        const std::vector<SculkChargeCursor> &cursors = arena.catalyst().getSpreader().getCursors();
        require(cursors.size() == 1 && cursors.front().mPosition == Vector3i(5, FLOOR_Y + 1, 0),
                "the death did not leave a cursor at the zombie's block");
        require(cursors.front().mCharge > 0, "the zombie's experience gave no charge");
        require(arena.orbs() == 0, "the zombie still dropped experience");
        require(arena.mLevel.getBlockState(CATALYST.x, CATALYST.y, CATALYST.z).mStates.getByte("bloom", 0) == 1,
                "the catalyst did not bloom");

        const BlockState bloomed = arena.mLevel.getBlockState(CATALYST.x, CATALYST.y, CATALYST.z);
        VanillaBlocks::fromState(bloomed)->onScheduledUpdate(arena.mOwner, arena.mLevel, CATALYST, bloomed);
        require(arena.mLevel.getBlockState(CATALYST.x, CATALYST.y, CATALYST.z).mStates.getByte("bloom", 0) == 0,
                "the bloom did not end");
    }

    // a death out of reach is left alone
    void farDeath() {
        Arena arena;
        auto *zombie = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(arena.mLevel, "minecraft:zombie", Vector3f(14.5f, FLOOR_Y + 1.0f, 0.5f)));
        require(zombie != nullptr, "the zombie did not spawn");
        zombie->kill(arena.mOwner);
        require(arena.catalyst().getSpreader().getCursors().empty(), "a death past 10 blocks charged the catalyst");
    }

    // a big charge spreads sculk over the stone and veins over what it touches, then runs out
    void spreading() {
        Arena arena;
        SculkSpreader &spreader = arena.catalyst().getSpreader();
        spreader.addCursors(Vector3i(6, FLOOR_Y + 1, 0), 60);
        std::mt19937 random(7);
        for (int32_t tick = 0; tick < 100000 && !spreader.getCursors().empty(); tick++) {
            spreader.updateCursors(&arena.mOwner, arena.mLevel, CATALYST, random, true);
        }

        require(spreader.getCursors().empty(), "the charge never ran out");
        require(arena.count("minecraft:sculk") > 0, "no stone turned to sculk");
        require(arena.count("minecraft:sculk_vein") > 0, "no vein spread");
        require(arena.blockAt(CATALYST.x, CATALYST.y, CATALYST.z) == "minecraft:sculk_catalyst",
                "the spread replaced the catalyst");
    }

    // a vein clings to the stone under the air it spreads into
    void veins() {
        Arena arena;
        const int32_t placed = SculkBehaviours::spreadVeinsAll(arena.mLevel, Vector3i(8, FLOOR_Y + 1, 8), true);
        require(placed == 1, "a same position spread did not place one vein");
        const BlockState vein = arena.mLevel.getBlockState(8, FLOOR_Y + 1, 8);
        require(vein.mName == "minecraft:sculk_vein" && SculkBehaviours::hasFace(vein, 0)
                        && !SculkBehaviours::hasFace(vein, 1),
                "the vein does not hang on the stone below");
    }

    void saved() {
        SculkSpreader spreader;
        spreader.addCursors(Vector3i(1, 2, 3), 2500);
        require(spreader.getCursors().size() == 3 && spreader.getTotalCharge() == 2500,
                "a charge was not split in cursors of 1000");
        Tag data = Tag::ofCompound();
        spreader.save(data);
        SculkSpreader loaded;
        loaded.load(data);
        require(loaded.getTotalCharge() == 2500 && loaded.getCursors().front().mDecayDelay == 1,
                "the cursors did not load back");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-sculk-catalyst-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        veins();
        death();
        farDeath();
        spreading();
        veins();
        saved();
        std::cout << "sculk catalyst checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
