#include "Actor/Mob/Hostile/CreeperActor.h"

#include "Actor/AI/Goal/FloatGoal.h"
#include "Actor/AI/Goal/HurtByTargetGoal.h"
#include "Actor/AI/Goal/LookAtPlayerGoal.h"
#include "Actor/AI/Goal/MeleeAttackGoal.h"
#include "Actor/AI/Goal/NearestAttackableTargetGoal.h"
#include "Actor/AI/Goal/RandomStrollGoal.h"
#include "Actor/AI/Goal/SwellGoal.h"
#include "Actor/ActorClassRegistry.h"
#include "Actor/ActorFlags.h"
#include "Actor/ServerPlayer.h"
#include "Level/Explosion.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <string>
#include <vector>

FALCON_REGISTER_ACTOR(CreeperActor, CreeperActor::IDENTIFIER);

namespace {
    const int32_t FLOAT_PRIORITY = 1;
    const int32_t SWELL_PRIORITY = 2;
    const int32_t MELEE_PRIORITY = 4;
    const float MELEE_SPEED = 0.25f;
    const int32_t MELEE_COOL_DOWN = 20;
    const float NO_ATTACK_RANGE_SQUARED = -1.0f;
    const int32_t STROLL_PRIORITY = 5;
    const float STROLL_SPEED = 0.2f;
    const int32_t STROLL_RANGE = 12;
    const int32_t STROLL_INTERVAL = 100;
    const int32_t STROLL_WATER_RETRIES = 10;
    const int32_t LOOK_PRIORITY = 6;
    const float LOOK_RANGE = 8.0f;
    const int32_t LOOK_PROBABILITY = 4;
    const int32_t LOOK_PROBABILITY_TOTAL = 10;
    const int32_t LOOK_DURATION = 100;
    const int32_t LOOK_CHECK_INTERVAL = 100;
    const int32_t NEAREST_TARGET_PRIORITY = 1;
    const int32_t HURT_BY_TARGET_PRIORITY = 2;
    const float TARGET_RANGE = 16.0f;

    const double EXPLOSION_RADIUS = 3.0;
    const double CHARGED_MULTIPLIER = 2.0;
    const int32_t FUSE_SYNC_INTERVAL = 5;
    const char *const FUSE_SOUND = "random.fuse";
    const char *const IGNITE_SOUND = "fire.ignite";
    const char *const FLINT_AND_STEEL = "minecraft:flint_and_steel";
    const char *const TAG_POWERED = "powered";
    const char *const TAG_IGNITED = "IsFuseLit";
    const char *const TAG_SWELL = "Fuse";

    struct HeadDrop {
        const char *mActor;
        const char *mHead;
    };

    const HeadDrop HEAD_DROPS[] = {
            {"minecraft:zombie", "minecraft:zombie_head"},
            {"minecraft:skeleton", "minecraft:skeleton_skull"},
            {"minecraft:creeper", "minecraft:creeper_head"},
            {"minecraft:wither_skeleton", "minecraft:wither_skeleton_skull"},
            {"minecraft:piglin", "minecraft:piglin_head"}
    };

    const char *headOf(const std::string &identifier) {
        for (const HeadDrop &drop: HEAD_DROPS) {
            if (identifier == drop.mActor)
                return drop.mHead;
        }

        return nullptr;
    }
}

void CreeperActor::registerGoals(GoalSelector &goalSelector) {
    goalSelector.addGoal(FLOAT_PRIORITY, std::make_unique<FloatGoal>());
    goalSelector.addGoal(SWELL_PRIORITY, std::make_unique<SwellGoal>());
    goalSelector.addGoal(MELEE_PRIORITY, std::make_unique<MeleeAttackGoal>(MELEE_SPEED, TARGET_RANGE,
                                                                           MELEE_COOL_DOWN,
                                                                           NO_ATTACK_RANGE_SQUARED));
    goalSelector.addGoal(STROLL_PRIORITY, std::make_unique<RandomStrollGoal>(STROLL_SPEED, STROLL_RANGE,
                                                                             STROLL_INTERVAL, true,
                                                                             STROLL_WATER_RETRIES));
    goalSelector.addGoal(LOOK_PRIORITY, std::make_unique<LookAtPlayerGoal>(LOOK_RANGE, LOOK_PROBABILITY,
                                                                           LOOK_PROBABILITY_TOTAL, LOOK_DURATION,
                                                                           LOOK_CHECK_INTERVAL));
    goalSelector.addGoal(NEAREST_TARGET_PRIORITY, std::make_unique<NearestAttackableTargetGoal>(TARGET_RANGE));
    goalSelector.addGoal(HURT_BY_TARGET_PRIORITY, std::make_unique<HurtByTargetGoal>());
}

