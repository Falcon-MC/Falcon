#include "World/Level/Block/Actor/NoteBlockActor.h"

namespace {
    const char *TAG_NOTE = "note";
    const char *TAG_POWERED = "powered";
}

Tag NoteBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putByte(TAG_NOTE, (int8_t) mNote);
    data.putBool(TAG_POWERED, mPowered);
    return data;
}

Tag NoteBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    data.putByte(TAG_NOTE, (int8_t) mNote);
    data.putBool(TAG_POWERED, mPowered);
    return data;
}

void NoteBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mNote = (uint8_t) data.getByte(TAG_NOTE, 0);
    mPowered = data.getBool(TAG_POWERED, false);
}
