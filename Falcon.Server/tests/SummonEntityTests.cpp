#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/AI/Goal/SummonEntityGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const char *const VEX = "minecraft:vex";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Summon entity", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 1; x++) {
                for (int32_t z = -2; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -24; x < 24; x++) {
                for (int32_t z = -24; z < 24; z++)
                    mLevel.setBlockState(x, 99, z, BlockState("minecraft:stone"));
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }

        std::vector<ServerActor *> summonedBy(const MobActor &summoner) {
            std::vector<ServerActor *> found;
            for (auto &entry: mOwner.getActors()) {
                ServerActor &actor = *entry.second;
                if (actor.getTypeId() == VEX && actor.getOwnerUniqueId() == summoner.getUniqueId())
                    found.push_back(&actor);
            }
            return found;
        }
    };

    SummonEntityGoal::Choice vexChoice() {
        SummonEntityGoal::Spawn spawn;
        spawn.mShape = SummonEntityGoal::Shape::Circle;
        spawn.mCount = 3;
        spawn.mEntityType = VEX;
        spawn.mSummonEvent = "minecraft:add_damage_timer";
        spawn.mSummonCap = 8;
        spawn.mSummonCapRadius = 16.0f;
        SummonEntityGoal::Choice choice;
        choice.mWeight = 1.0f;
        choice.mCastDuration = 5;
        choice.mCooldown = 340;
        choice.mSequence.push_back(spawn);
        return choice;
    }

    void summonsVexes() {
        Arena arena;
        MobActor &evoker = arena.spawn("minecraft:evocation_illager", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f(6.5f, 100.0f, 0.5f));
        require(evoker.setTarget(arena.mOwner, villager.getRuntimeId()), "target refused");

        SummonEntityGoal goal({vexChoice()});
        require(goal.canUse(arena.mOwner, evoker), "the only choice in range was not picked");
        goal.start(arena.mOwner, evoker);
        require(evoker.getFlags().get(ActorFlag::Casting), "the casting flag was not set");
        goal.tick(arena.mOwner, evoker);

        const std::vector<ServerActor *> vexes = arena.summonedBy(evoker);
        require(vexes.size() == 3, "expected 3 vexes, got " + std::to_string(vexes.size()));
        for (const ServerActor *vex: vexes) {
            const Vector3f &position = vex->getPosition();
            require(position.y == 100.0f, "a vex was not put on the floor");
            const float dx = position.x - 0.5f;
            const float dz = position.z - 0.5f;
            require(std::sqrt(dx * dx + dz * dz) < 2.5f, "a vex was summoned far from the circle");
        }
        for (int32_t tick = 0; tick < 4; tick++)
            goal.tick(arena.mOwner, evoker);
        require(!evoker.getFlags().get(ActorFlag::Casting), "the casting flag outlived cast_duration");
        require(!goal.canContinueToUse(arena.mOwner, evoker), "the goal went on after its sequence");
        goal.stop(arena.mOwner, evoker);
        require(!goal.canUse(arena.mOwner, evoker), "cast again inside cooldown_time");
    }

    void respectsCapAndRange() {
        Arena arena;
        MobActor &evoker = arena.spawn("minecraft:evocation_illager", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f(6.5f, 100.0f, 0.5f));
        require(evoker.setTarget(arena.mOwner, villager.getRuntimeId()), "target refused");

        SummonEntityGoal::Choice near = vexChoice();
        near.mMaxRange = 3.0f;
        SummonEntityGoal tooFar({near});
        require(!tooFar.canUse(arena.mOwner, evoker), "a choice out of max_activation_range was picked");

        SummonEntityGoal::Choice unweighted = vexChoice();
        unweighted.mWeight = 0.0f;
        SummonEntityGoal noWeight({unweighted});
        require(!noWeight.canUse(arena.mOwner, evoker), "a choice of weight 0 was picked");

        for (int32_t i = 0; i < 8; i++)
            arena.spawn(VEX, Vector3f(-3.5f + (float) i, 101.0f, 3.5f));
        SummonEntityGoal capped({vexChoice()});
        require(!capped.canUse(arena.mOwner, evoker), "summoned past summon_cap");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-summon-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        summonsVexes();
        respectsCapAndRange();
        std::cout << "summon entity checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
