#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackNbt.h"
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

    const Tag *chargedItemOf(const ItemStack &crossbow) {
        return crossbow.mTag.getType() == Tag::Type::Compound ? crossbow.mTag.get("chargedItem") : nullptr;
    }

    // a full charge loads one arrow and ends the use: shooting must not reload on its own
    void run() {
        ServerNetworkHandler owner("Crossbow", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 3002).first->second;
        player.setName("Crossbowman");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        player.setGameType((int32_t) GameType::Survival);

        PlayerInventory &inventory = player.getInventory();
        inventory.setItem(0, owner.createItemStack("minecraft:crossbow", 1));
        inventory.setItem(9, owner.createItemStack("minecraft:arrow", 2));
        inventory.setSelectedSlot(0);

        owner._useHeldItem(player);
        require(player.getFlags().get(ActorFlag::UsingItem), "the crossbow did not start loading");
        ServerTickProbe::advance(owner, 25);
        owner._tickItemUse(player);

        const Tag *charged = chargedItemOf(inventory.getItemInHand());
        require(charged != nullptr, "a full charge did not load the crossbow");
        const ItemStack arrow = ItemStackNbt::read(*charged, owner.getCodecContext());
        require(!arrow.isAir() && arrow.mCount == 1, "the loaded arrow is not saved as a one arrow stack");
        require(!player.getFlags().get(ActorFlag::UsingItem), "the use outlived the charge");
        require(inventory.getItem(9).mCount == 1, "loading did not take one arrow");

        ServerTickProbe::advance(owner, 6);
        owner._useHeldItem(player);
        require(owner.getActors().size() == 1, "the loaded crossbow did not shoot one arrow");
        const ItemStack &pickup = owner.getActors().begin()->second->getProjectileData().mPickupItem;
        require(!pickup.isAir() && pickup.mCount == 1, "the survival shot gives no arrow back on pickup");
        require(chargedItemOf(inventory.getItemInHand()) == nullptr, "the shot left the crossbow loaded");

        ServerTickProbe::advance(owner, 1);
        owner._tickItemUse(player);
        require(chargedItemOf(inventory.getItemInHand()) == nullptr, "the crossbow reloaded without a new charge");
        require(inventory.getItem(9).mCount == 1, "an arrow was taken without a new charge");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-crossbow-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Crossbow regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
