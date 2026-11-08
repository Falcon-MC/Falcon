#pragma once

#include "World/Actor/Mob/Hostile/HostileActor.h"

#include "Core/Math/AxisAlignedBB.h"

#include <array>
#include <functional>
#include <memory>
#include <optional>
#include <random>
#include <unordered_set>
#include <vector>

class EndCrystalActor;
class ServerPlayer;

/**
 * The dragon of the End. Its goals (DragonGoals) choose where it flies; the dragon itself steers toward that point,
 * moves its body parts, heals from the spike crystals and breaks the blocks it flies through (sub_14ACCA850).
 */
class EnderDragon : public HostileActor {
public:
    static constexpr const char *IDENTIFIER = "minecraft:ender_dragon";

    static constexpr int32_t NODE_COUNT = 24;

    static constexpr float DEFAULT_FLY_SPEED = 0.6f;

    enum class Part {
        Head,
        Neck,
        Body,
        Tail1,
        Tail2,
        Tail3,
        Wing1,
        Wing2,
        Count
    };

    /** A path over the dragon's flight nodes, walked one node at a time. */
    class Path {
    public:
        explicit Path(std::vector<Vector3i> nodes) : mNodes(std::move(nodes)) {
        }

        bool isDone() const {
            return mIndex >= mNodes.size();
        }

        const Vector3i &getNextNode() const {
            return mNodes[mIndex];
        }

        void advance() {
            mIndex++;
        }

        void finish() {
            mIndex = mNodes.size();
        }

        const std::vector<Vector3i> &getNodes() const {
            return mNodes;
        }

    private:
        std::vector<Vector3i> mNodes;
        size_t mIndex = 0;
    };

    EnderDragon(uint64_t runtimeId, const std::string &identifier);

    bool shouldRemoveInPeaceful() const override {
        return MobActor::shouldRemoveInPeaceful();
    }

    ActorSize getSize() const override {
        return ActorSize{13.0f, 4.0f};
    }

    float getDefaultMaxHealth() const override {
        return 200.0f;
    }

    bool preventsSleep() const override {
        return false;
    }

    // the dragon's experience falls during its death (sub_144AE8C40), not when it dies
    int getExperienceDrop() const override {
        return 0;
    }

    bool hasGravity() const override {
        return false;
    }

    bool isPushable() const override {
        return false;
    }

    int32_t getDeathDuration() const override {
        return 200;
    }

    EntityEventType getDeathEvent() const override {
        return EntityEventType::EnderDragonDeath;
    }

    void tick(ServerNetworkHandler &owner) override;

    void tickMovement(ServerNetworkHandler &owner) override;

    void tickDeath(ServerNetworkHandler &owner) override;

    bool senseDamage(ServerNetworkHandler &owner, float &amount, const ActorDamageSource &source) override;

    void onDamaged(ServerNetworkHandler &owner, Actor *attacker) override;

    void kill(ServerNetworkHandler &owner, ServerPlayer *source = nullptr, int32_t lootingLevel = 0) override;

    /** Hurts the dragon through one of its parts, the head taking the full damage (sub_14ACCD880). */
    bool hurtPart(ServerNetworkHandler &owner, Part part, float amount, const ActorDamageSource &source);

    void onCrystalDestroyed(ServerNetworkHandler &owner, EndCrystalActor &crystal, Actor *attacker);

    /** The death goal reached the exit portal: the dragon dies there. */
    void finishDying(ServerNetworkHandler &owner);

    const AxisAlignedBB &getPartBox(Part part) const {
        return mParts[(size_t) part];
    }

    Vector3f getPartCenter(Part part) const;

    const std::optional<Vector3f> &getTargetLocation() const {
        return mTargetLocation;
    }

    void setTargetLocation(const std::optional<Vector3f> &target) {
        mTargetLocation = target;
    }

    void setTurnSpeed(float turnSpeed) {
        mTurnSpeed = turnSpeed;
    }

    void setFlySpeed(float flySpeed) {
        mFlySpeed = flySpeed;
    }

    float getYawAcceleration() const {
        return mYawAcceleration;
    }

    void setYawAcceleration(float acceleration) {
        mYawAcceleration = acceleration;
    }

    bool isSitting() const;

