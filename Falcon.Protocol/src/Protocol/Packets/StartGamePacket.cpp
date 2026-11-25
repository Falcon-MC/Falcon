#include "Protocol/Packets/StartGamePacket.h"

#include "Core/NBT/NbtIo.h"
#include "Protocol/NetworkPacketHandler.h"

StartGamePacket::StartGamePacket()
        : mUniqueActorId(0), mRuntimeActorId(0), mPlayerGameType(GameType::Survival),
          mSeed(0), mSpawnBiomeType(SpawnBiomeType::Default), mDimensionId(0), mGeneratorId(1),
          mLevelGameType(GameType::Survival), mHardcore(false), mDifficulty(1), mDefaultSpawn(0, 64, 0),
          mAchievementsDisabled(true), mEditorWorldType(EditorWorldType::NonEditor), mCreatedInEditor(false),
          mExportedFromEditor(false), mEditorLevelMigrationVersion(EditorLevelMigrationVersion::Legacy),
          mDayCycleStopTime(0), mEduEditionOffers(0), mEduFeaturesEnabled(false),
          mRainLevel(0.0f), mLightningLevel(0.0f), mPlatformLockedContentConfirmed(false), mMultiplayerGame(true),
          mBroadcastingToLan(true), mXblBroadcastMode(GamePublishSetting::Public),
          mPlatformBroadcastMode(GamePublishSetting::Public), mCommandsEnabled(true), mTexturePacksRequired(false),
          mExperimentsPreviouslyToggled(false), mBonusChestEnabled(false), mStartingWithMap(false),
          mDefaultPlayerPermission(PlayerPermission::Member), mServerChunkTickRange(4), mBehaviorPackLocked(false),
          mResourcePackLocked(false), mFromLockedWorldTemplate(false), mUsingMsaGamertagsOnly(false),
          mFromWorldTemplate(false), mWorldTemplateOptionLocked(false), mOnlySpawningV1Villagers(false),
          mDisablingPersonas(false), mDisablingCustomSkins(false), mEmoteChatMuted(false),
          mLimitedWorldWidth(16), mLimitedWorldHeight(16), mNetherType(false),
          mHasForceExperimentalGameplay(false), mForceExperimentalGameplay(false),
          mChatRestrictionLevel(ChatRestrictionLevel::None), mDisablingPlayerInteractions(false),
          mServerEditorConnectionPolicy(0), mAllowAnonymousBlockDropsInEditorWorlds(false),
          mTrial(false), mRewindHistorySize(0), mServerAuthoritativeBlockBreaking(false), mCurrentTick(0),
          mEnchantmentSeed(0), mInventoriesServerAuthoritative(true), mPlayerPropertyData(Tag::ofCompound()),
          mBlockRegistryChecksum(0), mClientSideGenerationEnabled(false), mBlockNetworkIdsHashed(true),
          mHasServerConfigurationJoinInfo(false) {}

