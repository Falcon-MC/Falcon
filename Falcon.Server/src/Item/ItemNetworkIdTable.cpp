#include "Item/ItemNetworkIdTable.h"

#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "ItemComponents2169Nbt.h"
#include "ItemComponentsNbt.h"
#include "ItemPalette2169Json.h"
#include "ItemPaletteJson.h"

#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

namespace {
    const char *TAG_NAME = "name";
    const char *TAG_ID = "id";
    const char *TAG_COMPONENTS = "components";

    const int32_t PROTOCOL_1_26_40 = 2168;
    const int32_t PROTOCOL_1_26_45 = 2169;

    /**
     * Suffixes of the items an older version lacks whose base block it has, so they show as that block.
     */
    const std::array<const char *, 3> VARIANT_SUFFIXES = {"_double_slab", "_slab", "_stairs"};

    Tag loadComponents(const unsigned char *data, size_t size) {
        const std::string compressed((const char *) data, size);

        std::string decompressed;
        if (!Gzip::decompress(compressed, decompressed)) {
            LOG_WARN(LogAreaID::Server, "Failed to decompress embedded item components");
            return Tag::ofCompound();
        }

        try {
            ReadOnlyBinaryStream stream(decompressed);
            return NbtIo::readTag(stream, NbtVariant::BigEndian);
        } catch (const std::exception &exception) {
            LOG_WARN(LogAreaID::Server, "Failed to parse item components: %s", exception.what());
            return Tag::ofCompound();
        }
    }

    std::vector<ItemNetworkIdEntry> loadEntries(const char *paletteJson, const unsigned char *components,
                                                size_t componentsSize) {
        std::vector<ItemNetworkIdEntry> entries;

        const std::unique_ptr<json::Value> root = json::parse(paletteJson);
        const json::Value *items = root == nullptr ? nullptr : root->get("items");
        if (items == nullptr || !items->isArray()) {
            LOG_WARN(LogAreaID::Server, "Failed to parse an embedded item palette");
            return entries;
        }

        const Tag componentTags = loadComponents(components, componentsSize);
        entries.reserve(items->mArray.size());

        for (const std::unique_ptr<json::Value> &item: items->mArray) {
            const json::Value *name = item->get("name");
            const json::Value *id = item->get("id");
            if (name == nullptr || id == nullptr)
                continue;

            ItemNetworkIdEntry entry;
            entry.mIdentifier = name->string();
            entry.mNetworkId = id->integer();

            const json::Value *version = item->get("version");
            entry.mVersion = version == nullptr ? 0 : version->integer();

            const json::Value *componentBased = item->get("component_based");
            entry.mComponentBased = componentBased != nullptr && componentBased->boolean();

            const Tag *itemComponents = componentTags.get(entry.mIdentifier);
            const Tag *componentData = itemComponents == nullptr ? nullptr : itemComponents->get(TAG_COMPONENTS);
            if (componentData != nullptr) {
                entry.mComponents.putString(TAG_NAME, entry.mIdentifier);
                entry.mComponents.putInt(TAG_ID, entry.mNetworkId);
                entry.mComponents.put(TAG_COMPONENTS, *componentData);
            }

            entries.push_back(std::move(entry));
        }

        return entries;
    }

    const std::vector<ItemNetworkIdEntry> &entries() {
        static const std::vector<ItemNetworkIdEntry> loaded = loadEntries(
                FalconItemData::kItemPaletteJson, FalconItemData::kItemComponentsNbt,
                FalconItemData::kItemComponentsNbtSize);
        return loaded;
    }

    const std::vector<ItemNetworkIdEntry> &entries2169() {
        static const std::vector<ItemNetworkIdEntry> loaded = loadEntries(
                FalconItemData2169::kItemPaletteJson, FalconItemData2169::kItemComponentsNbt,
                FalconItemData2169::kItemComponentsNbtSize);
        return loaded;
    }

    std::unordered_map<std::string, int32_t> networkIdsByName(const std::vector<ItemNetworkIdEntry> &palette) {
        std::unordered_map<std::string, int32_t> ids;
        for (const ItemNetworkIdEntry &entry: palette)
            ids[entry.mIdentifier] = entry.mNetworkId;
        return ids;
    }

    std::string baseBlockOf(const std::string &identifier) {
        for (const char *suffix: VARIANT_SUFFIXES) {
            const std::string ending(suffix);
            if (identifier.size() > ending.size()
                && identifier.compare(identifier.size() - ending.size(), ending.size(), ending) == 0)
                return identifier.substr(0, identifier.size() - ending.size());
        }
        return "";
    }

    std::shared_ptr<const ItemNetworkIdMap> buildNetworkIds(const std::vector<ItemNetworkIdEntry> &palette) {
        const std::unordered_map<std::string, int32_t> clientIds = networkIdsByName(palette);
        std::shared_ptr<ItemNetworkIdMap> map = std::make_shared<ItemNetworkIdMap>();

        std::vector<const ItemNetworkIdEntry *> missing;
        for (const ItemNetworkIdEntry &entry: entries()) {
            const auto it = clientIds.find(entry.mIdentifier);
            if (it == clientIds.end())
                missing.push_back(&entry);
            else
                map->add(entry.mNetworkId, it->second);
        }

        for (const ItemNetworkIdEntry *entry: missing) {
            const auto it = clientIds.find(baseBlockOf(entry->mIdentifier));
            if (it != clientIds.end())
                map->add(entry->mNetworkId, it->second);
        }
        return map;
    }
}

const ItemNetworkIdEntry *ItemNetworkIdTable::getEntries() {
    return entries().data();
}

size_t ItemNetworkIdTable::getCount() {
    return entries().size();
}

const ItemNetworkIdEntry *ItemNetworkIdTable::find(const std::string &identifier) {
    static const std::unordered_map<std::string, const ItemNetworkIdEntry *> lookup = []() {
        std::unordered_map<std::string, const ItemNetworkIdEntry *> map;

        for (const ItemNetworkIdEntry &entry: entries())
            map[entry.mIdentifier] = &entry;

        return map;
    }();

    auto it = lookup.find(identifier);
    if (it == lookup.end())
        return nullptr;

    return it->second;
}

const std::vector<ItemNetworkIdEntry> &ItemNetworkIdTable::getPalette(int32_t protocol) {
    if (protocol == PROTOCOL_1_26_40 || protocol == PROTOCOL_1_26_45)
        return entries2169();
    return entries();
}

std::shared_ptr<const ItemNetworkIdMap> ItemNetworkIdTable::getNetworkIds(int32_t protocol) {
    if (protocol != PROTOCOL_1_26_40 && protocol != PROTOCOL_1_26_45)
        return nullptr;

    static const std::shared_ptr<const ItemNetworkIdMap> map2169 = buildNetworkIds(entries2169());
    return map2169;
}
