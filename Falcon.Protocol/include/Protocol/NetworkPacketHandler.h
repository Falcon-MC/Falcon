#pragma once

class NetworkIdentifier;

class AddBehaviorTreePacket;
class AddActorPacket;
class AddItemActorPacket;
class AddPaintingPacket;
class AddPlayerPacket;
class AddVolumeEntityPacket;
class AdventureSettingsPacket;
class AgentActionEventPacket;
class AgentAnimationPacket;
class AnimateEntityPacket;
class AnimatePacket;
class AnvilDamagePacket;
class AutomationClientConnectPacket;
class AwardAchievementPacket;
class AvailableCommandsPacket;
class AvailableActorIdentifiersPacket;
class BiomeDefinitionListPacket;
class BlockActorDataPacket;
class BlockEventPacket;
class BlockPickRequestPacket;
class BookEditPacket;
class BossEventPacket;
class CameraPacket;
class CameraAimAssistPacket;
class CameraAimAssistActorPriorityPacket;
class ClientCameraAimAssistPacket;
class CameraAimAssistPresetsPacket;
class CameraInstructionPacket;
class CameraPresetsPacket;
class CameraShakePacket;
class CameraSplinePacket;
class ChangeDimensionPacket;
class ChangeMobPropertyPacket;
class ChunkRadiusUpdatedPacket;
class ClientCacheBlobStatusPacket;
class ClientCacheMissResponsePacket;
class ClientCacheStatusPacket;
class ClientCheatAbilityPacket;
class ClientToServerHandshakePacket;
class ClientboundAttributeLayerSyncPacket;
class ClientboundCloseFormPacket;
class ClientboundControlSchemeSetPacket;
class ClientboundDataDrivenUICloseScreenPacket;
class ClientboundDataDrivenUIReloadPacket;
class ClientboundDataDrivenUIShowScreenPacket;
class ClientboundDataStorePacket;
class ClientboundDebugRendererPacket;
class ClientboundMapItemDataPacket;
class ClientboundTextureShiftPacket;
class ClientboundUpdateSoundDataPacket;
class CodeBuilderPacket;
class CommandBlockUpdatePacket;
class CommandOutputPacket;
class CommandRequestPacket;
class CodeBuilderSourcePacket;
class CompletedUsingItemPacket;
class ContainerClosePacket;
class ContainerOpenPacket;
class ContainerRegistryCleanupPacket;
class ContainerSetDataPacket;
class CorrectPlayerMovePredictionPacket;
class CraftingDataPacket;
class CraftingEventPacket;
class CreatePhotoPacket;
class CreativeContentPacket;
class CurrentStructureFeaturePacket;
class DeathInfoPacket;
class PrimitiveShapesPacket;
class DebugInfoPacket;
class DimensionDataPacket;
class DisconnectPacket;
class EditorNetworkPacket;
class EducationSettingsPacket;
class EduUriResourcePacket;
class EmoteListPacket;
class EmotePacket;
class ActorEventPacket;
class EntityFallPacket;
class ActorPickRequestPacket;
class LegacyTelemetryEventPacket;
class FeatureRegistryPacket;
class FilterTextPacket;
class GameRulesChangedPacket;
class GameTestRequestPacket;
class GameTestResultsPacket;
class GraphicsOverrideParameterPacket;
class GuiDataPickItemPacket;
class HurtArmorPacket;
class InteractPacket;
class InventoryContentPacket;
class InventorySlotPacket;
class InventoryTransactionPacket;
class ItemRegistryPacket;
class ItemStackRequestPacket;
class ItemStackResponsePacket;
class JigsawStructureDataPacket;
class LabTablePacket;
class LessonProgressPacket;
class LevelChunkPacket;
class LevelEventPacket;
class LevelSoundEventPacket;
class LocatorBarPacket;
class LoginPacket;
class MapCreateLockedCopyPacket;
class MapInfoRequestPacket;
class MobArmorEquipmentPacket;
class MobEffectPacket;
class MobEquipmentPacket;
class ModalFormRequestPacket;
class ModalFormResponsePacket;
class MotionPredictionHintsPacket;
class MoveActorAbsolutePacket;
class MoveActorDeltaPacket;
class MovePlayerPacket;
class MovementEffectPacket;
class ClientMovementPredictionSyncPacket;
class MultiplayerSettingsPacket;
class NetworkChunkPublisherUpdatePacket;
class NetworkSettingsPacket;
class NetworkStackLatencyPacket;
class NpcDialoguePacket;
class NpcRequestPacket;
class OnScreenTextureAnimationPacket;
class OpenSignPacket;
class PacketViolationWarningPacket;
class PartyChangedPacket;
class PartyDestinationCookieResponsePacket;

