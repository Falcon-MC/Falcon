#include "Network/Handler/InventoryHandler.h"
#include "Protocol/Packets/InventoryContentPacket.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "Protocol/Packets/ItemStackResponsePacket.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
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

    // a player who leaves with a chest open no longer counts as looking into it
    void run() {
        ServerNetworkHandler owner("Container leave", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(2, 100, 2);
        level.setBlock(position, VanillaBlocks::fromIdentifier("minecraft:chest")->toBlockState(), false);
        ChestBlockActor &chest = level.getBlockActors().getOrCreate<ChestBlockActor>(position);

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.setName("Leaver");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(2.5f, 100.0f, 0.5f));
        player.getInventoryManager().attach(&player, &owner);

        require(player.getInventoryManager().onClientOpenChest(position), "the chest did not open");
        require(chest.getViewerCount() == 1, "the open chest has no viewer");

        player.getInventory().setItem(0, owner.createItemStack("minecraft:stone", 4));
        chest.getContainer()->setContainerItem(0, owner.createItemStack("minecraft:stone", 64));
        const int window = player.getInventoryManager().getCurrentWindowId();
        ItemStackRequestAction place;
        place.mType = ItemStackRequestActionType::Place;
        place.mCount = 1;
        place.mSource.mContainer = place.mSource.mContainerName.mContainer = ContainerSlotType::Inventory;
        place.mSource.mStackNetworkId = player.getInventory().getItem(0).mNetId;
        place.mDestination.mContainer = place.mDestination.mContainerName.mContainer = ContainerSlotType::LevelEntity;
        place.mDestination.mStackNetworkId = chest.getContainer()->getContainerItem(0).mNetId;
        ItemStackRequestAction stage = place;
        stage.mDestination.mContainer = stage.mDestination.mContainerName.mContainer = ContainerSlotType::Inventory;
        stage.mDestination.mSlot = 1;
        stage.mDestination.mStackNetworkId = 0;

        ItemStackRequestAction take = place;
        take.mType = ItemStackRequestActionType::Take;
        std::swap(take.mSource, take.mDestination);
        ItemStackRequestAction split = take;
        split.mDestination.mSlot = 2;
        split.mDestination.mStackNetworkId = 0;
        ItemStackRequestPacket request;
        request.mRequests.resize(2);
        request.mRequests[0].mRequestId = -1;
        request.mRequests[0].mActions = {stage, place};
        request.mRequests[1].mRequestId = -3;
        request.mRequests[1].mActions = {take, split};

        std::vector<ItemStackResponseEntry> responses;
        bool chestRefreshed = false;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &bytes) {
            ReadOnlyBinaryStream header(bytes);
            unsigned char sender = 0, recipient = 0;
            const auto type = Packet::peekId(header, sender, recipient);
            ReadOnlyBinaryStream body(bytes);
            if (type == MinecraftPacketIds::ItemStackResponse) {
                ItemStackResponsePacket response;
                response.readHeader(body);
                response.read(body, owner.getCodecContext());
                responses = std::move(response.mEntries);
            } else if (type == MinecraftPacketIds::InventoryContent) {
                InventoryContentPacket content;
                content.readHeader(body);
                content.read(body, owner.getCodecContext());
                if (content.mContainerId == window && !content.mContents.empty())
                    chestRefreshed = !responses.empty() && content.mContents[0].mCount == 62;
            }
            return false;
        });
        InventoryHandler::handleItemStackRequest(owner, id, player, request);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(responses.size() == 2 && responses[0].mRequestId == -1 && responses[1].mRequestId == -3,
                "the response lost a request or changed their order");
        require(responses[0].mResult == ItemStackRequestHandler::RESULT_ERROR
                        && responses[1].mResult == ItemStackRequestHandler::RESULT_OK,
                "the full chest did not reject only the first request");
        require(player.getInventory().getItem(0).mCount == 5 && player.getInventory().getItem(1).isAir()
                        && player.getInventory().getItem(2).mCount == 1
                        && chest.getContainer()->getContainerItem(0).mCount == 62,
                "the rejected move changed the final stack counts");
        require(chestRefreshed, "the open chest did not resync after the response");

        ServerTickProbe::leave(owner, id);
        require(owner.getPlayers().find(id) == owner.getPlayers().end(), "the player stayed after leaving");
        require(chest.getViewerCount() == 0, "the chest still counts the player who left");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-container-leave-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Container leave regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
