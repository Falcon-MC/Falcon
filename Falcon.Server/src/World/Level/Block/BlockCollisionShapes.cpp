#include "World/Level/Block/BlockCollisionShapes.h"

#include "BlockCollisionShapesJson.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/Json/Json.h"
#include "World/Level/Block/BlockState.h"

#include <cstdlib>
#include <memory>
#include <string>
#include <unordered_map>

namespace {
    struct Table {
        std::vector<std::vector<AxisAlignedBB>> mShapes;
        std::unordered_map<int32_t, uint32_t> mStates;
    };

    Table load() {
        Table table;
        const std::unique_ptr<json::Value> root = json::parse(FalconBlockCollisionData::kBlockCollisionShapesJson);
        const json::Value *shapes = root == nullptr ? nullptr : root->get("shapes");
        const json::Value *states = root == nullptr ? nullptr : root->get("states");
        if (shapes == nullptr || states == nullptr || shapes->mType != json::Value::Type::Array
            || states->mType != json::Value::Type::Object) {
            LOG_WARN(LogAreaID::Server, "Failed to parse embedded block collision shapes");
            return table;
        }

        for (const std::unique_ptr<json::Value> &shape: shapes->mArray) {
            std::vector<AxisAlignedBB> boxes;
            for (const std::unique_ptr<json::Value> &box: shape->mArray) {
                if (box->mArray.size() != 6)
                    continue;
                boxes.emplace_back((float) box->mArray[0]->mNumber, (float) box->mArray[1]->mNumber,
                                   (float) box->mArray[2]->mNumber, (float) box->mArray[3]->mNumber,
                                   (float) box->mArray[4]->mNumber, (float) box->mArray[5]->mNumber);
            }
            table.mShapes.push_back(std::move(boxes));
        }
        for (const auto &entry: states->mObject) {
            const auto index = (uint32_t) entry.second->mNumber;
            if (index < table.mShapes.size())
                table.mStates[(int32_t) std::strtoll(entry.first.c_str(), nullptr, 10)] = index;
        }
        return table;
    }

    const Table &table() {
        static const Table loaded = load();
        return loaded;
    }
}

namespace BlockCollisionShapes {
    const std::vector<AxisAlignedBB> *find(const BlockState &state) {
        const Table &shapes = table();
        const auto found = shapes.mStates.find(state.getHash());
        return found == shapes.mStates.end() ? nullptr : &shapes.mShapes[found->second];
    }
}
