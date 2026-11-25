#include "Protocol/MinecraftPackets.h"

#include "Protocol/Codec/ProtocolCodecRegistry.h"

#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/ActorPickRequestPacket.h"
#include "Protocol/Packets/AddActorPacket.h"
#include "Protocol/Packets/AddBehaviorTreePacket.h"
#include "Protocol/Packets/AddItemActorPacket.h"
#include "Protocol/Packets/AddPaintingPacket.h"
#include "Protocol/Packets/AddPlayerPacket.h"
#include "Protocol/Packets/AddVolumeEntityPacket.h"
#include "Protocol/Packets/AdventureSettingsPacket.h"
#include "Protocol/Packets/AgentActionEventPacket.h"
#include "Protocol/Packets/AgentAnimationPacket.h"
#include "Protocol/Packets/AnimateEntityPacket.h"
#include "Protocol/Packets/AnimatePacket.h"
#include "Protocol/Packets/AnvilDamagePacket.h"
#include "Protocol/Packets/AutomationClientConnectPacket.h"
#include "Protocol/Packets/AvailableActorIdentifiersPacket.h"
#include "Protocol/Packets/AvailableCommandsPacket.h"
#include "Protocol/Packets/AwardAchievementPacket.h"
#include "Protocol/Packets/BiomeDefinitionListPacket.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Packets/BlockEventPacket.h"
#include "Protocol/Packets/BlockPickRequestPacket.h"
#include "Protocol/Packets/BookEditPacket.h"
#include "Protocol/Packets/BossEventPacket.h"
#include "Protocol/Packets/CameraAimAssistActorPriorityPacket.h"
#include "Protocol/Packets/CameraAimAssistPacket.h"
#include "Protocol/Packets/CameraAimAssistPresetsPacket.h"
#include "Protocol/Packets/CameraInstructionPacket.h"
#include "Protocol/Packets/CameraPacket.h"
#include "Protocol/Packets/CameraPresetsPacket.h"
#include "Protocol/Packets/CameraShakePacket.h"
#include "Protocol/Packets/CameraSplinePacket.h"
#include "Protocol/Packets/ChangeDimensionPacket.h"
#include "Protocol/Packets/ChangeMobPropertyPacket.h"
#include "Protocol/Packets/ChunkRadiusUpdatedPacket.h"
#include "Protocol/Packets/ClientCacheBlobStatusPacket.h"
#include "Protocol/Packets/ClientCacheMissResponsePacket.h"
#include "Protocol/Packets/ClientCacheStatusPacket.h"
#include "Protocol/Packets/ClientCameraAimAssistPacket.h"
#include "Protocol/Packets/ClientCheatAbilityPacket.h"
#include "Protocol/Packets/ClientMovementPredictionSyncPacket.h"
#include "Protocol/Packets/ClientToServerHandshakePacket.h"
#include "Protocol/Packets/ClientboundAttributeLayerSyncPacket.h"
#include "Protocol/Packets/ClientboundCloseFormPacket.h"
#include "Protocol/Packets/ClientboundControlSchemeSetPacket.h"
#include "Protocol/Packets/ClientboundDataDrivenUICloseScreenPacket.h"
#include "Protocol/Packets/ClientboundDataDrivenUIReloadPacket.h"
#include "Protocol/Packets/ClientboundDataDrivenUIShowScreenPacket.h"
#include "Protocol/Packets/ClientboundDataStorePacket.h"
#include "Protocol/Packets/ClientboundDebugRendererPacket.h"
#include "Protocol/Packets/ClientboundMapItemDataPacket.h"
#include "Protocol/Packets/ClientboundMatchmakingStatePacket.h"
#include "Protocol/Packets/ClientboundPlayAudioContentPacket.h"
#include "Protocol/Packets/ClientboundStonecutterSetRecipePacket.h"
#include "Protocol/Packets/ClientboundTextureShiftPacket.h"
#include "Protocol/Packets/ClientboundUpdateSoundDataPacket.h"
#include "Protocol/Packets/CodeBuilderPacket.h"
#include "Protocol/Packets/CodeBuilderSourcePacket.h"
#include "Protocol/Packets/CommandBlockUpdatePacket.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/CommandRequestPacket.h"
#include "Protocol/Packets/CompletedUsingItemPacket.h"
#include "Protocol/Packets/ContainerClosePacket.h"
#include "Protocol/Packets/ContainerOpenPacket.h"
#include "Protocol/Packets/ContainerRegistryCleanupPacket.h"
#include "Protocol/Packets/ContainerSetDataPacket.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "Protocol/Packets/CraftingDataPacket.h"
#include "Protocol/Packets/CraftingEventPacket.h"
#include "Protocol/Packets/CreatePhotoPacket.h"
#include "Protocol/Packets/CreativeContentPacket.h"
#include "Protocol/Packets/CurrentStructureFeaturePacket.h"
#include "Protocol/Packets/DeathInfoPacket.h"
#include "Protocol/Packets/DebugInfoPacket.h"
#include "Protocol/Packets/DimensionDataPacket.h"
#include "Protocol/Packets/DisconnectPacket.h"
#include "Protocol/Packets/EditorNetworkPacket.h"
#include "Protocol/Packets/EduUriResourcePacket.h"
#include "Protocol/Packets/EducationSettingsPacket.h"
#include "Protocol/Packets/EmoteListPacket.h"
#include "Protocol/Packets/EmotePacket.h"
#include "Protocol/Packets/EntityFallPacket.h"
#include "Protocol/Packets/FeatureRegistryPacket.h"
#include "Protocol/Packets/FilterTextPacket.h"
#include "Protocol/Packets/GameRulesChangedPacket.h"
#include "Protocol/Packets/GameTestRequestPacket.h"
#include "Protocol/Packets/GameTestResultsPacket.h"
#include "Protocol/Packets/GraphicsOverrideParameterPacket.h"
#include "Protocol/Packets/GuiDataPickItemPacket.h"
#include "Protocol/Packets/HurtArmorPacket.h"
#include "Protocol/Packets/InteractPacket.h"
#include "Protocol/Packets/InventoryContentPacket.h"
#include "Protocol/Packets/InventorySlotPacket.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/ItemRegistryPacket.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "Protocol/Packets/ItemStackResponsePacket.h"
#include "Protocol/Packets/JigsawStructureDataPacket.h"
#include "Protocol/Packets/LabTablePacket.h"
#include "Protocol/Packets/LegacyTelemetryEventPacket.h"
#include "Protocol/Packets/LessonProgressPacket.h"
#include "Protocol/Packets/LevelChunkPacket.h"
#include "Protocol/Packets/LevelEventGenericPacket.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/LocatorBarPacket.h"
#include "Protocol/Packets/LoginPacket.h"
#include "Protocol/Packets/MapCreateLockedCopyPacket.h"
#include "Protocol/Packets/MapInfoRequestPacket.h"
#include "Protocol/Packets/MobArmorEquipmentPacket.h"
#include "Protocol/Packets/MobEffectPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"
#include "Protocol/Packets/ModalFormRequestPacket.h"
#include "Protocol/Packets/ModalFormResponsePacket.h"
#include "Protocol/Packets/MotionPredictionHintsPacket.h"
#include "Protocol/Packets/MoveActorAbsolutePacket.h"
#include "Protocol/Packets/MoveActorDeltaPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Packets/MovementEffectPacket.h"
#include "Protocol/Packets/MultiplayerSettingsPacket.h"
#include "Protocol/Packets/NetworkChunkPublisherUpdatePacket.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Packets/NetworkStackLatencyPacket.h"
#include "Protocol/Packets/NpcDialoguePacket.h"
#include "Protocol/Packets/NpcRequestPacket.h"
#include "Protocol/Packets/OnScreenTextureAnimationPacket.h"
#include "Protocol/Packets/OpenSignPacket.h"
#include "Protocol/Packets/PacketViolationWarningPacket.h"
#include "Protocol/Packets/PartyChangedPacket.h"
#include "Protocol/Packets/PartyDestinationCookieResponsePacket.h"
#include "Protocol/Packets/PhotoInfoRequestPacket.h"
#include "Protocol/Packets/PhotoTransferPacket.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/PlayerActionPacket.h"
#include "Protocol/Packets/PlayerArmorDamagePacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Packets/PlayerEnchantOptionsPacket.h"
#include "Protocol/Packets/PlayerFogPacket.h"
#include "Protocol/Packets/PlayerHotbarPacket.h"
#include "Protocol/Packets/PlayerListPacket.h"
#include "Protocol/Packets/PlayerLocationPacket.h"
#include "Protocol/Packets/PlayerSkinPacket.h"
#include "Protocol/Packets/PlayerStartItemCooldownPacket.h"
#include "Protocol/Packets/PlayerToggleCrafterSlotRequestPacket.h"
#include "Protocol/Packets/PlayerUpdateEntityOverridesPacket.h"
#include "Protocol/Packets/PlayerVideoCapturePacket.h"
#include "Protocol/Packets/PositionTrackingDBClientRequestPacket.h"
#include "Protocol/Packets/PositionTrackingDBServerBroadcastPacket.h"
#include "Protocol/Packets/PrimitiveShapesPacket.h"
#include "Protocol/Packets/PurchaseReceiptPacket.h"
#include "Protocol/Packets/RecordStartedPacket.h"
#include "Protocol/Packets/RefreshEntitlementsPacket.h"
#include "Protocol/Packets/RemoveActorPacket.h"
#include "Protocol/Packets/RemoveObjectivePacket.h"
#include "Protocol/Packets/RemoveVolumeEntityPacket.h"
#include "Protocol/Packets/RequestAbilityPacket.h"
#include "Protocol/Packets/RequestChunkRadiusPacket.h"
#include "Protocol/Packets/RequestNetworkSettingsPacket.h"
#include "Protocol/Packets/RequestPermissionsPacket.h"
#include "Protocol/Packets/ResourcePackChunkDataPacket.h"
#include "Protocol/Packets/ResourcePackChunkRequestPacket.h"
#include "Protocol/Packets/ResourcePackClientResponsePacket.h"
#include "Protocol/Packets/ResourcePackDataInfoPacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ResourcePacksInfoPacket.h"
#include "Protocol/Packets/ResourcePacksReadyForValidationPacket.h"
#include "Protocol/Packets/RespawnPacket.h"
#include "Protocol/Packets/ScriptCustomEventPacket.h"
#include "Protocol/Packets/ScriptMessagePacket.h"
#include "Protocol/Packets/SendPartyDestinationCookiePacket.h"
#include "Protocol/Packets/ServerPlayerPostMovePositionPacket.h"
#include "Protocol/Packets/ServerPresenceInfoPacket.h"
#include "Protocol/Packets/ServerSettingsRequestPacket.h"
#include "Protocol/Packets/ServerSettingsResponsePacket.h"
#include "Protocol/Packets/ServerStatsPacket.h"
#include "Protocol/Packets/ServerStoreInfoPacket.h"
#include "Protocol/Packets/ServerToClientHandshakePacket.h"
#include "Protocol/Packets/ServerboundCursorItemDragPacket.h"
#include "Protocol/Packets/ServerboundDataDrivenScreenClosedPacket.h"
#include "Protocol/Packets/ServerboundMatchmakingCancelPacket.h"
#include "Protocol/Packets/ServerboundRegisterAudioContentPacket.h"
#include "Protocol/Packets/ServerboundStonecutterSetRecipePacket.h"
#include "Protocol/Packets/ServerboundDataStorePacket.h"
#include "Protocol/Packets/ServerboundDiagnosticsPacket.h"
#include "Protocol/Packets/ServerboundLoadingScreenPacket.h"
#include "Protocol/Packets/ServerboundPackSettingChangePacket.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Protocol/Packets/SetActorLinkPacket.h"
#include "Protocol/Packets/SetActorMotionPacket.h"
#include "Protocol/Packets/SetCommandsEnabledPacket.h"
#include "Protocol/Packets/SetDefaultGameTypePacket.h"
#include "Protocol/Packets/SetDifficultyPacket.h"
#include "Protocol/Packets/SetDisplayObjectivePacket.h"
#include "Protocol/Packets/SetHealthPacket.h"
#include "Protocol/Packets/SetHudPacket.h"
#include "Protocol/Packets/SetLastHurtByPacket.h"
#include "Protocol/Packets/SetLocalPlayerAsInitializedPacket.h"
#include "Protocol/Packets/SetMovementAuthorityPacket.h"
#include "Protocol/Packets/SetPassengerOfBlockPacket.h"
#include "Protocol/Packets/SetPlayerFurnaceOptionsPacket.h"
#include "Protocol/Packets/SetPlayerGameTypePacket.h"
#include "Protocol/Packets/SetPlayerInventoryOptionsPacket.h"
#include "Protocol/Packets/SetScorePacket.h"
#include "Protocol/Packets/SetScoreboardIdentityPacket.h"
#include "Protocol/Packets/SetSpawnPositionPacket.h"
#include "Protocol/Packets/SetTimePacket.h"
#include "Protocol/Packets/SetTitlePacket.h"
#include "Protocol/Packets/SettingsCommandPacket.h"
#include "Protocol/Packets/ShowCreditsPacket.h"
#include "Protocol/Packets/ShowProfilePacket.h"
#include "Protocol/Packets/ShowStoreOfferPacket.h"
#include "Protocol/Packets/SimpleEventPacket.h"
#include "Protocol/Packets/SimulationTypePacket.h"
#include "Protocol/Packets/SpawnExperienceOrbPacket.h"
#include "Protocol/Packets/SpawnParticleEffectPacket.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Protocol/Packets/StopSoundPacket.h"
#include "Protocol/Packets/StructureBlockUpdatePacket.h"
#include "Protocol/Packets/StructureTemplateDataRequestPacket.h"
#include "Protocol/Packets/StructureTemplateDataResponsePacket.h"
#include "Protocol/Packets/SubChunkPacket.h"
#include "Protocol/Packets/SubChunkRequestPacket.h"
#include "Protocol/Packets/SubClientLoginPacket.h"
#include "Protocol/Packets/SyncActorPropertyPacket.h"
#include "Protocol/Packets/SyncWorldClocksPacket.h"
#include "Protocol/Packets/TakeItemActorPacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Protocol/Packets/TickSyncPacket.h"
#include "Protocol/Packets/TickingAreasLoadStatusPacket.h"
#include "Protocol/Packets/ToastRequestPacket.h"
#include "Protocol/Packets/TransferPacket.h"
#include "Protocol/Packets/TrimDataPacket.h"
#include "Protocol/Packets/UnlockedRecipesPacket.h"
#include "Protocol/Packets/UpdateAbilitiesPacket.h"
#include "Protocol/Packets/UpdateAdventureSettingsPacket.h"
#include "Protocol/Packets/UpdateAttributesPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Protocol/Packets/UpdateBlockPropertiesPacket.h"
#include "Protocol/Packets/UpdateBlockSyncedPacket.h"
#include "Protocol/Packets/UpdateClientInputLocksPacket.h"
#include "Protocol/Packets/UpdateClientOptionsPacket.h"
#include "Protocol/Packets/UpdateEquipPacket.h"
#include "Protocol/Packets/UpdatePlayerGameTypePacket.h"
#include "Protocol/Packets/UpdateSoftEnumPacket.h"
#include "Protocol/Packets/UpdateSubChunkBlocksPacket.h"
#include "Protocol/Packets/UpdateTradePacket.h"
#include "Protocol/Packets/LecternUpdatePacket.h"
#include "Protocol/Packets/VoxelShapesPacket.h"

