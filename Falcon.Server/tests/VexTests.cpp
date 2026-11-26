#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Navigation/PathFinder.h"
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
        ServerNetworkHandler mOwner{"Vexes", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            // a solid stone block from x 2 to 6, y 60 to 70
            for (int32_t x = 2; x <= 6; x++) {
                for (int32_t y = 60; y <= 70; y++) {
                    for (int32_t z = -3; z <= 3; z++)
                        mLevel.setBlockState(x, y, z, BlockState("minecraft:stone"));
                }
            }
            // a floor in front of the wall: a zombie walking into it stands on it and cannot drop under the wall
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -3; z <= 3; z++)
                    mLevel.setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }

        void ticks(int32_t count) {
            for (int32_t tick = 0; tick < count; tick++) {
                mOwner.getWorlds().getDefault().getOverworld().tickTime();
                PathFinder::tryReserveSearch(-1);
                mOwner.tickActors();
            }
        }
    };

    // minecraft:physics has_collision false: the stone does not stop it
    void fliesThroughBlocks() {
        Arena arena;
        MobActor &vex = arena.spawn("minecraft:vex", Vector3f(0.5f, 64.0f, 0.5f));
        require(!vex.getPhysics().mHasCollision && !vex.getPhysics().mHasGravity, "the vex collides or falls");
        for (int32_t tick = 0; tick < 40 && vex.getPosition().x < 8.0f; tick++) {
            vex.setMotion(Vector3f(0.3f, 0.0f, 0.0f));
            vex.tickMovement(arena.mOwner);
        }
        require(vex.getPosition().x >= 8.0f, "the stone stopped the vex at x " + std::to_string(vex.getPosition().x));

        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(0.5f, 64.0f, 0.5f));
        require(zombie.getPhysics().mHasCollision, "a zombie lost its collision");
        for (int32_t tick = 0; tick < 40; tick++) {
            zombie.setMotion(Vector3f(0.3f, zombie.getMotion().y, 0.0f));
            zombie.tickMovement(arena.mOwner);
        }
        require(zombie.getPosition().x < 2.0f, "a zombie walked into the stone");
    }

    // inside the stone a vex does not suffocate, a zombie does
    void noSuffocation() {
        Arena arena;
        MobActor &vex = arena.spawn("minecraft:vex", Vector3f(4.5f, 64.0f, 0.5f));
        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(4.5f, 64.0f, -1.5f));
        const float vexHealth = vex.getHealth();
        const float zombieHealth = zombie.getHealth();
        arena.ticks(30);
        require(vex.getHealth() == vexHealth, "the vex suffocated inside the stone");
        require(zombie.getHealth() < zombieHealth, "the zombie inside the stone did not suffocate");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        fliesThroughBlocks();
        noSuffocation();
    } catch (const std::exception &error) {
        std::clog << "Vex regression failed: " << error.what() << '\n';
        return 1;
    }
    std::cout << "vex checks passed\n";
    return 0;
}
