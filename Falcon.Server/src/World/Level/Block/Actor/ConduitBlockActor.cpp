#include "World/Level/Block/Actor/ConduitBlockActor.h"

#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Movement/ActorPushSystem.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/BlockMaterial.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/LiquidBlocksFetch.h"
#include "World/Level/Block/Systems/PrecipitationSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"

#include <cmath>
#include <cstdlib>

namespace {
    const char *const TAG_TARGET = "Target";
    const char *const TAG_ACTIVE = "Active";

    const char *const MONSTER_FAMILY = "monster";

    const int64_t AMBIENT_TICKS = 80;
    const int32_t SHORT_SOUND_SPREAD = 40;
    const int32_t SHORT_SOUND_DELAY = 60;
    // the attack box grows the conduit's block by this much; a target is kept while closer than it
    const float ATTACK_REACH = 8.0f;
    const float SQRT_3 = 1.7320508f;

    // in water over its own box and pose, or rained on at the top of its box
    bool isInWaterOrRain(Level &level, const Actor &actor) {
        const Vector3f &position = actor.getPosition();
        const AxisAlignedBB box = ActorPushSystem::boundingBoxOf(actor);
        const ActorFlags &flags = actor.getFlags();
        const bool horizontal = flags.get(ActorFlag::Swimming) || flags.get(ActorFlag::Gliding)
                                || flags.get(ActorFlag::Crawling);
        if (LiquidBlocksFetch::at(level, position, horizontal, box.mMaxY - box.mMinY).water)
            return true;
        return PrecipitationSystem::isRainingOn(level, Vector3i((int32_t) std::floor(position.x),
                                                                (int32_t) std::floor(box.mMaxY),
                                                                (int32_t) std::floor(position.z)));
    }

    void addIfFrame(Level &level, const Vector3i &position, std::vector<Vector3i> &outFrame) {
        const BlockState *state = level.peekBlockPtr(position.x, position.y, position.z);
        if (state != nullptr && ConduitBlockActor::isFrameBlock(*state))
            outFrame.push_back(position);
    }
}

void ConduitBlockActor::loadNbt(const Tag &data, const PacketCodecContext &context) {
    (void) context;
    const Tag *target = data.get(TAG_TARGET);
    mTarget = target != nullptr && target->getType() == Tag::Type::Long ? target->asLong() : NO_TARGET;
    mActive = data.getByte(TAG_ACTIVE, 0) != 0;
}

Tag ConduitBlockActor::saveNbt() const {
    Tag data = Tag::ofCompound();
    data.putLong(TAG_TARGET, mTarget);
    data.putByte(TAG_ACTIVE, mActive ? 1 : 0);
    return data;
}

Tag ConduitBlockActor::getSpawnCompound() const {
    // the client spins the cage and draws the attack beam from these
    Tag data = BlockActor::getSpawnCompound();
    data.putLong(TAG_TARGET, mTarget);
    data.putByte(TAG_ACTIVE, mActive ? 1 : 0);
    return data;
}

bool ConduitBlockActor::isFrameBlock(const BlockState &state) {
    const Block *block = VanillaBlocks::fromState(state);
    return block != nullptr && block->isConduitFrame();
}

bool ConduitBlockActor::isSurroundedByWater(Level &level, const Vector3i &position) {
    for (int32_t x = position.x - 1; x <= position.x + 1; ++x) {
        for (int32_t y = position.y - 1; y <= position.y + 1; ++y) {
            for (int32_t z = position.z - 1; z <= position.z + 1; ++z) {
                if (x == position.x && y == position.y && z == position.z)
                    continue;
                const BlockState *state = level.peekBlockPtr(x, y, z);
                BlockMaterialType material;
                // the material of the block itself: waterlogging does not count
                if (state == nullptr || !BlockMaterial::find(*state, material) || material != BlockMaterialType::Water)
                    return false;
            }
        }
    }
    return true;
}

void ConduitBlockActor::collectFrame(Level &level, const Vector3i &position, std::vector<Vector3i> &outFrame) {
    // the z, y and x planes in turn; a block on two planes is counted once, with the first
    for (int32_t x = -2; x <= 2; ++x) {
        for (int32_t y = -2; y <= 2; ++y) {
            if (std::abs(x) == 2 || std::abs(y) == 2)
                addIfFrame(level, Vector3i(position.x + x, position.y + y, position.z), outFrame);
        }
    }
    for (int32_t x = -2; x <= 2; ++x) {
        for (int32_t z = -2; z <= 2; ++z) {
            if ((std::abs(x) == 2 || std::abs(z) == 2) && z != 0)
                addIfFrame(level, Vector3i(position.x + x, position.y, position.z + z), outFrame);
        }
    }
    for (int32_t y = -2; y <= 2; ++y) {
        for (int32_t z = -2; z <= 2; ++z) {
            if ((std::abs(y) == 2 || std::abs(z) == 2) && z != 0 && y != 0)
                addIfFrame(level, Vector3i(position.x, position.y + y, position.z + z), outFrame);
        }
    }
}

