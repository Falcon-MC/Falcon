#include "Network/Handler/ServerNetworkHandler.h"

#include "Scripting/Content/CustomContentRegistry.h"

#include "Actor/RideSystem.h"
#include "Core/Math/MathConstants.h"

#include "Command/ServerCommandOrigin.h"
#include "Command/DeopCommand.h"
#include "Command/EnchantCommand.h"
#include "Command/EffectCommand.h"
#include "Command/GameModeCommand.h"
#include "Command/GiveCommand.h"
#include "Command/DifficultyCommand.h"
#include "Command/ExecuteCommand.h"
#include "Command/FillCommand.h"
#include "Command/KillCommand.h"
#include "Command/ListCommand.h"
#include "Command/MeCommand.h"
#include "Command/SayCommand.h"
#include "Command/SetBlockCommand.h"
#include "Command/SetWorldSpawnCommand.h"
#include "Command/SpawnPointCommand.h"
#include "Command/SummonCommand.h"
#include "Command/TellCommand.h"
#include "Command/TellRawCommand.h"
#include "Command/TitleCommand.h"
#include "Command/XpCommand.h"
#include "Command/TimeCommand.h"
#include "Command/ProfilerCommand.h"
#include "Command/AboutCommand.h"
#include "Command/AllowListCommand.h"
#include "Command/BanCommand.h"
#include "Command/KickCommand.h"
#include "Command/PardonCommand.h"
#include "Command/TeleportCommand.h"
#include "Command/GameRuleCommand.h"
#include "Command/LocateCommand.h"
#include "Command/WeatherCommand.h"
#include "Block/BlockActorStore.h"
#include "Block/BlockPickItem.h"
#include "Level/Generator/Overworld/OverworldGenerator.h"
#include "Block/Systems/BlockContactSystem.h"
#include "Block/Systems/PistonSystem.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Item/Items/ElytraItem.h"
#include "Item/Items/TotemItem.h"
#include "Command/CameraCommand.h"
#include "Command/ClearCommand.h"
#include "Command/OpCommand.h"
#include "Command/PlayerCommandOrigin.h"
#include "Core/Debug/BedrockLog.h"
#include "Network/ConnectionRequest.h"
#include "Network/AuthKeyProvider.h"
#include "Network/TransportFactory.h"
#include "Network/NetherNet/NetherNetInstance.h"
#include "Network/Handler/BadPacketHandler.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ChatHandler.h"
#include "Network/Handler/InventoryHandler.h"
#include "Inventory/InventoryManager.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/LoginChainVerifier.h"
#include "Network/Handler/LoginHandler.h"
#include "Network/Handler/MovementHandler.h"
#include "Network/Handler/SubChunkRequestHandler.h"
#include "Protocol/Packets/SubChunkRequestPacket.h"
#include "Core/Utility/ReadOnlyBinaryStream.h"
#include "Level/LevelChunk.h"
#include "Block/BlockData.h"
#include "Block/BlockShape.h"
#include "Block/Inventory/EnderChestInventoryStore.h"
#include "Protocol/MinecraftPackets.h"
#include "Protocol/Packets/DisconnectPacket.h"
#include "Protocol/Packets/LevelChunkPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/LoginPacket.h"
#include "Protocol/Packets/NetworkChunkPublisherUpdatePacket.h"
#include "Protocol/Packets/NetworkSettingsPacket.h"
#include "Protocol/Packets/PlayStatusPacket.h"
#include "Protocol/Packets/RequestChunkRadiusPacket.h"
#include "Protocol/Packets/RequestAbilityPacket.h"
#include "Protocol/Packets/ChunkRadiusUpdatedPacket.h"
#include "Protocol/Packets/RequestNetworkSettingsPacket.h"
#include "Protocol/Packets/ResourcePackClientResponsePacket.h"
#include "Protocol/Packets/ResourcePackStackPacket.h"
#include "Protocol/Packets/ResourcePacksInfoPacket.h"
#include "Protocol/Packets/ResourcePackDataInfoPacket.h"
#include "Protocol/Packets/ResourcePackChunkDataPacket.h"
#include "Protocol/Packets/ResourcePackChunkRequestPacket.h"
#include "Protocol/Packets/SetLocalPlayerAsInitializedPacket.h"
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
#include "Actor/PlayerAbility.h"
#include "Protocol/Packets/StartGamePacket.h"
#include "Protocol/Packets/SetTimePacket.h"
#include "Protocol/Packets/AvailableActorIdentifiersPacket.h"
#include "Protocol/Packets/DeathInfoPacket.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/ChangeDimensionPacket.h"
#include "Protocol/Packets/PlayerActionPacket.h"
#include "Protocol/Packets/RespawnPacket.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Packets/BlockPickRequestPacket.h"
#include "Protocol/Packets/ActorPickRequestPacket.h"
#include "Protocol/Packets/EmotePacket.h"
#include "Protocol/Packets/ModalFormRequestPacket.h"
#include "Protocol/Packets/ModalFormResponsePacket.h"
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
#include "Block/Components/CreativeContentTable.h"
#include "Block/Block.h"
#include "Block/Actor/HopperBlockActor.h"
#include "Block/Systems/FurnaceSystem.h"
#include "Block/Systems/CommandBlockSystem.h"
#include "Block/Systems/FireSystem.h"
#include "Block/Systems/RandomTickSystem.h"
#include "Block/Systems/RedstoneSystem.h"
#include "Protocol/Packets/CommandBlockUpdatePacket.h"
#include "Block/Blocks/BedBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Protocol/Packets/AnimatePacket.h"
#include "Item/CraftingRecipeTable.h"
#include "Item/ItemNetworkIdTable.h"
#include "Item/ItemData.h"
#include "Item/VanillaItems.h"
#include "Item/ItemEnchantments.h"
#include "Item/Items/ChorusFruitItem.h"
#include "Core/NBT/NbtIo.h"

#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <optional>
#include <sstream>
#include <string>
#include <unordered_set>
#include <fstream>
#include <thread>
#include <utility>
#include "Protocol/Packets/UpdateAttributesPacket.h"

static const int64_t RESOURCE_PACK_CHUNK_SIZE = 1024 * 1024;
static const float PLAYER_BASE_OFFSET = 1.62f;
static const int32_t SLEEP_CHECK_DELAY_TICKS = 75;
static const int64_t DAY_LENGTH_TICKS = 24000;
static const float BED_HEIGHT = 0.5625f;
static const int64_t FOOD_USE_DURATION_TICKS = 32;
static const int64_t DRIED_KELP_USE_DURATION_TICKS = 16;
static const int64_t EARLY_CONSUMABLE_RELEASE_BLOCK_TICKS = 10;

static bool isDamageDisabledByGameRule(const GameRules &rules, const std::string &deathMessageKey) {
    if (deathMessageKey.rfind("death.fell.", 0) == 0 || deathMessageKey == "death.attack.stalagmite")
        return !rules.getBool("falldamage");

    if (deathMessageKey == "death.attack.lava" || deathMessageKey == "death.attack.onFire"
        || deathMessageKey == "death.attack.inFire" || deathMessageKey == "death.attack.hotFloor")
        return !rules.getBool("firedamage");

    if (deathMessageKey == "death.attack.drown")
        return !rules.getBool("drowningdamage");

    if (deathMessageKey == "death.attack.freeze")
        return !rules.getBool("freezedamage");

    return false;
}

static int64_t useDurationTicksFor(const ItemStack &item) {
    if (item.isAir() || item.mDefinition == nullptr)
        return FOOD_USE_DURATION_TICKS;

    if (item.mDefinition->getIdentifier() == "minecraft:dried_kelp")
        return DRIED_KELP_USE_DURATION_TICKS;

    return FOOD_USE_DURATION_TICKS;
}
static const float THROW_SPEED = 0.3f;
static const int THROW_PICKUP_DELAY = 40;
static const char *HEALTH_ATTRIBUTE = "minecraft:health";
static const float DEFAULT_MAX_HEALTH = 20.0f;
static const float ITEM_DROP_HEIGHT = 1.3f;

namespace {
    const float EYE_HEIGHT_FACTOR = 0.9f;
    const float PLAYER_COLLISION_HEIGHT = 1.8f;
    const float SUFFOCATION_DAMAGE = 1.0f;

    float maxHealthOf(const Actor &actor) {
        for (const AttributeData &attribute: actor.getAttributes().getAll()) {
            if (attribute.mName == HEALTH_ATTRIBUTE)
                return attribute.mMaximum;
        }

        return DEFAULT_MAX_HEALTH;
    }

    int protectionFactor(int level, float modifier) {
        if (level <= 0)
            return 0;
        return (int) std::floor((6.0f + (float) (level * level)) * modifier / 3.0f);
    }

