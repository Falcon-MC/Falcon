#include "Network/Handler/LoginHandler.h"
#include "Network/ServerNetworkHandler.h"
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
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the command list an operator is sent
    void operatorList() {
        ServerNetworkHandler owner("Command list", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4201).first->second;
        player.setName("Operator");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setOp(true);

        std::vector<std::string> sent;
        owner.getNetworkHandler().setOutboundFilter([&sent](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, recipient = 0;
            if (Packet::peekId(header, sender, recipient) == MinecraftPacketIds::AvailableCommands)
                sent.push_back(data);
            return false;
        });
        LoginHandler::sendAvailableCommands(owner, player);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(sent.size() == 1, "no command list was sent");

        // the whole list holds more enum values than the packet reader takes: the descriptions are looked for
        // in the bytes instead
        require(owner.getCommands().getCommand("help") == nullptr, "help is still registered");
        // the client keeps its own help: a second one makes it drop the arguments of every other command
        require(sent[0].find("commands.help.description") == std::string::npos, "help was sent to the client");
        require(sent[0].find("commands.gamemode.description") != std::string::npos, "an operator was not sent gamemode");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-command-list-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        operatorList();
        std::cout << "command list checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
