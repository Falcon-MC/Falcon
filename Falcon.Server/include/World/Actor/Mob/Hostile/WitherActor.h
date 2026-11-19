#pragma once

#include "World/Actor/Mob/Hostile/HostileActor.h"

#include "Core/Math/AxisAlignedBB.h"
#include "Core/Math/Vector3i.h"

#include <array>
#include <cstdint>
#include <random>
#include <unordered_map>
#include <unordered_set>

class Level;
class ServerPlayer;

// the 200 spawn frames, three heads picking targets and shooting skulls, the dives above half health and the
// charges below it, the blocks it breaks when hurt, and 200 death frames ending in an explosion
class WitherActor : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:wither";

    static constexpr float EASY_HEALTH = 300.0f;

    static constexpr float NORMAL_HEALTH = 450.0f;

    static constexpr float HARD_HEALTH = 600.0f;

    static constexpr int32_t HEAD_COUNT = 3;

    static constexpr int32_t SPAWNING_FRAMES = 200;

    static constexpr int32_t DEATH_FRAMES = 200;

    WitherActor(uint64_t runtimeId, const std::string &identifier);

    ActorSize getSize() const override {
        return ActorSize{1.0f, 3.0f};
    }

    float getDefaultMaxHealth() const override {
        return HARD_HEALTH;
    }

    float resolveMaxHealth(Difficulty difficulty) const override;

    int getExperienceDrop() const override {
        return 50;
    }

    bool hasGravity() const override {
        return false;
    }

    // the game sets the movement attribute to 0.6 once the spawn frames are over
    float getMovementSpeed() const override;

    // the frames run their own course, ending in remove()
    int32_t getDeathDuration() const override {
        return DEATH_FRAMES + 1;
    }

    void finalizeSpawn() override;

    void tick(ServerNetworkHandler &owner) override;

    void tickMovement(ServerNetworkHandler &owner) override;

    void tickDeath(ServerNetworkHandler &owner) override;

    bool senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) override;

    void onDamaged(ServerNetworkHandler &owner, Actor *attacker) override;

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    // above half health only, and then only instant health and instant damage
    bool canBeAffected(MobEffectId id) const override;

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

    // the dive WitherRandomAttackPosGoal flies
    bool isAerialAttackArmed() const {
        return mAerialAttackArmed;
    }

    void setFlyingToAttackPosition(bool flying) {
        mFlyingToAttackPosition = flying;
    }

    void endAerialAttack() {
        mAerialAttackArmed = false;
        mFlyingToAttackPosition = false;
        mAttackCooldownTicks = 20;
    }

    // the player who dealt the most damage, among those still around
    ServerPlayer *findHighestDamagePlayer(ServerNetworkHandler &owner) const;

    // the wither itself, its skull or its explosion: the game reads it from the damage's responsible actor
    static bool isWitherDamage(ServerNetworkHandler &owner, const ActorDamageSource &source);

    // a wither rose in the victim's block (a player's eyes) when it can grow there and mob griefing allows it,
    // dropped otherwise
    static void leaveWitherRose(ServerNetworkHandler &owner, const Actor &victim);

    // the T of soul sand or soul soil under three wither skeleton skulls, the skull at position just placed
    static bool trySummon(ServerNetworkHandler &owner, Level &level, const Vector3i &position);

    // a dispenser places its wither skeleton skull only in air, next to the soul blocks of the T
    static bool canDispenseSkull(Level &level, const Vector3i &position);

protected:
    bool runsGoals() const override {
        return mSpawningFrames <= 0;
    }

    bool dropsRewardsOnDeath() const override {
        return false;
    }

