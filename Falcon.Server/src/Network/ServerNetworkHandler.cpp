#include "Network/ServerNetworkHandler.h"

#include "Core/Text/StringUtil.h"

#include "World/Actor/Movement/PlayerMovementRegistry.h"

#include "Scripting/Content/CustomContentRegistry.h"

#include "World/Actor/RideSystem.h"
#include "Core/Math/MathConstants.h"

#include "Server/Commands/ServerCommandOrigin.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/BlockNetworkIdTable.h"
#include "World/Level/Block/BlockPickItem.h"
#include "World/Level/Storage/AutoCompaction.h"
#include "World/Level/LevelGen/Overworld/OverworldGenerator.h"
#include "World/Level/Block/Systems/BlockContactSystem.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "World/Item/ElytraItem.h"
#include "World/Item/TotemItem.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Core/Debug/BedrockLog.h"
#include "Network/ConnectionRequest.h"
#include "Network/AuthKeyProvider.h"
#include "Network/TransportFactory.h"
#include "Network/ClientAttestation.h"
#include "Server/ServerTelemetry.h"
#include "Network/NetherNet/NetherNetInstance.h"
#include "Network/Handler/BadPacketHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/BookHandler.h"
#include "Network/Handler/CrafterHandler.h"
#include "Network/Handler/LecternHandler.h"
#include "Network/Handler/LodestoneHandler.h"
#include "Network/Handler/ChatHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "World/Inventory/InventoryManager.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/LoginChainVerifier.h"
#include "Network/Handler/LoginHandler.h"
#include "Network/Handler/MovementHandler.h"
#include "Network/Handler/SubChunkRequestHandler.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginPackets.h"
#include "Server/ServerPaths.h"
#include "Protocol/Packets/SubChunkRequestPacket.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"
#include "Protocol/Codec/ProtocolCodec.h"
#include "Protocol/Packets/DisconnectPacket.h"
#include "Protocol/Packets/GameRulesChangedPacket.h"
#include "Protocol/Packets/LevelChunkPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/LoginPacket.h"
#include "Protocol/Packets/NetworkChunkPublisherUpdatePacket.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/MapInfoRequestPacket.h"
#include "Protocol/Packets/MapCreateLockedCopyPacket.h"
#include "Network/Handler/MapHandler.h"
#include "Protocol/Packets/RequestChunkRadiusPacket.h"
#include "Protocol/Packets/RequestAbilityPacket.h"
#include "Protocol/Packets/ChunkRadiusUpdatedPacket.h"
#include "Protocol/Packets/ClientboundCloseFormPacket.h"
#include "Protocol/Packets/RequestNetworkSettingsPacket.h"
#include "Protocol/Packets/ResourcePackClientResponsePacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ResourcePacksInfoPacket.h"
#include "Protocol/Packets/ResourcePackDataInfoPacket.h"
#include "Protocol/Packets/ResourcePackChunkDataPacket.h"
#include "Protocol/Packets/ResourcePackChunkRequestPacket.h"
#include "Protocol/Packets/SetLocalPlayerAsInitializedPacket.h"
#include "Protocol/Packets/NetworkStackLatencyPacket.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Packets/MovePlayerPacket.h"
#include "Protocol/Packets/CorrectPlayerMovePredictionPacket.h"
#include "Protocol/Packets/AvailableCommandsPacket.h"
#include "Protocol/Packets/BiomeDefinitionListPacket.h"
#include "Protocol/Packets/CreativeContentPacket.h"
#include "Protocol/Packets/ItemRegistryPacket.h"
#include "Protocol/Types/CreativeItemCategory.h"
#include "Protocol/Packets/CommandOutputPacket.h"
#include "Protocol/Packets/CommandRequestPacket.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Protocol/Packets/SetPlayerGameTypePacket.h"
#include "Protocol/Packets/PacketViolationWarningPacket.h"
#include "Protocol/Packets/PlayerListPacket.h"
#include "Protocol/Packets/TextPacket.h"
#include "Protocol/Packets/UpdateAbilitiesPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "World/Actor/Player/PlayerAbility.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Protocol/Packets/SetTimePacket.h"
#include "Protocol/Packets/SetDifficultyPacket.h"
#include "Protocol/Packets/SetSpawnPositionPacket.h"
#include "Protocol/Packets/AvailableActorIdentifiersPacket.h"
#include "Protocol/Packets/DeathInfoPacket.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/ChangeDimensionPacket.h"
#include "Protocol/Packets/PlayerActionPacket.h"
#include "Protocol/Packets/RespawnPacket.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Packets/BlockPickRequestPacket.h"
#include "Protocol/Packets/ActorPickRequestPacket.h"
#include "Protocol/Packets/AnimatePacket.h"
#include "Protocol/Packets/BookEditPacket.h"
#include "Protocol/Packets/LecternUpdatePacket.h"
#include "Protocol/Packets/PlayerToggleCrafterSlotRequestPacket.h"
#include "Protocol/Packets/PositionTrackingDBClientRequestPacket.h"
#include "Protocol/Packets/EmotePacket.h"
#include "Protocol/Packets/RequestPermissionsPacket.h"
#include "Protocol/Packets/SetDefaultGameTypePacket.h"
#include "Protocol/Packets/ModalFormRequestPacket.h"
#include "Protocol/Packets/ModalFormResponsePacket.h"
#include "Protocol/Packets/UpdateClientOptionsPacket.h"
#include "Protocol/Packets/ClientboundDataDrivenUICloseScreenPacket.h"
#include "Protocol/Packets/PlayerSkinPacket.h"
#include "Protocol/Packets/SetHealthPacket.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/ContainerClosePacket.h"
#include "Protocol/Packets/CompletedUsingItemPacket.h"
#include "Protocol/Packets/CraftingDataPacket.h"
#include "Protocol/Packets/CraftingEventPacket.h"
#include "Protocol/Packets/InteractPacket.h"
#include "Protocol/Packets/InventoryContentPacket.h"
#include "Protocol/Packets/InventorySlotPacket.h"
#include "Protocol/Packets/InventoryTransactionPacket.h"
#include "Protocol/Packets/ItemStackRequestPacket.h"
#include "Protocol/Packets/MobEquipmentPacket.h"
#include "Protocol/Packets/PlayerHotbarPacket.h"
#include "World/Level/Block/Components/CreativeContentTable.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/Systems/CommandBlockSystem.h"
#include "World/Level/Block/Systems/FireSystem.h"
#include "World/Level/Block/Systems/PrecipitationSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "Protocol/Packets/CommandBlockUpdatePacket.h"
#include "World/Level/Block/BedBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Item/ItemNetworkIdTable.h"
#include "World/Item/ItemData.h"
#include "World/Item/VanillaItems.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/ChorusFruitItem.h"
#include "Core/NBT/NbtIo.h"

#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <fstream>
#include <thread>
#include <utility>
#include "Protocol/Packets/UpdateAttributesPacket.h"

#include <rtc/global.hpp>

int64_t ServerNetworkHandler::randomSeed() {
    std::random_device device;
    return (int64_t) (((uint64_t) device() << 32) | (uint64_t) device());
}

namespace {
    // a Falcon choice, not a game value: the key fetch usually ends well under it
    const std::chrono::seconds LOGIN_KEY_WAIT(5);

    // the game's WebRTC never aborts SCTP over retransmissions (ICE consent checks, timers capped at 3 s);
    // libdatachannel aborts after 5 retransmits (~6 s locally): drops a client stalled under load
    void configureNetherNetSctp() {
        rtc::SctpSettings settings;
        settings.maxRetransmitAttempts = 60;
        settings.maxRetransmitTimeout = std::chrono::milliseconds(3000);
        rtc::SetSctpSettings(settings);
    }

    class JsonValidator {
    public:
        explicit JsonValidator(const std::string &value) : mValue(value) {}

        bool parse() {
            skipWhitespace();
            if (!parseValue())
                return false;
            skipWhitespace();
            return mPosition == mValue.size();
        }

    private:
        void skipWhitespace() {
            while (mPosition < mValue.size() && std::isspace((unsigned char) mValue[mPosition]))
                ++mPosition;
        }

        bool parseValue() {
            skipWhitespace();
            if (mPosition >= mValue.size())
                return false;

            switch (mValue[mPosition]) {
                case '{':
                case '[': {
                    if (mDepth >= MAX_DEPTH)
                        return false;
                    ++mDepth;
                    const bool valid = mValue[mPosition] == '{' ? parseObject() : parseArray();
                    --mDepth;
                    return valid;
                }
                case '"':
                    return parseString();
                case 't':
                    return parseLiteral("true");
                case 'f':
                    return parseLiteral("false");
                case 'n':
                    return parseLiteral("null");
                default:
                    return parseNumber();
            }
        }

        bool parseObject() {
            ++mPosition;
            skipWhitespace();
            if (mPosition < mValue.size() && mValue[mPosition] == '}') {
                ++mPosition;
                return true;
            }

            while (mPosition < mValue.size()) {
                if (!parseString())
                    return false;
                skipWhitespace();
                if (mPosition >= mValue.size() || mValue[mPosition++] != ':')
                    return false;
                if (!parseValue())
                    return false;
                skipWhitespace();
                if (mPosition >= mValue.size())
                    return false;
                if (mValue[mPosition] == '}') {
                    ++mPosition;
                    return true;
                }
                if (mValue[mPosition++] != ',')
                    return false;
                skipWhitespace();
            }
            return false;
        }

        bool parseArray() {
            ++mPosition;
            skipWhitespace();
            if (mPosition < mValue.size() && mValue[mPosition] == ']') {
                ++mPosition;
                return true;
            }

            while (mPosition < mValue.size()) {
                if (!parseValue())
                    return false;
                skipWhitespace();
                if (mPosition >= mValue.size())
                    return false;
                if (mValue[mPosition] == ']') {
                    ++mPosition;
                    return true;
                }
                if (mValue[mPosition++] != ',')
                    return false;
                skipWhitespace();
            }
            return false;
        }

        bool parseString() {
            if (mPosition >= mValue.size() || mValue[mPosition++] != '"')
                return false;
            while (mPosition < mValue.size()) {
                const unsigned char character = (unsigned char) mValue[mPosition++];
                if (character == '"')
                    return true;
                if (character < 0x20)
                    return false;
                if (character != '\\')
                    continue;
                if (mPosition >= mValue.size())
                    return false;
                const char escape = mValue[mPosition++];
                if (escape == 'u') {
                    if (mPosition + 4 > mValue.size())
                        return false;
                    for (size_t index = 0; index < 4; ++index) {
                        if (!std::isxdigit((unsigned char) mValue[mPosition++]))
                            return false;
                    }
                } else if (escape != '"' && escape != '\\' && escape != '/' && escape != 'b' && escape != 'f'
                           && escape != 'n' && escape != 'r' && escape != 't') {
                    return false;
                }
            }
            return false;
        }

        bool parseLiteral(const char *literal) {
            const size_t length = std::strlen(literal);
            if (mValue.compare(mPosition, length, literal) != 0)
                return false;
            mPosition += length;
            return true;
        }

        bool parseNumber() {
            const size_t start = mPosition;
            if (mPosition < mValue.size() && mValue[mPosition] == '-')
                ++mPosition;
            if (mPosition >= mValue.size())
                return false;
            if (mValue[mPosition] == '0') {
                ++mPosition;
            } else {
                if (mValue[mPosition] < '1' || mValue[mPosition] > '9')
                    return false;
                while (mPosition < mValue.size() && std::isdigit((unsigned char) mValue[mPosition]))
                    ++mPosition;
            }
            if (mPosition < mValue.size() && mValue[mPosition] == '.') {
                ++mPosition;
                const size_t fractionStart = mPosition;
                while (mPosition < mValue.size() && std::isdigit((unsigned char) mValue[mPosition]))
                    ++mPosition;
                if (fractionStart == mPosition)
                    return false;
            }
            if (mPosition < mValue.size() && (mValue[mPosition] == 'e' || mValue[mPosition] == 'E')) {
                ++mPosition;
                if (mPosition < mValue.size() && (mValue[mPosition] == '+' || mValue[mPosition] == '-'))
                    ++mPosition;
                const size_t exponentStart = mPosition;
                while (mPosition < mValue.size() && std::isdigit((unsigned char) mValue[mPosition]))
                    ++mPosition;
                if (exponentStart == mPosition)
                    return false;
            }
            return start != mPosition;
        }

        static constexpr int MAX_DEPTH = 64;

        const std::string &mValue;
        size_t mPosition = 0;
        int mDepth = 0;
    };

    // width * height * 4 bytes, or no image at all
    bool isValidSkinImage(const SkinImageData &image) {
        if (image.mWidth == 0 && image.mHeight == 0)
            return image.mData.empty();
        if (image.mWidth <= 0 || image.mHeight <= 0)
            return false;
        return image.mData.size() == (size_t) image.mWidth * (size_t) image.mHeight * 4;
    }

