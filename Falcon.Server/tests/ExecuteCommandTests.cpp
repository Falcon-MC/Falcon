#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/ExecuteCommand.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const char *const NO_TARGET = "commands.generic.noTargetMatch";
    const char *const OUT_OF_WORLD = "commands.testforblock.outOfWorld";

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class RecordingOrigin : public ServerCommandOrigin {
    public:
        explicit RecordingOrigin(ServerNetworkHandler *handler) : ServerCommandOrigin(handler) {}

        void sendTranslation(const std::string &key, const std::vector<std::string> &) override {
            mKeys.push_back(key);
        }

        bool sent(const char *key) const {
            return std::find(mKeys.begin(), mKeys.end(), key) != mKeys.end();
        }

        std::vector<std::string> mKeys;
    };

    void run(const std::filesystem::path &directory) {
        ServerNetworkHandler owner("Execute command", "", 1);
        const std::filesystem::path path = directory / "normal.properties";
        std::ofstream(path) << "difficulty=normal\n";
        owner.setProperties(PropertiesSettings(path.string()));
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        for (int32_t x = -1; x <= 1; x++) {
            for (int32_t z = -1; z <= 1; z++)
                level.insertChunk(LevelChunk(x, z)).setPopulated(true);
        }

        ExecuteCommand execute(owner);

        // a block test far from any loaded chunk is refused instead of generating the column
        RecordingOrigin unloaded(&owner);
        require(!execute.execute(unloaded, {"if", "block", "100000", "64", "100000", "air"}),
                "a block test in an unloaded chunk passed");
        require(unloaded.sent(OUT_OF_WORLD), "an unloaded block test was not answered as out of world");
        require(level.peekChunkPtr(100000 >> 4, 100000 >> 4) == nullptr, "the block test loaded a chunk");

        RecordingOrigin nobody(&owner);
        require(!execute.execute(nobody, {"at", "@e", "if", "block", "100000", "64", "100000", "air"}),
                "execute at ran without any entity");
        require(nobody.sent(NO_TARGET) && !nobody.sent(OUT_OF_WORLD), "an empty world still matched an entity");

        // execute at and as reach entities that are not players: the chain goes on to the block test
        require(owner.spawnActor(level, "minecraft:cow", Vector3f(0.5f, 100.0f, 0.5f)) != nullptr,
                "the cow did not spawn");
        for (const char *subcommand: {"at", "as"}) {
            RecordingOrigin origin(&owner);
            execute.execute(origin, {subcommand, "@e", "if", "block", "100000", "64", "100000", "air"});
            require(!origin.sent(NO_TARGET), std::string("execute ") + subcommand + " did not find the cow");
            require(origin.sent(OUT_OF_WORLD), std::string("execute ") + subcommand + " did not run for the cow");
        }
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-execute-command-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Execute command regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
