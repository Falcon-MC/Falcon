#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/CommandOrigin.h"
#include "Server/Commands/HudCommand.h"
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
    // the game's HudElement ids
    const int32_t HOTBAR = 5;
    const int32_t ITEM_TEXT = 12;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class RecordingOrigin : public CommandOrigin {
    public:
        std::string mKey;

        std::string getSenderName() const override {
            return "hud-test";
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
            (void) parameters;
            mKey = key;
        }

        CommandPermission getCommandPermission() const override {
            return CommandPermission::Internal;
        }
    };

    std::string run(HudCommand &command, const std::vector<std::string> &arguments) {
        RecordingOrigin origin;
        command.execute(origin, arguments);
        return origin.mKey;
    }

    void hidesAndResets(ServerNetworkHandler &owner) {
        const NetworkIdentifier id("hud", 1);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 1).first->second;
        player.moveToLevel(owner.getWorlds().getDefault().getLevel(DimensionType::Overworld));
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        HudCommand command(owner);

        require(run(command, {"@a", "hide", "hotbar"}) == "commands.hud.success", "hiding the hotbar failed");
        require(player.isHudElementHidden(HOTBAR) && !player.isHudElementHidden(ITEM_TEXT),
                "hide touched more than the hotbar");
        require(run(command, {"@a", "hide"}) == "commands.hud.success" && player.isHudElementHidden(ITEM_TEXT),
                "hiding without an element did not hide everything");
        require(run(command, {"@a", "reset", "all"}) == "commands.hud.success" && !player.isHudElementHidden(HOTBAR),
                "reset all left an element hidden");
        require(run(command, {"@a", "hide", "minimap"}) == "commands.generic.usage", "an unknown element was taken");
        require(run(command, {"@a", "show"}) == "commands.generic.usage", "an unknown visibility was taken");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-hud-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        ServerNetworkHandler owner("Hud command", "", 1);
        hidesAndResets(owner);
        std::cout << "hud command checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
