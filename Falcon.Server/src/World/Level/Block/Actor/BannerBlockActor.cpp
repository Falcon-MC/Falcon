#include "World/Level/Block/Actor/BannerBlockActor.h"

namespace {
    const char *TAG_BASE = "Base";
    const char *TAG_TYPE = "Type";
    const char *TAG_PATTERNS = "Patterns";
}

void BannerBlockActor::_putFields(Tag &data) const {
    data.putInt(TAG_BASE, mBase);
    data.putInt(TAG_TYPE, mType);
    // an empty pattern list is left out, as the game saves it
    if (!mPatterns.getList().empty())
        data.put(TAG_PATTERNS, mPatterns);
}

Tag BannerBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    _putFields(data);
    return data;
}

Tag BannerBlockActor::getSpawnCompound() const {
    Tag data = BlockActor::getSpawnCompound();
    _putFields(data);
    return data;
}

void BannerBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;

    mBase = data.getInt(TAG_BASE, 0);
    mType = data.getInt(TAG_TYPE, 0);
    const Tag *patterns = data.get(TAG_PATTERNS);
    mPatterns = patterns != nullptr && patterns->isList() ? *patterns : Tag::ofList(Tag::Type::Compound);
}
