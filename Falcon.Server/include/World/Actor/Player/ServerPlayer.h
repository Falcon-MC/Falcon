#pragma once

#include "Core/Math/AxisAlignedBB.h"

#include <cmath>

#include "World/Actor/Actor.h"
#include "World/Actor/Movement/ServerPlayerMovementComponent.h"
#include "World/Actor/Movement/PlayerMovementValidator.h"
#include "World/Actor/Movement/Components/SwimAmountComponent.h"
#include "World/Actor/DynamicPropertyValue.h"
#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Dimension/Dimension.h"
#include "World/Level/Map/MapRequestQueue.h"
#include "World/Inventory/PlayerInventory.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/NetworkIdentifier.h"
#include "Network/PacketSender.h"
#include "Protocol/Codec/ProtocolCodec.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Types/AdventureSettingData.h"
#include "Protocol/Types/SerializedSkin.h"
#include "World/Actor/MobEffect.h"
#include "World/Actor/Player/PlayerAcknowledgements.h"
#include "Server/Debug/DebugHud.h"
#include "World/Actor/Player/PlayerBossBars.h"
#include "World/Actor/Player/PlayerRaidState.h"

#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include <unordered_set>
#include <unordered_map>

class ServerNetworkHandler;

class ServerPlayer : public Actor {
public:
    bool canFreeze() const override {
        return true;
    }

    bool canStandOnPowderSnow() const override;

    // world the player was saved in; one player data file is shared by every world
    static constexpr const char *TAG_LEVEL = "Level";

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

    // a velocity the client applies itself (lunge); the packet carries the last received input tick: the client
    // applies it on the next input tick and replays from there, the simulation takes it on that tick, queue or not
    void applyServerMotion(ServerNetworkHandler &owner, const Vector3f &motion);

    const char *getIdentifier() const override {
        return "minecraft:player";
    }

    const NetworkIdentifier &getNetworkIdentifier() const {
        return mId;
    }

    LoginState getLoginState() const {
        return mLoginState;
    }

    void setLoginState(LoginState state) {
        mLoginState = state;
    }

    int64_t getLoggedInTick() const {
        return mLoggedInTick;
    }

    void setLoggedInTick(int64_t tick) {
        mLoggedInTick = tick;
    }

    // chosen from the announced version, fixed for the session
    const std::shared_ptr<const ProtocolCodec> &getCodec() const {
        return mCodec;
    }

    void setCodec(std::shared_ptr<const ProtocolCodec> codec) {
        mCodec = std::move(codec);
    }

    bool isSpawned() const {
        return mLoginState == LoginState::Spawned;
    }

    std::string getName() const override {
        return mName;
    }

    bool isPlayer() const override {
        return true;
    }

    void setName(const std::string &name) {
        mName = name;
    }

    const std::string &getUuid() const {
        return mUuid;
    }

    void setUuid(const std::string &uuid) {
        mUuid = uuid;
    }

    const std::string &getXuid() const {
        return mXuid;
    }

    void setXuid(const std::string &xuid) {
        mXuid = xuid;
    }

    const std::string &getPlayFabId() const {
        return mPlayFabId;
    }

    void setPlayFabId(const std::string &playFabId) {
        mPlayFabId = playFabId;
    }

    const SerializedSkin &getSkin() const {
        return mSkin;
    }

    void setSkin(const SerializedSkin &skin) {
        mSkin = skin;
    }

    int getBuildPlatform() const {
        return mBuildPlatform;
    }

    void setBuildPlatform(int platform) {
        mBuildPlatform = platform;
    }

    // CompatibleWithClientSideChunkGen from the login client data (sub_1422AA4E0, ServerPlayer+3576).
    bool isClientSideGenerationCompatible() const {
        return mClientSideGenerationCompatible;
    }

    void setClientSideGenerationCompatible(bool compatible) {
        mClientSideGenerationCompatible = compatible;
    }

    // the text filtering half of SyncedClientOptionsComponent, from the login client data (sub_143A10DC0, 26.60)
    void readSyncedClientOptions(const std::string &clientData);

    // SyncedClientOptionsComponent+24: FilterProfanity at login, then UpdateClientOptionsPacket
    bool isProfanityFilterEnabled() const {
        return mProfanityFilterEnabled;
    }