    // the cape and the animations are relayed to every player like the skin: each image must be the
    // size its header announces, which the packet itself then bounds
    bool isValidSkinExtras(const SerializedSkin &skin) {
        if (!isValidSkinImage(skin.mCapeData))
            return false;
        for (const SkinAnimationData &animation: skin.mAnimations) {
            if (!isValidSkinImage(animation.mImage))
                return false;
        }
        return true;
    }

    bool isValidSkin(const SerializedSkin &skin) {
        if (!isValidSkinExtras(skin))
            return false;
        if (skin.mSkinData.mWidth <= 0 || skin.mSkinData.mHeight <= 0)
            return skin.mPersona && !skin.mPersonaPieces.empty();
        if (skin.mSkinData.mWidth > 128 || skin.mSkinData.mHeight > 128)
            return false;

        const size_t expectedSize = (size_t) skin.mSkinData.mWidth * (size_t) skin.mSkinData.mHeight * 4;
        return skin.mSkinData.mData.size() == expectedSize;
    }

    void normalizeSkin(SerializedSkin &skin) {
        if (skin.mGeometryData.empty())
            skin.mGeometryData = "{}";
        if (skin.mGeometryDataEngineVersion.empty())
            skin.mGeometryDataEngineVersion = "0.0.0";
        if (skin.mSkinResourcePatch.empty())
            skin.mSkinResourcePatch = "{\"geometry\":{\"default\":\"geometry.humanoid.custom\"}}";
        if (skin.mFullSkinId.empty())
            skin.mFullSkinId = skin.mSkinId;
    }
}

void ServerNetworkHandler::boundSkin(SerializedSkin &skin) {
    if (isValidSkinExtras(skin))
        return;

    // a login is not refused over its cosmetics: an image that lies about its size is dropped
    if (!isValidSkinImage(skin.mCapeData))
        skin.mCapeData = SkinImageData();
    skin.mAnimations.erase(
            std::remove_if(skin.mAnimations.begin(), skin.mAnimations.end(),
                           [](const SkinAnimationData &animation) { return !isValidSkinImage(animation.mImage); }),
            skin.mAnimations.end());
}

ServerNetworkHandler::ServerNetworkHandler(const std::string &serverName, const std::string &subName, int maxPlayers,
                                           TransportLayer transport)
        : mCodecContext(mBlockDefinitions, mItemDefinitions), mMaxPlayers(maxPlayers), mIsListening(false),
          mNextRuntimeId(1), mPlayerData(ServerPaths::file("players")), mOps(ServerPaths::file("ops.txt")),
          mAllowList(ServerPaths::file("allowlist.json")), mBanList(ServerPaths::file("banned-players.json")),
          mIpBanList(ServerPaths::file("banned-ips.json")) {
    mPlayerMovementRegistry = std::make_unique<PlayerMovementRegistry>();
    mWorlds.add("Bedrock level", DEFAULT_VIEW_DISTANCE, 0);

    configureNetherNetSctp();
    std::unique_ptr<Connector> netherNet = TransportFactory::createConnector(transport, *this, true);

    if (netherNet != nullptr) {
        mNetherNetInstance = dynamic_cast<NetherNetInstance *>(netherNet.get());

        mNetworkHandler.reset(new NetworkHandler(std::move(netherNet)));
        mNetworkHandler->setProfiler(&mProfiler);
        mNetworkHandler->addListener(this);
        mNetworkHandler->setContextResolver([this](const NetworkIdentifier &id) { return &getCodecContext(id); });
    }

    mAnnouncement.mServerName = serverName;
    mAnnouncement.mSubName = subName;
    mAnnouncement.mGameMode = "Survival";
    mAnnouncement.mServerAvailability = 1;
    mAnnouncement.mMaxPlayers = maxPlayers;

    _registerCommands();
    _bindScoreboard();

    mResourcePacks.loadFromDirectory(ServerPaths::file("resource_packs"));
    mResourcePacks.loadBundledAddonsFrom(ServerPaths::file("behavior_packs"));
    mResourcePacks.loadCdnConfig(ServerPaths::file("cdn_config.json"));
    _registerVanillaDefinitions();
    _validateServerboundPackets();

    AuthKeyProvider::getInstance().start();
    mPluginManager = std::make_unique<PluginManager>(*this);
}

PlayerMovementRegistry &ServerNetworkHandler::getPlayerMovementRegistry() {
    return *mPlayerMovementRegistry;
}

void ServerNetworkHandler::erasePlayer(const NetworkIdentifier &id) {
    const auto player = mPlayers.find(id);
    if (player != mPlayers.end()) {
        mPlayerMovementRegistry->remove(player->second.getRuntimeId());
        mPlayers.erase(player);
    }
}

unsigned short ServerNetworkHandler::getBoundPort() const {
    return mNetherNetInstance != nullptr ? mNetherNetInstance->getBoundPort() : 0;
}

bool ServerNetworkHandler::isSignalingSecure() const {
    return mNetherNetInstance != nullptr && mNetherNetInstance->isSignalingSecure();
}

unsigned short ServerNetworkHandler::getExtraSignalingPort() const {
    return mNetherNetInstance != nullptr ? mNetherNetInstance->getExtraSignalingPort() : 0;
}

int ServerNetworkHandler::_getServerViewDistance() const {
    int distance = mProperties.getViewDistance();

    if (distance < 1)
        distance = 1;
    if (distance > MAX_VIEW_DISTANCE)
        distance = MAX_VIEW_DISTANCE;

    return distance;
}

size_t ServerNetworkHandler::_getChunkWorkerThreadCount() const {
    int configured = mProperties.getMaxThreads();

    if (configured <= 0) {
        const unsigned hardware = std::thread::hardware_concurrency();
        configured = hardware == 0 ? 2 : (int) (hardware > 3 ? hardware - 2 : 1);
    }

    if (configured < 1)
        configured = 1;

    return (size_t) configured;
}

void ServerNetworkHandler::_registerVanillaDefinitions() {
    LoginHandler::registerVanillaDefinitions(*this);
}

ServerNetworkHandler::~ServerNetworkHandler() {
    try {
        stopServerListening();
    } catch (...) {
        if (mNetworkHandler != nullptr)
            mNetworkHandler->disconnect();
    }

    if (mNetworkHandler != nullptr)
        mNetworkHandler->removeListener(this);
}

void ServerNetworkHandler::setMotd(const std::string &serverName, const std::string &subName) {
    mAnnouncement.mServerName = serverName;
    mAnnouncement.mSubName = subName;

    if (mIsListening)
        _updateServerAnnouncement();
}

void ServerNetworkHandler::setProtocolVersion(int protocolVersion, const std::string &gameVersion) {
    mAnnouncement.mProtocolVersion = protocolVersion;
    mAnnouncement.mGameVersion = gameVersion;

    if (mIsListening)
        _updateServerAnnouncement();
}

void ServerNetworkHandler::setProperties(const PropertiesSettings &properties) {
    mProperties = properties;
    mPlayerIdleTimeoutTicks = (int64_t) mProperties.getPlayerIdleTimeout() * TICKS_PER_MINUTE;

    // encryption is what proves a client holds the key in its token: without it a login seen once replays
    if (mProperties.getOnlineMode() && !mProperties.getNetworkEncryption())
        LOG_WARN(LogAreaID::Server, "online-mode is on but network-encryption is off: a login captured elsewhere "
                                    "can be replayed here. Turn network-encryption back on");

    if (mNetherNetInstance != nullptr) {
        mNetherNetInstance->setUnofficialClientsAllowed(mProperties.getAllowUnofficialClients());
        mNetherNetInstance->setAnonymousLanClientsAllowed(mProperties.getAllowLanWithoutIdentity());
        // the verdict comes back later, see the tick: the connection is not held for it
        mNetherNetInstance->setNegotiationFailureObserver([](int errorCode, int clientCheck) {
            if (clientCheck != 0)
                ServerTelemetry::getInstance().countFailure(ServerTelemetry::FailureStage::OfferCheck, clientCheck);
            else
                ServerTelemetry::getInstance().countFailure(ServerTelemetry::FailureStage::Offer, errorCode);
        });
        mNetherNetInstance->setConnectionObserver(
                [](const nethernet::Connection &connection) { ClientAttestation::getInstance().inspect(connection); });
        mNetherNetInstance->setServerDataProvider([this]() {
            nethernet::ServerData data;
            data.mServerName = mProperties.getServerName();
            data.mProtocol = mAnnouncement.mProtocolVersion;
            data.mGameVersion = mAnnouncement.mGameVersion;
            data.mLevelName = mProperties.getLevelName();
            data.mGameType = (int32_t) mProperties.getGameType();
            data.mPlayerCount = getActivePlayerCount();
            data.mMaxPlayerCount = mProperties.getMaxPlayers();
            data.mAcceptsOnlineAuth = mProperties.getOnlineMode();
            data.mAcceptsSelfSignedAuth = !mProperties.getOnlineMode();
            return data;
        });

        const std::string certificate = mProperties.getSignalingCertificate();
        const std::string privateKey = mProperties.getSignalingPrivateKey();
        const bool certificatePair = !certificate.empty() && !privateKey.empty();
        if (certificate.empty() != privateKey.empty())
            LOG_WARN(LogAreaID::Server, "signaling-certificate and signaling-private-key go together, both are "
                                        "ignored");
        if (certificatePair)
            mNetherNetInstance->setTlsCertificate(ServerPaths::file(certificate), ServerPaths::file(privateKey));
        else
            mNetherNetInstance->setTlsCertificate(std::string(), std::string());
        mNetherNetInstance->setExtraSignalingPort(mProperties.getSignalingExtraPort());

        const std::string acmeDomain = acme::normalizeDomain(mProperties.getSignalingAcmeDomain());
        const std::string acmeDirectory = mProperties.getSignalingAcmeDirectory();
        acme::ChallengeType challenge = acme::ChallengeType::TlsAlpn01;
        acme::parseChallengeType(mProperties.getSignalingAcmeChallenge(), challenge);
        mNetherNetInstance->disableAcme();
        if (acmeDomain.empty() || certificatePair) {
            if (!acmeDomain.empty())
                LOG_WARN(LogAreaID::Server, "signaling-certificate is set, signaling-acme-domain is ignored");
        } else if (!acme::isDomain(acmeDomain)) {
            LOG_WARN(LogAreaID::Server, "signaling-acme-domain must be a public domain name such as "
                                        "play.example.org, ACME is off");
        } else if (acmeDirectory.rfind("https://", 0) != 0) {
            LOG_WARN(LogAreaID::Server, "signaling-acme-directory must be an https:// URL, ACME is off");
        } else {
            acme::ManagerSettings acmeSettings;
            acmeSettings.mClient.mDirectoryUrl = acmeDirectory;
            acmeSettings.mClient.mDomain = acmeDomain;
            acmeSettings.mClient.mEmail = mProperties.getSignalingAcmeEmail();
            acmeSettings.mClient.mChallenge = challenge;
            acmeSettings.mStorageDirectory = ServerPaths::file("certificates");
            mNetherNetInstance->enableAcme(acmeSettings);
        }
    }

    const int64_t levelSeed = properties.getLevelSeed().empty()
                                      ? randomSeed()
                                      : OverworldGenerator::parseSeed(properties.getLevelSeed());

    for (World *world: mWorlds.getWorlds())
        mWorlds.remove(world->getId());
    _openWorld(properties.getLevelName(), levelSeed);
    loadScoreboard();

    for (const std::string &name: properties.getAutoloadWorlds()) {
        if (mWorlds.find(name) == nullptr && WorldManager::isValidName(name))
            _openWorld(name, randomSeed());
    }

    _logPackStack();

    switch (properties.getGameType()) {
        case GameType::Creative:
            mAnnouncement.mGameMode = "Creative";
            break;
        case GameType::Adventure:
            mAnnouncement.mGameMode = "Adventure";
            break;
        default:
            mAnnouncement.mGameMode = "Survival";
            break;
    }

    mAnnouncement.mServerAvailability = 1;

    if (mIsListening)
        _updateServerAnnouncement();
}

void ServerNetworkHandler::_logPackStack() const {
    const std::vector<ResourcePack> &packs = mResourcePacks.getPacks();

    if (packs.empty()) {
        LOG_INFO(LogAreaID::Server, "Pack Stack - None");
        return;
    }

    std::string names;
    for (const ResourcePack &pack: packs) {
        if (!names.empty())
            names += ", ";

        names += pack.mName;
    }

    LOG_INFO(LogAreaID::Server, "Pack Stack - %s", names.c_str());
}