class RecordStartedPacket;
class PhotoInfoRequestPacket;
class PhotoTransferPacket;
class PlayStatusPacket;
class PlaySoundPacket;
class PlayerActionPacket;
class PlayerListPacket;
class PlayerArmorDamagePacket;
class PlayerAuthInputPacket;
class PlayerEnchantOptionsPacket;
class PlayerFogPacket;
class PlayerHotbarPacket;
class PlayerLocationPacket;
class PlayerSkinPacket;
class PlayerStartItemCooldownPacket;
class PlayerUpdateEntityOverridesPacket;
class PlayerVideoCapturePacket;
class PositionTrackingDBClientRequestPacket;
class PositionTrackingDBServerBroadcastPacket;
class PurchaseReceiptPacket;
class RefreshEntitlementsPacket;
class RemoveActorPacket;
class RemoveObjectivePacket;
class RemoveVolumeEntityPacket;
class RequestAbilityPacket;
class RequestChunkRadiusPacket;
class RequestNetworkSettingsPacket;
class RequestPermissionsPacket;
class ResourcePackChunkDataPacket;
class ResourcePackChunkRequestPacket;
class ResourcePackClientResponsePacket;
class ResourcePackDataInfoPacket;
class ResourcePackStackPacket;
class ResourcePacksInfoPacket;
class ResourcePacksReadyForValidationPacket;
class RespawnPacket;
class ScriptCustomEventPacket;
class ScriptMessagePacket;
class SendPartyDestinationCookiePacket;
class ServerboundDataDrivenScreenClosedPacket;
class ServerboundDataStorePacket;
class ServerboundDiagnosticsPacket;
class ServerboundLoadingScreenPacket;
class ServerboundPackSettingChangePacket;
class ServerPlayerPostMovePositionPacket;
class ServerPresenceInfoPacket;
class ServerSettingsRequestPacket;
class ServerSettingsResponsePacket;
class ServerStatsPacket;
class ServerStoreInfoPacket;
class ServerToClientHandshakePacket;
class SetCommandsEnabledPacket;
class SetDefaultGameTypePacket;
class SetDifficultyPacket;
class SetDisplayObjectivePacket;
class SetActorDataPacket;
class SetActorLinkPacket;
class SetActorMotionPacket;
class SetHealthPacket;
class SetHudPacket;
class SetLastHurtByPacket;
class SetLocalPlayerAsInitializedPacket;
class SetPlayerGameTypePacket;
class SetPlayerFurnaceOptionsPacket;

class SetPlayerInventoryOptionsPacket;
class SetScorePacket;
class SetScoreboardIdentityPacket;
class SetSpawnPositionPacket;
class SetTimePacket;
class SetTitlePacket;
class SettingsCommandPacket;
class ShowCreditsPacket;
class ShowProfilePacket;
class ShowStoreOfferPacket;
class SimpleEventPacket;
class SimulationTypePacket;
class SpawnExperienceOrbPacket;
class SpawnParticleEffectPacket;
class StartGamePacket;
class StopSoundPacket;
class StructureBlockUpdatePacket;
class StructureTemplateDataRequestPacket;
class StructureTemplateDataResponsePacket;
class SubChunkPacket;
class SubChunkRequestPacket;
class SubClientLoginPacket;
class SyncActorPropertyPacket;
class SyncWorldClocksPacket;
class TakeItemActorPacket;
class TextPacket;
class TickSyncPacket;
class TickingAreasLoadStatusPacket;
class ToastRequestPacket;
class PlayerToggleCrafterSlotRequestPacket;
class TransferPacket;
class TrimDataPacket;
class UpdateAbilitiesPacket;
class UpdateAdventureSettingsPacket;
class UpdateAttributesPacket;
class UpdateBlockPacket;
class UpdateBlockPropertiesPacket;
class UpdateBlockSyncedPacket;
class UpdateClientInputLocksPacket;
class UpdateClientOptionsPacket;
class UpdateEquipPacket;
class UpdatePlayerGameTypePacket;
class UpdateSoftEnumPacket;
class UpdateSubChunkBlocksPacket;
class UpdateTradePacket;
class UnlockedRecipesPacket;
class VideoStreamConnectPacket;
class VoxelShapesPacket;

class NetworkPacketHandler {
public:
    virtual ~NetworkPacketHandler() = default;

    virtual void handle(const NetworkIdentifier &, const AddBehaviorTreePacket &) {}

    virtual void handle(const NetworkIdentifier &, const AddActorPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AddItemActorPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AddPaintingPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AddPlayerPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AddVolumeEntityPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AdventureSettingsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AgentActionEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AgentAnimationPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AnimateEntityPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AnimatePacket &) {}

    virtual void handle(const NetworkIdentifier &, const AnvilDamagePacket &) {}

