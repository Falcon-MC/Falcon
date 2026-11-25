#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/FloatGoal.h"
#include "World/Actor/AI/Goal/GoalSelector.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    const int32_t FLOOR_Y = 99;
    const int32_t FIELD_RADIUS = 24;
    // an adult rolls 0.001 a tick: never eating in this long is a one in a billion chance
    const int32_t GRAZING_TICKS = 20000;
    const int32_t POOL_X = 40;
    const int32_t POOL_BOTTOM = 94;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState block(const char *identifier) {
        return VanillaBlocks::fromIdentifier(identifier)->toBlockState();
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Float goal", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();

        Arena() {
            for (int32_t x = -2; x <= 3; ++x) {
                for (int32_t z = -2; z <= 2; ++z)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        void fill(int32_t minX, int32_t minY, int32_t minZ, int32_t maxX, int32_t maxY, int32_t maxZ,
                  const BlockState &state) {
            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t y = minY; y <= maxY; ++y) {
                    for (int32_t z = minZ; z <= maxZ; ++z)
                        mLevel.setBlockState(x, y, z, state);
                }
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event = nullptr) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) {
                        if (event != nullptr)
                            static_cast<MobActor &>(actor).setSpawnEvent(event);
                    }));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }

        int32_t count(const std::string &name) {
            int32_t total = 0;
            for (int32_t x = -FIELD_RADIUS; x <= FIELD_RADIUS; ++x) {
                for (int32_t z = -FIELD_RADIUS; z <= FIELD_RADIUS; ++z)
                    total += mLevel.getBlockState(x, FLOOR_Y, z).mName == name ? 1 : 0;
            }
            return total;
        }
    };

    bool sheared(const MobActor &sheep) {
        return sheep.getComponent("minecraft:is_sheared") != nullptr;
    }

    class JumpProbeGoal : public Goal {
    public:
        JumpProbeGoal() {
            setRequiredControlFlags((uint8_t) GoalControlFlag::Jump);
        }

        bool canUse(ServerNetworkHandler &owner, MobActor &mob) override {
            (void) owner;
            (void) mob;
            return true;
        }

        void start(ServerNetworkHandler &owner, MobActor &mob) override {
            (void) owner;
            (void) mob;
            mRunning = true;
        }

        void stop(ServerNetworkHandler &owner, MobActor &mob) override {
            (void) owner;
            (void) mob;
            mRunning = false;
        }

        bool mRunning = false;
    };

    void onlyInLiquid() {
        Arena arena;
        arena.fill(-2, FLOOR_Y, -2, 2, FLOOR_Y, 2, block("minecraft:stone"));
        arena.fill(POOL_X - 2, POOL_BOTTOM, -2, POOL_X + 2, POOL_BOTTOM, 2, block("minecraft:stone"));
        arena.fill(POOL_X - 2, POOL_BOTTOM + 1, -2, POOL_X + 2, FLOOR_Y, 2, block("minecraft:water"));
        arena.fill(-20, FLOOR_Y, -20, -16, FLOOR_Y, -16, block("minecraft:stone"));
        arena.fill(-20, FLOOR_Y + 1, -20, -16, FLOOR_Y + 2, -16, block("minecraft:lava"));

        MobActor &sheep = arena.spawn("minecraft:sheep", Vector3f(0.5f, 100.0f, 0.5f), "spawn_adult");
        FloatGoal goal;
        require(!goal.canUse(arena.mOwner, sheep), "the float goal ran on dry land");

        sheep.setPosition(Vector3f((float) POOL_X + 0.5f, 97.0f, 0.5f));
        require(goal.canUse(arena.mOwner, sheep), "the float goal did not run in water");

        sheep.setPosition(Vector3f(-17.5f, 100.0f, -17.5f));
        require(goal.canUse(arena.mOwner, sheep), "the float goal did not run in lava");
    }

    void freesTheJumpOnLand() {
        Arena arena;
        arena.fill(-2, FLOOR_Y, -2, 2, FLOOR_Y, 2, block("minecraft:stone"));
        arena.fill(POOL_X - 2, POOL_BOTTOM, -2, POOL_X + 2, POOL_BOTTOM, 2, block("minecraft:stone"));
        arena.fill(POOL_X - 2, POOL_BOTTOM + 1, -2, POOL_X + 2, FLOOR_Y, 2, block("minecraft:water"));
        MobActor &sheep = arena.spawn("minecraft:sheep", Vector3f(0.5f, 100.0f, 0.5f), "spawn_adult");

        GoalSelector selector;
        selector.addGoal(0, std::make_unique<FloatGoal>());
        auto probe = std::make_unique<JumpProbeGoal>();
        JumpProbeGoal &jumper = *probe;
        selector.addGoal(1, std::move(probe));

        selector.tick(arena.mOwner, sheep);
        require(jumper.mRunning, "a goal using the jump did not start beside the float goal on land");

        sheep.setPosition(Vector3f((float) POOL_X + 0.5f, 97.0f, 0.5f));
        selector.tick(arena.mOwner, sheep);
        require(!jumper.mRunning, "the float goal did not take the jump back in water");

        sheep.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        selector.tick(arena.mOwner, sheep);
        require(jumper.mRunning, "the jump goal did not start again out of the water");
        selector.stopAll(arena.mOwner, sheep);
    }

    void grazing() {
        Arena arena;
        arena.fill(-FIELD_RADIUS, FLOOR_Y, -FIELD_RADIUS, FIELD_RADIUS, FLOOR_Y, FIELD_RADIUS,
                   block("minecraft:grass_block"));
        MobActor &sheep = arena.spawn("minecraft:sheep", Vector3f(0.5f, 100.0f, 0.5f), "spawn_adult");
        sheep.fireEvent(arena.mOwner, "minecraft:on_sheared");
        require(sheared(sheep), "the sheep was not sheared");

        for (int32_t tick = 0; tick < GRAZING_TICKS && sheared(sheep); ++tick)
            sheep.tick(arena.mOwner);
        require(!sheared(sheep), "a sheared sheep on grass never grazed its wool back");
        require(arena.count("minecraft:dirt") >= 1, "the grazed grass did not turn to dirt");
    }

    void floating() {
        Arena arena;
        arena.fill(POOL_X - 3, POOL_BOTTOM, -3, POOL_X + 3, POOL_BOTTOM, 3, block("minecraft:stone"));
        arena.fill(POOL_X - 3, POOL_BOTTOM + 1, -3, POOL_X + 3, FLOOR_Y, 3, block("minecraft:water"));
        MobActor &sheep = arena.spawn("minecraft:sheep", Vector3f((float) POOL_X + 0.5f, 96.0f, 0.5f), "spawn_adult");

        float highest = 0.0f;
        for (int32_t tick = 0; tick < 200; ++tick) {
            sheep.tick(arena.mOwner);
            if (tick >= 100)
                highest = std::max(highest, sheep.getPosition().y);
        }
        require(highest > 98.5f, "the sheep did not float up to the surface");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-float-goal-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        onlyInLiquid();
        freesTheJumpOnLand();
        grazing();
        floating();
        std::cout << "float goal checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
