#include "Network/Handler/LoginHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the StartGame a joining player receives, read back from the wire
    std::optional<StartGamePacket> startGameFor(ServerNetworkHandler &owner, ServerPlayer &player) {
        std::optional<StartGamePacket> captured;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &bytes) {
            ReadOnlyBinaryStream header(bytes);
            unsigned char sender = 0, recipient = 0;
            if (Packet::peekId(header, sender, recipient) != MinecraftPacketIds::StartGame)
                return false;
            ReadOnlyBinaryStream body(bytes);
            StartGamePacket packet;
            packet.readHeader(body);
            packet.read(body, owner.getCodecContext());
            captured = packet;
            return false;
        });
        LoginHandler::sendStartGame(owner, player);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        return captured;
    }

    // the client only reports achievements to Xbox Live in a world that keeps them, and the game keeps them for
    // worlds without commands
    void run() {
        ServerNetworkHandler owner("Achievements", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4301).first->second;
        player.setName("Achiever");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setPosition(Vector3f(2.5f, 100.0f, 2.5f));

        owner.setCommandsEnabled(false);
        const std::optional<StartGamePacket> withoutCommands = startGameFor(owner, player);
        require(withoutCommands.has_value(), "no StartGame was sent");
        require(!withoutCommands->mCommandsEnabled && !withoutCommands->mAchievementsDisabled,
                "a world without commands told the client its achievements are off");

        owner.setCommandsEnabled(true);
        const std::optional<StartGamePacket> withCommands = startGameFor(owner, player);
        require(withCommands.has_value() && withCommands->mCommandsEnabled && withCommands->mAchievementsDisabled,
                "a world with commands kept its achievements");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-achievements-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Achievements regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    if (result == 0)
        std::cout << "Achievements checks passed\n";
    return result;
}
