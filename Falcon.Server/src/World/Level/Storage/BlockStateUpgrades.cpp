#include "World/Level/Storage/BlockStateUpgrades.h"

#include "World/Level/Block/BlockData.h"
#include "BlockUpgradeSchemas.h"
#include "Core/BlockState/BlockStateUpgrader.h"

#include <utility>
#include <vector>

namespace {
    BlockStateUpgrader buildUpgrader() {
        std::vector<BlockStateUpgradeSchema> schemas;
        for (const FalconBlockUpgradeData::EmbeddedSchema &schema: FalconBlockUpgradeData::kSchemas)
            schemas.push_back(BlockStateUpgradeSchema::fromJson(schema.mJson, schema.mId));

        return BlockStateUpgrader(std::move(schemas));
    }

    const BlockStateUpgrader &upgrader() {
        static const BlockStateUpgrader UPGRADER = buildUpgrader();
        return UPGRADER;
    }
}

BlockStateData BlockStateUpgrades::upgrade(const BlockStateData &state) {
    // Preview releases can add states without changing the block-state version. Schemas
    // sharing that version must still fill missing properties, while preserving existing ones.
    if (state.getVersion() > upgrader().getOutputVersion())
        return state;

    return upgrader().upgrade(state);
}

std::string BlockStateUpgrades::currentName(const std::string &name) {
    const std::string qualified = name.find(':') == std::string::npos ? "minecraft:" + name : name;
    if (BlockDataTable::find(qualified.c_str()) != nullptr)
        return qualified;

    return upgrade(BlockStateData(qualified, Tag::ofCompound(), 0)).getName();
}