    float applyArmorModifiers(const ServerPlayer &player, float amount, const std::string &deathMessageKey) {
        const bool fireDamage = deathMessageKey == "death.attack.lava"
                                || deathMessageKey == "death.attack.onFire"
                                || deathMessageKey == "death.attack.inFire";
        const bool fallDamage = deathMessageKey == "death.fell.accident.generic";
        const bool explosionDamage = deathMessageKey == "death.attack.explosion";
        const bool projectileDamage = deathMessageKey == "death.attack.arrow";
        const bool armorDamage = deathMessageKey != "death.attack.inFire"                                  
                                 && deathMessageKey != "death.attack.drown"
                                 && !fallDamage
                                 && deathMessageKey != "death.attack.outOfWorld"
                                 && deathMessageKey != "death.attack.thorns"
                                 && deathMessageKey != "death.attack.suicide";

        int armorPoints = 0;
        int enchantmentProtectionFactor = 0;
        for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot) {
            const ItemStack &armor = player.getInventory().getArmor(slot);
            if (armor.isAir() || armor.mDefinition == nullptr)
                continue;

            const ItemData *data = ItemDataTable::find(armor.mDefinition->getIdentifier());
            if (data != nullptr)
                armorPoints += data->mArmorPoints;

            enchantmentProtectionFactor += protectionFactor(
                    ItemEnchantments::getLevel(armor, EnchantmentIds::PROTECTION), 0.75f);
            if (fireDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::FIRE_PROTECTION), 1.25f);
            if (fallDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::FEATHER_FALLING), 2.5f);
            if (explosionDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::BLAST_PROTECTION), 1.5f);
            if (projectileDamage)
                enchantmentProtectionFactor += protectionFactor(
                        ItemEnchantments::getLevel(armor, EnchantmentIds::PROJECTILE_PROTECTION), 1.5f);
        }

        if (armorDamage)
            amount *= std::max(0.0f, 1.0f - std::min(1.0f, (float) armorPoints * 0.04f));

        if (enchantmentProtectionFactor > 0) {
            const int scaledProtection = std::min(
                    (int) std::ceil(std::min(enchantmentProtectionFactor, 25) *
                                    (50.0f + (float) (rand() % 51)) / 100.0f), 20);
            amount *= std::max(0.0f, 1.0f - (float) scaledProtection * 0.04f);
        }

        return amount;
    }

    const FoodItemComponent *findFoodComponent(const ItemStack &item);

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
                case '"': return parseString();
                case 't': return parseLiteral("true");
                case 'f': return parseLiteral("false");
                case 'n': return parseLiteral("null");
                default: return parseNumber();
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
                } else if (escape != '"' && escape != '\\' && escape != '/' && escape != 'b' &&
                           escape != 'f' && escape != 'n' && escape != 'r' && escape != 't') {
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

    bool isValidSkin(const SerializedSkin &skin) {
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

ServerNetworkHandler::ServerNetworkHandler(const std::string &serverName, const std::string &subName, int maxPlayers,
                                           TransportLayer transport)
        : mRakNetInstance(nullptr), mCodecContext(mBlockDefinitions, mItemDefinitions), mMaxPlayers(maxPlayers),
          mIsListening(false), mNextRuntimeId(1),
          mLevel("Bedrock level", DEFAULT_VIEW_DISTANCE),
          mPlayerData("players"), mOps("ops.txt"), mAllowList("allowlist.json"),
          mBanList("banned-players.json") {
    std::unique_ptr<Connector> rakNet = TransportFactory::createConnector(TransportLayer::RakNet, *this, true);

    if (rakNet != nullptr) {
        mRakNetInstance = dynamic_cast<RakNetInstance *>(rakNet.get());

        mNetworkHandler.reset(new NetworkHandler(std::move(rakNet)));
        mNetworkHandler->setProfiler(&mProfiler);
        mNetworkHandler->addListener(this);

        if (transport == TransportLayer::NetherNet) {
            std::unique_ptr<Connector> netherNet =
                    TransportFactory::createConnector(TransportLayer::NetherNet, *this, true);

            if (netherNet != nullptr) {
                mNetherNetInstance = dynamic_cast<NetherNetInstance *>(netherNet.get());
                mNetworkHandler->addConnector(std::move(netherNet));
            }
        }
    }

    mAnnouncement.mServerName = serverName;
    mAnnouncement.mSubName = subName;
    mAnnouncement.mGameMode = "Survival";
    mAnnouncement.mGameModeId = 1;
    mAnnouncement.mMaxPlayers = maxPlayers;

    mCommands.registerCommand(std::make_shared<GameModeCommand>(*this));
    mCommands.registerCommand(std::make_shared<OpCommand>(*this));
    mCommands.registerCommand(std::make_shared<DeopCommand>(*this));
    mCommands.registerCommand(std::make_shared<GiveCommand>(*this));
    mCommands.registerCommand(std::make_shared<EnchantCommand>(*this));
    mCommands.registerCommand(std::make_shared<EffectCommand>(*this));
    mCommands.registerCommand(std::make_shared<KillCommand>(*this));
    mCommands.registerCommand(std::make_shared<SetBlockCommand>(*this));
    mCommands.registerCommand(std::make_shared<FillCommand>(*this));
    mCommands.registerCommand(std::make_shared<SummonCommand>(*this));
    mCommands.registerCommand(std::make_shared<ExecuteCommand>(*this));
    mCommands.registerCommand(std::make_shared<XpCommand>(*this));
    mCommands.registerCommand(std::make_shared<DifficultyCommand>(*this));
    mCommands.registerCommand(std::make_shared<ListCommand>(*this));
    mCommands.registerCommand(std::make_shared<TitleCommand>(*this, false));
    mCommands.registerCommand(std::make_shared<TitleCommand>(*this, true));
    mCommands.registerCommand(std::make_shared<SpawnPointCommand>(*this, false));
    mCommands.registerCommand(std::make_shared<SpawnPointCommand>(*this, true));
    mCommands.registerCommand(std::make_shared<SetWorldSpawnCommand>(*this));
    mCommands.registerCommand(std::make_shared<SayCommand>(*this));
    mCommands.registerCommand(std::make_shared<TellCommand>(*this));
    mCommands.registerCommand(std::make_shared<MeCommand>(*this));
    mCommands.registerCommand(std::make_shared<TellRawCommand>(*this));
    mCommands.registerCommand(std::make_shared<TimeCommand>(*this));
    mCommands.registerCommand(std::make_shared<WeatherCommand>(*this));
    mCommands.registerCommand(std::make_shared<GameRuleCommand>(*this));
    mCommands.registerCommand(std::make_shared<ProfilerCommand>(*this));
    mCommands.registerCommand(std::make_shared<CameraCommand>(*this));
    mCommands.registerCommand(std::make_shared<ClearCommand>(*this));
    mCommands.registerCommand(std::make_shared<LocateCommand>(*this));
    mCommands.registerCommand(std::make_shared<AboutCommand>(*this));
    mCommands.registerCommand(std::make_shared<AllowListCommand>(*this));
    mCommands.registerCommand(std::make_shared<TeleportCommand>(*this));
    mCommands.registerCommand(std::make_shared<KickCommand>(*this));
    mCommands.registerCommand(std::make_shared<BanCommand>(*this));
    mCommands.registerCommand(std::make_shared<PardonCommand>(*this));

    mResourcePacks.loadFromDirectory("resource_packs");
    mResourcePacks.loadBundledAddonsFrom("behavior_packs");
    _registerVanillaDefinitions();

    AuthKeyProvider::getInstance().start();
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
    stopServerListening();

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

    if (mNetherNetInstance != nullptr) {
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
    }

    const int64_t levelSeed = OverworldGenerator::parseSeed(properties.getLevelSeed());

    mLevel = Level(properties.getLevelName(), _getServerViewDistance(), levelSeed, DimensionType::Overworld);
    mLevel.openStorage("worlds");
    mLevel.initializeWeather();
    mLevel.initializeGameRules();
    mLevel.startWorkers(_getChunkWorkerThreadCount());

    mNetherLevel.reset(new Level(properties.getLevelName(), _getServerViewDistance(), levelSeed,
                                 DimensionType::Nether));
    mTheEndLevel.reset(new Level(properties.getLevelName(), _getServerViewDistance(), levelSeed,
                                 DimensionType::TheEnd));

    if (mLevel.isStorageOpen()) {
        mNetherLevel->attachStorage(mLevel);
        mTheEndLevel->attachStorage(mLevel);
    }

    mNetherLevel->startWorkers(_getChunkWorkerThreadCount());
    mTheEndLevel->startWorkers(_getChunkWorkerThreadCount());

    const Level::PacketBroadcaster broadcaster = [this](Level &level, const Vector3f &position,
                                                        const Packet &packet) {
        BlockActionHandler::broadcastToViewers(*this, level, position, packet);
    };
    mLevel.setPacketBroadcaster(broadcaster);
    mNetherLevel->setPacketBroadcaster(broadcaster);
    mTheEndLevel->setPacketBroadcaster(broadcaster);

    mLevel.setOwner(this);
    mNetherLevel->setOwner(this);
    mTheEndLevel->setOwner(this);

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

    mAnnouncement.mGameModeId = (int) properties.getGameType();

    if (mIsListening)
        _updateServerAnnouncement();
}

Level &ServerNetworkHandler::getDimension(DimensionType dimension) {
    if (dimension == DimensionType::Nether && mNetherLevel != nullptr)
        return *mNetherLevel;

    if (dimension == DimensionType::TheEnd && mTheEndLevel != nullptr)
        return *mTheEndLevel;

    return mLevel;
}

Level &ServerNetworkHandler::getLevelFor(const Actor &actor) {
    return getDimension(actor.getDimension());
}

std::vector<Level *> ServerNetworkHandler::getLevels() {
    std::vector<Level *> levels{&mLevel};
    if (mNetherLevel != nullptr)
        levels.push_back(mNetherLevel.get());
    if (mTheEndLevel != nullptr)
        levels.push_back(mTheEndLevel.get());
    return levels;
}

void ServerNetworkHandler::_tickDimension(Level &level) {
    level.drainCompletedChunks();
    level.processGeneratedChanges();

    const std::vector<int64_t> repopulated = level.consumeRepopulatedChunks();
    if (!repopulated.empty()) {
        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || player.getDimension() != level.getDimensionType())
                continue;

            for (const int64_t hash: repopulated)
                ChunkStreamHandler::invalidateChunk(player, hash);
        }
    }

    std::vector<int64_t> activeColumns;
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.getDimension() != level.getDimensionType())
            continue;

        const int32_t centerX = (int32_t) std::floor(player.getPosition().x) >> 4;
        const int32_t centerZ = (int32_t) std::floor(player.getPosition().z) >> 4;

        for (int32_t dx = -1; dx <= 1; ++dx) {
            for (int32_t dz = -1; dz <= 1; ++dz)
                activeColumns.push_back(((int64_t) (centerX + dx) << 32) | (uint32_t) (centerZ + dz));
        }
    }

    level.setActiveColumns(activeColumns);
    syncActorPersistence(level, activeColumns);
    level.tick();

    for (const Level::FluidChange &change: level.consumeFluidChanges()) {
        UpdateBlockPacket update;
        update.mBlockPosition = change.position;
        update.mRuntimeId = (uint32_t) BlockStateHasher::hash(change.state.mName, change.state.mStates);
        update.mFlags = UpdateBlockPacket::Flag::All;
        update.mDataLayer = (uint32_t) change.layer;
        BlockActionHandler::broadcastToViewers(*this, level,
                                               Vector3f((float) change.position.x + 0.5f,
                                                        (float) change.position.y + 0.5f,
                                                        (float) change.position.z + 0.5f),
                                               update);
    }

    level.processChunkUnloads();
}

void ServerNetworkHandler::changePlayerDimension(ServerPlayer &player, DimensionType dimension,
                                                 const Vector3f &position) {
    if (player.getDimension() == dimension)
        return;

    Level &previous = getLevelFor(player);
    previous.unregisterAllChunkLoaders(player.getRuntimeId());

    player.setDimension(dimension);
    player.getVisibleActors().clear();
    player.getVisiblePlayers().clear();
    player.setAwaitingDimensionAck(true);
    player.resetChunkStreaming();
    player.getChunkStreamState() = ChunkStreamState();

    player.setPosition(position);
    player.clearPendingMove();

    ChangeDimensionPacket change;
    change.mDimension = Dimension::toId(dimension);
    change.mPosition = position;
    change.mRespawn = false;
    change.mHasLoadingScreenId = false;
    change.mLoadingScreenId = 0;
    sendPacketTo(player.getNetworkIdentifier(), change);

    ChunkStreamHandler::handleTeleport(*this, player);
}

