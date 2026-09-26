#include "Network/Handler/LoginHandler.h"

#include "Scripting/Content/CustomContentRegistry.h"

#include "Block/Components/CreativeContentTable.h"
#include "Block/DataDrivenBlockDefinitions.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/VoxelShapeRegistry.h"
#include "Command/Command.h"
#include "Core/Debug/BedrockLog.h"
#include "Core/NBT/NbtIo.h"
#include "Core/Utility/BinaryStream.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Actor/ActorAttributes.h"
#include "Actor/Definition/EntityDefinitions.h"
#include "Actor/VanillaActorTable.h"
#include "Actor/PlayerAbility.h"
#include "Actor/ServerPlayer.h"
#include "Item/CraftingRecipeTable.h"
#include "Item/ItemNetworkIdTable.h"
#include "Level/Level.h"
#include "Network/ConnectionRequest.h"
#include "Network/Crypto/EncryptionHandshake.h"
#include "Network/Crypto/KeyPair.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/LoginChainVerifier.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Packets/AvailableCommandsPacket.h"
#include "Protocol/Packets/AvailableActorIdentifiersPacket.h"
#include "Protocol/Packets/BiomeDefinitionListPacket.h"
#include "Protocol/Packets/CreativeContentPacket.h"
#include "Protocol/Packets/CameraPresetsPacket.h"
#include "Protocol/Types/CameraPresets.h"
#include "Protocol/Packets/ItemRegistryPacket.h"
#include "Protocol/Packets/JigsawStructureDataPacket.h"
#include "Protocol/Packets/LoginPacket.h"
#include "Protocol/Packets/PlayerListPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Packets/RequestNetworkSettingsPacket.h"
#include "Protocol/Packets/ResourcePackChunkDataPacket.h"
#include "Protocol/Packets/ResourcePackChunkRequestPacket.h"
#include "Protocol/Packets/ResourcePackClientResponsePacket.h"
#include "Protocol/Packets/ResourcePackDataInfoPacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ResourcePacksInfoPacket.h"
#include "Protocol/Packets/ServerToClientHandshakePacket.h"
#include "Protocol/Packets/SyncActorPropertyPacket.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Protocol/Packets/SetTimePacket.h"
#include "Protocol/Packets/UpdateAbilitiesPacket.h"
#include "Protocol/Packets/UpdateAttributesPacket.h"
#include "Level/BonusChest.h"
#include "Server/Localization.h"
#include "Server/PropertiesSettings.h"
#include "Server/ResourcePackManager.h"

#include <cstring>
#include <unordered_map>
#include <vector>

namespace {
    const int64_t RESOURCE_PACK_CHUNK_SIZE = 1024 * 1024;
    const float PLAYER_BASE_OFFSET = 1.62f;
    const size_t SPAWN_CHUNK_THRESHOLD = 56;

    Uuid listUuidFor(const ServerPlayer &player) {
        Uuid parsed = Uuid::fromString(player.getUuid());
        if (parsed.mostSignificantBits != 0 || parsed.leastSignificantBits != 0)
            return parsed;

        const std::hash<std::string> hasher;
        uint64_t most = hasher(player.getName() + "-most");
        const uint64_t least = hasher(player.getName() + "-least");
        if (most == 0)
            most = 1;

        return Uuid(most, least);
    }

    std::string packRecipeId(uint32_t value) {
        std::string result(4, '\0');
        result[0] = (char) (value >> 24);
        result[1] = (char) (value >> 16);
        result[2] = (char) (value >> 8);
        result[3] = (char) value;
        return result;
    }

    void sendPropertySchema(ServerNetworkHandler &owner, const ServerPlayer &player, const std::string &identifier,
                            const std::vector<ActorPropertyDescription> &schema) {
        if (schema.empty())
            return;

        Tag properties = Tag::ofList(Tag::Type::Compound);
        for (const ActorPropertyDescription &property: schema) {
            Tag entry = Tag::ofCompound();
            entry.putString("name", property.mName);

            switch (property.mType) {
                case ActorPropertyDescription::Type::Float:
                    entry.putInt("type", 1);
                    entry.putFloat("min", property.mMinFloat);
                    entry.putFloat("max", property.mMaxFloat);
                    break;
                case ActorPropertyDescription::Type::Bool:
                    entry.putInt("type", 2);
                    break;
                case ActorPropertyDescription::Type::Enum: {
                    entry.putInt("type", 3);
                    Tag values = Tag::ofList(Tag::Type::String);
                    for (const std::string &value: property.mEnumValues)
                        values.addToList(Tag::ofString(value));
                    entry.put("enum", values);
                    break;
                }
                default:
                    entry.putInt("type", 0);
                    entry.putInt("min", property.mMinInt);
                    entry.putInt("max", property.mMaxInt);
                    break;
            }

            properties.addToList(entry);
        }

        Tag data = Tag::ofCompound();
        data.putString("type", identifier);
        data.put("properties", properties);

        SyncActorPropertyPacket packet;
        packet.mData = data;
        owner.getNetworkHandler().send(player.getNetworkIdentifier(), packet, owner.getCodecContext());
    }
}

void LoginHandler::registerVanillaDefinitions(ServerNetworkHandler &owner) {
    for (const std::unique_ptr<Block> &block: VanillaBlocks::getAll()) {
        owner.getBlockDefinitions().registerDefinition(std::make_shared<BlockDefinition>(
                block->getIdentifier(), block->getNetworkHash(), block->getStates()));
    }

    for (size_t index = 0; index < ItemNetworkIdTable::getCount(); ++index) {
        const ItemNetworkIdEntry &entry = ItemNetworkIdTable::getEntries()[index];

        owner.getItemDefinitions().registerDefinition(std::make_shared<ItemDefinition>(
                entry.mIdentifier, entry.mNetworkId, entry.mComponentBased, entry.mComponents));
    }

    LOG_TRACE(LogAreaID::Server, "Registered %zu block(s) and %zu item definition(s)",
             VanillaBlocks::getAll().size(), owner.getItemDefinitions().size());

}

