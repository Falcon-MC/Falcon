#include "Network/Handler/BlockActionHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"

#include "World/Item/ItemProperties.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Level/Block/Components/BlockPlacementComponent.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/BlockPickItem.h"
#include "Core/Math/MathConstants.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Definition/BlockSensor.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Player/ExperienceValues.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Item/ItemData.h"
#include "World/Item/VanillaItems.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/BucketItem.h"
#include "World/Item/StringToItemParser.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Level.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Falcon/Plugin/PluginBlock.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/PlacedBlockAliases.h"
#include "Protocol/Types/StartGameTypes.h"
#include "World/Actor/Movement/MovementConstants.h"
#include "Core/Math/VectorMath.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace {
    const float MOVEMENT_BLOCK_RANGE = 3.0f;

    bool isNearPlayer(const ServerPlayer &player, const Vector3f &position) {
        const Vector3f &feet = player.getPosition();
        const float dx = position.x - feet.x;
        const float dy = position.y - feet.y;
        const float dz = position.z - feet.z;
        return dx * dx + dy * dy + dz * dz <= MOVEMENT_BLOCK_RANGE * MOVEMENT_BLOCK_RANGE;
    }

    using MovementPhysics::PLAYER_BASE_OFFSET;
    const double BREAK_SPEED_CHANGE_EPSILON = 0.0001;
    const double BREAK_PROGRESS_EPSILON = 0.000001;
    // progress counts server ticks from the tick after the start, not input ticks:
    // a client-predicted destroy can arrive up to this many ticks early
    const int64_t BREAK_TICK_TOLERANCE = 2;
    const float REACH_MAX_DIFF = 6.0f;
    const uint64_t RIGHT_CLICK_DEDUP_MICROSECONDS = 100000;

    bool isPlacementIgnoredActor(const std::string &identifier) {
        return identifier == "minecraft:xp_orb" || identifier == "minecraft:item"
               || identifier == "minecraft:area_effect_cloud" || identifier == "minecraft:fireworks_rocket"
               || identifier == "minecraft:painting";
    }

    AxisAlignedBB actorBox(const Vector3f &position, const ActorSize &size) {
        const float half = size.mWidth * 0.5f;
        return AxisAlignedBB(position.x - half, position.y, position.z - half, position.x + half,
                             position.y + size.mHeight, position.z + half);
    }


    float toolSpeedMultiplier(BlockToolTier tier) {
        switch (tier) {
            case BlockToolTier::Wooden:
                return 2.0f;
            case BlockToolTier::Stone:
                return 4.0f;
            case BlockToolTier::Iron:
                return 6.0f;
            case BlockToolTier::Diamond:
                return 8.0f;
            case BlockToolTier::Netherite:
                return 9.0f;
            case BlockToolTier::Gold:
                return 12.0f;
            default:
                return 1.0f;
        }
    }

    double calculateBreakSeconds(ServerPlayer &player, const BlockData *blockData) {
        if (blockData == nullptr) {
            return 0.05;
        }

        if (player.isCreative())
            return 0.0;

        if (blockData->mHardness < 0.0f)
            return -1.0;

        const double blockHardness = (double) blockData->mHardness;
        const ItemStack &heldItem = player.getInventory().getItemInHand();
        const ItemData *itemData = heldItem.isAir() || heldItem.mDefinition == nullptr
                                           ? nullptr
                                           : ItemDataTable::find(heldItem.mDefinition->getIdentifier());

        bool customTool = false;
        int32_t customSpeed = 0;
        bool customUseEfficiency = false;
        if (itemData == nullptr && !heldItem.isAir() && heldItem.mDefinition != nullptr) {
            const bool standardTool = blockData->mToolType == BlockToolType::Pickaxe
                                      || blockData->mToolType == BlockToolType::Shovel
                                      || blockData->mToolType == BlockToolType::Axe
                                      || blockData->mToolType == BlockToolType::Hoe;
            customSpeed = CustomContentRegistry::getInstance().getDiggerSpeed(
                    heldItem.mDefinition->getIdentifier(), blockData->mName, standardTool, customUseEfficiency);
            customTool = customSpeed > 0;
        }

        const Item *heldType = itemData == nullptr
                                       ? nullptr
                                       : VanillaItems::fromIdentifier(heldItem.mDefinition->getIdentifier());
        float itemSpeed = 1.0f;
        const bool itemDefinesSpeed = heldType != nullptr
                                      && heldType->getDestroySpeed(blockData->mIdentifier, itemSpeed);

        const bool matchesTool = customTool
                                 || (itemData != nullptr && blockData->mToolType != BlockToolType::None
                                     && (int32_t) itemData->mToolType == (int32_t) blockData->mToolType
                                     && itemData->mToolTier >= (int32_t) blockData->mToolTier);

        const bool canHarvest = BlockActionHandler::canHarvest(*blockData, heldItem);

        double seconds = canHarvest ? blockHardness * 1.5 : blockHardness * 5.0;

        float speedMultiplier = 1.0f;
        const bool correctTool = matchesTool || itemDefinesSpeed;

        if (itemDefinesSpeed)
            speedMultiplier = itemSpeed;
        else if (correctTool)
            speedMultiplier = customTool ? (float) customSpeed
                                         : toolSpeedMultiplier(
                                                   (BlockToolTier) (itemData == nullptr ? 0 : itemData->mToolTier));

        const int32_t efficiencyLevel = ItemEnchantments::getLevel(heldItem, EnchantmentIds::EFFICIENCY);
        if (correctTool && canHarvest && efficiencyLevel > 0 && (!customTool || customUseEfficiency))
            speedMultiplier += (float) (efficiencyLevel * efficiencyLevel + 1);

        const MobEffectInstance *haste = player.getEffect(MobEffectId::Haste);
        const MobEffectInstance *fatigue = player.getEffect(MobEffectId::MiningFatigue);
        const bool hasConduitPower = player.getEffect(MobEffectId::ConduitPower) != nullptr;

        int32_t hasteLevel = haste == nullptr ? 0 : haste->level();
        if (hasConduitPower)
            hasteLevel = std::max(hasteLevel, 2);

        if (hasteLevel > 0)
            speedMultiplier *= 1.0 + 0.2 * (double) hasteLevel;
        if (fatigue)
            speedMultiplier *= std::pow(0.3, (double) fatigue->level());

        seconds /= (double) speedMultiplier;

        // the client's rate this tick: no grace after landing
        if (player.isRiding() || (!player.isOnGround() && !player.isFlying()))
            seconds *= 5.0;

        const bool inWater = player.getFlags().get(ActorFlag::Swimming);
        const bool hasAquaAffinity = ItemEnchantments::getLevel(
                                             player.getInventory().getArmor(PlayerInventory::ARMOR_HEAD),
                                             EnchantmentIds::AQUA_AFFINITY)
                                     > 0;
        if (inWater && !hasAquaAffinity)
            seconds *= hasConduitPower && blockHardness >= 0.5 ? 2.5 : 5.0;

        return seconds;
    }

    double calculateBreakProgressPerTick(ServerPlayer &player, const BlockData *blockData) {
        const double seconds = calculateBreakSeconds(player, blockData);
        if (seconds < 0.0)
            return 0.0;
        if (seconds <= 0.0)
            return 1.0;

        return 1.0 / (seconds * 20.0);
    }

    int64_t expectedBreakTicks(ServerPlayer &player, const BlockData *blockData) {
        const double seconds = calculateBreakSeconds(player, blockData);
        if (seconds < 0.0)
            return std::numeric_limits<int64_t>::max();

        const double ticks = std::ceil(seconds * 20.0) - 1.0;
        return std::max<int64_t>(0, (int64_t) ticks);
    }

    bool hasReachedBreakTime(ServerNetworkHandler &owner, ServerPlayer &player, const BlockData *blockData) {
        if (!player.isBreakingBlock())
            return false;

        const int64_t elapsedTicks = owner.getCurrentTick() - player.getBreakStartTick();
        return elapsedTicks + BREAK_TICK_TOLERANCE >= expectedBreakTicks(player, blockData);
    }

    bool finiteClickPosition(const Vector3f &clickPosition) {
        return std::isfinite(clickPosition.x) && std::isfinite(clickPosition.y) && std::isfinite(clickPosition.z);
    }

    uint64_t steadyTimeMicros() {
        using namespace std::chrono;
        return (uint64_t) duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }

    using VectorMath::distanceSquared;

    bool isDuplicateRightClick(ServerPlayer &player, const ItemUseTransaction &transaction) {
        const uint64_t now = steadyTimeMicros();
        const bool duplicate = player.hasLastRightClick()
                               && now - player.getLastRightClickTimeMicros() < RIGHT_CLICK_DEDUP_MICROSECONDS
                               && player.getLastRightClickFace() == transaction.mBlockFace
                               && player.getLastRightClickBlockPosition() == transaction.mBlockPosition
                               && distanceSquared(player.getLastRightClickPlayerPosition(), transaction.mPlayerPosition)
                                          < 0.00001f
                               && distanceSquared(player.getLastRightClickPosition(), transaction.mClickPosition)
                                          < 0.00001f;

        player.recordRightClick(transaction.mBlockPosition, transaction.mBlockFace, transaction.mPlayerPosition,
                                transaction.mClickPosition, now);
        return duplicate;
    }

    std::string furnaceDropIdentifier(const std::string &identifier) {
        if (identifier == "minecraft:lit_furnace")
            return "minecraft:furnace";
        if (identifier == "minecraft:lit_blast_furnace")
            return "minecraft:blast_furnace";
        if (identifier == "minecraft:lit_smoker")
            return "minecraft:smoker";

        const std::string &aliased = PlacedBlockAliases::resolveItem(identifier);
        if (!aliased.empty())
            return aliased;

        return identifier;
    }

    // the undyed shulker box is its own item: the placement alias read backwards gives "minecraft:shulker_box",
    // the white one; wall signs and wall banners drop the item their standing form places
    std::string selfDropIdentifier(const std::string &identifier) {
        if (identifier == BlockNames::UNDYED_SHULKER_BOX)
            return identifier;
        if (identifier == BlockNames::WALL_BANNER || identifier == "minecraft:wall_sign"
            || BlockIdentifier::endsWith(identifier, "_wall_sign"))
            return BlockPickItem::identifierFor(identifier);

        return furnaceDropIdentifier(identifier);
    }

    bool isBreakableInCreative(const ServerPlayer &player, const std::string &identifier) {
        // a pack item can refuse to break blocks in creative, like a sword
        const ItemStack &held = player.getInventory().getItemInHand();
        if (!held.isAir() && held.mDefinition != nullptr) {
            const CustomItemDefinition *custom =
                    CustomContentRegistry::getInstance().findItem(held.mDefinition->getIdentifier());
            if (custom != nullptr && !custom->mCanDestroyInCreative)
                return false;
        }

        if (BlockIdentifier::equalsAny(identifier,
                                       {"minecraft:end_gateway", "minecraft:fire", "minecraft:soul_fire",
                                        "minecraft:invisible_bedrock", "minecraft:jigsaw", "minecraft:moving_block",
                                        "minecraft:bubble_column", "minecraft:water", "minecraft:flowing_water",
                                        "minecraft:lava", "minecraft:flowing_lava"}))
            return false;

        if (BlockIdentifier::equalsAny(identifier, {"minecraft:allow", "minecraft:deny", "minecraft:border_block"}))
            return player.isOp();

        return true;
    }

    void sendCurrentBlockState(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
        Level &level = owner.getLevelFor(player);
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return;

        // peek only: no synchronous chunk load or generation on the tick thread; unloaded: nothing to correct
        const BlockState *statePtr = level.peekBlockPtr(position.x, position.y, position.z);
        if (statePtr == nullptr)
            return;
        const BlockState state = *statePtr;

        UpdateBlockPacket update;
        update.mBlockPosition = position;
        update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
        update.mFlags = UpdateBlockPacket::Flag::All;
        update.mDataLayer = 0;
        BlockActionHandler::broadcastToViewers(
                owner, level, Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f),
                update);
    }

    void syncBlocksNearby(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &clicked, int face) {
        static const int sides[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};
        constexpr float maxDistanceSquared = 10000.0f;

        const Vector3f &feet = player.getPosition();
        const float dx = (float) clicked.x - feet.x;
        const float dy = (float) clicked.y - feet.y;
        const float dz = (float) clicked.z - feet.z;
        if (dx * dx + dy * dy + dz * dz >= maxDistanceSquared)
            return;

        std::vector<Vector3i> positions;
        positions.reserve(14);

        auto addUnique = [&positions](const Vector3i &position) {
            if (std::find(positions.begin(), positions.end(), position) == positions.end())
                positions.push_back(position);
        };

        auto addWithSides = [&](const Vector3i &center) {
            addUnique(center);
            for (const auto &side: sides)
                addUnique(Vector3i(center.x + side[0], center.y + side[1], center.z + side[2]));
        };

        addWithSides(clicked);
        if (face >= 0 && face < 6)
            addWithSides(Vector3i(clicked.x + sides[face][0], clicked.y + sides[face][1], clicked.z + sides[face][2]));

        Level &level = owner.getLevelFor(player);
        for (const Vector3i &position: positions) {
            if (position.y < level.getMinY() || position.y > level.getMaxY())
                continue;

            for (int32_t layer = 0; layer < 2; ++layer) {
                // peek only: no chunk generation from client coordinates when syncing around a click
                const BlockState *statePtr = level.peekBlockPtr(position.x, position.y, position.z, layer);
                if (statePtr == nullptr)
                    continue;
                const BlockState &state = *statePtr;

                UpdateBlockPacket update;
                update.mBlockPosition = position;
                update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
                update.mFlags = UpdateBlockPacket::Flag::All;
                update.mDataLayer = layer;
                owner.getNetworkHandler().send(player.getNetworkIdentifier(), update, owner.getCodecContext());
            }
        }
        player.awaitMovementChange();
    }

    bool isInSpawnProtection(ServerNetworkHandler &owner, ServerPlayer &player, Level &level,
                             const Vector3i &position) {
        const int radius = owner.getProperties().getSpawnProtection();
        if (radius < 0 || player.isOp())
            return false;

        const Vector3i spawn = level.getSpawnPosition();
        const float dx = (float) position.x - (float) spawn.x;
        const float dz = (float) position.z - (float) spawn.z;
        return dx * dx + dz * dz <= (float) (radius * radius);
    }

    bool isChunkReady(Level &level, const Vector3i &position) {
        const int32_t chunkX = position.x >> 4;
        const int32_t chunkZ = position.z >> 4;
        return level.isChunkResident(chunkX, chunkZ) && level.isChunkPopulated(chunkX, chunkZ);
    }

    void broadcastArmSwing(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3f &position) {
        ActorEventPacket swing;
        swing.mRuntimeActorId = player.getRuntimeId();
        swing.mEventId = (uint8_t) EntityEventType::ArmSwing;
        swing.mEventData = 0;
        swing.mHasFirePosition = false;
        BlockActionHandler::broadcastToViewers(owner, owner.getLevelFor(player), position, swing, &player);
    }
}