    void setSitting(ServerNetworkHandler &owner, bool sitting);

    int32_t getCrystalsAlive() const {
        return mCrystalsAlive;
    }

    void setCrystalsAlive(int32_t crystals) {
        mCrystalsAlive = crystals;
    }

    void setPreviouslyKilled(bool previouslyKilled) {
        mPreviouslyKilled = previouslyKilled;
    }

    int32_t getFlameCount() const {
        return mFlameCount;
    }

    void addFlame() {
        mFlameCount++;
    }

    void resetFlameCount() {
        mFlameCount = 0;
    }

    /** Fires one of the dragon's definition triggers, such as minecraft:on_start_landing. */
    void fireTrigger(ServerNetworkHandler &owner, const char *component);

    float nextFloat();

    int32_t nextInt(int32_t bound);

    /** The air block above the top of the exit portal's column, where the dragon lands (heightmap at 0, 0). */
    static int32_t getPortalY(Level &level);

    /** The flight node closest to the dragon, from the ring the alive crystals allow. */
    int32_t findClosestNode(ServerNetworkHandler &owner);

    int32_t findClosestNode(ServerNetworkHandler &owner, float x, float y, float z);

    /** A path between two flight nodes, ending on finalNode when one is given (sub_14ACCFA90). */
    std::unique_ptr<Path> findPath(int32_t from, int32_t to, const std::optional<Vector3i> &finalNode);

    /** The nodes of the dragon's flight graph once built, for tests: 12 outer, 8 middle and 4 inner. */
    const std::vector<Vector3i> &getNodes() const {
        return mNodes;
    }

    static uint32_t getNodeAdjacency(int32_t node);

    /** The circle of flight nodes, from the heights of the island under them. */
    void buildNodes(const std::function<int32_t(int32_t x, int32_t z)> &surfaceAt, int32_t seaLevel);

    /** The nearest player within range of position, who is not a spectator. */
    static ServerPlayer *nearestPlayer(ServerNetworkHandler &owner, const Level &level, const Vector3f &position,
                                       float range);

    void shootFireball(ServerNetworkHandler &owner, const Actor &target);

private:
    struct LatencyPosition {
        float mYaw;
        float mY;
    };

    LatencyPosition _getLatencyPosition(int32_t offset) const;

    bool _ensureNodes(ServerNetworkHandler &owner);

    void _tickCrystals(ServerNetworkHandler &owner, Level &level);

    void _tickSounds(ServerNetworkHandler &owner, Level &level);

    void _steer(ServerNetworkHandler &owner);

    void _tickParts(ServerNetworkHandler &owner, Level &level);

    void _setPart(Part part, float x, float y, float z);

    void _knockBack(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area);

    void _hurtTouching(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area);

    bool _breakBlocks(ServerNetworkHandler &owner, Level &level, const AxisAlignedBB &area);

    void _tickBossBar(ServerNetworkHandler &owner);

    void _sendBossEvent(ServerNetworkHandler &owner, ServerPlayer &player, int32_t action);

    void _clearBossBar(ServerNetworkHandler &owner);

    void _notifyFight(ServerNetworkHandler &owner);

    std::array<AxisAlignedBB, (size_t) Part::Count> mParts;
    std::array<LatencyPosition, 64> mPositions{};
    int32_t mPositionPointer = -1;
    std::optional<Vector3f> mTargetLocation;
    float mTurnSpeed = 0.7f;
    float mFlySpeed = DEFAULT_FLY_SPEED;
    float mYawAcceleration = 0.0f;
    float mFlapTime = 0.0f;
    float mPreviousFlapTime = 0.0f;
    int32_t mGrowlTime = 100;
    bool mInWall = false;
    float mSittingDamage = 0.0f;
    float mHealthBeforeHurt = 0.0f;
    int64_t mNearestCrystal = -1;
    int32_t mCrystalsAlive = 0;
    bool mPreviouslyKilled = false;
    int32_t mFlameCount = 0;
    bool mDying = false;
    Part mHurtPart = Part::Body;
    bool mDeathRequested = false;
    bool mFightNotified = false;
    std::vector<Vector3i> mNodes;
    std::unordered_set<int64_t> mBossViewers;
    float mBossPercent = -1.0f;
    std::mt19937 mRandom;
};