void LoginHandler::handleRequestNetworkSettings(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                                const RequestNetworkSettingsPacket &packet) {
    if (packet.mProtocolVersion != owner.getAnnouncement().mProtocolVersion) {
        PlayStatusPacket status;
        status.mStatus = packet.mProtocolVersion < owner.getAnnouncement().mProtocolVersion
                         ? PlayStatusPacket::Status::LoginFailedClientOld
                         : PlayStatusPacket::Status::LoginFailedServerOld;

        owner.getNetworkHandler().send(id, status, owner.getCodecContext());
        owner.getNetworkHandler().flush(id);

        LOG_WARN(LogAreaID::Network, "%s uses protocol %d but the server runs %d", id.getAddress().c_str(),
                 packet.mProtocolVersion, owner.getAnnouncement().mProtocolVersion);
        return;
    }

    NetworkSettingsPacket settings;
    settings.mCompressionThreshold = owner.getProperties().getCompressionThreshold();
    settings.mCompressionAlgorithm = owner.getProperties().getCompressionAlgorithm();
    settings.mClientThrottleEnabled = false;
    settings.mClientThrottleThreshold = 0;
    settings.mClientThrottleScalar = 0.0f;

    owner.getNetworkHandler().send(id, settings, owner.getCodecContext());

    owner.getNetworkHandler().flush(id);
    owner.getNetworkHandler().enableCompression(id, CompressedNetworkPeer::CompressionAlgorithm::ZLib,
                                                settings.mCompressionThreshold);

    owner.getPlayers().erase(id);
    auto inserted = owner.getPlayers().try_emplace(id, id, owner.allocateRuntimeId(), &owner);
    ServerPlayer &player = inserted.first->second;
    player.setLoginState(ServerPlayer::LoginState::NetworkSettingsSent);
}

void LoginHandler::handleLogin(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                               const LoginPacket &packet) {
    ConnectionRequest request;
    if (!request.parse(packet.mAuthJwt, packet.mClientJwt)) {
        LOG_WARN(LogAreaID::Network, "%s sent a login that could not be parsed", id.getAddress().c_str());
        owner._disconnect(id, "disconnectionScreen.unexpectedPacket");
        return;
    }

    LoginChainVerifier verifier;
    const bool verified = verifier.verify(packet.mAuthJwt, packet.mClientJwt);

    if (!verified) {
        LOG_WARN(LogAreaID::Network, "%s sent a login that failed verification: %s", id.getAddress().c_str(),
                 verifier.getFailureReason().c_str());
        owner._disconnect(id, "disconnectionScreen.notAuthenticated");
        owner.getPlayers().erase(id);
        return;
    }

    if (owner.getProperties().getOnlineMode() && !verifier.isSigned()) {
        LOG_WARN(LogAreaID::Network, "%s failed Xbox Live authentication", id.getAddress().c_str());
        owner._disconnect(id, "disconnectionScreen.notAuthenticated");
        owner.getPlayers().erase(id);
        return;
    }

    if (verifier.isSigned()) {
        player.setName(verifier.getDisplayName());
        player.setUuid(verifier.getIdentity());
        player.setXuid(verifier.getXuid());
    } else {
        const std::string &name = verifier.getDisplayName().empty() ? request.getDisplayName()
                                                                    : verifier.getDisplayName();
        const std::string &uuid = verifier.getIdentity().empty() ? request.getIdentity() : verifier.getIdentity();

        player.setName(name);
        player.setUuid(uuid);
        player.setXuid("");
    }

    const BanEntry *ban = owner.getBanList().find(player.getName());
    if (ban == nullptr)
        ban = owner.getIpBanList().find(id.getAddress());
    if (ban != nullptr) {
        LOG_INFO(LogAreaID::Server, "Player %s is banned", player.getName().c_str());
        const Localization &localization = Localization::getInstance();
        owner._disconnect(id, ban->mReason.empty()
                              ? localization.translate(request.getLanguageCode(), "falcon.disconnect.banned")
                              : localization.translate(request.getLanguageCode(), "falcon.disconnect.bannedReason",
                                                       {ban->mReason}));
        owner.getPlayers().erase(id);
        return;
    }

    if (!owner.isAllowListed(player)) {
        LOG_INFO(LogAreaID::Server, "Player %s is not in the allow list", player.getName().c_str());
        owner._disconnect(id, ServerNetworkHandler::NOT_ALLOW_LISTED_MESSAGE);
        owner.getPlayers().erase(id);
        return;
    }

    if (owner.isServerFull(id) && !owner.getAllowList().ignoresPlayerLimit(player.getName(), player.getXuid())) {
        LOG_INFO(LogAreaID::Server, "Player %s was refused because the server is full", player.getName().c_str());
        owner._disconnect(id, "disconnectionScreen.serverFull");
        owner.getPlayers().erase(id);
        return;
    }

    player.setSkin(request.getSkin());
    player.setBuildPlatform(request.getBuildPlatform());
    player.setLocale(request.getLanguageCode());

    if (owner.getProperties().getNetworkEncryption()) {
        startEncryption(owner, id, player, verifier.getIdentityPublicKey());
        return;
    }

    completeLogin(owner, id, player);
}

void LoginHandler::startEncryption(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player,
                                   const std::string &clientPublicKey) {
    const std::shared_ptr<KeyPair> serverKey = KeyPair::generate();
    ServerToClientHandshakePacket handshake;
    EncryptionKey key{};
    if (serverKey == nullptr
        || !EncryptionHandshake::createServerToken(*serverKey, clientPublicKey, handshake.mJwt, key)) {
        LOG_WARN(LogAreaID::Network, "Could not prepare the encryption handshake for %s", id.getAddress().c_str());
        owner._disconnect(id, "disconnectionScreen.notAuthenticated");
        owner.getPlayers().erase(id);
        return;
    }

    owner.getNetworkHandler().send(id, handshake, owner.getCodecContext());
    owner.getNetworkHandler().flush(id);
    owner.getNetworkHandler().enableEncryption(id, key);
    player.setLoginState(ServerPlayer::LoginState::EncryptionHandshake);
}

