#pragma once

#include "Level/BiomeRegistry.h"
#include "Level/Level.h"
#include "Level/PlayerDataProvider.h"
#include "Level/TickingAreaManager.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/PacketRateLimiter.h"
#include "Network/PingedCompatibleServer.h"
#include "Network/RakNetInstance.h"
#include "Protocol/NetworkPacketHandler.h"
#include "Protocol/PacketCodecContext.h"
#include "Protocol/Types/BlockDefinitionRegistry.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "Command/CommandMap.h"
#include "Network/PacketSender.h"
#include "Protocol/Types/CommandOriginData.h"
#include "Server/AllowList.h"
#include "Server/BanList.h"
#include "Server/OpList.h"
#include "Core/Event/EventBus.h"
#include "Scripting/BehaviorPackManager.h"
#include "Scripting/ScriptEngine.h"
#include "Server/Profiler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ResourcePackManager.h"
#include "Actor/ActorDamageSource.h"
#include "Actor/Misc/FallingBlock.h"
#include "Actor/Misc/PrimedTntActor.h"
#include "Actor/Misc/ItemActor.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Actor/Spawn/NaturalSpawner.h"
#include "Protocol/Types/ContainerSlotType.h"
#include "Protocol/Types/EntityDataMap.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/CraftingDataPacket.h"
#include "Protocol/Packets/CreativeContentPacket.h"

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <cstdint>
#include <cstddef>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <string>
#include <functional>
#include <chrono>
#include <deque>

class ItemUseTransaction;
class CommandOrigin;
class Block;
class CraftingEventPacket;
class CommandBlockUpdatePacket;
class Item;
class NetherNetInstance;
class PluginManager;

struct QueuedActorCommand {
    Level *mLevel = nullptr;
    Vector3f mPosition;
    Vector3f mRotation;
    std::string mCommand;
};