void ServerNetworkHandler::onPlayerDimensionChangeAck(ServerPlayer &player) {
    if (!player.isAwaitingDimensionAck())
        return;

    player.setAwaitingDimensionAck(false);
    player.teleport(*this, player.getPosition(), MovePlayerTeleportationCause::Behavior);
    ItemActorHandler::sendItemActorsTo(*this, player);
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

    if (!mNetworkHandler->host(definition)) {
        LOG_ERROR(LogAreaID::Network, "Failed to bind UDP port %u", definition.mPort);
        return false;
    }

    mMaxPlayers = definition.mMaxNumPlayers;
    mAnnouncement.mMaxPlayers = mMaxPlayers;
    mIsListening = true;

    _updateServerAnnouncement();
    mNetworkHandler->startIoThread();
    _loadScripts();
    return true;
}

void ServerNetworkHandler::_validatePackDependencies() {
    for (;;) {
        std::vector<std::string> available = mResourcePacks.getLoadedUuids();
        const std::vector<std::string> behaviorUuids = mBehaviorPacks.getLoadedUuids();
        available.insert(available.end(), behaviorUuids.begin(), behaviorUuids.end());

        const size_t removed = mResourcePacks.pruneUnsatisfied(available)
                               + mBehaviorPacks.pruneUnsatisfied(available);
        if (removed == 0)
            break;
    }
}

void ServerNetworkHandler::_loadScripts() {
    if (!mScriptEngine.isReady())
        return;

    mScriptEngine.bindHost(*this);

    mBehaviorPacks.discover("behavior_packs");
    _validatePackDependencies();

    CustomContentRegistry::getInstance().load(mBehaviorPacks, mItemDefinitions, mBlockDefinitions);

    loadWorldDynamicProperties();

    size_t loaded = 0;
    for (const BehaviorPack &pack: mBehaviorPacks.getPacks()) {
        if (!pack.hasScript())
            continue;

        if (mScriptEngine.evaluateFile(pack.scriptPath()))
            loaded++;
    }

    (void) loaded;

    mScriptEngine.onWorldInitialize();
}

void ServerNetworkHandler::stopServerListening() {
    if (!mIsListening)
        return;

    if (!mPlayers.empty()) {
        LOG_INFO(LogAreaID::Server, "Saving data for %zu player(s)", mPlayers.size());

        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;

            if (!player.getName().empty())
                _savePlayerData(player);

            _disconnect(entry.first, "disconnectionScreen.disconnected");
        }

        mPlayers.clear();
        mNetworkHandler->runEvents();
    }

    saveWorldDynamicProperties();

    saveAllActors();

    LOG_INFO(LogAreaID::Server, "Saving level %s", mLevel.getName().c_str());

    if (mNetherLevel != nullptr)
        mNetherLevel->closeStorage();

    if (mTheEndLevel != nullptr)
        mTheEndLevel->closeStorage();

    mLevel.closeStorage();

    AuthKeyProvider::getInstance().stop();

    mNetworkHandler->disconnect();
    mIsListening = false;

    LOG_INFO(LogAreaID::Server, "Server stopped.");
}

namespace {
    class ProfiledPlayerScope {
    public:
        ProfiledPlayerScope(Profiler &profiler, const ServerPlayer &player)
                : mProfiler(profiler), mPlayer(player), mChunksBefore(player.getSentChunkCount()),
                  mStart(std::chrono::steady_clock::now()) {}

        ~ProfiledPlayerScope() {
            if (!mProfiler.isActive())
                return;

            const double elapsed = std::chrono::duration<double, std::milli>(
                    std::chrono::steady_clock::now() - mStart).count();

            mProfiler.recordPlayer(mPlayer.getName(), elapsed,
                                   (uint32_t) (mPlayer.getSentChunkCount() - mChunksBefore));
        }

    private:
        Profiler &mProfiler;
        const ServerPlayer &mPlayer;
        size_t mChunksBefore;
        std::chrono::steady_clock::time_point mStart;
    };
}

void ServerNetworkHandler::tick() {
    if (!mIsListening)
        return;

    const std::chrono::steady_clock::time_point tickStart = std::chrono::steady_clock::now();

    mTickStartSamples.push_back(tickStart);
    if (mTickStartSamples.size() > TICK_SAMPLE_COUNT)
        mTickStartSamples.pop_front();

    mCurrentTick++;
    mLevel.tickTime();
    _tickSleep();

    const int autoSaveInterval = mProperties.getAutoSaveInterval();
    if (autoSaveInterval > 0 && mCurrentTick % autoSaveInterval == 0)
        autoSave();
    mProfiler.beginTick(mCurrentTick);

    mProfiler.beginSection(ProfilerSection::Weather);
    mLevel.updateSkyLightSubtracted();
    tickWeather();
    mProfiler.endSection(ProfilerSection::Weather);

    mProfiler.beginSection(ProfilerSection::ConsoleCommands);
    {
        std::lock_guard<std::mutex> lock(mConsoleQueueMutex);
        while (!mConsoleQueue.empty()) {
            ServerCommandOrigin sender(this);
            mCommands.dispatch(sender, mConsoleQueue.front());
            mConsoleQueue.pop();
        }
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

    mNetworkHandler->runEvents();

    mProfiler.beginSection(ProfilerSection::ChunkDrain);
    mLevel.drainCompletedChunks();
    mProfiler.endSection(ProfilerSection::ChunkDrain);

    mProfiler.beginSection(ProfilerSection::ChunkPopulation);
    mLevel.processGeneratedChanges();

    const std::vector<int64_t> repopulated = mLevel.consumeRepopulatedChunks();
    if (!repopulated.empty()) {
        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || player.getDimension() != DimensionType::Overworld)
                continue;

            for (const int64_t hash: repopulated)
                ChunkStreamHandler::invalidateChunk(player, hash);
        }
    }
    mProfiler.endSection(ProfilerSection::ChunkPopulation);

    {
        const int tickDistance = mProperties.getTickDistance();
        std::vector<int64_t> centers;

        for (auto &entry: mPlayers) {
            ServerPlayer &player = entry.second;
            if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent)
                continue;
            if (player.getDimension() != DimensionType::Overworld)
                continue;

            const int32_t centerX = (int32_t) std::floor(player.getPosition().x) >> 4;
            const int32_t centerZ = (int32_t) std::floor(player.getPosition().z) >> 4;
            centers.push_back(((int64_t) centerX << 32) | (uint32_t) centerZ);
        }

        std::sort(centers.begin(), centers.end());
        centers.erase(std::unique(centers.begin(), centers.end()), centers.end());

        if (mActorPersistencePending || tickDistance != mActiveTickDistance || centers != mActiveCenters) {
            mActiveCenters = centers;
            mActiveTickDistance = tickDistance;

            const size_t span = (size_t) (2 * tickDistance + 1);
            std::vector<int64_t> activeColumns;
            activeColumns.reserve(centers.size() * span * span);

            for (const int64_t center: centers) {
                const int32_t centerX = (int32_t) (center >> 32);
                const int32_t centerZ = (int32_t) (center & 0xffffffff);

                for (int32_t dx = -tickDistance; dx <= tickDistance; ++dx) {
                    for (int32_t dz = -tickDistance; dz <= tickDistance; ++dz)
                        activeColumns.push_back(((int64_t) (centerX + dx) << 32) | (uint32_t) (centerZ + dz));
                }
            }

            mLevel.setActiveColumns(activeColumns);

            mProfiler.beginSection(ProfilerSection::ActorPersistence);
            mActorPersistencePending = syncActorPersistence(mLevel, activeColumns);
            mProfiler.endSection(ProfilerSection::ActorPersistence);
        }
    }

    mProfiler.beginSection(ProfilerSection::Fluids);
    mLevel.tick();
    mProfiler.endSection(ProfilerSection::Fluids);

    mProfiler.beginSection(ProfilerSection::FluidBroadcast);
    for (const Level::FluidChange &change: mLevel.consumeFluidChanges()) {
        UpdateBlockPacket update;
        update.mBlockPosition = change.position;
        update.mRuntimeId = (uint32_t) BlockStateHasher::hash(change.state.mName, change.state.mStates);
        update.mFlags = UpdateBlockPacket::Flag::All;
        update.mDataLayer = (uint32_t) change.layer;
        BlockActionHandler::broadcastToViewers(*this, mLevel,
                                               Vector3f((float) change.position.x + 0.5f,
                                                        (float) change.position.y + 0.5f,
                                                        (float) change.position.z + 0.5f),
                                               update);
    }
    mProfiler.endSection(ProfilerSection::FluidBroadcast);

    mProfiler.beginSection(ProfilerSection::Players);
    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;

        if (player.getLoginState() < ServerPlayer::LoginState::StartGameSent)
            continue;

        ProfiledPlayerScope playerScope(mProfiler, player);

        const bool wasOnFire = player.isOnFire();
        player.tickGroundTracking();
        player.tickCombat(1);
        player.tickItemCooldowns(mCurrentTick);
        player.tickSpinAttack(*this);
        ElytraItem::tickGliding(*this, player);
        FurnaceSystem::tick(*this, player);

        if (player.hasPendingMove()) {
            const int32_t gameType = player.getGameType();

            if (gameType == (int32_t) GameType::Survival || gameType == (int32_t) GameType::Adventure) {
                MovementHandler::handleMovement(*this, player, player.getPendingMovePosition(),
                                                player.getPendingMoveRotation());
            } else {
                player.setPosition(player.getPendingMovePosition());
                player.setRotation(player.getPendingMoveRotation());
            }
            player.clearPendingMove();
        }

        _handleVoidDamage(player);
        _handleSuffocationDamage(player);
        MovementHandler::tickFluidEffects(*this, player);
        BlockContactSystem::tick(*this, player);

        const bool fireTickDamage = !player.isDead() && player.tickFire();

        if (player.getGameType() == (int32_t) GameType::Creative && player.getFireTicks() > 1)
            player.setFireTicks(1);

        if (fireTickDamage && player.getGameType() != (int32_t) GameType::Creative &&
            player.getGameType() != (int32_t) GameType::Spectator &&
            !player.hasEffect(MobEffectId::FireResistance))
            applyDamage(player, 1.0f, "death.attack.inFire", {player.getName()});

        if (wasOnFire != player.isOnFire())
            _sendEntityData(player);

        if (player.isSpawned() && player.getFlags().get(ActorFlag::UsingItem)) {
            const int64_t itemUseTicks = mCurrentTick - player.getItemUseStartTick();
            const ItemStack &item = player.getInventory().getItemInHand();
            const bool milk = !item.isAir() && item.mDefinition != nullptr
                              && std::string(item.mDefinition->getIdentifier()) == "minecraft:milk_bucket";
            const bool consumable = milk || findFoodComponent(item) != nullptr;
            if (consumable && itemUseTicks >= useDurationTicksFor(item)) {
                _consumeHeldItem(player);
            } else if (consumable && itemUseTicks > 0 && itemUseTicks % 4 == 0 && item.mDefinition != nullptr) {
                const int32_t eventData = (int32_t) (((item.mDefinition->getRuntimeId() & 0xffff) << 16)
                                                     | (item.mDamage & 0xffff));
                _broadcastEntityEvent(player, (uint8_t) EntityEventType::EatingItem, eventData);
            } else if (!consumable && item.mDefinition != nullptr) {
                const Item *itemType = VanillaItems::fromIdentifier(item.mDefinition->getIdentifier());
                if (itemType != nullptr)
                    itemType->onUsingTick(*this, player, item, (int32_t) itemUseTicks);
            }
        }

        const bool effectAttributesDirty = player.getEffects().consumeAttributesDirty();
        const bool effectStateChanged = player.isSpawned() && player.tickEffects(1);
        if (player.isSpawned() && (effectAttributesDirty || effectStateChanged)) {
            _sendAttributes(player);
            _sendEntityData(player);
        }

        if (player.isBreakingBlock()) {
            const Vector3i breakingPosition = player.getBreakingBlockPosition();
            const Vector3f breakingCenter((float) breakingPosition.x + 0.5f, (float) breakingPosition.y + 0.5f,
                                          (float) breakingPosition.z + 0.5f);
            const Vector3f feet = player.getPosition();
            const float dx = feet.x - breakingCenter.x;
            const float dy = feet.y - breakingCenter.y;
            const float dz = feet.z - breakingCenter.z;

            if (dx * dx + dy * dy + dz * dz > 256.0f) {
                BlockActionHandler::stopBreakingBlock(*this, player);
            } else {
                BlockActionHandler::continueBreakingBlock(*this, player);

                if (player.isBreakingBlock() && player.getBreakingFxTicker() % 5 == 0)
                    BlockActionHandler::sendBreakingFx(*this, player);
            }
        }

        const bool wasSprinting = player.getFlags().get(ActorFlag::Sprinting);
        const bool naturalRegeneration = getLevelFor(player).getGameRules().getBool("naturalregeneration");
        const bool hungerChanged = player.isSpawned()
                                   && player.tickHunger(1, (int) mProperties.getDifficulty(), naturalRegeneration);
        if (hungerChanged)
            _sendAttributes(player);
        if (player.isSpawned() && player.refreshVisibleEffects())
            _sendEntityData(player);

        if (player.consumeStarveDamage())
            applyDamage(player, 1.0f, "death.attack.starve", {player.getName()}, false, false);
        if (wasSprinting != player.getFlags().get(ActorFlag::Sprinting))
            _sendEntityData(player);

        if (!player.hasChunkPosition())
            continue;

        mProfiler.beginSection(ProfilerSection::ChunkStreaming);
        _sendChunks(player);
        _checkTerrainReady(player);
        mProfiler.endSection(ProfilerSection::ChunkStreaming);
    }
    mProfiler.endSection(ProfilerSection::Players);

    mProfiler.beginSection(ProfilerSection::Furnaces);
    FurnaceSystem::tickStored(*this);
    mProfiler.endSection(ProfilerSection::Furnaces);

    HopperBlockActor::tickAll(*this);

    mProfiler.beginSection(ProfilerSection::ItemActors);
    ItemActorHandler::tickItemActors(*this);
    mProfiler.endSection(ProfilerSection::ItemActors);

    tickActors();
    updateActorVisibility();
    updatePlayerVisibility();

    for (auto &entry: mPlayers)
        broadcastPlayerMove(entry.second);
    mLevel.processChunkUnloads();

    if (mNetherLevel != nullptr)
        _tickDimension(*mNetherLevel);

    if (mTheEndLevel != nullptr)
        _tickDimension(*mTheEndLevel);

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
    }
    mProfiler.endSection(ProfilerSection::Fire);

    mProfiler.beginSection(ProfilerSection::Announcement);
    _updateServerAnnouncement();
    mProfiler.endSection(ProfilerSection::Announcement);

    mScriptEngine.tick(mCurrentTick);

    const ChunkWorker *chunkWorker = mLevel.getChunkWorker();
    mProfiler.endTick((uint32_t) mPlayers.size(), (uint32_t) mLevel.getLoadedChunkCount(),
                      chunkWorker == nullptr ? 0 : (uint32_t) chunkWorker->getPendingTaskCount(),
                      (uint32_t) mLevel.getLastFluidProcessedCount(),
                      (uint32_t) mLevel.getScheduledFluidCount());

    const double elapsedMs = std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - tickStart).count();

    mTickDurationSamples.push_back(elapsedMs);
    if (mTickDurationSamples.size() > TICK_SAMPLE_COUNT)
        mTickDurationSamples.pop_front();
}

