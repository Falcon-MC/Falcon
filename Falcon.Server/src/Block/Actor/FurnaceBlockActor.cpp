#include "Block/Actor/FurnaceBlockActor.h"

#include "Inventory/InventoryManager.h"

namespace {
    const char *TAG_KIND = "FurnaceKind";
    const char *TAG_BURN_TIME = "BurnTime";
    const char *TAG_BURN_DURATION = "BurnDuration";
    const char *TAG_COOK_TIME = "CookTime";
}

Tag FurnaceBlockActor::saveNbt() const {
    Tag data = mInventory.saveNbt();
    data.putInt(TAG_KIND, (int32_t) mKind);
    data.putShort(TAG_BURN_TIME, (int16_t) mBurnTime);
    data.putShort(TAG_BURN_DURATION, (int16_t) mMaxBurnTime);
    data.putShort(TAG_COOK_TIME, (int16_t) mCookTime);
    return data;
}

void FurnaceBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mKind = (FurnaceKind) data.getInt(TAG_KIND);
    mBurnTime = data.getShort(TAG_BURN_TIME);
    mMaxBurnTime = data.getShort(TAG_BURN_DURATION);
    mCookTime = data.getShort(TAG_COOK_TIME);
    mInventory.loadNbt(data, context);
}

bool FurnaceBlockActor::tick(ServerNetworkHandler &owner) {
    InventoryManager::tickStoredFurnace(owner, *this);
    return true;
}
