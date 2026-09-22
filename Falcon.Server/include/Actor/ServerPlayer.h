#pragma once

#include "Actor/Actor.h"
#include "Actor/DynamicPropertyValue.h"
#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "Inventory/InventoryManager.h"
#include "Level/Dimension.h"
#include "Inventory/PlayerInventory.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/NetworkIdentifier.h"
#include "Network/PacketSender.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Types/AdventureSettingData.h"
#include "Protocol/Types/SerializedSkin.h"
#include "Actor/MobEffect.h"

#include <chrono>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class ServerNetworkHandler;

class ServerPlayer : public Actor {
public:
    enum class LoginState : int {
        Connecting = 0,
        NetworkSettingsSent = 1,
        EncryptionHandshake = 2,
        LoggedIn = 3,
        ResourcePacksSent = 4,
        StartGameSent = 5,
        Spawned = 6
    };

    ServerPlayer(const NetworkIdentifier &id, uint64_t runtimeId, PacketSender *sender = nullptr);

    using Actor::teleport;

    void teleport(ServerNetworkHandler &owner, const Vector3f &position,
                  MovePlayerTeleportationCause cause = MovePlayerTeleportationCause::Behavior);

    const char *getIdentifier() const override { return "minecraft:player"; }

    const NetworkIdentifier &getNetworkIdentifier() const { return mId; }

    LoginState getLoginState() const { return mLoginState; }

    void setLoginState(LoginState state) { mLoginState = state; }

    bool isSpawned() const { return mLoginState == LoginState::Spawned; }

    std::string getName() const override {
        return mName;
    }

    bool isPlayer() const override {
        return true;
    }

    void setName(const std::string &name) { mName = name; }

    const std::string &getUuid() const { return mUuid; }

    void setUuid(const std::string &uuid) { mUuid = uuid; }

    const std::string &getXuid() const { return mXuid; }

    void setXuid(const std::string &xuid) { mXuid = xuid; }

    const SerializedSkin &getSkin() const { return mSkin; }

    void setSkin(const SerializedSkin &skin) { mSkin = skin; }

    int getBuildPlatform() const { return mBuildPlatform; }

    void setBuildPlatform(int platform) { mBuildPlatform = platform; }

    const std::string &getLocale() const { return mLocale; }

    void setLocale(const std::string &locale) { mLocale = locale; }

    std::string localize(const std::string &key, const std::vector<std::string> &parameters = {}) const;

    int32_t getGameType() const { return mGameType; }

    void setGameType(int32_t gameType) { mGameType = gameType; }

    bool attackActor(ServerNetworkHandler &owner, uint64_t targetRuntimeId);

    bool isOp() const { return mIsOp; }

    void setOp(bool isOp) { mIsOp = isOp; }

    bool isFlying() const { return mFlying; }

    void setFlying(bool flying) {
        if (mFlying == flying)
            return;

        resetFallDistance();
        mFlying = flying;
    }

    CommandPermission getCommandPermission() const {
        return mIsOp ? CommandPermission::GameDirectors : CommandPermission::Any;
    }

    int64_t getFirstPlayed() const { return mFirstPlayed; }

    void setFirstPlayed(int64_t firstPlayed) { mFirstPlayed = firstPlayed; }

    PlayerInventory &getInventory() { return mInventory; }

    const PlayerInventory &getInventory() const { return mInventory; }

    std::unordered_map<std::string, DynamicPropertyValue> &getDynamicProperties() { return mDynamicProperties; }

    int64_t getLastItemUseTick() const { return mLastItemUseTick; }

    void setLastItemUseTick(int64_t tick) { mLastItemUseTick = tick; }

    InventoryManager &getInventoryManager() { return mInventoryManager; }

    PacketSender *getPacketSender() const { return mSender; }

    void setEffectsNetworkReady(bool ready) { mEffectsNetworkReady = ready; }

    void syncEffects();

    Tag saveNbt(const std::string &levelName) const;

    void loadNbt(const Tag &data, const PacketCodecContext &context);

    void sendMessage(const std::string &message);

