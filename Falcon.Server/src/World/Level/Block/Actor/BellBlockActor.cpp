#include "World/Level/Block/Actor/BellBlockActor.h"

namespace {
    const char *TAG_DIRECTION = "Direction";
    const char *TAG_RINGING = "Ringing";
    const char *TAG_TICKS = "Ticks";
}

Tag BellBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putInt(TAG_DIRECTION, mDirection);
    data.putBool(TAG_RINGING, mRinging);
    data.putInt(TAG_TICKS, mTicks);
    return data;
}

Tag BellBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putInt(TAG_DIRECTION, mDirection);
    data.putBool(TAG_RINGING, mRinging);
    data.putInt(TAG_TICKS, mTicks);
    return data;
}

void BellBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mDirection = data.getInt(TAG_DIRECTION, 0);
    mRinging = data.getBool(TAG_RINGING, false);
    mTicks = data.getInt(TAG_TICKS, 0);
}