void BlockActionHandler::broadcastToViewers(ServerNetworkHandler &owner, Level &level, const Vector3f &position,
                                            const Packet &packet, const ServerPlayer *except) {
    const int32_t chunkX = (int32_t) std::floor(position.x) >> 4;
    const int32_t chunkZ = (int32_t) std::floor(position.z) >> 4;
    const int64_t key = ((int64_t) chunkX << 32) | (uint32_t) chunkZ;

    for (auto &entry: owner.getPlayers()) {
        if (&entry.second == except)
            continue;

        if (&owner.getLevelFor(entry.second) != &level || entry.second.getSentChunks().count(key) == 0)
            continue;

        owner.getNetworkHandler().send(entry.first, packet, owner.getCodecContext());
        if (packet.getId() == MinecraftPacketIds::UpdateBlock && isNearPlayer(entry.second, position))
            entry.second.awaitMovementChange();
    }
}

void BlockActionHandler::broadcastBlockUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                              const BlockState &state, uint32_t layer) {
    UpdateBlockPacket update;
    update.mBlockPosition = position;
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = layer;
    broadcastToViewers(owner, level,
                       Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f),
                       update);
}

void BlockActionHandler::broadcastBlockActorData(ServerNetworkHandler &owner, Level &level,
                                                 const BlockActor &blockActor) {
    const Vector3i &position = blockActor.getPosition();
    BlockActorDataPacket data;
    data.mBlockPosition = position;
    data.mData = blockActor.getSpawnCompound();
    broadcastToViewers(owner, level,
                       Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f), data);
}

