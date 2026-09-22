#include "Block/Blocks/PlacementRuleBlocks.h"

#include "Block/BlockClassRegistry.h"

FALCON_REGISTER_BLOCK(LadderBlock, 180);
FALCON_REGISTER_BLOCK(ReplaceableBlock, 330);
FALCON_REGISTER_BLOCK(SnowLayerBlock, 340);
FALCON_REGISTER_BLOCK(SlabBlock, 350);
FALCON_REGISTER_BLOCK(DoubleSlabBlock, 355);
FALCON_REGISTER_BLOCK(CandleBlock, 360);
FALCON_REGISTER_BLOCK(ScaffoldingBlock, 370);
FALCON_REGISTER_BLOCK(CarpetBlock, 380);
FALCON_REGISTER_BLOCK(PressurePlateBlock, 390);
FALCON_REGISTER_BLOCK(RedStoneWireBlock, 400);

#include "Block/BlockIdentifier.h"
#include "Block/BlockSupport.h"
#include "Block/Blocks/FenceBlocks.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Components/PlacementOrientation.h"
#include "Level/Generator/Overworld/Feature/Decoration/DecorationSupport.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"

#include <unordered_set>

namespace {
    const int SNOW_LAYER_MAX_HEIGHT = 7;
    const int CANDLES_MAX = 3;
    const char *SNOW_LAYER_HEIGHT = "height";
    const char *CANDLES = "candles";
    const char *VERTICAL_HALF = "minecraft:vertical_half";
    const char *STABILITY_CHECK = "stability_check";
    const char *STABILITY = "stability";
    const int UNSTABLE_STABILITY = 7;

    const std::unordered_set<std::string> &replaceableIdentifiers() {
        static const std::unordered_set<std::string> identifiers = {
                "minecraft:water", "minecraft:flowing_water", "minecraft:lava", "minecraft:flowing_lava",
                "minecraft:bubble_column", "minecraft:bush", "minecraft:crimson_roots", "minecraft:warped_roots",
                "minecraft:nether_sprouts", "minecraft:fire", "minecraft:soul_fire", "minecraft:large_fern",
                "minecraft:tall_grass", "minecraft:short_dry_grass", "minecraft:tall_dry_grass",
                "minecraft:leaf_litter", "minecraft:glow_lichen", "minecraft:sculk_vein", "minecraft:resin_clump",
                "minecraft:seagrass", "minecraft:vine"
        };
        return identifiers;
    }

    BlockState belowOf(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y - 1, position.z);
    }

    BlockState stateAt(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z);
    }

    const std::string SLAB_SUFFIX = "_slab";
    const std::string DOUBLE_SLAB_SUFFIX = "_double_slab";
    const std::string COPPER_SLAB_SUFFIX = "cut_copper_slab";
    const std::string DOUBLE_COPPER_SLAB_SUFFIX = "double_cut_copper_slab";
    const int32_t DOUBLE_SLAB_RESOURCE_COUNT = 2;
    const float SLAB_HEIGHT = 0.5f;

    std::string replaceSuffix(const std::string &identifier, const std::string &suffix,
                              const std::string &replacement) {
        return identifier.substr(0, identifier.size() - suffix.size()) + replacement;
    }
}

bool ReplaceableBlock::matches(const std::string &identifier) {
    return replaceableIdentifiers().find(identifier) != replaceableIdentifiers().end()
           || BlockIdentifier::startsWith(identifier, "minecraft:light_block");
}

bool ReplaceableBlock::canBeReplaced(const BlockState &state) const {
    (void) state;

    return true;
}

bool SnowLayerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:snow_layer";
}

bool SnowLayerBlock::canBeReplaced(const BlockState &state) const {
    return state.mStates.getInt(SNOW_LAYER_HEIGHT, 0) < SNOW_LAYER_MAX_HEIGHT;
}

PlacementMergeResult SnowLayerBlock::mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                                    const Vector3f &clickPosition, Vector3i &position,
                                                    BlockState &state) const {
    (void) blockFace;
    (void) clickPosition;

    const Vector3i candidates[2] = {clickedPosition, position};
    for (const Vector3i &candidate: candidates) {
        const BlockState existing = stateAt(level, candidate);
        if (existing.mName != getIdentifier())
            continue;

        const int32_t height = existing.mStates.getInt(SNOW_LAYER_HEIGHT, 0);
        if (height >= SNOW_LAYER_MAX_HEIGHT)
            continue;

        Tag states = existing.mStates;
        states.putInt(SNOW_LAYER_HEIGHT, height + 1);
        position = candidate;
        state = BlockState(existing.mName, states);
        return PlacementMergeResult::Merged;
    }

    return PlacementMergeResult::None;
}