void ServerNetworkHandler::_updateServerAnnouncement() {
    mAnnouncement.mCurrentPlayers = getActivePlayerCount();

    if (mRakNetInstance != nullptr)
        mRakNetInstance->announceServer(mAnnouncement);
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

void ServerNetworkHandler::onNewIncomingConnection(const NetworkIdentifier &id) {
    LOG_INFO(LogAreaID::Network, "Player connected: %s", id.toString().c_str());
}

void ServerNetworkHandler::onConnectionClosed(const NetworkIdentifier &id, DisconnectFailReason reason,
                                              const std::string &message) {
    (void) message;

    mRateLimiters.erase(id);

    ServerPlayer *player = _getPlayer(id);
    const std::string playerName = player != nullptr && !player->getName().empty()
                                   ? player->getName()
                                   : id.toString();
    if (player != nullptr && !player->getName().empty()) {
        stopSleep(*player);
        if (player->isRiding())
            RideSystem::dismount(*this, *player, false);
        RideSystem::ejectAll(*this, *player);
        _savePlayerData(*player);
        EnderChestInventoryStore::getInstance().remove(player->getUniqueId());

        if (player->isSpawned()) {
            broadcastTranslation("multiplayer.player.left", {player->getName()});
            _removeFromPlayerList(*player);

            PlayerLeaveAfterEvent leaveEvent(player->getName());
            mEventBus.after().mPlayerLeave.emit(leaveEvent);
        }
    }

    if (player != nullptr) {
        despawnPlayerForViewers(*player);
        getLevelFor(*player).unregisterAllChunkLoaders(player->getRuntimeId());
    }

    mModalFormCallbacks.erase(id);
    mPlayers.erase(id);
    LOG_INFO(LogAreaID::Network, "%s disconnected, reason: %s", playerName.c_str(), toString(reason));
}

int64_t ServerNetworkHandler::getUptimeSeconds() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - mStartTime).count();
}

double ServerNetworkHandler::getTicksPerSecond() const {
    if (mTickStartSamples.size() < 2)
        return 20.0;

    const double span = std::chrono::duration<double>(
            mTickStartSamples.back() - mTickStartSamples.front()).count();
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
    if (!player.isSpawned() || formData.empty() || formData.size() > 1024 * 1024 ||
        !JsonValidator(formData).parse())
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

void ServerNetworkHandler::_savePlayerData(const ServerPlayer &player) {
    if (mPlayerData.saveData(player.getName(), player.saveNbt(mLevel.getName())))
        LOG_INFO(LogAreaID::Server, "Saved player data for %s", player.getName().c_str());
    else
        LOG_WARN(LogAreaID::Server, "Could not save player data for %s", player.getName().c_str());
}

void ServerNetworkHandler::_loadPlayerData(ServerPlayer &player) {
    player.setPosition(mLevel.getSpawnPositionForPlayer());
    player.setOp(mOps.isOp(player.getName()));

    Tag data;
    if (!mPlayerData.loadData(player.getName(), data)) {
        player.resetFallDistance();
        return;
    }

    player.loadNbt(data, mCodecContext);
    player.resetFallDistance();

    if (player.getAttributes().get(HEALTH_ATTRIBUTE) <= 0.0f) {
        player.setDead(false);
        player.getAttributes().set(HEALTH_ATTRIBUTE, maxHealthOf(player));
        player.setPosition(mLevel.getSpawnPositionForPlayer());
        player.resetFallDistance();
    }

}

void ServerNetworkHandler::loadActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ) {
    const std::vector<Tag> entities = level.loadEntities(chunkX, chunkZ);

    for (const Tag &tag: entities) {
        const std::string identifier = tag.getString("identifier", std::string());
        if (identifier.empty())
            continue;

        const uint64_t runtimeId = allocateRuntimeId();
        const int64_t uniqueId = (int64_t) runtimeId;

        std::unique_ptr<ServerActor> actor;
        if (identifier == FallingBlockActor::IDENTIFIER)
            actor.reset(new FallingBlockActor(runtimeId, BlockState()));
        else if (identifier == PrimedTntActor::IDENTIFIER)
            actor.reset(new PrimedTntActor(runtimeId, PrimedTntActor::DEFAULT_FUSE));
        else
            actor.reset(new ServerActor(runtimeId, identifier));

        actor->getAttributes() = ActorAttributes::createActorDefaults();

        const CustomActorDefinition *definition = CustomContentRegistry::getInstance().getActorDefinition(identifier);
        if (definition != nullptr) {
            actor->setDefinition(definition);
            actor->setProjectile(definition->mIsProjectile);
        }

        actor->loadNbt(tag);
        actor->setDimension(level.getDimensionType());

        ServerActor *result = actor.get();
        mActors[uniqueId] = std::move(actor);

        broadcastActorSpawn(*result);
    }
}

void ServerNetworkHandler::loadBlockActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ) {
    level.getBlockActors().loadChunk(chunkX, chunkZ, level.loadBlockEntities(chunkX, chunkZ), mCodecContext);
}

void ServerNetworkHandler::saveBlockActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ, bool cull) {
    level.saveBlockEntities(chunkX, chunkZ, level.getBlockActors().saveChunk(chunkX, chunkZ));

    if (cull)
        level.getBlockActors().unloadChunk(chunkX, chunkZ);
}

void ServerNetworkHandler::saveActorsForChunk(Level &level, int32_t chunkX, int32_t chunkZ, bool cull) {
    std::vector<Tag> entities;
    std::vector<int64_t> culled;

    for (auto &entry: mActors) {
        ServerActor &actor = *entry.second;
        if (!actor.shouldSave() || actor.getDimension() != level.getDimensionType())
            continue;

        const Vector3f position = actor.getPosition();
        const int32_t actorChunkX = (int32_t) std::floor(position.x) >> 4;
        const int32_t actorChunkZ = (int32_t) std::floor(position.z) >> 4;
        if (actorChunkX != chunkX || actorChunkZ != chunkZ)
            continue;

        entities.push_back(actor.saveNbt());
        if (cull)
            culled.push_back(entry.first);
    }

    level.saveEntities(chunkX, chunkZ, entities);

    for (const int64_t uniqueId: culled) {
        auto it = mActors.find(uniqueId);
        if (it == mActors.end())
            continue;

        broadcastActorRemove(*it->second);
        mActors.erase(it);
    }
}