private:
    void _startSpawning(ServerNetworkHandler &owner);

    void _initializeHealthSteps();

    // vertical motion, the charge and the body yaw, before the movement of the tick
    void _preAiStep(ServerNetworkHandler &owner);

    void _tickSpawning(ServerNetworkHandler &owner, Level &level);

    void _serverAiStep(ServerNetworkHandler &owner, Level &level);

    void _tickHeads(ServerNetworkHandler &owner, Level &level, Actor *target, bool shoots);

    void _tickCharge(ServerNetworkHandler &owner, Level &level);

    void _explodeAtHalfHealth(ServerNetworkHandler &owner, Level &level);

    void _explode(ServerNetworkHandler &owner, Level &level, const Vector3f &center);

    // head 0 sits on top, heads 1 and 2 on either side
    Vector3f _headPosition(int32_t head) const;

    void _shootSkull(ServerNetworkHandler &owner, Level &level, int32_t head, const Vector3f &target, bool dangerous);

    void _setHeadTarget(ServerNetworkHandler &owner, int32_t head, uint64_t runtimeId);

    void _breakBlocks(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area, int32_t inflate,
                      bool charging);

    void _hurtCharged(ServerNetworkHandler &owner, Level &level);

    void _setInvulnerableTicks(ServerNetworkHandler &owner, int32_t ticks);

    void _setAirAttack(ServerNetworkHandler &owner, bool airAttack);

    void _sendData(ServerNetworkHandler &owner, int32_t id) const;

    void _appendData(EntityDataMap &metadata, int32_t id) const;

    void _addDamage(const Actor *attacker, float amount);

    void _tickBossBar(ServerNetworkHandler &owner);

    void _sendBossEvent(ServerNetworkHandler &owner, ServerPlayer &player, int32_t action);

    void _clearBossBar(ServerNetworkHandler &owner);

    void _awardSpawnAchievement(ServerNetworkHandler &owner);

    // the boss killed event, to every player who damaged it
    void _sendBossKilled(ServerNetworkHandler &owner);

    int32_t _nextInt(int32_t bound);

    float _nextFloat();

    bool mSpawnPending = false;
    bool mHealthStepsReady = false;

    // the synced data the client draws the heads, the armor and the flashing from
    int32_t mInvulnerableTicks = 0;
    std::array<uint64_t, HEAD_COUNT> mHeadTargets{};
    std::array<int64_t, HEAD_COUNT> mHeadTargetUniqueIds{-1, -1, -1};
    bool mAirAttack = false;
    int32_t mSpawningFrames = SPAWNING_FRAMES;

    std::array<int32_t, HEAD_COUNT> mNextHeadUpdate{};
    std::array<int32_t, HEAD_COUNT> mIdleHeadUpdates{};
    int32_t mBlockBreakTicks = 0;
    int32_t mPhaseHealth = 0;
    int32_t mPhase = 1;
    bool mHalfHealthExplosionPending = false;
    bool mCharging = false;
    Vector3f mChargeDirection;
    int32_t mChargeTicks = 0;
    int32_t mChargeWindupTicks = 0;
    int32_t mShotCount = 0;
    int32_t mFireRate = 20;
    float mSpinSpeed = 10.0f;
    int32_t mFrozenTicks = 0;
    int32_t mAerialAttackTicks = 0;
    bool mAerialAttackArmed = false;
    bool mFlyingToAttackPosition = false;
    int32_t mSavedMaxHealth = 0;
    int32_t mAerialAttackInterval = 40;
    int32_t mHealthStep = -1;
    int32_t mLastHealthStep = -1;
    int32_t mAttackCooldownTicks = 0;
    int32_t mIdleTargetTicks = 0;
    float mHeadRange = 30.0f;
    bool mSkipCharge = false;
    int32_t mShootTicks = 20;
    int32_t mMaxShieldHealth = 40;
    int32_t mShieldHealth = 40;

    // the damage each player dealt, by runtime id
    std::unordered_map<uint64_t, int32_t> mDamageDealt;

    int32_t mDeathFlashInterval = 15;
    uint64_t mKillerRuntimeId = 0;
    int32_t mKillerLooting = 0;

    std::unordered_set<int64_t> mBossViewers;
    float mBossPercent = -1.0f;
    std::mt19937 mRandom;
};