void LoginHandler::handleClientToServerHandshake(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                                 ServerPlayer &player) {
    if (player.getLoginState() != ServerPlayer::LoginState::EncryptionHandshake) {
        owner._disconnect(id, "disconnectionScreen.unexpectedPacket");
        return;
    }

    completeLogin(owner, id, player);
}

void LoginHandler::completeLogin(ServerNetworkHandler &owner, const NetworkIdentifier &id, ServerPlayer &player) {
    player.setLoginState(ServerPlayer::LoginState::LoggedIn);

    LOG_INFO(LogAreaID::Server, "Player %s logged in, uuid %s, xuid %s", player.getName().c_str(),
             player.getUuid().c_str(), player.getXuid().c_str());

    PlayStatusPacket status;
    status.mStatus = PlayStatusPacket::Status::LoginSuccess;
    owner.getNetworkHandler().send(id, status, owner.getCodecContext());

    ResourcePacksInfoPacket packs;
    packs.mForcedToAccept = owner.getProperties().getTexturePackRequired();
    packs.mWorldTemplateVersion = "";
    packs.mHasAddonPacks = false;
    packs.mScriptingEnabled = false;
    packs.mVibrantVisualsForceDisabled = false;

    for (const ResourcePack &pack: owner.getResourcePacks().getPacks()) {
        ResourcePacksInfoPacket::Entry entry;
        entry.mPackId = pack.mUuid;
        entry.mPackVersion = pack.mVersion;
        entry.mPackSize = pack.mSize;
        entry.mContentKey = pack.mContentKey;
        entry.mContentId = pack.mUuidString;
        entry.mCdnUrl = pack.mCdnUrl;
        packs.mResourcePackInfos.push_back(entry);
    }

    owner.getNetworkHandler().send(id, packs, owner.getCodecContext());

    player.setLoginState(ServerPlayer::LoginState::ResourcePacksSent);
}

void LoginHandler::handleResourcePackClientResponse(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                                    ServerPlayer &player,
                                                    const ResourcePackClientResponsePacket &packet) {
    switch (packet.mStatus) {
        case ResourcePackClientResponsePacket::Status::SendPacks: {
            for (const std::string &requested: packet.mPackIds) {
                const std::string uuid = requested.substr(0, requested.find('_'));
                const ResourcePack *pack = owner.getResourcePacks().findById(uuid);
                if (pack == nullptr)
                    continue;

                ResourcePackDataInfoPacket info;
                info.mPackId = pack->mUuid;
                info.mPackVersion = pack->mVersion;
                info.mMaxChunkSize = RESOURCE_PACK_CHUNK_SIZE;
                info.mChunkCount = (int64_t) ((pack->mSize + RESOURCE_PACK_CHUNK_SIZE - 1) / RESOURCE_PACK_CHUNK_SIZE);
                info.mCompressedPackSize = (int64_t) pack->mSize;
                info.mHash = pack->mSha256;
                info.mType = ResourcePackType::Resources;
                owner.getNetworkHandler().send(id, info, owner.getCodecContext());
            }
            break;
        }

        case ResourcePackClientResponsePacket::Status::HaveAllPacks: {
            ResourcePackStackPacket stack;
            stack.mForcedToAccept = owner.getProperties().getTexturePackRequired();
            stack.mGameVersion = owner.getAnnouncement().mGameVersion;

            for (const ResourcePack &pack: owner.getResourcePacks().getPacks()) {
                ResourcePackStackPacket::Entry entry;
                entry.mPackId = pack.mUuidString;
                entry.mPackVersion = pack.mVersion;
                stack.mResourcePacks.push_back(entry);
            }

            owner.getNetworkHandler().send(id, stack, owner.getCodecContext());
            break;
        }

        case ResourcePackClientResponsePacket::Status::Completed:
            sendStartGame(owner, player);
            break;

        case ResourcePackClientResponsePacket::Status::Refused:
            owner._disconnect(id, "disconnectionScreen.resourcePack");
            break;

        default:
            break;
    }
}

void LoginHandler::handleResourcePackChunkRequest(ServerNetworkHandler &owner, const NetworkIdentifier &id,
                                                  const ResourcePackChunkRequestPacket &packet) {
    const ResourcePack *pack = owner.getResourcePacks().findById(packet.mPackId.toString());
    if (pack == nullptr) {
        LOG_WARN(LogAreaID::Network, "Client requested unknown resource pack %s", packet.mPackId.toString().c_str());
        return;
    }

    const uint64_t offset = (uint64_t) packet.mChunkIndex * (uint64_t) RESOURCE_PACK_CHUNK_SIZE;
    if (offset >= pack->mSize)
        return;

    const uint64_t remaining = pack->mSize - offset;
    const uint64_t length = remaining < (uint64_t) RESOURCE_PACK_CHUNK_SIZE ? remaining
                                                                            : (uint64_t) RESOURCE_PACK_CHUNK_SIZE;

    ResourcePackChunkDataPacket chunk;
    chunk.mPackId = pack->mUuid;
    chunk.mPackVersion = pack->mVersion;
    chunk.mChunkIndex = packet.mChunkIndex;
    chunk.mProgress = (int64_t) offset;
    chunk.mData = pack->mData.substr(offset, length);
    owner.getNetworkHandler().send(id, chunk, owner.getCodecContext());
}

