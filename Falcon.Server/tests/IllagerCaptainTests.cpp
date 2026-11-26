#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Illager captains", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            mLevel.insertChunk(LevelChunk(0, 0)).setPopulated(true);
            for (int32_t x = 0; x < 16; x++) {
                for (int32_t z = 0; z < 16; z++)
                    mLevel.setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        MobActor &spawn(const char *identifier, const char *event) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, Vector3f(8.5f, 64.0f, 8.5f),
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        void tick() {
            mOwner.getWorlds().getDefault().getOverworld().tickTime();
            PathFinder::tryReserveSearch(-1);
            mOwner.tickActors();
        }
    };

    bool captainFlag(const MobActor &mob) {
        return mob.getFlags().get(ActorFlag::IsIllagerCaptain);
    }

    // minecraft:is_illager_captain shows as the captain actor flag, on spawn and on promotion
    void flagFollowsComponent() {
        Arena arena;
        MobActor &vindicator = arena.spawn("minecraft:vindicator", "minecraft:spawn_as_illager_captain");
        MobActor &pillager = arena.spawn("minecraft:pillager", "minecraft:spawn_as_illager_captain");
        MobActor &follower = arena.spawn("minecraft:vindicator", "minecraft:spawn_as_patrol_follower");
        arena.tick();
        require(vindicator.getComponent("minecraft:is_illager_captain") != nullptr && captainFlag(vindicator),
                "a vindicator captain does not show as a captain");
        require(captainFlag(pillager), "a pillager captain does not show as a captain");
        require(!captainFlag(follower), "a patrol follower shows as a captain");

        follower.fireEvent(arena.mOwner, "minecraft:promote_to_patrol_captain");
        arena.tick();
        require(captainFlag(follower), "a promoted patrol captain does not show as a captain");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        flagFollowsComponent();
    } catch (const std::exception &error) {
        std::clog << "Illager captain regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "illager captain checks passed\n";
    return 0;
}
