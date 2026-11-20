#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/AI/Goal/KnockbackRoarGoal.h"
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
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::shared_ptr<json::Value> jsonOf(const char *text) {
        return std::shared_ptr<json::Value>(json::parse(text));
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Knockback roar", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 1; x++) {
                for (int32_t z = -2; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }
    };

    KnockbackRoarGoal::Settings ravagerRoar() {
        KnockbackRoarGoal::Settings settings;
        settings.mHorizontalStrength = 3;
        settings.mKnockbackFilters =
                jsonOf("{\"test\":\"is_family\",\"subject\":\"other\",\"operator\":\"!=\",\"value\":\"ravager\"}");
        settings.mDamageFilters =
                jsonOf("{\"test\":\"is_family\",\"subject\":\"other\",\"operator\":\"!=\",\"value\":\"illager\"}");
        settings.mOnRoarEnd = jsonOf("{\"event\":\"minecraft:become_stunned\",\"target\":\"self\"}");
        return settings;
    }

    void roars() {
        Arena arena;
        MobActor &ravager = arena.spawn("minecraft:ravager", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(3.5f, 100.0f, 0.5f));
        MobActor &pillager = arena.spawn("minecraft:pillager", Vector3f(-2.5f, 100.0f, 0.5f));
        MobActor &cow = arena.spawn("minecraft:cow", Vector3f(-12.5f, 100.0f, 0.5f));
        // the same place in the nether: out of the roar's reach
        Level &nether = arena.mOwner.getWorlds().getDefault().getLevel(DimensionType::Nether);
        nether.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        ServerActor *netherPig = arena.mOwner.spawnActor(nether, "minecraft:pig", Vector3f(3.5f, 100.0f, 0.5f));
        MobActor *elsewhere = dynamic_cast<MobActor *>(netherPig);
        require(elsewhere != nullptr, "the nether pig did not spawn");
        const float pigHealth = pig.getHealth();
        const float pillagerHealth = pillager.getHealth();

        KnockbackRoarGoal goal(ravagerRoar());
        for (int32_t call = 0; call < 3; call++)
            require(!goal.canUse(arena.mOwner, ravager), "roared inside cooldown_time");
        require(goal.canUse(arena.mOwner, ravager), "the roar never came off cooldown");
        goal.start(arena.mOwner, ravager);
        require(ravager.getFlags().get(ActorFlag::Roaring), "the roaring flag was not set");

        for (int32_t tick = 0; tick < 9; tick++)
            goal.tick(arena.mOwner, ravager);
        require(pig.getHealth() == pigHealth, "the roar struck before attack_time");
        goal.tick(arena.mOwner, ravager);
        require(pig.getHealth() == pigHealth - 6.0f, "the pig did not take knockback_damage");
        require(pig.getMotion().x > 0.0f, "the pig was not pushed away");
        require(pillager.getHealth() == pillagerHealth, "damage_filters let an illager be hurt");
        require(pillager.getMotion().x < 0.0f, "the pillager was not pushed away");
        require(cow.getMotion().x == 0.0f, "a mob out of knockback_range was pushed");
        require(elsewhere->getMotion().x == 0.0f, "a mob in another level was pushed");

        for (int32_t tick = 0; tick < 10; tick++)
            goal.tick(arena.mOwner, ravager);
        require(!goal.canContinueToUse(arena.mOwner, ravager), "the roar outlived its duration");
        goal.stop(arena.mOwner, ravager);
        require(!ravager.getFlags().get(ActorFlag::Roaring), "the roaring flag stayed set");
        require(ravager.hasComponentGroup("stunned"), "on_roar_end was not fired");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-roar-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        roars();
        std::cout << "knockback roar checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
