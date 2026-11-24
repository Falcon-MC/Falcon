#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/AnimateEntityPacket.h"
#include "Server/Commands/PlayAnimationCommand.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
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

        void sendTranslation(const std::string &key, const std::vector<std::string> &) override {
            mKeys.push_back(key);
        }

        std::vector<std::string> mKeys;
    };

    void run() {
        // left out, the options take the game's defaults
        AnimateEntityPacket packet;
        std::string invalid;
        require(PlayAnimationCommand::readAnimation({"animation.wave"}, packet, invalid), "a bare animation failed");
        require(packet.mAnimation == "animation.wave" && packet.mNextState == "default"
                        && packet.mBlendOutTime == 0.0f && packet.mStopExpression == "query.any_animation_finished"
                        && packet.mStopExpressionVersion == 1 && packet.mController == "__runtime_controller",
                "the defaults are not the game's");
        require(PlayAnimationCommand::readAnimation({"a", "next", "0.5", "query.is_moving", "ctrl"}, packet, invalid)
                        && packet.mNextState == "next" && packet.mBlendOutTime == 0.5f
                        && packet.mStopExpression == "query.is_moving" && packet.mController == "ctrl",
                "the options given were not taken");
        require(!PlayAnimationCommand::readAnimation({"a", "next", "soon"}, packet, invalid) && invalid == "soon",
                "a blend out time that is no number was taken");

        ServerNetworkHandler owner("Play animation", "", 1);
        PlayAnimationCommand command(owner);
        RecordingOrigin console(&owner);
        require(!command.execute(console, {"@e", "animation.wave"}), "an animation for nobody succeeded");
        require(console.mKeys.back() == "commands.generic.noTargetMatch", "nobody to animate was not reported");

        EntityDefinitions::initialize();
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        require(owner.spawnActor(level, "minecraft:pig", Vector3f(0.5f, 100.0f, 0.5f)) != nullptr, "no pig");
        require(command.execute(console, {"@e", "animation.wave", "default", "0.2"}), "the pig was not animated");
        require(console.mKeys.back() == "commands.playanimation.success", "the success was not reported");
        require(!command.execute(console, {"@e", "animation.wave", "default", "x"})
                        && console.mKeys.back() == "commands.generic.num.invalid",
                "a bad blend out time was not refused");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-play-animation-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
        std::cout << "play animation checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Play animation regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
