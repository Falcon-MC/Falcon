#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/AI/Goal/ChargeAttackGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Charge attack", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "charge.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, "mob did not spawn");
            return *mob;
        }
    };

    ChargeAttackGoal::Settings alwaysCharges() {
        ChargeAttackGoal::Settings settings;
        settings.mSuccessRate = 1.0f;
        return settings;
    }

    void needsRoom(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &vex = arena.spawn("minecraft:vex", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f(1.5f, 100.0f, 0.5f));
        require(vex.setTarget(arena.mOwner, villager.getRuntimeId()), "target refused");

        ChargeAttackGoal goal(alwaysCharges());
        require(!goal.canUse(arena.mOwner, vex), "charged inside min_distance");

        villager.setPosition(Vector3f(6.5f, 100.0f, 0.5f));
        ChargeAttackGoal never(ChargeAttackGoal::Settings{2.0f, 3.0f, 0.0f, 1.0f});
        require(!never.canUse(arena.mOwner, vex), "charged with a zero success_rate");
        require(goal.canUse(arena.mOwner, vex), "a far target was not charged");
    }

    void chargeAndStrike(const std::filesystem::path &directory) {
        Arena arena(directory);
        MobActor &vex = arena.spawn("minecraft:vex", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f(6.5f, 100.0f, 0.5f));
        require(vex.setTarget(arena.mOwner, villager.getRuntimeId()), "target refused");

        ChargeAttackGoal goal(alwaysCharges());
        require(goal.canUse(arena.mOwner, vex), "the charge did not start");
        goal.start(arena.mOwner, vex);
        require(vex.getMoveControl().hasWanted(), "no dash was aimed");
        require(vex.getMoveControl().getWantedPosition().y > 100.0f, "the dash was not aimed above the feet");
        require(vex.getFlags().get(ActorFlag::Charging), "the charging flag was not set");
        require(goal.canContinueToUse(arena.mOwner, vex), "the charge ended at once");
        require(!goal.canUse(arena.mOwner, vex), "a second charge started during the dash");

        const float healthBefore = villager.getHealth();
        vex.setPosition(Vector3f(6.5f, 100.0f, 0.5f));
        goal.tick(arena.mOwner, vex);
        require(villager.getHealth() < healthBefore, "touching the target did not strike it");
        require(!vex.getFlags().get(ActorFlag::Charging), "the strike did not end the charge");
        require(!goal.canContinueToUse(arena.mOwner, vex), "the charge went on after the strike");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-charge-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        needsRoom(directory);
        chargeAndStrike(directory);
        std::cout << "charge attack checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