    void sendTranslation(const std::string &key, const std::vector<std::string> &parameters = {});

    void sendTip(const std::string &message);

    void sendPopup(const std::string &message);

    void sendTitle(const std::string &title, const std::string &subtitle = "", int32_t fadeInTime = 10,
                   int32_t stayTime = 70, int32_t fadeOutTime = 20);

    void sendTitleText(const std::string &text, bool json = false);

    void sendSubtitle(const std::string &text, bool json = false);

    void sendActionBar(const std::string &text, bool json = false);

    void setTitleTimes(int32_t fadeInTime, int32_t stayTime, int32_t fadeOutTime);

    void clearTitle();

    void resetTitle();

    bool hasPendingMove() const { return mHasPendingMove; }

    const Vector3f &getPendingMovePosition() const { return mPendingMovePosition; }

    const Vector3f &getPendingMoveRotation() const { return mPendingMoveRotation; }

    void queueMove(const Vector3f &position, const Vector3f &rotation) {
        mPendingMovePosition = position;
        mPendingMoveRotation = rotation;
        mHasPendingMove = true;
    }

    void clearPendingMove() { mHasPendingMove = false; }

    int64_t getItemUseStartTick() const { return mItemUseStartTick; }

    void setItemUseStartTick(int64_t tick) { mItemUseStartTick = tick; }

    ChunkStreamState &getChunkStreamState() { return mChunkStreamState; }

    bool isAwaitingDimensionAck() const { return mAwaitingDimensionAck; }

    void setAwaitingDimensionAck(bool awaiting) { mAwaitingDimensionAck = awaiting; }

    void consumeOneHeldItem();

    bool isSleeping() const {
        return mSleeping;
    }

    const Vector3i &getSleepingPosition() const {
        return mSleepingPosition;
    }

    void setSleeping(const Vector3i &position) {
        mSleeping = true;
        mSleepingPosition = position;
    }

    void clearSleeping() {
        mSleeping = false;
    }

    bool hasSpawnPoint() const {
        return mHasSpawnPoint;
    }

    const Vector3i &getSpawnPoint() const {
        return mSpawnPoint;
    }

    void setSpawnPoint(const Vector3i &position) {
        mHasSpawnPoint = true;
        mSpawnPoint = position;
    }

    void clearSpawnPoint() {
        mHasSpawnPoint = false;
    }

    int32_t getTicksSinceInAir() const { return mTicksSinceInAir; }

    void tickGroundTracking() {
        if (isOnGround())
            mTicksSinceInAir++;
        else
            mTicksSinceInAir = 0;
    }

    bool isSpinAttacking() const { return mSpinAttackTicks > 0; }

    void startSpinAttack(ServerNetworkHandler &owner, int32_t ticks);

    void tickSpinAttack(ServerNetworkHandler &owner);

    bool isAwaitingConsumableRelease() const { return mAwaitingConsumableRelease; }

    void setAwaitingConsumableRelease() { mAwaitingConsumableRelease = true; }

    void clearAwaitingConsumableRelease() { mAwaitingConsumableRelease = false; }

    int64_t getLastEarlyConsumableReleaseTick() const { return mLastEarlyConsumableReleaseTick; }

    void setLastEarlyConsumableReleaseTick(int64_t tick) { mLastEarlyConsumableReleaseTick = tick; }

    bool hasItemCooldown(const ItemStack &item, int64_t currentTick) const;

    int getItemCooldownRemaining(const ItemStack &item, int64_t currentTick) const;

    void startItemCooldown(const ItemStack &item, int64_t currentTick, int ticks = 0);

    void tickItemCooldowns(int64_t currentTick);

    bool hasLastRightClick() const { return mHasLastRightClick; }

    const Vector3i &getLastRightClickBlockPosition() const { return mLastRightClickBlockPosition; }

    int32_t getLastRightClickFace() const { return mLastRightClickFace; }

    const Vector3f &getLastRightClickPlayerPosition() const { return mLastRightClickPlayerPosition; }

    const Vector3f &getLastRightClickPosition() const { return mLastRightClickPosition; }