class ServerNetworkHandler : public NetworkHandler::Listener,
                             public NetworkPacketHandler,
                             public RakPeerHelper::IPSupportInterface,
                             public PacketSender {
public:
    using ModalFormCallback = std::function<void(ServerPlayer &, const std::string &, bool)>;

    ServerNetworkHandler(const std::string &serverName, const std::string &subName, int maxPlayers,
                         TransportLayer transport = TransportLayer::RakNet);

    bool isTransportReady() const { return mNetworkHandler != nullptr; }

    ~ServerNetworkHandler() override;

    bool startServerListening(const ConnectionDefinition &definition);

    void stopServerListening();

    void tick();

    void setMotd(const std::string &serverName, const std::string &subName);

    void setProtocolVersion(int protocolVersion, const std::string &gameVersion);

    void setProperties(const PropertiesSettings &properties);

    const PropertiesSettings &getProperties() const { return mProperties; }

    int getActivePlayerCount() const { return (int) mNetworkHandler->getConnectionCount(); }

    int64_t getCurrentTick() const { return mCurrentTick; }

    int64_t getUptimeSeconds() const;

    double getTicksPerSecond() const;

    double getMillisecondsPerTick() const;

    Profiler &getProfiler() { return mProfiler; }

    EventBus &getEventBus() { return mEventBus; }

    ScriptEngine &getScriptEngine() { return mScriptEngine; }

    PluginManager &getPluginManager() { return *mPluginManager; }

    std::string &getCraftingDataBytes() { return mCraftingDataBytes; }

    std::string &getCreativeContentBytes() { return mCreativeContentBytes; }

    std::string &getItemComponentsBytes() { return mItemComponentsBytes; }

    double getPeakMillisecondsPerTick() const;

    int getMaxPlayers() const { return mMaxPlayers; }

    NetworkHandler &getNetworkHandler() { return *mNetworkHandler; }

    Level &getLevel() { return mLevel; }

    Level &getDimension(DimensionType dimension);

    Level &getLevelFor(const Actor &actor);

    /** Every loaded level: the Overworld, then the Nether and the End when they exist. */
    std::vector<Level *> getLevels();

    void changePlayerDimension(ServerPlayer &player, DimensionType dimension, const Vector3f &position);

    void changeActorDimension(Actor &actor, DimensionType dimension, const Vector3f &position);

    void onPlayerDimensionChangeAck(ServerPlayer &player);

    std::unordered_map<NetworkIdentifier, ServerPlayer, NetworkIdentifier::Hasher> &getPlayers() { return mPlayers; }

    std::unordered_map<int64_t, std::unique_ptr<ServerActor>> &getActors() { return mActors; }

    ServerActor *spawnActor(Level &level, const std::string &identifier, const Vector3f &position,
                            const std::function<void(ServerActor &)> &configure = nullptr);

    ServerActor *spawnBabyActor(Level &level, const std::string &identifier, const Vector3f &position,
                                float scale);

    FallingBlock *spawnFallingBlock(Level &level, const BlockState &state, const Vector3f &position);

    PrimedTntActor *spawnPrimedTnt(Level &level, const Vector3f &position, const Vector3f &motion, int32_t fuse);

    void spawnExperienceOrbs(Level &level, const Vector3f &position, int amount);

    // Bows fire from eye level while thrown items leave the hand slightly lower.
    static constexpr float THROWN_PROJECTILE_DROP = -0.3f;

    ServerActor *spawnProjectile(ServerPlayer &player, const std::string &identifier, float speed,
                                 float verticalOffset = 0.0f);

    bool allowProjectileLaunch(ServerActor &projectile, Actor *shooter);

    bool onThrownProjectileHit(ServerActor &projectile, const Vector3f &hitPosition, ServerPlayer *hitPlayer);

    bool onThrownProjectileHitActor(ServerActor &projectile, const Vector3f &hitPosition, ServerActor &hitActor);

    bool onArrowProjectileHitTarget(ServerActor &projectile, const Vector3f &hitPosition, Actor &target);

    static float computeProjectileDamage(ServerActor &projectile);

    void returnProjectileToOwner(ServerPlayer &player, ServerActor &projectile);

    void dropProjectileItem(ServerActor &projectile, const Vector3f &position);

    void sendWeatherTo(ServerPlayer &player);

    void broadcastWeather();

    void setRaining(bool raining);

    void setThundering(bool thundering);

    void strikeLightning(Level &level, const Vector3f &position);

    void tickWeather();

    void applyPotionEffects(ServerPlayer &player, int32_t potionId, float durationScale);

    void applyPotionEffects(ServerActor &actor, int32_t potionId, float durationScale);

    void spawnLingeringCloud(Level &level, const Vector3f &position, int32_t potionId);

    void broadcastLevelEvent(Level &level, int32_t eventId, const Vector3f &position, int32_t data);

    ServerActor *getActor(int64_t uniqueId);

    ServerActor *getActorByRuntimeId(uint64_t runtimeId);

    void removeActor(int64_t uniqueId);

    void broadcastActorSpawn(ServerActor &actor);

    void refreshContainerViewers(const Vector3i &position, const ServerPlayer *except = nullptr);

    void sendActorsTo(ServerPlayer &player);

    void broadcastActorRemove(ServerActor &actor);

    void updateActorVisibility();

    /** Whether the viewer's client should have the other player spawned: same dimension, chunk sent. */
    bool canPlayerSeePlayer(ServerPlayer &viewer, const ServerPlayer &target) const;

    /** Spawns and despawns players for each other as they come in and out of view. */
    void updatePlayerVisibility();

    /** Sends the player's movement to every client that has them spawned. */
    void broadcastPlayerMove(ServerPlayer &player);

    /** Despawns the player for every client that has them spawned, as they leave the server. */
    void despawnPlayerForViewers(ServerPlayer &player);

    bool canPlayerSeeActor(ServerPlayer &player, const Actor &actor) const;

    void broadcastActorMove(ServerActor &actor);

    void syncActorProperties(ServerActor &actor);

    void queueActorCommand(ServerActor &actor, const std::string &command);

    void runQueuedActorCommands();

    void syncActorFlags(ServerActor &actor);

    void syncActorScale(ServerActor &actor, float scale);

    void syncActorAttributes(ServerActor &actor);

    void syncActorFirework(ServerActor &actor);

    void sendActorMetadata(ServerActor &actor, const EntityDataMap &metadata);

    void sendActorNameTag(ServerActor &actor);

    void broadcastActorEvent(ServerActor &actor, EntityEventType eventType);

    bool damageActor(ServerActor &actor, float amount, Actor *attacker, int32_t lootingLevel = -1);

    bool damageActor(ServerActor &actor, float amount, const ActorDamageSource &source, int32_t lootingLevel = -1);

    void hurtActor(Actor &actor, float amount, const std::string &deathMessageKey);

    void sendActorMotion(Actor &actor);

    void knockBack(Actor &actor, float deltaX, float deltaZ, float force, float verticalLimit = 0.4f);

    void pushFrom(Actor &actor, const Vector3f &origin, float strength, float lift);

    void playActorAnimation(ServerActor &actor, const std::string &animation);

    void applyActorEffect(ServerActor &actor, int32_t effectId, int32_t amplifier, int32_t durationTicks,
                          bool particles);

    void playSoundFor(ServerPlayer &player, const std::string &sound, const Vector3f &position, float volume,
                      float pitch);

    void spawnParticleEffect(Level &level, const std::string &identifier, const Vector3f &position);

    void playLevelSound(Level &level, const std::string &sound, const Vector3f &position,
                        const std::string &actorType = ":", int32_t extraData = -1);

    void playNamedSound(Level &level, const std::string &sound, const Vector3f &position, float volume,
                        float pitch);

    void spawnItemActor(Level &level, const std::string &typeId, int32_t amount, const Vector3f &position);

    void sendActionBar(ServerPlayer &player, const std::string &text, bool json);

    void sendTitle(ServerPlayer &player, const std::string &text, bool json);

    void sendJsonMessage(ServerPlayer &player, const std::string &json);

    void playPlayerAnimation(ServerPlayer &player, const std::string &animation);

    void setPlayerEquipment(ServerPlayer &player, const std::string &slot, const std::string &typeId, int32_t amount,
                            int32_t damage = 0, const Tag &dynamicProperties = Tag());

    void damagePlayerHeldItem(ServerPlayer &player, int32_t amount);

    ItemStack createItemStack(const std::string &identifier, int32_t count);

    int32_t repairWithMending(ServerPlayer &player, int32_t xp);

    void setContainerSlot(ServerPlayer &player, int32_t slot, const std::string &typeId, int32_t amount,
                          const Tag &dynamicProperties);

    void startPlayerItemCooldown(ServerPlayer &player, const std::string &category, int32_t durationTicks);

    void clearPlayerCamera(ServerPlayer &player);

    void displayScoreboardObjective(const std::string &slot, const std::string &objectiveId,
                                    const std::string &displayName);

    void clearScoreboardDisplay(const std::string &slot);

    void setScoreboardScore(const std::string &objectiveId, const std::string &participant, int32_t score);

    void removeScoreboardScore(const std::string &objectiveId, const std::string &participant);

    void removeScoreboardObjective(const std::string &objectiveId);

    void syncPlayerAttributes(ServerPlayer &player);

    void tickActors();

    void loadActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ);

    void saveActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ, bool cull);

    void loadBlockActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ);

    void saveBlockActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ, bool cull);

    void saveAllActors();

    void autoSave();

    bool syncActorPersistence(Level &level, const std::vector<int64_t> &activeColumns);

    void emitItemUse(ServerPlayer &player);

    std::unordered_map<std::string, DynamicPropertyValue> &getWorldDynamicProperties() {
        return mWorldDynamicProperties;
    }

    void loadWorldDynamicProperties();

    void saveWorldDynamicProperties();

    std::vector<std::unique_ptr<ItemActor>> &getItemEntities() { return mItemEntities; }

    uint64_t allocateRuntimeId() { return mNextRuntimeId++; }

    int64_t allocateActorUniqueId();

    TickingAreaManager &getTickingAreas() { return mTickingAreas; }

    void markActiveColumnsDirty() { mActorPersistencePending = true; }

    BlockDefinitionRegistry &getBlockDefinitions() { return mBlockDefinitions; }

    ItemDefinitionRegistry &getItemDefinitions() { return mItemDefinitions; }

    const PacketCodecContext &getCodecContext() const { return mCodecContext; }

    ServerPlayer *getPlayerByName(const std::string &name);

    std::vector<ServerPlayer *> resolveTargets(CommandOrigin &sender, const std::string &selector);

    std::vector<std::string> getPlayerNames() const;

    void setPlayerGameMode(ServerPlayer &player, int gameMode);

    void setPlayerOp(ServerPlayer &player, bool isOp);

    OpList &getOpList() { return mOps; }

    static constexpr const char *NOT_ALLOW_LISTED_MESSAGE = "disconnectionScreen.notAllowed";

    AllowList &getAllowList() {
        return mAllowList;
    }

    BanList &getBanList() {
        return mBanList;
    }

    BanList &getIpBanList() {
        return mIpBanList;
    }

    bool isAutoSaveEnabled() const { return mAutoSaveEnabled; }

    void setAutoSaveEnabled(bool enabled) { mAutoSaveEnabled = enabled; }

    bool isStopRequested() const { return mStopRequested; }

    void requestStop() { mStopRequested = true; }

    void setMaxPlayers(int maxPlayers);

    bool isAllowListed(ServerPlayer &player);

    bool isServerFull(const NetworkIdentifier &joining) const;

    bool sleepOn(ServerPlayer &player, const Vector3i &head);

    void stopSleep(ServerPlayer &player);

    void wakeSleepersAt(const Vector3i &head);

    void setAllowListEnabled(bool enabled);

    void kickNotAllowListedPlayers();

    void sendCommandOutput(ServerPlayer &player, const CommandOriginData &origin, const std::string &message);

    void sendCommandOutput(ServerPlayer &player, const CommandOriginData &origin, const std::string &key,
                           const std::vector<std::string> &parameters);

    void broadcastSystemMessage(const std::string &message);

    void broadcastTranslation(const std::string &key, const std::vector<std::string> &parameters = {});

    void broadcastWorldTime();

    bool changeGameRule(const std::string &name, const std::string &value);

    void setDefaultGameType(GameType gameType);

    ItemActor *dropItem(Level &level, const Vector3f &position, const ItemStack &item, const Vector3f &motion,
                        int pickupDelay);

    DamageResult hurt(ServerPlayer &player, float amount, const ActorDamageSource &source);

    void killPlayer(ServerPlayer &player, const std::string &deathMessageKey,
                    const std::vector<std::string> &deathMessageParameters = {});

    void _throwItem(ServerPlayer &player, const ItemStack &item);

    void _useHeldItem(ServerPlayer &player);

    void _consumeHeldItem(ServerPlayer &player);

    void _tickItemUse(ServerPlayer &player);

    void _completeItemUse(ServerPlayer &player, int32_t itemId);

    void _sendInventory(ServerPlayer &player);

    const std::vector<CreativeItemData> &getCreativeItems() { _buildCreativeContent(); return mCreativeItems; }

    const std::vector<ItemStack> &getRecipeOutputs() { _buildCraftingData(); return mRecipeOutputs; }

    const std::vector<uint32_t> &getRecipeSourceIndices() { _buildCraftingData(); return mRecipeSourceIndices; }

    std::vector<CreativeItemData> &getCreativeItemsMutable() { return mCreativeItems; }

    std::vector<ItemStack> &getRecipeOutputsMutable() { return mRecipeOutputs; }

    std::vector<uint32_t> &getRecipeSourceIndicesMutable() { return mRecipeSourceIndices; }

    CraftingDataPacket &getCachedCraftingData() { return mCachedCraftingData; }

    void _loadPlayerData(ServerPlayer &player);

    void _sendHealth(ServerPlayer &player);

    void _sendActorSpawn(ServerPlayer &player, ServerActor &actor);

    void _sendActorRemove(ServerPlayer &player, const ServerActor &actor);

    ServerActor *_registerActor(std::unique_ptr<ServerActor> actor);

    void _unregisterActor(int64_t uniqueId);

    void _resolvePendingRides();

    bool _equipHeldArmor(ServerPlayer &player, const Item &itemType);

    void _sendEntityData(ServerPlayer &player);

    EntityDataMap _buildPlayerData(ServerPlayer &player);

    void _sendPlayerSpawn(ServerPlayer &viewer, ServerPlayer &target);

    void _sendPlayerRemove(ServerPlayer &viewer, const ServerPlayer &target);

    void _broadcastEntityEvent(const Actor &entity, uint8_t eventId);

    void _broadcastEntityEvent(const Actor &entity, uint8_t eventId, int32_t eventData);

    void _sendChunks(ServerPlayer &player);

    void _handleFallDamage(ServerPlayer &player, const Block *supportBlock = nullptr);

    void _disconnect(const NetworkIdentifier &id, const std::string &reason);

    bool _allowPacket(const NetworkIdentifier &id, RateLimitedPacket category);

    void _rejectBadPacket(const NetworkIdentifier &id, const std::string &reason);

    ResourcePackManager &getResourcePacks() { return mResourcePacks; }

    PingedCompatibleServer &getAnnouncement() { return mAnnouncement; }

    CommandMap &getCommands() { return mCommands; }

    BiomeRegistry &getBiomes() { return mBiomes; }

    std::vector<CreativeItemGroup> &getCreativeGroups() { return mCreativeGroups; }

    PlayerDataProvider &getPlayerDataProvider() { return mPlayerData; }

    void _registerVanillaDefinitions();

    void _sendStartGame(ServerPlayer &player);

    void _sendAbilities(ServerPlayer &player);

    void _addToPlayerList(ServerPlayer &player);

    void _removeFromPlayerList(ServerPlayer &player);

    void _sendAttributes(ServerPlayer &player);

    void _checkTerrainReady(ServerPlayer &player);

    void _sendBiomeDefinitions(ServerPlayer &player);

    void _sendItemComponents(ServerPlayer &player);

    void _sendActorIdentifiers(ServerPlayer &player);

    void _sendCraftingData(ServerPlayer &player);

    void _buildCraftingData();

    void _buildCreativeContent();

    void _sendCreativeContent(ServerPlayer &player);

    void _sendAvailableCommands(ServerPlayer &player);

    void sendPacketTo(const NetworkIdentifier &id, const Packet &packet) override;

    void queueConsoleCommand(const std::string &commandLine);

    void postToMainThread(std::function<void()> task);

    void sendModalForm(ServerPlayer &player, uint32_t formId, const std::string &formData,
                       ModalFormCallback callback = {});