void LoginHandler::sendStartGame(ServerNetworkHandler &owner, ServerPlayer &player) {
    const NetworkIdentifier &id = player.getNetworkIdentifier();

    player.getFlags().applyPlayerDefaults();
    player.getAttributes() = ActorAttributes::createPlayerDefaults();
    player.setGameType((int32_t) owner.getProperties().getGameType());
    owner._loadPlayerData(player);
    player.setHungerEnabled(player.getGameType() == (int32_t) GameType::Survival
                            || player.getGameType() == (int32_t) GameType::Adventure);

    StartGamePacket startGame;
    startGame.mUniqueActorId = player.getUniqueId();
    startGame.mRuntimeActorId = player.getRuntimeId();
    startGame.mPlayerGameType = (GameType) player.getGameType();
    startGame.mPlayerPosition = Vector3f(player.getPosition().x, player.getPosition().y + PLAYER_BASE_OFFSET,
                                        player.getPosition().z);
    startGame.mRotation = Vector2f(player.getRotation().x, player.getRotation().y);

    startGame.mSeed = owner.getLevel().getSeed();
    startGame.mDimensionId = Dimension::toId(player.getDimension());
    startGame.mGeneratorId = 1;
    startGame.mLevelGameType = owner.getProperties().getGameType();
    startGame.mDifficulty = (int32_t) owner.getProperties().getDifficulty();
    startGame.mDefaultSpawn = owner.getLevel().getSpawnPosition();
    startGame.mCommandsEnabled = owner.getProperties().getAllowCheats();
    startGame.mTexturePacksRequired = owner.getProperties().getTexturePackRequired();
    startGame.mDefaultPlayerPermission = owner.getProperties().getDefaultPlayerPermissionLevel();
    startGame.mChatRestrictionLevel = owner.getProperties().getChatRestrictionLevel();
    startGame.mDisablingPlayerInteractions = owner.getProperties().getDisablePlayerInteraction();
    startGame.mClientSideGenerationEnabled = owner.getProperties().getClientSideChunkGenerationEnabled();
    startGame.mDisablingCustomSkins = owner.getProperties().getDisableCustomSkins();
    startGame.mServerChunkTickRange = owner.getProperties().getTickDistance();
    startGame.mVanillaVersion = owner.getAnnouncement().mGameVersion;
    startGame.mLevelId = "RmFsY29u";
    startGame.mLevelName = owner.getLevel().getName();
    startGame.mMultiplayerCorrelationId = "";
    startGame.mServerEngine = "Falcon";
    startGame.mCurrentTick = 0;
    startGame.mEnchantmentSeed = 0;
    startGame.mBlockNetworkIdsHashed = owner.getProperties().getBlockNetworkIdsAreHashes();
    startGame.mInventoriesServerAuthoritative = true;

    startGame.mBlockProperties = DataDrivenBlockDefinitions::getAll();
    const std::vector<BlockPropertyData> &customBlocks = CustomContentRegistry::getInstance().getBlockProperties();
    startGame.mBlockProperties.insert(startGame.mBlockProperties.end(), customBlocks.begin(), customBlocks.end());

    startGame.mGamerules = owner.getLevel().getGameRules().toNetwork();

    JigsawStructureDataPacket jigsawStructureData;
    jigsawStructureData.mJigsawStructureData.put("processors", Tag::ofList(Tag::Type::Compound));
    jigsawStructureData.mJigsawStructureData.put("template_pools", Tag::ofList(Tag::Type::Compound));
    jigsawStructureData.mJigsawStructureData.put("jigsaws", Tag::ofList(Tag::Type::Compound));
    jigsawStructureData.mJigsawStructureData.put("structure_sets", Tag::ofList(Tag::Type::Compound));
    owner.getNetworkHandler().send(id, jigsawStructureData, owner.getCodecContext());

    owner.getNetworkHandler().send(id, VoxelShapeRegistry::getPacket(), owner.getCodecContext());

    owner.getNetworkHandler().send(id, startGame, owner.getCodecContext());

    SetTimePacket time;
    time.mTime = (int32_t) owner.getLevel().getDayTime();
    owner.getNetworkHandler().send(id, time, owner.getCodecContext());

    player.setLoginState(ServerPlayer::LoginState::StartGameSent);
    player.getInventoryManager().attach(&player, &owner);

    sendItemComponents(owner, player);
    sendActorIdentifiers(owner, player);
    sendCameraPresets(owner, player);
    sendBiomeDefinitions(owner, player);
    sendAttributes(owner, player);
    sendAbilities(owner, player);
    owner._sendEntityData(player);

    player.getInventoryManager().syncAll();
    player.getInventoryManager().syncSelectedHotbarSlot();

    sendCreativeContent(owner, player);
    owner._sendCraftingData(player);

    addToPlayerList(owner, player);
}

PlayerAbilityData LoginHandler::buildAbilities(ServerNetworkHandler &owner, const ServerPlayer &player) {
    const int32_t gameType = player.getGameType();
    const bool isCreative = gameType == (int32_t) GameType::Creative;
    const bool isSpectator = gameType == (int32_t) GameType::Spectator;
    const bool isAdventure = gameType == (int32_t) GameType::Adventure;

    const uint32_t abilitiesSet = (1u << ((int) PlayerAbility::VerticalFlySpeed + 1)) - 1u;

    uint32_t abilityValues = 1u << (int) PlayerAbility::WalkSpeed;
    abilityValues |= 1u << (int) PlayerAbility::FlySpeed;
    abilityValues |= 1u << (int) PlayerAbility::VerticalFlySpeed;
    abilityValues |= 1u << (int) PlayerAbility::WorldBuilder;

    if (isSpectator) {
        abilityValues |= 1u << (int) PlayerAbility::Invulnerable;
        abilityValues |= 1u << (int) PlayerAbility::Flying;
        abilityValues |= 1u << (int) PlayerAbility::MayFly;
        abilityValues |= 1u << (int) PlayerAbility::NoClip;
    } else {
        abilityValues |= 1u << (int) PlayerAbility::Build;
        abilityValues |= 1u << (int) PlayerAbility::Mine;
        abilityValues |= 1u << (int) PlayerAbility::DoorsAndSwitches;
        abilityValues |= 1u << (int) PlayerAbility::OpenContainers;
        abilityValues |= 1u << (int) PlayerAbility::AttackMobs;

        if (!isAdventure)
            abilityValues |= 1u << (int) PlayerAbility::AttackPlayers;

        if (isCreative) {
            abilityValues |= 1u << (int) PlayerAbility::MayFly;
            abilityValues |= 1u << (int) PlayerAbility::Instabuild;
        }
    }

    if (player.isOp()) {
        abilityValues |= 1u << (int) PlayerAbility::OperatorCommands;
        abilityValues |= 1u << (int) PlayerAbility::Teleport;

        if (!isSpectator) {
            abilityValues |= 1u << (int) PlayerAbility::Build;
            abilityValues |= 1u << (int) PlayerAbility::Mine;
            abilityValues |= 1u << (int) PlayerAbility::DoorsAndSwitches;
            abilityValues |= 1u << (int) PlayerAbility::OpenContainers;
            abilityValues |= 1u << (int) PlayerAbility::AttackPlayers;
            abilityValues |= 1u << (int) PlayerAbility::AttackMobs;
        }
    }

    if (player.isFlying())
        abilityValues |= 1u << (int) PlayerAbility::Flying;

    AbilityLayer layer;
    layer.mLayerType = 1;
    layer.mAbilitiesSet = abilitiesSet;
    layer.mAbilityValues = abilityValues;
    layer.mWalkSpeed = 0.1f;
    layer.mFlySpeed = 0.05f;
    layer.mVerticalFlySpeed = 1.0f;

    PlayerAbilityData data;
    data.mUniqueActorId = player.getUniqueId();
    data.mPlayerPermission = (uint8_t) (player.isOp() ? PlayerPermission::Operator
                                                      : owner.getProperties().getDefaultPlayerPermissionLevel());
    data.mCommandPermission = (uint8_t) (player.isOp() ? CommandPermission::GameDirectors : CommandPermission::Any);
    data.mAbilityLayers.push_back(layer);
    return data;
}

