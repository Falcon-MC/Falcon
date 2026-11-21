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
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <cmath>
#include <memory>

FALCON_REGISTER_BLOCK(BellBlock, 200);

namespace {
    const char *const SOUND_BELL_HIT = "block.bell.hit";
    const char *const HIDE_COMPONENT = "minecraft:hide";
    const char *const WIND_CHARGE = "minecraft:wind_charge_projectile";
    const char *const BREEZE_WIND_CHARGE = "minecraft:breeze_wind_charge_projectile";
    // how far around the bell's block the mobs hear it
    const float ALERT_REACH_XZ = 32.0f;
    const float ALERT_REACH_Y = 12.0f;

    // the ring direction the bell swings to for each axis the player faces
    const int32_t RING_DIRECTION[] = {2, 3, 0, 1};

    // the player's facing axis from its yaw: 0 and 2 along z, 1 and 3 along x
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
        if (state.mName == BlockNames::AIR)
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

// a burst rings it the way it faces, but never while it still swings
bool BellBlock::onWindBurst(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const {
    BellBlockActor &bell = bellAt(owner, level, position, state);
    if (!bell.isRinging())
        ring(owner, level, position, state, state.mStates.getInt("direction", 0), nullptr);
    return true;
}

bool BellBlock::onProjectileHit(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state, ServerActor &projectile) const {
    // a wind charge rings it through its burst instead
    const std::string identifier = projectile.getIdentifier();
    if (identifier == WIND_CHARGE || identifier == BREEZE_WIND_CHARGE)
        return false;
    ring(owner, level, position, state, state.mStates.getInt("direction", 0), nullptr);
    return false;
}

void BellBlock::onRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state) const {
    const bool powered = RedstoneSystem::isGettingPower(owner, level, position);
    BellBlockActor &bell = bellAt(owner, level, position, state);
    if (powered == bell.isPowered())
        return;
    bell.setPowered(powered);
    if (powered)
        ring(owner, level, position, state, state.mStates.getInt("direction", 0), nullptr);
}

void BellBlock::ring(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                     int32_t direction, Actor *source) {
    BellBlockActor &bell = bellAt(owner, level, position, state);
    bell.ring(direction);
    BlockActionHandler::broadcastBlockActorData(owner, level, bell);
    owner.playLevelSound(level, SOUND_BELL_HIT, Vector3f((float) position.x, (float) position.y, (float) position.z));

    Tag states = state.mStates;
    if (states.contains("toggle_bit")) {
        states.putByte("toggle_bit", states.getByte("toggle_bit", 0) != 0 ? 0 : 1);
        const BlockState toggled(state.mName, states);
        level.setBlock(position, toggled, true);
        BlockActionHandler::broadcastBlockUpdate(owner, level, position, toggled);
    }
    const Vector3f centre((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    level.postGameEvent(GameEvents::BLOCK_CHANGE, centre, source);

    if (!bell.canAlert())
        return;
    const AxisAlignedBB heard =
            AxisAlignedBB((float) position.x, (float) position.y, (float) position.z, (float) position.x + 1.0f,
                          (float) position.y + 1.0f, (float) position.z + 1.0f)
                    .expand(ALERT_REACH_XZ, ALERT_REACH_Y, ALERT_REACH_XZ);
    owner.forEachActorNear(centre, ALERT_REACH_XZ * 1.5f, [&](ServerActor &actor) {
        MobActor *mob = dynamic_cast<MobActor *>(&actor);
        if (mob == nullptr || !mob->isIn(level) || mob->getComponent(HIDE_COMPONENT) == nullptr)
            return;
        const ActorSize size = mob->getSize();
        const Vector3f &at = mob->getPosition();
        const float half = size.mWidth * 0.5f;
        if (AxisAlignedBB(at.x - half, at.y, at.z - half, at.x + half, at.y + size.mHeight, at.z + half)
                    .intersectsWith(heard))
            mob->setHeardBell(true);
    });
    bell.startAlertCooldown();
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

    ring(owner, owner.getLevelFor(player), position, state, RING_DIRECTION[facing], &player);
    return true;
}

void BellBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                         const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) usedItem;
    (void) blockFace;
    Level &level = owner.getLevelFor(player);
    bellAt(owner, level, position, state);
    // the game reads the power every tick, so a bell placed beside a live source rings at once
    onRedstonePower(owner, level, position, state);
}

const char *BellBlock::getBlockActorId() const {
    return BellBlockActor::BLOCK_ACTOR_ID;
}