private:
    bool onValidateIncomingConnection(const NetworkIdentifier &id) override;

    void onNewIncomingConnection(const NetworkIdentifier &id) override;

    void onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                            const std::string &message) override;

    void onDataReceived(const NetworkIdentifier &id, const std::string &data) override;

    void onConnectionFailed(const NetworkIdentifier &id) override;

    void handle(const NetworkIdentifier &id, const RequestNetworkSettingsPacket &packet) override;

    void handle(const NetworkIdentifier &id, const LoginPacket &packet) override;

    void handle(const NetworkIdentifier &id, const ClientToServerHandshakePacket &packet) override;

    void handle(const NetworkIdentifier &id, const ResourcePackClientResponsePacket &packet) override;

    void handle(const NetworkIdentifier &id, const ResourcePackChunkRequestPacket &packet) override;

    void handle(const NetworkIdentifier &id, const SetLocalPlayerAsInitializedPacket &packet) override;

    void handle(const NetworkIdentifier &id, const PlayerAuthInputPacket &packet) override;

    void handle(const NetworkIdentifier &id, const CompletedUsingItemPacket &packet) override;

    void handle(const NetworkIdentifier &id, const CommandRequestPacket &packet) override;

    void handle(const NetworkIdentifier &id, const TextPacket &packet) override;

    void handle(const NetworkIdentifier &id, const PacketViolationWarningPacket &packet) override;

    void handle(const NetworkIdentifier &id, const RequestChunkRadiusPacket &packet) override;

    void handle(const NetworkIdentifier &id, const SubChunkRequestPacket &packet) override;

    void handle(const NetworkIdentifier &id, const RequestAbilityPacket &packet) override;

    void handle(const NetworkIdentifier &id, const MobEquipmentPacket &packet) override;

    void handle(const NetworkIdentifier &id, const PlayerHotbarPacket &packet) override;

    void handle(const NetworkIdentifier &id, const ItemStackRequestPacket &packet) override;

    void handle(const NetworkIdentifier &id, const CraftingEventPacket &packet) override;

    void handle(const NetworkIdentifier &id, const InventoryTransactionPacket &packet) override;

    void handle(const NetworkIdentifier &id, const ContainerClosePacket &packet) override;

    void handle(const NetworkIdentifier &id, const InteractPacket &packet) override;

    void handle(const NetworkIdentifier &id, const PlayerActionPacket &packet) override;

    void handle(const NetworkIdentifier &id, const RespawnPacket &packet) override;

    void handle(const NetworkIdentifier &id, const SetPlayerGameTypePacket &packet) override;

    void handle(const NetworkIdentifier &id, const BlockActorDataPacket &packet) override;

    void handle(const NetworkIdentifier &id, const CommandBlockUpdatePacket &packet) override;

    void handle(const NetworkIdentifier &id, const BlockPickRequestPacket &packet) override;

    void handle(const NetworkIdentifier &id, const ActorPickRequestPacket &packet) override;

    void handle(const NetworkIdentifier &id, const EmotePacket &packet) override;

    void handle(const NetworkIdentifier &id, const ModalFormResponsePacket &packet) override;

    void handle(const NetworkIdentifier &id, const PlayerSkinPacket &packet) override;

    ServerPlayer *_getPlayer(const NetworkIdentifier &id);

    int _getServerViewDistance() const;

    void _logPackStack() const;

    size_t _getChunkWorkerThreadCount() const;

    void _loadScripts();

    void _validatePackDependencies();

    void _savePlayerData(const ServerPlayer &player);

    void _handleVoidDamage(ServerPlayer &player);

    void _handleSuffocationDamage(ServerPlayer &player);

    bool _isEyeInsideSolidBlock(Level &level, const Vector3f &position, float height);

    void _dropInventoryOnDeath(ServerPlayer &player);

    void _respawnPlayer(ServerPlayer &player);

    Vector3f _respawnPositionFor(ServerPlayer &player);

    void _tickSleep();

    void _tickPlayer(ServerPlayer &player);

    void _registerCommands();

    void onReceiveIPSupport(RakPeerHelper::IPSupport support) override;

    void _updateServerAnnouncement();

    std::unique_ptr<NetworkHandler> mNetworkHandler;
    RakNetInstance *mRakNetInstance;
    NetherNetInstance *mNetherNetInstance = nullptr;
    BlockDefinitionRegistry mBlockDefinitions;
    ItemDefinitionRegistry mItemDefinitions;
    PacketCodecContext mCodecContext;

    PropertiesSettings mProperties;
    static const int DEFAULT_VIEW_DISTANCE = 4;
    static const int MAX_VIEW_DISTANCE = 12;