// per-tick progress as a fraction of 65535: the client animates the crack at the server's pace
int32_t BlockActionHandler::breakSpeedEventData(double speed) {
    return (int32_t) std::clamp(65535.0 * speed, 0.0, 65535.0);
}

bool BlockActionHandler::canInteractWithBlock(ServerPlayer &player, const Vector3i &position) {
    return _canInteractWithBlock(player, position, 1.0f);
}

bool BlockActionHandler::_canInteractWithBlock(ServerPlayer &player, const Vector3i &position, float rangeScalar) {
    const float maxDistance = player.isCreative() ? 13.0f : 7.0f;

    const Vector3f feet = player.getPosition();
    const Vector3f eyePosition(feet.x, feet.y + PLAYER_BASE_OFFSET, feet.z);
    const Vector3f target((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);

    const float dx = target.x - eyePosition.x;
    const float dy = target.y - eyePosition.y;
    const float dz = target.z - eyePosition.z;

    const double distanceSquared = (double) dx * dx + (double) dy * dy + (double) dz * dz;
    const double maximumSquared = (double) maxDistance * maxDistance * (double) rangeScalar * rangeScalar;
    if (!std::isfinite(distanceSquared) || !std::isfinite(rangeScalar) || rangeScalar < 1.0f
        || distanceSquared > maximumSquared)
        return false;

    const float yawRad = player.getRotation().y * MathConstants::PI_F / 180.0f;
    const float halfPi = 1.57079633f;
    float directionX = -std::cos(yawRad - halfPi);
    float directionZ = -std::sin(yawRad - halfPi);

    const float length = std::sqrt(directionX * directionX + directionZ * directionZ);
    if (length > 0.0f) {
        directionX /= length;
        directionZ /= length;
    }

    // beyond reach, also reject blocks more than REACH_MAX_DIFF behind the eye plane facing the yaw
    const float dot = directionX * eyePosition.x + directionZ * eyePosition.z;
    const float dot1 = directionX * target.x + directionZ * target.z;
    return (dot1 - dot) >= -REACH_MAX_DIFF;
}

bool BlockActionHandler::canBreakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
    // reach first: nothing is read at a position the client only named
    const PropertiesSettings &properties = owner.getProperties();
    const float scalar = properties.getServerAuthoritativeBlockBreaking()
                                 ? properties.getServerAuthoritativeBlockBreakingRangeScalar()
                                 : 1.0f;
    if (!_canInteractWithBlock(player, position, scalar))
        return false;

    Level &level = owner.getLevelFor(player);
    // a column not in memory is unknown: the callers read it next, and a read would generate it
    if (!level.isChunkResident(position.x >> 4, position.z >> 4))
        return false;

    if (player.isAdventure()) {
        const BlockState *block = level.peekBlockPtr(position.x, position.y, position.z);
        if (block == nullptr || !ItemProperties::canDestroy(player.getInventory().getItemInHand(), block->mName))
            return false;
    }

    // a deny block forbids breaking as much as placing: else breaking bypasses it
    return isBlockChangeAllowed(level, position, player);
}