void LoginHandler::sendAbilities(ServerNetworkHandler &owner, ServerPlayer &player) {
    UpdateAbilitiesPacket abilities;
    abilities.mAbilities = buildAbilities(owner, player);

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), abilities, owner.getCodecContext());
}

Uuid LoginHandler::playerListUuid(const ServerPlayer &player) {
    return listUuidFor(player);
}

void LoginHandler::sendBiomeDefinitions(ServerNetworkHandler &owner, ServerPlayer &player) {
    BiomeDefinitionListPacket biomes;
    biomes.mBiomes = owner.getBiomes().getBiomes();

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), biomes, owner.getCodecContext());
}

void LoginHandler::sendCameraPresets(ServerNetworkHandler &owner, ServerPlayer &player) {
    CameraPresetsPacket presets;
    presets.mPresets = standardCameraPresets();

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), presets, owner.getCodecContext());
}

void LoginHandler::sendItemComponents(ServerNetworkHandler &owner, ServerPlayer &player) {
    std::string &cached = owner.getItemComponentsBytes();

    if (cached.empty()) {
        ItemRegistryPacket components;

        for (const std::shared_ptr<ItemDefinition> &definition: owner.getItemDefinitions().getAll()) {
            ItemComponentEntry entry;
            entry.mIdentifier = definition->getIdentifier();
            entry.mRuntimeId = (int16_t) definition->getRuntimeId();
            entry.mComponentBased = definition->isComponentBased();

            const ItemNetworkIdEntry *source = ItemNetworkIdTable::find(definition->getIdentifier());
            entry.mItemVersion = source != nullptr ? source->mVersion : (definition->isComponentBased() ? 1 : 0);

            entry.mComponentData = definition->getComponentData();
            components.mEntries.push_back(entry);
        }

        BinaryStream stream;
        components.writeWithHeader(stream, owner.getCodecContext());
        cached = stream.getBuffer();
    }

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), cached);
}

void LoginHandler::sendActorIdentifiers(ServerNetworkHandler &owner, ServerPlayer &player) {
    AvailableActorIdentifiersPacket identifiers;
    identifiers.mIdentifiers = Tag::ofCompound();

    Tag idlist = Tag::ofList(Tag::Type::Compound);
    int32_t rid = 1;

    const char *const *vanillaIds = VanillaActorTable::getIdentifiers();
    for (size_t index = 0; index < VanillaActorTable::getCount(); ++index) {
        Tag entry = Tag::ofCompound();
        entry.putString("bid", "");
        entry.putByte("hasspawnegg", 0);
        entry.putString("id", vanillaIds[index]);
        entry.putInt("rid", rid++);
        entry.putByte("summonable", 1);
        entry.putByte("experimental", 0);
        idlist.addToList(entry);
    }

    for (const CustomActorDefinition &actor: CustomContentRegistry::getInstance().getActors()) {
        Tag entry = Tag::ofCompound();
        entry.putString("bid", "");
        entry.putByte("hasspawnegg", actor.mIsSpawnable ? 1 : 0);
        entry.putString("id", actor.mIdentifier);
        entry.putInt("rid", rid++);
        entry.putByte("summonable", actor.mIsSummonable ? 1 : 0);
        entry.putByte("experimental", actor.mIsExperimental ? 1 : 0);
        idlist.addToList(entry);
    }
    identifiers.mIdentifiers.put("idlist", idlist);

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), identifiers, owner.getCodecContext());

    const CustomContentRegistry &custom = CustomContentRegistry::getInstance();
    for (const auto &entry: EntityDefinitions::getAllProperties()) {
        if (custom.getActorDefinition(entry.first) == nullptr)
            sendPropertySchema(owner, player, entry.first, entry.second);
    }

    for (const CustomActorDefinition &actor: custom.getActors())
        sendPropertySchema(owner, player, actor.mIdentifier, actor.mProperties);
}