public:
    unsigned getChunksPerTick() const { return CHUNKS_PER_TICK; }

private:
    static const unsigned CHUNKS_PER_TICK = 32;
    static const unsigned CHUNK_REQUESTS_PER_TICK = 64;
    static const size_t SPAWN_CHUNK_THRESHOLD = 56;

    void _tickDimension(Level &level);

    Level mLevel;
    std::unique_ptr<Level> mNetherLevel;
    std::unique_ptr<Level> mTheEndLevel;
    BiomeRegistry mBiomes;
    std::vector<CreativeItemGroup> mCreativeGroups;
    std::vector<CreativeItemData> mCreativeItems;
    CraftingDataPacket mCachedCraftingData;
    std::vector<ItemStack> mRecipeOutputs;
    std::vector<uint32_t> mRecipeSourceIndices;
    PlayerDataProvider mPlayerData;
    OpList mOps;
    AllowList mAllowList;
    BanList mBanList;
    BanList mIpBanList;
    bool mAutoSaveEnabled = true;
    bool mStopRequested = false;
    ResourcePackManager mResourcePacks;
    CommandMap mCommands;
    PingedCompatibleServer mAnnouncement;
    int mMaxPlayers;
    bool mIsListening;

    std::unordered_map<NetworkIdentifier, ServerPlayer, NetworkIdentifier::Hasher> mPlayers;
    std::unordered_map<int64_t, std::unique_ptr<ServerActor>> mActors;
    std::unordered_map<uint64_t, int64_t> mActorUniqueIdsByRuntimeId;
    std::unordered_set<int64_t> mVehiclesWithPendingPassengers;
    TickingAreaManager mTickingAreas;
    uint32_t mActorUniqueIdCounter = 0;
    std::vector<QueuedActorCommand> mQueuedActorCommands;
    NaturalSpawner mNaturalSpawner;
    std::unordered_map<NetworkIdentifier, PacketRateLimiter, NetworkIdentifier::Hasher> mRateLimiters;

    struct LingeringCloud {
        int32_t mPotionId;
        int32_t mAge;
        int32_t mWaitTime;
        int32_t mDuration;
        int32_t mNextApply;
        int32_t mReapplicationDelay;
        float mRadius;
        float mRadiusPerTick;
        float mRadiusOnUse;
        Vector3f mPosition;
        DimensionType mDimension = DimensionType::Overworld;
    };

    std::unordered_map<int64_t, LingeringCloud> mLingeringClouds;
    std::array<std::unordered_set<int64_t>, Dimension::DIMENSION_COUNT> mActorLoadedChunks;
    std::vector<int64_t> mDetachedActors;
    std::vector<int64_t> mActiveCenters;
    int mActiveTickDistance = -1;
    bool mActorPersistencePending = true;
    std::unordered_map<std::string, DynamicPropertyValue> mWorldDynamicProperties;
    std::unordered_map<std::string, int64_t> mScoreboardIds;
    int64_t mNextScoreboardId = 1;
    std::unordered_map<NetworkIdentifier, std::unordered_map<uint32_t, ModalFormCallback>,
                       NetworkIdentifier::Hasher> mModalFormCallbacks;
    std::vector<std::unique_ptr<ItemActor>> mItemEntities;
    uint64_t mNextRuntimeId;
    int64_t mCurrentTick = 0;
    int32_t mSleepTicks = 0;
    std::chrono::steady_clock::time_point mStartTime = std::chrono::steady_clock::now();

    Profiler mProfiler;
    EventBus mEventBus;
    ScriptEngine mScriptEngine;
    BehaviorPackManager mBehaviorPacks;

    std::string mCraftingDataBytes;
    std::string mCreativeContentBytes;
    std::string mItemComponentsBytes;

    static const size_t TICK_SAMPLE_COUNT = 100;
    std::deque<std::chrono::steady_clock::time_point> mTickStartSamples;
    std::deque<double> mTickDurationSamples;

    std::mutex mConsoleQueueMutex;
    std::queue<std::string> mConsoleQueue;

    std::mutex mMainThreadTaskMutex;
    std::vector<std::function<void()>> mMainThreadTasks;

    std::unique_ptr<PluginManager> mPluginManager;
};
