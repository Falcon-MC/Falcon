#include "Block/Components/CreativeContentTable.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "CreativeItemsJson.h"
#include "Network/ProtocolData.h"

#include <deque>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    const char *BASE64_ALPHABET = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

    std::string decodeBase64(const std::string &value) {
        static int table[256];
        static bool ready = false;

        if (!ready) {
            for (int index = 0; index < 256; index++)
                table[index] = -1;
            for (int index = 0; index < 64; index++)
                table[(unsigned char) BASE64_ALPHABET[index]] = index;
            ready = true;
        }

        std::string out;
        int buffer = 0;
        int bits = 0;

        for (const char character: value) {
            const int decoded = table[(unsigned char) character];
            if (decoded < 0)
                continue;

            buffer = (buffer << 6) | decoded;
            bits += 6;

            if (bits >= 8) {
                bits -= 8;
                out.push_back((char) ((buffer >> bits) & 0xff));
            }
        }

        return out;
    }

    struct Storage {
        std::deque<std::string> mStrings;
        std::deque<std::vector<unsigned char>> mBlobs;
        std::vector<CreativeGroupEntry> mGroups;
        std::vector<CreativeEntry> mEntries;
    };

    const char *keep(Storage &storage, const std::string &value) {
        storage.mStrings.push_back(value);
        return storage.mStrings.back().c_str();
    }

    void readGroups(Storage &storage, const json::Value &root) {
        const json::Value *groups = root.get("groups");
        if (groups == nullptr || !groups->isArray())
            return;

        storage.mGroups.reserve(groups->mArray.size());

        for (const std::unique_ptr<json::Value> &group: groups->mArray) {
            if (!group->isObject())
                continue;

            const json::Value *category = group->get("creative_category");
            const json::Value *name = group->get("name");
            const json::Value *icon = group->get("icon");

            std::string iconIdentifier;
            if (icon != nullptr && icon->isObject()) {
                const json::Value *identifier = icon->get("id");
                if (identifier != nullptr)
                    iconIdentifier = identifier->string();
            }

            CreativeGroupEntry entry;
            entry.mCategory = (uint8_t) (category == nullptr ? 0 : category->integer());
            entry.mName = keep(storage, name == nullptr ? std::string() : name->string());
            entry.mIconIdentifier = keep(storage, iconIdentifier);
            storage.mGroups.push_back(entry);
        }
    }

    void readItems(Storage &storage, const json::Value &root) {
        const json::Value *items = root.get("items");
        if (items == nullptr || !items->isArray())
            return;

        storage.mEntries.reserve(items->mArray.size());

        for (const std::unique_ptr<json::Value> &item: items->mArray) {
            if (!item->isObject())
                continue;

            const json::Value *identifier = item->get("id");
            if (identifier == nullptr || !identifier->isString())
                continue;

            const json::Value *damage = item->get("damage");
            const json::Value *groupIndex = item->get("group_index");
            const json::Value *blockState = item->get("block_state_b64");
            const json::Value *nbt = item->get("nbt_b64");

            CreativeEntry entry;
            entry.mIdentifier = keep(storage, identifier->string());
            entry.mGroupIndex = groupIndex == nullptr ? -1 : groupIndex->integer(-1);
            entry.mDamage = damage == nullptr ? 0 : damage->integer();
            entry.mNbt = nullptr;
            entry.mNbtSize = 0;

            if (nbt != nullptr && nbt->isString()) {
                const std::string decoded = decodeBase64(nbt->string());
                storage.mBlobs.emplace_back(decoded.begin(), decoded.end());
                entry.mNbt = storage.mBlobs.back().data();
                entry.mNbtSize = storage.mBlobs.back().size();
            }

            storage.mEntries.push_back(entry);
        }
    }

    Storage load(const char *json) {
        Storage result;

        const std::unique_ptr<json::Value> root = json::parse(std::string(json));

        if (root == nullptr || !root->isObject()) {
            LOG_WARN(LogAreaID::Server, "Failed to parse the creative items data");
            return result;
        }

        readGroups(result, *root);
        readItems(result, *root);

        LOG_TRACE(LogAreaID::Server, "Loaded %zu creative group(s) and %zu creative item(s)",
                  result.mGroups.size(), result.mEntries.size());
        return result;
    }

    const Storage &storage() {
        static const Storage loaded = load(FalconCreativeItemData::kCreativeItemsJson);
        return loaded;
    }

    std::string keyOf(const CreativeEntry &entry) {
        std::string key = entry.mIdentifier;
        key += '\x1f';
        key += std::to_string(entry.mDamage);
        key += '\x1f';
        if (entry.mNbt != nullptr)
            key.append((const char *) entry.mNbt, entry.mNbtSize);
        return key;
    }

    const std::unordered_set<std::string> &olderKeys(const ProtocolData &data) {
        static std::mutex mutex;
        static std::unordered_map<int32_t, std::unordered_set<std::string>> keys;

        const std::lock_guard<std::mutex> lock(mutex);
        const auto it = keys.find(data.mProtocol);
        if (it != keys.end())
            return it->second;

        const Storage older = load(data.mCreativeItems);
        std::unordered_set<std::string> &result = keys[data.mProtocol];
        for (const CreativeEntry &entry: older.mEntries)
            result.insert(keyOf(entry));
        return result;
    }
}

const CreativeGroupEntry *CreativeContentTable::getGroups() {
    return storage().mGroups.data();
}

size_t CreativeContentTable::getGroupCount() {
    return storage().mGroups.size();
}

const CreativeEntry *CreativeContentTable::getEntries() {
    return storage().mEntries.data();
}

size_t CreativeContentTable::getEntryCount() {
    return storage().mEntries.size();
}

bool CreativeContentTable::isKnownBy(size_t index, int32_t protocol) {
    const ProtocolData *data = ProtocolData::find(protocol);
    if (data == nullptr)
        return true;
    return olderKeys(*data).count(keyOf(storage().mEntries[index])) != 0;
}
