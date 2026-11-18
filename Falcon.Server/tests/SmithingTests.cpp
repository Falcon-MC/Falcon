#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/SharedPacket.h"
#include "Network/Handler/LoginHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "World/Level/Block/SmithingTableBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Level/Level.h"
#include "World/Level/WorldManager.h"

#include <algorithm>
#include <array>
#include <iostream>
#include <set>
#include <stdexcept>

namespace {
    using Slot = ContainerSlotType;
    constexpr std::array INPUTS{Slot::SmithingTableInput, Slot::SmithingTableMaterial, Slot::SmithingTableTemplate};

    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack makeItem(ServerNetworkHandler &owner, const char *name, int count) {
        ItemStack item = ItemStack::air();
        item.mDefinition = owner.getItemDefinitions().getDefinition(name);
        require(item.mDefinition != nullptr, "missing item definition");
        item.mCount = count;
        return item;
    }

    ItemStackRequestSlotData slot(Slot container, int index) {
        ItemStackRequestSlotData result;
        result.mContainer = container;
        result.mContainerName.mContainer = container;
        result.mSlot = index;
        return result;
    }

    ItemStackRequest craft(int32_t recipeId, Slot outputSlot) {
        ItemStackRequest result;
        ItemStackRequestAction recipe;
        recipe.mType = ItemStackRequestActionType::CraftRecipe;
        recipe.mNumberOfRequestedCrafts = 1;
        recipe.mRecipeNetworkId = recipeId;
        result.mActions.push_back(recipe);

        for (int i = 0; i < PlayerInventory::SMITHING_SIZE; ++i) {
            ItemStackRequestAction consume;
            consume.mType = ItemStackRequestActionType::Consume;
            consume.mCount = 1;
            consume.mSource = slot(INPUTS[(size_t) i], PlayerInventory::SMITHING_FIRST_SLOT + i);
            result.mActions.push_back(consume);
        }

        ItemStackRequestAction take;
        take.mType = ItemStackRequestActionType::Take;
        take.mCount = 1;
        take.mSource = slot(outputSlot, 50);
        take.mDestination = slot(Slot::Inventory, 0);
        result.mActions.push_back(take);
        return result;
    }

    int run(ServerNetworkHandler &owner, PlayerInventory &inventory, const ItemStackRequest &request, bool open = true) {
        return ItemStackRequestHandler::execute(
                inventory, request, {}, owner.getRecipeOutputs(), owner.getRecipeSourceIndices(), false, false,
                nullptr, nullptr, &owner.getCodecContext(), nullptr, 0, false, nullptr, false, nullptr, nullptr, nullptr,
                nullptr, nullptr, nullptr, nullptr, nullptr, open ? &owner.getCachedCraftingData() : nullptr, 2225)
                .mResult;
    }

    void checkRecipes(ServerNetworkHandler &owner) {
        require(VanillaBlocks::getAs<SmithingTableBlock>("minecraft:smithing_table") != nullptr,
                "the smithing table block was not registered");
        const NetworkIdentifier id;
        ServerPlayer player(id, 4001);
        LoginHandler::sendCraftingData(owner, player);
        const auto &context = owner.getCodecContext(id);
        require(context.getCodec().getProtocolVersion() == 2225, "expected the Bedrock 26.60 codec");
        const auto &catalog = owner.getCachedCraftingData();
        const auto &shared = owner.getSharedLoginPacket(
                ServerNetworkHandler::SharedLoginPacket::CraftingData, context.getCodec().getProtocolVersion());
        require(shared != nullptr, "login did not produce a crafting packet");

        ReadOnlyBinaryStream stream(shared->getPacket());
        // This is a server-generated catalog. Give the test decoder room for the full shaped recipe list.
        auto settings = stream.getEncodingSettings();
        settings.mMaxListSize = std::max(settings.mMaxListSize, catalog.mShapedRecipes.size());
        stream.setEncodingSettings(settings);
        require(stream.getUnsignedVarInt() == (uint32_t) CraftingDataPacket::ID,
                "incorrect crafting data packet header");
        CraftingDataPacket packet;
        packet.read(stream, context);
        require(stream.feof(), "crafting packet has unread bytes");
        require(packet.mCleanRecipes && packet.mShapedRecipes.size() == catalog.mShapedRecipes.size(),
                "the login catalog lost its shaped recipes");
        require(packet.mSmithingTransformRecipes.size() == 12 && packet.mSmithingTrimRecipes.size() == 1,
                "vanilla smithing recipes did not reach crafting data");
        std::set<int32_t> ids;
        for (const auto &entry: catalog.mShapedRecipes)
            ids.insert(entry.mRecipeNetId);
        for (const auto &entry: catalog.mShapelessRecipes)
            ids.insert(entry.mRecipeNetId);
        for (const auto &entry: catalog.mMultiRecipes)
            ids.insert(entry.mRecipeNetId);
        for (const auto &entry: packet.mSmithingTransformRecipes)
            require(entry.mRecipeNetId > 0 && ids.insert(entry.mRecipeNetId).second, "colliding smithing recipe ID");
        const auto &trim = packet.mSmithingTrimRecipes.front();
        require(ids.insert(trim.mRecipeNetId).second && trim.mInput.mItemTag == "minecraft:trimmable_armors"
                        && trim.mTemplate.mItemTag == "minecraft:trim_templates"
                        && trim.mAddition.mItemTag == "minecraft:trim_materials",
                "armor trim recipe lost its ingredient tags");
    }