    virtual void handle(const NetworkIdentifier &, const AutomationClientConnectPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AwardAchievementPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AvailableCommandsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const AvailableActorIdentifiersPacket &) {}

    virtual void handle(const NetworkIdentifier &, const BiomeDefinitionListPacket &) {}

    virtual void handle(const NetworkIdentifier &, const BlockActorDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const BlockEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const BlockPickRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const BookEditPacket &) {}

    virtual void handle(const NetworkIdentifier &, const BossEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraAimAssistPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraAimAssistActorPriorityPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientCameraAimAssistPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraAimAssistPresetsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraInstructionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraPresetsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraShakePacket &) {}

    virtual void handle(const NetworkIdentifier &, const CameraSplinePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ChangeDimensionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ChangeMobPropertyPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ChunkRadiusUpdatedPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientCacheBlobStatusPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientCacheMissResponsePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientCacheStatusPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientCheatAbilityPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientToServerHandshakePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundAttributeLayerSyncPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundCloseFormPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundControlSchemeSetPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundDataDrivenUICloseScreenPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundDataDrivenUIReloadPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundDataDrivenUIShowScreenPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundDataStorePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundDebugRendererPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundMapItemDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundTextureShiftPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientboundUpdateSoundDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CodeBuilderPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CommandBlockUpdatePacket &) {}

    virtual void handle(const NetworkIdentifier &, const CommandOutputPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CommandRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CodeBuilderSourcePacket &) {}

    virtual void handle(const NetworkIdentifier &, const CompletedUsingItemPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ContainerClosePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ContainerOpenPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ContainerRegistryCleanupPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ContainerSetDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CorrectPlayerMovePredictionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CraftingDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CraftingEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CreatePhotoPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CreativeContentPacket &) {}

    virtual void handle(const NetworkIdentifier &, const CurrentStructureFeaturePacket &) {}

    virtual void handle(const NetworkIdentifier &, const DeathInfoPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PrimitiveShapesPacket &) {}

    virtual void handle(const NetworkIdentifier &, const DebugInfoPacket &) {}

    virtual void handle(const NetworkIdentifier &, const DimensionDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const DisconnectPacket &) {}

    virtual void handle(const NetworkIdentifier &, const EditorNetworkPacket &) {}

    virtual void handle(const NetworkIdentifier &, const EducationSettingsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const EduUriResourcePacket &) {}

    virtual void handle(const NetworkIdentifier &, const EmoteListPacket &) {}

    virtual void handle(const NetworkIdentifier &, const EmotePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ActorEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const EntityFallPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ActorPickRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const LegacyTelemetryEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const FeatureRegistryPacket &) {}

    virtual void handle(const NetworkIdentifier &, const FilterTextPacket &) {}

    virtual void handle(const NetworkIdentifier &, const GameRulesChangedPacket &) {}

    virtual void handle(const NetworkIdentifier &, const GameTestRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const GameTestResultsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const GraphicsOverrideParameterPacket &) {}

    virtual void handle(const NetworkIdentifier &, const GuiDataPickItemPacket &) {}

    virtual void handle(const NetworkIdentifier &, const HurtArmorPacket &) {}

    virtual void handle(const NetworkIdentifier &, const InteractPacket &) {}

    virtual void handle(const NetworkIdentifier &, const InventoryContentPacket &) {}

    virtual void handle(const NetworkIdentifier &, const InventorySlotPacket &) {}

    virtual void handle(const NetworkIdentifier &, const InventoryTransactionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ItemRegistryPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ItemStackRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ItemStackResponsePacket &) {}

    virtual void handle(const NetworkIdentifier &, const JigsawStructureDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const LabTablePacket &) {}

    virtual void handle(const NetworkIdentifier &, const LessonProgressPacket &) {}

    virtual void handle(const NetworkIdentifier &, const LevelChunkPacket &) {}

    virtual void handle(const NetworkIdentifier &, const LevelEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const LevelSoundEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const LocatorBarPacket &) {}

    virtual void handle(const NetworkIdentifier &, const LoginPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MapCreateLockedCopyPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MapInfoRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MobArmorEquipmentPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MobEffectPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MobEquipmentPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ModalFormRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ModalFormResponsePacket &) {}

    virtual void handle(const NetworkIdentifier &, const MotionPredictionHintsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MoveActorAbsolutePacket &) {}

    virtual void handle(const NetworkIdentifier &, const MoveActorDeltaPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MovePlayerPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MovementEffectPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ClientMovementPredictionSyncPacket &) {}

    virtual void handle(const NetworkIdentifier &, const MultiplayerSettingsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const NetworkChunkPublisherUpdatePacket &) {}

