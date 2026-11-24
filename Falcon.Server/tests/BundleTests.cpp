#include "Network/ServerNetworkHandler.h"
#include "World/Inventory/BundleInventory.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"

#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStack makeItem(ServerNetworkHandler &owner, const std::string &identifier, int32_t count) {
        ItemStack item;
        item.mDefinition = owner.getItemDefinitions().getDefinition(identifier);
        item.mBlockDefinition = owner.getBlockDefinitions().getDefinition(identifier);
        item.mCount = count;
        return item;
    }

    ItemStack makeBundle(ServerNetworkHandler &owner, int32_t id, int32_t diamonds) {
        ItemStack bundle = makeItem(owner, "minecraft:bundle", 1);
        bundle.mTag = Tag::ofCompound();
        bundle.mTag.putInt(BundleInventory::TAG_BUNDLE_ID, id);
        std::vector<ItemStack> contents((size_t) BundleInventory::SIZE, ItemStack::air());
        if (diamonds > 0)
            contents[0] = makeItem(owner, "minecraft:diamond", diamonds);
        BundleInventory::writeContents(bundle, contents);
        return bundle;
    }

    int32_t diamondsIn(ServerNetworkHandler &owner, const ItemStack &bundle) {
        int32_t count = 0;
        for (const ItemStack &item: BundleInventory::readContents(bundle, owner.getCodecContext()))
            count += item.isAir() ? 0 : item.mCount;
        return count;
    }

    ItemStackResponseEntry takeOne(ServerNetworkHandler &owner, PlayerInventory &inventory, int32_t bundleId) {
        ItemStackRequestAction take;
        take.mType = ItemStackRequestActionType::Take;
        take.mCount = 1;
        take.mSource.mContainer = ContainerSlotType::DynamicContainer;
        take.mSource.mContainerName.mContainer = ContainerSlotType::DynamicContainer;
        take.mSource.mContainerName.mHasDynamicId = true;
        take.mSource.mContainerName.mDynamicId = bundleId;
        take.mSource.mSlot = 0;
        take.mDestination.mContainer = ContainerSlotType::Inventory;
        take.mDestination.mContainerName.mContainer = ContainerSlotType::Inventory;
        take.mDestination.mSlot = 5;

        ItemStackRequest request;
        request.mRequestId = -1;
        request.mActions.push_back(take);

        const std::vector<CreativeItemData> creative;
        return ItemStackRequestHandler::execute(inventory, request, creative, owner.getRecipeOutputs(),
                                                owner.getRecipeSourceIndices(), false, false, nullptr, nullptr,
                                                &owner.getCodecContext(), nullptr, 0, false, nullptr, false, nullptr,
                                                nullptr, nullptr, nullptr, nullptr, nullptr);
    }

    // ids restart with the server: a bundle kept from before can meet a new one under the same id
    void run(ServerNetworkHandler &owner) {
        PlayerInventory inventory;
        inventory.setItem(0, makeBundle(owner, 7, 8));
        inventory.setItem(1, makeBundle(owner, 7, 0));

        require(takeOne(owner, inventory, 7).mResult != ItemStackRequestHandler::RESULT_OK,
                "a request on an id two bundles share was accepted");
        require(diamondsIn(owner, inventory.getItem(0)) == 8 && diamondsIn(owner, inventory.getItem(1)) == 0
                        && inventory.getItem(5).isAir(),
                "a refused bundle request changed the inventory");

        require(BundleInventory::separateBundleIds(inventory, nullptr), "the shared id was not noticed");
        const int32_t first = BundleInventory::getBundleId(inventory.getItem(0));
        const int32_t second = BundleInventory::getBundleId(inventory.getItem(1));
        require(first != second && first >= 0 && second >= 0, "the two bundles still share an id");
        require(diamondsIn(owner, inventory.getItem(0)) == 8 && diamondsIn(owner, inventory.getItem(1)) == 0,
                "giving a bundle a new id changed its contents");
        require(!BundleInventory::separateBundleIds(inventory, nullptr), "distinct ids were changed again");

        require(takeOne(owner, inventory, first).mResult == ItemStackRequestHandler::RESULT_OK,
                "a request on a bundle with its own id was refused");
        require(diamondsIn(owner, inventory.getItem(0)) == 7 && diamondsIn(owner, inventory.getItem(1)) == 0
                        && !inventory.getItem(5).isAir() && inventory.getItem(5).mCount == 1,
                "taking one item out of a bundle did not move exactly one");
    }
}

int main() {
    try {
        ServerNetworkHandler owner("Bundle", "", 1);
        run(owner);
    } catch (const std::exception &error) {
        std::clog << "Bundle regression failed: " << error.what() << '\n';
        return 1;
    }
    return 0;
}