    void checkWindow(ServerNetworkHandler &owner) {
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(2, 100, 2);
        const auto *block = VanillaBlocks::getAs<SmithingTableBlock>("minecraft:smithing_table");
        require(block != nullptr, "smithing table is missing");
        level.setBlock(position, block->toBlockState(), false);

        const NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.setName("Smith");
        player.moveToLevel(level);
        player.setPosition(Vector3f(2.5f, 100.0f, 3.5f));
        auto &manager = player.getInventoryManager();
        manager.attach(&player, &owner);
        require(block->onInteract(owner, player, position, block->toBlockState()) && manager.isSmithingOpen()
                        && manager.getCurrentWindowType() == ContainerType::SmithingTable,
                "normal block interaction did not open the smithing window");

        auto &inventory = player.getInventory();
        inventory.setSmithingItem(0, makeItem(owner, "minecraft:diamond_sword", 1));
        inventory.setSmithingItem(1, makeItem(owner, "minecraft:netherite_ingot", 2));
        inventory.setSmithingItem(2, makeItem(owner, "minecraft:netherite_upgrade_smithing_template", 3));
        const auto &recipes = owner.getCachedCraftingData().mSmithingTransformRecipes;
        const auto recipe = std::find_if(recipes.begin(), recipes.end(), [](const auto &entry) {
            return entry.mRecipeId == "minecraft:smithing_netherite_sword";
        });
        require(recipe != recipes.end(), "missing sword upgrade");
        ItemStackRequestPacket outgoing;
        auto request = craft(recipe->mRecipeNetId, Slot::CreatedOutput);
        request.mRequestId = -1;
        outgoing.mRequests.push_back(request);
        BinaryStream buffer;
        outgoing.write(buffer, owner.getCodecContext());
        ReadOnlyBinaryStream stream(buffer.getBuffer());
        ItemStackRequestPacket incoming;
        incoming.read(stream, owner.getCodecContext());

        player.setPosition(Vector3f(18.5f, 100.0f, 3.5f));
        InventoryHandler::handleItemStackRequest(owner, id, player, incoming);
        require(inventory.getItem(0).isAir() && inventory.getSmithingItem(0).mCount == 1
                        && inventory.getSmithingItem(1).mCount == 2 && inventory.getSmithingItem(2).mCount == 3,
                "out-of-reach request changed the smithing inventory");

        player.setPosition(Vector3f(2.5f, 100.0f, 3.5f));
        incoming.mRequests.front().mRequestId = -3;
        InventoryHandler::handleItemStackRequest(owner, id, player, incoming);
        require(!inventory.getItem(0).isAir()
                        && inventory.getItem(0).mDefinition->getIdentifier() == "minecraft:netherite_sword"
                        && inventory.getSmithingItem(0).isAir() && inventory.getSmithingItem(1).mCount == 1
                        && inventory.getSmithingItem(2).mCount == 2,
                "decoded smithing request did not deliver the result and charge the inputs");

        manager.closeAllWindows();

        int returned = 0;
        for (int index = 0; index < PlayerInventory::CONTAINER_SIZE; ++index)
            returned += inventory.getItem(index).mCount;
        require(!manager.isSmithingOpen() && returned == 4 && inventory.getSmithingItem(0).isAir()
                        && inventory.getSmithingItem(1).isAir() && inventory.getSmithingItem(2).isAir(),
                "closing the window did not return the remaining ingredients");
    }

