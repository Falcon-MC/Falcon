#include "Block/Actor/CopperGolemStatueBlockActor.h"

#include <algorithm>

namespace {
    const char *TAG_POSE = "Pose";
    const char *TAG_ACTOR = "Actor";
}

void CopperGolemStatueBlockActor::setPose(int32_t pose) {
    mPose = std::max(0, std::min(pose, POSE_COUNT - 1));
}

Tag CopperGolemStatueBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putInt(TAG_POSE, mPose);
    if (hasStoredActor())
        data.put(TAG_ACTOR, mStoredActor);
    return data;
}

Tag CopperGolemStatueBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putInt(TAG_POSE, mPose);
    return data;
}

void CopperGolemStatueBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    setPose(data.getInt(TAG_POSE, 0));

    const Tag *stored = data.get(TAG_ACTOR);
    if (stored != nullptr && stored->isCompound())
        mStoredActor = *stored;
}