void ServerNetworkHandler::saveAllActors() {
    for (Level *level: getLevels()) {
        for (const int64_t column: mActorLoadedChunks[level->getDimensionId()]) {
            const int32_t chunkX = (int32_t) (column >> 32);
            const int32_t chunkZ = (int32_t) (column & 0xffffffff);
            saveActorsForChunk(*level, chunkX, chunkZ, false);
            saveBlockActorsForChunk(*level, chunkX, chunkZ, false);
        }
    }
}

void ServerNetworkHandler::autoSave() {
    if (!mLevel.isStorageOpen())
        return;

    const std::chrono::steady_clock::time_point start = std::chrono::steady_clock::now();

    for (auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (player.isSpawned() && !player.getName().empty())
            _savePlayerData(player);
    }

    saveWorldDynamicProperties();
    saveAllActors();

    mLevel.saveAll();
    if (mNetherLevel != nullptr)
        mNetherLevel->saveAll();
    if (mTheEndLevel != nullptr)
        mTheEndLevel->saveAll();

    const long long elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
    LOG_INFO(LogAreaID::Server, "Auto-saved the world in %lld ms", elapsed);
}

bool ServerNetworkHandler::syncActorPersistence(Level &level, const std::vector<int64_t> &activeColumns) {
    if (!level.isStorageOpen())
        return false;

    std::unordered_set<int64_t> &loadedChunks = mActorLoadedChunks[level.getDimensionId()];
    const std::unordered_set<int64_t> active(activeColumns.begin(), activeColumns.end());
    bool deferred = false;

    for (const int64_t column: active) {
        if (loadedChunks.count(column) != 0)
            continue;

        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);
        if (!level.isChunkResident(chunkX, chunkZ)) {
            deferred = true;
            continue;
        }

        loadActorsForChunk(level, chunkX, chunkZ);
        loadBlockActorsForChunk(level, chunkX, chunkZ);
        loadedChunks.insert(column);
    }

    std::vector<int64_t> unloaded;
    for (const int64_t column: loadedChunks) {
        if (active.count(column) == 0)
            unloaded.push_back(column);
    }

    for (const int64_t column: unloaded) {
        const int32_t chunkX = (int32_t) (column >> 32);
        const int32_t chunkZ = (int32_t) (column & 0xffffffff);
        saveActorsForChunk(level, chunkX, chunkZ, true);
        saveBlockActorsForChunk(level, chunkX, chunkZ, true);
        loadedChunks.erase(column);
    }

    return deferred;
}

void ServerNetworkHandler::onDataReceived(const NetworkIdentifier &id, const std::string &data) {
    if (!_allowPacket(id, RateLimitedPacket::Inbound))
        return;

    try {
        ReadOnlyBinaryStream stream(data);

        unsigned char senderSubId;
        unsigned char clientSubId;
        const MinecraftPacketIds packetId = Packet::peekId(stream, senderSubId, clientSubId);

        std::shared_ptr<Packet> packet = MinecraftPackets::createPacket(packetId);
        if (!packet) {
            LOG_TRACE(LogAreaID::Network, "Unhandled packet id %d from %s", (int) packetId, id.getAddress().c_str());
            return;
        }

        packet->mSenderSubId = senderSubId;
        packet->mClientSubId = clientSubId;

        {
            ProfilerScopedSection decodeSection(mProfiler, ProfilerSection::NetworkDecode, true);
            packet->read(stream, mCodecContext);
        }

        {
            ProfilerScopedSection handleSection(mProfiler, ProfilerSection::NetworkHandlePacket, true);
            packet->handle(id, *this);
        }
    } catch (const BinaryDataException &exception) {
        LOG_WARN(LogAreaID::Network, "Malformed packet from %s: %s", id.getAddress().c_str(), exception.what());
        _disconnect(id, "disconnectionScreen.unexpectedPacket");
    } catch (const std::exception &exception) {
        LOG_ERROR(LogAreaID::Network, "Unhandled exception while processing a packet from %s: %s",
                  id.getAddress().c_str(), exception.what());
        _disconnect(id, "disconnectionScreen.internalError.cantConnect");
    } catch (...) {
        LOG_ERROR(LogAreaID::Network, "Unknown exception while processing a packet from %s", id.getAddress().c_str());
        _disconnect(id, "disconnectionScreen.internalError.cantConnect");
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
        limiter = mRateLimiters.emplace(id, PacketRateLimiter(rates)).first;
    }

    if (limiter->second.tryAcquire(category))
        return true;

    if (category == RateLimitedPacket::Inbound && limiter->second.markFlooded()) {
        LOG_WARN(LogAreaID::Network, "Disconnecting %s for packet flooding", id.getAddress().c_str());
        _disconnect(id, "disconnectionScreen.unexpectedPacket");
    }

    return false;
}

ServerPlayer *ServerNetworkHandler::_getPlayer(const NetworkIdentifier &id) {
    auto it = mPlayers.find(id);
    return it == mPlayers.end() ? nullptr : &it->second;
}

void ServerNetworkHandler::_disconnect(const NetworkIdentifier &id, const std::string &reason) {
    DisconnectPacket disconnect;
    disconnect.mReason = 0;
    disconnect.mMessageSkipped = false;
    disconnect.mKickMessage = reason;

    mNetworkHandler->send(id, disconnect, mCodecContext);
    mNetworkHandler->flush(id);
}

