#include "World/Level/Block/BellBlock.h"

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/OrientationHelpers.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/Actor/BellBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>
#include <memory>

FALCON_REGISTER_BLOCK(BellBlock, 200);

namespace {
    const char *const SOUND_BELL_HIT = "block.bell.hit";

    // the ring direction the bell swings to for each axis the player faces (byte_14F0F3DA0)
    const int32_t RING_DIRECTION[] = {2, 3, 0, 1};

    // the player's facing axis from its yaw: 0 and 2 along z, 1 and 3 along x (sub_14315C7C0)
    int32_t facingOf(float yaw) {
        const float radians = yaw * 0.017453292f;
        const float sine = std::sin(radians);
        const float cosine = std::cos(radians);
        if (std::fabs(sine) <= std::fabs(cosine))
            return cosine < 0.0f ? 2 : 0;
        return sine <= 0.0f ? 3 : 1;
    }

    // a bell placed by a player, or one from before bells had an actor here, gets it on first need
    BellBlockActor &bellAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state) {
        if (BellBlockActor *existing = level.getBlockActors().find<BellBlockActor>(position))
            return *existing;
        std::unique_ptr<BellBlockActor> created(new BellBlockActor());
        created->setPosition(position);
        created->setState(state);
        BellBlockActor &inserted = *created;
        level.getBlockActors().insert(std::move(created));
        BlockActionHandler::broadcastBlockActorData(owner, level, inserted);
        return inserted;
    }

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

bool BellBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:bell";
}

BlockState BellBlock::applyPlacementOrientation(const BlockState &state, const BlockPlacementContext &context) const {
    using namespace PlacementOrientation;

    BlockState result = Block::applyPlacementOrientation(state, context);
    Tag states = result.mStates;
    const int facing = context.mFace >= FACE_NORTH ? context.mFace : context.mOppositeFacing;
    OrientationHelpers::setFacingDirection(states, facing);

    if (states.contains("attachment") && context.mFace != FACE_UP && context.mFace != FACE_DOWN
        && isSolidNeighbour(context.mLevel, relativePosition(context.mBlockPosition, context.mFace)))
        states.putString("attachment", "multiple");

    return BlockState(result.mName, states);
}

bool BellBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                           const BlockState &state) const {
    const std::string attachment = state.mStates.getString("attachment", "standing");
    const int32_t direction = state.mStates.getInt("direction", 0);
    const int32_t facing = facingOf(player.getRotation().y);
    const bool sameAxis = (direction & 1) == (facing & 1);

    // hanging always rings; standing rings along its own axis, wall bells across it
    bool rings = attachment == "hanging";
    if (attachment == "standing")
        rings = sameAxis;
    else if (attachment == "side" || attachment == "multiple")
        rings = !sameAxis;
    if (!rings)
        return false;

    Level &level = owner.getLevelFor(player);
    BellBlockActor *bell = &bellAt(owner, level, position, state);

    // then the hit and toggle_bit flipped, as sub_1432FFE10 does
    bell->ring(RING_DIRECTION[facing]);
    BlockActionHandler::broadcastBlockActorData(owner, level, *bell);
    owner.playLevelSound(level, SOUND_BELL_HIT, Vector3f((float) position.x, (float) position.y, (float) position.z));

    Tag states = state.mStates;
    if (states.contains("toggle_bit")) {
        states.putByte("toggle_bit", states.getByte("toggle_bit", 0) != 0 ? 0 : 1);
        const BlockState toggled(state.mName, states);
        level.setBlock(position, toggled, true);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, toggled);
    }
    return true;
}

void BellBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;
    bellAt(owner, owner.getLevelFor(player), position, state);
}
