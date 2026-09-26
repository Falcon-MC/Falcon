#include "Block/Actor/FurnaceBlockActor.h"

#include "Inventory/InventoryManager.h"

#include <cmath>
#include <random>

namespace {
    const char *TAG_KIND = "FurnaceKind";
    const char *TAG_BURN_TIME = "BurnTime";
    const char *TAG_BURN_DURATION = "BurnDuration";
    const char *TAG_COOK_TIME = "CookTime";
    const char *TAG_STORED_EXPERIENCE = "StoredXpInt";

    std::mt19937 &experienceRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }
}

Tag FurnaceBlockActor::saveNbt() const {
    Tag data = mInventory.saveNbt();
    data.putInt(TAG_KIND, (int32_t) mKind);
    data.putShort(TAG_BURN_TIME, (int16_t) mBurnTime);
    data.putShort(TAG_BURN_DURATION, (int16_t) mMaxBurnTime);
    data.putShort(TAG_COOK_TIME, (int16_t) mCookTime);
    data.putShort(TAG_STORED_EXPERIENCE, (int16_t) mStoredExperience);
    return data;
}

void FurnaceBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mKind = (FurnaceKind) data.getInt(TAG_KIND);
    mBurnTime = data.getShort(TAG_BURN_TIME);
    mMaxBurnTime = data.getShort(TAG_BURN_DURATION);
    mCookTime = data.getShort(TAG_COOK_TIME);
    mStoredExperience = (float) data.getShort(TAG_STORED_EXPERIENCE);
    mInventory.loadNbt(data, context);
}

bool FurnaceBlockActor::tick(ServerNetworkHandler &owner) {
    InventoryManager::tickStoredFurnace(owner, *this);
    return true;
}

int32_t FurnaceBlockActor::takeStoredExperience() {
    const float whole = std::floor(mStoredExperience);
    const float fraction = mStoredExperience - whole;
    const bool roundUp = std::uniform_real_distribution<float>(0.0f, 1.0f)(experienceRandom()) < fraction;
    mStoredExperience = 0.0f;
    return (int32_t) whole + (roundUp ? 1 : 0);
}