    void setProfanityFilterEnabled(bool enabled) {
        mProfanityFilterEnabled = enabled;
    }

    // SyncedClientOptionsComponent+16: TextFilteringDebugTimeoutMilliSeconds, 0 when the client sent none
    int getTextFilteringDebugTimeout() const {
        return mTextFilteringDebugTimeout;
    }

    // The share of its view the server builds for this player (ClientSideGeneration::computeServerBuildRatio).
    float getServerBuildRatio() const {
        return mServerBuildRatio;
    }

    void setServerBuildRatio(float ratio) {
        mServerBuildRatio = ratio;
    }

    // Player+0xBC0, which EmptyMapItem::use numbers its maps with (sub_147B9F900)
    int32_t takeMapNameIndex() {
        return mMapNameIndex++;
    }

    MapRequestQueue &getMapRequests() {
        return mMapRequests;
    }

    const std::string &getLocale() const {
        return mLocale;
    }

    void setLocale(const std::string &locale) {
        mLocale = locale;
    }

    std::string localize(const std::string &key, const std::vector<std::string> &parameters = {}) const;

    int32_t getGameType() const {
        return mGameType;
    }

    void setGameType(int32_t gameType) {
        mGameType = gameType;
    }

    int64_t getLastActiveTick() const {
        return mLastActiveTick;
    }

    void markActive(int64_t tick) {
        mLastActiveTick = tick;
    }

    // true when the look direction changed since the previous input
    bool updateInputRotation(const Vector3f &rotation) {
        const bool changed = rotation.x != mLastInputRotation.x || rotation.y != mLastInputRotation.y;
        mLastInputRotation = rotation;
        return changed;
    }

    bool attackActor(ServerNetworkHandler &owner, uint64_t targetRuntimeId);

    Vector3f getLookDirection() const;

    bool isOp() const {
        return mIsOp;
    }

    void setOp(bool isOp) {
        mIsOp = isOp;
    }

    bool isFlying() const {
        return mFlying;
    }

    // what the last PlayerAuthInput reported: held buttons, the movement vector and the input device
    struct InputState {
        int32_t mInputMode = 0;
        bool mJumpHeld = false;
        bool mSneakHeld = false;
        bool mJumping = false;
        float mMoveX = 0.0f;
        float mMoveY = 0.0f;
    };

    InputState &getInputState() {
        return mInputState;
    }

    // the script chat decorations: name prefix and suffix around the author, a prefix before the message
    struct ChatStyle {
        std::string mNamePrefix;
        std::string mNameSuffix;
        std::string mMessagePrefix;
    };

    ChatStyle &getChatStyle() {
        return mChatStyle;
    }

    const InputState &getInputState() const {
        return mInputState;
    }

    bool hasSimulatedVelocity() const {
        return mHasSimulatedVelocity;
    }

    const Vector3f &getSimulatedVelocity() const {
        return mSimulatedVelocity;
    }

    void setSimulatedVelocity(const Vector3f &velocity) {
        mSimulatedVelocity = velocity;
        mHasSimulatedVelocity = true;
    }

    void clearSimulatedVelocity() {
        mHasSimulatedVelocity = false;
    }

    bool hasSimulatedPosition() const {
        return mHasSimulatedPosition;
    }

    const Vector3f &getSimulatedPosition() const {
        return mSimulatedPosition;
    }

    void setSimulatedPosition(const Vector3f &position) {
        mSimulatedPosition = position;
        mHasSimulatedPosition = true;
        mHasSimulatedGround = false;
    }

    void clearSimulatedPosition() {
        mHasSimulatedPosition = false;
        mHasSimulatedGround = false;
    }

    bool hasSimulatedGround() const {
        return mHasSimulatedGround;
    }

    bool isSimulatedOnGround() const {
        return mSimulatedOnGround;
    }

    void setSimulatedOnGround(bool onGround) {
        mSimulatedOnGround = onGround;
        mHasSimulatedGround = true;
    }

    // the box the replay ended on: rebuilt from the feet, a box clamped against a wall at x = 1 comes back at
    // 0.99999995 and stands on the corner of the block beside it
    void setSimulatedBox(const AxisAlignedBB &box, const Vector3f &feet) {
        mSimulatedBox = box;
        mSimulatedBoxFeet = feet;
        mHasSimulatedBox = true;
    }

