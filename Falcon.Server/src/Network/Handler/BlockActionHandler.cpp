#include "Network/Handler/BlockActionHandler.h"

#include "Block/BlockData.h"
#include "Block/BlockSupport.h"
#include "Block/Components/BlockPlacementComponent.h"
#include "Block/Components/PlacementOrientation.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/BlockActorStore.h"
#include "Block/BlockIdentifier.h"
#include "Block/BlockPickItem.h"
#include "Core/Math/MathConstants.h"
#include "Actor/ActorClassRegistry.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Actor/ExperienceValues.h"
#include "Block/BlockShape.h"
#include "Item/ItemData.h"
#include "Item/VanillaItems.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Item/ItemEnchantments.h"
#include "Item/Items/BucketItem.h"
#include "Item/StringToItemParser.h"
#include "Inventory/InventoryManager.h"
#include "Level/Level.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ItemActorHandler.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/ActorEventPacket.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Protocol/Packets/BlockActorDataPacket.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/PlacedBlockAliases.h"
#include "Protocol/Types/StartGameTypes.h"

#include <algorithm>
#include <vector>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <string>
#include <utility>

namespace {
    const float PLAYER_BASE_OFFSET = 1.62f;
    const double BREAK_SPEED_CHANGE_EPSILON = 0.0001;
    const double BREAK_PROGRESS_EPSILON = 0.000001;
    const float REACH_MAX_DIFF = 6.0f;
    const uint64_t RIGHT_CLICK_DEDUP_MICROSECONDS = 100000;

    bool isPlacementIgnoredActor(const std::string &identifier) {
        return identifier == "minecraft:xp_orb" || identifier == "minecraft:item"
               || identifier == "minecraft:area_effect_cloud" || identifier == "minecraft:fireworks_rocket"
               || identifier == "minecraft:painting";
    }

    AxisAlignedBB actorBox(const Vector3f &position, const ActorSize &size) {
        const float half = size.mWidth * 0.5f;
        return AxisAlignedBB(position.x - half, position.y, position.z - half,
                             position.x + half, position.y + size.mHeight, position.z + half);
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

        if (player.getGameType() == (int32_t) GameType::Creative)
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
            const bool standardTool = blockData->mToolType == BlockToolType::Pickaxe ||
                                      blockData->mToolType == BlockToolType::Shovel ||
                                      blockData->mToolType == BlockToolType::Axe ||
                                      blockData->mToolType == BlockToolType::Hoe;
            customSpeed = CustomContentRegistry::getInstance().getDiggerSpeed(
                    heldItem.mDefinition->getIdentifier(), blockData->mName, standardTool, customUseEfficiency);
            customTool = customSpeed > 0;
        }

        const bool matchesTool = customTool || (itemData != nullptr && blockData->mToolType != BlockToolType::None &&
                                (int32_t) itemData->mToolType == (int32_t) blockData->mToolType &&
                                itemData->mToolTier >= (int32_t) blockData->mToolTier);

        const bool canHarvest = blockData->mToolType == BlockToolType::None || blockData->mHandHarvest ||
                                matchesTool;

        double seconds = canHarvest ? blockHardness * 1.5 : blockHardness * 5.0;

        float speedMultiplier = 1.0f;
        const bool correctTool = matchesTool;

        if (correctTool)
            speedMultiplier = customTool ? (float) customSpeed
                                         : toolSpeedMultiplier((BlockToolTier) (itemData == nullptr ? 0
                                                                                                    : itemData->mToolTier));

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

        if (player.getTicksSinceInAir() < 5 && !player.isFlying())
            seconds *= 5.0;

        const bool inWater = player.getFlags().get(ActorFlag::Swimming);
        const bool hasAquaAffinity =
                ItemEnchantments::getLevel(player.getInventory().getArmor(PlayerInventory::ARMOR_HEAD),
                                           EnchantmentIds::AQUA_AFFINITY) > 0;
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

    int32_t breakSpeedEventData(double speed) {
        return (int32_t) std::clamp(65535.0 * speed, 0.0, 65535.0);
    }

    int64_t expectedBreakTicks(ServerPlayer &player, const BlockData *blockData) {
        const double seconds = calculateBreakSeconds(player, blockData);
        if (seconds < 0.0)
            return std::numeric_limits<int64_t>::max();

        const double ticks = std::ceil(seconds * 20.0) - 1.0;
        return std::max<int64_t>(0, (int64_t) ticks);
    }