bool ServerNetworkHandler::startServerListening(const ConnectionDefinition &definition) {
    if (mIsListening)
        return false;

    if (mNetworkHandler == nullptr) {
        LOG_ERROR(LogAreaID::Network, "No transport was created, cannot start listening");
        return false;
    }

    if (mPluginsEnabled)
        mPluginManager->loadAll(ServerPaths::file("plugins"));
    CustomContentRegistry::getInstance().freeze();
    mPluginManager->enableAll();

    if (!mNetworkHandler->host(definition)) {
        LOG_ERROR(LogAreaID::Network, "Failed to bind UDP port %u", definition.mPort);
        mPluginManager->disableAll();
        return false;
    }

    mMaxPlayers = definition.mMaxNumPlayers;
    mAnnouncement.mMaxPlayers = mMaxPlayers;
    mIsListening = true;

    _updateServerAnnouncement();
    _queueSpawnWarmup();
    mNetworkHandler->startIoThread();
    _loadScripts();
    return true;
}

void ServerNetworkHandler::_validatePackDependencies() {
    for (;;) {
        std::vector<std::string> available = mResourcePacks.getLoadedUuids();
        const std::vector<std::string> behaviorUuids = mBehaviorPacks.getLoadedUuids();
        available.insert(available.end(), behaviorUuids.begin(), behaviorUuids.end());

        const size_t removed = mResourcePacks.pruneUnsatisfied(available) + mBehaviorPacks.pruneUnsatisfied(available);
        if (removed == 0)
            break;
    }
}

void ServerNetworkHandler::_loadScripts() {
    if (!mScriptEngine.isReady())
        return;

    mScriptEngine.bindHost(*this);

    mBehaviorPacks.discover(ServerPaths::file("behavior_packs"));
    _validatePackDependencies();

    CustomContentRegistry::getInstance().load(mBehaviorPacks, mItemDefinitions, mBlockDefinitions);

    loadWorldDynamicProperties();

    mScriptEngine.loadPacks(mBehaviorPacks.getPacks());
    mScriptEngine.startup();
}

void ServerNetworkHandler::stopServerListening() {
    if (!mIsListening)
        return;

    // set first: after an exception mid-shutdown the destructor won't retry it
    mIsListening = false;
    struct DisconnectOnExit {
        NetworkHandler *handler;
        ~DisconnectOnExit() {
            try {
                handler->disconnect();
            } catch (...) {
            }
        }
    } disconnectOnExit{mNetworkHandler.get()};

    std::exception_ptr error;
    const auto finish = [&error](const std::function<void()> &step) {
        try {
            step();
        } catch (...) {
            if (!error)
                error = std::current_exception();
        }
    };
    finish([this]() { mScriptEngine.shutdown(); });
    finish([this]() { mPluginManager->disableAll(); });

    if (!mPlayers.empty()) {
        LOG_INFO(LogAreaID::Server, "Saving data for %zu player(s)", mPlayers.size());

        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;

            finish([this, &player]() {
                if (!player.getName().empty())
                    _savePlayerData(player, true);
            });
            finish([this, &entry, &player]() {
                _disconnect(entry.first, DisconnectReason::Shutdown, player.localize("falcon.disconnect.disconnected"));
            });
        }

        mPlayers.clear();
        finish([this]() { mNetworkHandler->runEvents(); });
    }

    finish([this]() { saveWorldDynamicProperties(true); });
    finish([this]() { saveScoreboard(true); });
    finish([this]() { saveAllActors(); });

    for (World *world: mWorlds.getWorlds()) {
        LOG_INFO(LogAreaID::Server, "Saving level %s", world->getName().c_str());
        AutoCompaction::untrack(world->getOverworld());
        finish([world]() { world->close(); });
    }

    finish([]() { AuthKeyProvider::getInstance().stop(); });
    finish([this]() { mNetworkHandler->disconnect(); });

    LOG_INFO(LogAreaID::Server, "Server stopped.");
    if (error)
        std::rethrow_exception(error);
}

void ServerNetworkHandler::tick(bool paused) {
    mSimulationPaused = paused;
    if (!mIsListening)
        return;

    _retryLoginsAwaitingKeys();
    ++mChunkStreamTick;
    if (mChunkStreamTick % CHUNK_PACKET_EXPIRY_INTERVAL == 0)
        mChunkPacketCache.expire(mChunkStreamTick);
    _updateJoinBurst();
    for (World *world: mWorlds.getWorlds())
        world->getTickingAreas().retryPendingLoads(*world);
    if (paused) {
        mNetworkHandler->runEvents();
        std::vector<std::string> commands;
        {
            std::lock_guard<std::mutex> lock(mConsoleQueueMutex);
            while (!mConsoleQueue.empty()) {
                commands.push_back(std::move(mConsoleQueue.front()));
                mConsoleQueue.pop();
            }
        }
        for (const std::string &command: commands)
            executeConsoleCommand(command);
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(mMainThreadTaskMutex);
            tasks.swap(mMainThreadTasks);
        }
        for (const auto &task: tasks)
            task();
        for (Level *level: getLevels()) {
            level->drainCompletedChunks();
            level->processGeneratedChanges();
            const std::vector<int64_t> repopulated = level->consumeRepopulatedChunks();
            if (repopulated.empty())
                continue;
            for (auto &entry: mPlayers) {
                ServerPlayer &player = entry.second;
                if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent || !player.isIn(*level))
                    continue;
                for (const int64_t hash: repopulated)
                    ChunkStreamHandler::invalidateChunk(player, hash);
            }
        }
        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;
            if (player.getLoginState() >= ServerPlayer::LoginState::StartGameSent && player.hasChunkPosition()) {
                _sendChunks(player);
                _checkTerrainReady(player);
            }
            player.getAcknowledgements().flush(*this, entry.first);
        }
        return;
    }

    const std::chrono::steady_clock::time_point tickStart = std::chrono::steady_clock::now();

    mTickStartSamples.push_back(tickStart);
    if (mTickStartSamples.size() > TICK_SAMPLE_COUNT)
        mTickStartSamples.pop_front();

    mCurrentTick++;
    for (World *world: mWorlds.getWorlds())
        world->getOverworld().tickTime();
    _tickSleep();

    const int autoSaveInterval = mProperties.getAutoSaveInterval();
    if (mAutoSaveEnabled && autoSaveInterval > 0 && mCurrentTick % autoSaveInterval == 0)
        autoSave();
    // chunk generation and saving copy whole chunks: freed memory piles up even without unloads
    if (mCurrentTick % HEAP_RELEASE_INTERVAL_TICKS == 0)
        Level::releaseFreedHeap();
    mProfiler.beginTick(mCurrentTick);

    mProfiler.beginSection(ProfilerSection::Weather);
    for (World *world: mWorlds.getWorlds())
        world->getOverworld().updateSkyLightSubtracted();
    tickWeather();
    mProfiler.endSection(ProfilerSection::Weather);

    mProfiler.beginSection(ProfilerSection::ConsoleCommands);
    {
        std::vector<std::string> commandLines;
        {
            std::lock_guard<std::mutex> lock(mConsoleQueueMutex);
            while (!mConsoleQueue.empty()) {
                commandLines.push_back(std::move(mConsoleQueue.front()));
                mConsoleQueue.pop();
            }
        }

        for (const std::string &commandLine: commandLines)
            executeConsoleCommand(commandLine);
    }

    {
        std::vector<std::function<void()>> tasks;
        {
            std::lock_guard<std::mutex> lock(mMainThreadTaskMutex);
            tasks.swap(mMainThreadTasks);
        }

        for (const std::function<void()> &task: tasks)
            task();
    }
    mProfiler.endSection(ProfilerSection::ConsoleCommands);

    mPluginManager->tick();
    if (mPluginManager->hasSubscribers(FALCON_EVENT_SERVER_TICK)) {
        PluginEvent tickEvent;
        tickEvent.mType = FALCON_EVENT_SERVER_TICK;
        tickEvent.mTick = (uint64_t) mCurrentTick;
        mPluginManager->dispatch(tickEvent);
    }

    mNetworkHandler->runEvents();

    for (World *world: mWorlds.getWorlds())
        _tickOverworld(*world);
    _tickSpawnWarmup();

    mProfiler.beginSection(ProfilerSection::Players);
    for (auto &entry: mPlayers)
        _tickPlayer(entry.second);
    mProfiler.endSection(ProfilerSection::Players);

    mProfiler.beginSection(ProfilerSection::BlockActors);
    for (Level *level: getLevels())
        level->getBlockActors().tick(*this);
    mProfiler.endSection(ProfilerSection::BlockActors);

    mProfiler.beginSection(ProfilerSection::ItemActors);
    ItemActorHandler::tickItemActors(*this);
    mProfiler.endSection(ProfilerSection::ItemActors);

    tickActors();
    runQueuedActorCommands();
    updateActorVisibility();
    updatePlayerVisibility();

    for (auto &entry: mPlayers)
        broadcastPlayerMove(entry.second);

    for (World *world: mWorlds.getWorlds()) {
        _processChunkUnloads(world->getOverworld());
        _tickDimension(*world, world->getLevel(DimensionType::Nether));
        _tickDimension(*world, world->getLevel(DimensionType::TheEnd));
        for (Level *level: world->getLevels()) {
            if (Dimension::isCustom(level->getDimensionType()))
                _tickDimension(*world, *level);
        }
    }

    const std::vector<Level *> levels = getLevels();

    mProfiler.beginSection(ProfilerSection::Redstone);
    for (Level *level: levels) {
        level->tickBlockUpdates();
        RedstoneSystem::tick(*this, *level);
        PistonSystem::tick(*this, *level);
        CommandBlockSystem::tickCommandBlocks(*this, *level);
    }
    mProfiler.endSection(ProfilerSection::Redstone);

    mProfiler.beginSection(ProfilerSection::Fire);
    for (Level *level: levels) {
        FireSystem::tick(*this, *level);
        RandomTickSystem::tick(*this, *level);
        PrecipitationSystem::tick(*this, *level);
    }
    mProfiler.endSection(ProfilerSection::Fire);

    for (Level *level: levels)
        mNaturalSpawner.tick(*this, *level);

    mProfiler.beginSection(ProfilerSection::Announcement);
    _updateServerAnnouncement();
    mProfiler.endSection(ProfilerSection::Announcement);

    mScriptEngine.tick(mCurrentTick);

    std::vector<std::pair<NetworkIdentifier, std::string>> timedOut;
    std::vector<NetworkIdentifier> idle;
    std::vector<std::pair<NetworkIdentifier, int>> unanswered;
    const bool unofficialClientsAllowed = mProperties.getAllowUnofficialClients();
    for (auto &entry: mPlayers) {
        const int failedCheck = unofficialClientsAllowed ? 0 : mClientProbes.tick(*this, entry.second, mCurrentTick);
        if (failedCheck != 0)
            unanswered.emplace_back(entry.first, failedCheck);
        entry.second.getAcknowledgements().flush(*this, entry.first);
        if (entry.second.getAcknowledgements().hasTimedOut(mCurrentTick))
            timedOut.emplace_back(entry.first, entry.second.localize("falcon.disconnect.timedOut"));
        else if (mPlayerIdleTimeoutTicks > 0 && entry.second.isSpawned()
                 && mCurrentTick - entry.second.getLastActiveTick() >= mPlayerIdleTimeoutTicks)
            idle.push_back(entry.first);
    }
    for (const auto &entry: timedOut)
        _disconnect(entry.first, DisconnectReason::Timeout, entry.second);
    for (const NetworkIdentifier &entry: idle)
        _disconnect(entry, DisconnectReason::KickedForIdle, IDLE_MESSAGE);
    for (const auto &entry: unanswered) {
        const ServerPlayer *refused = _getPlayer(entry.first);
        ServerTelemetry::getInstance().countCheck(entry.second);
        ServerTelemetry::getInstance().countRefusal(entry.second, refused != nullptr ? refused->getBuildPlatform() : 0);
        _rejectBadPacket(entry.first, "client check " + std::to_string(entry.second));
    }

    if (!unofficialClientsAllowed) {
        // a second or so: the verdict often comes back before the login that names the platform
        static const int64_t REMOVAL_WAIT_TICKS = 40;

        for (const auto &verdict: ClientAttestation::getInstance().takeVerdicts()) {
            if (verdict.second > 0) {
                mPendingRemovals.push_back({verdict.first, 100 + verdict.second, mCurrentTick + REMOVAL_WAIT_TICKS});
            } else {
                ServerTelemetry::getInstance().countCheck(1000 - verdict.second);
                LOG_INFO(LogAreaID::Network, "Client note %d for %s", 100 - verdict.second,
                         verdict.first.getAddress().c_str());
            }
        }

        for (size_t index = 0; index < mPendingRemovals.size();) {
            const PendingRemoval removal = mPendingRemovals[index];
            const ServerPlayer *refused = _getPlayer(removal.mId);
            const bool named = refused != nullptr && refused->getLoggedInTick() != 0;
            if (!named && mCurrentTick < removal.mDeadline) {
                index++;
                continue;
            }

            mPendingRemovals.erase(mPendingRemovals.begin() + (long) index);
            ServerTelemetry::getInstance().countCheck(removal.mCheck);
            ServerTelemetry::getInstance().countRefusal(removal.mCheck, named ? refused->getBuildPlatform() : 0);
            _rejectBadPacket(removal.mId, "client check " + std::to_string(removal.mCheck));
        }
    }

    uint32_t loadedChunks = 0;
    uint32_t pendingChunkTasks = 0;
    uint32_t fluidsProcessed = 0;
    uint32_t fluidsScheduled = 0;
    for (World *world: mWorlds.getWorlds()) {
        Level &overworld = world->getOverworld();
        const ChunkWorker *chunkWorker = overworld.getChunkWorker();
        loadedChunks += (uint32_t) overworld.getLoadedChunkCount();
        pendingChunkTasks += chunkWorker == nullptr ? 0 : (uint32_t) chunkWorker->getPendingTaskCount();
        fluidsProcessed += (uint32_t) overworld.getLastFluidProcessedCount();
        fluidsScheduled += (uint32_t) overworld.getScheduledFluidCount();
    }
    ServerTelemetry::getInstance().setPlayers((int) mPlayers.size());
    mProfiler.endTick((uint32_t) mPlayers.size(), loadedChunks, pendingChunkTasks, fluidsProcessed, fluidsScheduled);

    const double elapsedMs =
            std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tickStart).count();

    ServerTelemetry::getInstance().recordTick(elapsedMs);
    mTickDurationSamples.push_back(elapsedMs);
    if (mTickDurationSamples.size() > TICK_SAMPLE_COUNT)
        mTickDurationSamples.pop_front();
}