void ServerNetworkHandler::_rejectBadPacket(const NetworkIdentifier &id, const std::string &reason) {
    LOG_WARN(LogAreaID::Network, "Bad packet from %s: %s", id.getAddress().c_str(), reason.c_str());
    _disconnect(id, "disconnectionScreen.unexpectedPacket");
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const RequestNetworkSettingsPacket &packet) {
    LoginHandler::handleRequestNetworkSettings(*this, id, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const LoginPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr) {
        _disconnect(id, "disconnectionScreen.unexpectedPacket");
        return;
    }

    LoginHandler::handleLogin(*this, id, *player, packet);
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

    for (const auto &entry: mPlayers) {
        if (entry.first == player.getNetworkIdentifier() || entry.second.isSpawned())
            mNetworkHandler->send(entry.first, entityData, mCodecContext);
    }
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
            _disconnect(entry.first, NOT_ALLOW_LISTED_MESSAGE);
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

void ServerNetworkHandler::_sendAttributes(ServerPlayer &player) {
    LoginHandler::sendAttributes(*this, player);
}

void ServerNetworkHandler::_sendHealth(ServerPlayer &player) {
    SetHealthPacket health;
    health.mHealth = (int32_t) std::ceil(player.getAttributes().get(HEALTH_ATTRIBUTE));

    mNetworkHandler->send(player.getNetworkIdentifier(), health, mCodecContext);

    _sendAttributes(player);
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

void ServerNetworkHandler::_handleFallDamage(ServerPlayer &player, const Block *supportBlock) {
    if (player.isFlying() || player.isDead())
        return;

    float damage = player.computeFallDamage();
    if (supportBlock != nullptr) {
        const std::optional<float> blockDamage = supportBlock->getFallDamage(player, damage);
        if (blockDamage.has_value())
            damage = std::max(0.0f, *blockDamage);
    }

    if (damage < 1.0f)
        return;

    applyDamage(player, damage, "death.fell.accident.generic", {player.getName()});
}

void ServerNetworkHandler::_handleVoidDamage(ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead())
        return;

    if (player.getPosition().y > (float) (LevelChunk::MIN_Y - 16))
        return;

    applyDamage(player, 10.0f, "death.attack.outOfWorld", {player.getName()});
}

bool ServerNetworkHandler::_isEyeInsideSolidBlock(Level &level, const Vector3f &position, float height) {
    const float eyeY = position.y + height * EYE_HEIGHT_FACTOR;
    const int32_t blockX = (int32_t) std::floor(position.x);
    const int32_t blockY = (int32_t) std::floor(eyeY);
    const int32_t blockZ = (int32_t) std::floor(position.z);

    if (blockY < level.getMinY() || blockY > level.getMaxY())
        return false;

    const BlockState *state = level.peekBlockPtr(blockX, blockY, blockZ);
    if (state == nullptr)
        return false;

    const BlockData *data = BlockDataTable::find(state->mName.c_str());
    if (data == nullptr || !data->mSolid || data->mTransparent)
        return false;

    return BlockShape::isPositionInside(*state, blockX, blockY, blockZ, position.x, eyeY, position.z);
}

void ServerNetworkHandler::_handleSuffocationDamage(ServerPlayer &player) {
    if (!player.isSpawned() || player.isDead())
        return;

    const int32_t gameType = player.getGameType();
    if (gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator)
        return;

    if (!_isEyeInsideSolidBlock(getLevelFor(player), player.getPosition(), PLAYER_COLLISION_HEIGHT))
        return;

    applyDamage(player, SUFFOCATION_DAMAGE, "death.attack.inWall", {player.getName()});
}

void ServerNetworkHandler::applyDamage(ServerPlayer &player, float amount, const std::string &deathMessageKey,
                                       const std::vector<std::string> &deathMessageParameters, bool applyArmor,
                                       bool respectCooldown) {
    if (!player.isSpawned() || player.isDead() || amount <= 0.0f)
        return;

    const int32_t gameType = player.getGameType();
    if (gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator)
        return;

    if (isDamageDisabledByGameRule(getLevelFor(player).getGameRules(), deathMessageKey))
        return;

    const float rawAmount = amount;

    if (respectCooldown && deathMessageKey != "death.attack.suicide" && player.getNoDamageTicks() > 0) {
        if (player.getLastDamageAmount() >= amount)
            return;
        amount -= player.getLastDamageAmount();
    }

    if (respectCooldown) {
        player.setNoDamageTicks(10);
        player.setLastDamageAmount(rawAmount);
    }

    if (applyArmor)
        amount = applyArmorModifiers(player, amount, deathMessageKey);

    if (deathMessageKey != "death.attack.outOfWorld" && deathMessageKey != "death.attack.suicide") {
        if (const MobEffectInstance *resistance = player.getEffect(MobEffectId::Resistance))
            amount *= 1.0f - std::min(1.0f, 0.2f * (float) resistance->level());
    }

    if (amount <= 0.0f)
        return;

    if (player.getHealth() - amount < 1.0f && deathMessageKey != "death.attack.outOfWorld" &&
        deathMessageKey != "death.attack.suicide" && TotemItem::consume(*this, player))
        return;

    const float health = player.reduceHealth(amount);

    EntityHurtAfterEvent hurtEvent(player, amount, deathMessageKey);
    mEventBus.after().mEntityHurt.emit(hurtEvent);

    if (health <= 0.0f) {
        killPlayer(player, deathMessageKey, deathMessageParameters);
        return;
    }

    _sendHealth(player);
    _broadcastEntityEvent(player, (uint8_t) EntityEventType::HurtAnimation);
}

void ServerNetworkHandler::killPlayer(ServerPlayer &player, const std::string &deathMessageKey,
                                      const std::vector<std::string> &deathMessageParameters) {
    if (player.isDead())
        return;

    stopSleep(player);
    if (player.isRiding())
        RideSystem::dismount(*this, player, false);
    RideSystem::ejectAll(*this, player);
    player.kill();
    player.setOnFire(false);
    _sendEntityData(player);

    EntityDieAfterEvent dieEvent(player, deathMessageKey);
    mEventBus.after().mEntityDie.emit(dieEvent);

    player.getInventoryManager().onCurrentWindowRemove();

    const GameRules &rules = getLevelFor(player).getGameRules();
    const bool keepInventory = rules.getBool("keepinventory");

    if (!keepInventory) {
        _dropInventoryOnDeath(player);

        if (player.getGameType() != (int32_t) GameType::Creative) {
            const int droppedExperience = player.getExperience().getXpDropAmount();
            if (droppedExperience > 0)
                spawnExperienceOrbs(getLevelFor(player), player.getPosition(), droppedExperience);
        }

        player.getExperience().reset();
        player.syncExperience();
        _sendAttributes(player);
    }

    _sendHealth(player);
    _broadcastEntityEvent(player, (uint8_t) EntityEventType::DeathAnimation);

    const std::string key = deathMessageKey.empty() ? "death.attack.generic" : deathMessageKey;
    const std::vector<std::string> parameters = deathMessageParameters.empty()
                                                ? std::vector<std::string>{player.getName()}
                                                : deathMessageParameters;
    if (rules.getBool("showdeathmessages"))
        broadcastTranslation(key, parameters);

    const Vector3f spawn = mLevel.getSpawnPositionForPlayer();

    RespawnPacket respawn;
    respawn.mPosition = Vector3f(spawn.x, spawn.y + PLAYER_BASE_OFFSET, spawn.z);
    respawn.mState = RespawnPacket::State::ServerSearching;
    respawn.mRuntimeActorId = player.getRuntimeId();
    mNetworkHandler->send(player.getNetworkIdentifier(), respawn, mCodecContext);

    DeathInfoPacket info;
    info.mCauseAttackName = key;
    info.mMessageList = parameters;
    mNetworkHandler->send(player.getNetworkIdentifier(), info, mCodecContext);

    LOG_INFO(LogAreaID::Server, "%s died", player.getName().c_str());
}

void ServerNetworkHandler::_throwItem(ServerPlayer &player, const ItemStack &item) {
    if (item.isAir() || item.mCount <= 0)
        return;

    const Vector3f position = player.getPosition();
    const Vector3f dropPosition(position.x, position.y + ITEM_DROP_HEIGHT, position.z);

    const float yaw = player.getRotation().y * MathConstants::PI_F / 180.0f;
    const float pitch = player.getRotation().x * MathConstants::PI_F / 180.0f;

    const Vector3f motion(-std::sin(yaw) * std::cos(pitch) * THROW_SPEED,
                          -std::sin(pitch) * THROW_SPEED + 0.1f,
                          std::cos(yaw) * std::cos(pitch) * THROW_SPEED);

    dropItem(getLevelFor(player), dropPosition, item, motion, THROW_PICKUP_DELAY);
}

void ServerNetworkHandler::_dropInventoryOnDeath(ServerPlayer &player) {
    PlayerInventory &inventory = player.getInventory();
    Level &level = getLevelFor(player);
    const Vector3f position = player.getPosition();
    const Vector3f dropPosition(position.x, position.y + ITEM_DROP_HEIGHT, position.z);

    const auto dropOnDeath = [&](const ItemStack &item) {
        if (ItemEnchantments::getLevel(item, EnchantmentIds::VANISHING) > 0)
            return;

        dropItem(level, dropPosition, item, ItemActorHandler::randomDropAroundMotion(),
                 ItemActorHandler::DEATH_DROP_PICKUP_DELAY);
    };

    for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; slot++)
        dropOnDeath(inventory.getItem(slot));

    for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; slot++)
        dropOnDeath(inventory.getArmor(slot));

    dropOnDeath(inventory.getOffhand());
    dropOnDeath(inventory.getCursor());

    for (int slot = 0; slot < PlayerInventory::CRAFTING_SIZE; ++slot)
        dropOnDeath(inventory.getCraftingItem(slot));

    for (int slot = 0; slot < PlayerInventory::CRAFTING_TABLE_SIZE; ++slot)
        dropOnDeath(inventory.getCraftingTableItem(slot));

    for (int slot = 0; slot < PlayerInventory::FURNACE_SIZE; ++slot)
        dropOnDeath(inventory.getFurnaceItem(slot));

    inventory.clear();
    inventory.setSelectedSlot(0);

    _sendInventory(player);
}

bool ServerNetworkHandler::sleepOn(ServerPlayer &player, const Vector3i &head) {
    for (const auto &entry: mPlayers) {
        const ServerPlayer &other = entry.second;
        if (&other != &player && other.isSleeping() && other.getSleepingPosition() == head)
            return false;
    }

    player.setSleeping(head);
    player.setSpawnPoint(head);
    player.getFlags().set(ActorFlag::Sleeping, true);
    player.teleport(*this, Vector3f((float) head.x + 0.5f, (float) head.y + 0.5f, (float) head.z + 0.5f));
    BedBlock::setOccupied(*this, getLevelFor(player), head, true);
    _sendEntityData(player);

    mSleepTicks = SLEEP_CHECK_DELAY_TICKS;
    return true;
}

void ServerNetworkHandler::stopSleep(ServerPlayer &player) {
    if (!player.isSleeping())
        return;

    const Vector3i head = player.getSleepingPosition();
    player.clearSleeping();
    player.getFlags().set(ActorFlag::Sleeping, false);
    _sendEntityData(player);

    bool bedStillUsed = false;
    for (const auto &entry: mPlayers) {
        if (entry.second.isSleeping() && entry.second.getSleepingPosition() == head)
            bedStillUsed = true;
    }
    if (!bedStillUsed)
        BedBlock::setOccupied(*this, getLevelFor(player), head, false);

    mSleepTicks = 0;

    AnimatePacket wakeUp;
    wakeUp.mRuntimeActorId = player.getRuntimeId();
    wakeUp.mAction = AnimatePacket::Action::WakeUp;
    mNetworkHandler->send(player.getNetworkIdentifier(), wakeUp, mCodecContext);
}

void ServerNetworkHandler::wakeSleepersAt(const Vector3i &head) {
    for (auto &entry: mPlayers) {
        if (entry.second.isSleeping() && entry.second.getSleepingPosition() == head)
            stopSleep(entry.second);
    }
}

void ServerNetworkHandler::_tickSleep() {
    if (mSleepTicks <= 0 || --mSleepTicks > 0)
        return;

    int players = 0;
    int sleeping = 0;
    for (const auto &entry: mPlayers) {
        const ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.getDimension() != DimensionType::Overworld)
            continue;

        players++;
        if (player.isSleeping())
            sleeping++;
    }

    const int32_t percentage = mLevel.getGameRules().getInt("playerssleepingpercentage");
    if (players == 0 || sleeping == 0 || sleeping * 100 / players < percentage)
        return;

    if (!mLevel.isNight() && !mLevel.isThundering())
        return;

    mLevel.setTime(mLevel.getTime() + DAY_LENGTH_TICKS - mLevel.getDayTime());
    broadcastWorldTime();

    for (auto &entry: mPlayers) {
        if (entry.second.isSleeping())
            stopSleep(entry.second);
    }
}

Vector3f ServerNetworkHandler::_respawnPositionFor(ServerPlayer &player) {
    if (!player.hasSpawnPoint())
        return mLevel.getSpawnPositionForPlayer();

    const Vector3i bed = player.getSpawnPoint();
    if (!BedBlock::isValidAt(mLevel, bed)) {
        player.clearSpawnPoint();
        player.sendTranslation("§7%tile.bed.notValid", {});
        return mLevel.getSpawnPositionForPlayer();
    }

    return Vector3f((float) bed.x + 0.5f, (float) bed.y + BED_HEIGHT, (float) bed.z + 0.5f);
}

