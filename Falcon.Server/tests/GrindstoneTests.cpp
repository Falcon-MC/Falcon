#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/AnvilRepair.h"
#include "World/Inventory/GrindstoneRepair.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Item/ItemData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Block/GrindstoneBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <iostream>
#include <stdexcept>

namespace {
    using Action = ItemStackRequestActionType;
    using Slot = ContainerSlotType;

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack item(ServerNetworkHandler &server, const char *name) {
        ItemStack result = ItemStack::air();
        result.mDefinition = server.getItemDefinitions().getDefinition(name);
        require(result.mDefinition != nullptr, "missing item definition");
        result.mCount = 1;
        return result;
    }

    ItemStackRequestSlotData slot(Slot type, int index) {
        ItemStackRequestSlotData result;
        result.mContainer = type;
        result.mContainerName.mContainer = type;
        result.mSlot = index;
        return result;
    }

    ItemStackRequest makeRequest() {
        ItemStackRequest request;
        ItemStackRequestAction craft;
        craft.mType = Action::CraftRepairAndDisenchant;
        craft.mNumberOfRequestedCrafts = 1;
        craft.mRepairCost = 1;
        request.mActions.push_back(craft);

        ItemStackRequestAction consume;
        consume.mType = Action::Consume;
        consume.mCount = 1;
        consume.mSource = slot(Slot::GrindstoneInput, PlayerInventory::GRINDSTONE_FIRST_SLOT);
        request.mActions.push_back(consume);
        consume.mSource = slot(Slot::GrindstoneAdditional, PlayerInventory::GRINDSTONE_FIRST_SLOT + 1);
        request.mActions.push_back(consume);

        ItemStackRequestAction take;
        take.mType = Action::Take;
        take.mCount = 1;
        take.mSource = slot(Slot::CreatedOutput, 50);
        take.mDestination = slot(Slot::Inventory, 0);
        request.mActions.push_back(take);
        return request;
    }

    int execute(ServerNetworkHandler &server, PlayerInventory &inventory, const ItemStackRequest &request,
                bool open, int32_t &xpCost) {
        return ItemStackRequestHandler::execute(
                inventory, request, {}, {}, {}, false, false, nullptr, nullptr, &server.getCodecContext(),
                nullptr, 0, false, nullptr, false, nullptr, nullptr, nullptr, nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr, 2225, open, &xpCost)
                .mResult;
    }

    void checkTransaction(ServerNetworkHandler &server) {
        PlayerInventory inventory;
        ItemStack sword = item(server, "minecraft:diamond_sword");
        const int maxDamage = ItemDataTable::find("minecraft:diamond_sword")->mMaxDurability;
        sword.mDamage = maxDamage - 100;
        ItemEnchantments::write(sword, {{EnchantmentIds::SHARPNESS, 2}, {EnchantmentIds::VANISHING, 1}});
        sword.mTag.putInt(AnvilRepair::TAG_REPAIR_COST, 7);
        sword.mTag.putInt("Damage", sword.mDamage);
        sword.mUserData = "cached wire data";
        inventory.setGrindstoneItem(0, sword);
        inventory.setGrindstoneItem(1, sword);

        const auto request = makeRequest();
        int32_t xpCost = -1;
        auto incomplete = request;
        incomplete.mActions.erase(incomplete.mActions.begin() + 2);
        require(execute(server, inventory, incomplete, true, xpCost) == ItemStackRequestHandler::RESULT_ERROR,
                "a request with one unpaid input was accepted");
        auto wrongSlot = request;
        wrongSlot.mActions[2].mSource.mSlot = PlayerInventory::GRINDSTONE_FIRST_SLOT;
        require(execute(server, inventory, wrongSlot, true, xpCost) == ItemStackRequestHandler::RESULT_ERROR,
                "an additional input with the wrong slot number was accepted");
        auto untaken = request;
        untaken.mActions.pop_back();
        require(execute(server, inventory, untaken, true, xpCost) == ItemStackRequestHandler::RESULT_ERROR,
                "inputs were consumed for an untaken result");
        require(execute(server, inventory, request, false, xpCost) == ItemStackRequestHandler::RESULT_ERROR,
                "a closed grindstone accepted a request");
        require(xpCost == -1 && inventory.getItem(0).isAir() && inventory.getGrindstoneItem(0).mCount == 1
                        && inventory.getGrindstoneItem(1).mCount == 1,
                "a rejected request changed the inventory or granted experience");

        require(execute(server, inventory, request, true, xpCost) == ItemStackRequestHandler::RESULT_OK,
                "a valid grindstone request failed");
        const ItemStack &result = inventory.getItem(0);
        require(inventory.getGrindstoneItem(0).isAir() && inventory.getGrindstoneItem(1).isAir()
                        && result.mCount == 1,
                "the inputs and output did not commit together");
        require(result.mDamage == maxDamage - 200 - maxDamage / 20
                        && ItemEnchantments::getLevel(result, EnchantmentIds::SHARPNESS) == 0
                        && ItemEnchantments::getLevel(result, EnchantmentIds::VANISHING) == 1
                        && AnvilRepair::getRepairCost(result) == 1 && xpCost == 24
                        && result.mTag.get("Damage") == nullptr && result.mUserData.empty(),
                "wrong durability, curse, repair cost, or experience");
        xpCost = -1;
        require(execute(server, inventory, request, true, xpCost) == ItemStackRequestHandler::RESULT_ERROR
                        && result.mCount == 1 && xpCost == -1,
                "a replay delivered a second result or more experience");
    }