bool SlabBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, SLAB_SUFFIX) && identifier.find("double_") == std::string::npos;
}

bool SlabBlock::isTopSlab(const BlockState &state) {
    return state.mStates.getString(VERTICAL_HALF, "bottom") == "top";
}

std::string SlabBlock::getDoubleSlabIdentifier() const {
    if (BlockIdentifier::endsWith(getIdentifier(), COPPER_SLAB_SUFFIX))
        return replaceSuffix(getIdentifier(), COPPER_SLAB_SUFFIX, DOUBLE_COPPER_SLAB_SUFFIX);

    return replaceSuffix(getIdentifier(), SLAB_SUFFIX, DOUBLE_SLAB_SUFFIX);
}

bool SlabBlock::getCollisionShape(const BlockState &state, AxisAlignedBB &shape) const {
    shape = isTopSlab(state) ? AxisAlignedBB(0.0f, SLAB_HEIGHT, 0.0f, 1.0f, 1.0f, 1.0f)
                             : AxisAlignedBB(0.0f, 0.0f, 0.0f, 1.0f, SLAB_HEIGHT, 1.0f);
    return true;
}

PlacementMergeResult SlabBlock::mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                               const Vector3f &clickPosition, Vector3i &position,
                                               BlockState &state) const {
    using namespace PlacementOrientation;

    const Block *doubleSlab = VanillaBlocks::fromIdentifier(getDoubleSlabIdentifier());
    const BlockState clicked = stateAt(level, clickedPosition);
    const bool clickedSameSlab = clicked.mName == getIdentifier();

    bool top = clickPosition.y > 0.5f;
    if (blockFace == FACE_DOWN) {
        if (clickedSameSlab && isTopSlab(clicked)) {
            if (doubleSlab == nullptr)
                return PlacementMergeResult::Rejected;

            position = clickedPosition;
            state = doubleSlab->toBlockState();
            return PlacementMergeResult::Merged;
        }
        top = true;
    } else if (blockFace == FACE_UP) {
        if (clickedSameSlab && !isTopSlab(clicked)) {
            if (doubleSlab == nullptr)
                return PlacementMergeResult::Rejected;

            position = clickedPosition;
            state = doubleSlab->toBlockState();
            return PlacementMergeResult::Merged;
        }
        top = false;
    }

    const BlockState existing = stateAt(level, position);
    if (existing.mName != getIdentifier() || isTopSlab(existing) == top)
        return PlacementMergeResult::None;

    if (doubleSlab == nullptr)
        return PlacementMergeResult::Rejected;

    state = doubleSlab->toBlockState();
    return PlacementMergeResult::Merged;
}

bool DoubleSlabBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, DOUBLE_SLAB_SUFFIX)
           || BlockIdentifier::endsWith(identifier, DOUBLE_COPPER_SLAB_SUFFIX);
}

std::string DoubleSlabBlock::getSlabIdentifier() const {
    if (BlockIdentifier::endsWith(getIdentifier(), DOUBLE_COPPER_SLAB_SUFFIX))
        return replaceSuffix(getIdentifier(), DOUBLE_COPPER_SLAB_SUFFIX, COPPER_SLAB_SUFFIX);

    return replaceSuffix(getIdentifier(), DOUBLE_SLAB_SUFFIX, SLAB_SUFFIX);
}

std::string DoubleSlabBlock::getResourceItem(const BlockState &state) const {
    (void) state;
    return getSlabIdentifier();
}

int32_t DoubleSlabBlock::getResourceCount(const BlockState &state) const {
    (void) state;
    return DOUBLE_SLAB_RESOURCE_COUNT;
}

bool CandleBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:candle" || BlockIdentifier::endsWith(identifier, "_candle");
}

PlacementMergeResult CandleBlock::mergePlacement(Level &level, const Vector3i &clickedPosition, int blockFace,
                                                 const Vector3f &clickPosition, Vector3i &position,
                                                 BlockState &state) const {
    (void) blockFace;
    (void) clickPosition;

    Vector3i candidate = clickedPosition;
    BlockState existing = stateAt(level, candidate);
    if (!matches(existing.mName)) {
        const Vector3i above = PlacementOrientation::relativePosition(clickedPosition, PlacementOrientation::FACE_UP);
        const BlockState aboveState = stateAt(level, above);
        if (matches(aboveState.mName)) {
            candidate = above;
            existing = aboveState;
        } else {
            candidate = position;
            existing = stateAt(level, position);
        }
    }

    if (existing.mName == getIdentifier()) {
        const int32_t candles = existing.mStates.getInt(CANDLES, 0);
        if (candles >= CANDLES_MAX)
            return PlacementMergeResult::Rejected;

        Tag states = existing.mStates;
        states.putInt(CANDLES, candles + 1);
        position = candidate;
        state = BlockState(existing.mName, states);
        return PlacementMergeResult::Merged;
    }

    if (matches(existing.mName))
        return PlacementMergeResult::Rejected;

    return PlacementMergeResult::None;
}