void ServerNetworkHandler::_respawnPlayer(ServerPlayer &player) {
    if (!player.isDead())
        return;

    const Vector3f spawn = _respawnPositionFor(player);

    if (player.getDimension() != DimensionType::Overworld)
        changePlayerDimension(player, DimensionType::Overworld, spawn);

    player.setDead(false);
    player.getAttributes().set(HEALTH_ATTRIBUTE, maxHealthOf(player));
    player.teleport(spawn);
    player.markTeleported();
    player.setRotation(Vector3f(0.0f, 0.0f, 0.0f));
    player.clearPendingMove();
    player.resetAirSupply();

    const Vector3f eyePosition(spawn.x, spawn.y + PLAYER_BASE_OFFSET, spawn.z);

    RespawnPacket respawn;
    respawn.mPosition = eyePosition;
    respawn.mState = RespawnPacket::State::ServerReady;
    respawn.mRuntimeActorId = player.getRuntimeId();
    mNetworkHandler->send(player.getNetworkIdentifier(), respawn, mCodecContext);

    MovePlayerPacket move;
    move.mRuntimeActorId = (int64_t) player.getRuntimeId();
    move.mPosition = eyePosition;
    move.mRotation = player.getRotation();
    move.mMode = MovePlayerMode::Respawn;
    move.mOnGround = true;
    mNetworkHandler->send(player.getNetworkIdentifier(), move, mCodecContext);

    _sendHealth(player);
    _sendEntityData(player);
    _sendAbilities(player);
    _sendInventory(player);
    ItemActorHandler::sendItemActorsTo(*this, player);
    sendActorsTo(player);
    _sendChunks(player);

    LOG_INFO(LogAreaID::Server, "%s respawned at %.2f %.2f %.2f", player.getName().c_str(), spawn.x, spawn.y,
             spawn.z);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PlayerActionPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    switch (packet.mAction) {
        case PlayerActionType::StartBreak:
        case PlayerActionType::BlockContinueDestroy:
            if (player->hasLastBlockAttacked() && player->getLastBlockAttacked() == packet.mBlockPosition)
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
        case PlayerActionType::StopBreak:
            BlockActionHandler::stopBreakingBlock(*this, *player);
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
    if (player == nullptr || !player->isSpawned() || packet.mRuntimeActorId != player->getRuntimeId() ||
        packet.mEmoteId.empty() || packet.mEmoteId.size() > 256)
        return;

    EmotePacket emote = packet;
    emote.mRuntimeActorId = player->getRuntimeId();
    emote.mXuid = player->getXuid();

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.getNetworkIdentifier() != id)
            mNetworkHandler->send(entry.second.getNetworkIdentifier(), emote, mCodecContext);
    }
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
    if (!cancelled && (packet.mFormData.empty() || packet.mFormData.size() > 1024 * 1024 ||
                       !JsonValidator(packet.mFormData).parse()))
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
        callback(*player, cancelled ? std::string() : packet.mFormData, cancelled);
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
    if (currentSkin.mFullSkinId == skin.mFullSkinId && currentSkin.mSkinData.mWidth == skin.mSkinData.mWidth &&
        currentSkin.mSkinData.mHeight == skin.mSkinData.mHeight && currentSkin.mSkinData.mData == skin.mSkinData.mData)
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
    bool isSignBlock(const std::string &identifier);
    void putPickedItem(ServerPlayer &player, ItemStack item, int hotbarSlot);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const BlockActorDataPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    const Vector3f position = player->getPosition();
    const float dx = position.x - ((float) packet.mBlockPosition.x + 0.5f);
    const float dy = position.y - ((float) packet.mBlockPosition.y + 0.5f);
    const float dz = position.z - ((float) packet.mBlockPosition.z + 0.5f);
    if (dx * dx + dy * dy + dz * dz > 10000.0f)
        return;

    const BlockState state = getLevelFor(*player).getBlockState(packet.mBlockPosition.x, packet.mBlockPosition.y,
                                                                packet.mBlockPosition.z);
    if (!packet.mData.isCompound())
        return;

    const bool hasSignText = packet.mData.contains("FrontText") || packet.mData.contains("BackText") ||
                             packet.mData.contains("Text1") || packet.mData.contains("Text2") ||
                             packet.mData.contains("Text3") || packet.mData.contains("Text4");
    if (!isSignBlock(state.mName) && !hasSignText)
        return;

    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.second.getNetworkIdentifier(), packet, mCodecContext);
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
        if (actor->isRemoved() || actor->getRuntimeId() != packet.mRuntimeActorId ||
            actor->getDimension() != player->getDimension())
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

void ServerNetworkHandler::broadcastTranslation(const std::string &key,
                                                const std::vector<std::string> &parameters) {
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            entry.second.sendTranslation(key, parameters);
    }
}

void ServerNetworkHandler::broadcastWorldTime() {
    SetTimePacket packet;
    packet.mTime = (int32_t) getLevel().getDayTime();
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            mNetworkHandler->send(entry.first, packet, mCodecContext);
    }
}

void ServerNetworkHandler::sendPacketTo(const NetworkIdentifier &id, const Packet &packet) {
    mNetworkHandler->send(id, packet, mCodecContext);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PlayerAuthInputPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || player->isDead())
        return;

    if (!_allowPacket(id, RateLimitedPacket::Movement))
        return;

    if (!std::isfinite(packet.mPosition.x) || !std::isfinite(packet.mPosition.y) ||
        !std::isfinite(packet.mPosition.z) || !std::isfinite(packet.mRotation.x) ||
        !std::isfinite(packet.mRotation.y) || !std::isfinite(packet.mRotation.z)) {
        return;
    }

    std::string badPacketReason;
    if (BadPacketHandler::inspect(*player, packet, badPacketReason)) {
        _rejectBadPacket(id, badPacketReason);
        return;
    }

    MovementHandler::handlePlayerAuthInput(*this, id, *player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const CompletedUsingItemPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    if (packet.mType != ItemUseType::Eat && packet.mType != ItemUseType::Consume)
        return;

    if (!player->getFlags().get(ActorFlag::UsingItem))
        return;

    const int64_t heldTicks = mCurrentTick - player->getItemUseStartTick();
    if (heldTicks < useDurationTicksFor(player->getInventory().getItemInHand()))
        return;

    const ItemStack &heldItem = player->getInventory().getItemInHand();
    if (heldItem.isAir() || heldItem.mDefinition == nullptr ||
        static_cast<uint16_t>(packet.mItemId) != static_cast<uint16_t>(heldItem.mDefinition->getRuntimeId()))
        return;

    _consumeHeldItem(*player);
}

namespace {
    bool isSignBlock(const std::string &identifier) {
        return identifier.find("sign") != std::string::npos;
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

        const int slot = hotbarSlot >= 0 && hotbarSlot < PlayerInventory::HOTBAR_SIZE
                             ? hotbarSlot
                             : inventory.getSelectedSlot();

        item.mCount = 1;
        inventory.setItem(slot, std::move(item));
        inventory.setSelectedSlot(slot);
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        player.getInventoryManager().syncSelectedHotbarSlot();
    }

    const FoodItemComponent *findFoodComponent(const ItemStack &item) {
        if (item.isAir() || item.mDefinition == nullptr)
            return nullptr;

        const ItemComponents &components = ItemDataTable::getComponents(item.mDefinition->getIdentifier());
        const FoodItemComponent *food = components.get<FoodItemComponent>();
        if (food == nullptr || !food->isEdible())
            return nullptr;

        return food;
    }

    void applyFoodEffects(EventBus &bus, ServerPlayer &player, const FoodItemComponent &food) {
        for (const FoodEffect &effect: food.getEffects()) {
            MobEffectInstance instance;
            instance.mId = (MobEffectId) effect.mEffectId;
            instance.mAmplifier = effect.mAmplifier;
            instance.mDuration = effect.mDurationTicks;

            if (player.addEffect(instance)) {
                EffectAddAfterEvent event(player, effect.mEffectId, effect.mAmplifier, effect.mDurationTicks);
                bus.after().mEffectAdd.emit(event);
            }
        }
    }
}

void ServerNetworkHandler::emitItemUse(ServerPlayer &player) {
    const ItemStack &heldItem = player.getInventory().getItemInHand();
    if (heldItem.isAir() || heldItem.mDefinition == nullptr)
        return;

    static const int64_t ITEM_USE_DEBOUNCE_TICKS = 4;
    if (mCurrentTick - player.getLastItemUseTick() < ITEM_USE_DEBOUNCE_TICKS)
        return;
    player.setLastItemUseTick(mCurrentTick);

    ItemUseAfterEvent useEvent(player, heldItem.mDefinition->getIdentifier());
    mEventBus.after().mItemUse.emit(useEvent);
}

bool ServerNetworkHandler::_equipHeldArmor(ServerPlayer &player, const Item &itemType) {
    const ArmorSlot slot = itemType.getArmorSlot();
    if (slot == ArmorSlot::None)
        return false;

    int armorSlot = PlayerInventory::ARMOR_HEAD;
    switch (slot) {
        case ArmorSlot::Head:
            armorSlot = PlayerInventory::ARMOR_HEAD;
            break;
        case ArmorSlot::Chest:
            armorSlot = PlayerInventory::ARMOR_TORSO;
            break;
        case ArmorSlot::Legs:
            armorSlot = PlayerInventory::ARMOR_LEGS;
            break;
        case ArmorSlot::Feet:
            armorSlot = PlayerInventory::ARMOR_FEET;
            break;
        default:
            return false;
    }

    PlayerInventory &inventory = player.getInventory();

    ItemStack worn = inventory.getArmor(armorSlot);
    ItemStack held = inventory.getItemInHand();

    inventory.setArmor(armorSlot, held);
    inventory.setItemInHand(std::move(worn));

    player.getInventoryManager().syncContents(InventoryManager::InventoryId::Armor);
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                          inventory.getSelectedSlot());
    InventoryHandler::sendArmorContent(*this, player);
    InventoryHandler::sendHeldItem(*this, player);

    playLevelSound(getLevelFor(player), LevelSoundEvent::ARMOR_EQUIP_GENERIC, player.getPosition(),
                   "minecraft:player");
    return true;
}

void ServerNetworkHandler::_useHeldItem(ServerPlayer &player) {
    const ItemStack &heldItem = player.getInventory().getItemInHand();

    emitItemUse(player);

    if (!heldItem.isAir() && heldItem.mDefinition != nullptr) {
        const Item *itemType = VanillaItems::fromIdentifier(heldItem.mDefinition->getIdentifier());
        if (itemType != nullptr && _equipHeldArmor(player, *itemType))
            return;

        if (itemType != nullptr && itemType->onUse(*this, player, heldItem))
            return;

        if (itemType != nullptr && !player.getFlags().get(ActorFlag::UsingItem)
            && itemType->onStartUsing(*this, player, heldItem)) {
            player.getFlags().set(ActorFlag::UsingItem, true);
            player.setItemUseStartTick(mCurrentTick);
            _sendEntityData(player);
            return;
        }
    }

    const bool isMilk = !heldItem.isAir() && heldItem.mDefinition != nullptr
                        && std::string(heldItem.mDefinition->getIdentifier()) == "minecraft:milk_bucket";
    const bool isPotion = !heldItem.isAir() && heldItem.mDefinition != nullptr
                          && std::string(heldItem.mDefinition->getIdentifier()) == "minecraft:potion";
    const FoodItemComponent *food = findFoodComponent(heldItem);
    if (food == nullptr && !isMilk && !isPotion)
        return;

    if (!isMilk && !isPotion && ChorusFruitItem::isChorusFruit(heldItem)
        && !ChorusFruitItem::canConsume(*this, player))
        return;

    if (player.isAwaitingConsumableRelease())
        return;

    if (mCurrentTick - player.getLastEarlyConsumableReleaseTick() < EARLY_CONSUMABLE_RELEASE_BLOCK_TICKS)
        return;

    if (player.hasItemCooldown(heldItem, mCurrentTick))
        return;

    const bool usingItem = player.getFlags().get(ActorFlag::UsingItem);

    if (!isMilk && !isPotion && !food->canAlwaysEat() && !player.canEat()) {
        if (usingItem) {
            player.getFlags().set(ActorFlag::UsingItem, false);
            _sendEntityData(player);
        }
        return;
    }

    if (usingItem)
        return;

    player.getFlags().set(ActorFlag::UsingItem, true);
    player.setItemUseStartTick(mCurrentTick);
    _sendEntityData(player);

    if (heldItem.mDefinition != nullptr) {
        ItemStartUseAfterEvent event(player, heldItem.mDefinition->getIdentifier(),
                                     (int32_t) useDurationTicksFor(heldItem));
        mEventBus.after().mItemStartUse.emit(event);
    }
}

