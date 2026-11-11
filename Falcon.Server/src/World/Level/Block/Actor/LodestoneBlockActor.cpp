#include "World/Level/Block/Actor/LodestoneBlockActor.h"

#include "World/Level/PositionTrackingDB.h"

namespace {
    const char *const TAG_TRACKING_HANDLE = "trackingHandle";
}

// sub_144C40FF0: written only once linked
Tag LodestoneBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    if (mTrackingId != PositionTrackingDB::INVALID_ID)
        data.putInt(TAG_TRACKING_HANDLE, mTrackingId);
    return data;
}

// sub_144C40C90
void LodestoneBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    const Tag *handle = data.get(TAG_TRACKING_HANDLE);
    if (handle != nullptr && handle->getType() == Tag::Type::Int)
        mTrackingId = handle->asInt();
}
