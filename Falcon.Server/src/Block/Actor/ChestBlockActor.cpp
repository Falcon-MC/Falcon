#include "Block/Actor/ChestBlockActor.h"

#include "Block/BlockActorStore.h"
#include "Level/Level.h"

namespace {
    const char *TAG_PAIR_X = "pairx";
    const char *TAG_PAIR_Z = "pairz";
    const char *TAG_PAIR_LEAD = "pairlead";

    bool isPrimaryOf(const Vector3i &first, const Vector3i &second) {
        return (first.x + (second.z << 15)) <= (second.x + (first.z << 15));
    }
}

Tag ChestBlockActor::saveNbt() const {
    Tag data = mInventory.saveNbt();

    if (mPaired) {
        data.putInt(TAG_PAIR_X, mPairPosition.x);
        data.putInt(TAG_PAIR_Z, mPairPosition.z);
        data.putByte(TAG_PAIR_LEAD, mPairLead ? 1 : 0);
    }

    return data;
}

Tag ChestBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    if (mPaired) {
        data.putInt(TAG_PAIR_X, mPairPosition.x);
        data.putInt(TAG_PAIR_Z, mPairPosition.z);
    }
    return data;
}

void ChestBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    mPaired = data.get(TAG_PAIR_X) != nullptr && data.get(TAG_PAIR_Z) != nullptr;
    if (mPaired) {
        mPairPosition = Vector3i(data.getInt(TAG_PAIR_X), mPosition.y, data.getInt(TAG_PAIR_Z));
        mPairLead = data.getByte(TAG_PAIR_LEAD) != 0;
    }

    mInventory.loadNbt(data, context);
}

void ChestBlockActor::pairWith(ChestBlockActor &other) {
    mPaired = true;
    mPairPosition = other.getPosition();
    other.mPaired = true;
    other.mPairPosition = mPosition;

    const bool selfPrimary = isPrimaryOf(mPosition, other.mPosition);
    mPairLead = selfPrimary;
    other.mPairLead = !selfPrimary;
}

void ChestBlockActor::unpair() {
    if (!mPaired)
        return;

    ChestBlockActor *pair = getPair();
    mPaired = false;
    mPairLead = false;

    if (pair != nullptr) {
        pair->mPaired = false;
        pair->mPairLead = false;
    }
}

ChestBlockActor *ChestBlockActor::getPair() {
    if (!mPaired || mLevel == nullptr)
        return nullptr;

    return mLevel->getBlockActors().find<ChestBlockActor>(mPairPosition);
}

void ChestBlockActor::addViewer() {
    ++mViewers;
}

void ChestBlockActor::removeViewer() {
    if (mViewers > 0)
        --mViewers;
}

int ChestBlockActor::getViewerCount() const {
    if (!mPaired || mLevel == nullptr)
        return mViewers;

    const ChestBlockActor *pair = mLevel->getBlockActors().find<ChestBlockActor>(mPairPosition);
    return pair == nullptr ? mViewers : mViewers + pair->mViewers;
}

Container *ChestBlockActor::getContainer() {
    ChestBlockActor *pair = getPair();
    if (pair == nullptr) {
        mDoubleInventory.reset();
        return &mInventory;
    }

    ChestBlockActor *primary = getPrimary();
    ChestBlockActor *secondary = primary == this ? pair : this;

    mDoubleInventory.reset(new DoubleChestInventory(primary->mInventory, secondary->mInventory));
    return mDoubleInventory.get();
}

ChestBlockActor *ChestBlockActor::getPrimary() {
    if (!mPaired)
        return this;

    ChestBlockActor *pair = getPair();
    if (pair == nullptr)
        return this;

    return mPairLead ? this : pair;
}
