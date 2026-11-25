#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/Commands/MusicCommand.h"
#include "Server/ServerPaths.h"

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

    class RecordingOrigin : public CommandOrigin {
    public:
        std::string mKey;
        std::vector<std::string> mParameters;

        std::string getSenderName() const override {
            return "music-test";
        }

        bool isPlayer() const override {
            return false;
        }

        ServerPlayer *asPlayer() override {
            return nullptr;
        }

        void sendMessage(const std::string &message) override {
            (void) message;
        }

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            mKey = key;
            mParameters = parameters;
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }
    };

    struct Run {
        bool mResult;
        std::string mKey;
        std::vector<std::string> mParameters;
    };

    Run run(MusicCommand &command, const std::vector<std::string> &arguments) {
        RecordingOrigin origin;
        const bool result = command.execute(origin, arguments);
        return {result, origin.mKey, origin.mParameters};
    }

    void answers(ServerNetworkHandler &owner) {
        MusicCommand command(owner);

        Run played = run(command, {"play", "record.cat", "0.5", "2", "loop"});
        require(played.mResult && played.mKey == "commands.music.success.playAction"
                        && played.mParameters == std::vector<std::string>({"record.cat"}),
                "play did not answer with the track");
        Run queued = run(command, {"queue", "record.cat"});
        require(queued.mResult && queued.mKey == "commands.music.success.queueAction", "queue failed");
        Run stopped = run(command, {"stop", "1.5"});
        require(stopped.mResult && stopped.mKey == "commands.music.success.stopAction", "stop failed");
        Run volume = run(command, {"volume", "0"});
        require(volume.mResult && volume.mKey == "commands.music.success.volumeAction"
                        && volume.mParameters == std::vector<std::string>({"0.00"}),
                "volume did not take 0 or did not print two decimals");

        Run quiet = run(command, {"play", "record.cat", "0"});
        require(!quiet.mResult && quiet.mKey == "commands.generic.num.tooSmall"
                        && quiet.mParameters == std::vector<std::string>({"0.00", "0.01"}),
                "a silent track was played");
        Run loud = run(command, {"volume", "1.5"});
        require(!loud.mResult && loud.mKey == "commands.generic.num.tooBig", "a volume over 1 was taken");
        Run slow = run(command, {"stop", "11"});
        require(!slow.mResult && slow.mKey == "commands.generic.num.tooBig"
                        && slow.mParameters == std::vector<std::string>({"11.00", "10.00"}),
                "a fade over 10 seconds was taken");
        Run empty = run(command, {"play", ""});
        require(!empty.mResult && empty.mKey == "commands.music.failure.emptyTrackName", "an empty track was played");
        Run bad = run(command, {"play", "record.cat", "1", "0", "forever"});
        require(!bad.mResult && bad.mKey == "commands.generic.usage", "an unknown repeat mode was taken");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-music-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        ServerNetworkHandler owner("Music command", "", 1);
        answers(owner);
        std::cout << "music command checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