bool ScaffoldingBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:scaffolding";
}

Vector3i ScaffoldingBlock::resolvePlacementPosition(Level &level, const Vector3i &position, int blockFace) const {
    if (blockFace != PlacementOrientation::FACE_UP)
        return position;

    Vector3i resolved = position;
    while (resolved.y <= level.getMaxY() && stateAt(level, resolved).mName == getIdentifier())
        resolved.y++;

    return resolved;
}

bool ScaffoldingBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    using namespace PlacementOrientation;

    if (LiquidView(stateAt(level, position)).isLava())
        return false;

    const BlockState clicked = stateAt(level, BlockSupport::supportOf(position, blockFace));
    const BlockState below = belowOf(level, position);
    if (clicked.mName == getIdentifier() || below.mName == getIdentifier()
        || DecorationSupport::isAir(below) || DecorationSupport::isSolid(below))
        return true;

    for (int side = FACE_NORTH; side <= FACE_EAST; ++side) {
        if (side == blockFace)
            continue;

        if (stateAt(level, relativePosition(position, side)).mName == getIdentifier())
            return true;
    }

    return false;
}

void ScaffoldingBlock::onPlacing(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                 BlockState &state) const {
    (void) owner;
    (void) level;
    (void) position;

    if (!state.mStates.contains(STABILITY_CHECK))
        return;

    Tag states = state.mStates;
    states.putByte(STABILITY_CHECK, 1);
    state = BlockState(state.mName, states);
}

void ScaffoldingBlock::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state) const {
    using namespace PlacementOrientation;

    if (!state.mStates.contains(STABILITY))
        return;

    if (DecorationSupport::isSolid(belowOf(level, position))) {
        if (state.mStates.getInt(STABILITY, 0) == 0 && state.mStates.getByte(STABILITY_CHECK, 0) == 0)
            return;

        Tag states = state.mStates;
        states.putInt(STABILITY, 0);
        states.putByte(STABILITY_CHECK, 0);
        level.setBlock(position, BlockState(state.mName, states), false);
        return;
    }

    int stability = UNSTABLE_STABILITY;
    for (int face = FACE_DOWN; face <= FACE_EAST; ++face) {
        if (face == FACE_UP)
            continue;

        const BlockState side = stateAt(level, relativePosition(position, face));
        if (side.mName != state.mName)
            continue;

        const int sideStability = side.mStates.getInt(STABILITY, UNSTABLE_STABILITY);
        if (sideStability >= stability)
            continue;

        stability = face == FACE_DOWN ? sideStability : sideStability + 1;
    }

    if (stability >= UNSTABLE_STABILITY) {
        BlockActionHandler::destroyBlock(owner, level, position, state, true, ItemStack::air());
        return;
    }

    if (state.mStates.getInt(STABILITY, 0) == stability && state.mStates.getByte(STABILITY_CHECK, 0) == 0)
        return;

    Tag states = state.mStates;
    states.putInt(STABILITY, stability);
    states.putByte(STABILITY_CHECK, 0);
    level.setBlock(position, BlockState(state.mName, states), false);
}

bool CarpetBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_carpet");
}

bool CarpetBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    return !DecorationSupport::isAir(belowOf(level, position));
}

bool CarpetBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

bool PressurePlateBlock::matches(const std::string &identifier) {
    return BlockIdentifier::endsWith(identifier, "_pressure_plate");
}

bool PressurePlateBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    const BlockState below = belowOf(level, position);
    return BlockSupport::isAttachable(below, PlacementOrientation::FACE_UP)
           || VanillaBlocks::getAs<FenceBlock>(below.mName) != nullptr;
}

bool PressurePlateBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
}

bool RedStoneWireBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:redstone_wire";
}

bool RedStoneWireBlock::canPlaceAt(Level &level, const Vector3i &position, int blockFace) const {
    (void) blockFace;

    return DecorationSupport::isSolid(belowOf(level, position));
}

bool RedStoneWireBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    return canPlaceAt(level, position, PlacementOrientation::FACE_UP);
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
        || VanillaBlocks::getAs<ThinFenceBlock>(support.mName) != nullptr
        || BlockIdentifier::endsWith(support.mName, "_leaves") || support.mName == "minecraft:beacon")
        return false;

    return BlockSupport::isAttachable(support, blockFace);
}

bool LadderBlock::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    if (!state.mStates.contains("facing_direction"))
        return true;

    return canPlaceAt(level, position, state.mStates.getInt("facing_direction"));
}