    bool hasReachedBreakTime(ServerNetworkHandler &owner, ServerPlayer &player,
                             const BlockData *blockData) {
        if (!player.isBreakingBlock())
            return false;

        const int64_t elapsedTicks = owner.getCurrentTick() - player.getBreakStartTick();
        return elapsedTicks >= expectedBreakTicks(player, blockData);
    }

    bool finiteClickPosition(const Vector3f &clickPosition) {
        return std::isfinite(clickPosition.x) && std::isfinite(clickPosition.y)
               && std::isfinite(clickPosition.z);
    }

    uint64_t steadyTimeMicros() {
        using namespace std::chrono;
        return (uint64_t) duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
    }

    float distanceSquared(const Vector3f &left, const Vector3f &right) {
        const float x = left.x - right.x;
        const float y = left.y - right.y;
        const float z = left.z - right.z;
        return x * x + y * y + z * z;
    }

    bool isDuplicateRightClick(ServerPlayer &player, const ItemUseTransaction &transaction) {
        const uint64_t now = steadyTimeMicros();
        const bool duplicate = player.hasLastRightClick()
                               && now - player.getLastRightClickTimeMicros() < RIGHT_CLICK_DEDUP_MICROSECONDS
                               && player.getLastRightClickFace() == transaction.mBlockFace
                               && player.getLastRightClickBlockPosition() == transaction.mBlockPosition
                               && distanceSquared(player.getLastRightClickPlayerPosition(),
                                                  transaction.mPlayerPosition) < 0.00001f
                               && distanceSquared(player.getLastRightClickPosition(), transaction.mClickPosition)
                                  < 0.00001f;

        player.recordRightClick(transaction.mBlockPosition, transaction.mBlockFace,
                                transaction.mPlayerPosition, transaction.mClickPosition, now);
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

    bool isBreakableInCreative(const ServerPlayer &player, const std::string &identifier) {
        if (BlockIdentifier::equalsAny(identifier, {
                "minecraft:end_gateway", "minecraft:fire", "minecraft:soul_fire", "minecraft:invisible_bedrock",
                "minecraft:jigsaw", "minecraft:moving_block", "minecraft:bubble_column", "minecraft:water",
                "minecraft:flowing_water", "minecraft:lava", "minecraft:flowing_lava"}))
            return false;

        if (BlockIdentifier::equalsAny(identifier, {"minecraft:allow", "minecraft:deny", "minecraft:border_block"}))
            return player.isOp();

        return true;
    }

    void sendCurrentBlockState(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
        Level &level = owner.getLevelFor(player);
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return;

        const BlockState state = level.getBlockState(position.x, position.y, position.z);

        UpdateBlockPacket update;
        update.mBlockPosition = position;
        update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
        update.mFlags = UpdateBlockPacket::Flag::All;
        update.mDataLayer = 0;
        BlockActionHandler::broadcastToViewers(owner, level,
                                               Vector3f((float) position.x + 0.5f,
                                                        (float) position.y + 0.5f,
                                                        (float) position.z + 0.5f),
                                               update);
    }

    void syncBlocksNearby(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &clicked, int face) {
        static const int sides[6][3] = {
                {0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
        };
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
                const BlockState state = level.getBlockStateAtLayer(position.x, position.y, position.z, layer);

                UpdateBlockPacket update;
                update.mBlockPosition = position;
                update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
                update.mFlags = UpdateBlockPacket::Flag::All;
                update.mDataLayer = layer;
                owner.getNetworkHandler().send(player.getNetworkIdentifier(), update, owner.getCodecContext());
            }
        }
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

        if (&owner.getLevelFor(entry.second) == &level && entry.second.getSentChunks().count(key) != 0)
            owner.getNetworkHandler().send(entry.first, packet, owner.getCodecContext());
    }
}

void BlockActionHandler::broadcastBlockUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                              const BlockState &state) {
    UpdateBlockPacket update;
    update.mBlockPosition = position;
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;
    broadcastToViewers(owner, level,
                       Vector3f((float) position.x + 0.5f,
                                (float) position.y + 0.5f,
                                (float) position.z + 0.5f),
                       update);
}

