#include "World/Level/Block/BlockSupportRules.h"

#include "BlockSupportRulesJson.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Level.h"

#include <cstdlib>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace {
    struct Table {
        std::vector<std::unordered_set<int32_t>> mRules;
        std::unordered_map<int32_t, uint32_t> mBlocks;
    };

    Table load() {
        Table table;
        const std::unique_ptr<json::Value> root = json::parse(FalconBlockSupportData::kBlockSupportRulesJson);
        const json::Value *rules = root == nullptr ? nullptr : root->get("rules");
        const json::Value *blocks = root == nullptr ? nullptr : root->get("blocks");
        if (rules == nullptr || blocks == nullptr || rules->mType != json::Value::Type::Array
            || blocks->mType != json::Value::Type::Object) {
            LOG_WARN(LogAreaID::Server, "Failed to parse embedded block support rules");
            return table;
        }
        for (const std::unique_ptr<json::Value> &rule: rules->mArray) {
            std::unordered_set<int32_t> holding;
            for (const std::unique_ptr<json::Value> &state: rule->mArray)
                holding.insert((int32_t) state->mNumber);
            table.mRules.push_back(std::move(holding));
        }
        for (const auto &entry: blocks->mObject) {
            const auto rule = (uint32_t) entry.second->mNumber;
            if (rule < table.mRules.size())
                table.mBlocks[(int32_t) std::strtoll(entry.first.c_str(), nullptr, 10)] = rule;
        }
        return table;
    }
}

std::optional<bool> BlockSupportRules::survivesAt(Level &level, const Vector3i &position, const BlockState &state) {
    static const Table table = load();
    const auto found = table.mBlocks.find(state.getHash());
    if (found == table.mBlocks.end())
        return std::nullopt;
    // an unloaded chunk below is unknown, not air
    const BlockState *below = level.peekBlockPtr(position.x, position.y - 1, position.z);
    return below == nullptr || table.mRules[found->second].count(below->getHash()) != 0;
}