    bool findSimulatedBox(const Vector3f &feet, float height, AxisAlignedBB &box) const {
        if (!mHasSimulatedBox || feet.x != mSimulatedBoxFeet.x || feet.y != mSimulatedBoxFeet.y
            || feet.z != mSimulatedBoxFeet.z || std::fabs(mSimulatedBox.mMaxY - mSimulatedBox.mMinY - height) > 1.0e-4f)
            return false;
        box = mSimulatedBox;
        return true;
    }

    void clearMovementSimulation() {
        mHasSimulatedBox = false;
        mMovementValidator.reset();
        mHasSimulatedVelocity = false;
        mHasSimulatedPosition = false;
        mHasSimulatedGround = false;
        mHasPendingKnockback = false;
        mSimulatedSneaking = false;
        mSimulatedCrawling = false;
        mSimulatedSwimming = false;
        mSlowdownTicks = 0;
        mHasSupportingBlock = false;
    }

    PlayerMovementValidator &getMovementValidator() {
        return mMovementValidator;
    }

    SwimAmountComponent &getSwimAmount() {
        return mSwimAmount;
    }

    bool hasSupportingBlock() const {
        return mHasSupportingBlock;
    }

    const Vector3i &getSupportingBlock() const {
        return mSupportingBlock;
    }

    void setSupportingBlock(const Vector3i &position) {
        mSupportingBlock = position;
        mHasSupportingBlock = true;
    }

    void clearSupportingBlock() {
        mHasSupportingBlock = false;
    }

    int32_t getSlowdownTicks() const {
        return mSlowdownTicks;
    }

    void setSlowdownTicks(int32_t ticks) {
        mSlowdownTicks = ticks;
    }

    bool isSimulatedSneaking() const {
        return mSimulatedSneaking;
    }

    bool isSimulatedCrawling() const {
        return mSimulatedCrawling;
    }

    bool isSimulatedSwimming() const {
        return mSimulatedSwimming;
    }

    void setSimulatedSwimming(bool swimming) {
        mSimulatedSwimming = swimming;
    }

    void setSimulatedPose(bool sneaking, bool crawling) {
        mSimulatedSneaking = sneaking;
        mSimulatedCrawling = crawling;
    }

    int32_t getJumpDelay() const {
        return mJumpDelay;
    }

    void setJumpDelay(int32_t ticks) {
        mJumpDelay = ticks;
    }

    void queueKnockback(const Vector3f &motion) {
        mPendingKnockback = motion;
        mHasPendingKnockback = true;
    }

    bool takeKnockback(Vector3f &out) {
        if (!mHasPendingKnockback)
            return false;
        out = mPendingKnockback;
        mHasPendingKnockback = false;
        return true;
    }

    void clearPendingKnockback() {
        mHasPendingKnockback = false;
    }

    // motion due on this input tick or an earlier one (applyServerMotion)
    bool takeServerMotion(int64_t inputTick, Vector3f &out) {
        if (!mHasPendingServerMotion || inputTick < mPendingServerMotionTick)
            return false;
        out = mPendingServerMotion;
        mHasPendingServerMotion = false;
        return true;
    }

    void scheduleKnockback(const Vector3f &motion) {
        mAcknowledgements.add([this, motion]() { queueKnockback(motion); });
    }

    PlayerAcknowledgements &getAcknowledgements() {
        return mAcknowledgements;
    }

    void awaitMovementChange() {
        mPendingMovementChanges++;
        mAcknowledgements.add([this]() {
            if (mPendingMovementChanges > 0)
                mPendingMovementChanges--;
        });
    }

    bool hasPendingMovementChange() const {
        return mPendingMovementChanges > 0;
    }

    // a client acks a movement change within a round trip; one holding the ack back would move unchecked until the
    // timeout: the simulation resumes after a bounded number of inputs
    bool skipsForPendingMovementChange() {
        if (!hasPendingMovementChange()) {
            mInputsWhilePending = 0;
            return false;
        }
        return ++mInputsWhilePending <= MAX_INPUTS_WHILE_PENDING;
    }

    void setFlying(bool flying) {
        if (mFlying == flying)
            return;

        resetFallDistance();
        mFlying = flying;
    }

    CommandPermission getCommandPermission() const {
        return mIsOp ? CommandPermission::GameDirectors : CommandPermission::Any;
    }