bool BlockActionHandler::canInteractWithBlock(ServerPlayer &player, const Vector3i &position) {
    const float maxDistance = player.getGameType() == (int32_t) GameType::Creative ? 13.0f : 7.0f;

    const Vector3f feet = player.getPosition();
    const Vector3f eyePosition(feet.x, feet.y + PLAYER_BASE_OFFSET, feet.z);
    const Vector3f target((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);

    const float dx = target.x - eyePosition.x;
    const float dy = target.y - eyePosition.y;
    const float dz = target.z - eyePosition.z;

    const float distanceSquared = dx * dx + dy * dy + dz * dz;
    if (distanceSquared > maxDistance * maxDistance)
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

    const float dot = directionX * eyePosition.x + directionZ * eyePosition.z;
    const float dot1 = directionX * target.x + directionZ * target.z;
    return (dot1 - dot) >= -REACH_MAX_DIFF;
}

void BlockActionHandler::breakBlock(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
    Level &level = owner.getLevelFor(player);

    const BlockState brokenState = level.getChunk(position.x >> 4, position.z >> 4)
                                         .getBlock(position.x & 15, position.y, position.z & 15);

    if (brokenState.mName == "minecraft:air") {
        return;
    }

    PlayerBreakBlockBeforeEvent breakEvent(player, position);
    owner.getEventBus().before().mPlayerBreakBlock.emit(breakEvent);
    if (breakEvent.isCancelled()) {
        sendCurrentBlockState(owner, player, position);
        return;
    }

    const std::string brokenIdentifier = brokenState.mName;
    const BlockData *brokenData = BlockDataTable::find(brokenState.mName.c_str());
    const bool creative = player.getGameType() == (int32_t) GameType::Creative;
    const ItemStack heldItem = player.getInventory().getItemInHand();

    destroyBlock(owner, level, position, brokenState, !creative, heldItem);

    PlayerBreakBlockAfterEvent brokenEvent(player, position, brokenIdentifier);
    owner.getEventBus().after().mPlayerBreakBlock.emit(brokenEvent);

    if (brokenData != nullptr && brokenData->mHardness > 0.0f)
        owner.damagePlayerHeldItem(player, 1);

    const bool silkTouch = ItemEnchantments::getLevel(heldItem, EnchantmentIds::SILK_TOUCH) > 0;
    if (!creative && !silkTouch) {
        const int experience = ExperienceValues::getOreDropExperience(brokenState.mName);
        if (experience > 0) {
            const Vector3f orbPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                       (float) position.z + 0.5f);
            owner.spawnExperienceOrbs(level, orbPosition, experience);
        }
    }

    player.exhaust(0.005f);
}

void BlockActionHandler::spawnBlockDrops(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &brokenState, const ItemStack &tool) {
    const BlockData *brokenData = BlockDataTable::find(brokenState.mName.c_str());
    if (brokenData == nullptr)
        return;

    const Block *brokenBlock = VanillaBlocks::fromIdentifier(brokenState.mName);
    const bool silkTouch = ItemEnchantments::getLevel(tool, EnchantmentIds::SILK_TOUCH) > 0;
    const int32_t fortuneLevel = ItemEnchantments::getLevel(tool, EnchantmentIds::FORTUNE);

    auto spawnDrop = [&](const std::string &identifier, int32_t count) {
        if (identifier.empty() || count <= 0)
            return;

        Item parsedItem;
        if (!StringToItemParser::getInstance().parse(identifier, parsedItem))
            return;

        ItemStack drop;
        drop.mDefinition = owner.getItemDefinitions().getDefinition(parsedItem.getIdentifier());
        drop.mBlockDefinition = owner.getBlockDefinitions().getDefinition(parsedItem.getIdentifier());
        drop.mCount = count;

        if (brokenBlock != nullptr)
            brokenBlock->writeDropContents(level, position, drop);

        const Vector3f dropPosition((float) position.x + 0.5f, (float) position.y + 0.5f,
                                    (float) position.z + 0.5f);
        owner.dropItem(level, dropPosition, drop, ItemActorHandler::randomDropMotion(),
                       ItemActorHandler::DROP_PICKUP_DELAY);
    };

    const bool grassLike = brokenState.mName == "minecraft:short_grass"
                           || brokenState.mName == "minecraft:tall_grass";

    if (grassLike) {
        const bool usedShears = tool.mDefinition != nullptr
                                && tool.mDefinition->getIdentifier() == "minecraft:shears";
        if (usedShears)
            spawnDrop(furnaceDropIdentifier(brokenState.mName), 1);

        if (rand() % 8 == 0) {
            const int32_t seedCount = fortuneLevel == 0 ? 1 : 1 + rand() % (fortuneLevel * 2);
            spawnDrop("minecraft:wheat_seeds", seedCount);
        }
        return;
    }

    std::string dropIdentifier;
    int32_t dropCount = 0;
    const std::string normalizedDropIdentifier = furnaceDropIdentifier(brokenState.mName);

    if (silkTouch && brokenData->mSilkTouch) {
        dropIdentifier = normalizedDropIdentifier;
        dropCount = 1;
    } else if (brokenData->mDropKind == BlockDropKind::Self) {
        dropIdentifier = normalizedDropIdentifier;
        dropCount = brokenData->mDropMin;
    } else if (brokenData->mDropKind == BlockDropKind::Other && brokenData->mDropIdentifier != nullptr) {
        dropIdentifier = brokenData->mDropIdentifier;
        const int32_t range = (int32_t) brokenData->mDropMax - (int32_t) brokenData->mDropMin + 1;
        dropCount = (int32_t) brokenData->mDropMin + (range > 0 ? rand() % range : 0);

        if (fortuneLevel > 0)
            dropCount += rand() % (fortuneLevel + 1);
    }

    spawnDrop(dropIdentifier, dropCount);
}