void ServerNetworkHandler::_updateJoinBurst() {
    // bounded in time: a client stalling before its spawn must not slow the others for good
    mJoinBurst = false;
    for (const auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (player.getLoginState() >= ServerPlayer::LoginState::LoggedIn && !player.hasSpawnChunksReady()
            && mCurrentTick - player.getLoggedInTick() < JOIN_BURST_TICKS) {
            mJoinBurst = true;
            return;
        }
    }
}

void ServerNetworkHandler::_queueSpawnWarmup() {
    const Vector3i spawn = mWorlds.getDefault().getOverworld().getSpawnPosition();
    const int32_t centerX = spawn.x >> 4;
    const int32_t centerZ = spawn.z >> 4;

    std::vector<std::pair<int32_t, int64_t>> chunks;
    for (int32_t dx = -SPAWN_WARMUP_RADIUS; dx <= SPAWN_WARMUP_RADIUS; ++dx) {
        for (int32_t dz = -SPAWN_WARMUP_RADIUS; dz <= SPAWN_WARMUP_RADIUS; ++dz) {
            const int32_t distance = dx * dx + dz * dz;
            if (distance <= SPAWN_WARMUP_RADIUS * SPAWN_WARMUP_RADIUS)
                chunks.emplace_back(distance, ChunkStreamHandler::packChunk(centerX + dx, centerZ + dz));
        }
    }

    // nearest first: generation backpressure admits a few per tick
    std::sort(chunks.begin(), chunks.end());
    mSpawnWarmup.clear();
    for (const auto &chunk: chunks)
        mSpawnWarmup.push_back(chunk.second);
}

void ServerNetworkHandler::_tickSpawnWarmup() {
    if (mSpawnWarmup.empty())
        return;

    Level &level = mWorlds.getDefault().getOverworld();
    std::vector<int64_t> pending;
    for (const int64_t hash: mSpawnWarmup) {
        const int32_t chunkX = ChunkStreamHandler::unpackChunkX(hash);
        const int32_t chunkZ = ChunkStreamHandler::unpackChunkZ(hash);
        if (level.isChunkPopulated(chunkX, chunkZ))
            continue;

        level.requestChunkAsync(chunkX, chunkZ);
        pending.push_back(hash);
    }
    mSpawnWarmup.swap(pending);
}

void ServerNetworkHandler::pumpLoginEvents() {
    if (!mIsListening || mNetworkHandler == nullptr)
        return;

    mNetworkHandler->runEventsFor([this](const NetworkIdentifier &id) {
        const ServerPlayer *player = _getPlayer(id);
        return player == nullptr || player->getLoginState() < ServerPlayer::LoginState::StartGameSent;
    });
}

void ServerNetworkHandler::_updateServerAnnouncement() {
    mAnnouncement.mCurrentPlayers = getActivePlayerCount();
}

bool ServerNetworkHandler::onValidateIncomingConnection(const NetworkIdentifier &id) {
    (void) id;
    return true;
}

bool ServerNetworkHandler::isServerFull(const NetworkIdentifier &joining) const {
    int loggedIn = 0;
    for (const auto &entry: mPlayers) {
        if (entry.first == joining)
            continue;

        if (entry.second.getLoginState() >= ServerPlayer::LoginState::LoggedIn)
            loggedIn++;
    }

    return loggedIn >= mMaxPlayers;
}

void ServerNetworkHandler::onNewIncomingConnection(const NetworkIdentifier &id, uint16_t mtuSize) {
    LOG_INFO(LogAreaID::Network, "Player connected: %s", id.toString().c_str());

    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_CONNECTION_OPEN)) {
        const bool rakNet = id.getType() == NetworkIdentifier::Type::RakNet;
        PluginEvent open;
        open.mType = FALCON_EVENT_CONNECTION_OPEN;
        open.mCancellable = true;
        open.mAddress = id.getAddress();
        open.mTransport = rakNet ? "raknet" : "nethernet";
        open.mClientGuid = rakNet ? id.getGuid().g : 0;
        open.mMtuSize = mtuSize;
        plugins->dispatch(open);
        if (open.mCancelled) {
            LOG_INFO(LogAreaID::Network, "Connection from %s was refused by a plugin", id.getAddress().c_str());
            mNetworkHandler->closeConnection(id);
        }
    }
}

void ServerNetworkHandler::onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                                              const std::string &message) {
    (void) message;

    mRateLimiters.erase(id);
    mClientProbes.forget(id);
    for (size_t index = 0; index < mPendingRemovals.size();) {
        if (mPendingRemovals[index].mId == id)
            mPendingRemovals.erase(mPendingRemovals.begin() + (long) index);
        else
            index++;
    }

    ServerPlayer *player = _getPlayer(id);
    World *playerWorld = player != nullptr ? &getWorldFor(*player) : nullptr;
    const std::string playerName = player != nullptr && !player->getName().empty() ? player->getName() : id.toString();
    if (player != nullptr && !player->getName().empty()) {
        if (player->isSpawned())
            mScriptEngine.beforePlayerLeave(*player);
        stopSleep(*player);
        if (player->isRiding())
            RideSystem::dismount(*this, *player, false);
        RideSystem::ejectAll(*this, *player);
        if (player->isSpawned())
            InventoryHandler::resetInventory(*this, *player);
        _savePlayerData(*player);
        EnderChestInventoryStore::getInstance().remove(player->getUniqueId());

        if (player->isSpawned()) {
            broadcastTranslation("multiplayer.player.left", {player->getName()});
            _removeFromPlayerList(*player);

            PlayerLeaveAfterEvent leaveEvent(player->getName(), player);
            mEventBus.after().mPlayerLeave.emit(leaveEvent);
        }
    }

    if (player != nullptr) {
        despawnPlayerForViewers(*player);
        getLevelFor(*player).unregisterAllChunkLoaders(player->getRuntimeId());
    }

    auto pendingForms = mModalFormCallbacks.find(id);
    if (pendingForms != mModalFormCallbacks.end()) {
        auto callbacks = std::move(pendingForms->second);
        mModalFormCallbacks.erase(pendingForms);
        if (player != nullptr) {
            for (auto &entry: callbacks)
                entry.second(*player, std::string(), ModalFormOutcome::PlayerQuit);
        }
    }
    _endDataDrivenScreens(id, DataDrivenScreenOutcome::PlayerLeft);
    erasePlayer(id);
    // an awake player leaving can bring the remaining sleepers up to the threshold
    if (playerWorld != nullptr)
        _updateSleepingPlayerList(*playerWorld);
    LOG_INFO(LogAreaID::Network, "%s disconnected, reason: %s", playerName.c_str(), toString(reason));
}

int64_t ServerNetworkHandler::getUptimeSeconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now() - mStartTime).count();
}

double ServerNetworkHandler::getTicksPerSecond() const {
    if (mTickStartSamples.size() < 2)
        return 20.0;

    const double span = std::chrono::duration<double>(mTickStartSamples.back() - mTickStartSamples.front()).count();
    if (span <= 0.0)
        return 20.0;

    const double tps = (double) (mTickStartSamples.size() - 1) / span;
    return tps > 20.0 ? 20.0 : tps;
}

double ServerNetworkHandler::getMillisecondsPerTick() const {
    if (mTickDurationSamples.empty())
        return 0.0;

    double total = 0.0;
    for (double sample: mTickDurationSamples)
        total += sample;

    return total / (double) mTickDurationSamples.size();
}

double ServerNetworkHandler::getPeakMillisecondsPerTick() const {
    double peak = 0.0;
    for (double sample: mTickDurationSamples) {
        if (sample > peak)
            peak = sample;
    }

    return peak;
}

void ServerNetworkHandler::sendModalForm(ServerPlayer &player, uint32_t formId, const std::string &formData,
                                         ModalFormCallback callback) {
    if (!player.isSpawned() || formData.empty() || formData.size() > 1024 * 1024 || !JsonValidator(formData).parse())
        return;

    auto &callbacks = mModalFormCallbacks[player.getNetworkIdentifier()];
    if (callback)
        callbacks[formId] = std::move(callback);
    else
        callbacks.erase(formId);

    ModalFormRequestPacket request;
    request.mFormId = formId;
    request.mFormData = formData;
    mNetworkHandler->send(player.getNetworkIdentifier(), request, mCodecContext);
}

void ServerNetworkHandler::closeAllForms(ServerPlayer &player) {
    if (!player.isSpawned())
        return;

    ClientboundCloseFormPacket packet;
    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);

    // no form id closes every data-driven screen; each comes back as ProgrammaticCloseAll
    ClientboundDataDrivenUICloseScreenPacket closeScreens;
    mNetworkHandler->send(player.getNetworkIdentifier(), closeScreens, mCodecContext);
}

void ServerNetworkHandler::setMaxPlayers(int maxPlayers) {
    mMaxPlayers = maxPlayers;
    mAnnouncement.mMaxPlayers = maxPlayers;
}

void ServerNetworkHandler::onDataReceived(const NetworkIdentifier &id, const std::string &data) {
    if (!_allowPacket(id, RateLimitedPacket::Inbound))
        return;

    try {
        std::string received;
        const std::string *source = &data;
        if (mPluginManager->hasSubscribers(FALCON_EVENT_DATA_PACKET_RECEIVE)) {
            received = data;
            if (!PluginPackets::onReceive(*mPluginManager, id, received))
                return;
            source = &received;
        }

        ReadOnlyBinaryStream stream(*source);

        unsigned char senderSubId;
        unsigned char clientSubId;
        const MinecraftPacketIds packetId = Packet::peekId(stream, senderSubId, clientSubId);
        const LoginGate gate = _checkLoginState(id, packetId);
        if (gate == LoginGate::Reject) {
            _rejectBadPacket(id, "packet " + std::to_string((int) packetId) + " is not expected at this login step");
            return;
        }
        if (gate == LoginGate::Ignore)
            return;

        if (mSimulationPaused) {
            switch (packetId) {
                case MinecraftPacketIds::RequestNetworkSettings:
                case MinecraftPacketIds::Login:
                case MinecraftPacketIds::ClientToServerHandshake:
                case MinecraftPacketIds::ResourcePackClientResponse:
                case MinecraftPacketIds::ResourcePackChunkRequest:
                case MinecraftPacketIds::ClientCacheStatus:
                case MinecraftPacketIds::SetLocalPlayerAsInitialized:
                case MinecraftPacketIds::RequestChunkRadius:
                case MinecraftPacketIds::SubChunkRequest:
                case MinecraftPacketIds::NetworkStackLatency:
                case MinecraftPacketIds::PacketViolationWarning:
                    break;
                default:
                    return;
            }
        }

        // the shared codec builds clientbound packets too; decoding those exposes unhardened client-side decoders
        if (!_isServerboundPacket(packetId)) {
            LOG_TRACE(LogAreaID::Network, "Dropping non-serverbound packet id %d from %s", (int) packetId,
                      id.getAddress().c_str());
            return;
        }

        const PacketCodecContext &context = getCodecContext(id);
        const ProtocolCodec &codec = context.getCodec();
        std::shared_ptr<Packet> packet = codec.createPacket(packetId);
        if (!packet) {
            LOG_TRACE(LogAreaID::Network, "Unhandled packet id %d from %s", (int) packetId, id.getAddress().c_str());
            return;
        }

        packet->mSenderSubId = senderSubId;
        packet->mClientSubId = clientSubId;

        {
            ProfilerScopedSection decodeSection(mProfiler, ProfilerSection::NetworkDecode, true);
            codec.read(*packet, stream, context);
        }

        {
            ProfilerScopedSection handleSection(mProfiler, ProfilerSection::NetworkHandlePacket, true);
            packet->handle(id, *this);
        }
    } catch (const BinaryDataException &exception) {
        // a decode or handler failure ends only this connection, never the server
        LOG_WARN(LogAreaID::Network, "Malformed packet from %s: %s", id.getAddress().c_str(), exception.what());
        _disconnect(id, DisconnectReason::BadPacket, "disconnectionScreen.unexpectedPacket");
    } catch (const std::exception &exception) {
        LOG_ERROR(LogAreaID::Network, "Unhandled exception while processing a packet from %s: %s",
                  id.getAddress().c_str(), exception.what());
        _disconnect(id, DisconnectReason::Unknown, "disconnectionScreen.internalError.cantConnect");
    } catch (...) {
        LOG_ERROR(LogAreaID::Network, "Unknown exception while processing a packet from %s", id.getAddress().c_str());
        _disconnect(id, DisconnectReason::Unknown, "disconnectionScreen.internalError.cantConnect");
    }
}