    int64_t getFirstPlayed() const {
        return mFirstPlayed;
    }

    void setFirstPlayed(int64_t firstPlayed) {
        mFirstPlayed = firstPlayed;
    }

    PlayerInventory &getInventory() {
        return mInventory;
    }

    const PlayerInventory &getInventory() const {
        return mInventory;
    }

    void recordHurtBy(uint64_t runtimeId, int64_t tick) {
        mLastHurtByRuntimeId = runtimeId;
        mLastHurtByTick = tick;
    }

    void recordAttacked(uint64_t runtimeId, int64_t tick) {
        mLastAttackedRuntimeId = runtimeId;
        mLastAttackedTick = tick;
    }

    uint64_t getLastHurtByRuntimeId() const {
        return mLastHurtByRuntimeId;
    }

    int64_t getLastHurtByTick() const {
        return mLastHurtByTick;
    }

    uint64_t getLastAttackedRuntimeId() const {
        return mLastAttackedRuntimeId;
    }

    int64_t getLastAttackedTick() const {
        return mLastAttackedTick;
    }

    std::unordered_map<std::string, DynamicPropertyValue> &getDynamicProperties() {
        return mDynamicProperties;
    }

    int64_t getLastItemUseTick() const {
        return mLastItemUseTick;
    }

    void setLastItemUseTick(int64_t tick) {
        mLastItemUseTick = tick;
    }

    InventoryManager &getInventoryManager() {
        return mInventoryManager;
    }

    const InventoryManager &getInventoryManager() const {
        return mInventoryManager;
    }

    PacketSender *getPacketSender() const {
        return mSender;
    }

    void setEffectsNetworkReady(bool ready) {
        mEffectsNetworkReady = ready;
    }

    PlayerBossBars &getBossBars() {
        return mBossBars;
    }

    DebugHud &getDebugHud() {
        return mDebugHud;
    }

    PlayerRaidState &getRaidState() {
        return mRaidState;
    }

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

    static const int HUD_ELEMENT_COUNT = 13;

    // Player::setHudVisibility (sub_141030B10): no elements means all of them
    void setHudVisibility(bool hidden, const std::vector<int32_t> *elements = nullptr);

    // ClientInputLocks: one bit per InputPermissionCategory value, set when locked; true when it changed
    bool setInputLocked(int32_t category, bool locked);

    bool isInputLocked(int32_t category) const {
        return category >= 0 && category < 32 && (mInputLocks & (1u << category)) != 0;
    }

    bool isHudElementHidden(int32_t element) const {
        return element >= 0 && element < HUD_ELEMENT_COUNT && mHiddenHudElements[element];
    }

    void resetTitle();

    bool hasPendingMove() const {
        return mHasPendingMove;
    }

    const Vector3f &getPendingMovePosition() const {
        return mPendingMovePosition;
    }

    const Vector3f &getPendingMoveRotation() const {
        return mPendingMoveRotation;
    }

    void queueMove(const Vector3f &position, const Vector3f &rotation) {
        mPendingMovePosition = position;
        mPendingMoveRotation = rotation;
        mHasPendingMove = true;
    }

    void clearPendingMove() {
        mHasPendingMove = false;
    }

    int64_t getItemUseStartTick() const {
        return mItemUseStartTick;
    }

    void setItemUseStartTick(int64_t tick) {
        mItemUseStartTick = tick;
    }

    ChunkStreamState &getChunkStreamState() {
        return mChunkStreamState;
    }

    bool isAwaitingDimensionAck() const {
        return mAwaitingDimensionAck;
    }

    void setAwaitingDimensionAck(bool awaiting) {
        if (awaiting) {
            mMovementInputs.clearPending();
            clearMovementSimulation();
        }
        mAwaitingDimensionAck = awaiting;
    }

    // a world change inside one dimension sent an intermediate dimension; the real one follows the ack
    bool hasPendingDimensionHop() const {
        return mPendingDimensionHop;
    }

    void setPendingDimensionHop(bool pending) {
        mPendingDimensionHop = pending;
    }

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

    void resetSleepCounter() {
        mSleepCounter = 0;
    }

    void tickSleepCounter() {
        if (mSleepCounter < DEEP_SLEEP_TICKS)
            mSleepCounter++;
    }