void BlockActionHandler::destroyBlock(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &brokenState, bool dropItems, const ItemStack &tool) {
    const int32_t brokenHash = BlockStateHasher::hash(brokenState.mName, brokenState.mStates);
    const Block *brokenBlock = VanillaBlocks::fromIdentifier(brokenState.mName);

    level.setBlockState(position.x, position.y, position.z, BlockState("minecraft:air"));

    if (brokenBlock != nullptr) {
        for (const Vector3i &affected: brokenBlock->getAffectedBlocks(level, position, brokenState)) {
            const BlockState affectedState = level.getBlockState(affected.x, affected.y, affected.z);
            if (affectedState.mName == "minecraft:air")
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

    LevelEventPacket destroy;
    destroy.mEventId = LevelEventPacket::Event::ParticleDestroy;
    destroy.mPosition = Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    destroy.mData = brokenHash;

    broadcastToViewers(owner, level, destroy.mPosition, update);
    broadcastToViewers(owner, level, destroy.mPosition, destroy);

    if (brokenBlock != nullptr)
        brokenBlock->onBroken(owner, level, position, brokenState);

    level.onBlockBroken(position, brokenState);
}

void BlockActionHandler::startBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                            const Vector3i &position, int32_t face) {
    Level &level = owner.getLevelFor(player);
    if (position.y < level.getMinY() || position.y > level.getMaxY())
        return;

    if (player.isBreakingBlock() && player.getBreakingBlockPosition() == position)
        return;

    if (player.isBreakingBlock())
        stopBreakingBlock(owner, player);

    if (isInSpawnProtection(owner, player, level, position)) {
        sendCurrentBlockState(owner, player, position);
        return;
    }

    const BlockState &state = level.getChunk(position.x >> 4, position.z >> 4)
                                    .getBlock(position.x & 15, position.y, position.z & 15);

    if (state.mName == "minecraft:air") {
        return;
    }

    const Block *punchedBlock = VanillaBlocks::fromIdentifier(state.mName);
    if (punchedBlock != nullptr && punchedBlock->onPunch(owner, player, position, state))
        return;

    const BlockData *blockData = BlockDataTable::find(state.mName.c_str());
    const bool creative = player.getGameType() == (int32_t) GameType::Creative;
    if (creative && !isBreakableInCreative(player, state.mName))
        return;

    const double breakSpeed = creative ? 1.0 : calculateBreakProgressPerTick(player, blockData);

    if (breakSpeed <= 0.0) {
        return;
    }

    player.startBreakingBlock(position, face, breakSpeed, owner.getCurrentTick());

    LevelEventPacket start;
    start.mEventId = LevelEventPacket::Event::BlockStartBreak;
    start.mPosition = Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    start.mData = breakSpeedEventData(breakSpeed);

    broadcastToViewers(owner, level, start.mPosition, start);

    if (creative)
        completeBreakingBlock(owner, player, position);
}

void BlockActionHandler::continueBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isBreakingBlock())
        return;

    if (owner.getCurrentTick() <= player.getBreakStartTick())
        return;

    const Vector3i position = player.getBreakingBlockPosition();
    Level &level = owner.getLevelFor(player);
    if (position.y < level.getMinY() || position.y > level.getMaxY()) {
        stopBreakingBlock(owner, player);
        return;
    }

    const BlockState &state = level.getChunk(position.x >> 4, position.z >> 4)
                                    .getBlock(position.x & 15, position.y, position.z & 15);

    if (state.mName == "minecraft:air") {
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

        LevelEventPacket update;
        update.mEventId = LevelEventPacket::Event::BlockUpdateBreak;
        update.mPosition = Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f,
                                    (float) position.z + 0.5f);
        update.mData = breakSpeedEventData(newBreakSpeed);
        broadcastToViewers(owner, level, update.mPosition, update);
    }

    player.addBreakProgress(player.getBreakSpeed());

    if (player.getBreakProgress() >= 1.0) {
        completeBreakingBlock(owner, player, position, false);
    }
}