    void checkTransform(ServerNetworkHandler &owner) {
        const auto &recipes = owner.getCachedCraftingData().mSmithingTransformRecipes;
        const auto recipe = std::find_if(recipes.begin(), recipes.end(), [](const auto &entry) {
            return entry.mRecipeId == "minecraft:smithing_netherite_sword";
        });
        require(recipe != recipes.end(), "missing sword upgrade");

        PlayerInventory inventory;
        ItemStack sword = makeItem(owner, "minecraft:diamond_sword", 1);
        sword.mDamage = 230;
        sword.mTag = Tag::ofCompound();
        sword.mTag.putInt("RepairCost", 7);
        inventory.setSmithingItem(0, sword);
        inventory.setSmithingItem(1, makeItem(owner, "minecraft:netherite_ingot", 2));
        inventory.setSmithingItem(2, makeItem(owner, "minecraft:netherite_upgrade_smithing_template", 2));

        const auto request = craft(recipe->mRecipeNetId, Slot::CreatedOutput);
        auto unpaid = request;
        unpaid.mActions.erase(unpaid.mActions.begin() + 3);
        require(run(owner, inventory, unpaid) == ItemStackRequestHandler::RESULT_ERROR, "missing template was accepted");
        auto wrongSlot = request;
        wrongSlot.mActions[2].mSource.mSlot = 53;
        require(run(owner, inventory, wrongSlot) == ItemStackRequestHandler::RESULT_ERROR, "wrong slot was accepted");
        require(run(owner, inventory, request, false) == ItemStackRequestHandler::RESULT_ERROR,
                "craft was accepted with the window closed");
        require(inventory.getItem(0).isAir() && inventory.getSmithingItem(0).mCount == 1
                        && inventory.getSmithingItem(1).mCount == 2 && inventory.getSmithingItem(2).mCount == 2,
                "rejected request did not roll back");

        require(run(owner, inventory, request) == ItemStackRequestHandler::RESULT_OK, "sword upgrade failed");
        const ItemStack &upgraded = inventory.getItem(0);
        require(!upgraded.isAir() && upgraded.mDefinition->getIdentifier() == "minecraft:netherite_sword"
                        && upgraded.mDamage == 230 && upgraded.mTag.getInt("RepairCost") == 7,
                "sword result lost its damage or item data");
        require(inventory.getSmithingItem(0).isAir() && inventory.getSmithingItem(1).mCount == 1
                        && inventory.getSmithingItem(2).mCount == 1,
                "upgrade did not consume exactly one of each ingredient");
    }

    void checkTrim(ServerNetworkHandler &owner) {
        PlayerInventory inventory;
        inventory.setSmithingItem(0, makeItem(owner, "minecraft:copper_helmet", 1));
        inventory.setSmithingItem(1, makeItem(owner, "minecraft:resin_brick", 2));
        inventory.setSmithingItem(2, makeItem(owner, "minecraft:bolt_armor_trim_smithing_template", 2));
        const auto request = craft(owner.getCachedCraftingData().mSmithingTrimRecipes.front().mRecipeNetId,
                                   Slot::SmithingTableResult);
        require(run(owner, inventory, request) == ItemStackRequestHandler::RESULT_OK, "valid trim was refused");
        const ItemStack &helmet = inventory.getItem(0);
        const Tag *trim = helmet.mTag.get("Trim");
        require(trim != nullptr && trim->getString("Pattern") == "bolt" && trim->getString("Material") == "resin",
                "trim has the wrong item data");
        inventory.setSmithingItem(0, helmet);
        inventory.setItem(0, ItemStack::air());
        require(run(owner, inventory, request) == ItemStackRequestHandler::RESULT_ERROR,
                "applying the same trim twice consumed materials");
        require(inventory.getSmithingItem(1).mCount == 1 && inventory.getSmithingItem(2).mCount == 1,
                "refused trim changed the materials");
    }
}

int main() {
    try {
        ServerNetworkHandler owner("Smithing", "", 1);
        owner.getScriptEngine().bindHost(owner);
        LoginHandler::buildCraftingData(owner);
        checkRecipes(owner);
        checkTransform(owner);
        checkTrim(owner);
        checkWindow(owner);
    } catch (const std::exception &error) {
        std::clog << "Smithing regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
