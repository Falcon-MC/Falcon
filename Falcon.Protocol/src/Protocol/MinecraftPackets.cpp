#include "Protocol/MinecraftPackets.h"

#include "Protocol/Packets/AddBehaviorTreePacket.h"
#include "Protocol/Packets/AddActorPacket.h"
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
#include "Protocol/Packets/AwardAchievementPacket.h"
#include "Protocol/Packets/AvailableCommandsPacket.h"
#include "Protocol/Packets/AvailableActorIdentifiersPacket.h"
#include "Protocol/Packets/BiomeDefinitionListPacket.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Packets/BlockEventPacket.h"
#include "Protocol/Packets/BlockPickRequestPacket.h"
#include "Protocol/Packets/BookEditPacket.h"
#include "Protocol/Packets/BossEventPacket.h"
#include "Protocol/Packets/CameraPacket.h"
#include "Protocol/Packets/CameraAimAssistPacket.h"
#include "Protocol/Packets/CameraAimAssistActorPriorityPacket.h"
#include "Protocol/Packets/ClientCameraAimAssistPacket.h"
#include "Protocol/Packets/CameraAimAssistPresetsPacket.h"
#include "Protocol/Packets/CameraInstructionPacket.h"
#include "Protocol/Packets/CameraPresetsPacket.h"
#include "Protocol/Packets/CameraShakePacket.h"
#include "Protocol/Packets/CameraSplinePacket.h"
#include "Protocol/Packets/ChangeDimensionPacket.h"
#include "Protocol/Packets/ChangeMobPropertyPacket.h"
#include "Protocol/Packets/ChunkRadiusUpdatedPacket.h"
#include "Protocol/Packets/ClientCacheBlobStatusPacket.h"
#include "Protocol/Packets/ClientCacheMissResponsePacket.h"
#include "Protocol/Packets/ClientCacheStatusPacket.h"
#include "Protocol/Packets/ClientCheatAbilityPacket.h"
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
#include "Protocol/Packets/ClientboundTextureShiftPacket.h"
#include "Protocol/Packets/ClientboundUpdateSoundDataPacket.h"
#include "Protocol/Packets/CodeBuilderPacket.h"
#include "Protocol/Packets/CommandBlockUpdatePacket.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/CommandRequestPacket.h"
#include "Protocol/Packets/CodeBuilderSourcePacket.h"
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
#include "Protocol/Packets/PrimitiveShapesPacket.h"
#include "Protocol/Packets/DebugInfoPacket.h"
#include "Protocol/Packets/DimensionDataPacket.h"
#include "Protocol/Packets/DisconnectPacket.h"
#include "Protocol/Packets/EditorNetworkPacket.h"
#include "Protocol/Packets/EducationSettingsPacket.h"
#include "Protocol/Packets/EduUriResourcePacket.h"
#include "Protocol/Packets/EmoteListPacket.h"
#include "Protocol/Packets/EmotePacket.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/EntityFallPacket.h"
#include "Protocol/Packets/ActorPickRequestPacket.h"
#include "Protocol/Packets/LegacyTelemetryEventPacket.h"
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
#include "Protocol/Packets/ClientMovementPredictionSyncPacket.h"
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
#include "Protocol/Packets/RecordStartedPacket.h"
#include "Protocol/Packets/PhotoInfoRequestPacket.h"
#include "Protocol/Packets/PhotoTransferPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/PlaySoundPacket.h"
#include "Protocol/Packets/PlayerActionPacket.h"
#include "Protocol/Packets/PlayerListPacket.h"
#include "Protocol/Packets/PlayerArmorDamagePacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Packets/PlayerEnchantOptionsPacket.h"
#include "Protocol/Packets/PlayerFogPacket.h"
#include "Protocol/Packets/PlayerHotbarPacket.h"
#include "Protocol/Packets/PlayerLocationPacket.h"
#include "Protocol/Packets/PlayerSkinPacket.h"
#include "Protocol/Packets/PlayerStartItemCooldownPacket.h"
#include "Protocol/Packets/PlayerUpdateEntityOverridesPacket.h"
#include "Protocol/Packets/PlayerVideoCapturePacket.h"
#include "Protocol/Packets/PositionTrackingDBClientRequestPacket.h"
#include "Protocol/Packets/PositionTrackingDBServerBroadcastPacket.h"
#include "Protocol/Packets/PurchaseReceiptPacket.h"
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
#include "Protocol/Packets/ServerboundDataDrivenScreenClosedPacket.h"
#include "Protocol/Packets/ServerboundDataStorePacket.h"
#include "Protocol/Packets/ServerboundDiagnosticsPacket.h"
#include "Protocol/Packets/ServerboundLoadingScreenPacket.h"
#include "Protocol/Packets/ServerboundPackSettingChangePacket.h"
#include "Protocol/Packets/ServerPlayerPostMovePositionPacket.h"
#include "Protocol/Packets/ServerPresenceInfoPacket.h"
#include "Protocol/Packets/ServerSettingsRequestPacket.h"
#include "Protocol/Packets/ServerSettingsResponsePacket.h"
#include "Protocol/Packets/ServerStatsPacket.h"
#include "Protocol/Packets/ServerStoreInfoPacket.h"
#include "Protocol/Packets/ServerToClientHandshakePacket.h"
#include "Protocol/Packets/SetCommandsEnabledPacket.h"
#include "Protocol/Packets/SetDefaultGameTypePacket.h"
#include "Protocol/Packets/SetDifficultyPacket.h"
#include "Protocol/Packets/SetDisplayObjectivePacket.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Protocol/Packets/SetActorLinkPacket.h"
#include "Protocol/Packets/SetActorMotionPacket.h"
#include "Protocol/Packets/SetHealthPacket.h"
#include "Protocol/Packets/SetHudPacket.h"
#include "Protocol/Packets/SetLastHurtByPacket.h"
#include "Protocol/Packets/SetLocalPlayerAsInitializedPacket.h"
#include "Protocol/Packets/SetMovementAuthorityPacket.h"
#include "Protocol/Packets/SetPlayerGameTypePacket.h"
#include "Protocol/Packets/SetPlayerFurnaceOptionsPacket.h"
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
#include "Protocol/Packets/PlayerToggleCrafterSlotRequestPacket.h"
#include "Protocol/Packets/TransferPacket.h"
#include "Protocol/Packets/TrimDataPacket.h"
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
#include "Protocol/Packets/UnlockedRecipesPacket.h"
#include "Protocol/Packets/VideoStreamConnectPacket.h"
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

    std::once_flag &getRegistrationFlag() {
        static std::once_flag flag;
        return flag;
    }

}

