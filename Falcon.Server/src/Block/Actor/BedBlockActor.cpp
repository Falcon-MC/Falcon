#include "Block/Actor/BedBlockActor.h"

namespace {
    const char *TAG_COLOR = "color";
}

Tag BedBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putByte(TAG_COLOR, mColor);
    return data;
}

void BedBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mColor = data.getByte(TAG_COLOR, 0);
}