    void checkWindow(ServerNetworkHandler &server, const GrindstoneBlock &block) {
        Level &level = server.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(2, 100, 2);
        const BlockState state = block.toBlockState();
        level.setBlock(position, state, false);

        const NetworkIdentifier id;
        ServerPlayer &player = server.getPlayers().try_emplace(id, id, 4001).first->second;
        player.setName("GrindstoneTest");
        player.moveToLevel(level);
        player.setPosition(Vector3f(2.5f, 100.0f, 3.5f));
        auto &manager = player.getInventoryManager();
        manager.attach(&player, &server);
        require(block.onInteract(server, player, position, state) && manager.isGrindstoneOpen()
                        && manager.getCurrentWindowType() == ContainerType::Grindstone,
                "normal interaction did not open a grindstone window");

        auto &inventory = player.getInventory();
        ItemStack sword = item(server, "minecraft:diamond_sword");
        sword.mDamage = 300;
        ItemEnchantments::write(sword, {{EnchantmentIds::VANISHING, 1}});
        inventory.setGrindstoneItem(0, sword);
        inventory.setGrindstoneItem(1, sword);
        ItemStackRequestPacket outgoing;
        outgoing.mRequests.push_back(makeRequest());
        outgoing.mRequests.front().mRequestId = -1;
        BinaryStream buffer;
        outgoing.write(buffer, server.getCodecContext());
        ReadOnlyBinaryStream stream(buffer.getBuffer());
        ItemStackRequestPacket incoming;
        incoming.read(stream, server.getCodecContext());
        InventoryHandler::handleItemStackRequest(server, id, player, incoming);
        require(inventory.getItem(0).mCount == 1 && inventory.getGrindstoneItem(0).isAir()
                        && inventory.getGrindstoneItem(1).isAir(),
                "decoded grindstone request did not exchange the two inputs for one result");

        inventory.setGrindstoneItem(0, sword);
        player.setPosition(Vector3f(18.5f, 100.0f, 3.5f));
        incoming.mRequests.front().mRequestId = -3;
        InventoryHandler::handleItemStackRequest(server, id, player, incoming);
        require(!manager.isGrindstoneOpen() && inventory.getGrindstoneItem(0).isAir()
                        && inventory.getItem(0).mCount == 1 && inventory.getItem(1).mCount == 1,
                "leaving reach did not close the screen and return its input once");
    }

    void checkResults(ServerNetworkHandler &server) {
        const auto book = server.getItemDefinitions().getDefinition("minecraft:book");
        ItemStack enchanted = item(server, "minecraft:enchanted_book");
        ItemEnchantments::write(enchanted, {{EnchantmentIds::SHARPNESS, 2}});
        const auto result = GrindstoneRepair::createResult(ItemStack::air(), enchanted, book);
        require(!result.mItem.isAir() && result.mItem.mDefinition == book && result.mExperienceCost == 12,
                "disenchanting a lone book in the additional slot did not return a book");
        ItemEnchantments::write(enchanted, {{EnchantmentIds::VANISHING, 1}});
        const auto cursed = GrindstoneRepair::createResult(enchanted, ItemStack::air(), book);
        require(cursed.mItem.isAir()
                        || (cursed.mItem.mDefinition == enchanted.mDefinition && cursed.mExperienceCost == 0),
                "a cursed book lost its curse or produced experience");

        ItemStack tool = item(server, "minecraft:diamond_sword");
        require(GrindstoneRepair::createResult(tool, ItemStack::air(), book).mItem.isAir(),
                "a plain tool offered a result by itself");
        tool.mTag = Tag::ofCompound();
        tool.mTag.putByte("Unbreakable", 1);
        require(GrindstoneRepair::createResult(tool, tool, book).mItem.isAir(),
                "two unbreakable tools were accepted for repair");
    }
}

int main() {
    try {
        ServerNetworkHandler server("Grindstone", "", 1);
        const auto *block = VanillaBlocks::getAs<GrindstoneBlock>("minecraft:grindstone");
        require(block != nullptr, "the grindstone block was not registered");
        checkTransaction(server);
        checkResults(server);
        checkWindow(server, *block);
    } catch (const std::exception &error) {
        std::clog << "Grindstone regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
