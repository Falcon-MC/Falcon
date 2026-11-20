#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/AI/Goal/SendEventGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

namespace {
    const char *const RED_GROUP = "minecraft:sheep_red";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Send event", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }
    };

    SendEventGoal::Choice wololo() {
        SendEventGoal::Step step;
        step.mBaseDelay = 40;
        step.mEvent = "wololo";
        SendEventGoal::Choice choice;
        choice.mMinRange = 0.0f;
        choice.mMaxRange = 16.0f;
        choice.mWeight = 3.0f;
        choice.mCastDuration = 60;
        choice.mFilters = std::shared_ptr<json::Value>(
                json::parse("{\"test\":\"is_family\",\"subject\":\"other\",\"value\":\"sheep\"}"));
        choice.mSequence.push_back(step);
        return choice;
    }

    void convertsTheSheep() {
        Arena arena;
        MobActor &evoker = arena.spawn("minecraft:evocation_illager", Vector3f(0.5f, 100.0f, 0.5f));
        arena.spawn("minecraft:cow", Vector3f(3.5f, 100.0f, 0.5f));
        MobActor &sheep = arena.spawn("minecraft:sheep", Vector3f(6.5f, 100.0f, 0.5f));
        require(!sheep.hasComponentGroup(RED_GROUP), "the sheep started red");

        SendEventGoal goal({wololo()});
        require(goal.canUse(arena.mOwner, evoker), "the sheep in range was not chosen");
        goal.start(arena.mOwner, evoker);
        require(evoker.getFlags().get(ActorFlag::Casting), "the casting flag was not set");

        for (int32_t tick = 0; tick < 40; tick++)
            goal.tick(arena.mOwner, evoker);
        require(!sheep.hasComponentGroup(RED_GROUP), "the event fired before base_delay");
        goal.tick(arena.mOwner, evoker);
        require(sheep.hasComponentGroup(RED_GROUP), "wololo was not sent to the sheep");
        require(goal.canContinueToUse(arena.mOwner, evoker), "the cast ended before cast_duration");

        for (int32_t tick = 0; tick < 19; tick++)
            goal.tick(arena.mOwner, evoker);
        require(!evoker.getFlags().get(ActorFlag::Casting), "the casting flag outlived cast_duration");
        require(!goal.canContinueToUse(arena.mOwner, evoker), "the goal went on after its sequence");
    }

    void ignoresFarSheep() {
        Arena arena;
        MobActor &evoker = arena.spawn("minecraft:evocation_illager", Vector3f(0.5f, 100.0f, 0.5f));
        arena.spawn("minecraft:sheep", Vector3f(20.5f, 100.0f, 0.5f));
        SendEventGoal goal({wololo()});
        require(!goal.canUse(arena.mOwner, evoker), "a sheep past max_activation_range was chosen");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-send-event-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        convertsTheSheep();
        ignoresFarSheep();
        std::cout << "send event checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
