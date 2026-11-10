#include "World/Level/Storage/BlockStateUpgrades.h"

#include "World/Level/Block/BlockData.h"
#include "BlockUpgradeSchemas.h"
#include "LegacyBlockTables.h"
#include "Core/BlockState/BlockStateUpgrader.h"
#include "Core/BlockState/LegacyBlockStates.h"

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

    const LegacyBlockStates &legacyStates() {
        static const LegacyBlockStates STATES(
                std::string(FalconLegacyBlockData::kIdMap, FalconLegacyBlockData::kIdMapSize),
                std::string((const char *) FalconLegacyBlockData::kIdMeta, FalconLegacyBlockData::kIdMetaSize));
        return STATES;
    }
}

BlockStateData BlockStateUpgrades::fromLegacy(int32_t id, int32_t meta) {
    return legacyStates().fromLegacy(id, meta);
}

BlockStateData BlockStateUpgrades::upgrade(const BlockStateData &state) {
    // > not >=: previews add states without a version bump; same-version schemas still fill missing
    // properties and keep existing ones
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
