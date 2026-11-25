#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/EffectCommand.h"
#include "Server/Commands/ExecuteCommand.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/ServerActor.h"
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

        size_t start = 0;
        const auto includes = [](const std::vector<std::string> &values, const std::string &name) {
            return std::find(values.begin(), values.end(), name) != values.end();
        };
        const auto &commands = owner.getCommands();
        require(includes(commands.complete("/wo", {}, start), "world") && start == 1,
                "console did not complete a command after the slash");
        require(includes(commands.complete("world lo", {}, start), "load") && start == 6,
                "console did not complete a subcommand");
        require(includes(commands.complete("give Ale", {"Alex Stone"}, start), "\"Alex Stone\""),
                "console did not quote a player name");
        require(includes(commands.complete("give @p", {}, start), "@p"), "console did not complete a selector");
        require(includes(commands.complete("give @a dia", {}, start), "diamond"), "console did not complete an item");
        require(includes(commands.complete("setblock ~ ~ ~ minecraft:ston", {}, start), "minecraft:stone"),
                "console did not complete a block after a position");

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

        // /effect takes any entity, as the game's target does: a zombie can be kept from burning
        ServerActor *zombie = owner.spawnActor(level, "minecraft:zombie", Vector3f(4.5f, 100.0f, 0.5f));
        require(zombie != nullptr, "the zombie did not spawn");
        EffectCommand effect(owner);
        RecordingOrigin effectOrigin(&owner);
        require(effect.execute(effectOrigin, {"@e[type=zombie]", "fire_resistance", "60", "0", "true"}),
                "effect refused the zombie");
        require(zombie->hasEffect(MobEffectId::FireResistance), "the zombie got no fire resistance");
        RecordingOrigin clearOrigin(&owner);
        require(effect.execute(clearOrigin, {"@e[type=zombie]", "clear"}), "effect clear refused the zombie");
        require(!zombie->hasEffect(MobEffectId::FireResistance), "effect clear left the zombie's effect");
        RecordingOrigin noneOrigin(&owner);
        require(!effect.execute(noneOrigin, {"@e[type=creeper]", "speed"}), "effect ran with no target");
        require(noneOrigin.sent(NO_TARGET), "an effect without a target was not answered as no target");
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