void LoginHandler::buildCraftingData(ServerNetworkHandler &owner) {
    if (owner.getCachedCraftingData().mCleanRecipes)
        return;

    const CraftingIngredientData *ingredients = CraftingRecipeTable::getIngredients();
    const CraftingOutputData *outputs = CraftingRecipeTable::getOutputs();
    const CraftingRecipeData *recipes = CraftingRecipeTable::getRecipes();

    CraftingDataPacket &cached = owner.getCachedCraftingData();
    std::vector<ItemStack> &recipeOutputs = owner.getRecipeOutputsMutable();
    std::vector<uint32_t> &recipeSourceIndices = owner.getRecipeSourceIndicesMutable();

    for (size_t index = 0; index < CraftingRecipeTable::getRecipeCount(); ++index) {
        const CraftingRecipeData &source = recipes[index];

        CraftingRecipeEntry entry;
        entry.mWidth = source.mWidth;
        entry.mHeight = source.mHeight;
        entry.mUuid = Uuid();
        entry.mBlockName = "crafting_table";
        entry.mPriority = 50;
        entry.mSymmetric = source.mWidth > 0;
        entry.mRecipeNetId = (int32_t) recipeOutputs.size() + 1;
        entry.mRecipeId = packRecipeId((uint32_t) entry.mRecipeNetId);

        for (uint32_t i = 0; i < source.mIngredientCount; ++i) {
            const CraftingIngredientData &ingredient = ingredients[source.mIngredientOffset + i];

            RecipeIngredientEntry parsed;
            if (ingredient.mItemId != nullptr) {
                parsed.mHasItem = true;
                parsed.mItemId = ingredient.mItemId;
                parsed.mAuxValue = ingredient.mAuxValue < 0 ? 0x7fff : ingredient.mAuxValue;
                parsed.mCount = ingredient.mCount;
            } else {
                parsed.mCount = 0;
            }

            entry.mInputs.push_back(parsed);
        }

        ItemStack resolvedOutput;

        for (uint32_t i = 0; i < source.mOutputCount; ++i) {
            const CraftingOutputData &output = outputs[source.mOutputOffset + i];
            const ItemNetworkIdEntry *networkEntry = ItemNetworkIdTable::find(output.mItemId);

            if (networkEntry == nullptr)
                continue;

            RecipeOutputEntry parsedOutput;
            parsedOutput.mRuntimeId = networkEntry->mNetworkId;
            parsedOutput.mCount = output.mCount;
            parsedOutput.mMeta = 0;
            parsedOutput.mIsShield = std::string(output.mItemId) == "minecraft:shield";
            const auto blockDefinition = owner.getBlockDefinitions().getDefinition(output.mItemId);
            parsedOutput.mBlockRuntimeId = blockDefinition == nullptr ? 0 : blockDefinition->getRuntimeId();
            entry.mOutputs.push_back(parsedOutput);

            if (resolvedOutput.isAir()) {
                resolvedOutput.mDefinition = owner.getItemDefinitions().getDefinition(output.mItemId);
                resolvedOutput.mBlockDefinition = owner.getBlockDefinitions().getDefinition(output.mItemId);
                resolvedOutput.mCount = output.mCount;
            }
        }

        if (entry.mOutputs.empty())
            continue;

        if (source.mWidth > 0)
            cached.mShapedRecipes.push_back(entry);
        else
            cached.mShapelessRecipes.push_back(entry);

        recipeOutputs.push_back(resolvedOutput);
        recipeSourceIndices.push_back((uint32_t) index);
    }

    const std::vector<CustomRecipe> &customRecipes = CustomContentRegistry::getInstance().getRecipes();
    for (size_t customIndex = 0; customIndex < customRecipes.size(); ++customIndex) {
        const CustomRecipe &recipe = customRecipes[customIndex];

        std::shared_ptr<ItemDefinition> outputDefinition = owner.getItemDefinitions().getDefinition(recipe.mResultItem);
        if (outputDefinition == nullptr)
            continue;

        CraftingRecipeEntry entry;
        entry.mWidth = recipe.mShaped ? recipe.mWidth : 0;
        entry.mHeight = recipe.mShaped ? recipe.mHeight : 0;
        entry.mUuid = Uuid();
        entry.mBlockName = "crafting_table";
        entry.mPriority = 50;
        entry.mSymmetric = false;
        entry.mRecipeNetId = (int32_t) recipeOutputs.size() + 1;
        entry.mRecipeId = packRecipeId((uint32_t) entry.mRecipeNetId);

        for (const CustomRecipeIngredient &ingredient: recipe.mInputs) {
            RecipeIngredientEntry parsed;
            if (ingredient.mEmpty || ingredient.mItemId.empty()) {
                parsed.mCount = 0;
            } else {
                parsed.mHasItem = true;
                parsed.mItemId = ingredient.mItemId;
                parsed.mAuxValue = 0x7fff;
                parsed.mCount = ingredient.mCount;
            }
            entry.mInputs.push_back(parsed);
        }

        RecipeOutputEntry parsedOutput;
        parsedOutput.mRuntimeId = outputDefinition->getRuntimeId();
        parsedOutput.mCount = recipe.mResultCount;
        parsedOutput.mMeta = 0;
        parsedOutput.mIsShield = false;
        const auto outputBlock = owner.getBlockDefinitions().getDefinition(recipe.mResultItem);
        parsedOutput.mBlockRuntimeId = outputBlock == nullptr ? 0 : outputBlock->getRuntimeId();
        entry.mOutputs.push_back(parsedOutput);

        if (recipe.mShaped)
            cached.mShapedRecipes.push_back(entry);
        else
            cached.mShapelessRecipes.push_back(entry);

        ItemStack resolvedOutput;
        resolvedOutput.mDefinition = outputDefinition;
        resolvedOutput.mBlockDefinition = owner.getBlockDefinitions().getDefinition(recipe.mResultItem);
        resolvedOutput.mCount = recipe.mResultCount;

        recipeOutputs.push_back(resolvedOutput);
        recipeSourceIndices.push_back((uint32_t) (CraftingRecipeTable::getRecipeCount() + customIndex));
    }

    const FurnaceRecipeData *furnaceRecipes = CraftingRecipeTable::getFurnaceRecipes();
    int32_t furnaceRecipeNetId = (int32_t) recipeOutputs.size() + 1;
    static const char *const furnaceBlocks[] = {"furnace", "blast_furnace", "smoker"};
    for (const char *blockName: furnaceBlocks) {
        for (size_t index = 0; index < CraftingRecipeTable::getFurnaceRecipeCount(); ++index) {
            const FurnaceRecipeData &source = furnaceRecipes[index];
            const ItemNetworkIdEntry *inputNetwork = ItemNetworkIdTable::find(source.mInputItemId);
            const ItemNetworkIdEntry *outputNetwork = ItemNetworkIdTable::find(source.mOutputItemId);
            if (inputNetwork == nullptr || outputNetwork == nullptr) {
                continue;
            }

            CraftingRecipeEntry entry;
            entry.mRecipeId = packRecipeId((uint32_t) furnaceRecipeNetId);
            entry.mWidth = 1;
            entry.mHeight = 1;
            entry.mBlockName = blockName;
            entry.mPriority = 50;
            entry.mSymmetric = false;
            entry.mRecipeNetId = furnaceRecipeNetId++;

            RecipeIngredientEntry input;
            input.mHasItem = true;
            input.mItemId = source.mInputItemId;
            input.mAuxValue = source.mInputAuxValue < 0 ? 0x7fff : source.mInputAuxValue;
            input.mCount = source.mInputCount;
            entry.mInputs.push_back(std::move(input));

            RecipeOutputEntry output;
            output.mRuntimeId = outputNetwork->mNetworkId;
            output.mCount = source.mOutputCount;
            output.mMeta = 0;
            const auto blockDefinition = owner.getBlockDefinitions().getDefinition(source.mOutputItemId);
            output.mBlockRuntimeId = blockDefinition == nullptr ? 0 : blockDefinition->getRuntimeId();
            output.mIsShield = std::string(source.mOutputItemId) == "minecraft:shield";
            entry.mOutputs.push_back(output);
            cached.mShapelessRecipes.push_back(std::move(entry));
        }
    }

    cached.mCleanRecipes = true;
}

