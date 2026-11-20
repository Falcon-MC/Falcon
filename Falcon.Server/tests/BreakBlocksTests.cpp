#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
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
    const char *const PUMPKIN = "minecraft:pumpkin";
    const char *const STONE = "minecraft:stone";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Break blocks", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Arena() {
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -8; x < 8; x++) {
                for (int32_t z = -8; z < 8; z++)
                    mLevel.setBlockState(x, 99, z, BlockState(STONE));
            }
            // a pumpkin and a stone the ravager's box touches
            mLevel.setBlockState(1, 100, 0, BlockState(PUMPKIN));
            mLevel.setBlockState(-1, 100, 0, BlockState(STONE));
            // out of reach
            mLevel.setBlockState(4, 100, 0, BlockState(PUMPKIN));
        }

        MobActor &ravager() {
            MobActor *mob = dynamic_cast<MobActor *>(
                    mOwner.spawnActor(mLevel, "minecraft:ravager", Vector3f(0.5f, 100.0f, -0.6f)));
            require(mob != nullptr, "the ravager did not spawn");
            return *mob;
        }

        std::string blockAt(int32_t x, int32_t y, int32_t z) {
            return mLevel.getBlockState(x, y, z).mName;
        }
    };

    void breaksListedBlocks() {
        Arena arena;
        MobActor &ravager = arena.ravager();
        ravager.tick(arena.mOwner);
        require(arena.blockAt(1, 100, 0) != PUMPKIN, "the touched pumpkin was not broken");
        require(arena.blockAt(-1, 100, 0) == STONE, "a block outside breakable_blocks was broken");
        require(arena.blockAt(4, 100, 0) == PUMPKIN, "a pumpkin out of reach was broken");
    }

    void respectsMobGriefing() {
        Arena arena;
        arena.mLevel.getGameRules().setFromString("mobgriefing", "false");
        MobActor &ravager = arena.ravager();
        ravager.tick(arena.mOwner);
        require(arena.blockAt(1, 100, 0) == PUMPKIN, "a block was broken with mobgriefing off");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-break-blocks-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        breaksListedBlocks();
        respectsMobGriefing();
        std::cout << "break blocks checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