bool ServerNetworkHandler::_allowPacket(const NetworkIdentifier &id, RateLimitedPacket category) {
    auto limiter = mRateLimiters.find(id);
    if (limiter == mRateLimiters.end()) {
        PacketRateLimiter::Rates rates{};
        rates[(size_t) RateLimitedPacket::Inbound] = mProperties.getMaxInboundPacketsPerSecond();
        rates[(size_t) RateLimitedPacket::Command] = mProperties.getMaxCommandsPerSecond();
        rates[(size_t) RateLimitedPacket::Chat] = mProperties.getMaxChatMessagesPerSecond();
        rates[(size_t) RateLimitedPacket::FormResponse] = mProperties.getMaxFormResponsesPerSecond();
        rates[(size_t) RateLimitedPacket::Movement] = mProperties.getMaxMovementPacketsPerSecond();
        rates[(size_t) RateLimitedPacket::ViolationWarning] = 1;
        limiter = mRateLimiters.emplace(id, PacketRateLimiter(rates)).first;
    }

    if (limiter->second.tryAcquire(category))
        return true;

    // only the overall inbound budget disconnects, once; other categories drop the packet:
    // a chat or command burst does not kick
    if (category == RateLimitedPacket::Inbound && limiter->second.markFlooded()) {
        LOG_WARN(LogAreaID::Network, "Disconnecting %s for packet flooding", id.getAddress().c_str());
        _disconnect(id, DisconnectReason::BadPacket, "disconnectionScreen.unexpectedPacket");
    }

    return false;
}

const std::unordered_set<MinecraftPacketIds> &ServerNetworkHandler::_serverboundPackets() {
    // login/cache packets sent before a player exists, plus one entry per handle() override;
    // anything else is dropped before createPacket; _validateServerboundPackets() checks it at startup
    static const std::unordered_set<MinecraftPacketIds> allowed = {
            MinecraftPacketIds::RequestNetworkSettings,
            MinecraftPacketIds::Login,
            MinecraftPacketIds::ClientToServerHandshake,
            MinecraftPacketIds::ClientCacheStatus,
            MinecraftPacketIds::ResourcePackClientResponse,
            MinecraftPacketIds::ResourcePackChunkRequest,
            MinecraftPacketIds::SetLocalPlayerAsInitialized,
            MinecraftPacketIds::NetworkStackLatency,
            MinecraftPacketIds::PlayerAuthInput,
            MinecraftPacketIds::CompletedUsingItem,
            MinecraftPacketIds::CommandRequest,
            MinecraftPacketIds::Text,
            MinecraftPacketIds::PacketViolationWarning,
            MinecraftPacketIds::RequestChunkRadius,
            MinecraftPacketIds::SubChunkRequest,
            MinecraftPacketIds::RequestAbility,
            MinecraftPacketIds::MobEquipment,
            MinecraftPacketIds::PlayerHotbar,
            MinecraftPacketIds::ItemStackRequest,
            MinecraftPacketIds::CraftingEvent,
            MinecraftPacketIds::InventoryTransaction,
            MinecraftPacketIds::ContainerClose,
            MinecraftPacketIds::Interact,
            MinecraftPacketIds::PlayerAction,
            MinecraftPacketIds::Respawn,
            MinecraftPacketIds::SetPlayerGameType,
            MinecraftPacketIds::BlockActorData,
            MinecraftPacketIds::CommandBlockUpdate,
            MinecraftPacketIds::BlockPickRequest,
            MinecraftPacketIds::ActorPickRequest,
            MinecraftPacketIds::Emote,
            MinecraftPacketIds::ModalFormResponse,
            MinecraftPacketIds::ServerboundDataStore,
            MinecraftPacketIds::ServerboundDataDrivenScreenClosed,
            MinecraftPacketIds::UpdateClientOptions,
            MinecraftPacketIds::MapInfoRequest,
            MinecraftPacketIds::MapCreateLockedCopy,
            MinecraftPacketIds::Animate,
            MinecraftPacketIds::LevelSoundEvent,
            MinecraftPacketIds::SetDefaultGameType,
            MinecraftPacketIds::SetDifficulty,
            MinecraftPacketIds::RequestPermissions,
            MinecraftPacketIds::BookEdit,
            MinecraftPacketIds::LecternUpdate,
            MinecraftPacketIds::PositionTrackingDBClientRequest,
            MinecraftPacketIds::PlayerToggleCrafterSlotRequest,
            MinecraftPacketIds::PlayerSkin,
    };

    return allowed;
}

bool ServerNetworkHandler::_isServerboundPacket(MinecraftPacketIds packetId) {
    const std::unordered_set<MinecraftPacketIds> &allowed = _serverboundPackets();
    return allowed.find(packetId) != allowed.end();
}

void ServerNetworkHandler::_validateServerboundPackets() const {
    // a handled packet the codec cannot build would be dropped silently: log it at startup
    const ProtocolCodec &codec = mCodecContext.getCodec();
    for (const MinecraftPacketIds id: _serverboundPackets()) {
        if (!codec.supports(id))
            LOG_ERROR(LogAreaID::Network, "Serverbound packet %d is in the allowlist but not in the codec", (int) id);
    }
}

ServerNetworkHandler::LoginGate ServerNetworkHandler::_checkLoginState(const NetworkIdentifier &id,
                                                                       MinecraftPacketIds packetId) {
    using LoginState = ServerPlayer::LoginState;

    if (packetId == MinecraftPacketIds::PacketViolationWarning)
        return LoginGate::Accept;

    const ServerPlayer *player = _getPlayer(id);
    if (packetId == MinecraftPacketIds::RequestNetworkSettings)
        return player == nullptr ? LoginGate::Accept : LoginGate::Reject;

    bool expected;
    switch (packetId) {
        case MinecraftPacketIds::Login:
            expected = player != nullptr && player->getLoginState() == LoginState::NetworkSettingsSent;
            break;
        case MinecraftPacketIds::ClientToServerHandshake:
            expected = player != nullptr && player->getLoginState() == LoginState::EncryptionHandshake;
            break;
        case MinecraftPacketIds::ResourcePackClientResponse:
        case MinecraftPacketIds::ResourcePackChunkRequest:
            expected = player != nullptr && player->getLoginState() == LoginState::ResourcePacksSent;
            break;
        case MinecraftPacketIds::SetLocalPlayerAsInitialized:
            if (player != nullptr && player->getLoginState() == LoginState::Spawned)
                return LoginGate::Ignore;
            expected = player != nullptr && player->getLoginState() == LoginState::StartGameSent;
            break;
        default:
            return player != nullptr && player->getLoginState() >= LoginState::StartGameSent ? LoginGate::Accept
                                                                                             : LoginGate::Ignore;
    }
    return expected ? LoginGate::Accept : LoginGate::Reject;
}

const PacketCodecContext &ServerNetworkHandler::getCodecContext(const NetworkIdentifier &id) {
    const ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || player->getCodec() == nullptr)
        return mCodecContext;

    const std::shared_ptr<const ProtocolCodec> &codec = player->getCodec();
    std::unique_ptr<PacketCodecContext> &context = mProtocolContexts[codec->getProtocolVersion()];
    if (context == nullptr) {
        context = std::make_unique<PacketCodecContext>(mBlockDefinitions, mItemDefinitions, codec);
        context->setItemNetworkIds(_itemNetworkIdsFor(codec->getProtocolVersion()));
        context->setBlockNetworkIds(BlockNetworkIdTable::getNetworkIds(codec->getProtocolVersion()));
    }
    return *context;
}

std::shared_ptr<const ItemNetworkIdMap> ServerNetworkHandler::_itemNetworkIdsFor(int protocol) const {
    const std::shared_ptr<const ItemNetworkIdMap> vanilla = ItemNetworkIdTable::getNetworkIds(protocol);
    if (vanilla == nullptr)
        return nullptr;

    std::shared_ptr<ItemNetworkIdMap> map = std::make_shared<ItemNetworkIdMap>(*vanilla);
    std::vector<int32_t> colliding;
    for (const std::shared_ptr<ItemDefinition> &definition: mItemDefinitions.getAll()) {
        if (ItemNetworkIdTable::find(definition->getIdentifier()) != nullptr)
            continue;

        if (map->toCurrent(definition->getRuntimeId()) == 0)
            map->add(definition->getRuntimeId(), definition->getRuntimeId());
        else
            colliding.push_back(definition->getRuntimeId());
    }

    int32_t freeId = 1;
    for (const int32_t runtimeId: colliding) {
        while (map->toCurrent(freeId) != 0)
            ++freeId;
        map->add(runtimeId, freeId);
    }
    return map;
}

ServerPlayer *ServerNetworkHandler::_getPlayer(const NetworkIdentifier &id) {
    auto it = mPlayers.find(id);
    return it == mPlayers.end() ? nullptr : &it->second;
}

void ServerNetworkHandler::_disconnect(const NetworkIdentifier &id, DisconnectReason reason,
                                       const std::string &message) {
    // a player who never made it into the world: the reason is what an operator would ask about
    const ServerPlayer *leaving = _getPlayer(id);
    if (leaving == nullptr || !leaving->isSpawned())
        ServerTelemetry::getInstance().countFailure(ServerTelemetry::FailureStage::Login, (int) reason);

    DisconnectPacket disconnect;
    disconnect.mReason = (int32_t) reason;
    disconnect.mMessageSkipped = false;
    disconnect.mKickMessage = message;

    mNetworkHandler->send(id, disconnect, mCodecContext);
    mNetworkHandler->flush(id);
    // Disconnect is advisory: a hostile client can ignore it; commands are queued, the message goes out first
    mNetworkHandler->closeConnection(id);
}

void ServerNetworkHandler::_rejectBadPacket(const NetworkIdentifier &id, const std::string &reason) {
    LOG_WARN(LogAreaID::Network, "Bad packet from %s: %s", id.getAddress().c_str(), reason.c_str());
    _disconnect(id, DisconnectReason::BadPacket, "disconnectionScreen.unexpectedPacket");
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const RequestNetworkSettingsPacket &packet) {
    LoginHandler::handleRequestNetworkSettings(*this, id, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const LoginPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr) {
        _disconnect(id, DisconnectReason::UnexpectedPacket, "disconnectionScreen.unexpectedPacket");
        return;
    }

    // the server listens before the first auth keys arrive: checked now, a signed login would read as unsigned
    if (AuthKeyProvider::getInstance().isFetchingFirstKeys()) {
        for (LoginAwaitingKeys &waiting: mLoginsAwaitingKeys) {
            if (waiting.mId == id) {
                waiting.mPacket = packet;
                return;
            }
        }
        mLoginsAwaitingKeys.push_back(LoginAwaitingKeys{id, packet, std::chrono::steady_clock::now()});
        return;
    }

    LoginHandler::handleLogin(*this, id, *player, packet);
}