void LoginHandler::sendCraftingData(ServerNetworkHandler &owner, ServerPlayer &player) {
    std::string &cached = owner.getCraftingDataBytes();

    if (cached.empty()) {
        buildCraftingData(owner);

        BinaryStream stream;
        owner.getCachedCraftingData().writeWithHeader(stream, owner.getCodecContext());
        cached = stream.getBuffer();
    }

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), cached);
}

namespace {
    bool isEducationGroup(const char *name) {
        if (name == nullptr)
            return false;

        static const char *const EDUCATION_GROUPS[] = {
                "itemGroup.name.element",
                "itemGroup.name.chemistrytable",
                "itemGroup.name.compounds",
                "itemGroup.name.products",
        };

        for (const char *group: EDUCATION_GROUPS) {
            if (std::strcmp(name, group) == 0)
                return true;
        }

        return false;
    }
}

void LoginHandler::buildCreativeContent(ServerNetworkHandler &owner) {
    if (!owner.getCreativeItemsMutable().empty())
        return;

    std::vector<int32_t> groupRemap(CreativeContentTable::getGroupCount(), -1);

    for (size_t index = 0; index < CreativeContentTable::getGroupCount(); ++index) {
        const CreativeGroupEntry &source = CreativeContentTable::getGroups()[index];

        if (isEducationGroup(source.mName))
            continue;

        groupRemap[index] = (int32_t) owner.getCreativeGroups().size();

        CreativeItemGroup group;
        group.mCategory = source.mCategory;
        group.mName = source.mName;
        group.mIcon = ItemStack::air();

        std::shared_ptr<ItemDefinition> icon = owner.getItemDefinitions().getDefinition(source.mIconIdentifier);
        if (icon != nullptr) {
            group.mIcon.mDefinition = icon;
            group.mIcon.mBlockDefinition = owner.getBlockDefinitions().getDefinition(source.mIconIdentifier);
            group.mIcon.mCount = 1;
        }

        owner.getCreativeGroups().push_back(group);
    }

    int32_t netId = 1;

    for (size_t index = 0; index < CreativeContentTable::getEntryCount(); ++index) {
        const CreativeEntry &source = CreativeContentTable::getEntries()[index];

        std::shared_ptr<ItemDefinition> definition = owner.getItemDefinitions().getDefinition(source.mIdentifier);
        if (definition == nullptr)
            continue;

        int32_t groupIndex = source.mGroupIndex;
        if (groupIndex >= 0) {
            if (groupIndex >= (int32_t) groupRemap.size() || groupRemap[groupIndex] < 0)
                continue;

            groupIndex = groupRemap[groupIndex];
        }

        CreativeItemData entry;
        entry.mNetId = netId++;
        entry.mGroupIndex = groupIndex;
        entry.mItem.mDefinition = definition;
        entry.mItem.mCount = 1;
        entry.mItem.mDamage = source.mDamage;

        if (source.mNbt != nullptr) {
            ReadOnlyBinaryStream stream(std::string((const char *) source.mNbt, source.mNbtSize));
            entry.mItem.mTag = NbtIo::readTag(stream, NbtVariant::LittleEndian);
        }

        entry.mItem.mBlockDefinition = owner.getBlockDefinitions().getDefinition(source.mIdentifier);

        owner.getCreativeItemsMutable().push_back(entry);
    }

    CustomContentRegistry &content = CustomContentRegistry::getInstance();
    std::unordered_map<std::string, int32_t> customGroupIndices;

    auto categoryId = [](const std::string &category) -> uint8_t {
        if (category == "construction")
            return 1;
        if (category == "nature")
            return 2;
        if (category == "equipment")
            return 3;
        return 4;
    };

    auto resolveGroupIndex = [&](const std::string &category, const std::string &group,
                                 const std::shared_ptr<ItemDefinition> &icon,
                                 const std::shared_ptr<BlockDefinition> &iconBlock) -> int32_t {
        const std::string key = category + "\x1f" + group;
        auto it = customGroupIndices.find(key);
        if (it != customGroupIndices.end())
            return it->second;

        CreativeItemGroup created;
        created.mCategory = categoryId(category);
        created.mName = group;
        created.mIcon = ItemStack::air();
        if (!group.empty() && icon != nullptr) {
            created.mIcon.mDefinition = icon;
            created.mIcon.mBlockDefinition = iconBlock;
            created.mIcon.mCount = 1;
        }

        const int32_t index = (int32_t) owner.getCreativeGroups().size();
        owner.getCreativeGroups().push_back(created);
        customGroupIndices[key] = index;
        return index;
    };

    for (const CustomItemDefinition &item: content.getItems()) {
        std::shared_ptr<ItemDefinition> definition = owner.getItemDefinitions().getDefinition(item.mIdentifier);
        if (definition == nullptr)
            continue;

        CreativeItemData entry;
        entry.mNetId = netId++;
        entry.mGroupIndex = resolveGroupIndex(item.mCreativeCategory, item.mCreativeGroup, definition, nullptr);
        entry.mItem.mDefinition = definition;
        entry.mItem.mCount = 1;
        owner.getCreativeItemsMutable().push_back(entry);
    }

    for (const CustomBlockDefinition &block: content.getBlocks()) {
        std::shared_ptr<ItemDefinition> definition = owner.getItemDefinitions().getDefinition(block.mIdentifier);
        if (definition == nullptr)
            continue;

        std::shared_ptr<BlockDefinition> blockDefinition = owner.getBlockDefinitions().getDefinition(block.mIdentifier);

        CreativeItemData entry;
        entry.mNetId = netId++;
        entry.mGroupIndex = resolveGroupIndex(block.mMenuCategory, block.mCreativeGroup, definition, blockDefinition);
        entry.mItem.mDefinition = definition;
        entry.mItem.mBlockDefinition = blockDefinition;
        entry.mItem.mCount = 1;
        owner.getCreativeItemsMutable().push_back(entry);
    }
}