void BlockActionHandler::completeBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                               const Vector3i &position, bool clientPredicted) {
    Level &playerLevel = owner.getLevelFor(player);
    if (position.y < playerLevel.getMinY() || position.y > playerLevel.getMaxY())
        return;

    if (!canInteractWithBlock(player, position) || isInSpawnProtection(owner, player, playerLevel, position)) {
        sendCurrentBlockState(owner, player, position);
        return;
    }

    const bool creative = player.getGameType() == (int32_t) GameType::Creative;
    if (creative && !player.isBreakingBlock()) {
        startBreakingBlock(owner, player, position, 0);
        return;
    }

    if (!creative) {
        Level &level = owner.getLevelFor(player);
        const BlockState &state = level.getChunk(position.x >> 4, position.z >> 4)
                                        .getBlock(position.x & 15, position.y, position.z & 15);
        if (state.mName == "minecraft:air") {
            stopBreakingBlock(owner, player);
            return;
        }

        if (!clientPredicted) {
            if (!player.isBreakingBlock() || player.getBreakingBlockPosition() != position) {
                sendCurrentBlockState(owner, player, position);
                return;
            }

            const BlockData *blockData = BlockDataTable::find(state.mName.c_str());
            if (!hasReachedBreakTime(owner, player, blockData) ||
                player.getBreakProgress() + BREAK_PROGRESS_EPSILON < 1.0) {
                sendCurrentBlockState(owner, player, position);
                return;
            }
        }
    }

    if (player.isBreakingBlock())
        stopBreakingBlock(owner, player);

    breakBlock(owner, player, position);
}

void BlockActionHandler::sendBreakingFx(ServerNetworkHandler &owner, ServerPlayer &player) {
    const Vector3i position = player.getBreakingBlockPosition();
    Level &level = owner.getLevelFor(player);
    const BlockState &state = level.getChunk(position.x >> 4, position.z >> 4)
                                    .getBlock(position.x & 15, position.y, position.z & 15);

    if (state.mName == "minecraft:air")
        return;

    const int32_t blockHash = BlockStateHasher::hash(state.mName, state.mStates);
    const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);

    LevelEventPacket punch;
    punch.mEventId = LevelEventPacket::Event::ParticlePunchBlock;
    punch.mPosition = center;
    punch.mData = blockHash | (player.getBreakingFace() << 24);
    broadcastToViewers(owner, level, center, punch);

    owner.playLevelSound(level, LevelSoundEvent::HIT, center, "", blockHash);

    ActorEventPacket swing;
    swing.mRuntimeActorId = player.getRuntimeId();
    swing.mEventId = (uint8_t) EntityEventType::ArmSwing;
    swing.mEventData = 0;
    swing.mHasFirePosition = false;
    broadcastToViewers(owner, level, center, swing, &player);

}

void BlockActionHandler::stopBreakingBlock(ServerNetworkHandler &owner, ServerPlayer &player) {
    if (!player.isBreakingBlock())
        return;

    const Vector3i position = player.getBreakingBlockPosition();
    player.stopBreakingBlock();

    LevelEventPacket stop;
    stop.mEventId = LevelEventPacket::Event::BlockStopBreak;
    stop.mPosition = Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    stop.mData = 0;

    broadcastToViewers(owner, owner.getLevelFor(player), stop.mPosition, stop);

}

