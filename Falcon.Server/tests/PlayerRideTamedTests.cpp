#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Control/MoveControl.h"
#include "World/Actor/AI/Goal/PlayerRideTamedGoal.h"
#include "World/Actor/ActorFlags.h"
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

class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner) {
        ++owner.mCurrentTick;
    }
};

namespace {
    const int32_t FLOOR_Y = 99;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Ride tamed", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            const BlockState grass("minecraft:grass_block");
            for (int32_t x = -32; x < 32; x++) {
                for (int32_t z = -32; z < 32; z++)
                    mLevel.setBlockState(x, FLOOR_Y, z, grass);
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            auto *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }

        void seat(MobActor &rider, MobActor &mount) {
            mount._addPassenger(rider.getUniqueId());
            rider._setVehicleId(mount.getUniqueId());
        }

        // ticks the mob spent with somewhere to walk to: a rider sitting still pushes its mount, the walk is the
        // goal's to stop
        int32_t ticksWalking(MobActor &mob, int32_t ticks) {
            int32_t walking = 0;
            for (int32_t tick = 0; tick < ticks; tick++) {
                ServerTickProbe::advance(mOwner);
                mOwner.tickActors();
                if (mob.getMoveControl().hasWanted())
                    walking++;
            }
            return walking;
        }
    };

    // the game's goal: any rider and the tamed flag, nothing else; it holds moving and looking
    void conditions() {
        Arena arena;
        MobActor &donkey = arena.spawn("minecraft:donkey", Vector3f(0.5f, FLOOR_Y + 1.0f, 0.5f));
        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(4.5f, FLOOR_Y + 1.0f, 4.5f));
        PlayerRideTamedGoal goal;
        require(goal.getRequiredControlFlags() == ((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look),
                "the goal does not hold moving and looking");

        require(!goal.canUse(arena.mOwner, donkey), "a wild donkey without a rider runs the goal");
        arena.seat(pig, donkey);
        require(!goal.canUse(arena.mOwner, donkey), "a wild donkey with a rider runs the goal");
        donkey.getFlags().set(ActorFlag::Tamed, true);
        require(goal.canUse(arena.mOwner, donkey), "a tamed donkey carrying a pig does not run the goal");
    }

    // a tamed donkey carries no saddle, so nobody steers it: the goal keeps it from strolling off with its rider
    void riddenDonkeyStaysPut() {
        Arena arena;
        MobActor &ridden = arena.spawn("minecraft:donkey", Vector3f(-10.5f, FLOOR_Y + 1.0f, 0.5f));
        MobActor &alone = arena.spawn("minecraft:donkey", Vector3f(10.5f, FLOOR_Y + 1.0f, 0.5f));
        ridden.fireEvent(arena.mOwner, "minecraft:on_tame");
        alone.fireEvent(arena.mOwner, "minecraft:on_tame");
        require(ridden.getComponent("minecraft:behavior.player_ride_tamed") != nullptr,
                "taming did not give the donkey the goal");
        ridden.getFlags().set(ActorFlag::Tamed, true);
        alone.getFlags().set(ActorFlag::Tamed, true);

        MobActor &pig = arena.spawn("minecraft:pig", Vector3f(-10.5f, FLOOR_Y + 1.0f, 4.5f));
        arena.seat(pig, ridden);

        const int32_t minute = 20 * 60;
        const int32_t riddenWalking = arena.ticksWalking(ridden, minute);
        require(riddenWalking == 0,
                "a tamed donkey with a rider walked for " + std::to_string(riddenWalking) + " ticks");
        const int32_t aloneWalking = arena.ticksWalking(alone, minute);
        require(aloneWalking > 0, "a tamed donkey without a rider never strolled");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-ride-tamed-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        conditions();
        riddenDonkeyStaysPut();
        std::cout << "player ride tamed checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