    bool isSleepingLongEnough() const {
        return mSleepCounter >= DEEP_SLEEP_TICKS;
    }

    bool hasSpawnPoint() const {
        return mHasSpawnPoint;
    }

    const Vector3i &getSpawnPoint() const {
        return mSpawnPoint;
    }

    // a bed only counts in the world it was placed in
    const std::string &getSpawnWorld() const {
        return mSpawnWorld;
    }

    // where the player stood when the spawn point was set (SpawnX/Y/Z): an anchor respawns around it
    const Vector3i &getSpawnStandPosition() const {
        return mSpawnStandPosition;
    }

    DimensionType getSpawnDimension() const {
        return mSpawnDimension;
    }

    // false for a spawn set by command: no bed or anchor has to hold it
    bool hasSpawnBlock() const {
        return mHasSpawnBlock;
    }

    void setSpawnPoint(const Vector3i &position, const Vector3i &standPosition, DimensionType dimension,
                       const std::string &world) {
        mHasSpawnPoint = true;
        mHasSpawnBlock = true;
        mSpawnPoint = position;
        mSpawnStandPosition = standPosition;
        mSpawnDimension = dimension;
        mSpawnWorld = world;
    }

    // sub_1402DC450: /spawnpoint keeps the position and dimension, clears the block
    void setSpawnPosition(const Vector3i &standPosition, DimensionType dimension, const std::string &world) {
        setSpawnPoint(standPosition, standPosition, dimension, world);
        mHasSpawnBlock = false;
    }

    // sub_1402DC130: floor x and z, ceil of the feet
    Vector3i getSpawnStandPositionHere() const;

    void clearSpawnPoint() {
        mHasSpawnPoint = false;
    }

    int32_t getTicksSinceInAir() const {
        return mTicksSinceInAir;
    }

    void tickGroundTracking() {
        if (isOnGround())
            mTicksSinceInAir++;
        else
            mTicksSinceInAir = 0;
    }

    bool isSpinAttacking() const {
        return mSpinAttackTicks > 0;
    }

    void startSpinAttack(ServerNetworkHandler &owner, int32_t ticks);

    void tickSpinAttack(ServerNetworkHandler &owner);

    bool isAwaitingConsumableRelease() const {
        return mAwaitingConsumableRelease;
    }

    void setAwaitingConsumableRelease() {
        mAwaitingConsumableRelease = true;
    }

    void clearAwaitingConsumableRelease() {
        mAwaitingConsumableRelease = false;
    }

    int64_t getLastEarlyConsumableReleaseTick() const {
        return mLastEarlyConsumableReleaseTick;
    }

    void setLastEarlyConsumableReleaseTick(int64_t tick) {
        mLastEarlyConsumableReleaseTick = tick;
    }

    bool hasItemCooldown(const ItemStack &item, int64_t currentTick) const;

    int getItemCooldownRemaining(const ItemStack &item, int64_t currentTick) const;

    void startItemCooldown(const ItemStack &item, int64_t currentTick, int ticks = 0);

    void tickItemCooldowns(int64_t currentTick);

    bool hasLastRightClick() const {
        return mHasLastRightClick;
    }

    const Vector3i &getLastRightClickBlockPosition() const {
        return mLastRightClickBlockPosition;
    }

    int32_t getLastRightClickFace() const {
        return mLastRightClickFace;
    }

    const Vector3f &getLastRightClickPlayerPosition() const {
        return mLastRightClickPlayerPosition;
    }

    const Vector3f &getLastRightClickPosition() const {
        return mLastRightClickPosition;
    }

    uint64_t getLastRightClickTimeMicros() const {
        return mLastRightClickTimeMicros;
    }

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

    void markTeleported() {
        mMovementInputs.clearPending();
        clearMovementSimulation();
        mLastTeleport = std::chrono::steady_clock::now();
        awaitMovementChange();
        const uint64_t teleport = ++mTeleportCount;
        mAwaitingTeleport = true;
        mAcknowledgements.add([this, teleport]() {
            if (mTeleportCount == teleport)
                mAwaitingTeleport = false;
        });
    }

    // inputs sent before the client took the teleport still carry the old position: they must not move the player
    // back; bounded by the grace in case the ack never comes
    bool isAwaitingTeleport() const {
        return mAwaitingTeleport && std::chrono::steady_clock::now() - mLastTeleport < POST_TELEPORT_GRACE;
    }