void LoginHandler::sendCreativeContent(ServerNetworkHandler &owner, ServerPlayer &player) {
    std::string &cached = owner.getCreativeContentBytes();

    if (cached.empty()) {
        buildCreativeContent(owner);

        CreativeContentPacket creative;
        creative.mGroups = owner.getCreativeGroups();
        creative.mItems = owner.getCreativeItemsMutable();

        BinaryStream stream;
        creative.writeWithHeader(stream, owner.getCodecContext());
        cached = stream.getBuffer();
    }

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), cached);
}

void LoginHandler::sendAvailableCommands(ServerNetworkHandler &owner, ServerPlayer &player) {
    AvailableCommandsPacket availableCommands;

    for (Command *command: owner.getCommands().getCommands()) {
        if ((int) player.getCommandPermission() < (int) command->getRequiredPermission())
            continue;

        CommandData data;
        data.mName = command->getName();
        data.mDescription = command->getDescription();
        data.mPermission = command->getRequiredPermission();

        data.mOverloads = command->getOverloads();

        availableCommands.mCommands.push_back(data);
    }

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), availableCommands, owner.getCodecContext());
}

void LoginHandler::sendAttributes(ServerNetworkHandler &owner, ServerPlayer &player) {
    UpdateAttributesPacket attributes;
    attributes.mRuntimeActorId = (int64_t) player.getRuntimeId();
    attributes.mTick = 0;
    attributes.mAttributes = player.getAttributes().getAll();

    owner.getNetworkHandler().send(player.getNetworkIdentifier(), attributes, owner.getCodecContext());
}

void LoginHandler::addToPlayerList(ServerNetworkHandler &owner, ServerPlayer &player) {
    PlayerListPacket::Entry newEntry(listUuidFor(player));
    newEntry.mActorId = player.getUniqueId();
    newEntry.mName = player.getName();
    newEntry.mXuid = player.getXuid();
    newEntry.mSkin = player.getSkin();
    newEntry.mBuildPlatform = player.getBuildPlatform();

    PlayerListPacket announce;
    announce.mEntries.push_back(newEntry);

    PlayerListPacket existing;

    for (auto &entry: owner.getPlayers()) {
        if (!entry.second.isSpawned() && &entry.second != &player)
            continue;

        owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), announce, owner.getCodecContext());

        if (&entry.second != &player) {
            PlayerListPacket::Entry existingEntry(listUuidFor(entry.second));
            existingEntry.mActorId = entry.second.getUniqueId();
            existingEntry.mName = entry.second.getName();
            existingEntry.mXuid = entry.second.getXuid();
            existingEntry.mSkin = entry.second.getSkin();
            existingEntry.mBuildPlatform = entry.second.getBuildPlatform();
            existing.mEntries.push_back(existingEntry);
        }
    }

    if (!existing.mEntries.empty())
        owner.getNetworkHandler().send(player.getNetworkIdentifier(), existing, owner.getCodecContext());
}

void LoginHandler::removeFromPlayerList(ServerNetworkHandler &owner, ServerPlayer &player) {
    PlayerListPacket::Entry removalEntry(listUuidFor(player));
    removalEntry.mAction = PlayerListPacket::Action::Remove;

    PlayerListPacket removal;
    removal.mEntries.push_back(removalEntry);

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && &entry.second != &player)
            owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), removal, owner.getCodecContext());
    }
}

void LoginHandler::handleSetLocalPlayerAsInitialized(ServerNetworkHandler &owner, ServerPlayer &player) {
    player.setLoginState(ServerPlayer::LoginState::Spawned);
    player.grantSpawnInvulnerability();
    BonusChest::placeIfPending(owner, owner.getLevel());
    LOG_INFO(LogAreaID::Server, "Player %s spawned", player.getName().c_str());

    sendAvailableCommands(owner, player);

    player.setEffectsNetworkReady(true);
    player.syncEffects();

    owner._sendInventory(player);
    owner._sendHealth(player);
    ItemActorHandler::sendItemActorsTo(owner, player);
    owner.sendActorsTo(player);
    owner.sendWeatherTo(player);

    owner.broadcastTranslation("multiplayer.player.joined", {player.getName()});
}

void LoginHandler::checkTerrainReady(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (player.hasSpawnChunksReady())
        return;

    if (player.getSentChunkCount() < SPAWN_CHUNK_THRESHOLD)
        return;

    player.setSpawnChunksReady(true);

    PlayStatusPacket spawn;
    spawn.mStatus = PlayStatusPacket::Status::PlayerSpawn;
    owner.getNetworkHandler().send(player.getNetworkIdentifier(), spawn, owner.getCodecContext());

}