void ServerNetworkHandler::_retryLoginsAwaitingKeys() {
    if (mLoginsAwaitingKeys.empty())
        return;

    // a fetch with no network can take its full HTTP timeouts: past the wait, checked without the keys
    const bool fetching = AuthKeyProvider::getInstance().isFetchingFirstKeys();
    const std::chrono::steady_clock::time_point expired = std::chrono::steady_clock::now() - LOGIN_KEY_WAIT;
    std::vector<LoginAwaitingKeys> ready;
    for (auto it = mLoginsAwaitingKeys.begin(); it != mLoginsAwaitingKeys.end();) {
        if (fetching && it->mReceived > expired) {
            ++it;
            continue;
        }
        ready.push_back(std::move(*it));
        it = mLoginsAwaitingKeys.erase(it);
    }

    for (const LoginAwaitingKeys &login: ready) {
        // disconnected while waiting: nobody to log in
        ServerPlayer *player = _getPlayer(login.mId);
        if (player != nullptr)
            LoginHandler::handleLogin(*this, login.mId, *player, login.mPacket);
    }
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ClientToServerHandshakePacket &packet) {
    (void) packet;

    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr) {
        _disconnect(id, DisconnectReason::UnexpectedPacket, "disconnectionScreen.unexpectedPacket");
        return;
    }

    LoginHandler::handleClientToServerHandshake(*this, id, *player);
}

void ServerNetworkHandler::onConnectionFailed(const NetworkIdentifier &id) {
    LOG_WARN(LogAreaID::Network, "%s sent a batch with an invalid encryption checksum", id.getAddress().c_str());
    _disconnect(id, DisconnectReason::BadPacket, "disconnectionScreen.unexpectedPacket");
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ResourcePackClientResponsePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    LoginHandler::handleResourcePackClientResponse(*this, id, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ResourcePackChunkRequestPacket &packet) {
    LoginHandler::handleResourcePackChunkRequest(*this, id, packet);
}

void ServerNetworkHandler::_sendStartGame(ServerPlayer &player) {
    LoginHandler::sendStartGame(*this, player);
}

void ServerNetworkHandler::_sendEntityData(ServerPlayer &player) {
    SetActorDataPacket entityData;
    entityData.mRuntimeActorId = (int64_t) player.getRuntimeId();
    entityData.mTick = 0;
    entityData.mMetadata = _buildPlayerData(player);

    std::vector<NetworkIdentifier> recipients;
    for (const auto &entry: mPlayers) {
        if (entry.first == player.getNetworkIdentifier() || entry.second.isSpawned())
            recipients.push_back(entry.first);
    }
    mNetworkHandler->sendToMany(recipients, entityData, mCodecContext);
}

EntityDataMap ServerNetworkHandler::_buildPlayerData(ServerPlayer &player) {
    EntityDataMap metadata;

    EntityDataEntry flags;
    flags.mId = ActorFlags::FLAGS_DATA_ID;
    flags.mFormat = EntityDataFormat::Long;
    flags.mLongValue = player.getFlags().getLowBits();
    metadata.mEntries.push_back(flags);

    EntityDataEntry flags2;
    flags2.mId = ActorFlags::FLAGS_2_DATA_ID;
    flags2.mFormat = EntityDataFormat::Long;
    flags2.mLongValue = player.getFlags().getHighBits();
    metadata.mEntries.push_back(flags2);


    EntityDataEntry flags3;

    flags3.mId = ActorFlags::FLAGS_3_DATA_ID;

    flags3.mFormat = EntityDataFormat::Long;

    flags3.mLongValue = player.getFlags().getExtendedBits();
metadata.mEntries.push_back(flags3);


    EntityDataEntry nameplateDistance;

    nameplateDistance.mId = ActorFlags::NAMEPLATE_RENDER_DISTANCE_DATA_ID;

    nameplateDistance.mFormat = EntityDataFormat::Float;

    nameplateDistance.mFloatValue = player.getNameplateRenderDistance();

    metadata.mEntries.push_back(nameplateDistance);

    EntityDataEntry playerFlags;
    playerFlags.mId = ActorFlags::PLAYER_FLAGS_DATA_ID;
    playerFlags.mFormat = EntityDataFormat::Byte;
    playerFlags.mByteValue = player.isSleeping() ? ActorFlags::PLAYER_FLAG_SLEEP : 0;
    metadata.mEntries.push_back(playerFlags);

    EntityDataEntry air;
    air.mId = ActorFlags::AIR_SUPPLY_DATA_ID;
    air.mFormat = EntityDataFormat::Short;
    air.mShortValue = (int16_t) player.getAirSupply();
    metadata.mEntries.push_back(air);

    EntityDataEntry maxAir;
    maxAir.mId = ActorFlags::AIR_SUPPLY_MAX_DATA_ID;
    maxAir.mFormat = EntityDataFormat::Short;
    maxAir.mShortValue = (int16_t) ServerPlayer::MAX_AIR_SUPPLY;
    metadata.mEntries.push_back(maxAir);

    EntityDataEntry visibleEffects;
    visibleEffects.mId = ActorFlags::VISIBLE_MOB_EFFECTS_DATA_ID;
    visibleEffects.mFormat = EntityDataFormat::Long;
    visibleEffects.mLongValue = player.getVisibleEffectsData();
    metadata.mEntries.push_back(visibleEffects);

    if (player.isSleeping()) {
        EntityDataEntry bedPosition;
        bedPosition.mId = ActorFlags::BED_POSITION_DATA_ID;
        bedPosition.mFormat = EntityDataFormat::Vector3i;
        bedPosition.mVector3iValue = player.getSleepingPosition();
        metadata.mEntries.push_back(bedPosition);
    }

    return metadata;
}

bool ServerNetworkHandler::isAllowListed(ServerPlayer &player) {
    if (!mProperties.getAllowList())
        return true;

    return mAllowList.isAllowed(player.getName(), player.getXuid());
}

void ServerNetworkHandler::setAllowListEnabled(bool enabled) {
    mProperties.setProperty("allow-list", enabled ? "true" : "false");

    if (enabled)
        kickNotAllowListedPlayers();
}

void ServerNetworkHandler::kickNotAllowListedPlayers() {
    for (auto &entry: mPlayers) {
        if (entry.second.getLoginState() < ServerPlayer::LoginState::LoggedIn)
            continue;

        if (!isAllowListed(entry.second))
            _disconnect(entry.first, DisconnectReason::NotAllowed, NOT_ALLOW_LISTED_MESSAGE);
    }
}

void ServerNetworkHandler::setPlayerOp(ServerPlayer &player, bool isOp) {
    player.setOp(isOp);
    _sendAbilities(player);

    if (player.isSpawned())
        _sendAvailableCommands(player);

    if (isOp)
        player.sendTranslation("commands.op.success", {player.getName()});
    else
        player.sendTranslation("commands.deop.success", {player.getName()});
}

void ServerNetworkHandler::queueConsoleCommand(const std::string &commandLine) {
    std::lock_guard<std::mutex> lock(mConsoleQueueMutex);
    mConsoleQueue.push(commandLine);
}

void ServerNetworkHandler::executeConsoleCommand(std::string commandLine) {
    PluginEvent event;
    event.mType = FALCON_EVENT_SERVER_COMMAND;
    event.mCancellable = true;
    event.mMessage = &commandLine;
    mPluginManager->dispatch(event);
    if (!event.mCancelled) {
        ServerCommandOrigin sender(this);
        mCommands.dispatch(sender, commandLine);
    }
}

void ServerNetworkHandler::postToMainThread(std::function<void()> task) {
    if (task == nullptr)
        return;

    std::lock_guard<std::mutex> lock(mMainThreadTaskMutex);
    mMainThreadTasks.push_back(std::move(task));
}

void ServerNetworkHandler::_sendAbilities(ServerPlayer &player) {
    LoginHandler::sendAbilities(*this, player);
}

void ServerNetworkHandler::_sendBiomeDefinitions(ServerPlayer &player) {
    LoginHandler::sendBiomeDefinitions(*this, player);
}

void ServerNetworkHandler::_sendItemComponents(ServerPlayer &player) {
    LoginHandler::sendItemComponents(*this, player);
}

void ServerNetworkHandler::_sendActorIdentifiers(ServerPlayer &player) {
    LoginHandler::sendActorIdentifiers(*this, player);
}

void ServerNetworkHandler::_buildCraftingData() {
    LoginHandler::buildCraftingData(*this);
}

void ServerNetworkHandler::_sendCraftingData(ServerPlayer &player) {
    LoginHandler::sendCraftingData(*this, player);
}

void ServerNetworkHandler::_buildCreativeContent() {
    LoginHandler::buildCreativeContent(*this);
}

void ServerNetworkHandler::_sendCreativeContent(ServerPlayer &player) {
    LoginHandler::sendCreativeContent(*this, player);
}

void ServerNetworkHandler::_sendInventory(ServerPlayer &player) {
    InventoryHandler::sendInventory(*this, player);
}

void ServerNetworkHandler::_sendAvailableCommands(ServerPlayer &player) {
    LoginHandler::sendAvailableCommands(*this, player);
}

void ServerNetworkHandler::_sendAttributes(ServerPlayer &player, bool movementChange) {
    LoginHandler::sendAttributes(*this, player, movementChange);
}

void ServerNetworkHandler::_broadcastEntityEvent(const Actor &entity, uint8_t eventId) {
    _broadcastEntityEvent(entity, eventId, 0);
}

void ServerNetworkHandler::_broadcastEntityEvent(const Actor &entity, uint8_t eventId, int32_t eventData) {
    ActorEventPacket event;
    event.mRuntimeActorId = entity.getRuntimeId();
    event.mEventId = eventId;
    event.mEventData = eventData;
    event.mHasFirePosition = false;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.second.getNetworkIdentifier(), event, mCodecContext);
    }
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PlayerActionPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    switch (packet.mAction) {
        case PlayerActionType::StartBreak:
        case PlayerActionType::BlockContinueDestroy:
            // a repeat on the block being broken keeps its progress; a ContinueBreak only marked a new block
            if (player->hasLastBlockAttacked() && player->getLastBlockAttacked() == packet.mBlockPosition
                && player->isBreakingBlock() && player->getBreakingBlockPosition() == packet.mBlockPosition)
                return;
            BlockActionHandler::startBreakingBlock(*this, *player, packet.mBlockPosition, packet.mFace);
            player->setLastBlockAttacked(packet.mBlockPosition);
            return;
        case PlayerActionType::ContinueBreak:
            if (player->isBreakingBlock() && player->getBreakingBlockPosition() == packet.mBlockPosition)
                player->setBreakingFace(packet.mFace);
            player->setLastBlockAttacked(packet.mBlockPosition);
            return;
        case PlayerActionType::AbortBreak:
            BlockActionHandler::stopBreakingBlock(*this, *player);
            player->clearLastBlockAttacked();
            return;
        case PlayerActionType::StopBreak:
            // sent right before the finishing destroy: only AbortBreak cancels, the break stays until validated
            player->clearLastBlockAttacked();
            return;
        case PlayerActionType::BlockPredictDestroy:
            BlockActionHandler::completeBreakingBlock(*this, *player, packet.mBlockPosition);
            player->clearLastBlockAttacked();
            return;
        case PlayerActionType::DimensionChangeRequestOrCreativeDestroyBlock:
            if (player->isAwaitingDimensionAck()) {
                onPlayerDimensionChangeAck(*player);
                return;
            }
            if (player->getGameType() == (int32_t) GameType::Creative)
                BlockActionHandler::completeBreakingBlock(*this, *player, packet.mBlockPosition);
            return;
        case PlayerActionType::DimensionChangeSuccess:
            onPlayerDimensionChangeAck(*player);
            return;
        case PlayerActionType::StopSleep:
            stopSleep(*player);
            return;
        case PlayerActionType::StartItemUseOn:
            mScriptEngine.onItemStartUseOn(*player, packet.mBlockPosition, packet.mFace);
            return;
        case PlayerActionType::StopItemUseOn:
            mScriptEngine.onItemStopUseOn(*player, packet.mBlockPosition);
            return;
        default:
            break;
    }

    if (packet.mAction == PlayerActionType::Respawn)
        _respawnPlayer(*player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const RespawnPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    if (packet.mState != RespawnPacket::State::ClientReady)
        return;

    _respawnPlayer(*player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const SetPlayerGameTypePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    const int32_t currentGameType = player->getGameType();
    if (packet.mGamemode == currentGameType)
        return;

    SetPlayerGameTypePacket correction;
    correction.mGamemode = currentGameType;
    mNetworkHandler->send(id, correction, mCodecContext);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const EmotePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || packet.mRuntimeActorId != player->getRuntimeId()
        || packet.mEmoteId.empty() || packet.mEmoteId.size() > 256)
        return;

    mScriptEngine.onPlayerEmote(*player, packet.mEmoteId);

    EmotePacket emote = packet;
    emote.mRuntimeActorId = player->getRuntimeId();
    emote.mXuid = player->getXuid();

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.getNetworkIdentifier() != id)
            mNetworkHandler->send(entry.second.getNetworkIdentifier(), emote, mCodecContext);
    }
}

// the client animates its own swing and paddles: the others only see them through this packet
void ServerNetworkHandler::handle(const NetworkIdentifier &id, const AnimatePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || packet.mRuntimeActorId != player->getRuntimeId())
        return;
    if (packet.mAction != AnimatePacket::Action::SwingArm && packet.mAction != AnimatePacket::Action::RowRight
        && packet.mAction != AnimatePacket::Action::RowLeft)
        return;
    if (packet.mAction != AnimatePacket::Action::SwingArm && !player->isRiding())
        return;

    const Vector3f &position = player->getPosition();
    BlockActionHandler::broadcastToViewers(*this, getLevelFor(*player), position, packet, player);
}