void ServerNetworkHandler::_consumeHeldItem(ServerPlayer &player) {
    const bool wasUsing = player.getFlags().get(ActorFlag::UsingItem);
    const int64_t heldTicks = mCurrentTick - player.getItemUseStartTick();

    PlayerInventory &inventory = player.getInventory();
    const int slot = inventory.getSelectedSlot();

    const ItemStack &heldItem = inventory.getItemInHand();
    const bool isMilk = !heldItem.isAir() && heldItem.mDefinition != nullptr
                        && std::string(heldItem.mDefinition->getIdentifier()) == "minecraft:milk_bucket";
    const bool isPotion = !heldItem.isAir() && heldItem.mDefinition != nullptr
                          && std::string(heldItem.mDefinition->getIdentifier()) == "minecraft:potion";
    const FoodItemComponent *food = findFoodComponent(heldItem);
    const bool consumable = food != nullptr || isMilk || isPotion;

    if (wasUsing && consumable && heldTicks < useDurationTicksFor(heldItem))
        return;

    if (wasUsing) {
        player.getFlags().set(ActorFlag::UsingItem, false);
        _sendEntityData(player);
    }

    if (!consumable)
        return;

    if (!isMilk && ChorusFruitItem::isChorusFruit(heldItem)
        && !ChorusFruitItem::canConsume(*this, player)) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        return;
    }

    const ItemStack usedItem = heldItem;

    if (!wasUsing) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        return;
    }

    if (!isMilk && !isPotion && !food->canAlwaysEat() && !player.canEat()) {
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
        _sendAttributes(player);
        return;
    }

    if (usedItem.mDefinition != nullptr) {
        const std::string identifier = usedItem.mDefinition->getIdentifier();
        ItemCompleteUseAfterEvent completeEvent(player, identifier, (int32_t) useDurationTicksFor(usedItem));
        mEventBus.after().mItemCompleteUse.emit(completeEvent);
    }

    if (isMilk) {
        player.getEffects().clear();
    } else if (isPotion) {
        applyPotionEffects(player, usedItem.mDamage, 1.0f);
    } else {
        if (ChorusFruitItem::isChorusFruit(usedItem))
            ChorusFruitItem::onEaten(*this, player);

        player.consumeFood(food->getNutrition(), food->getSaturation());
        applyFoodEffects(mEventBus, player, *food);

        LevelSoundEventPacket burp;
        burp.mSound = LevelSoundEvent::BURP;
        burp.mPosition = player.getPosition();
        burp.mExtraData = -1;
        burp.mActorType = ":";
        burp.mIsBabyMob = false;
        burp.mDisableRelativeVolume = false;
        burp.mActorUniqueId = -1;
        burp.mHasFirePosition = false;
        BlockActionHandler::broadcastToViewers(*this, getLevelFor(player), player.getPosition(), burp);
    }

    ItemStack remaining = inventory.getItemInHand();
    remaining.mCount -= 1;
    if (remaining.mCount <= 0) {
        if (isMilk) {
            remaining = ItemStack::air();
            remaining.mDefinition = mItemDefinitions.getDefinition("minecraft:bucket");
            remaining.mBlockDefinition = mBlockDefinitions.getDefinition("minecraft:bucket");
            remaining.mCount = remaining.mDefinition == nullptr ? 0 : 1;
        } else {
            remaining = ItemStack::air();
        }
    }

    inventory.setItemInHand(std::move(remaining));
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

    player.startItemCooldown(usedItem, mCurrentTick);
    player.setAwaitingConsumableRelease();

    _sendAttributes(player);

}

namespace {
    std::string toLowerCopy(const std::string &value) {
        std::string lowered = value;
        for (char &character: lowered) {
            if (character >= 'A' && character <= 'Z')
                character = (char) (character - 'A' + 'a');
        }
        return lowered;
    }
}

ServerPlayer *ServerNetworkHandler::getPlayerByName(const std::string &name) {
    for (auto &entry: mPlayers) {
        if (entry.second.getName() == name)
            return &entry.second;
    }

    const std::string lowered = toLowerCopy(name);

    for (auto &entry: mPlayers) {
        if (toLowerCopy(entry.second.getName()) == lowered)
            return &entry.second;
    }

    ServerPlayer *partial = nullptr;
    for (auto &entry: mPlayers) {
        const std::string candidate = toLowerCopy(entry.second.getName());
        if (candidate.compare(0, lowered.size(), lowered) != 0)
            continue;

        if (partial != nullptr)
            return nullptr;

        partial = &entry.second;
    }

    return partial;
}

std::vector<std::string> ServerNetworkHandler::getPlayerNames() const {
    std::vector<std::string> names;
    names.reserve(mPlayers.size());

    for (const auto &entry: mPlayers) {
        if (!entry.second.getName().empty())
            names.push_back(entry.second.getName());
    }

    return names;
}

std::vector<ServerPlayer *> ServerNetworkHandler::resolveTargets(CommandOrigin &sender,
                                                                 const std::string &selector) {
    std::vector<ServerPlayer *> targets;

    if (selector == "@a" || selector == "@e") {
        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned())
                targets.push_back(&entry.second);
        }
        return targets;
    }

    if (selector == "@s" || selector == "@p") {
        ServerPlayer *self = sender.asPlayer();
        if (self != nullptr)
            targets.push_back(self);
        return targets;
    }

    if (selector == "@r") {
        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned()) {
                targets.push_back(&entry.second);
                break;
            }
        }
        return targets;
    }

    ServerPlayer *named = getPlayerByName(selector);
    if (named != nullptr)
        targets.push_back(named);

    return targets;
}

void ServerNetworkHandler::setPlayerGameMode(ServerPlayer &player, int gameMode) {
    const int32_t previousGameMode = player.getGameType();

    player.setGameType(gameMode);
    player.setHungerEnabled(gameMode == (int32_t) GameType::Survival || gameMode == (int32_t) GameType::Adventure);

    const bool mayFly = gameMode == (int32_t) GameType::Creative || gameMode == (int32_t) GameType::Spectator;
    if (!mayFly && player.isFlying()) {
        player.setFlying(false);
        player.setOnGround(MovementHandler::checkGroundState(getLevelFor(player), player.getPosition()));
    }

    if (gameMode == (int32_t) GameType::Spectator) {
        player.setFlying(true);
        player.setOnGround(false);
    }

    _sendAbilities(player);

    SetPlayerGameTypePacket packet;
    packet.mGamemode = gameMode;
    mNetworkHandler->send(player.getNetworkIdentifier(), packet, mCodecContext);

    player.resetFallDistance();

    if (previousGameMode != gameMode) {
        PlayerGameModeChangeAfterEvent event(player, previousGameMode, gameMode);
        mEventBus.after().mPlayerGameModeChange.emit(event);
    }
}

void ServerNetworkHandler::sendCommandOutput(ServerPlayer &player, const CommandOriginData &origin,
                                             const std::string &message) {
    CommandOutputPacket output;
    output.mOrigin = origin;
    output.mType = CommandOutputType::AllOutput;
    output.mSuccessCount = 1;

    CommandOutputMessage line;
    line.mInternal = false;
    line.mMessageId = message;
    output.mMessages.push_back(line);

    mNetworkHandler->send(player.getNetworkIdentifier(), output, mCodecContext);
}

void ServerNetworkHandler::sendCommandOutput(ServerPlayer &player, const CommandOriginData &origin,
                                             const std::string &key,
                                             const std::vector<std::string> &parameters) {
    CommandOutputPacket output;
    output.mOrigin = origin;
    output.mType = CommandOutputType::AllOutput;
    output.mSuccessCount = 1;

    CommandOutputMessage line;
    line.mInternal = false;
    line.mMessageId = key;
    line.mParameters = parameters;
    output.mMessages.push_back(std::move(line));

    mNetworkHandler->send(player.getNetworkIdentifier(), output, mCodecContext);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const TextPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

    if (packet.mType != TextPacket::Type::Chat || !_allowPacket(id, RateLimitedPacket::Chat))
        return;

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


    ChunkStreamHandler::handleViewDistanceChange(*this, *player);
    _sendChunks(*player);
    _checkTerrainReady(*player);
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

    const int32_t gameType = player->getGameType();
    const bool mayFly = gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator;

    if (packet.mBoolValue && !mayFly) {
        _disconnect(id, player->localize("falcon.disconnect.flyingDisabled"));
        mPlayers.erase(id);
        return;
    }

    player->setFlying(packet.mBoolValue);

    if (!packet.mBoolValue)
        player->setOnGround(MovementHandler::checkGroundState(getLevelFor(*player), player->getPosition()));

    _sendAbilities(*player);
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

    if (packet.mRuntimeActorId != player->getRuntimeId())
        return;

    InventoryHandler::handleOpenInventory(*this, *player);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const ItemStackRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !player->isSpawned())
        return;

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

    InventoryHandler::handleContainerClose(*player, packet);
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const PacketViolationWarningPacket &packet) {
    (void) id;

    const char *causeName = toString((MinecraftPacketIds) packet.mPacketCauseId);
    LOG_ERROR(LogAreaID::Network, "CLIENT REJECTED PACKET id=%d (%s), type=%d, severity=%d, context: %s",
              packet.mPacketCauseId, causeName, (int) packet.mType, (int) packet.mSeverity, packet.mContext.c_str());
}

void ServerNetworkHandler::handle(const NetworkIdentifier &id, const CommandRequestPacket &packet) {
    ServerPlayer *player = _getPlayer(id);
    if (player == nullptr || !_allowPacket(id, RateLimitedPacket::Command))
        return;

    LOG_INFO(LogAreaID::Server, "%s issued command: %s", player->getName().c_str(), packet.mCommand.c_str());

    PlayerCommandOrigin sender(*this, *player, packet.mOrigin);
    mCommands.dispatch(sender, packet.mCommand);
}

void ServerNetworkHandler::onReceiveIPSupport(RakPeerHelper::IPSupport support) {
    if (mRakNetInstance == nullptr)
        return;

    const ConnectionDefinition &definition = mRakNetInstance->getConnectionDefinition();

    if (support == RakPeerHelper::IPSupport::IPv4 || support == RakPeerHelper::IPSupport::Both)
        LOG_INFO(LogAreaID::Network, "IPv4 supported, port: %u: Used for gameplay", definition.mPort);

    if (support == RakPeerHelper::IPSupport::IPv6 || support == RakPeerHelper::IPSupport::Both)
        LOG_INFO(LogAreaID::Network, "IPv6 supported, port: %u: Used for gameplay", definition.mPortV6);
}