void MinecraftPackets::registerPacket(MinecraftPacketIds id, const PacketFactory &factory) {
    if (!factory)
        return;

    std::lock_guard<std::mutex> guard(getFactoryMutex());
    getFactories()[(int) id] = factory;
}

void MinecraftPackets::_registerDefaultPackets() {
    registerPacket<AddBehaviorTreePacket>();
    registerPacket<AddActorPacket>();
    registerPacket<AddItemActorPacket>();
    registerPacket<AddPaintingPacket>();
    registerPacket<AddPlayerPacket>();
    registerPacket<AddVolumeEntityPacket>();
    registerPacket<AdventureSettingsPacket>();
    registerPacket<AgentActionEventPacket>();
    registerPacket<AgentAnimationPacket>();
    registerPacket<AnimateEntityPacket>();
    registerPacket<AnimatePacket>();
    registerPacket<AnvilDamagePacket>();
    registerPacket<AutomationClientConnectPacket>();
    registerPacket<AwardAchievementPacket>();
    registerPacket<AvailableCommandsPacket>();
    registerPacket<AvailableActorIdentifiersPacket>();
    registerPacket<BiomeDefinitionListPacket>();
    registerPacket<BlockActorDataPacket>();
    registerPacket<BlockEventPacket>();
    registerPacket<BlockPickRequestPacket>();
    registerPacket<BookEditPacket>();
    registerPacket<BossEventPacket>();
    registerPacket<CameraPacket>();
    registerPacket<CameraAimAssistPacket>();
    registerPacket<CameraAimAssistActorPriorityPacket>();
    registerPacket<ClientCameraAimAssistPacket>();
    registerPacket<CameraAimAssistPresetsPacket>();
    registerPacket<CameraInstructionPacket>();
    registerPacket<CameraPresetsPacket>();
    registerPacket<CameraShakePacket>();
    registerPacket<CameraSplinePacket>();
    registerPacket<ChangeDimensionPacket>();
    registerPacket<ChangeMobPropertyPacket>();
    registerPacket<ChunkRadiusUpdatedPacket>();
    registerPacket<ClientCacheBlobStatusPacket>();
    registerPacket<ClientCacheMissResponsePacket>();
    registerPacket<ClientCacheStatusPacket>();
    registerPacket<ClientCheatAbilityPacket>();
    registerPacket<ClientToServerHandshakePacket>();
    registerPacket<ClientboundAttributeLayerSyncPacket>();
    registerPacket<ClientboundCloseFormPacket>();
    registerPacket<ClientboundControlSchemeSetPacket>();
    registerPacket<ClientboundDataDrivenUICloseScreenPacket>();
    registerPacket<ClientboundDataDrivenUIReloadPacket>();
    registerPacket<ClientboundDataDrivenUIShowScreenPacket>();
    registerPacket<ClientboundDataStorePacket>();
    registerPacket<ClientboundDebugRendererPacket>();
    registerPacket<ClientboundMapItemDataPacket>();
    registerPacket<ClientboundTextureShiftPacket>();
    registerPacket<ClientboundUpdateSoundDataPacket>();
    registerPacket<CodeBuilderPacket>();
    registerPacket<CommandBlockUpdatePacket>();
    registerPacket<CommandOutputPacket>();
    registerPacket<CommandRequestPacket>();
    registerPacket<CodeBuilderSourcePacket>();
    registerPacket<CompletedUsingItemPacket>();
    registerPacket<ContainerClosePacket>();
    registerPacket<ContainerOpenPacket>();
    registerPacket<ContainerRegistryCleanupPacket>();
    registerPacket<ContainerSetDataPacket>();
    registerPacket<CorrectPlayerMovePredictionPacket>();
    registerPacket<CraftingDataPacket>();
    registerPacket<CraftingEventPacket>();
    registerPacket<CreatePhotoPacket>();
    registerPacket<CreativeContentPacket>();
    registerPacket<CurrentStructureFeaturePacket>();
    registerPacket<DeathInfoPacket>();
    registerPacket<PrimitiveShapesPacket>();
    registerPacket<DebugInfoPacket>();
    registerPacket<DimensionDataPacket>();
    registerPacket<DisconnectPacket>();
    registerPacket<EditorNetworkPacket>();
    registerPacket<EducationSettingsPacket>();
    registerPacket<EduUriResourcePacket>();
    registerPacket<EmoteListPacket>();
    registerPacket<EmotePacket>();
    registerPacket<ActorEventPacket>();
    registerPacket<EntityFallPacket>();
    registerPacket<ActorPickRequestPacket>();
    registerPacket<LegacyTelemetryEventPacket>();
    registerPacket<FeatureRegistryPacket>();
    registerPacket<FilterTextPacket>();
    registerPacket<GameRulesChangedPacket>();
    registerPacket<GameTestRequestPacket>();
    registerPacket<GameTestResultsPacket>();
    registerPacket<GraphicsOverrideParameterPacket>();
    registerPacket<GuiDataPickItemPacket>();
    registerPacket<HurtArmorPacket>();
    registerPacket<InteractPacket>();
    registerPacket<InventoryContentPacket>();
    registerPacket<InventorySlotPacket>();
    registerPacket<InventoryTransactionPacket>();
    registerPacket<ItemRegistryPacket>();
    registerPacket<ItemStackRequestPacket>();
    registerPacket<ItemStackResponsePacket>();
    registerPacket<JigsawStructureDataPacket>();
    registerPacket<LabTablePacket>();
    registerPacket<LessonProgressPacket>();
    registerPacket<LevelChunkPacket>();
    registerPacket<LevelEventGenericPacket>();
    registerPacket<LevelEventPacket>();
    registerPacket<LevelSoundEventPacket>();
    registerPacket<LocatorBarPacket>();
    registerPacket<LoginPacket>();
    registerPacket<MapCreateLockedCopyPacket>();
    registerPacket<MapInfoRequestPacket>();
    registerPacket<MobArmorEquipmentPacket>();
    registerPacket<MobEffectPacket>();
    registerPacket<MobEquipmentPacket>();
    registerPacket<ModalFormRequestPacket>();
    registerPacket<ModalFormResponsePacket>();
    registerPacket<MotionPredictionHintsPacket>();
    registerPacket<MoveActorAbsolutePacket>();
    registerPacket<MoveActorDeltaPacket>();
    registerPacket<MovePlayerPacket>();
    registerPacket<MovementEffectPacket>();
    registerPacket<ClientMovementPredictionSyncPacket>();
    registerPacket<MultiplayerSettingsPacket>();
    registerPacket<NetworkChunkPublisherUpdatePacket>();
    registerPacket<NetworkSettingsPacket>();
    registerPacket<NetworkStackLatencyPacket>();
    registerPacket<NpcDialoguePacket>();
    registerPacket<NpcRequestPacket>();
    registerPacket<OnScreenTextureAnimationPacket>();
    registerPacket<OpenSignPacket>();
    registerPacket<PacketViolationWarningPacket>();
    registerPacket<PartyChangedPacket>();
    registerPacket<PartyDestinationCookieResponsePacket>();
    registerPacket<RecordStartedPacket>();
    registerPacket<PhotoInfoRequestPacket>();
    registerPacket<PhotoTransferPacket>();
    registerPacket<PlayStatusPacket>();
    registerPacket<PlaySoundPacket>();
    registerPacket<PlayerActionPacket>();
    registerPacket<PlayerListPacket>();
    registerPacket<PlayerArmorDamagePacket>();
    registerPacket<PlayerAuthInputPacket>();
    registerPacket<PlayerEnchantOptionsPacket>();
    registerPacket<PlayerFogPacket>();
    registerPacket<PlayerHotbarPacket>();
    registerPacket<PlayerLocationPacket>();
    registerPacket<PlayerSkinPacket>();
    registerPacket<PlayerStartItemCooldownPacket>();
    registerPacket<PlayerUpdateEntityOverridesPacket>();
    registerPacket<PlayerVideoCapturePacket>();
    registerPacket<PositionTrackingDBClientRequestPacket>();
    registerPacket<PositionTrackingDBServerBroadcastPacket>();
    registerPacket<PurchaseReceiptPacket>();
    registerPacket<RefreshEntitlementsPacket>();
    registerPacket<RemoveActorPacket>();
    registerPacket<RemoveObjectivePacket>();
    registerPacket<RemoveVolumeEntityPacket>();
    registerPacket<RequestAbilityPacket>();
    registerPacket<RequestChunkRadiusPacket>();
    registerPacket<RequestNetworkSettingsPacket>();
    registerPacket<RequestPermissionsPacket>();
    registerPacket<ResourcePackChunkDataPacket>();
    registerPacket<ResourcePackChunkRequestPacket>();
    registerPacket<ResourcePackClientResponsePacket>();
    registerPacket<ResourcePackDataInfoPacket>();
    registerPacket<ResourcePackStackPacket>();
    registerPacket<ResourcePacksInfoPacket>();
    registerPacket<ResourcePacksReadyForValidationPacket>();
    registerPacket<RespawnPacket>();
    registerPacket<ScriptCustomEventPacket>();
    registerPacket<ScriptMessagePacket>();
    registerPacket<SendPartyDestinationCookiePacket>();
    registerPacket<ServerboundDataDrivenScreenClosedPacket>();
    registerPacket<ServerboundDataStorePacket>();
    registerPacket<ServerboundDiagnosticsPacket>();
    registerPacket<ServerboundLoadingScreenPacket>();
    registerPacket<ServerboundPackSettingChangePacket>();
    registerPacket<ServerPlayerPostMovePositionPacket>();
    registerPacket<ServerPresenceInfoPacket>();
    registerPacket<ServerSettingsRequestPacket>();
    registerPacket<ServerSettingsResponsePacket>();
    registerPacket<ServerStatsPacket>();
    registerPacket<ServerStoreInfoPacket>();
    registerPacket<ServerToClientHandshakePacket>();
    registerPacket<SetCommandsEnabledPacket>();
    registerPacket<SetDefaultGameTypePacket>();
    registerPacket<SetDifficultyPacket>();
    registerPacket<SetDisplayObjectivePacket>();
    registerPacket<SetActorDataPacket>();
    registerPacket<SetActorLinkPacket>();
    registerPacket<SetActorMotionPacket>();
    registerPacket<SetHealthPacket>();
    registerPacket<SetHudPacket>();
    registerPacket<SetLastHurtByPacket>();
    registerPacket<SetLocalPlayerAsInitializedPacket>();
    registerPacket<SetMovementAuthorityPacket>();
    registerPacket<SetPlayerGameTypePacket>();
    registerPacket<SetPlayerFurnaceOptionsPacket>();
    registerPacket<SetPlayerInventoryOptionsPacket>();
    registerPacket<SetScorePacket>();
    registerPacket<SetScoreboardIdentityPacket>();
    registerPacket<SetSpawnPositionPacket>();
    registerPacket<SetTimePacket>();
    registerPacket<SetTitlePacket>();
    registerPacket<SettingsCommandPacket>();
    registerPacket<ShowCreditsPacket>();
    registerPacket<ShowProfilePacket>();
    registerPacket<ShowStoreOfferPacket>();
    registerPacket<SimpleEventPacket>();
    registerPacket<SimulationTypePacket>();
    registerPacket<SpawnExperienceOrbPacket>();
    registerPacket<SpawnParticleEffectPacket>();
    registerPacket<StartGamePacket>();
    registerPacket<StopSoundPacket>();
    registerPacket<StructureBlockUpdatePacket>();
    registerPacket<StructureTemplateDataRequestPacket>();
    registerPacket<StructureTemplateDataResponsePacket>();
    registerPacket<SubChunkPacket>();
    registerPacket<SubChunkRequestPacket>();
    registerPacket<SubClientLoginPacket>();
    registerPacket<SyncActorPropertyPacket>();
    registerPacket<SyncWorldClocksPacket>();
    registerPacket<TakeItemActorPacket>();
    registerPacket<TextPacket>();
    registerPacket<TickSyncPacket>();
    registerPacket<TickingAreasLoadStatusPacket>();
    registerPacket<ToastRequestPacket>();
    registerPacket<PlayerToggleCrafterSlotRequestPacket>();
    registerPacket<TransferPacket>();
    registerPacket<TrimDataPacket>();
    registerPacket<UpdateAbilitiesPacket>();
    registerPacket<UpdateAdventureSettingsPacket>();
    registerPacket<UpdateAttributesPacket>();
    registerPacket<UpdateBlockPacket>();
    registerPacket<UpdateBlockPropertiesPacket>();
    registerPacket<UpdateBlockSyncedPacket>();
    registerPacket<UpdateClientInputLocksPacket>();
    registerPacket<UpdateClientOptionsPacket>();
    registerPacket<UpdateEquipPacket>();
    registerPacket<UpdatePlayerGameTypePacket>();
    registerPacket<UpdateSoftEnumPacket>();
    registerPacket<UpdateSubChunkBlocksPacket>();
    registerPacket<UpdateTradePacket>();
    registerPacket<UnlockedRecipesPacket>();
    registerPacket<VideoStreamConnectPacket>();
    registerPacket<VoxelShapesPacket>();
}

std::shared_ptr<Packet> MinecraftPackets::createPacket(MinecraftPacketIds id) {
    std::call_once(getRegistrationFlag(), &MinecraftPackets::_registerDefaultPackets);

    std::lock_guard<std::mutex> guard(getFactoryMutex());

    auto it = getFactories().find((int) id);
    if (it == getFactories().end())
        return nullptr;

    return it->second();
}
