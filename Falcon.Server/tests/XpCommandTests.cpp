#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/Commands/XpCommand.h"
#include "Server/ServerPaths.h"

#include <algorithm>
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

    // an amount past int32 is refused, as strtol already does where long is 32 bits
    void run() {
        ServerNetworkHandler owner("Xp command", "", 1);
        XpCommand xp(owner);
        RecordingOrigin console(&owner);

        require(!xp.execute(console, {"5000000000", "Nobody"}), "a 5000000000 amount was accepted");
        require(console.mKeys.size() == 1 && console.mKeys.back() == "commands.generic.num.invalid",
                "a 5000000000 amount was not refused as an invalid number");

        require(!xp.execute(console, {"5", "Nobody"}), "an unknown target was accepted");
        require(console.mKeys.back() == "commands.generic.noTargetMatch"
                        && std::count(console.mKeys.begin(), console.mKeys.end(), "commands.generic.num.invalid") == 1,
                "an amount in range was refused before the target");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-xp-command-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Xp command regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