    bool changedSkinWithin(int seconds) const {
        return mHasChangedSkin && std::chrono::steady_clock::now() - mLastSkinChange < std::chrono::seconds(seconds);
    }

    void markSkinChanged() {
        mHasChangedSkin = true;
        mLastSkinChange = std::chrono::steady_clock::now();
    }

    // movement costs no food for a second after a teleport
    bool wasRecentlyTeleported() const {
        return std::chrono::steady_clock::now() - mLastTeleport < POST_TELEPORT_GRACE;
    }

    int getAirSupply() const {
        return mAirSupply;
    }

    void setAirSupply(int airSupply) {
        mAirSupply = airSupply;
    }

    void resetAirSupply() {
        mAirSupply = MAX_AIR_SUPPLY;
    }

    int getTurtleHelmetTicks() const {
        return mTurtleHelmetTicks;
    }

    void setTurtleHelmetTicks(int ticks) {
        mTurtleHelmetTicks = ticks;
    }

    bool isBreakingBlock() const {
        return mIsBreakingBlock;
    }

    const Vector3i &getBreakingBlockPosition() const {
        return mBreakingBlockPosition;
    }

    int32_t getBreakingFace() const {
        return mBreakingFace;
    }

    void setBreakingFace(int32_t face) {
        mBreakingFace = face;
    }

    bool hasLastBlockAttacked() const {
        return mHasLastBlockAttacked;
    }

    const Vector3i &getLastBlockAttacked() const {
        return mLastBlockAttacked;
    }

    void setLastBlockAttacked(const Vector3i &position) {
        mHasLastBlockAttacked = true;
        mLastBlockAttacked = position;
    }

    void clearLastBlockAttacked() {
        mHasLastBlockAttacked = false;
    }

    double getBreakProgress() const {
        return mBreakProgress;
    }

    double getBreakSpeed() const {
        return mBreakSpeed;
    }

    int64_t getBreakStartTick() const {
        return mBreakStartTick;
    }

    void addBreakProgress(double amount) {
        mBreakProgress += amount;
    }

    void setBreakSpeed(double speed) {
        mBreakSpeed = speed;
    }

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

    int getBreakingFxTicker() {
        return mBreakingFxTicker++;
    }

    ServerPlayerMovementComponent &getMovementInputs() {
        return mMovementInputs;
    }

    bool hasSeenNonZeroClientTick() const {
        return mSawNonZeroClientTick;
    }

    void markNonZeroClientTickSeen() {
        mSawNonZeroClientTick = true;
    }

    std::unordered_set<int64_t> &getSentChunks() {
        return mSentChunks;
    }

    std::unordered_set<uint64_t> &getVisibleActors() {
        return mVisibleActors;
    }

    // the other players this client has spawned
    std::unordered_set<uint64_t> &getVisiblePlayers() {
        return mVisiblePlayers;
    }

    bool hasMovedSinceBroadcast() const {
        return mPosition != mBroadcastPosition || mRotation != mBroadcastRotation;
    }

    void markMoveBroadcast() {
        mBroadcastPosition = mPosition;
        mBroadcastRotation = mRotation;
    }

    size_t getSentChunkCount() const {
        return mSentChunkCount;
    }

    void addSentChunkCount(size_t count) {
        mSentChunkCount += count;
    }

    bool hasSpawnChunksReady() const {
        return mSpawnChunksReady;
    }

    void setSpawnChunksReady(bool ready) {
        mSpawnChunksReady = ready;
    }

    bool hasChunkPosition() const {
        return mHasChunkPosition;
    }

    int32_t getLastChunkX() const {
        return mLastChunkX;
    }

    int32_t getLastChunkZ() const {
        return mLastChunkZ;
    }

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

    bool isBlockingWithShield() const;

    void tickShield(ServerNetworkHandler &owner);

    void interruptShieldForAttack(ServerNetworkHandler &owner);

    bool blockWithShield(ServerNetworkHandler &owner, const Vector3f &source, float damage, Actor *attacker,
                         bool disablesShield);

private:
    float _applyAttackerModifiers(float baseDamage, float damage) const;

    bool _isCriticalHit() const;

    bool _isFacing(ServerNetworkHandler &owner, const Vector3f &targetFeet, float width, float height) const;

