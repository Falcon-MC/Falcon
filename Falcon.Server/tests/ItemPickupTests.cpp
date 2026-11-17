#include "Network/Handler/ItemActorHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void partiallyFullInventoryPicksUpAvailableItems() {
        ServerNetworkHandler owner("Item pickup", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 3001).first->second;
        player.setName("Collector");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));

        PlayerInventory &inventory = player.getInventory();
        inventory.setItem(0, owner.createItemStack("minecraft:diamond", 63));
        for (int slot = 1; slot < PlayerInventory::CONTAINER_SIZE; ++slot)
            inventory.setItem(slot, owner.createItemStack("minecraft:dirt", 64));

        ItemActor *dropped = owner.dropItem(level, Vector3f(0.5f, 101.0f, 0.5f),
                                            owner.createItemStack("minecraft:diamond", 4), Vector3f(), 0);
        require(dropped != nullptr, "could not create the dropped stack");

        ItemActorHandler::tickItemActors(owner);

        require(inventory.getItem(0).mCount == 64, "the available inventory slot was not filled");
        require(owner.getItemEntities().size() == 1 && !dropped->isRemoved(),
                "the partially picked-up item entity was removed");
        require(dropped->getItem().mCount == 3, "the uncollected items did not remain on the ground");

        ItemActorHandler::tickItemActors(owner);
        require(owner.getItemEntities().size() == 1 && dropped->getItem().mCount == 3,
                "a full inventory lost the uncollected items");

        inventory.setItem(1, ItemStack::air());
        ItemActorHandler::tickItemActors(owner);
        require(owner.getItemEntities().empty(), "the remaining items were not picked up when space opened");
        require(inventory.getItem(0).mCount == 64 && inventory.getItem(1).mCount == 3,
                "partial pickup duplicated or lost items");
    }
}

int main() {
    try {
        partiallyFullInventoryPicksUpAvailableItems();
        std::cout << "item pickup regressions passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
