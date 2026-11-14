#include "Falcon/Plugin/PluginServerApi.h"

#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/DynamicPropertyStore.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/World.h"

#include <algorithm>
#include <exception>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace PluginApiHelpers;

namespace {
    using Properties = std::unordered_map<std::string, DynamicPropertyValue>;

    const char *const DYNAMIC_PROPERTIES_TAG = "DynamicProperties";

    // plugin values share the dynamic property store of scripts, under the plugin's name
    std::string prefixOf(FalconPlugin *source) {
        return plugin(source)->mDescription.mName + ":";
    }

    bool validKey(const char *key) {
        return key != nullptr && *key != '\0';
    }

    Properties *propertiesOf(FalconEntity *target) {
        Actor *value = entity(target);
        if (ServerPlayer *playerValue = dynamic_cast<ServerPlayer *>(value))
            return &playerValue->getDynamicProperties();
        if (ServerActor *actorValue = dynamic_cast<ServerActor *>(value))
            return &actorValue->getDynamicProperties();
        return nullptr;
    }

    const char *readValue(const Properties &properties, const std::string &key) {
        const auto found = properties.find(key);
        if (found == properties.end() || found->second.mType != DynamicPropertyValue::Type::String)
            return nullptr;
        return hold(found->second.mString);
    }

    void writeValue(Properties &properties, const std::string &key, const char *value) {
        if (value == nullptr) {
            properties.erase(key);
            return;
        }

        DynamicPropertyValue stored;
        stored.mType = DynamicPropertyValue::Type::String;
        stored.mString = value;
        properties[key] = stored;
    }

    std::vector<std::string> keysOf(const Properties &properties, const std::string &prefix) {
        std::vector<std::string> keys;
        for (const auto &entry: properties) {
            if (entry.first.size() > prefix.size() && entry.first.compare(0, prefix.size(), prefix) == 0
                && entry.second.mType == DynamicPropertyValue::Type::String)
                keys.push_back(entry.first.substr(prefix.size()));
        }
        std::sort(keys.begin(), keys.end());
        return keys;
    }

    const char *keyAt(const Properties &properties, const std::string &prefix, uint32_t index) {
        const std::vector<std::string> keys = keysOf(properties, prefix);
        return index < keys.size() ? hold(keys[index]) : nullptr;
    }

    const char *entityData(FalconPlugin *source, FalconEntity *target, const char *key) {
        const Properties *properties = propertiesOf(target);
        if (properties == nullptr || !validKey(key))
            return nullptr;
        return readValue(*properties, prefixOf(source) + key);
    }

    int entitySetData(FalconPlugin *source, FalconEntity *target, const char *key, const char *value) {
        Properties *properties = propertiesOf(target);
        if (properties == nullptr || !validKey(key))
            return 0;
        writeValue(*properties, prefixOf(source) + key, value);
        return 1;
    }

    uint32_t entityDataCount(FalconPlugin *source, FalconEntity *target) {
        const Properties *properties = propertiesOf(target);
        return properties == nullptr ? 0 : (uint32_t) keysOf(*properties, prefixOf(source)).size();
    }

    const char *entityDataKey(FalconPlugin *source, FalconEntity *target, uint32_t index) {
        const Properties *properties = propertiesOf(target);
        return properties == nullptr ? nullptr : keyAt(*properties, prefixOf(source), index);
    }

    // items keep their dynamic properties in their own tag, the way scripts save them
    Properties itemProperties(const ItemStack &stack) {
        Properties properties;
        const Tag *stored = stack.mTag.isCompound() ? stack.mTag.get(DYNAMIC_PROPERTIES_TAG) : nullptr;
        if (stored != nullptr)
            deserializeDynamicProperties(*stored, properties);
        return properties;
    }