void StartGamePacket::write(BinaryStream &stream, const PacketCodecContext &context) const {
    stream.putVarLong(mUniqueActorId);
    stream.putUnsignedVarLong(mRuntimeActorId);
    stream.putVarInt((int32_t) mPlayerGameType);
    stream.putVector3f(mPlayerPosition);
    stream.putVector2f(mRotation);

    // level settings
    stream.putLLong((uint64_t) mSeed);
    stream.putLShort((uint16_t) mSpawnBiomeType);
    stream.putString(mCustomBiomeName);
    stream.putVarInt(mDimensionId);
    stream.putVarInt(mGeneratorId);
    stream.putVarInt((int32_t) mLevelGameType);
    stream.putBool(mHardcore);
    stream.putVarInt(mDifficulty);
    stream.putBlockPosition(mDefaultSpawn);
    stream.putBool(mAchievementsDisabled);
    stream.putVarInt((int32_t) mEditorWorldType);
    stream.putBool(mCreatedInEditor);
    stream.putBool(mExportedFromEditor);
    if (context.getCapabilities().mStartGameEditorMigration)
        stream.putByte((unsigned char) mEditorLevelMigrationVersion);
    stream.putVarInt(mDayCycleStopTime);
    stream.putUnsignedVarInt(mEduEditionOffers);
    stream.putBool(mEduFeaturesEnabled);
    stream.putString(mEducationProductionId);
    stream.putLFloat(mRainLevel);
    stream.putLFloat(mLightningLevel);
    stream.putBool(mPlatformLockedContentConfirmed);
    stream.putBool(mMultiplayerGame);
    stream.putBool(mBroadcastingToLan);
    stream.putVarInt((int32_t) mXblBroadcastMode);
    stream.putVarInt((int32_t) mPlatformBroadcastMode);
    stream.putBool(mCommandsEnabled);
    stream.putBool(mTexturePacksRequired);

    stream.putArrayLength((uint32_t) mGamerules.size());
    for (const GameRuleData &rule: mGamerules)
        rule.write(stream);

    writeExperiments(stream, mExperiments);
    stream.putBool(mExperimentsPreviouslyToggled);
    stream.putBool(mBonusChestEnabled);
    stream.putBool(mStartingWithMap);
    stream.putByte((unsigned char) mDefaultPlayerPermission);
    stream.putLInt((uint32_t) mServerChunkTickRange);
    stream.putBool(mBehaviorPackLocked);
    stream.putBool(mResourcePackLocked);
    stream.putBool(mFromLockedWorldTemplate);
    stream.putBool(mUsingMsaGamertagsOnly);
    stream.putBool(mFromWorldTemplate);
    stream.putBool(mWorldTemplateOptionLocked);
    stream.putBool(mOnlySpawningV1Villagers);
    stream.putBool(mDisablingPersonas);
    stream.putBool(mDisablingCustomSkins);
    stream.putBool(mEmoteChatMuted);
    stream.putString(mVanillaVersion);
    stream.putLInt((uint32_t) mLimitedWorldWidth);
    stream.putLInt((uint32_t) mLimitedWorldHeight);
    stream.putBool(mNetherType);
    stream.putString(mEduSharedUriResource.mButtonName);
    stream.putString(mEduSharedUriResource.mLinkUri);

    stream.putOptionalPresent(mHasForceExperimentalGameplay);
    if (mHasForceExperimentalGameplay)
        stream.putBool(mForceExperimentalGameplay);

    stream.putByte((unsigned char) mChatRestrictionLevel);
    stream.putBool(mDisablingPlayerInteractions);
    stream.putVarInt(mServerEditorConnectionPolicy);
    stream.putBool(mAllowAnonymousBlockDropsInEditorWorlds);

    // level identity
    stream.putString(mLevelId);
    stream.putString(mLevelName);
    stream.putString(mPremiumWorldTemplateId);
    stream.putBool(mTrial);

    stream.putVarInt(mRewindHistorySize);
    stream.putBool(mServerAuthoritativeBlockBreaking);

    stream.putLLong((uint64_t) mCurrentTick);
    stream.putVarInt(mEnchantmentSeed);

    stream.putArrayLength((uint32_t) mBlockProperties.size());
    for (const BlockPropertyData &block: mBlockProperties) {
        stream.putString(block.mName);
        NbtIo::writeTag(stream, block.mProperties, NbtVariant::Network);
    }

    // item definitions were removed from this packet in v776

    stream.putString(mMultiplayerCorrelationId);
    stream.putBool(mInventoriesServerAuthoritative);
    stream.putString(mServerEngine);
    NbtIo::writeTag(stream, mPlayerPropertyData, NbtVariant::Network);
    stream.putLLong(mBlockRegistryChecksum);
    stream.putUuid(mWorldTemplateId);
    stream.putBool(mClientSideGenerationEnabled);
    stream.putBool(mBlockNetworkIdsHashed);
    stream.putBool(mNetworkPermissions.mServerAuthSounds);

    stream.putOptionalPresent(mHasServerConfigurationJoinInfo);
    if (mHasServerConfigurationJoinInfo)
        mServerConfigurationJoinInfo.write(stream);

    stream.putString(mServerId);
    stream.putString(mScenarioId);
    stream.putString(mWorldId);
    stream.putString(mOwnerId);
}