    uint64_t getLastRightClickTimeMicros() const { return mLastRightClickTimeMicros; }

    void recordRightClick(const Vector3i &blockPosition, int32_t face, const Vector3f &playerPosition,
                          const Vector3f &clickPosition, uint64_t timeMicros) {
        mHasLastRightClick = true;
        mLastRightClickBlockPosition = blockPosition;
        mLastRightClickFace = face;
        mLastRightClickPlayerPosition = playerPosition;
        mLastRightClickPosition = clickPosition;
        mLastRightClickTimeMicros = timeMicros;
    }

    static constexpr int MAX_AIR_SUPPLY = 400;

    static constexpr std::chrono::milliseconds POST_TELEPORT_GRACE{1000};

    void markTeleported() { mLastTeleport = std::chrono::steady_clock::now(); }

    /** Whether the player changed skin less than the given number of seconds ago. */
    bool changedSkinWithin(int seconds) const {
        return mHasChangedSkin && std::chrono::steady_clock::now() - mLastSkinChange < std::chrono::seconds(seconds);
    }

    void markSkinChanged() {
        mHasChangedSkin = true;
        mLastSkinChange = std::chrono::steady_clock::now();
    }

    /** Whether the player was teleported less than a second ago, when their movement costs no food. */
    bool wasRecentlyTeleported() const {
        return std::chrono::steady_clock::now() - mLastTeleport < POST_TELEPORT_GRACE;
    }

    int getAirSupply() const { return mAirSupply; }

    void setAirSupply(int airSupply) { mAirSupply = airSupply; }

    void resetAirSupply() { mAirSupply = MAX_AIR_SUPPLY; }

    int getTurtleHelmetTicks() const { return mTurtleHelmetTicks; }

    void setTurtleHelmetTicks(int ticks) { mTurtleHelmetTicks = ticks; }

    bool isBreakingBlock() const { return mIsBreakingBlock; }

    const Vector3i &getBreakingBlockPosition() const { return mBreakingBlockPosition; }

    int32_t getBreakingFace() const { return mBreakingFace; }

    void setBreakingFace(int32_t face) { mBreakingFace = face; }

    bool hasLastBlockAttacked() const { return mHasLastBlockAttacked; }

    const Vector3i &getLastBlockAttacked() const { return mLastBlockAttacked; }

    void setLastBlockAttacked(const Vector3i &position) {
        mHasLastBlockAttacked = true;
        mLastBlockAttacked = position;
    }

    void clearLastBlockAttacked() { mHasLastBlockAttacked = false; }

    double getBreakProgress() const { return mBreakProgress; }

    double getBreakSpeed() const { return mBreakSpeed; }

    int64_t getBreakStartTick() const { return mBreakStartTick; }

    void addBreakProgress(double amount) { mBreakProgress += amount; }

    void setBreakSpeed(double speed) { mBreakSpeed = speed; }

    void startBreakingBlock(const Vector3i &position, int32_t face, double speed, int64_t startTick = 0) {
        mIsBreakingBlock = true;
        mBreakingBlockPosition = position;
        mBreakingFace = face;
        mBreakSpeed = speed;
        mBreakProgress = 0.0;
        mBreakingFxTicker = 0;
        mBreakStartTick = startTick;
    }

    void stopBreakingBlock() {
        mIsBreakingBlock = false;
        mBreakSpeed = 0.0;
        mBreakProgress = 0.0;
        mBreakStartTick = 0;
    }

    int getBreakingFxTicker() { return mBreakingFxTicker++; }

    bool hasSeenNonZeroClientTick() const { return mSawNonZeroClientTick; }

    void markNonZeroClientTickSeen() { mSawNonZeroClientTick = true; }

    std::unordered_set<int64_t> &getSentChunks() { return mSentChunks; }

    std::unordered_set<uint64_t> &getVisibleActors() { return mVisibleActors; }

    /** Runtime ids of the other players this player's client currently has spawned. */
    std::unordered_set<uint64_t> &getVisiblePlayers() { return mVisiblePlayers; }

    bool hasMovedSinceBroadcast() const {
        return mPosition != mBroadcastPosition || mRotation != mBroadcastRotation;
    }

