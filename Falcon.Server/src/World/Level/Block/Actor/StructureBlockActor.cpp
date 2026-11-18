#include "World/Level/Block/Actor/StructureBlockActor.h"

#include <algorithm>

namespace {
    int32_t intOf(const Tag &data, const char *key, int32_t fallback = 0) {
        const Tag *tag = data.get(key);
        return tag != nullptr && tag->getType() == Tag::Type::Int ? tag->asInt() : fallback;
    }

    uint8_t byteOf(const Tag &data, const char *key) {
        const Tag *tag = data.get(key);
        return tag != nullptr && tag->getType() == Tag::Type::Byte ? (uint8_t) tag->asByte() : 0;
    }

    float floatOf(const Tag &data, const char *key, float fallback = 0.0f) {
        const Tag *tag = data.get(key);
        return tag != nullptr && tag->getType() == Tag::Type::Float ? tag->asFloat() : fallback;
    }

    int64_t longOf(const Tag &data, const char *key) {
        const Tag *tag = data.get(key);
        return tag != nullptr && tag->getType() == Tag::Type::Long ? tag->asLong() : 0;
    }

    std::string stringOf(const Tag &data, const char *key) {
        const Tag *tag = data.get(key);
        return tag != nullptr && tag->getType() == Tag::Type::String ? tag->asString() : std::string();
    }
}

Tag StructureBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putInt("data", (int32_t) mMode);
    data.putInt("redstoneSaveMode", mRedstoneSaveMode);
    data.putInt("xStructureOffset", mOffset.x);
    data.putInt("yStructureOffset", mOffset.y);
    data.putInt("zStructureOffset", mOffset.z);
    data.putInt("xStructureSize", mSize.x);
    data.putInt("yStructureSize", mSize.y);
    data.putInt("zStructureSize", mSize.z);
    data.putString("structureName", mStructureName);
    if (mHasFilteredName)
        data.putString("filteredStructureName", mFilteredName);
    data.putString("dataField", mDataField);
    data.putByte("ignoreEntities", mIgnoreEntities ? 1 : 0);
    data.putByte("includePlayers", mIncludePlayers ? 1 : 0);
    data.putByte("removeBlocks", mRemoveBlocks ? 1 : 0);
    data.putLong("lastTouchedPlayerId", mLastTouchedPlayerId);
    // corner blocks never draw their box
    data.putByte("showBoundingBox", mShowBoundingBox && mMode != Mode::Corner ? 1 : 0);
    data.putByte("rotation", (int8_t) mRotation);
    data.putByte("mirror", (int8_t) mMirror);
    data.putByte("animationMode", (int8_t) mAnimationMode);
    data.putFloat("animationSeconds", mAnimationSeconds);
    data.putFloat("integrity", mIntegrity);
    data.putLong("seed", mSeed);
    return data;
}

void StructureBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    mMode = (Mode) intOf(data, "data");
    mRedstoneSaveMode = intOf(data, "redstoneSaveMode");
    mOffset = Vector3i(intOf(data, "xStructureOffset"), intOf(data, "yStructureOffset"),
                       intOf(data, "zStructureOffset"));
    mSize = Vector3i(intOf(data, "xStructureSize"), intOf(data, "yStructureSize"), intOf(data, "zStructureSize"));
    mStructureName = stringOf(data, "structureName");
    mHasFilteredName = data.get("filteredStructureName") != nullptr;
    mFilteredName = stringOf(data, "filteredStructureName");
    mDataField = stringOf(data, "dataField");
    mIgnoreEntities = byteOf(data, "ignoreEntities") != 0;
    mIncludePlayers = byteOf(data, "includePlayers") != 0;
    mRemoveBlocks = byteOf(data, "removeBlocks") != 0;
    mLastTouchedPlayerId = longOf(data, "lastTouchedPlayerId");
    mShowBoundingBox = byteOf(data, "showBoundingBox") != 0 && mMode != Mode::Corner;
    mRotation = byteOf(data, "rotation");
    mMirror = byteOf(data, "mirror");
    mAnimationMode = byteOf(data, "animationMode");
    // older worlds kept ticks: the longer of the two wins
    const Tag *ticks = data.get("animationTicks");
    const float fromTicks = ticks != nullptr && ticks->getType() == Tag::Type::Int ? (float) ticks->asInt() / 20.0f
                                                                                   : 0.0f;
    mAnimationSeconds = std::max(floatOf(data, "animationSeconds"), fromTicks);
    // integrity is kept in [0, 100]
    mIntegrity = std::min(100.0f, std::max(floatOf(data, "integrity"), 0.0f));
    mSeed = longOf(data, "seed");
}