// sounds the client plays itself (attacks, swimming, eating...) reach the players around it
void ServerNetworkHandler::handle(const NetworkIdentifier &id, const LevelSoundEventPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || packet.mSound.empty() || packet.mSound.size() > 256
        || packet.mActorType.size() > 256)
        return;
    // sent to whoever views the sender, not the position it names: a client cannot reach far players with it
    BlockActionHandler::broadcastToViewers(*this, getLevelFor(*player), player->getPosition(), packet, player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const SetDefaultGameTypePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || !player->isOp())
        return;
    const int32_t mode = packet.mGamemode;
    if ((mode < 0 || mode > 2) && mode != (int32_t) GameType::Spectator)
        return;
    setDefaultGameType((GameType) mode);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const SetDifficultyPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || !player->isOp() || packet.mDifficulty > 3)
        return;
    changeDifficulty((Difficulty) packet.mDifficulty);
}

// the pause menu's player permissions: an operator promotes or demotes another player
void ServerNetworkHandler::handle(const NetworkIdentifier &id, const RequestPermissionsPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || !player->isOp())
        return;
    ServerPlayer *target = nullptr;
    for (auto &entry: mPlayers) {
        if (entry.second.getUniqueId() == packet.mUniqueActorId)
            target = &entry.second;
    }
    if (target == nullptr || target == player)
        return;

    const bool op = packet.mPermissions == PlayerPermission::Operator;
    if (op == target->isOp())
        return;
    if (op)
        mOps.addOp(target->getName(), target->getXuid());
    else
        mOps.removeOp(target->getName());
    setPlayerOp(*target, op);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const BookEditPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    BookHandler::handleEdit(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const LecternUpdatePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    LecternHandler::handleUpdate(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PositionTrackingDBClientRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    LodestoneHandler::handleRequest(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PlayerToggleCrafterSlotRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    CrafterHandler::handleToggleSlot(*this, *player, packet);
}

// ServerNetworkHandler::handle(UpdateClientOptionsPacket) (sub_14228D1E0, 26.60): an absent field keeps its value
void ServerNetworkHandler::handle(const NetworkIdentifier &id, const UpdateClientOptionsPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    if (packet.mHasFilterProfanityChange)
        player->setProfanityFilterEnabled(packet.mFilterProfanityChange);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ModalFormResponsePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || packet.mHasFormData == packet.mHasCancelReason)
        return;

    if (!_allowPacket(id, RateLimitedPacket::FormResponse))
        return;

    const bool cancelled = packet.mHasCancelReason;
    if (cancelled && (int) packet.mCancelReason < (int) ModalFormResponsePacket::CancelReason::UserClosed)
        return;
    if (cancelled && (int) packet.mCancelReason > (int) ModalFormResponsePacket::CancelReason::UserBusy)
        return;
    if (!cancelled
        && (packet.mFormData.empty() || packet.mFormData.size() > 1024 * 1024
            || !JsonValidator(packet.mFormData).parse()))
        return;

    auto playerCallbacks = mModalFormCallbacks.find(id);
    if (playerCallbacks == mModalFormCallbacks.end())
        return;

    auto callbackIt = playerCallbacks->second.find(packet.mFormId);
    if (callbackIt == playerCallbacks->second.end())
        return;

    ModalFormCallback callback = std::move(callbackIt->second);
    playerCallbacks->second.erase(callbackIt);
    if (playerCallbacks->second.empty())
        mModalFormCallbacks.erase(playerCallbacks);

    if (callback)
        callback(*player, cancelled ? std::string() : packet.mFormData,
                 !cancelled ? ModalFormOutcome::Answered
                 : packet.mCancelReason == ModalFormResponsePacket::CancelReason::UserBusy
                         ? ModalFormOutcome::UserBusy
                         : ModalFormOutcome::UserClosed);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PlayerSkinPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || player->isDead() || !isValidSkin(packet.mSkin))
        return;

    const Uuid playerUuid = Uuid::fromString(player->getUuid());
    const Uuid emptyUuid;
    if (playerUuid != emptyUuid && packet.mUuid != emptyUuid && packet.mUuid != playerUuid)
        return;

    if (player->changedSkinWithin(mProperties.getSkinChangeCooldown())) {
        LOG_WARN(LogAreaID::Server, "Player %s changed skin too quickly", player->getName().c_str());
        return;
    }

    SerializedSkin skin = packet.mSkin;
    normalizeSkin(skin);

    const SerializedSkin &currentSkin = player->getSkin();
    if (currentSkin.mFullSkinId == skin.mFullSkinId && currentSkin.mSkinData.mWidth == skin.mSkinData.mWidth
        && currentSkin.mSkinData.mHeight == skin.mSkinData.mHeight
        && currentSkin.mSkinData.mData == skin.mSkinData.mData)
        return;

    player->setSkin(skin);
    player->markSkinChanged();

    PlayerSkinPacket update = packet;
    update.mUuid = LoginHandler::playerListUuid(*player);
    update.mSkin = player->getSkin();
    update.mNewSkinName = skin.mSkinId;
    update.mOldSkinName.clear();
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.second.getNetworkIdentifier(), update, mCodecContext);
    }
}

namespace {
    bool isSignBlock(const std::string &identifier) {
        return identifier.find("sign") != std::string::npos;
    }

    void copySignString(const Tag &from, Tag &to, const char *key, size_t maximum) {
        if (from.contains(key, Tag::Type::String))
            to.putString(key, from.getString(key).substr(0, maximum));
    }

    void copySignByte(const Tag &from, Tag &to, const char *key) {
        if (from.contains(key, Tag::Type::Byte))
            to.putByte(key, from.getByte(key));
    }

    Tag sanitizedSignData(const Tag &source, const Vector3i &position, bool hanging) {
        static const size_t MAX_SIGN_TEXT = 512;
        static const size_t MAX_SIGN_OWNER = 64;

        Tag data = Tag::ofCompound();
        data.putString("id", hanging ? "HangingSign" : "Sign");
        data.putInt("x", position.x);
        data.putInt("y", position.y);
        data.putInt("z", position.z);
        copySignByte(source, data, "IsWaxed");

        // older clients send the lines flat
        const char *const legacyKeys[] = {"Text", "Text1", "Text2", "Text3", "Text4", "TextOwner"};
        for (const char *key: legacyKeys)
            copySignString(source, data, key, MAX_SIGN_TEXT);

        const char *const sideKeys[] = {"FrontText", "BackText"};
        for (const char *sideKey: sideKeys) {
            const Tag *side = source.get(sideKey);
            if (side == nullptr || !side->isCompound())
                continue;

            Tag clean = Tag::ofCompound();
            copySignString(*side, clean, "Text", MAX_SIGN_TEXT);
            copySignString(*side, clean, "FilteredText", MAX_SIGN_TEXT);
            copySignString(*side, clean, "TextOwner", MAX_SIGN_OWNER);
            copySignByte(*side, clean, "HideGlowOutline");
            copySignByte(*side, clean, "IgnoreLighting");
            copySignByte(*side, clean, "PersistFormatting");
            if (side->contains("SignTextColor", Tag::Type::Int))
                clean.putInt("SignTextColor", side->getInt("SignTextColor"));
            data.put(sideKey, clean);
        }
        return data;
    }

    void putPickedItem(ServerPlayer &player, ItemStack item, int hotbarSlot) {
        if (item.isAir() || item.mCount <= 0)
            return;

        PlayerInventory &inventory = player.getInventory();
        int existingSlot = -1;
        for (int index = 0; index < PlayerInventory::CONTAINER_SIZE; index++) {
            if (PlayerInventory::canStack(inventory.getItem(index), item)) {
                existingSlot = index;
                break;
            }
        }

        if (existingSlot >= 0) {
            if (existingSlot < PlayerInventory::HOTBAR_SIZE) {
                inventory.setSelectedSlot(existingSlot);
                player.getInventoryManager().syncSelectedHotbarSlot();
            } else {
                const int selectedSlot = inventory.getSelectedSlot();
                ItemStack held = inventory.getItem(selectedSlot);
                inventory.setItem(selectedSlot, inventory.getItem(existingSlot));
                inventory.setItem(existingSlot, std::move(held));
                player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, selectedSlot);
                player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, existingSlot);
            }
            return;
        }

        if (player.getGameType() != (int32_t) GameType::Creative)
            return;

        const int slot = hotbarSlot >= 0 && hotbarSlot < PlayerInventory::HOTBAR_SIZE ? hotbarSlot
                                                                                      : inventory.getSelectedSlot();

        item.mCount = 1;
        inventory.setItem(slot, std::move(item));
        inventory.setSelectedSlot(slot);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        player.getInventoryManager().syncSelectedHotbarSlot();
    }
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const BlockActorDataPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    // the editor only opens on a sign in reach
    if (!BlockActionHandler::canInteractWithBlock(*player, packet.mBlockPosition))
        return;

    // peek only: a far position must not force a chunk load
    const BlockState *statePtr = getLevelFor(*player).peekBlockPtr(packet.mBlockPosition.x, packet.mBlockPosition.y,
                                                                   packet.mBlockPosition.z);
    if (statePtr == nullptr || !isSignBlock(statePtr->mName))
        return;
    if (!packet.mData.isCompound())
        return;

    // rebuilt from the fields a sign has: the client's compound goes to every player, so nothing else
    // in it may ride along
    BlockActorDataPacket relayed;
    relayed.mBlockPosition = packet.mBlockPosition;
    relayed.mData = sanitizedSignData(packet.mData, packet.mBlockPosition,
                                      statePtr->mName.find("hanging") != std::string::npos);
    if (PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_SIGN_CHANGE)) {
        const char *const sides[] = {"FrontText", "BackText"};
        for (const char *side: sides) {
            Tag *sideData = relayed.mData.get(side);
            if (sideData == nullptr || !sideData->isCompound())
                continue;

            std::string text = sideData->getString("Text");
            PluginEvent signEvent;
            signEvent.mType = FALCON_EVENT_SIGN_CHANGE;
            signEvent.mCancellable = true;
            signEvent.mPlayer = player;
            signEvent.mLevel = &getLevelFor(*player);
            signEvent.mBlockPosition = packet.mBlockPosition;
            signEvent.mState = side == sides[0];
            signEvent.mMessage = &text;
            plugins->dispatch(signEvent);
            if (signEvent.mCancelled)
                return;
            sideData->putString("Text", text);
        }
    }

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.sharesLevelWith(*player))
            mNetworkHandler->send(entry.second.getNetworkIdentifier(), relayed, mCodecContext);
    }
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const CommandBlockUpdatePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    CommandBlockSystem::onCommandBlockUpdate(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const BlockPickRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || player->isDead())
        return;

    const Vector3f position = player->getPosition();
    const float dx = position.x - ((float) packet.mBlockPosition.x + 0.5f);
    const float dy = position.y - ((float) packet.mBlockPosition.y + 0.5f);
    const float dz = position.z - ((float) packet.mBlockPosition.z + 0.5f);
    if (dx * dx + dy * dy + dz * dz > 10000.0f)
        return;

    Level &level = getLevelFor(*player);
    const BlockState state = level.getBlockState(packet.mBlockPosition.x, packet.mBlockPosition.y,
                                                 packet.mBlockPosition.z);
    const std::string pickIdentifier = BlockPickItem::identifierFor(state.mName);
    if (pickIdentifier.empty())
        return;

    std::shared_ptr<ItemDefinition> itemDefinition = mItemDefinitions.getDefinition(pickIdentifier);
    std::shared_ptr<BlockDefinition> blockDefinition = mBlockDefinitions.getDefinition(pickIdentifier);
    if (itemDefinition == nullptr)
        return;

    ItemStack picked;
    picked.mDefinition = std::move(itemDefinition);
    picked.mBlockDefinition = std::move(blockDefinition);
    picked.mCount = 1;

    if (packet.mAddUserData) {
        const BlockActor *blockActor = level.getBlockActors().find(packet.mBlockPosition);
        if (blockActor != nullptr)
            BlockPickItem::attachBlockData(picked, *blockActor);
    }

    putPickedItem(*player, std::move(picked), packet.mHotbarSlot);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ActorPickRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned() || player->isDead())
        return;

    for (const std::unique_ptr<ItemActor> &actor: mItemEntities) {
        if (actor->isRemoved() || actor->getRuntimeId() != packet.mRuntimeActorId || !actor->sharesLevelWith(*player))
            continue;

        const Vector3f position = player->getPosition();
        const Vector3f entityPosition = actor->getPosition();
        const float dx = position.x - entityPosition.x;
        const float dy = position.y - entityPosition.y;
        const float dz = position.z - entityPosition.z;
        if (dx * dx + dy * dy + dz * dz > 10000.0f)
            return;

        putPickedItem(*player, actor->getItem(), packet.mHotbarSlot);
        return;
    }
}