void StartGamePacket::read(ReadOnlyBinaryStream &stream, const PacketCodecContext &context) {
    mUniqueActorId = stream.getVarLong();
    mRuntimeActorId = stream.getUnsignedVarLong();
    mPlayerGameType = (GameType) stream.getVarInt();
    mPlayerPosition = stream.getVector3f();
    mRotation = stream.getVector2f();

    mSeed = (int64_t) stream.getLLong();
    mSpawnBiomeType = (SpawnBiomeType) stream.getLShort();
    mCustomBiomeName = stream.getString();
    mDimensionId = stream.getVarInt();
    mGeneratorId = stream.getVarInt();
    mLevelGameType = (GameType) stream.getVarInt();
    mHardcore = stream.getBool();
    mDifficulty = stream.getVarInt();
    mDefaultSpawn = stream.getBlockPosition();
    mAchievementsDisabled = stream.getBool();
    mEditorWorldType = (EditorWorldType) stream.getVarInt();
    mCreatedInEditor = stream.getBool();
    mExportedFromEditor = stream.getBool();
    if (context.getCapabilities().mStartGameEditorMigration)
        mEditorLevelMigrationVersion = (EditorLevelMigrationVersion) (int8_t) stream.getByte();
    mDayCycleStopTime = stream.getVarInt();
    mEduEditionOffers = stream.getUnsignedVarInt();
    mEduFeaturesEnabled = stream.getBool();
    mEducationProductionId = stream.getString();
    mRainLevel = stream.getLFloat();
    mLightningLevel = stream.getLFloat();
    mPlatformLockedContentConfirmed = stream.getBool();
    mMultiplayerGame = stream.getBool();
    mBroadcastingToLan = stream.getBool();
    mXblBroadcastMode = (GamePublishSetting) stream.getVarInt();
    mPlatformBroadcastMode = (GamePublishSetting) stream.getVarInt();
    mCommandsEnabled = stream.getBool();
    mTexturePacksRequired = stream.getBool();

    const uint32_t ruleCount = stream.getArrayLength();
    mGamerules.clear();
    mGamerules.reserve(ruleCount);

    for (uint32_t i = 0; i < ruleCount; i++) {
        GameRuleData rule;
        rule.read(stream);
        mGamerules.push_back(rule);
    }

    readExperiments(stream, mExperiments);
    mExperimentsPreviouslyToggled = stream.getBool();
    mBonusChestEnabled = stream.getBool();
    mStartingWithMap = stream.getBool();
    mDefaultPlayerPermission = (PlayerPermission) stream.getByte();
    mServerChunkTickRange = stream.getSignedLInt();
    mBehaviorPackLocked = stream.getBool();
    mResourcePackLocked = stream.getBool();
    mFromLockedWorldTemplate = stream.getBool();
    mUsingMsaGamertagsOnly = stream.getBool();
    mFromWorldTemplate = stream.getBool();
    mWorldTemplateOptionLocked = stream.getBool();
    mOnlySpawningV1Villagers = stream.getBool();
    mDisablingPersonas = stream.getBool();
    mDisablingCustomSkins = stream.getBool();
    mEmoteChatMuted = stream.getBool();
    mVanillaVersion = stream.getString();
    mLimitedWorldWidth = stream.getSignedLInt();
    mLimitedWorldHeight = stream.getSignedLInt();
    mNetherType = stream.getBool();
    mEduSharedUriResource.mButtonName = stream.getString();
    mEduSharedUriResource.mLinkUri = stream.getString();

    mHasForceExperimentalGameplay = stream.getOptionalPresent();
    if (mHasForceExperimentalGameplay)
        mForceExperimentalGameplay = stream.getBool();

    mChatRestrictionLevel = (ChatRestrictionLevel) stream.getByte();
    mDisablingPlayerInteractions = stream.getBool();
    mServerEditorConnectionPolicy = stream.getVarInt();
    mAllowAnonymousBlockDropsInEditorWorlds = stream.getBool();

    mLevelId = stream.getString();
    mLevelName = stream.getString();
    mPremiumWorldTemplateId = stream.getString();
    mTrial = stream.getBool();

    mRewindHistorySize = stream.getVarInt();
    mServerAuthoritativeBlockBreaking = stream.getBool();

    mCurrentTick = (int64_t) stream.getLLong();
    mEnchantmentSeed = stream.getVarInt();

    const uint32_t blockCount = stream.getArrayLength();
    mBlockProperties.clear();
    mBlockProperties.reserve(blockCount);

    for (uint32_t i = 0; i < blockCount; i++) {
        BlockPropertyData block;
        block.mName = stream.getString();
        block.mProperties = NbtIo::readTag(stream, NbtVariant::Network);
        mBlockProperties.push_back(block);
    }

    mMultiplayerCorrelationId = stream.getString();
    mInventoriesServerAuthoritative = stream.getBool();
    mServerEngine = stream.getString();
    mPlayerPropertyData = NbtIo::readTag(stream, NbtVariant::Network);
    mBlockRegistryChecksum = stream.getLLong();
    mWorldTemplateId = stream.getUuid();
    mClientSideGenerationEnabled = stream.getBool();
    mBlockNetworkIdsHashed = stream.getBool();
    mNetworkPermissions.mServerAuthSounds = stream.getBool();

    mHasServerConfigurationJoinInfo = stream.getOptionalPresent();
    if (mHasServerConfigurationJoinInfo)
        mServerConfigurationJoinInfo.read(stream);

    mServerId = stream.getString();
    mScenarioId = stream.getString();
    mWorldId = stream.getString();
    mOwnerId = stream.getString();
}

void StartGamePacket::handle(const NetworkIdentifier &id, NetworkPacketHandler &handler) const {
    handler.handle(id, *this);
}
