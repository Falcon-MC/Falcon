#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockEventPacket.h"
#include "Protocol/Packets/ContainerClosePacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/EnderChestBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
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

class ServerTickProbe {
public:
    static void leave(ServerNetworkHandler &owner, const NetworkIdentifier &id) {
        owner.onConnectionClosed(id, DisconnectFailReason::Disconnected, "");
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ServerPlayer &join(ServerNetworkHandler &owner, Level &level, const NetworkIdentifier &id, int64_t runtimeId,
                       const char *name) {
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, runtimeId).first->second;
        player.setName(name);
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(2.5f, 100.0f, 0.5f));
        player.getInventoryManager().attach(&player, &owner);
        // the chest's chunk was sent, so its lid events reach this player
        player.getSentChunks().insert(0);
        return player;
    }

    // two players look into one ender chest: the lid opens once and shuts behind the last of them
    void run() {
        ServerNetworkHandler owner("Ender chest viewers", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(2, 100, 2);
        level.setBlock(position, VanillaBlocks::fromIdentifier("minecraft:ender_chest")->toBlockState(), false);
        EnderChestBlockActor &chest = level.getBlockActors().getOrCreate<EnderChestBlockActor>(position);

        const NetworkIdentifier firstId(std::string("10.0.0.1"), 19132);
        const NetworkIdentifier secondId(std::string("10.0.0.2"), 19132);
        ServerPlayer &first = join(owner, level, firstId, 4101, "First");
        ServerPlayer &second = join(owner, level, secondId, 4102, "Second");

        // the lid values the first player is sent
        std::vector<int32_t> lid;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &to, std::string &bytes) {
            ReadOnlyBinaryStream header(bytes);
            unsigned char sender = 0, recipient = 0;
            if (!(to == firstId) || Packet::peekId(header, sender, recipient) != MinecraftPacketIds::BlockEvent)
                return false;
            ReadOnlyBinaryStream body(bytes);
            BlockEventPacket event;
            event.readHeader(body);
            event.read(body, owner.getCodecContext());
            if (event.mBlockPosition == position && event.mEventType == 1)
                lid.push_back(event.mEventData);
            return false;
        });

        require(first.getInventoryManager().onClientOpenBlockContainer(position, ContainerType::Container),
                "the first player could not open the ender chest");
        require(chest.getViewerCount() == 1 && lid == std::vector<int32_t>{1},
                "the first player did not open the lid once");

        require(second.getInventoryManager().onClientOpenBlockContainer(position, ContainerType::Container),
                "the second player could not open the ender chest");
        require(chest.getViewerCount() == 2 && lid.size() == 1, "a second player opened the lid again");

        ContainerClosePacket close;
        close.mWindowId = (int8_t) InventoryManager::CONTAINER_ID_NONE;
        InventoryHandler::handleContainerClose(first, close);
        require(chest.getViewerCount() == 1 && lid.size() == 1,
                "the lid shut while the second player was still looking in");

        ServerTickProbe::leave(owner, secondId);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(chest.getViewerCount() == 0, "the ender chest still counts the player who left");
        require(lid == std::vector<int32_t>({1, 0}), "the lid did not shut behind the last player");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-ender-chest-viewers-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Ender chest viewer regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    if (result == 0)
        std::cout << "Ender chest viewer checks passed\n";
    return result;
}