    void markMoveBroadcast() {
        mBroadcastPosition = mPosition;
        mBroadcastRotation = mRotation;
    }

    size_t getSentChunkCount() const { return mSentChunkCount; }

    void addSentChunkCount(size_t count) { mSentChunkCount += count; }

    bool hasSpawnChunksReady() const { return mSpawnChunksReady; }

    void setSpawnChunksReady(bool ready) { mSpawnChunksReady = ready; }

    bool hasChunkPosition() const { return mHasChunkPosition; }

    int32_t getLastChunkX() const { return mLastChunkX; }

    int32_t getLastChunkZ() const { return mLastChunkZ; }

    void setLastChunkPosition(int32_t chunkX, int32_t chunkZ) {
        mLastChunkX = chunkX;
        mLastChunkZ = chunkZ;
        mHasChunkPosition = true;
    }

    void resetChunkStreaming() {
        mSentChunks.clear();
        mSentChunkCount = 0;
        mSpawnChunksReady = false;
        mHasChunkPosition = false;
    }

private:
    float _applyAttackerModifiers(float baseDamage, float damage) const;

    bool _isCriticalHit() const;

    NetworkIdentifier mId;
    LoginState mLoginState;
    std::string mName;
    std::string mUuid;
    std::unordered_map<std::string, DynamicPropertyValue> mDynamicProperties;
    int64_t mLastItemUseTick = -1000;
    std::string mXuid;
    int mBuildPlatform = -1;
    std::string mLocale;
    int64_t mItemUseStartTick = 0;
    int32_t mSpinAttackTicks = 0;
    ChunkStreamState mChunkStreamState;
    bool mAwaitingDimensionAck = false;
    bool mSleeping = false;
    Vector3i mSleepingPosition;
    bool mHasSpawnPoint = false;
    Vector3i mSpawnPoint;
    int32_t mTicksSinceInAir = 0;
    bool mAwaitingConsumableRelease = false;
    int64_t mLastEarlyConsumableReleaseTick = 0;
    bool mHasLastRightClick = false;
    Vector3i mLastRightClickBlockPosition;
    int32_t mLastRightClickFace = 0;
    Vector3f mLastRightClickPlayerPosition;
    Vector3f mLastRightClickPosition;
    uint64_t mLastRightClickTimeMicros = 0;
    int mAirSupply = MAX_AIR_SUPPLY;
    int mTurtleHelmetTicks = 0;
    std::chrono::steady_clock::time_point mLastTeleport{};
    std::chrono::steady_clock::time_point mLastSkinChange{};
    bool mHasChangedSkin = false;
    bool mHasPendingMove = false;
    Vector3f mPendingMovePosition;
    Vector3f mPendingMoveRotation;
    bool mIsBreakingBlock = false;
    Vector3i mBreakingBlockPosition;
    int32_t mBreakingFace = 0;
    double mBreakProgress = 0.0;
    double mBreakSpeed = 0.0;
    int64_t mBreakStartTick = 0;
    int mBreakingFxTicker = 0;
    bool mHasLastBlockAttacked = false;
    Vector3i mLastBlockAttacked;
    bool mSawNonZeroClientTick = false;
    std::unordered_set<int64_t> mSentChunks;
    std::unordered_set<uint64_t> mVisibleActors;
    std::unordered_set<uint64_t> mVisiblePlayers;
    Vector3f mBroadcastPosition;
    Vector3f mBroadcastRotation;
    size_t mSentChunkCount = 0;
    bool mSpawnChunksReady = false;
    int32_t mLastChunkX = 0;
    int32_t mLastChunkZ = 0;
    bool mHasChunkPosition = false;
    SerializedSkin mSkin;
    int32_t mGameType = 0;
    int64_t mFirstPlayed = 0;
    bool mIsOp = false;
    bool mFlying = false;
    PlayerInventory mInventory;
    InventoryManager mInventoryManager;
    PacketSender *mSender;
    bool mEffectsNetworkReady = false;
    std::unordered_map<std::string, int64_t> mItemCooldowns;
};