void BlockActionHandler::breakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
    Level &level = owner.getLevelFor(player);

    const BlockState brokenState =
            level.getChunk(position.x >> 4, position.z >> 4).getBlock(position.x & 15, position.y, position.z & 15);

    if (brokenState.mName == BlockNames::AIR) {
        return;
    }

    PlayerBreakBlockBeforeEvent breakEvent(player, position);
    owner.getEventBus().before().mPlayerBreakBlock.emit(breakEvent);
    if (breakEvent.isCancelled()) {
        sendCurrentBlockState(owner, player, position);
        return;
    }

    PluginEvent pluginEvent;
    pluginEvent.mType = FALCON_EVENT_BLOCK_BREAK;
    pluginEvent.mCancellable = true;
    pluginEvent.mPlayer = &player;
    pluginEvent.mBlockPosition = position;
    pluginEvent.mBlockName = brokenState.mName;
    PluginManager::getInstance().dispatch(pluginEvent);
    if (pluginEvent.mCancelled) {
        sendCurrentBlockState(owner, player, position);
        return;
    }

    const std::string brokenIdentifier = brokenState.mName;
    const BlockData *brokenData = BlockDataTable::find(brokenState.mName.c_str());
    const bool creative = player.isCreative();
    const ItemStack heldItem = player.getInventory().getItemInHand();

    // a block the held item cannot harvest drops nothing for a player; explosions and pistons still drop it
    const bool harvested = brokenData == nullptr || canHarvest(*brokenData, heldItem);
    destroyBlock(owner, level, position, brokenState, !creative && harvested, heldItem, &player);
    BlockSensor::onBlockBroken(owner, level, position, brokenState, player);
    PluginBlock::brokenBy(player, position, brokenIdentifier);

    if (brokenData != nullptr && brokenData->mHardness > 0.0f)
        owner.damagePlayerHeldItem(player, 1);

    // the destroyed-by-player event goes out once the tool took its wear
    PlayerBreakBlockAfterEvent brokenEvent(player, position, brokenState);
    brokenEvent.mItemBeforeBreak = heldItem;
    brokenEvent.mItemAfterBreak = player.getInventory().getItemInHand();
    owner.getEventBus().after().mPlayerBreakBlock.emit(brokenEvent);

    const bool silkTouch = ItemEnchantments::getLevel(heldItem, EnchantmentIds::SILK_TOUCH) > 0;
    if (!creative && !silkTouch && harvested) {
        const int experience = ExperienceValues::getOreDropExperience(brokenState.mName);
        if (experience > 0) {
            const Vector3f orbPosition((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
            owner.spawnExperienceOrbs(level, orbPosition, experience);
        }
    }

    player.exhaust(0.005f);
}

void BlockActionHandler::spawnBlockDrops(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &brokenState, const ItemStack &tool) {
    const Vector3f dropPosition((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    for (const ItemStack &drop: blockDrops(owner, &level, position, brokenState, tool))
        owner.dropItem(level, dropPosition, drop, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
}

std::vector<ItemStack> BlockActionHandler::blockDrops(ServerNetworkHandler &owner, Level *level,
                                                      const Vector3i &position, const BlockState &brokenState,
                                                      const ItemStack &tool) {
    std::vector<ItemStack> drops;
    const BlockData *brokenData = BlockDataTable::find(brokenState.mName.c_str());
    const Block *brokenBlock = VanillaBlocks::fromIdentifier(brokenState.mName);
    if (brokenData == nullptr && brokenBlock == nullptr)
        return drops;
    const bool silkTouch = ItemEnchantments::getLevel(tool, EnchantmentIds::SILK_TOUCH) > 0;
    const int32_t fortuneLevel = ItemEnchantments::getLevel(tool, EnchantmentIds::FORTUNE);

    bool ownItem = false;
    auto addDrop = [&](const std::string &identifier, int32_t count) {
        if (identifier.empty() || count <= 0)
            return;

        Item parsedItem;
        if (!StringToItemParser::getInstance().parse(identifier, parsedItem))
            return;

        ItemStack drop;
        drop.mDefinition = owner.getItemDefinitions().getDefinition(parsedItem.getIdentifier());
        drop.mBlockDefinition = owner.getBlockDefinitions().getDefinition(parsedItem.getIdentifier());
        drop.mCount = count;

        if (brokenBlock != nullptr && level != nullptr)
            brokenBlock->writeDropContents(*level, position, drop);
        // the block's own item carries what its block actor holds (bed color, banner patterns)
        if (level != nullptr && ownItem) {
            if (const BlockActor *actor = level->getBlockActors().find(position))
                BlockPickItem::applyActorItemData(drop, *actor);
        }
        drops.push_back(std::move(drop));
    };

    std::vector<BlockDrop> classDrops;
    if (brokenBlock != nullptr && brokenBlock->getDrops(brokenState, tool, fortuneLevel, classDrops)) {
        for (const BlockDrop &drop: classDrops)
            addDrop(furnaceDropIdentifier(drop.mIdentifier), drop.mCount);
        return drops;
    }

    if (brokenData == nullptr)
        return drops;

    std::string dropIdentifier;
    int32_t dropCount = 0;
    const std::string normalizedDropIdentifier = selfDropIdentifier(
            brokenBlock != nullptr ? brokenBlock->getResourceItem(brokenState) : brokenState.mName);
    const int32_t resourceCount = brokenBlock != nullptr ? brokenBlock->getResourceCount(brokenState) : 1;

    if (silkTouch && brokenData->mSilkTouch) {
        dropIdentifier = normalizedDropIdentifier;
        dropCount = resourceCount;
    } else if (brokenData->mDropKind == BlockDropKind::Self) {
        dropIdentifier = normalizedDropIdentifier;
        dropCount = resourceCount;
    } else if (brokenData->mDropKind == BlockDropKind::Other && brokenData->mDropIdentifier != nullptr) {
        dropIdentifier = brokenData->mDropIdentifier;
        const int32_t range = (int32_t) brokenData->mDropMax - (int32_t) brokenData->mDropMin + 1;
        dropCount = (int32_t) brokenData->mDropMin + (range > 0 ? rand() % range : 0);

        if (fortuneLevel > 0)
            dropCount += rand() % (fortuneLevel + 1);
    }

    ownItem = dropIdentifier == normalizedDropIdentifier;
    addDrop(dropIdentifier, dropCount);
    return drops;
}

bool BlockActionHandler::canHarvest(const BlockData &block, const ItemStack &tool) {
    if (block.mToolType == BlockToolType::None || block.mHandHarvest)
        return true;
    if (tool.isAir() || tool.mDefinition == nullptr)
        return false;

    const std::string &identifier = tool.mDefinition->getIdentifier();
    const ItemData *itemData = ItemDataTable::find(identifier);
    if (itemData == nullptr) {
        const bool standardTool = block.mToolType == BlockToolType::Pickaxe || block.mToolType == BlockToolType::Shovel
                                  || block.mToolType == BlockToolType::Axe || block.mToolType == BlockToolType::Hoe;
        bool useEfficiency = false;
        return CustomContentRegistry::getInstance().getDiggerSpeed(identifier, block.mName, standardTool, useEfficiency)
               > 0;
    }
    if ((int32_t) itemData->mToolType == (int32_t) block.mToolType && itemData->mToolTier >= (int32_t) block.mToolTier)
        return true;
    const Item *type = VanillaItems::fromIdentifier(identifier);
    return type != nullptr && type->canDestroySpecial(block.mIdentifier);
}

void BlockActionHandler::destroyBlock(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &brokenState, bool dropItems, const ItemStack &tool,
                                      const Actor *entitySource, const Vector3i *blockSource) {
    const int32_t brokenHash = BlockStateHasher::hash(brokenState.mName, brokenState.mStates);
    const Block *brokenBlock = VanillaBlocks::fromIdentifier(brokenState.mName);

    // the drops and onBroken still read the actor: it leaves with the block once they ran
    std::unique_ptr<BlockActor> heldActor = level.getBlockActors().take(position);
    level.setBlockState(position.x, position.y, position.z, BlockState(BlockNames::AIR));
    if (heldActor != nullptr)
        level.getBlockActors().insert(std::move(heldActor));

    if (brokenBlock != nullptr) {
        for (const Vector3i &affected: brokenBlock->getAffectedBlocks(level, position, brokenState)) {
            const BlockState affectedState = level.getBlockState(affected.x, affected.y, affected.z);
            if (affectedState.mName == BlockNames::AIR)
                continue;

            destroyBlock(owner, level, affected, affectedState, false, ItemStack::air());
        }
    }

    const BlockState replacement = level.getBlockState(position.x, position.y, position.z);
    const int32_t replacementHash = BlockStateHasher::hash(replacement.mName, replacement.mStates);

    if (dropItems && level.getGameRules().getBool("dotiledrops"))
        spawnBlockDrops(owner, level, position, brokenState, tool);

    UpdateBlockPacket update;
    update.mBlockPosition = position;
    update.mRuntimeId = (uint32_t) replacementHash;
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;

    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    broadcastToViewers(owner, level, center, update);
    owner.broadcastLevelEvent(level, LevelEventPacket::Event::ParticleDestroy, center, brokenHash);

    if (brokenBlock != nullptr)
        brokenBlock->onBroken(owner, level, position, brokenState);

    level.onBlockBroken(position, brokenState);
    if (brokenBlock == nullptr)
        owner.getScriptEngine().onCustomBlockBreak(level, position, brokenState, entitySource, blockSource);
    level.removeForeignBlockActor(position, level.getBlockState(position.x, position.y, position.z));
}

void BlockActionHandler::startBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                                            int32_t face) {
    Level &level = owner.getLevelFor(player);
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return;

    if (!canBreakBlock(owner, player, position)) {
        if (player.isBreakingBlock())
            stopBreakingBlock(owner, player);
        sendCurrentBlockState(owner, player, position);
        return;
    }

    if (player.isBreakingBlock() && player.getBreakingBlockPosition() == position)
        return;

    if (player.isBreakingBlock())
        stopBreakingBlock(owner, player);

    if (isInSpawnProtection(owner, player, level, position)) {
        sendCurrentBlockState(owner, player, position);
        return;
    }

    const BlockState state =
            level.getChunk(position.x >> 4, position.z >> 4).getBlock(position.x & 15, position.y, position.z & 15);

    if (state.mName == BlockNames::AIR) {
        return;
    }

    owner.getScriptEngine().onEntityHitBlock(player, position, face);

    const Block *punchedBlock = VanillaBlocks::fromIdentifier(state.mName);
    if (punchedBlock != nullptr && punchedBlock->onPunch(owner, player, position, state))
        return;

    const BlockData *blockData = BlockDataTable::find(state.mName.c_str());
    const bool creative = player.isCreative();
    if (creative && !isBreakableInCreative(player, state.mName))
        return;

    const double breakSpeed = creative ? 1.0 : calculateBreakProgressPerTick(player, blockData);

    if (breakSpeed <= 0.0) {
        return;
    }

    player.startBreakingBlock(position, face, breakSpeed, owner.getCurrentTick());
    owner.getScriptEngine().onPlayerStartBreakingBlock(player, position, face);

    owner.broadcastLevelEvent(level, LevelEventPacket::Event::BlockStartBreak,
                              Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f),
                              breakSpeedEventData(breakSpeed));

    if (creative)
        completeBreakingBlock(owner, player, position);
}

void BlockActionHandler::continueBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isBreakingBlock())
        return;

    if (owner.getCurrentTick() <= player.getBreakStartTick())
        return;

    const Vector3i position = player.getBreakingBlockPosition();
    if (!canBreakBlock(owner, player, position)) {
        stopBreakingBlock(owner, player);
        return;
    }
    Level &level = owner.getLevelFor(player);
    if (position.y < level.getMinY() || position.y > level.getMaxY()) {
        stopBreakingBlock(owner, player);
        return;
    }

    const BlockState &state =
            level.getChunk(position.x >> 4, position.z >> 4).getBlock(position.x & 15, position.y, position.z & 15);

    if (state.mName == BlockNames::AIR) {
        stopBreakingBlock(owner, player);
        return;
    }

    const BlockData *blockData = BlockDataTable::find(state.mName.c_str());
    const double newBreakSpeed = calculateBreakProgressPerTick(player, blockData);

    if (newBreakSpeed <= 0.0) {
        stopBreakingBlock(owner, player);
        return;
    }

    if (std::fabs(newBreakSpeed - player.getBreakSpeed()) > BREAK_SPEED_CHANGE_EPSILON) {
        player.setBreakSpeed(newBreakSpeed);

        owner.broadcastLevelEvent(
                level, LevelEventPacket::Event::BlockUpdateBreak,
                Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f),
                breakSpeedEventData(newBreakSpeed));
    }

    player.addBreakProgress(player.getBreakSpeed());

    if (owner.getProperties().getServerAuthoritativeBlockBreaking() && player.getBreakProgress() >= 1.0) {
        completeBreakingBlock(owner, player, position);
    }
}