ItemActor *ServerNetworkHandler::dropItem(Level &level, const Vector3f &position, const ItemStack &item,
                                          const Vector3f &motion, int pickupDelay) {
    return ItemActorHandler::dropItem(*this, level, position, item, motion, pickupDelay);
}

void ServerNetworkHandler::_sendChunks(ServerPlayer &player) {
    ChunkStreamHandler::tick(*this, player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const SetLocalPlayerAsInitializedPacket &packet) {
    (void) packet;

    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    LoginHandler::handleSetLocalPlayerAsInitialized(*this, *player);

    PlayerJoinAfterEvent joinEvent(*player);
    mEventBus.after().mPlayerJoin.emit(joinEvent);

    PlayerSpawnAfterEvent spawnEvent(*player, true);
    mEventBus.after().mPlayerSpawn.emit(spawnEvent);
}

void ServerNetworkHandler::_addToPlayerList(ServerPlayer &player) {
    LoginHandler::addToPlayerList(*this, player);
}

void ServerNetworkHandler::_removeFromPlayerList(ServerPlayer &player) {
    LoginHandler::removeFromPlayerList(*this, player);
}

void ServerNetworkHandler::broadcastSystemMessage(const std::string &message) {
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            entry.second.sendMessage(message);
    }
}

void ServerNetworkHandler::broadcastTranslation(const std::string &key, const std::vector<std::string> &parameters) {
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            entry.second.sendTranslation(key, parameters);
    }
}

void ServerNetworkHandler::broadcastWorldTime(World &world) {
    SetTimePacket packet;
    packet.mTime = (int32_t) world.getOverworld().getDayTime();
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.getWorldId() == world.getId())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

bool ServerNetworkHandler::changeGameRule(World &world, const std::string &name, const std::string &value) {
    Level &overworld = world.getOverworld();
    GameRules &rules = overworld.getGameRules();
    if (!rules.setFromString(name, value))
        return false;

    const GameRules::Rule *rule = rules.find(name);
    GameRulesChangedPacket changed;
    changed.mGameRules.push_back(rules.toChangedNetwork(*rule));
    for (auto &entry: mPlayers) {
        if (entry.second.getWorldId() == world.getId())
            mNetworkHandler->send(entry.first, changed, mCodecContext);
    }

    overworld.saveGameRules();
    mScriptEngine.onGameRuleChange(rule->mName, rule->mType == GameRuleData::Type::Bool, rule->mBoolValue,
                                   rule->mIntValue);
    return true;
}

void ServerNetworkHandler::_sendWorldState(ServerPlayer &player) {
    World &world = getWorldFor(player);
    const GameRules &rules = world.getOverworld().getGameRules();

    SetTimePacket time;
    time.mTime = (int32_t) world.getOverworld().getDayTime();
    sendPacketTo(player.getNetworkIdentifier(), time);

    GameRulesChangedPacket changed;
    for (const GameRules::Rule &rule: rules.getRules())
        changed.mGameRules.push_back(rules.toChangedNetwork(rule));
    sendPacketTo(player.getNetworkIdentifier(), changed);

    sendWeatherTo(player);
}

void ServerNetworkHandler::setDefaultGameType(GameType gameType) {
    static const char *const NAMES[] = {"survival", "creative", "adventure"};
    const int index = (int) gameType;
    mProperties.setProperty("gamemode", index >= 0 && index < 3 ? NAMES[index] : "spectator");
}

void ServerNetworkHandler::setDifficulty(const std::string &difficulty) {
    mProperties.setProperty("difficulty", difficulty);
}

void ServerNetworkHandler::changeDifficulty(Difficulty difficulty) {
    static const char *const NAMES[] = {"peaceful", "easy", "normal", "hard"};
    setDifficulty(NAMES[(size_t) difficulty]);

    SetDifficultyPacket packet;
    packet.mDifficulty = (uint32_t) difficulty;
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::changeWorldSpawn(Level &level, const Vector3i &spawn) {
    level.setSpawnPosition(spawn);
    level.saveLevelDat();

    SetSpawnPositionPacket packet;
    packet.mSpawnType = SetSpawnPositionPacket::Type::WorldSpawn;
    packet.mBlockPosition = spawn;
    packet.mDimensionId = level.getDimensionId();
    packet.mSpawnPosition = spawn;
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::sendPacketTo(const NetworkIdentifier &id, const Packet &packet) {
    mNetworkHandler->send(id, packet, mCodecContext);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const NetworkStackLatencyPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player != nullptr)
        player->getAcknowledgements().acknowledge(packet.mTimestamp);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PlayerAuthInputPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || player->isDead())
        return;

    if (!_allowPacket(id, RateLimitedPacket::Movement))
        return;

    if (!std::isfinite(packet.mPosition.x) || !std::isfinite(packet.mPosition.y) || !std::isfinite(packet.mPosition.z)
        || !std::isfinite(packet.mRotation.x) || !std::isfinite(packet.mRotation.y)
        || !std::isfinite(packet.mRotation.z) || !std::isfinite(packet.mDelta.x) || !std::isfinite(packet.mDelta.y)
        || !std::isfinite(packet.mDelta.z)) {
        return;
    }

    std::string badPacketReason;
    if (BadPacketHandler::inspect(*player, packet, badPacketReason)) {
        _rejectBadPacket(id, badPacketReason);
        return;
    }

    if (!player->isAwaitingDimensionAck() && player->getLoginState() >= ServerPlayer::LoginState::StartGameSent)
        player->getMovementInputs().enqueue(packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const CompletedUsingItemPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    if (packet.mType != ItemUseType::Eat && packet.mType != ItemUseType::Consume)
        return;

    _completeItemUse(*player, (int32_t) packet.mItemId);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const TextPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    if (packet.mType != TextPacket::Type::Chat || !_allowPacket(id, RateLimitedPacket::Chat))
        return;

    player->markActive(mCurrentTick);

    const std::string message = packet.mMessage.substr(0, packet.mMessage.find('\n'));
    if (message.find_first_not_of(" \t\r") == std::string::npos)
        return;

    const int maxLength = mProperties.getMaxChatMessageLength();
    if (maxLength > 0 && message.size() > (size_t) maxLength)
        return;

    ChatHandler::broadcastChat(*this, *player, message);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const RequestChunkRadiusPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    std::string badPacketReason;
    if (BadPacketHandler::inspect(*player, packet, badPacketReason)) {
        _rejectBadPacket(id, badPacketReason);
        return;
    }

    int32_t granted = packet.mRadius;
    const int32_t maximum = _getServerViewDistance();

    if (granted > maximum)
        granted = maximum;
    if (granted < 1)
        granted = 1;

    ChunkRadiusUpdatedPacket radius;
    radius.mRadius = granted;
    mNetworkHandler->send(id, radius, mCodecContext);

    // sub_14102BCD0: the share of the view left to the server follows the client's hardware unless
    // server-build-radius-ratio fixes it. A client that cannot generate chunks gets everything built.
    float ratio = 1.0f;
    if (player->isClientSideGenerationCompatible()) {
        ratio = mProperties.getServerBuildRadiusRatio();
        if (ratio < 0.0f)
            ratio = ClientSideGeneration::computeServerBuildRatio(maximum, packet.mMaxRadius);
    }
    player->setServerBuildRatio(ratio);

    player->getChunkStreamState().mRadius = granted;
    ChunkStreamHandler::handleViewDistanceChange(*this, *player);
    _sendChunks(*player);
    _checkTerrainReady(*player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const MapInfoRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    MapHandler::handleInfoRequest(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const MapCreateLockedCopyPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    MapHandler::handleCreateLockedCopy(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const SubChunkRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    SubChunkRequestHandler::handleRequest(*this, *player, packet);
}

void ServerNetworkHandler::_checkTerrainReady(ServerPlayer &player) {
    LoginHandler::checkTerrainReady(*this, player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const RequestAbilityPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr)
        return;

    if (packet.mAbility != Ability::Flying || packet.mType != AbilityValueType::Boolean)
        return;

    MovementHandler::requestFlight(*this, id, *player, packet.mBoolValue, true);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const MobEquipmentPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    std::string badPacketReason;
    if (BadPacketHandler::inspect(*player, packet, badPacketReason)) {
        _rejectBadPacket(id, badPacketReason);
        return;
    }

    InventoryHandler::handleMobEquipment(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PlayerHotbarPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    InventoryHandler::handlePlayerHotbar(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const InteractPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    if (packet.mAction == InteractPacket::Action::LeaveVehicle) {
        if (player->isRiding())
            RideSystem::dismount(*this, *player, true);
        return;
    }

    if (packet.mAction != InteractPacket::Action::OpenInventory)
        return;

    // riding a tamed mount, the inventory key opens the mount's window
    // the mount or the player: both accepted, which one the client sends is not checked
    ServerActor *mount = player->isRiding()
                                 ? dynamic_cast<ServerActor *>(RideSystem::resolve(*this, player->getVehicleId()))
                                 : nullptr;
    if (mount != nullptr && mount->getEntityContainer() != nullptr
        && (packet.mRuntimeActorId == player->getRuntimeId() || packet.mRuntimeActorId == mount->getRuntimeId())
        && player->getInventoryManager().openEntityContainer(*mount, ContainerType::Horse))
        return;

    if (packet.mRuntimeActorId != player->getRuntimeId())
        return;

    InventoryHandler::handleOpenInventory(*this, *player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ItemStackRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    player->markActive(mCurrentTick);

    std::string badPacketReason;
    if (BadPacketHandler::inspect(*player, packet, badPacketReason)) {
        _rejectBadPacket(id, badPacketReason);
        return;
    }

    InventoryHandler::handleItemStackRequest(*this, id, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const CraftingEventPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    InventoryHandler::handleCraftingEvent(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const InventoryTransactionPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    player->markActive(mCurrentTick);

    std::string badPacketReason;
    if (BadPacketHandler::inspect(*player, packet, badPacketReason)) {
        _rejectBadPacket(id, badPacketReason);
        return;
    }

    InventoryHandler::handleTransaction(*this, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ContainerClosePacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    player->markActive(mCurrentTick);

    InventoryHandler::handleContainerClose(*player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PacketViolationWarningPacket &packet) {
    // accepted at every login step, a failed encryption handshake reports through it: one a second,
    // or before a login it is anyone's free line in the log
    if (!_allowPacket(id, RateLimitedPacket::ViolationWarning))
        return;

    // the answer a probe was waiting for is not a rejection to report
    ServerPlayer *probed = _getPlayer(id);
    if (probed != nullptr && !mProperties.getAllowUnofficialClients()
        && mClientProbes.onViolationWarning(*probed, packet))
        return;

    const char *causeName = toString((MinecraftPacketIds) packet.mPacketCauseId);
    LOG_ERROR(LogAreaID::Network, "CLIENT REJECTED PACKET id=%d (%s), type=%d, severity=%d, context: %s",
              packet.mPacketCauseId, causeName, (int) packet.mType, (int) packet.mSeverity,
              StringUtil::printable(packet.mContext).c_str());
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const CommandRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !_allowPacket(id, RateLimitedPacket::Command))
        return;

    player->markActive(mCurrentTick);
    LOG_INFO(LogAreaID::Server, "%s issued command: %s", player->getName().c_str(),
             StringUtil::printable(packet.mCommand, 512).c_str());

    std::string commandLine = packet.mCommand;
    PluginEvent commandEvent;
    commandEvent.mType = FALCON_EVENT_PLAYER_COMMAND;
    commandEvent.mCancellable = true;
    commandEvent.mPlayer = player;
    commandEvent.mMessage = &commandLine;
    mPluginManager->dispatch(commandEvent);
    if (commandEvent.mCancelled)
        return;

    PlayerCommandOrigin sender(*this, *player, packet.mOrigin);
    mCommands.dispatch(sender, commandLine);
}

void ServerNetworkHandler::onReceiveIPSupport(RakPeerHelper::IPSupport support) {
    if (mNetherNetInstance == nullptr)
        return;

    const ConnectionDefinition &definition = mNetherNetInstance->getConnectionDefinition();

    if (support == RakPeerHelper::IPSupport::IPv4 || support == RakPeerHelper::IPSupport::Both)
        LOG_INFO(LogAreaID::Network, "IPv4 supported, port: %u: Used for gameplay", definition.mPort);

    if (support == RakPeerHelper::IPSupport::IPv6 || support == RakPeerHelper::IPSupport::Both)
        LOG_INFO(LogAreaID::Network, "IPv6 supported, port: %u: Used for gameplay", definition.mPortV6);
}