#include <mutex>
#include <unordered_map>

namespace {

    std::mutex &getFactoryMutex() {
        static std::mutex mutex;
        return mutex;
    }

    std::unordered_map<int, MinecraftPackets::PacketFactory> &getFactories() {
        static std::unordered_map<int, MinecraftPackets::PacketFactory> factories;
        return factories;
    }

}

void MinecraftPackets::registerPacket(MinecraftPacketIds id, const PacketFactory &factory) {
    if (!factory)
        return;

    std::lock_guard<std::mutex> guard(getFactoryMutex());
    getFactories()[(int) id] = factory;
}

void MinecraftPackets::registerVanillaPackets(ProtocolCodec &codec) {
    codec.registerPacket<ActorEventPacket>();
    codec.registerPacket<ActorPickRequestPacket>();
    codec.registerPacket<AddActorPacket>();
    codec.registerPacket<AddBehaviorTreePacket>();
    codec.registerPacket<AddItemActorPacket>();
    codec.registerPacket<AddPaintingPacket>();
    codec.registerPacket<AddPlayerPacket>();
    codec.registerPacket<AddVolumeEntityPacket>();
    codec.registerPacket<AdventureSettingsPacket>();
    codec.registerPacket<AgentActionEventPacket>();
    codec.registerPacket<AgentAnimationPacket>();
    codec.registerPacket<AnimateEntityPacket>();
    codec.registerPacket<AnimatePacket>();
    codec.registerPacket<AnvilDamagePacket>();
    codec.registerPacket<AutomationClientConnectPacket>();
    codec.registerPacket<AvailableActorIdentifiersPacket>();
    codec.registerPacket<AvailableCommandsPacket>();
    codec.registerPacket<AwardAchievementPacket>();
    codec.registerPacket<BiomeDefinitionListPacket>();
    codec.registerPacket<BlockActorDataPacket>();
    codec.registerPacket<BlockEventPacket>();
    codec.registerPacket<BlockPickRequestPacket>();
    codec.registerPacket<BookEditPacket>();
    codec.registerPacket<BossEventPacket>();
    codec.registerPacket<CameraAimAssistActorPriorityPacket>();
    codec.registerPacket<CameraAimAssistPacket>();
    codec.registerPacket<CameraAimAssistPresetsPacket>();
    codec.registerPacket<CameraInstructionPacket>();
    codec.registerPacket<CameraPacket>();
    codec.registerPacket<CameraPresetsPacket>();
    codec.registerPacket<CameraShakePacket>();
    codec.registerPacket<CameraSplinePacket>();
    codec.registerPacket<ChangeDimensionPacket>();
    codec.registerPacket<ChangeMobPropertyPacket>();
    codec.registerPacket<ChunkRadiusUpdatedPacket>();
    codec.registerPacket<ClientCacheBlobStatusPacket>();
    codec.registerPacket<ClientCacheMissResponsePacket>();
    codec.registerPacket<ClientCacheStatusPacket>();
    codec.registerPacket<ClientCameraAimAssistPacket>();
    codec.registerPacket<ClientCheatAbilityPacket>();
    codec.registerPacket<ClientMovementPredictionSyncPacket>();
    codec.registerPacket<ClientToServerHandshakePacket>();
    codec.registerPacket<ClientboundAttributeLayerSyncPacket>();
    codec.registerPacket<ClientboundCloseFormPacket>();
    codec.registerPacket<ClientboundControlSchemeSetPacket>();
    codec.registerPacket<ClientboundDataDrivenUICloseScreenPacket>();
    codec.registerPacket<ClientboundDataDrivenUIReloadPacket>();
    codec.registerPacket<ClientboundDataDrivenUIShowScreenPacket>();
    codec.registerPacket<ClientboundDataStorePacket>();
    codec.registerPacket<ClientboundDebugRendererPacket>();
    codec.registerPacket<ClientboundMapItemDataPacket>();
    codec.registerPacket<ClientboundMatchmakingStatePacket>();
    codec.registerPacket<ClientboundPlayAudioContentPacket>();
    codec.registerPacket<ClientboundStonecutterSetRecipePacket>();
    codec.registerPacket<ClientboundTextureShiftPacket>();
    codec.registerPacket<ClientboundUpdateSoundDataPacket>();
    codec.registerPacket<CodeBuilderPacket>();
    codec.registerPacket<CodeBuilderSourcePacket>();
    codec.registerPacket<CommandBlockUpdatePacket>();
    codec.registerPacket<CommandOutputPacket>();
    codec.registerPacket<CommandRequestPacket>();
    codec.registerPacket<CompletedUsingItemPacket>();
    codec.registerPacket<ContainerClosePacket>();
    codec.registerPacket<ContainerOpenPacket>();
    codec.registerPacket<ContainerRegistryCleanupPacket>();
    codec.registerPacket<ContainerSetDataPacket>();
    codec.registerPacket<CorrectPlayerMovePredictionPacket>();
    codec.registerPacket<CraftingDataPacket>();
    codec.registerPacket<CraftingEventPacket>();
    codec.registerPacket<CreatePhotoPacket>();
    codec.registerPacket<CreativeContentPacket>();
    codec.registerPacket<CurrentStructureFeaturePacket>();
    codec.registerPacket<DeathInfoPacket>();
    codec.registerPacket<DebugInfoPacket>();
    codec.registerPacket<DimensionDataPacket>();
    codec.registerPacket<DisconnectPacket>();
    codec.registerPacket<EditorNetworkPacket>();
    codec.registerPacket<EduUriResourcePacket>();
    codec.registerPacket<EducationSettingsPacket>();
    codec.registerPacket<EmoteListPacket>();
    codec.registerPacket<EmotePacket>();
    codec.registerPacket<EntityFallPacket>();
    codec.registerPacket<FeatureRegistryPacket>();
    codec.registerPacket<FilterTextPacket>();
    codec.registerPacket<GameRulesChangedPacket>();
    codec.registerPacket<GameTestRequestPacket>();
    codec.registerPacket<GameTestResultsPacket>();
    codec.registerPacket<GraphicsOverrideParameterPacket>();
    codec.registerPacket<GuiDataPickItemPacket>();
    codec.registerPacket<HurtArmorPacket>();
    codec.registerPacket<InteractPacket>();
    codec.registerPacket<InventoryContentPacket>();
    codec.registerPacket<InventorySlotPacket>();
    codec.registerPacket<InventoryTransactionPacket>();
    codec.registerPacket<ItemRegistryPacket>();
    codec.registerPacket<ItemStackRequestPacket>();
    codec.registerPacket<ItemStackResponsePacket>();
    codec.registerPacket<JigsawStructureDataPacket>();
    codec.registerPacket<LabTablePacket>();
    codec.registerPacket<LegacyTelemetryEventPacket>();
    codec.registerPacket<LessonProgressPacket>();
    codec.registerPacket<LevelChunkPacket>();
    codec.registerPacket<LevelEventGenericPacket>();
    codec.registerPacket<LevelEventPacket>();
    codec.registerPacket<LevelSoundEventPacket>();
    codec.registerPacket<LocatorBarPacket>();
    codec.registerPacket<LoginPacket>();
    codec.registerPacket<MapCreateLockedCopyPacket>();
    codec.registerPacket<MapInfoRequestPacket>();
    codec.registerPacket<MobArmorEquipmentPacket>();
    codec.registerPacket<MobEffectPacket>();
    codec.registerPacket<MobEquipmentPacket>();
    codec.registerPacket<ModalFormRequestPacket>();
    codec.registerPacket<ModalFormResponsePacket>();
    codec.registerPacket<MotionPredictionHintsPacket>();
    codec.registerPacket<MoveActorAbsolutePacket>();
    codec.registerPacket<MoveActorDeltaPacket>();
    codec.registerPacket<MovePlayerPacket>();
    codec.registerPacket<MovementEffectPacket>();
    codec.registerPacket<MultiplayerSettingsPacket>();
    codec.registerPacket<NetworkChunkPublisherUpdatePacket>();
    codec.registerPacket<NetworkSettingsPacket>();
    codec.registerPacket<NetworkStackLatencyPacket>();
    codec.registerPacket<NpcDialoguePacket>();
    codec.registerPacket<NpcRequestPacket>();
    codec.registerPacket<OnScreenTextureAnimationPacket>();
    codec.registerPacket<OpenSignPacket>();
    codec.registerPacket<PacketViolationWarningPacket>();
    codec.registerPacket<PartyChangedPacket>();
    codec.registerPacket<PartyDestinationCookieResponsePacket>();
    codec.registerPacket<PhotoInfoRequestPacket>();
    codec.registerPacket<PhotoTransferPacket>();
    codec.registerPacket<PlaySoundPacket>();
    codec.registerPacket<PlayStatusPacket>();
    codec.registerPacket<PlayerActionPacket>();
    codec.registerPacket<PlayerArmorDamagePacket>();
    codec.registerPacket<PlayerAuthInputPacket>();
    codec.registerPacket<PlayerEnchantOptionsPacket>();
    codec.registerPacket<PlayerFogPacket>();
    codec.registerPacket<PlayerHotbarPacket>();
    codec.registerPacket<PlayerListPacket>();
    codec.registerPacket<PlayerLocationPacket>();
    codec.registerPacket<PlayerSkinPacket>();
    codec.registerPacket<PlayerStartItemCooldownPacket>();
    codec.registerPacket<PlayerToggleCrafterSlotRequestPacket>();
    codec.registerPacket<PlayerUpdateEntityOverridesPacket>();
    codec.registerPacket<PlayerVideoCapturePacket>();
    codec.registerPacket<PositionTrackingDBClientRequestPacket>();
    codec.registerPacket<PositionTrackingDBServerBroadcastPacket>();
    codec.registerPacket<PrimitiveShapesPacket>();
    codec.registerPacket<PurchaseReceiptPacket>();
    codec.registerPacket<RecordStartedPacket>();
    codec.registerPacket<RefreshEntitlementsPacket>();
    codec.registerPacket<RemoveActorPacket>();
    codec.registerPacket<RemoveObjectivePacket>();
    codec.registerPacket<RemoveVolumeEntityPacket>();
    codec.registerPacket<RequestAbilityPacket>();
    codec.registerPacket<RequestChunkRadiusPacket>();
    codec.registerPacket<RequestNetworkSettingsPacket>();
    codec.registerPacket<RequestPermissionsPacket>();
    codec.registerPacket<ResourcePackChunkDataPacket>();
    codec.registerPacket<ResourcePackChunkRequestPacket>();
    codec.registerPacket<ResourcePackClientResponsePacket>();
    codec.registerPacket<ResourcePackDataInfoPacket>();
    codec.registerPacket<ResourcePackStackPacket>();
    codec.registerPacket<ResourcePacksInfoPacket>();
    codec.registerPacket<ResourcePacksReadyForValidationPacket>();
    codec.registerPacket<RespawnPacket>();
    codec.registerPacket<ScriptCustomEventPacket>();
    codec.registerPacket<ScriptMessagePacket>();
    codec.registerPacket<SendPartyDestinationCookiePacket>();
    codec.registerPacket<ServerPlayerPostMovePositionPacket>();
    codec.registerPacket<ServerPresenceInfoPacket>();
    codec.registerPacket<ServerSettingsRequestPacket>();
    codec.registerPacket<ServerSettingsResponsePacket>();
    codec.registerPacket<ServerStatsPacket>();
    codec.registerPacket<ServerStoreInfoPacket>();
    codec.registerPacket<ServerToClientHandshakePacket>();
    codec.registerPacket<ServerboundCursorItemDragPacket>();
    codec.registerPacket<ServerboundDataDrivenScreenClosedPacket>();
    codec.registerPacket<ServerboundDataStorePacket>();
    codec.registerPacket<ServerboundDiagnosticsPacket>();
    codec.registerPacket<ServerboundLoadingScreenPacket>();
    codec.registerPacket<ServerboundMatchmakingCancelPacket>();
    codec.registerPacket<ServerboundPackSettingChangePacket>();
    codec.registerPacket<ServerboundRegisterAudioContentPacket>();
    codec.registerPacket<ServerboundStonecutterSetRecipePacket>();
    codec.registerPacket<SetActorDataPacket>();
    codec.registerPacket<SetActorLinkPacket>();
    codec.registerPacket<SetActorMotionPacket>();
    codec.registerPacket<SetCommandsEnabledPacket>();
    codec.registerPacket<SetDefaultGameTypePacket>();
    codec.registerPacket<SetDifficultyPacket>();
    codec.registerPacket<SetDisplayObjectivePacket>();
    codec.registerPacket<SetHealthPacket>();
    codec.registerPacket<SetHudPacket>();
    codec.registerPacket<SetLastHurtByPacket>();
    codec.registerPacket<SetLocalPlayerAsInitializedPacket>();
    codec.registerPacket<SetMovementAuthorityPacket>();
    codec.registerPacket<SetPassengerOfBlockPacket>();
    codec.registerPacket<SetPlayerFurnaceOptionsPacket>();
    codec.registerPacket<SetPlayerGameTypePacket>();
    codec.registerPacket<SetPlayerInventoryOptionsPacket>();
    codec.registerPacket<SetScorePacket>();
    codec.registerPacket<SetScoreboardIdentityPacket>();
    codec.registerPacket<SetSpawnPositionPacket>();
    codec.registerPacket<SetTimePacket>();
    codec.registerPacket<SetTitlePacket>();
    codec.registerPacket<SettingsCommandPacket>();
    codec.registerPacket<ShowCreditsPacket>();
    codec.registerPacket<ShowProfilePacket>();
    codec.registerPacket<ShowStoreOfferPacket>();
    codec.registerPacket<SimpleEventPacket>();
    codec.registerPacket<SimulationTypePacket>();
    codec.registerPacket<SpawnExperienceOrbPacket>();
    codec.registerPacket<SpawnParticleEffectPacket>();
    codec.registerPacket<StartGamePacket>();
    codec.registerPacket<StopSoundPacket>();
    codec.registerPacket<StructureBlockUpdatePacket>();
    codec.registerPacket<StructureTemplateDataRequestPacket>();
    codec.registerPacket<StructureTemplateDataResponsePacket>();
    codec.registerPacket<SubChunkPacket>();
    codec.registerPacket<SubChunkRequestPacket>();
    codec.registerPacket<SubClientLoginPacket>();
    codec.registerPacket<SyncActorPropertyPacket>();
    codec.registerPacket<SyncWorldClocksPacket>();
    codec.registerPacket<TakeItemActorPacket>();
    codec.registerPacket<TextPacket>();
    codec.registerPacket<TickSyncPacket>();
    codec.registerPacket<TickingAreasLoadStatusPacket>();
    codec.registerPacket<ToastRequestPacket>();
    codec.registerPacket<TransferPacket>();
    codec.registerPacket<TrimDataPacket>();
    codec.registerPacket<UnlockedRecipesPacket>();
    codec.registerPacket<UpdateAbilitiesPacket>();
    codec.registerPacket<UpdateAdventureSettingsPacket>();
    codec.registerPacket<UpdateAttributesPacket>();
    codec.registerPacket<UpdateBlockPacket>();
    codec.registerPacket<UpdateBlockPropertiesPacket>();
    codec.registerPacket<UpdateBlockSyncedPacket>();
    codec.registerPacket<UpdateClientInputLocksPacket>();
    codec.registerPacket<UpdateClientOptionsPacket>();
    codec.registerPacket<UpdateEquipPacket>();
    codec.registerPacket<UpdatePlayerGameTypePacket>();
    codec.registerPacket<UpdateSoftEnumPacket>();
    codec.registerPacket<UpdateSubChunkBlocksPacket>();
    codec.registerPacket<UpdateTradePacket>();
    codec.registerPacket<LecternUpdatePacket>();
    codec.registerPacket<VoxelShapesPacket>();
}

std::shared_ptr<Packet> MinecraftPackets::createPacket(MinecraftPacketIds id) {
    {
        std::lock_guard<std::mutex> guard(getFactoryMutex());
        auto it = getFactories().find((int) id);
        if (it != getFactories().end())
            return it->second();
    }

    return ProtocolCodecRegistry::instance().getDefault().createPacket(id);
}
