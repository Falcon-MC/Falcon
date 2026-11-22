#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <algorithm>
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
    const int32_t PIT_MIN = 6;
    const int32_t PIT_MAX = 12;
    const int32_t PIT_BOTTOM_Y = 79;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Mob falls", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -3; x <= 4; x++) {
                for (int32_t z = -3; z <= 4; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            const BlockState stone("minecraft:stone");
            const BlockState grass("minecraft:grass_block");
            for (int32_t x = -40; x < 56; x++) {
                for (int32_t z = -40; z < 56; z++) {
                    const bool pit = x >= PIT_MIN && x <= PIT_MAX && z >= PIT_MIN && z <= PIT_MAX;
                    mLevel.setBlockState(x, pit ? PIT_BOTTOM_Y : FLOOR_Y, z, pit ? stone : grass);
                }
            }
        }
    };

    // the lowest a lone mob reaches in five minutes beside a 20 block deep pit; dead ones (zombies in daylight)
    // stop counting
    float lowestBesidePit(const char *kind, const Vector3f &start) {
        Arena arena;
        ServerActor *mob = arena.mOwner.spawnActor(arena.mLevel, kind, start);
        require(dynamic_cast<MobActor *>(mob) != nullptr, std::string(kind) + " did not spawn");
        const int64_t id = mob->getUniqueId();

        float lowest = start.y;
        for (int32_t tick = 0; tick < 20 * 60 * 5; tick++) {
            ServerTickProbe::advance(arena.mOwner);
            arena.mOwner.tickActors();
            const auto found = arena.mOwner.getActors().find(id);
            if (found == arena.mOwner.getActors().end() || !found->second->isAlive())
                break;
            lowest = std::min(lowest, found->second->getPosition().y);
        }
        return lowest;
    }

    // a strolling mob never walks over the edge: its path stops on the middle of a block, never on a target left
    // on a block edge, where the mob slid off; one mob per arena, a crowd shoves its edge in as the game does
    void strollersStayOutOfThePit() {
        const char *const kinds[] = {"minecraft:cow", "minecraft:creeper", "minecraft:pig", "minecraft:zombie"};
        // on the block beside each side of the pit
        const Vector3f starts[] = {Vector3f(PIT_MIN - 0.5f, FLOOR_Y + 1.0f, PIT_MIN + 3.5f),
                                   Vector3f(PIT_MAX + 1.5f, FLOOR_Y + 1.0f, PIT_MAX - 2.5f),
                                   Vector3f(PIT_MIN + 3.5f, FLOOR_Y + 1.0f, PIT_MIN - 0.5f),
                                   Vector3f(PIT_MAX - 2.5f, FLOOR_Y + 1.0f, PIT_MAX + 1.5f)};
        int32_t fell = 0;
        // two rounds: a stroll that ends on an edge is drawn about one time in three
        for (int32_t round = 0; round < 2; round++) {
            for (const char *kind: kinds) {
                for (const Vector3f &start: starts) {
                    const float lowest = lowestBesidePit(kind, start);
                    if (lowest >= (float) FLOOR_Y - 3.0f)
                        continue;
                    fell++;
                    std::cout << kind << " fell to y " << lowest << '\n';
                }
            }
        }
        require(fell == 0, std::to_string(fell) + " mobs walked into the pit");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-mob-falls-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        strollersStayOutOfThePit();
        std::cout << "mob fall checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
