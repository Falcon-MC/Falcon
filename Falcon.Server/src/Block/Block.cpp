#include "Block/Block.h"

#include "Block/Components/BlockBehavior.h"
#include "Block/Components/BlockBehaviorRegistry.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/BlockData.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Types/ItemStack.h"

#include <cstdlib>

namespace {
    bool isSolidNeighbour(Level *level, const Vector3i &position) {
        if (level == nullptr)
            return false;

        const BlockState state = level->getBlockState(position.x, position.y, position.z);
        if (state.mName == "minecraft:air")
            return false;

        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data != nullptr && data->mSolid;
    }
}

const BlockData *Block::getData() const {
    return BlockDataTable::find(mIdentifier.c_str());
}

BlockState Block::applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    Tag states = state.mStates;
    const int face = context.mFace;
    const int oppositeFacing = context.mOppositeFacing;
    const int ordinal = context.mOrdinal;

    if (states.contains("minecraft:cardinal_direction"))
        states.putString("minecraft:cardinal_direction", cardinalName(oppositeFacing));

    if (states.contains("minecraft:block_face"))
        states.putString("minecraft:block_face", faceName(face));

    if (states.contains("facing_direction"))
        states.putInt("facing_direction", oppositeFacing);

    if (states.contains("minecraft:facing_direction"))
        states.putString("minecraft:facing_direction", faceName(oppositeFacing));

    if (states.contains("lever_direction"))
        states.putString("lever_direction", leverDirection(face, context.mPlayerFacing));

    if (states.contains("torch_facing_direction"))
        states.putString("torch_facing_direction", torchFacingDirection(face));

    if (states.contains("direction")) {
        int direction = ordinal;
        if (states.contains("attachment"))
            direction = face >= FACE_NORTH ? horizontalOrdinal(face) : horizontalOrdinal(oppositeFacing);
        states.putInt("direction", direction);
    }

    if (states.contains("attachment")) {
        const char *attachment = face == FACE_UP ? "standing" : (face == FACE_DOWN ? "hanging" : "side");
        states.putString("attachment", attachment);
    }

    if (states.contains("weirdo_direction"))
        states.putInt("weirdo_direction", weirdoDirection(context.mPlayerFacing));

    if (states.contains("ground_sign_direction"))
        states.putInt("ground_sign_direction", signRotation(context.mYaw));

    if (states.contains("coral_fan_direction"))
        states.putInt("coral_fan_direction", face >= FACE_NORTH ? face - FACE_NORTH : 0);

    if (states.contains("multi_face_direction_bits"))
        states.putInt("multi_face_direction_bits", 1 << face);

    if (states.contains("pillar_axis"))
        states.putString("pillar_axis", pillarAxis(face));

    if (states.contains("upside_down_bit")) {
        const bool upsideDown = face == FACE_DOWN
                                || (face != FACE_UP && context.mClickPosition.y > 0.5f);
        states.putByte("upside_down_bit", upsideDown ? 1 : 0);
    }

    if (states.contains("minecraft:vertical_half")) {
        const bool top = face == FACE_DOWN
                         || (face != FACE_UP && context.mClickPosition.y > 0.5f);
        states.putString("minecraft:vertical_half", top ? "top" : "bottom");
    }

    if (states.contains("hanging")) {
        const bool hanging = face == FACE_DOWN
                             || (face != FACE_UP
                                 && !isSolidNeighbour(context.mLevel, relativePosition(context.mBlockPosition, FACE_DOWN)));
        states.putByte("hanging", hanging ? 1 : 0);
    }

    if (states.contains("upper_block_bit"))
        states.putByte("upper_block_bit", 0);

    if (states.contains("open_bit"))
        states.putByte("open_bit", 0);

    if (states.contains("head_piece_bit"))
        states.putByte("head_piece_bit", 0);

    if (states.contains("door_hinge_bit"))
        states.putByte("door_hinge_bit", 0);

    if (states.contains("orientation"))
        states.putString("orientation", crafterOrientation(context.mPitch, oppositeFacing));

    return BlockState(state.mName, states);
}

std::vector<BlockPlacementEntry> Block::getPlacementBlocks(Level &level, const Vector3i &position,
                                                           const BlockState &state, int playerFacing) const {
    (void) level;
    (void) position;
    (void) state;
    (void) playerFacing;
    return {};
}

std::vector<Vector3i> Block::getAffectedBlocks(Level &level, const Vector3i &position,
                                               const BlockState &state) const {
    (void) level;
    (void) position;
    (void) state;
    return {};
}

bool Block::isShears(const ItemStack &tool) {
    return tool.mDefinition != nullptr && tool.mDefinition->getIdentifier() == "minecraft:shears";
}

std::vector<BlockDrop> Block::grassDrops(const BlockState &state, const ItemStack &tool, int32_t fortuneLevel,
                                         int32_t seedChance) {
    std::vector<BlockDrop> drops;
    if (isShears(tool))
        drops.push_back({state.mName, 1});

    if (rand() % seedChance == 0) {
        const int32_t seedCount = fortuneLevel == 0 ? 1 : 1 + rand() % (fortuneLevel * 2);
        drops.push_back({"minecraft:wheat_seeds", seedCount});
    }

    return drops;
}

bool Block::canSurvive(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) level;
    (void) position;
    (void) state;
    return true;
}

void Block::onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                               const BlockState &state) const {
    if (canSurvive(level, position, state))
        return;

    BlockActionHandler::destroyBlock(owner, level, position, state, true, ItemStack::air());
}

const BlockBehavior &Block::getBehavior() const {
    const BlockData *data = getData();
    if (data != nullptr && data->mBehaviorIdentifier != nullptr)
        return BlockBehaviorRegistry::get(data->mBehaviorIdentifier);
    return BlockBehaviorRegistry::get(mIdentifier);
}

float Block::getFrictionFactor() const {
    return getBehavior().getFrictionFactor();
}

bool Block::onEntityLand(Actor &actor, float downwardVelocity) const {
    return getBehavior().onEntityLand(actor, downwardVelocity);
}

std::optional<float> Block::getFallDamage(const Actor &actor, float vanillaFallDamage) const {
    return getBehavior().getFallDamage(actor, vanillaFallDamage);
}
