#include "World/Level/Block/BlockTags.h"

#include "BlockTagsJson.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "World/Level/Block/BlockState.h"

#include <algorithm>
#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <unordered_set>

namespace {
    struct Table {
        std::unordered_map<std::string, std::unordered_set<std::string>> mBlocksByTag;
        std::unordered_map<int32_t, std::unordered_set<std::string>> mTagsByState;
    };

    Table load() {
        Table table;
        const std::unique_ptr<json::Value> root = json::parse(FalconBlockTagsData::kBlockTagsJson);
        const json::Value *tags = root == nullptr ? nullptr : root->get("tags");
        const json::Value *states = root == nullptr ? nullptr : root->get("states");
        if (tags == nullptr || states == nullptr || tags->mType != json::Value::Type::Object
            || states->mType != json::Value::Type::Object) {
            LOG_WARN(LogAreaID::Server, "Failed to parse embedded block tags");
            return table;
        }
        for (const auto &entry: tags->mObject) {
            std::unordered_set<std::string> &blocks = table.mBlocksByTag[entry.first];
            for (const std::unique_ptr<json::Value> &name: entry.second->mArray)
                blocks.insert(name->mString);
        }
        for (const auto &entry: states->mObject) {
            std::unordered_set<std::string> &names =
                    table.mTagsByState[(int32_t) std::strtoll(entry.first.c_str(), nullptr, 10)];
            for (const std::unique_ptr<json::Value> &tag: entry.second->mArray)
                names.insert(tag->mString);
        }
        return table;
    }
}

namespace {
    const Table &table() {
        static const Table loaded = load();
        return loaded;
    }
}

bool BlockTags::has(const BlockState &state, const std::string &tag) {
    const Table &table = ::table();
    const auto byState = table.mTagsByState.find(state.getHash());
    if (byState != table.mTagsByState.end())
        return byState->second.count(tag) != 0;
    const auto byTag = table.mBlocksByTag.find(tag);
    return byTag != table.mBlocksByTag.end() && byTag->second.count(state.mName) != 0;
}

std::vector<std::string> BlockTags::all(const BlockState &state) {
    const Table &table = ::table();
    std::vector<std::string> tags;
    const auto byState = table.mTagsByState.find(state.getHash());
    if (byState != table.mTagsByState.end()) {
        tags.assign(byState->second.begin(), byState->second.end());
    } else {
        for (const auto &entry: table.mBlocksByTag) {
            if (entry.second.count(state.mName) != 0)
                tags.push_back(entry.first);
        }
    }
    std::sort(tags.begin(), tags.end());
    return tags;
}
