#include "World/Level/Block/Actor/BedBlockActor.h"

namespace {
    const char *TAG_COLOR = "color";
}

Tag BedBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putByte(TAG_COLOR, mColor);
    return data;
}

Tag BedBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putByte(TAG_COLOR, mColor);
    return data;
}

void BedBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mColor = data.getByte(TAG_COLOR, 0);
}

bool BedBlockActor::claimPet(int64_t uniqueId) {
    if (mPetId != NO_PET && mPetId != uniqueId)
        return false;
    mPetId = uniqueId;
    return true;
}

void BedBlockActor::releasePet(int64_t uniqueId) {
    if (mPetId == uniqueId)
        mPetId = NO_PET;
}
