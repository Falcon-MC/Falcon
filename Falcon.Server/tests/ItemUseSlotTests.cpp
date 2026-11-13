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

class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a drawn bow switched for a golden apple must not hand the apple its elapsed time
    void run() {
        ServerNetworkHandler owner("Item use slot", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 3001).first->second;
        player.setName("Archer");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));

        PlayerInventory &inventory = player.getInventory();
        inventory.setItem(0, owner.createItemStack("minecraft:bow", 1));
        inventory.setItem(1, owner.createItemStack("minecraft:golden_apple", 2));
        inventory.setItem(9, owner.createItemStack("minecraft:arrow", 16));
        inventory.setSelectedSlot(0);

        owner._useHeldItem(player);
        require(player.getFlags().get(ActorFlag::UsingItem), "the bow did not start drawing");
        ServerTickProbe::advance(owner, 40);

        inventory.setSelectedSlot(1);
        owner._tickItemUse(player);
        require(!player.getFlags().get(ActorFlag::UsingItem), "the use survived the slot change");
        require(inventory.getItem(1).mCount == 2, "the apple was eaten with the bow's draw time");

        owner._completeItemUse(player, (int32_t) inventory.getItem(1).mDefinition->getRuntimeId());
        require(inventory.getItem(1).mCount == 2, "a completed use ate the apple without eating it");

        // a use that keeps its item still completes
        owner._useHeldItem(player);
        require(player.getFlags().get(ActorFlag::UsingItem), "the apple did not start being eaten");
        ServerTickProbe::advance(owner, 40);
        owner._tickItemUse(player);
        require(inventory.getItem(1).mCount == 1, "a full use of the apple did not eat it");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-item-use-slot-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Item use slot regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