    void storeItemProperties(ItemStack &stack, const Properties &properties) {
        if (properties.empty()) {
            if (stack.mTag.isCompound()) {
                stack.mTag.remove(DYNAMIC_PROPERTIES_TAG);
                if (stack.mTag.isEmpty())
                    stack.mTag = Tag();
            }
            return;
        }

        if (!stack.mTag.isCompound())
            stack.mTag = Tag::ofCompound();
        stack.mTag.put(DYNAMIC_PROPERTIES_TAG, serializeDynamicProperties(properties));
    }

    bool usable(FalconItem *target) {
        return target != nullptr && !item(target)->isAir();
    }

    const char *itemData(FalconPlugin *source, FalconItem *target, const char *key) {
        if (!usable(target) || !validKey(key))
            return nullptr;
        return readValue(itemProperties(*item(target)), prefixOf(source) + key);
    }

    int itemSetData(FalconPlugin *source, FalconItem *target, const char *key, const char *value) {
        if (!usable(target) || !validKey(key))
            return 0;

        ItemStack &stack = *item(target);
        Properties properties = itemProperties(stack);
        writeValue(properties, prefixOf(source) + key, value);
        storeItemProperties(stack, properties);
        return 1;
    }

    uint32_t itemDataCount(FalconPlugin *source, FalconItem *target) {
        return usable(target) ? (uint32_t) keysOf(itemProperties(*item(target)), prefixOf(source)).size() : 0;
    }

    const char *itemDataKey(FalconPlugin *source, FalconItem *target, uint32_t index) {
        return usable(target) ? keyAt(itemProperties(*item(target)), prefixOf(source), index) : nullptr;
    }

    const uint8_t *encode(const Tag &tag, uint32_t *length) {
        BinaryStream stream;
        NbtIo::writeTag(stream, tag, NbtVariant::LittleEndian);
        const char *data = hold(stream.getBuffer());
        if (length != nullptr)
            *length = (uint32_t) stream.getBuffer().size();
        return reinterpret_cast<const uint8_t *>(data);
    }

    const uint8_t *itemNbt(FalconItem *target, uint32_t *length) {
        if (length != nullptr)
            *length = 0;
        if (target == nullptr || !item(target)->mTag.isCompound() || item(target)->mTag.isEmpty())
            return nullptr;
        return encode(item(target)->mTag, length);
    }

    int itemSetNbt(FalconItem *target, const uint8_t *data, uint32_t length) {
        if (!usable(target))
            return 0;
        if (data == nullptr || length == 0) {
            item(target)->mTag = Tag();
            return 1;
        }

        Tag parsed;
        try {
            ReadOnlyBinaryStream stream(std::string(reinterpret_cast<const char *>(data), length));
            parsed = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        } catch (const std::exception &) {
            return 0;
        }
        if (!parsed.isCompound())
            return 0;

        item(target)->mTag = parsed.isEmpty() ? Tag() : std::move(parsed);
        return 1;
    }

    const uint8_t *entityNbt(FalconEntity *target, uint32_t *length) {
        if (length != nullptr)
            *length = 0;

        Actor *value = entity(target);
        if (const ServerPlayer *playerValue = dynamic_cast<const ServerPlayer *>(value))
            return encode(playerValue->saveNbt(owner().getWorldFor(*playerValue).getName()), length);
        if (const ServerActor *actorValue = dynamic_cast<const ServerActor *>(value))
            return encode(actorValue->saveNbt(), length);
        return nullptr;
    }
}

void PluginServerApi::fillData(FalconServerApi &api) {
    api.entityData = &entityData;
    api.entitySetData = &entitySetData;
    api.entityDataCount = &entityDataCount;
    api.entityDataKey = &entityDataKey;
    api.itemData = &itemData;
    api.itemSetData = &itemSetData;
    api.itemDataCount = &itemDataCount;
    api.itemDataKey = &itemDataKey;
    api.itemNbt = &itemNbt;
    api.itemSetNbt = &itemSetNbt;
    api.entityNbt = &entityNbt;
}
