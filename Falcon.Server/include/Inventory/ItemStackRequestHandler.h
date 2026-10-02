#pragma once

#include "Inventory/PlayerInventory.h"
#include "Inventory/CraftingManager.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Packets/CreativeContentPacket.h"
#include "Protocol/Packets/ItemStackResponsePacket.h"
#include "Protocol/Types/ItemStackRequest.h"

#include <cstdint>
#include <vector>

class BundleSyncData {
public:
    int32_t mBundleId = 0;
    ContainerSlotType mOwnerContainer = ContainerSlotType::Unknown;
    int mOwnerSlot = 0;
    std::vector<ItemStack> mContents;
};

/**
 * Validates and applies one client item stack request (moves, swaps, drops, crafting,
 * creative picks, bundles) atomically, and builds the response listing the slots the
 * client must resync.
 */
class ItemStackRequestHandler {
public:
    static const int RESULT_OK = 0;
    static const int RESULT_ERROR = 1;

    static ItemStackResponseEntry execute(PlayerInventory &inventory, const ItemStackRequest &request,
                                          const std::vector<CreativeItemData> &creativeItems,
                                          const std::vector<ItemStack> &recipeOutputs,
                                          const std::vector<uint32_t> &recipeSourceIndices,
                                          bool craftingTableOpen,
                                          bool furnaceOpen,
                                          Container *openContainer = nullptr,
                                          std::vector<ItemStack> *outDroppedItems = nullptr,
                                          const PacketCodecContext *codecContext = nullptr,
                                          std::vector<BundleSyncData> *outBundles = nullptr,
                                          int32_t playerXpLevel = 0,
                                          bool creativeMode = false,
                                          int32_t *outEnchantLevelsConsumed = nullptr);
};
