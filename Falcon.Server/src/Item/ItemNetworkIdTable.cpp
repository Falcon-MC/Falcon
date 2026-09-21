#include "Item/ItemNetworkIdTable.h"

#include "Core/Archive/Gzip.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "ItemComponentsNbt.h"
#include "ItemPaletteJson.h"

#include <memory>
#include <unordered_map>
#include <vector>

namespace {
    const char *TAG_NAME = "name";
    const char *TAG_ID = "id";
    const char *TAG_COMPONENTS = "components";

    Tag loadComponents() {
        const std::string compressed((const char *) FalconItemData::kItemComponentsNbt,
                                     FalconItemData::kItemComponentsNbtSize);

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

    std::vector<ItemNetworkIdEntry> loadEntries() {
        std::vector<ItemNetworkIdEntry> entries;

        const std::unique_ptr<json::Value> root = json::parse(FalconItemData::kItemPaletteJson);
        const json::Value *items = root == nullptr ? nullptr : root->get("items");
        if (items == nullptr || !items->isArray()) {
            LOG_WARN(LogAreaID::Server, "Failed to parse the embedded item palette");
            return entries;
        }

        const Tag components = loadComponents();
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

            const Tag *itemComponents = components.get(entry.mIdentifier);
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
        static const std::vector<ItemNetworkIdEntry> loaded = loadEntries();
        return loaded;
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
