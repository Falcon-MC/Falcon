#include "Block/Blocks/PlacementRuleBlocks.h"

#include "Block/BlockIdentifier.h"
#include "Block/BlockSupport.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"

#include <unordered_set>

namespace {
    const int SNOW_LAYER_MAX_HEIGHT = 7;

    const std::unordered_set<std::string> &replaceableIdentifiers() {
        static const std::unordered_set<std::string> identifiers = {
                "minecraft:water", "minecraft:flowing_water", "minecraft:lava", "minecraft:flowing_lava",
                "minecraft:bubble_column", "minecraft:bush", "minecraft:crimson_roots", "minecraft:warped_roots",
                "minecraft:nether_sprouts", "minecraft:fire", "minecraft:soul_fire", "minecraft:large_fern",
                "minecraft:tall_grass", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
                "minecraft:leaf_litter", "minecraft:glow_lichen", "minecraft:sculk_vein", "minecraft:resin_clump",
                "minecraft:seagrass", "minecraft:vine", "minecraft:snow_layer"
        };
        return identifiers;
    }

    BlockState belowOf(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y - 1, position.z);
    }
}

bool ReplaceableBlock::matches(const std::string &identifier) {
    return replaceableIdentifiers().find(identifier) != replaceableIdentifiers().end()
           || BlockIdentifier::startsWith(identifier, "minecraft:light_block");
}

bool ReplaceableBlock::canBeReplaced(const BlockState &state) const {
    if (state.mName == "minecraft:snow_layer")
        return state.mStates.getInt("height", 0) < SNOW_LAYER_MAX_HEIGHT;

    return true;
}

bool CarpetBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_carpet");
}

bool CarpetBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    return !DecorationSupport::isAir(belowOf(level, position));
}

bool PressurePlateBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_pressure_plate");
}

bool PressurePlateBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    const BlockState below = belowOf(level, position);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP)
           || BlockIdentifier::endsWith(below.mName, "_fence");
}

bool RedstoneWireBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:redstone_wire";
}

bool RedstoneWireBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    return DecorationSupport::isSolid(belowOf(level, position));
}

bool LadderBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:ladder";
}

bool LadderBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace == PlacementOrientation::FACE_DOWN || blockFace == PlacementOrientation::FACE_UP)
        return false;

    const Vector3i supportPosition = BlockSupport::supportOf(position, blockFace);
    const BlockState support = level.getBlockState(supportPosition.x, supportPosition.y, supportPosition.z);

    if (BlockIdentifier::endsWith(support.mName, "_stained_glass")
        || BlockIdentifier::endsWith(support.mName, "_stained_glass_pane")
        || BlockIdentifier::endsWith(support.mName, "_leaves") || support.mName == "minecraft:beacon")
        return false;

    return BlockSupport::isAttachable(support, blockFace);
}
