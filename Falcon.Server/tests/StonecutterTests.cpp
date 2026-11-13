#include "Network/ServerNetworkHandler.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/StonecutterRecipes.h"
#include "World/Item/CraftingRecipeTable.h"

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const StonecutterRecipeData *findRecipe(const char *recipeId) {
        const StonecutterRecipeData *recipes = CraftingRecipeTable::getStonecutterRecipes();
        for (size_t index = 0; index < CraftingRecipeTable::getStonecutterRecipeCount(); ++index) {
            if (std::strcmp(recipes[index].mRecipeId, recipeId) == 0)
                return &recipes[index];
        }
        return nullptr;
    }

    void checkRecipeTable() {
        require(CraftingRecipeTable::getStonecutterRecipeCount() > 0, "no stonecutter recipe was loaded");
        const StonecutterRecipeData *slab = findRecipe("minecraft:stonecutter_andesite_slab");
        require(slab != nullptr, "the andesite slab stonecutter recipe is missing");
        require(std::string(slab->mInputItemId) == "minecraft:andesite" && slab->mInputAuxValue == -1,
                "the andesite slab recipe reads the wrong input");
        require(std::string(slab->mOutputItemId) == "minecraft:andesite_slab" && slab->mOutputCount == 2,
                "the andesite slab recipe reads the wrong output");
        require(std::string(slab->mUuid) == "82f4e0ca-2e60-46ac-84e8-0dc309bfdb07",
                "the andesite slab recipe reads the wrong uuid");
    }

    void checkCraftingData(ServerNetworkHandler &owner) {
        const std::unordered_map<int32_t, uint32_t> &indices = owner.getStonecutterRecipeIndices();
        require(!indices.empty(), "no stonecutter recipe reached the crafting data");

        const CraftingDataPacket &packet = owner.getCachedCraftingData();
        int32_t lastOther = 0;
        int32_t firstStonecutter = INT32_MAX;
        size_t stonecutterCount = 0;
        auto visit = [&](const CraftingRecipeEntry &entry) {
            if (entry.mBlockName != "stonecutter") {
                lastOther = std::max(lastOther, entry.mRecipeNetId);
                return;
            }
            ++stonecutterCount;
            firstStonecutter = std::min(firstStonecutter, entry.mRecipeNetId);
            require(indices.count(entry.mRecipeNetId) == 1, "a stonecutter recipe has no table index");
            require(entry.mWidth == 0 && entry.mInputs.size() == 1 && entry.mOutputs.size() == 1,
                    "a stonecutter recipe is not a one-input shapeless recipe");
        };
        for (const CraftingRecipeEntry &entry: packet.mShapedRecipes)
            visit(entry);
        for (const CraftingRecipeEntry &entry: packet.mShapelessRecipes)
            visit(entry);
        for (const MultiRecipeEntry &entry: packet.mMultiRecipes)
            lastOther = std::max(lastOther, entry.mRecipeNetId);

        require(stonecutterCount == indices.size(), "stonecutter recipes and their indices disagree");
        require(firstStonecutter == lastOther + 1, "stonecutter recipes do not follow every other recipe");

        const auto andesite = std::find_if(
                packet.mShapelessRecipes.begin(), packet.mShapelessRecipes.end(),
                [&](const CraftingRecipeEntry &entry) {
                    const auto it = indices.find(entry.mRecipeNetId);
                    return entry.mBlockName == "stonecutter" && it != indices.end()
                           && std::string(CraftingRecipeTable::getStonecutterRecipes()[it->second].mRecipeId)
                                      == "minecraft:stonecutter_andesite_slab";
                });
        require(andesite != packet.mShapelessRecipes.end(), "the andesite slab recipe is not sent");
        require(andesite->mInputs.front().mItemId == "minecraft:andesite"
                        && andesite->mInputs.front().mAuxValue == 0x7fff && andesite->mOutputs.front().mCount == 2,
                "the andesite slab recipe is sent wrong");
    }

    int32_t netIdOf(ServerNetworkHandler &owner, const char *recipeId) {
        for (const auto &entry: owner.getStonecutterRecipeIndices()) {
            if (std::strcmp(CraftingRecipeTable::getStonecutterRecipes()[entry.second].mRecipeId, recipeId) == 0)
                return entry.first;
        }
        throw std::runtime_error(std::string("no network id for ") + recipeId);
    }

    ItemStack makeItem(ServerNetworkHandler &owner, const char *identifier, int count) {
        ItemStack item = ItemStack::air();
        item.mDefinition = owner.getItemDefinitions().getDefinition(identifier);
        item.mBlockDefinition = owner.getBlockDefinitions().getDefinition(identifier);
        item.mCount = count;
        require(item.mDefinition != nullptr, std::string("unknown item ") + identifier);
        return item;
    }

    ItemStackRequestSlotData slotOf(ContainerSlotType container, int32_t slot) {
        ItemStackRequestSlotData data;
        data.mContainer = container;
        data.mContainerName.mContainer = container;
        data.mSlot = slot;
        return data;
    }

    ItemStackRequest cutRequest(int32_t recipeNetId, int32_t crafts, int32_t consumed, int32_t taken) {
        ItemStackRequest request;
        request.mRequestId = -1;

        ItemStackRequestAction craft;
        craft.mType = ItemStackRequestActionType::CraftRecipe;
        craft.mRecipeNetworkId = recipeNetId;
        craft.mNumberOfRequestedCrafts = crafts;
        request.mActions.push_back(craft);

        ItemStackRequestAction consume;
        consume.mType = ItemStackRequestActionType::Consume;
        consume.mCount = consumed;
        consume.mSource = slotOf(ContainerSlotType::StonecutterInput, PlayerInventory::STONECUTTER_INPUT_NETWORK_SLOT);
        request.mActions.push_back(consume);

        ItemStackRequestAction take;
        take.mType = ItemStackRequestActionType::Take;
        take.mCount = taken;
        take.mSource = slotOf(ContainerSlotType::CreatedOutput, 50);
        take.mDestination = slotOf(ContainerSlotType::Inventory, 0);
        request.mActions.push_back(take);
        return request;
    }

    int run(ServerNetworkHandler &owner, PlayerInventory &inventory, const ItemStackRequest &request,
            bool stonecutterOpen) {
        const std::vector<CreativeItemData> creative;
        return ItemStackRequestHandler::execute(
                       inventory, request, creative, owner.getRecipeOutputs(), owner.getRecipeSourceIndices(), false,
                       false, nullptr, nullptr, &owner.getCodecContext(), nullptr, 0, false, nullptr, false, nullptr,
                       nullptr, nullptr, nullptr, stonecutterOpen ? &owner.getStonecutterRecipeIndices() : nullptr)
                .mResult;
    }

    void checkInputMatching(ServerNetworkHandler &owner) {
        const StonecutterRecipeData *chiseled = findRecipe(
                "minecraft:chiseled_deepslate_from_cobbled_deepslate_stonecutting");
        require(chiseled != nullptr && std::string(chiseled->mInputItemId) == "minecraft:cobbled_deepslate:0",
                "the chiseled deepslate recipe no longer names its input with a data value");
        require(StonecutterRecipes::matchesInput(*chiseled, makeItem(owner, "minecraft:cobbled_deepslate", 1)),
                "an input named with a data value does not match its item");
        require(!StonecutterRecipes::matchesInput(*chiseled, makeItem(owner, "minecraft:deepslate", 1)),
                "another item matches the chiseled deepslate recipe");
    }

    void checkRequests(ServerNetworkHandler &owner) {
        const int32_t slab = netIdOf(owner, "minecraft:stonecutter_andesite_slab");
        const int32_t chiseled = netIdOf(owner, "minecraft:chiseled_deepslate_from_cobbled_deepslate_stonecutting");

        PlayerInventory inventory;
        inventory.setStonecutterInput(makeItem(owner, "minecraft:andesite", 5));
        require(run(owner, inventory, cutRequest(slab, 2, 2, 4), true) == ItemStackRequestHandler::RESULT_OK,
                "cutting two andesite into slabs was refused");
        require(inventory.getStonecutterInput().mCount == 3, "the cut did not take two andesite");
        require(inventory.getItem(0).mDefinition != nullptr
                        && inventory.getItem(0).mDefinition->getIdentifier() == "minecraft:andesite_slab"
                        && inventory.getItem(0).mCount == 4,
                "the cut did not give four andesite slabs");

        const ItemStackRequest wrongRecipe = cutRequest(chiseled, 1, 1, 1);
        require(run(owner, inventory, wrongRecipe, true) == ItemStackRequestHandler::RESULT_ERROR,
                "a recipe for another input was crafted");
        require(run(owner, inventory, cutRequest(slab, 1, 2, 2), true) == ItemStackRequestHandler::RESULT_ERROR,
                "a cut consuming more than its recipe was accepted");
        require(run(owner, inventory, cutRequest(slab, 4, 4, 8), true) == ItemStackRequestHandler::RESULT_ERROR,
                "a cut needing more input than the slot holds was accepted");
        require(run(owner, inventory, cutRequest(slab, 1, 1, 2), false) == ItemStackRequestHandler::RESULT_ERROR,
                "a cut was accepted with no stonecutter open");
        require(inventory.getStonecutterInput().mCount == 3 && inventory.getItem(0).mCount == 4,
                "a refused cut changed the inventory");
    }
}

int main() {
    try {
        checkRecipeTable();
        ServerNetworkHandler owner("Stonecutter", "", 1);
        checkCraftingData(owner);
        checkInputMatching(owner);
        checkRequests(owner);
    } catch (const std::exception &error) {
        std::clog << "Stonecutter regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