void BlockActionHandler::resyncDroppedInput(ServerNetworkHandler &owner, ServerPlayer &player,
                                            const PlayerAuthInputPacket &packet) {
    for (const PlayerBlockActionData &action: packet.mPlayerActions) {
        if (action.mAction == PlayerActionType::BlockPredictDestroy
            || action.mAction == PlayerActionType::DimensionChangeRequestOrCreativeDestroyBlock)
            syncBlocksNearby(owner, player, action.mBlockPosition, action.mFace);
    }
    if (packet.mHasItemUseTransaction
        && (packet.mItemUseTransaction.mActionType == 0 || packet.mItemUseTransaction.mActionType == 2))
        syncBlocksNearby(owner, player, packet.mItemUseTransaction.mBlockPosition,
                         packet.mItemUseTransaction.mBlockFace);
}

namespace {
    BlockActionHandler::BreakRejectionHook breakRejectionHook = nullptr;

    void reportBreakRejection(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const char *reason, const BlockState *state, const BlockData *blockData) {
        if (breakRejectionHook == nullptr)
            return;
        BlockActionHandler::BreakRejection rejection;
        rejection.mReason = reason;
        rejection.mBlock = state != nullptr ? state->mName : std::string();
        rejection.mBreaking = player.isBreakingBlock();
        rejection.mBreakingPosition = player.getBreakingBlockPosition();
        rejection.mElapsedTicks = player.isBreakingBlock() ? owner.getCurrentTick() - player.getBreakStartTick() : 0;
        rejection.mExpectedTicks = blockData != nullptr ? expectedBreakTicks(player, blockData) : 0;
        rejection.mProgress = player.getBreakProgress();
        rejection.mSpeed = player.getBreakSpeed();
        breakRejectionHook(player, position, rejection);
    }
}

void BlockActionHandler::setBreakRejectionHook(BreakRejectionHook hook) {
    breakRejectionHook = hook;
}

