#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/Commands/InputPermissionCommand.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const int32_t JUMP = 6;
    const int32_t CAMERA = 1;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class RecordingOrigin : public CommandOrigin {
    public:
        std::string mLast;

        std::string getSenderName() const override {
            return "inputpermission-test";
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
            mLast = key;
            for (const std::string &parameter: parameters)
                mLast += "|" + parameter;
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }
    };

    std::string run(InputPermissionCommand &command, const std::vector<std::string> &arguments) {
        RecordingOrigin origin;
        command.execute(origin, arguments);
        return origin.mLast;
    }

    void setsAndQueries(ServerNetworkHandler &owner) {
        const NetworkIdentifier id("inputpermission", 1);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1).first->second;
        player.moveToLevel(owner.getWorlds().getDefault().getLevel(DimensionType::Overworld));
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        InputPermissionCommand command(owner);

        const std::string set = run(command, {"set", "@a", "jump", "disabled"});
        require(set.rfind("commands.inputpermission.set.outputoneplayer|%commands.inputpermission.jump|"
                          "%commands.inputpermission.disabled|",
                          0)
                        == 0,
                "disabling jump did not report it: " + set);
        require(player.isInputLocked(JUMP) && !player.isInputLocked(CAMERA), "set locked the wrong input");

        require(run(command, {"query", "@a", "jump"})
                        == "commands.inputpermission.queryverbose|%commands.inputpermission.jump|0|1",
                "the query did not count one disabled player");
        require(run(command, {"query", "@a", "jump", "disabled"})
                        == "commands.inputpermission.query|%commands.inputpermission.jump|1|"
                           "%commands.inputpermission.disabled",
                "the filtered query did not count the disabled player");

        run(command, {"set", "@a", "jump", "enabled"});
        require(!player.isInputLocked(JUMP), "enabling jump left it locked");

        require(run(command, {"set", "@a", "fly", "enabled"}) == "commands.inputpermission.set.missingpermission",
                "an unknown permission was taken");
        require(run(command, {"set", "@a", "jump"}) == "commands.inputpermission.set.missingstate",
                "set ran without a state");
        require(run(command, {"query", "@a", "jump", "maybe"}) == "commands.inputpermission.set.invalidfilter|maybe",
                "a query took an unknown filter");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-inputpermission-"
               + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        ServerNetworkHandler owner("Input permission command", "", 1);
        setsAndQueries(owner);
        std::cout << "inputpermission command checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