bool ConduitBlockActor::tick(ServerNetworkHandler &owner) {
    if (mLevel == nullptr)
        return true;

    Level &level = *mLevel;
    const BlockState *state = level.peekBlockPtr(mPosition.x, mPosition.y, mPosition.z);
    if (state == nullptr)
        return true;
    if (state->mName != BlockNames::CONDUIT)
        return false;

    // the sounds play at the block's corner, as the game does
    const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
    const int64_t worldTick = level.getWorldTick();
    if (worldTick % AMBIENT_TICKS == 0 && mActive)
        owner.playLevelSound(level, "conduit.ambient", corner);
    if (worldTick > mNextShortSoundTick && mActive) {
        mNextShortSoundTick = worldTick + MobActor::randomRange(0, SHORT_SOUND_SPREAD - 1) + SHORT_SOUND_DELAY;
        owner.playLevelSound(level, "conduit.short", corner);
    }

    if (mCountdown-- > 1)
        return true;

    mCountdown = UPDATE_TICKS;
    _update(owner, level);
    return true;
}

void ConduitBlockActor::_update(ServerNetworkHandler &owner, Level &level) {
    const bool wasActive = mActive;
    mActive = false;
    mHunting = false;
    mFrame.clear();

    const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
    // only losing the water plays the deactivate sound: a broken frame stops it silently
    if (!isSurroundedByWater(level, mPosition)) {
        if (wasActive)
            owner.playLevelSound(level, "conduit.deactivate", corner);
        return;
    }

    collectFrame(level, mPosition, mFrame);
    const int32_t frameBlocks = (int32_t) mFrame.size();
    mEffectRange = rangeFor(frameBlocks);
    mHunting = frameBlocks >= HUNTING_FRAME;
    if (frameBlocks >= ACTIVE_FRAME) {
        mActive = true;
        if (!wasActive)
            owner.playLevelSound(level, "conduit.activate", corner);
        _applyEffects(owner, level);
        _attack(owner, level);
    }
    BlockActionHandler::broadcastBlockActorData(owner, level, *this);
}

void ConduitBlockActor::_applyEffects(ServerNetworkHandler &owner, Level &level) {
    const float range = (float) mEffectRange;
    const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);

    owner.forEachPlayerNear(corner, range * SQRT_3 + 1.0f, [&](ServerPlayer &player) {
        if (!player.isSpawned() || player.isDead() || !player.isIn(level) || !isInWaterOrRain(level, player))
            return;

        // a cube around the block's corner, edges included, against the eyes
        const Vector3f &feet = player.getPosition();
        const float eyes = feet.y + MovementPhysics::PLAYER_BASE_OFFSET;
        if (feet.x < corner.x - range || feet.x > corner.x + range || eyes < corner.y - range
            || eyes > corner.y + range || feet.z < corner.z - range || feet.z > corner.z + range)
            return;

        MobEffectInstance instance;
        instance.mId = MobEffectId::ConduitPower;
        instance.mAmplifier = 0;
        instance.mDuration = EFFECT_TICKS;
        instance.mAmbient = true;
        owner.addEffect(player, instance);
    });
}

void ConduitBlockActor::_attack(ServerNetworkHandler &owner, Level &level) {
    if (!mHunting)
        mTarget = NO_TARGET;

    const Vector3f corner((float) mPosition.x, (float) mPosition.y, (float) mPosition.z);
    ServerActor *target = mTarget == NO_TARGET ? nullptr : owner.getActor(mTarget);
    if (target != nullptr) {
        const Vector3f &position = target->getPosition();
        const float dx = corner.x - position.x;
        const float dy = corner.y - position.y;
        const float dz = corner.z - position.z;
        if (!target->isAlive() || !target->isIn(level) || !isInWaterOrRain(level, *target)
            || dx * dx + dy * dy + dz * dz >= ATTACK_REACH * ATTACK_REACH) {
            mTarget = NO_TARGET;
            return;
        }

        owner.playLevelSound(level, "conduit.attack", corner);
        owner.damageActor(*target, ATTACK_DAMAGE,
                          ActorDamageSource::environment("death.attack.magic", target->getName()).withoutArmor());
        return;
    }

    // no target, or one gone from the world: pick a new one, the attack waits for the next update
    if (!mHunting)
        return;

    const AxisAlignedBB area((float) mPosition.x - ATTACK_REACH, (float) mPosition.y - ATTACK_REACH,
                             (float) mPosition.z - ATTACK_REACH, (float) mPosition.x + 1.0f + ATTACK_REACH,
                             (float) mPosition.y + 1.0f + ATTACK_REACH, (float) mPosition.z + 1.0f + ATTACK_REACH);
    const Vector3f center((float) mPosition.x + 0.5f, (float) mPosition.y + 0.5f, (float) mPosition.z + 0.5f);
    std::vector<int64_t> candidates;
    owner.forEachActorNear(center, (ATTACK_REACH + 0.5f) * SQRT_3 + 2.0f, [&](ServerActor &actor) {
        if (!actor.isAlive() || !actor.isIn(level) || !EntityFilter::hasFamily(actor, MONSTER_FAMILY)
            || !ActorPushSystem::boundingBoxOf(actor).intersectsWith(area) || !isInWaterOrRain(level, actor))
            return;
        candidates.push_back(actor.getUniqueId());
    });
    if (!candidates.empty())
        mTarget = candidates[(size_t) MobActor::randomRange(0, (int) candidates.size() - 1)];
}