void BlockActionHandler::completeBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                               const Vector3i &position) {
    Level &playerLevel = owner.getLevelFor(player);
    if (position.y < playerLevel.getMinY() || position.y > playerLevel.getMaxY())
        return;

    if (!canBreakBlock(owner, player, position) || isInSpawnProtection(owner, player, playerLevel, position)) {
        reportBreakRejection(owner, player, position, "not allowed", nullptr, nullptr);
        sendCurrentBlockState(owner, player, position);
        return;
    }

    const bool creative = player.isCreative();
    if (creative && !player.isBreakingBlock()) {
        startBreakingBlock(owner, player, position, 0);
        return;
    }

    if (!creative) {
        Level &level = owner.getLevelFor(player);
        const BlockState &state =
                level.getChunk(position.x >> 4, position.z >> 4).getBlock(position.x & 15, position.y, position.z & 15);
        if (state.mName == BlockNames::AIR) {
            stopBreakingBlock(owner, player);
            return;
        }

        if (!player.isBreakingBlock() || player.getBreakingBlockPosition() != position) {
            reportBreakRejection(owner, player, position,
                                 player.isBreakingBlock() ? "breaking another block" : "no break started", &state,
                                 BlockDataTable::find(state.mName.c_str()));
            sendCurrentBlockState(owner, player, position);
            return;
        }

        const BlockData *blockData = BlockDataTable::find(state.mName.c_str());
        if (!hasReachedBreakTime(owner, player, blockData)
            || player.getBreakProgress() + player.getBreakSpeed() * (double) BREAK_TICK_TOLERANCE
                               + BREAK_PROGRESS_EPSILON
                       < 1.0) {
            reportBreakRejection(owner, player, position,
                                 !hasReachedBreakTime(owner, player, blockData) ? "too early" : "progress short",
                                 &state, blockData);
            sendCurrentBlockState(owner, player, position);
            return;
        }
    }

    if (player.isBreakingBlock())
        stopBreakingBlock(owner, player, false);

    breakBlock(owner, player, position);
}

void BlockActionHandler::sendBreakingFx(ServerNetworkHandler &owner, ServerPlayer &player) {
    const Vector3i position = player.getBreakingBlockPosition();
    Level &level = owner.getLevelFor(player);
    const BlockState &state =
            level.getChunk(position.x >> 4, position.z >> 4).getBlock(position.x & 15, position.y, position.z & 15);

    if (state.mName == BlockNames::AIR)
        return;

    const int32_t blockHash = BlockStateHasher::hash(state.mName, state.mStates);
    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);

    LevelEventPacket punch;
    punch.mEventId = LevelEventPacket::Event::ParticlePunchBlock;
    punch.mPosition = center;
    punch.mData = blockHash;
    punch.mBlockFace = player.getBreakingFace();
    broadcastToViewers(owner, level, center, punch);

    owner.playBlockSound(level, LevelSoundEvent::HIT, center, blockHash);

    ActorEventPacket swing;
    swing.mRuntimeActorId = player.getRuntimeId();
    swing.mEventId = (uint8_t) EntityEventType::ArmSwing;
    swing.mEventData = 0;
    swing.mHasFirePosition = false;
    broadcastToViewers(owner, level, center, swing, &player);
}

void BlockActionHandler::stopBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player, bool cancelled) {
    if (!player.isBreakingBlock())
        return;

    const Vector3i position = player.getBreakingBlockPosition();
    if (cancelled)
        owner.getScriptEngine().onPlayerCancelBreakingBlock(player, position, player.getBreakingFace(),
                                                            player.getBreakProgress());
    player.stopBreakingBlock();

    owner.broadcastLevelEvent(owner.getLevelFor(player), LevelEventPacket::Event::BlockStopBreak,
                              Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f),
                              0);
}

bool BlockActionHandler::matchesTransactionItem(const PacketCodecContext &context, const ItemStack &held,
                                                const ItemStack &sent) {
    if (held.isAir() || sent.isAir())
        return held.isAir() && sent.isAir();

    return context.toNetworkItemId(held.mDefinition->getRuntimeId())
                   == context.toNetworkItemId(sent.mDefinition->getRuntimeId())
           && held.mDamage == sent.mDamage;
}

// the nearest allow/deny below in the same column decides; deny still lets creative build
bool BlockActionHandler::isBlockChangeAllowed(Level &level, const Vector3i &position, const ServerPlayer &player) {
    if (!level.isChunkResident(position.x >> 4, position.z >> 4))
        return false;

    for (int32_t y = position.y - 1; y >= level.getMinY(); --y) {
        const BlockState *state = level.peekBlockPtr(position.x, y, position.z);
        if (state == nullptr)
            continue;

        if (state->mName == "minecraft:allow")
            return true;

        if (state->mName == "minecraft:deny")
            return player.isCreative();
    }

    return true;
}

bool BlockActionHandler::isBlockedByActor(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          const BlockState &state, const ServerPlayer &placer) {
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr || !data->mSolid)
        return false;

    const AxisAlignedBB shape = BlockShape::getShapeAt(state, position.x, position.y, position.z);

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &other = entry.second;

        if (&other == &placer || !other.isSpawned() || other.isDead())
            continue;

        if (other.isSpectator())
            continue;

        if (&owner.getLevelFor(other) != &level)
            continue;

        const ActorSize size = ActorClassRegistry::getSize("minecraft:player");
        if (shape.intersectsWith(actorBox(other.getPosition(), size)))
            return true;
    }

    for (auto &entry: owner.getActors()) {
        ServerActor &actor = *entry.second;

        if (actor.isProjectile() || actor.isDead() || isPlacementIgnoredActor(actor.getIdentifier()))
            continue;

        if (&owner.getLevelFor(actor) != &level)
            continue;

        const ActorSize size = ActorClassRegistry::getSize(actor.getIdentifier());
        if (shape.intersectsWith(actorBox(actor.getPosition(), size)))
            return true;
    }

    return false;
}

bool BlockActionHandler::placeBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                    const ItemUseTransaction &transaction) {
    if (transaction.mBlockFace < 0 || transaction.mBlockFace >= 6)
        return false;

    // a simulated player's clicks come from the server: no resent packet to drop
    if (!player.isSimulated() && isDuplicateRightClick(player, transaction))
        return false;

    if (!player.isSpawned() || player.isDead() || player.isSpectator())
        return false;

    if (transaction.mHotbarSlot < 0 || transaction.mHotbarSlot >= PlayerInventory::HOTBAR_SIZE)
        return false;

    PlayerInventory &inventory = player.getInventory();
    bool selectedSlotChanged = false;
    if (inventory.getSelectedSlot() != transaction.mHotbarSlot) {
        player.getInventoryManager().onClientSelectHotbarSlot(transaction.mHotbarSlot);
        inventory.setSelectedSlot(transaction.mHotbarSlot);
        selectedSlotChanged = true;
    }

    bool handled = false;

    // the game checks no reach for a simulated player
    if (player.isSimulated() || canInteractWithBlock(player, transaction.mBlockPosition)) {
        if (player.isCreative())
            handled = interactBlock(owner, player, transaction, selectedSlotChanged);
        else if (matchesTransactionItem(owner.getCodecContext(player.getNetworkIdentifier()), inventory.getItemInHand(),
                                        transaction.mItemInHand))
            handled = interactBlock(owner, player, transaction, selectedSlotChanged);
    }

    if (handled)
        return true;

    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());

    if (selectedSlotChanged)
        InventoryHandler::sendHeldItem(owner, player);

    const Vector3f playerPosition = player.getPosition();
    const float deltaX = (float) transaction.mBlockPosition.x - playerPosition.x;
    const float deltaY = (float) transaction.mBlockPosition.y - playerPosition.y;
    const float deltaZ = (float) transaction.mBlockPosition.z - playerPosition.z;

    if (deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ > MAX_SYNC_DISTANCE_SQUARED)
        return false;

    syncBlocksNearby(owner, player, transaction.mBlockPosition, transaction.mBlockFace);
    return false;
}

