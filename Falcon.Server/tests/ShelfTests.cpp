#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/SetBlockCommand.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
#include "World/Level/Block/ContainerBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i SHELF(4, 100, 4);
    // the south face: the slots run along x, the readable band is y 0.25 to 0.6875
    const int FRONT = 3;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &server, const char *name, int count = 1) {
        ItemStack result = server.createItemStack(name, count);
        require(!result.isAir(), "missing item definition");
        return result;
    }

    std::string slotName(Level &level, int slot) {
        ShelfBlockActor *shelf = level.getBlockActors().find<ShelfBlockActor>(SHELF);
        if (shelf == nullptr)
            return "";
        const ItemStack &stored = shelf->getInventory().getContainerItem(slot);
        return stored.isAir() ? "air" : stored.mDefinition->getIdentifier();
    }
}

int main() {
    try {
        ServerNetworkHandler server("Shelf", "", 1);
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        BlockState state;
        require(SetBlockCommand::resolveBlock("minecraft:oak_shelf", state), "no oak shelf");
        state.mStates.putString("minecraft:cardinal_direction", "south");
        level.setBlock(SHELF, state, false);
        const Block *block = VanillaBlocks::fromIdentifier("minecraft:oak_shelf");
        require(block != nullptr, "the shelf block was not registered");

        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4007).first->second;
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.moveToLevel(level);
        player.setPosition(Vector3f(4.5f, 101.0f, 6.5f));
        PlayerInventory &inventory = player.getInventory();

        inventory.setItemInHand(item(server, "minecraft:diamond", 3));
        require(block->onInteractAt(server, player, SHELF, state, FRONT, Vector3f(0.1f, 0.5f, 1.0f)),
                "a diamond was not put on the shelf");
        require(slotName(level, 0) == "minecraft:diamond" && inventory.getItemInHand().isAir(),
                "the whole stack did not go to the left slot");

        require(!block->onInteractAt(server, player, SHELF, state, FRONT, Vector3f(0.5f, 0.5f, 1.0f)),
                "an empty hand on an empty slot did something");
        require(!block->onInteractAt(server, player, SHELF, state, 2, Vector3f(0.1f, 0.5f, 0.0f)),
                "the back of the shelf took an interaction");
        require(!block->onInteractAt(server, player, SHELF, state, FRONT, Vector3f(0.1f, 0.8f, 1.0f)),
                "a click above the items was read");

        inventory.setItemInHand(item(server, "minecraft:stick"));
        require(block->onInteractAt(server, player, SHELF, state, FRONT, Vector3f(0.2f, 0.3f, 1.0f))
                        && slotName(level, 0) == "minecraft:stick"
                        && inventory.getItemInHand().mDefinition->getIdentifier() == "minecraft:diamond",
                "the stick and the diamonds did not change places");

        // the right third
        inventory.setItemInHand(ItemStack::air());
        require(!block->onInteractAt(server, player, SHELF, state, FRONT, Vector3f(0.9f, 0.5f, 1.0f)),
                "an empty right slot was taken from");
        ItemStack locked = item(server, "minecraft:apple");
        locked.mTag = Tag::ofCompound();
        locked.mTag.putByte("minecraft:item_lock", 1);
        inventory.setItemInHand(locked);
        require(!block->onInteractAt(server, player, SHELF, state, FRONT, Vector3f(0.9f, 0.5f, 1.0f)),
                "a locked item went on the shelf");

        // instant building: the shelf takes a copy, the hand keeps its own
        player.setGameType(1);
        inventory.setItemInHand(item(server, "minecraft:apple", 5));
        require(block->onInteractAt(server, player, SHELF, state, FRONT, Vector3f(0.9f, 0.5f, 1.0f))
                        && slotName(level, 2) == "minecraft:apple" && inventory.getItemInHand().mCount == 5,
                "in creative the hand lost its item");

        BlockState powered = state;
        powered.mStates.putByte("powered_bit", 1);
        require(!block->onInteractAt(server, player, SHELF, powered, FRONT, Vector3f(0.5f, 0.5f, 1.0f)),
                "a powered shelf swapped one slot");
        std::cout << "shelf checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Shelf regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
