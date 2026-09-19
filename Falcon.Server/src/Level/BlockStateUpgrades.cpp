#include "Level/BlockStateUpgrades.h"

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
    if (state.getVersion() >= upgrader().getOutputVersion())
        return state;

    return upgrader().upgrade(state);
}