bool BlockActionHandler::interactBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const ItemUseTransaction &transaction, bool selectedSlotChanged) {
    if (!finiteClickPosition(transaction.mClickPosition))
        return false;

    PlayerInventory &inventory = player.getInventory();
    Level &level = owner.getLevelFor(player);

    // the clicked column is read below: one not in memory would be generated on the tick thread
    if (!player.isSimulated()
        && !level.isChunkResident(transaction.mBlockPosition.x >> 4, transaction.mBlockPosition.z >> 4))
        return false;

    const BlockState clickedState = level.getBlockState(transaction.mBlockPosition.x, transaction.mBlockPosition.y,
                                                        transaction.mBlockPosition.z);

    static const int offsets[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    const int face = transaction.mBlockFace;
    const bool replacesClicked = BlockSupport::isReplaceable(clickedState);
    const int placementFace = replacesClicked ? PlacementOrientation::FACE_UP : face;

    Vector3i target = transaction.mBlockPosition;
    if (!replacesClicked) {
        target = Vector3i(transaction.mBlockPosition.x + offsets[face][0],
                          transaction.mBlockPosition.y + offsets[face][1],
                          transaction.mBlockPosition.z + offsets[face][2]);
    }

    const ItemStack &handItem = inventory.getItemInHand();
    const Block *heldBlock = handItem.mBlockDefinition != nullptr
                                     ? VanillaBlocks::fromIdentifier(handItem.mBlockDefinition->getIdentifier())
                                     : nullptr;

    if (heldBlock != nullptr)
        target = heldBlock->resolvePlacementPosition(level, target, face);

    if (target.y < level.getMinY() || target.y > level.getMaxY())
        return false;

    if (level.getDimensionType() == DimensionType::Nether && target.y > NETHER_PLACEMENT_LIMIT)
        return false;

    if (clickedState.mName == BlockNames::AIR)
        return false;

    if (isInSpawnProtection(owner, player, level, transaction.mBlockPosition))
        return false;

    ScriptEngine &scripts = owner.getScriptEngine();
    if (scripts.beforePlayerInteractWithBlock(player, transaction.mBlockPosition, face))
        return false;

    PluginEvent interactPluginEvent;
    interactPluginEvent.mType = FALCON_EVENT_PLAYER_INTERACT_BLOCK;
    interactPluginEvent.mCancellable = true;
    interactPluginEvent.mPlayer = &player;
    interactPluginEvent.mBlockPosition = transaction.mBlockPosition;
    interactPluginEvent.mBlockFace = (uint32_t) face;
    PluginManager::getInstance().dispatch(interactPluginEvent);
    if (interactPluginEvent.mCancelled)
        return false;

    scripts.onPlayerInteractWithBlock(player, clickedState.mName, transaction.mBlockPosition, face,
                                      transaction.mClickPosition);
    scripts.onItemUseOnBlock(player, transaction.mBlockPosition, face, transaction.mClickPosition);

    const Block *clickedBlock = VanillaBlocks::fromIdentifier(clickedState.mName);
    if (clickedBlock != nullptr)
        clickedBlock->onTouch(owner, player, transaction.mBlockPosition, clickedState);

    const ItemStack &interactItem = inventory.getItemInHand();
    const bool onCooldown = player.hasItemCooldown(interactItem, owner.getCurrentTick());

    const bool useBlock = !player.getFlags().get(ActorFlag::Sneaking) || inventory.getItemInHand().isAir();

    const Item *itemType = !onCooldown && !interactItem.isAir() && interactItem.mDefinition != nullptr
                                   ? VanillaItems::fromIdentifier(interactItem.mDefinition->getIdentifier())
                                   : nullptr;
    const bool itemFirst = itemType != nullptr && itemType->takesPriorityOverBlockInteraction();

    if (itemFirst
        && itemType->onUseOnBlock(owner, player, interactItem, transaction.mBlockPosition, transaction.mBlockFace,
                                  transaction.mClickPosition))
        return true;

    if (!onCooldown && useBlock
        && PluginBlock::interactAt(player, transaction.mBlockPosition, (uint32_t) transaction.mBlockFace,
                                   clickedState.mName))
        return true;

    if (!onCooldown && useBlock && clickedBlock != nullptr
        && clickedBlock->onInteract(owner, player, transaction.mBlockPosition, clickedState))
        return true;

    if (!itemFirst && itemType != nullptr
        && itemType->onUseOnBlock(owner, player, interactItem, transaction.mBlockPosition, transaction.mBlockFace,
                                  transaction.mClickPosition))
        return true;

    if (transaction.mClientInteractPrediction != ItemUsePredictedResult::Success) {
        return true;
    }

    const ItemStack &heldItem = inventory.getItemInHand();

    // a pack item with minecraft:block_placer places that block, on the blocks it lists when it lists any
    std::shared_ptr<BlockDefinition> placerDefinition;
    if (!heldItem.isAir() && heldItem.mDefinition != nullptr && heldItem.mBlockDefinition == nullptr) {
        const CustomItemDefinition *placer =
                CustomContentRegistry::getInstance().findItem(heldItem.mDefinition->getIdentifier());
        if (placer != nullptr && placer->mHasBlockPlacer
            && (placer->mPlacerUseOn.empty()
                || std::find(placer->mPlacerUseOn.begin(), placer->mPlacerUseOn.end(), clickedState.mName)
                           != placer->mPlacerUseOn.end()))
            placerDefinition = owner.getBlockDefinitions().getDefinition(placer->mPlacerBlock);
    }

    const bool bucket = BucketItem::isBucket(heldItem);
    if (heldItem.isAir() || heldItem.mCount <= 0 || heldItem.mDefinition == nullptr
        || (!bucket && heldItem.mBlockDefinition == nullptr && placerDefinition == nullptr)) {
        if (!bucket && !heldItem.isAir() && heldItem.mDefinition != nullptr)
            owner.emitItemUse(player);
        return false;
    }

    if (bucket) {
        BucketItem::use(owner, player, transaction);
        return true;
    }

    if (player.isAdventure()
        && std::find(heldItem.mCanPlace.begin(), heldItem.mCanPlace.end(), clickedState.mName)
                   == heldItem.mCanPlace.end()) {
        return false;
    }

    broadcastArmSwing(owner, player,
                      Vector3f((float) transaction.mBlockPosition.x + 0.5f, (float) transaction.mBlockPosition.y + 0.5f,
                               (float) transaction.mBlockPosition.z + 0.5f));

    if (!isChunkReady(level, target))
        return false;

    if (isInSpawnProtection(owner, player, level, target))
        return false;

    if (!isBlockChangeAllowed(level, target, player) || PistonSystem::isMovingInto(level, target))
        return false;

    const BlockDefinition &definition = heldItem.mBlockDefinition != nullptr ? *heldItem.mBlockDefinition
                                                                             : *placerDefinition;

    BlockState mergedState(definition.getIdentifier(), definition.getState());
    PlacementMergeResult mergeResult = PlacementMergeResult::None;
    if (heldBlock != nullptr) {
        mergeResult = heldBlock->mergePlacement(level, transaction.mBlockPosition, face, transaction.mClickPosition,
                                                target, mergedState);
    }

    if (mergeResult == PlacementMergeResult::Rejected)
        return false;

    const bool merged = mergeResult == PlacementMergeResult::Merged;

    // TODO: snowlogging
    const BlockState targetState = level.getBlockState(target.x, target.y, target.z);
    if (!merged && !BlockSupport::isReplaceable(targetState))
        return false;

    const BlockData *placedData = BlockDataTable::find(definition.getIdentifier().c_str());

    const BlockState targetOverlay = level.getBlockStateAtLayer(target.x, target.y, target.z, 1);
    const bool targetHasLiquid = LiquidView(targetState).isLiquid() || LiquidView(targetOverlay).isLiquid();
    if (!merged && targetHasLiquid && placedData != nullptr && !placedData->mSolid
        && LiquidPhysicsSystem::getWaterloggingLevel(BlockState(definition.getIdentifier(), definition.getState()))
                   == 0)
        return false;

    BlockState placedState = merged ? mergedState
                                    : BlockPlacementComponent::apply(
                                              BlockState(definition.getIdentifier(), definition.getState()), &level,
                                              player.getRotation().y, player.getRotation().x, placementFace,
                                              transaction.mClickPosition, player.getPosition(), target);

    if (!merged && scripts.beforeCustomBlockPlace(player, level, target, face, placedState))
        return false;

    const Block *placedBlock = VanillaBlocks::fromIdentifier(placedState.mName);
    if (!merged && placedBlock != nullptr && !placedBlock->canPlaceAt(level, target, placementFace))
        return false;

    const CustomBlockDefinition *custom = merged ? nullptr
                                                 : CustomContentRegistry::getInstance().findBlock(placedState.mName);
    if (custom != nullptr && !custom->mPlacementFilter.allowsPlacement(level, target, placementFace))
        return false;

    if (isBlockedByActor(owner, level, target, placedState, player))
        return false;

    if (scripts.beforePlayerPlaceBlock(player, level, target, face, transaction.mClickPosition, placedState))
        return false;

    PluginEvent placePluginEvent;
    placePluginEvent.mType = FALCON_EVENT_BLOCK_PLACE;
    placePluginEvent.mCancellable = true;
    placePluginEvent.mPlayer = &player;
    placePluginEvent.mBlockPosition = target;
    placePluginEvent.mBlockName = placedState.mName;
    PluginManager::getInstance().dispatch(placePluginEvent);
    if (placePluginEvent.mCancelled)
        return false;

    std::vector<BlockPlacementEntry> placementBlocks;

    if (!merged && placedBlock != nullptr) {
        placedBlock->onPlacing(owner, level, target, placedState);

        placementBlocks = placedBlock->getPlacementBlocks(
                level, target, placedState, BlockPlacementComponent::getHorizontalFacing(player.getRotation().y));

        for (const BlockPlacementEntry &entry: placementBlocks) {
            if (entry.mPosition.y < level.getMinY() || entry.mPosition.y > level.getMaxY())
                return false;

            const BlockState existing = level.getBlockState(entry.mPosition.x, entry.mPosition.y, entry.mPosition.z);
            if (!BlockSupport::isReplaceable(existing))
                return false;

            if (isBlockedByActor(owner, level, entry.mPosition, entry.mState, player))
                return false;
        }
    }

    const bool waterlogsTarget = LiquidView(targetState).isWater()
                                 && LiquidPhysicsSystem::getWaterloggingLevel(placedState) > 0;

    const int32_t blockHash = BlockStateHasher::hash(placedState.mName, placedState.mStates);
    level.setBlockState(target.x, target.y, target.z, placedState);

    for (const BlockPlacementEntry &entry: placementBlocks) {
        level.setBlockState(entry.mPosition.x, entry.mPosition.y, entry.mPosition.z, entry.mState);
        broadcastBlockUpdate(owner, level, entry.mPosition, entry.mState);
    }

    if (waterlogsTarget)
        level.setBlockStateAtLayer(target.x, target.y, target.z, 1, targetState);

    PlayerPlaceBlockAfterEvent placeEvent(player, target, placedState.mName);
    owner.getEventBus().after().mPlayerPlaceBlock.emit(placeEvent);

    const ItemStack placedWithItem = heldItem;

    if (!player.isCreative()) {
        ItemStack remaining = heldItem;
        remaining.mCount--;
        if (remaining.mCount <= 0)
            remaining = ItemStack::air();
        inventory.setItemInHand(std::move(remaining));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());
    }

    if (selectedSlotChanged || !player.isCreative())
        InventoryHandler::sendHeldItem(owner, player);

    UpdateBlockPacket update;
    update.mBlockPosition = target;
    update.mRuntimeId = (uint32_t) blockHash;
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;

    const Vector3f targetCenter((float) target.x + 0.5f, (float) target.y + 0.5f, (float) target.z + 0.5f);
    broadcastToViewers(owner, level, targetCenter, update);

    if (waterlogsTarget) {
        const BlockState overlay = level.getBlockStateAtLayer(target.x, target.y, target.z, 1);

        UpdateBlockPacket waterUpdate;
        waterUpdate.mBlockPosition = target;
        waterUpdate.mRuntimeId = (uint32_t) BlockStateHasher::hash(overlay.mName, overlay.mStates);
        waterUpdate.mFlags = UpdateBlockPacket::Flag::All;
        waterUpdate.mDataLayer = 1;
        broadcastToViewers(owner, level, targetCenter, waterUpdate);
    }

    owner.playBlockSound(level, LevelSoundEvent::PLACE, targetCenter, (int32_t) blockHash);

    if (placedBlock != nullptr)
        placedBlock->onPlaced(owner, player, target, placedState, placedWithItem, placementFace);

    BlockActor *blockActor = level.getBlockActors().find(target);
    if (blockActor != nullptr) {
        BlockPickItem::restoreBlockData(*blockActor, placedWithItem, owner.getCodecContext());

        BlockActorDataPacket data;
        data.mBlockPosition = target;
        data.mData = blockActor->getSpawnCompound();
        broadcastToViewers(owner, level, targetCenter, data);
    }

    level.onBlockPlaced(target, placedState);
    if (!merged)
        scripts.onCustomBlockPlaced(level, target, placedState, targetState);
    return true;
}
