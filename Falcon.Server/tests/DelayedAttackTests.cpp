#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/AI/Goal/DelayedAttackGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    // the ravager's: attack_duration 0.75 and hit_delay_pct 0.5
    const int32_t DURATION = 15;
    const int32_t HIT_DELAY = 7;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Delayed attack", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -8; x < 8; x++) {
                for (int32_t z = -8; z < 8; z++)
                    mLevel.setBlockState(x, 99, z, BlockState("minecraft:stone"));
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }
    };

    void swingsThenHits() {
        Arena arena;
        MobActor &ravager = arena.spawn("minecraft:ravager", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f(2.5f, 100.0f, 0.5f));
        require(ravager.setTarget(arena.mOwner, villager.getRuntimeId()), "target refused");

        DelayedAttackGoal goal(0.3f, 32.0f, 1.5f, DURATION, HIT_DELAY);
        require(goal.canUse(arena.mOwner, ravager), "a target in range was not attacked");
        goal.start(arena.mOwner, ravager);
        const float health = villager.getHealth();

        goal.tick(arena.mOwner, ravager);
        require(ravager.getFlags().get(ActorFlag::DelayedAttacking), "the swing did not start");
        for (int32_t tick = 1; tick < HIT_DELAY; tick++)
            goal.tick(arena.mOwner, ravager);
        require(villager.getHealth() == health, "the hit landed before hit_delay_pct");
        goal.tick(arena.mOwner, ravager);
        require(villager.getHealth() < health, "the hit did not land at hit_delay_pct");

        for (int32_t tick = HIT_DELAY + 1; tick <= DURATION; tick++)
            goal.tick(arena.mOwner, ravager);
        require(!ravager.getFlags().get(ActorFlag::DelayedAttacking), "the swing outlived attack_duration");

        const float afterHit = villager.getHealth();
        for (int32_t tick = 0; tick < DURATION; tick++)
            goal.tick(arena.mOwner, ravager);
        require(!ravager.getFlags().get(ActorFlag::DelayedAttacking), "a swing started without a pause");
        require(villager.getHealth() == afterHit, "hit again during the pause");
        goal.tick(arena.mOwner, ravager);
        require(ravager.getFlags().get(ActorFlag::DelayedAttacking), "no second swing after the pause");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-delayed-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        swingsThenHits();
        std::cout << "delayed attack checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