bool BlockActionHandler::matchesTransactionItem(const ItemStack &held, const ItemStack &sent) {
    if (held.isAir() || sent.isAir())
        return held.isAir() && sent.isAir();

    return held.mDefinition->getIdentifier() == sent.mDefinition->getIdentifier()
           && held.mDamage == sent.mDamage;
}

bool BlockActionHandler::isBlockChangeAllowed(Level &level, const Vector3i &position, const ServerPlayer &player) {
    for (int32_t y = position.y - 1; y >= level.getMinY(); --y) {
        const std::string &identifier = level.getBlockState(position.x, y, position.z).mName;

        if (identifier == "minecraft:allow")
            return true;

        if (identifier == "minecraft:deny")
            return player.getGameType() == (int32_t) GameType::Creative;
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

        if (other.getGameType() == (int32_t) GameType::Spectator)
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

void BlockActionHandler::placeBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                    const ItemUseTransaction &transaction) {
    if (transaction.mBlockFace < 0 || transaction.mBlockFace >= 6)
        return;

    if (isDuplicateRightClick(player, transaction))
        return;

    if (!player.isSpawned() || player.isDead() || player.getGameType() == (int32_t) GameType::Spectator)
        return;

    if (transaction.mHotbarSlot < 0 || transaction.mHotbarSlot >= PlayerInventory::HOTBAR_SIZE)
        return;

    PlayerInventory &inventory = player.getInventory();
    bool selectedSlotChanged = false;
    if (inventory.getSelectedSlot() != transaction.mHotbarSlot) {
        player.getInventoryManager().onClientSelectHotbarSlot(transaction.mHotbarSlot);
        inventory.setSelectedSlot(transaction.mHotbarSlot);
        selectedSlotChanged = true;
    }

    bool handled = false;

    if (canInteractWithBlock(player, transaction.mBlockPosition)) {
        if (player.getGameType() == (int32_t) GameType::Creative)
            handled = interactBlock(owner, player, transaction, selectedSlotChanged);
        else if (matchesTransactionItem(inventory.getItemInHand(), transaction.mItemInHand))
            handled = interactBlock(owner, player, transaction, selectedSlotChanged);
    }

    if (handled)
        return;

    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, inventory.getSelectedSlot());

    if (selectedSlotChanged)
        InventoryHandler::sendHeldItem(owner, player);

    const Vector3f playerPosition = player.getPosition();
    const float deltaX = (float) transaction.mBlockPosition.x - playerPosition.x;
    const float deltaY = (float) transaction.mBlockPosition.y - playerPosition.y;
    const float deltaZ = (float) transaction.mBlockPosition.z - playerPosition.z;

    if (deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ > MAX_SYNC_DISTANCE_SQUARED)
        return;

    syncBlocksNearby(owner, player, transaction.mBlockPosition, transaction.mBlockFace);
}

