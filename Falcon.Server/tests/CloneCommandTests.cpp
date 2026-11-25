#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class RecordingOrigin : public ServerCommandOrigin {
    public:
        explicit RecordingOrigin(ServerNetworkHandler *handler) : ServerCommandOrigin(handler) {}

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            mLast = key;
            for (const std::string &parameter: parameters)
                mLast += "|" + parameter;
        }

        std::string mLast;
    };

    std::string nameAt(Level &level, int32_t x, int32_t y, int32_t z) {
        return level.getBlockState(x, y, z).mName;
    }

    void run() {
        ServerNetworkHandler owner("Clone", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        for (int32_t x = -1; x <= 7; ++x) {
            for (int32_t z = -1; z <= 7; ++z)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }
        level.setBlockState(0, 64, 0, BlockState("minecraft:stone"));
        level.setBlockState(1, 64, 0, BlockState("minecraft:stone"));
        level.setBlockState(0, 65, 0, BlockState("minecraft:dirt"));
        std::unique_ptr<BlockActor> chest = BlockActorStore::create("Chest");
        require(chest != nullptr, "no chest block actor");
        chest->setPosition(Vector3i(1, 65, 0));
        level.getBlockActors().insert(std::move(chest));

        RecordingOrigin console(&owner);
        CommandMap &commands = owner.getCommands();
        // air is copied too: four blocks from a 2 x 2 x 1 box
        require(commands.dispatch(console, "clone 0 64 0 1 65 0 5 64 0") && console.mLast == "commands.clone.success|4",
                "a plain clone did not copy the four blocks: " + console.mLast);
        require(nameAt(level, 5, 64, 0) == "minecraft:stone" && nameAt(level, 5, 65, 0) == "minecraft:dirt",
                "the blocks did not land in place");
        require(level.getBlockActors().find(Vector3i(6, 65, 0)) != nullptr, "the block actor was not copied");

        // ~ coordinates written without spaces, from the console at 0 0 0
        require(commands.dispatch(console, "clone ~~64~ ~1~65~ ~40~64~") && console.mLast == "commands.clone.success|4",
                "unspaced relative coordinates were refused: " + console.mLast);
        require(nameAt(level, 40, 64, 0) == "minecraft:stone", "unspaced relative coordinates copied elsewhere");
        std::vector<std::string> tokens = {"~~-1.5~", "~hi~", "~", "5~"};
        std::vector<size_t> starts = {0, 8, 13, 15};
        CommandMap::splitRelativeCoordinates(tokens, &starts);
        require(tokens == std::vector<std::string>{"~", "~-1.5", "~", "~hi~", "~", "5~"}
                        && starts == std::vector<size_t>{0, 1, 6, 8, 13, 15},
                "relative coordinates were split wrongly");

        // the console faces south: ^ forward is +z, ^ up is +y
        require(commands.dispatch(console, "setblock ^ ^64.5 ^50.5 stone")
                        && nameAt(level, 0, 64, 50) == "minecraft:stone",
                "setblock did not take local coordinates: " + console.mLast);
        require(!commands.dispatch(console, "setblock ^ 64 ^1 stone"), "setblock mixed local and world coordinates");

        require(commands.dispatch(console, "clone 0 64 0 1 65 0 10 64 0 masked")
                        && console.mLast == "commands.clone.success|3",
                "masked copied the air: " + console.mLast);

        require(!commands.dispatch(console, "clone 0 64 0 1 65 0 1 64 0")
                        && console.mLast == "commands.clone.noOverlap",
                "an overlapping copy was allowed");
        require(commands.dispatch(console, "clone 0 64 0 1 65 0 1 64 0 replace force"), "force did not overlap");

        require(commands.dispatch(console, "clone 5 64 0 5 64 0 20 64 0 replace move")
                        && nameAt(level, 5, 64, 0) == "minecraft:air" && nameAt(level, 20, 64, 0) == "minecraft:stone",
                "move did not empty the source");

        require(commands.dispatch(console, "clone 0 64 0 1 65 0 30 64 0 filtered normal stone 0")
                        && console.mLast == "commands.clone.success|2" && nameAt(level, 30, 65, 0) == "minecraft:air",
                "filtered copied other blocks: " + console.mLast);

        // the volume is the blocks counted, corners included: 101 x 101 x 101 past 384 x 2048
        require(!commands.dispatch(console, "clone 0 0 0 100 100 100 0 101 0")
                        && console.mLast == "commands.clone.tooManyBlocks|1030301|786432",
                "a copy past the limit was not refused: " + console.mLast);
        require(!commands.dispatch(console, "clone 0 64 0 1 65 0 500 64 0")
                        && console.mLast == "commands.generic.outOfWorld",
                "a copy into an unloaded chunk was not refused");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-clone-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
        std::cout << "clone checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Clone regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
