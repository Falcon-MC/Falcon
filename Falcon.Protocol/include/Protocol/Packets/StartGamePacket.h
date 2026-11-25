#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "Core/NBT/Tag.h"
#include "Core/Utility/UUID.h"
#include "Protocol/Packet.h"
#include "Protocol/Types/StartGameTypes.h"

#include <string>
#include <vector>

class StartGamePacket : public Packet {
public:
    static const MinecraftPacketIds ID = MinecraftPacketIds::StartGame;

    StartGamePacket();

    MinecraftPacketIds getId() const override { return ID; }

    const char *getName() const override { return "StartGamePacket"; }

    void write(BinaryStream &stream, const PacketCodecContext &context) const override;

    void read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) override;

    void handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const override;

    int64_t mUniqueActorId;
    uint64_t mRuntimeActorId;
    GameType mPlayerGameType;
    Vector3f mPlayerPosition;
    Vector2f mRotation;

    // level settings
    int64_t mSeed;
    SpawnBiomeType mSpawnBiomeType;
    std::string mCustomBiomeName;
    int32_t mDimensionId;
    int32_t mGeneratorId;
    GameType mLevelGameType;
    bool mHardcore;
    int32_t mDifficulty;
    Vector3i mDefaultSpawn;
    bool mAchievementsDisabled;
    EditorWorldType mEditorWorldType;
    bool mCreatedInEditor;
    bool mExportedFromEditor;
    EditorLevelMigrationVersion mEditorLevelMigrationVersion;
    int32_t mDayCycleStopTime;
    uint32_t mEduEditionOffers;
    bool mEduFeaturesEnabled;
    std::string mEducationProductionId;
    float mRainLevel;
    float mLightningLevel;
    bool mPlatformLockedContentConfirmed;
    bool mMultiplayerGame;
    bool mBroadcastingToLan;
    GamePublishSetting mXblBroadcastMode;
    GamePublishSetting mPlatformBroadcastMode;
    bool mCommandsEnabled;
    bool mTexturePacksRequired;
    std::vector<GameRuleData> mGamerules;
    std::vector<ExperimentData> mExperiments;
    bool mExperimentsPreviouslyToggled;
    bool mBonusChestEnabled;
    bool mStartingWithMap;
    PlayerPermission mDefaultPlayerPermission;
    int32_t mServerChunkTickRange;
    bool mBehaviorPackLocked;
    bool mResourcePackLocked;
    bool mFromLockedWorldTemplate;
    bool mUsingMsaGamertagsOnly;
    bool mFromWorldTemplate;
    bool mWorldTemplateOptionLocked;
    bool mOnlySpawningV1Villagers;
    bool mDisablingPersonas;
    bool mDisablingCustomSkins;
    bool mEmoteChatMuted;
    std::string mVanillaVersion;
    int32_t mLimitedWorldWidth;
    int32_t mLimitedWorldHeight;
    bool mNetherType;
    EduSharedUriResource mEduSharedUriResource;
    bool mHasForceExperimentalGameplay;
    bool mForceExperimentalGameplay;
    ChatRestrictionLevel mChatRestrictionLevel;
    bool mDisablingPlayerInteractions;
    int32_t mServerEditorConnectionPolicy;
    bool mAllowAnonymousBlockDropsInEditorWorlds;

    // level identity
    std::string mLevelId;
    std::string mLevelName;
    std::string mPremiumWorldTemplateId;
    bool mTrial;

    // synced player movement settings
    int32_t mRewindHistorySize;
    bool mServerAuthoritativeBlockBreaking;

    int64_t mCurrentTick;
    int32_t mEnchantmentSeed;
    std::vector<BlockPropertyData> mBlockProperties;
    std::string mMultiplayerCorrelationId;
    bool mInventoriesServerAuthoritative;
    std::string mServerEngine;
    Tag mPlayerPropertyData;
    uint64_t mBlockRegistryChecksum;
    Uuid mWorldTemplateId;
    bool mClientSideGenerationEnabled;
    bool mBlockNetworkIdsHashed;
    NetworkPermissions mNetworkPermissions;

    bool mHasServerConfigurationJoinInfo;
    ServerConfigurationJoinInfo mServerConfigurationJoinInfo;

    std::string mServerId;
    std::string mScenarioId;
    std::string mWorldId;
    std::string mOwnerId;
};
