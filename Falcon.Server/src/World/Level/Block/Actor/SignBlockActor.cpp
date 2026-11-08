#include "World/Level/Block/Actor/SignBlockActor.h"

namespace {
    const char *TAG_FRONT = "FrontText";
    const char *TAG_BACK = "BackText";
    const char *TAG_WAXED = "IsWaxed";
}

Tag SignBlockActor::emptyText() {
    Tag text = Tag::ofCompound();
    text.putString("FilteredText", "");
    text.putBool("HideGlowOutline", false);
    text.putBool("IgnoreLighting", false);
    text.putBool("PersistFormatting", true);
    // 0xFF000000: black, fully opaque
    text.putInt("SignTextColor", -16777216);
    text.putString("Text", "");
    text.putString("TextOwner", "");
    return text;
}

void SignBlockActor::_putFields(Tag &data) const {
    data.put(TAG_BACK, mBack);
    data.put(TAG_FRONT, mFront);
    data.putBool(TAG_WAXED, mWaxed);
}

Tag SignBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag SignBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

void SignBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    const Tag *front = data.get(TAG_FRONT);
    const Tag *back = data.get(TAG_BACK);
    mFront = front != nullptr && front->isCompound() ? *front : emptyText();
    mBack = back != nullptr && back->isCompound() ? *back : emptyText();
    mWaxed = data.getBool(TAG_WAXED, false);
}
