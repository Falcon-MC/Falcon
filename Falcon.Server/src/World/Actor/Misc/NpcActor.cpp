#include "World/Actor/Misc/NpcActor.h"

#include "Core/Json/Json.h"
#include "Network/Handler/NpcHandler.h"
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
    // the game's own spelling
    const char *const INTERACT_TEXT_TAG = "InterativeText";
    const char *const ACTIONS_TAG = "Actions";
    const char *const PLAYER_SCENES_TAG = "PlayerSceneMapping";
    const char *const PLAYER_ID_TAG = "PlayerID";

    const json::Value *definitionData() {
        const json::Value *definition = EntityDefinitions::find(NpcActor::IDENTIFIER);
        const json::Value *components = definition == nullptr ? nullptr : definition->get("components");
        const json::Value *npc = components == nullptr ? nullptr : components->get("minecraft:npc");
        return npc == nullptr ? nullptr : npc->get("npc_data");
    }

    std::vector<NpcActor::Skin> definitionSkins() {
        std::vector<NpcActor::Skin> skins;
        const json::Value *data = definitionData();
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
        : ServerActor(runtimeId, identifier), mSkins(definitionSkins()) {
    const json::Value *data = definitionData();
    if (data != nullptr && data->isObject())
        mNpcData = json::stringify(*data);
}

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
    _sendString(owner, NAME_RAW_TEXT_DATA_ID, mRawName);
    owner->sendActorNameTag(*this);
}

const std::string &NpcActor::getSceneFor(int64_t playerUniqueId) const {
    const auto scene = mPlayerScenes.find(playerUniqueId);
    return scene == mPlayerScenes.end() ? mSceneName : scene->second;
}

const std::string &NpcActor::getOpenedScene(int64_t playerUniqueId) const {
    static const std::string NONE;
    const auto scene = mOpenedScenes.find(playerUniqueId);
    return scene == mOpenedScenes.end() ? NONE : scene->second;
}

bool NpcActor::setActions(ServerNetworkHandler *owner, std::vector<NpcAction> actions) {
    if (actions == mActions)
        return false;
    mActions = std::move(actions);
    _sendString(owner, ACTIONS_DATA_ID, NpcAction::writeList(mActions));
    return true;
}

void NpcActor::setInteractText(ServerNetworkHandler *owner, const std::string &text) {
    mInteractText = text;
    _sendString(owner, INTERACT_TEXT_DATA_ID, mInteractText);
}

bool NpcActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    NpcHandler::openDialogue(owner, *this, player, getSceneFor(player.getUniqueId()));
    return true;
}

void NpcActor::_sendString(ServerNetworkHandler *owner, int32_t id, const std::string &value) {
    if (owner == nullptr)
        return;
    EntityDataEntry entry;
    entry.mId = id;
    entry.mFormat = EntityDataFormat::String;
    entry.mStringValue = value;
    EntityDataMap metadata;
    metadata.mEntries.push_back(entry);
    owner->sendActorMetadata(*this, metadata);
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
    EntityDataEntry hasNpc;
    hasNpc.mId = HAS_NPC_DATA_ID;
    hasNpc.mFormat = EntityDataFormat::Byte;
    hasNpc.mByteValue = 1;
    metadata.mEntries.push_back(hasNpc);
    const std::pair<int32_t, const std::string *> strings[] = {
            {NPC_DATA_DATA_ID, &mNpcData}, {INTERACT_TEXT_DATA_ID, &mInteractText}, {NAME_RAW_TEXT_DATA_ID, &mRawName}};
    for (const auto &[id, value]: strings) {
        if (value->empty())
            continue;
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::String;
        entry.mStringValue = *value;
        metadata.mEntries.push_back(entry);
    }
    if (mActions.empty())
        return;
    EntityDataEntry actions;
    actions.mId = ACTIONS_DATA_ID;
    actions.mFormat = EntityDataFormat::String;
    actions.mStringValue = NpcAction::writeList(mActions);
    metadata.mEntries.push_back(actions);
}

Tag NpcActor::saveNbt() const {
    Tag data = ServerActor::saveNbt();
    data.putString(RAW_NAME_TAG, mRawName);
    data.putString(INTERACT_TEXT_TAG, mInteractText);
    data.putString(ACTIONS_TAG, NpcAction::writeList(mActions));
    if (!mPlayerScenes.empty()) {
        Tag scenes = Tag::ofList(Tag::Type::Compound);
        for (const auto &[player, scene]: mPlayerScenes) {
            Tag entry = Tag::ofCompound();
            entry.putLong(PLAYER_ID_TAG, player);
            entry.putString(SCENE_NAME_TAG, scene);
            scenes.addToList(std::move(entry));
        }
        data.put(PLAYER_SCENES_TAG, std::move(scenes));
    }
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
    mInteractText = data.getString(INTERACT_TEXT_TAG, mInteractText);
    if (data.contains(ACTIONS_TAG))
        mActions = NpcAction::parseList(data.getString(ACTIONS_TAG));
    mSceneName = data.getString(SCENE_NAME_TAG, mSceneName);
    const Tag *scenes = data.get(PLAYER_SCENES_TAG);
    if (scenes != nullptr && scenes->isList()) {
        for (const Tag &entry: scenes->getList()) {
            if (entry.isCompound() && entry.contains(PLAYER_ID_TAG))
                mPlayerScenes[entry.getLong(PLAYER_ID_TAG)] = entry.getString(SCENE_NAME_TAG);
        }
    }
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
