#include "World/Actor/Misc/NpcActor.h"

#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"

FALCON_REGISTER_ACTOR(NpcActor, NpcActor::IDENTIFIER);

namespace {
    const char *const RAW_NAME_TAG = "RawtextName";
    const char *const SCENE_NAME_TAG = "SceneName";
    const char *const VARIANT_TAG = "Variant";
    const char *const MARK_VARIANT_TAG = "MarkVariant";

    std::vector<NpcActor::Skin> definitionSkins() {
        std::vector<NpcActor::Skin> skins;
        const json::Value *definition = EntityDefinitions::find(NpcActor::IDENTIFIER);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *npc = components == nullptr ? nullptr : components->get("minecraft:npc");
        const json::Value *data = npc == nullptr ? nullptr : npc->get("npc_data");
        const json::Value *list = data == nullptr ? nullptr : data->get("skin_list");
        if (list == nullptr || !list->isArray())
            return skins;
        for (const std::unique_ptr<json::Value> &entry: list->mArray) {
            NpcActor::Skin skin;
            const json::Value *variant = entry->get("variant");
            const json::Value *mark = entry->get("mark_variant");
            if (variant != nullptr && variant->isNumber())
                skin.mVariant = (int32_t) variant->mNumber;
            if (mark != nullptr && mark->isNumber())
                skin.mMarkVariant = (int32_t) mark->mNumber;
            skins.push_back(skin);
        }
        return skins;
    }

    std::string firstCodePoints(const std::string &text, size_t count) {
        size_t seen = 0;
        for (size_t index = 0; index < text.size(); ++index) {
            if (((unsigned char) text[index] & 0xC0) == 0x80)
                continue;
            if (seen == count)
                return text.substr(0, index);
            ++seen;
        }
        return text;
    }
}

NpcActor::NpcActor(uint64_t runtimeId, const std::string &identifier)
        : ServerActor(runtimeId, identifier), mSkins(definitionSkins()) {}

int32_t NpcActor::getSkinIndex() const {
    return mSkinIndex >= 0 && (size_t) mSkinIndex < mSkins.size() ? mSkinIndex : -1;
}

bool NpcActor::setSkinIndex(ServerNetworkHandler *owner, int32_t index) {
    if (index < 0 || (size_t) index >= mSkins.size() || index == mSkinIndex)
        return false;
    mSkinIndex = index;
    if (owner != nullptr) {
        EntityDataMap metadata;
        _appendSkin(metadata);
        if (!metadata.mEntries.empty())
            owner->sendActorMetadata(*this, metadata);
    }
    return true;
}

std::string NpcActor::resolveName(const std::string &rawName) {
    // ResolvedTextObject: the components' text joined; a plain string is kept
    const std::unique_ptr<json::Value> parsed = json::parse(rawName);
    const json::Value *rawtext = parsed == nullptr ? nullptr : parsed->get("rawtext");
    if (rawtext == nullptr || !rawtext->isArray())
        return rawName;
    std::string text;
    for (const std::unique_ptr<json::Value> &component: rawtext->mArray) {
        const json::Value *part = component->get("text");
        if (part != nullptr && part->isString())
            text += part->mString;
    }
    return text;
}

void NpcActor::setName(ServerNetworkHandler *owner, const std::string &rawName) {
    mRawName = rawName;
    setNameTag(firstCodePoints(resolveName(rawName), MAX_NAME_TAG_LENGTH));
    if (owner == nullptr)
        return;
    EntityDataEntry raw;
    raw.mId = NAME_RAW_TEXT_DATA_ID;
    raw.mFormat = EntityDataFormat::String;
    raw.mStringValue = mRawName;
    EntityDataMap metadata;
    metadata.mEntries.push_back(raw);
    owner->sendActorMetadata(*this, metadata);
    owner->sendActorNameTag(*this);
}

void NpcActor::_appendSkin(EntityDataMap &metadata) const {
    const int32_t index = getSkinIndex();
    if (index < 0)
        return;
    // each value the skin sets lands in its actor data
    const Skin &skin = mSkins[(size_t) index];
    if (skin.mVariant.has_value()) {
        EntityDataEntry variant;
        variant.mId = ActorFlags::VARIANT_DATA_ID;
        variant.mFormat = EntityDataFormat::Int;
        variant.mIntValue = *skin.mVariant;
        metadata.mEntries.push_back(variant);
    }
    if (skin.mMarkVariant.has_value()) {
        EntityDataEntry mark;
        mark.mId = ActorFlags::MARK_VARIANT_DATA_ID;
        mark.mFormat = EntityDataFormat::Int;
        mark.mIntValue = *skin.mMarkVariant;
        metadata.mEntries.push_back(mark);
    }
}

void NpcActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    _appendSkin(metadata);
    if (mRawName.empty())
        return;
    EntityDataEntry raw;
    raw.mId = NAME_RAW_TEXT_DATA_ID;
    raw.mFormat = EntityDataFormat::String;
    raw.mStringValue = mRawName;
    metadata.mEntries.push_back(raw);
}

Tag NpcActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    data.putString(RAW_NAME_TAG, mRawName);
    if (!mSceneName.empty())
        data.putString(SCENE_NAME_TAG, mSceneName);
    const int32_t index = getSkinIndex();
    if (index >= 0) {
        const Skin &skin = mSkins[(size_t) index];
        data.putInt(VARIANT_TAG, skin.mVariant.value_or(0));
        data.putInt(MARK_VARIANT_TAG, skin.mMarkVariant.value_or(0));
    }
    return data;
}

void NpcActor::loadNbt(const Tag &data) {
    ServerActor::loadNbt(data);
    mRawName = data.getString(RAW_NAME_TAG, mRawName);
    mSceneName = data.getString(SCENE_NAME_TAG, mSceneName);
    if (!data.contains(VARIANT_TAG))
        return;
    const int32_t variant = data.getInt(VARIANT_TAG);
    const int32_t mark = data.getInt(MARK_VARIANT_TAG);
    for (size_t index = 0; index < mSkins.size(); ++index) {
        if (mSkins[index].mVariant.value_or(0) == variant && mSkins[index].mMarkVariant.value_or(0) == mark) {
            mSkinIndex = (int32_t) index;
            return;
        }
    }
}
