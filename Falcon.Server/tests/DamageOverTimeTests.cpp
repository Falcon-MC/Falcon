#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const char *const PERIODIC_DAMAGE_EVENT = "minecraft:add_periodic_damage";
    const int32_t HURT_INTERVAL_TICKS = 20;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Damage over time", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        MobActor &spawnVex() {
            const Vector3f position(0.5f, 100.0f, 0.5f);
            MobActor *vex = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, "minecraft:vex", position));
            require(vex != nullptr, "the vex did not spawn");
            return *vex;
        }

        void tick(MobActor &mob, int32_t ticks) {
            for (int32_t i = 0; i < ticks; i++)
                mob.tick(mOwner);
        }
    };

    void untouchedWithoutComponent() {
        Arena arena;
        MobActor &vex = arena.spawnVex();
        const float health = vex.getHealth();
        arena.tick(vex, HURT_INTERVAL_TICKS * 2);
        require(vex.getHealth() == health, "a vex without minecraft:damage_over_time was hurt");
    }

    void hurtsEveryInterval() {
        Arena arena;
        MobActor &vex = arena.spawnVex();
        vex.fireEvent(arena.mOwner, PERIODIC_DAMAGE_EVENT);
        const float health = vex.getHealth();

        // the countdown starts full: no hurt before time_between_hurt has passed
        arena.tick(vex, HURT_INTERVAL_TICKS - 1);
        require(vex.getHealth() == health, "hurt before time_between_hurt");
        arena.tick(vex, 1);
        require(vex.getHealth() == health - 1.0f, "not hurt by damage_per_hurt after time_between_hurt");

        arena.tick(vex, HURT_INTERVAL_TICKS - 1);
        require(vex.getHealth() == health - 1.0f, "hurt again before the next interval");
        arena.tick(vex, 1);
        require(vex.getHealth() == health - 2.0f, "not hurt again after the next interval");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-dot-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        untouchedWithoutComponent();
        hurtsEveryInterval();
        std::cout << "damage over time checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
