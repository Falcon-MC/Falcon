#include "Item/ItemNetworkIdTable.h"

#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "ItemComponentsNbt.h"
#include "ItemPaletteJson.h"
#include "Network/ProtocolData.h"

#include <array>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace {
    const char *TAG_NAME = "name";
    const char *TAG_ID = "id";
    const char *TAG_COMPONENTS = "components";

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

    const std::vector<ItemNetworkIdEntry> &olderEntries(const ProtocolData &data) {
        static std::mutex mutex;
        static std::unordered_map<int32_t, std::vector<ItemNetworkIdEntry>> loaded;

        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = loaded.find(data.mProtocol);
        if (it != loaded.end())
            return it->second;

        return loaded.emplace(data.mProtocol, loadEntries(data.mItemPalette, data.mItemComponents,
                                                          data.mItemComponentsSize)).first->second;
    }

    std::unordered_map<std::string, int32_t> networkIdsByName(const std::vector<ItemNetworkIdEntry> &palette) {
        std::unordered_map<std::string, int32_t> ids;
        for (const ItemNetworkIdEntry &entry: palette)
            ids[entry.mIdentifier] = entry.mNetworkId;
        return ids;
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
            const auto it = clientIds.find(ItemNetworkIdTable::getVariantBase(entry->mIdentifier));
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

std::string ItemNetworkIdTable::getVariantBase(const std::string &identifier) {
    for (const char *suffix: VARIANT_SUFFIXES) {
        const std::string ending(suffix);
        if (identifier.size() > ending.size()
            && identifier.compare(identifier.size() - ending.size(), ending.size(), ending) == 0)
            return identifier.substr(0, identifier.size() - ending.size());
    }
    return "";
}

const std::vector<ItemNetworkIdEntry> &ItemNetworkIdTable::getPalette(int32_t protocol) {
    const ProtocolData *data = ProtocolData::find(protocol);
    return data == nullptr ? entries() : olderEntries(*data);
}

std::shared_ptr<const ItemNetworkIdMap> ItemNetworkIdTable::getNetworkIds(int32_t protocol) {
    const ProtocolData *data = ProtocolData::find(protocol);
    if (data == nullptr)
        return nullptr;

    static std::mutex mutex;
    static std::unordered_map<int32_t, std::shared_ptr<const ItemNetworkIdMap>> maps;

    const std::lock_guard<std::mutex> lock(mutex);
    std::shared_ptr<const ItemNetworkIdMap> &map = maps[data->mProtocol];
    if (map == nullptr)
        map = buildNetworkIds(olderEntries(*data));
    return map;
}