    bool _hasShieldReady(int64_t currentTick) const;

    bool _shouldRaiseShield(int64_t currentTick) const;

    void _setShieldFlags(ServerNetworkHandler &owner, bool blocking, bool transition);

    void _damageShield(ServerNetworkHandler &owner, float damage);

    NetworkIdentifier mId;
    LoginState mLoginState;
    std::shared_ptr<const ProtocolCodec> mCodec;
    std::string mName;
    std::string mUuid;
    std::unordered_map<std::string, DynamicPropertyValue> mDynamicProperties;
    int64_t mLastItemUseTick = -1000;
    std::string mXuid;
    std::string mPlayFabId;
    int mBuildPlatform = -1;
    bool mClientSideGenerationCompatible = false;
    bool mProfanityFilterEnabled = true;
    int mTextFilteringDebugTimeout = 0;
    float mServerBuildRatio = 1.0f;
    int32_t mMapNameIndex = 0;
    MapRequestQueue mMapRequests;
    std::string mLocale;
    int64_t mItemUseStartTick = 0;
    int32_t mSpinAttackTicks = 0;
    ChunkStreamState mChunkStreamState;
    bool mAwaitingDimensionAck = false;
    bool mPendingDimensionHop = false;
    static constexpr int32_t DEEP_SLEEP_TICKS = 100;

    bool mSleeping = false;
    int32_t mSleepCounter = 0;
    Vector3i mSleepingPosition;
    bool mHasSpawnPoint = false;
    bool mHasSpawnBlock = false;
    Vector3i mSpawnPoint;
    Vector3i mSpawnStandPosition;
    DimensionType mSpawnDimension = DimensionType::Overworld;
    std::string mSpawnWorld;
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
    ServerPlayerMovementComponent mMovementInputs;
    PlayerMovementValidator mMovementValidator;
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
    int64_t mLastActiveTick = 0;
    int64_t mLoggedInTick = 0;
    Vector3f mLastInputRotation;
    int64_t mFirstPlayed = 0;
    bool mIsOp = false;
    bool mFlying = false;
    InputState mInputState;
    ChatStyle mChatStyle;
    bool mHasSimulatedVelocity = false;
    Vector3f mSimulatedVelocity;
    bool mHasSimulatedPosition = false;
    Vector3f mSimulatedPosition;
    AxisAlignedBB mSimulatedBox;
    Vector3f mSimulatedBoxFeet;
    bool mHasSimulatedBox = false;
    bool mHasSimulatedGround = false;
    bool mSimulatedOnGround = false;
    int32_t mJumpDelay = 0;
    bool mSimulatedSneaking = false;
    bool mSimulatedCrawling = false;
    bool mSimulatedSwimming = false;
    SwimAmountComponent mSwimAmount;
    int32_t mSlowdownTicks = 0;
    bool mHasSupportingBlock = false;
    Vector3i mSupportingBlock;
    PlayerAcknowledgements mAcknowledgements;
    int32_t mPendingMovementChanges = 0;
    static constexpr int32_t MAX_INPUTS_WHILE_PENDING = 20;
    bool mAwaitingTeleport = false;
    uint64_t mTeleportCount = 0;
    int32_t mInputsWhilePending = 0;
    bool mHasPendingKnockback = false;
    Vector3f mPendingKnockback;
    bool mHasPendingServerMotion = false;
    Vector3f mPendingServerMotion;
    int64_t mPendingServerMotionTick = 0;
    PlayerInventory mInventory;
    uint64_t mLastHurtByRuntimeId = 0;
    int64_t mLastHurtByTick = 0;
    uint64_t mLastAttackedRuntimeId = 0;
    int64_t mLastAttackedTick = 0;
    InventoryManager mInventoryManager;
    PacketSender *mSender;
    bool mEffectsNetworkReady = false;
    PlayerRaidState mRaidState;
    PlayerBossBars mBossBars;
    bool mHiddenHudElements[HUD_ELEMENT_COUNT] = {};
    uint32_t mInputLocks = 0;
    DebugHud mDebugHud;
    std::unordered_map<std::string, int64_t> mItemCooldowns;
    int32_t mShieldTransitionTicks = 0;
    int32_t mShieldInterruptTicks = 0;
    bool mShieldReblockAfterAttack = false;
    int64_t mShieldDisabledUntilTick = 0;
};