bool BlockActionHandler::interactBlock(ServerNetworkHandler &owner, ServerPlayer &player,
                                       const ItemUseTransaction &transaction, bool selectedSlotChanged) {
    if (!finiteClickPosition(transaction.mClickPosition))
        return false;

    PlayerInventory &inventory = player.getInventory();
    Level &level = owner.getLevelFor(player);

    const BlockState clickedState = level.getBlockState(transaction.mBlockPosition.x,
                                                        transaction.mBlockPosition.y,
                                                        transaction.mBlockPosition.z);

    static const int offsets[6][3] = {
            {0,  -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}
    };

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

    if (clickedState.mName == "minecraft:air")
        return false;

    if (isInSpawnProtection(owner, player, level, transaction.mBlockPosition))
        return false;

    owner.getScriptEngine().onItemUseOnBlock(player, transaction.mBlockPosition.x,
                                             transaction.mBlockPosition.y, transaction.mBlockPosition.z);

    const Block *clickedBlock = VanillaBlocks::fromIdentifier(clickedState.mName);
    if (clickedBlock != nullptr)
        clickedBlock->onTouch(owner, player, transaction.mBlockPosition, clickedState);

    const ItemStack &interactItem = inventory.getItemInHand();
    const bool onCooldown = player.hasItemCooldown(interactItem, owner.getCurrentTick());

    const bool useBlock = !player.getFlags().get(ActorFlag::Sneaking) || inventory.getItemInHand().isAir();

    if (!onCooldown && useBlock && clickedBlock != nullptr &&
        clickedBlock->onInteract(owner, player, transaction.mBlockPosition, clickedState))
        return true;

    if (!onCooldown && !interactItem.isAir() && interactItem.mDefinition != nullptr) {
        const Item *itemType = VanillaItems::fromIdentifier(interactItem.mDefinition->getIdentifier());
        if (itemType != nullptr && itemType->onUseOnBlock(owner, player, interactItem, transaction.mBlockPosition,
                                                          transaction.mBlockFace, transaction.mClickPosition))
            return true;
    }

    if (transaction.mClientInteractPrediction != ItemUsePredictedResult::Success)
        return true;

    const ItemStack &heldItem = inventory.getItemInHand();

    const bool bucket = BucketItem::isBucket(heldItem);
    if (heldItem.isAir() || heldItem.mCount <= 0 || heldItem.mDefinition == nullptr
        || (!bucket && heldItem.mBlockDefinition == nullptr)) {
        if (!bucket && !heldItem.isAir() && heldItem.mDefinition != nullptr)
            owner.emitItemUse(player);
        return false;
    }

    if (bucket) {
        BucketItem::use(owner, player, transaction);
        return true;
    }

    if (player.getGameType() == (int32_t) GameType::Adventure
        && std::find(heldItem.mCanPlace.begin(), heldItem.mCanPlace.end(), clickedState.mName)
           == heldItem.mCanPlace.end()) {
        return false;
    }

    broadcastArmSwing(owner, player,
                      Vector3f((float) transaction.mBlockPosition.x + 0.5f,
                               (float) transaction.mBlockPosition.y + 0.5f,
                               (float) transaction.mBlockPosition.z + 0.5f));

    if (!isChunkReady(level, target))
        return false;

    if (isInSpawnProtection(owner, player, level, target))
        return false;

    if (!isBlockChangeAllowed(level, target, player))
        return false;

    const BlockDefinition &definition = *heldItem.mBlockDefinition;

    BlockState mergedState(definition.getIdentifier(), definition.getState());
    PlacementMergeResult mergeResult = PlacementMergeResult::None;
    if (heldBlock != nullptr) {
        mergeResult = heldBlock->mergePlacement(level, transaction.mBlockPosition, face, transaction.mClickPosition,
                                                target, mergedState);
    }

    if (mergeResult == PlacementMergeResult::Rejected)
        return false;

    const bool merged = mergeResult == PlacementMergeResult::Merged;

    //TODO: snowlogging is not implemented
    const BlockState targetState = level.getBlockState(target.x, target.y, target.z);
    if (!merged && !BlockSupport::isReplaceable(targetState))
        return false;

    const BlockData *placedData = BlockDataTable::find(definition.getIdentifier().c_str());

    const BlockState targetOverlay = level.getBlockStateAtLayer(target.x, target.y, target.z, 1);
    const bool targetHasLiquid = LiquidView(targetState).isLiquid() || LiquidView(targetOverlay).isLiquid();
    if (!merged && targetHasLiquid && placedData != nullptr && placedData->mWaterloggingLevel == 0
        && !placedData->mSolid)
        return false;

    BlockState placedState = merged ? mergedState : BlockPlacementComponent::apply(
            BlockState(definition.getIdentifier(), definition.getState()), &level,
            player.getRotation().y, player.getRotation().x, placementFace,
            transaction.mClickPosition, player.getPosition(), target);

    const Block *placedBlock = VanillaBlocks::fromIdentifier(placedState.mName);
    if (!merged && placedBlock != nullptr && !placedBlock->canPlaceAt(level, target, placementFace))
        return false;

    if (isBlockedByActor(owner, level, target, placedState, player))
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

    const bool waterlogsTarget = LiquidView(targetState).isWater() && placedData != nullptr
                                 && placedData->mWaterloggingLevel > 0;

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

    if (player.getGameType() != (int32_t) GameType::Creative) {
        ItemStack remaining = heldItem;
        remaining.mCount--;
        if (remaining.mCount <= 0)
            remaining = ItemStack::air();
        inventory.setItemInHand(std::move(remaining));
        player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                              inventory.getSelectedSlot());
    }

    if (selectedSlotChanged || player.getGameType() != (int32_t) GameType::Creative)
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

    owner.playLevelSound(level, LevelSoundEvent::PLACE, targetCenter, "", (int32_t) blockHash);

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
    return true;
}