    virtual void handle(const NetworkIdentifier &, const NetworkSettingsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const NetworkStackLatencyPacket &) {}

    virtual void handle(const NetworkIdentifier &, const NpcDialoguePacket &) {}

    virtual void handle(const NetworkIdentifier &, const NpcRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const OnScreenTextureAnimationPacket &) {}

    virtual void handle(const NetworkIdentifier &, const OpenSignPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PacketViolationWarningPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PartyChangedPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PartyDestinationCookieResponsePacket &) {}

    virtual void handle(const NetworkIdentifier &, const RecordStartedPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PhotoInfoRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PhotoTransferPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayStatusPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlaySoundPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerActionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerListPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerArmorDamagePacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerAuthInputPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerEnchantOptionsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerFogPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerHotbarPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerLocationPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerSkinPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerStartItemCooldownPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerUpdateEntityOverridesPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerVideoCapturePacket &) {}

    virtual void handle(const NetworkIdentifier &, const PositionTrackingDBClientRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PositionTrackingDBServerBroadcastPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PurchaseReceiptPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RefreshEntitlementsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RemoveActorPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RemoveObjectivePacket &) {}

    virtual void handle(const NetworkIdentifier &, const RemoveVolumeEntityPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RequestAbilityPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RequestChunkRadiusPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RequestNetworkSettingsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RequestPermissionsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ResourcePackChunkDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ResourcePackChunkRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ResourcePackClientResponsePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ResourcePackDataInfoPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ResourcePackStackPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ResourcePacksInfoPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ResourcePacksReadyForValidationPacket &) {}

    virtual void handle(const NetworkIdentifier &, const RespawnPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ScriptCustomEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ScriptMessagePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SendPartyDestinationCookiePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerboundDataDrivenScreenClosedPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerboundDataStorePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerboundDiagnosticsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerboundLoadingScreenPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerboundPackSettingChangePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerPlayerPostMovePositionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerPresenceInfoPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerSettingsRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerSettingsResponsePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerStatsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerStoreInfoPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ServerToClientHandshakePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetCommandsEnabledPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetDefaultGameTypePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetDifficultyPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetDisplayObjectivePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetActorDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetActorLinkPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetActorMotionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetHealthPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetHudPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetLastHurtByPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetLocalPlayerAsInitializedPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetPlayerGameTypePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetPlayerFurnaceOptionsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetPlayerInventoryOptionsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetScorePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetScoreboardIdentityPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetSpawnPositionPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetTimePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SetTitlePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SettingsCommandPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ShowCreditsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ShowProfilePacket &) {}

    virtual void handle(const NetworkIdentifier &, const ShowStoreOfferPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SimpleEventPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SimulationTypePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SpawnExperienceOrbPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SpawnParticleEffectPacket &) {}

    virtual void handle(const NetworkIdentifier &, const StartGamePacket &) {}

    virtual void handle(const NetworkIdentifier &, const StopSoundPacket &) {}

    virtual void handle(const NetworkIdentifier &, const StructureBlockUpdatePacket &) {}

    virtual void handle(const NetworkIdentifier &, const StructureTemplateDataRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const StructureTemplateDataResponsePacket &) {}

    virtual void handle(const NetworkIdentifier &, const SubChunkPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SubChunkRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SubClientLoginPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SyncActorPropertyPacket &) {}

    virtual void handle(const NetworkIdentifier &, const SyncWorldClocksPacket &) {}

    virtual void handle(const NetworkIdentifier &, const TakeItemActorPacket &) {}

    virtual void handle(const NetworkIdentifier &, const TextPacket &) {}

    virtual void handle(const NetworkIdentifier &, const TickSyncPacket &) {}

    virtual void handle(const NetworkIdentifier &, const TickingAreasLoadStatusPacket &) {}

    virtual void handle(const NetworkIdentifier &, const ToastRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const PlayerToggleCrafterSlotRequestPacket &) {}

    virtual void handle(const NetworkIdentifier &, const TransferPacket &) {}

    virtual void handle(const NetworkIdentifier &, const TrimDataPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateAbilitiesPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateAdventureSettingsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateAttributesPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateBlockPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateBlockPropertiesPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateBlockSyncedPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateClientInputLocksPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateClientOptionsPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateEquipPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdatePlayerGameTypePacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateSoftEnumPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateSubChunkBlocksPacket &) {}

    virtual void handle(const NetworkIdentifier &, const UpdateTradePacket &) {}

    virtual void handle(const NetworkIdentifier &, const UnlockedRecipesPacket &) {}

    virtual void handle(const NetworkIdentifier &, const VideoStreamConnectPacket &) {}

    virtual void handle(const NetworkIdentifier &, const VoxelShapesPacket &) {}

};