void CreeperActor::tick(ServerNetworkHandler &owner) {
    if (mExploded)
        return;

    HostileActor::tick(owner);

    if (!isAlive() || isDead())
        return;

    const int32_t direction = mIgnited ? 1 : mSwellDirection;
    if (direction > 0 && mSwell == 0)
        owner.playNamedSound(owner.getLevelFor(*this), FUSE_SOUND, getPosition(), 1.0f, 0.5f);

    mSwell += direction;
    if (mSwell < 0)
        mSwell = 0;

    _syncFuse(owner);

    if (mSwell >= MAX_SWELL)
        _explode(owner);
}

void CreeperActor::_syncFuse(ServerNetworkHandler &owner) {
    const bool visible = mSwell > 0;
    if (visible == mFuseVisible && (!visible || mSwell % FUSE_SYNC_INTERVAL != 0))
        return;

    mFuseVisible = visible;
    getFlags().set(ActorFlag::Ignited, visible);

    EntityDataMap metadata;
    fillSpawnMetadata(metadata);
    owner.sendActorMetadata(*this, metadata);
}

void CreeperActor::_explode(ServerNetworkHandler &owner) {
    mExploded = true;

    Level &level = owner.getLevelFor(*this);
    const Vector3f center = getPosition();
    const double radius = mPowered ? EXPLOSION_RADIUS * CHARGED_MULTIPLIER : EXPLOSION_RADIUS;

    std::vector<int64_t> headCandidates;
    if (mPowered) {
        for (auto &entry: owner.getActors()) {
            ServerActor &actor = *entry.second;
            if (&actor != this && actor.isAlive() && headOf(actor.getTypeId()) != nullptr)
                headCandidates.push_back(entry.first);
        }
    }

    Explosion explosion(owner, level, center, radius, this, false);
    if (level.getGameRules().getBool("mobgriefing"))
        explosion.explode();
    else
        explosion.explodeWithoutBlocks();

    _dropChargedHead(owner, headCandidates);
}

void CreeperActor::_dropChargedHead(ServerNetworkHandler &owner, const std::vector<int64_t> &candidates) {
    for (const int64_t id: candidates) {
        const auto found = owner.getActors().find(id);
        if (found == owner.getActors().end())
            continue;

        ServerActor &actor = *found->second;
        if (actor.isAlive() && !actor.isDead())
            continue;

        owner.spawnItemActor(owner.getLevelFor(actor), headOf(actor.getTypeId()), 1, actor.getPosition());
        return;
    }
}

bool CreeperActor::onInteract(ServerNetworkHandler &owner, ServerPlayer &player) {
    const ItemStack &held = player.getInventory().getItemInHand();
    if (mIgnited || held.isAir() || held.mDefinition == nullptr
        || held.mDefinition->getIdentifier() != FLINT_AND_STEEL)
        return HostileActor::onInteract(owner, player);

    mIgnited = true;
    owner.playNamedSound(owner.getLevelFor(*this), IGNITE_SOUND, getPosition(), 1.0f, 1.0f);
    owner.damagePlayerHeldItem(player, 1);
    return true;
}

void CreeperActor::onStruckByLightning(ServerNetworkHandler &owner) {
    if (mPowered)
        return;

    mPowered = true;
    getFlags().set(ActorFlag::Powered, true);
    owner.syncActorFlags(*this);
}

void CreeperActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    EntityDataEntry flags;
    flags.mId = ActorFlags::FLAGS_DATA_ID;
    flags.mFormat = EntityDataFormat::Long;
    flags.mLongValue = getFlags().getLowBits();
    metadata.mEntries.push_back(flags);

    EntityDataEntry flags2;
    flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
    flags2.mFormat = EntityDataFormat::Long;
    flags2.mLongValue = getFlags().getHighBits();
    metadata.mEntries.push_back(flags2);

    EntityDataEntry fuse;
    fuse.mId = ActorFlags::FUSE_LENGTH_DATA_ID;
    fuse.mFormat = EntityDataFormat::Int;
    fuse.mIntValue = mSwell;
    metadata.mEntries.push_back(fuse);
}

Tag CreeperActor::saveNbt() const {
    Tag data = HostileActor::saveNbt();
    data.putByte(TAG_POWERED, mPowered ? 1 : 0);
    data.putByte(TAG_IGNITED, mIgnited ? 1 : 0);
    data.putByte(TAG_SWELL, (int8_t) mSwell);
    return data;
}

void CreeperActor::loadNbt(const Tag &data) {
    HostileActor::loadNbt(data);
    mPowered = data.getByte(TAG_POWERED, 0) != 0;
    mIgnited = data.getByte(TAG_IGNITED, 0) != 0;
    mSwell = data.getByte(TAG_SWELL, 0);
    getFlags().set(ActorFlag::Powered, mPowered);
    getFlags().set(ActorFlag::Ignited, mSwell > 0);
    mFuseVisible = mSwell > 0;
}
