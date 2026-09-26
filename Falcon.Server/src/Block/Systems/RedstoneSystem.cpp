#include "Block/Systems/RedstoneSystem.h"

#include "Block/Actor/ChestBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Block/BlockData.h"
#include "Actor/Misc/PrimedTntActor.h"
#include "Block/Blocks/ButtonBlock.h"
#include "Block/Blocks/CommandBlock.h"
#include "Block/Blocks/DaylightDetectorBlock.h"
#include "Block/Blocks/DoorBlock.h"
#include "Block/Blocks/LeverBlock.h"
#include "Block/Blocks/OpenableBlock.h"
#include "Block/Blocks/DoorOrientationBlock.h"
#include "Block/Blocks/FenceGateOrientationBlock.h"
#include "Block/Blocks/TrapdoorOrientationBlock.h"
#include "Block/Blocks/PressurePlateBlock.h"
#include "Block/Blocks/RedStoneWireBlock.h"
#include "Block/Blocks/ObserverBlock.h"
#include "Block/Blocks/RedstoneBlock.h"
#include "Block/Blocks/RedstoneComparatorBlock.h"
#include "Block/Blocks/RedstoneDiodeBlock.h"
#include "Block/Blocks/RedstoneLampBlock.h"
#include "Block/Blocks/RedstoneRepeaterBlock.h"
#include "Block/Blocks/RedstoneTorchBlock.h"
#include "Block/Blocks/TrappedChestBlock.h"
#include "Block/Blocks/SlabBlock.h"
#include "Block/Blocks/TntBlock.h"
#include "Block/Blocks/VanillaBlocks.h"
#include "Block/Systems/CommandBlockSystem.h"
#include "Block/Systems/FallingBlockSystem.h"
#include "Block/Systems/FireSystem.h"
#include "Block/Systems/PistonSystem.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Plugin/PluginEvent.h"
#include "Plugin/PluginManager.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

Vector3i RedstoneFace::offset(int face)
{
    switch (face) {
        case RedstoneFace::DOWN:
            return Vector3i(0, -1, 0);
        case RedstoneFace::UP:
            return Vector3i(0, 1, 0);
        case RedstoneFace::NORTH:
            return Vector3i(0, 0, -1);
        case RedstoneFace::SOUTH:
            return Vector3i(0, 0, 1);
        case RedstoneFace::WEST:
            return Vector3i(-1, 0, 0);
        case RedstoneFace::EAST:
            return Vector3i(1, 0, 0);
        default:
            return Vector3i(0, 0, 0);
    }
}

Vector3i RedstoneFace::relative(const Vector3i &position, int face)
{
    const Vector3i delta = RedstoneFace::offset(face);
    return Vector3i(position.x + delta.x, position.y + delta.y, position.z + delta.z);
}

int RedstoneFace::opposite(int face)
{
    if (face < 0 || face >= RedstoneFace::COUNT)
        return RedstoneFace::NONE;

    return face ^ 1;
}

bool RedstoneFace::isHorizontal(int face)
{
    return face >= RedstoneFace::NORTH && face <= RedstoneFace::EAST;
}

int RedstoneFace::rotateY(int face)
{
    switch (face) {
        case RedstoneFace::NORTH:
            return RedstoneFace::EAST;
        case RedstoneFace::EAST:
            return RedstoneFace::SOUTH;
        case RedstoneFace::SOUTH:
            return RedstoneFace::WEST;
        case RedstoneFace::WEST:
            return RedstoneFace::NORTH;
        default:
            return RedstoneFace::NONE;
    }
}

int RedstoneFace::rotateYCounterClockwise(int face)
{
    switch (face) {
        case RedstoneFace::NORTH:
            return RedstoneFace::WEST;
        case RedstoneFace::WEST:
            return RedstoneFace::SOUTH;
        case RedstoneFace::SOUTH:
            return RedstoneFace::EAST;
        case RedstoneFace::EAST:
            return RedstoneFace::NORTH;
        default:
            return RedstoneFace::NONE;
    }
}

const char *RedstoneFace::name(int face)
{
    switch (face) {
        case RedstoneFace::DOWN:
            return "down";
        case RedstoneFace::UP:
            return "up";
        case RedstoneFace::NORTH:
            return "north";
        case RedstoneFace::SOUTH:
            return "south";
        case RedstoneFace::WEST:
            return "west";
        case RedstoneFace::EAST:
            return "east";
        default:
            return "down";
    }
}

int RedstoneFace::fromName(const std::string &name)
{
    if (name == "down")
        return RedstoneFace::DOWN;
    if (name == "up")
        return RedstoneFace::UP;
    if (name == "north")
        return RedstoneFace::NORTH;
    if (name == "south")
        return RedstoneFace::SOUTH;
    if (name == "west")
        return RedstoneFace::WEST;
    if (name == "east")
        return RedstoneFace::EAST;

    return RedstoneFace::NONE;
}

namespace {
    const int TORCH_TICK_RATE = 2;
    const int BUTTON_HOLD_TICKS = 30;
    const int PRESSURE_PLATE_RECHECK_TICKS = 20;
    const int LIT_LAMP_TURN_OFF_DELAY = 4;
    const int OBSERVER_PULSE_TICKS = 2;
    const int COMPARATOR_DELAY = 2;
    const int DIODE_PLACE_DELAY = 1;
    const char *SOUND_POWER_ON = LevelSoundEvent::POWER_ON;
    const char *SOUND_POWER_OFF = LevelSoundEvent::POWER_OFF;

    struct RedstoneState {
        std::unordered_map<int64_t, int32_t> mComparatorOutputs;
        std::vector<Vector3i> mPendingNotifications;
    };

    std::array<RedstoneState, Dimension::DIMENSION_COUNT> gStates;

    RedstoneState &stateOf(Level &level)
    {
        return gStates[level.getDimensionId()];
    }

    int redstoneChange(Level &level, const Vector3i &position, int previousPower, int power)
    {
        PluginManager *plugins = PluginManager::findWithSubscribers(FALCON_EVENT_REDSTONE_CHANGE);
        if (plugins == nullptr)
            return power;

        PluginEvent event;
        event.mType = FALCON_EVENT_REDSTONE_CHANGE;
        event.mLevel = &level;
        event.mBlockPosition = position;
        event.mAmount = (double) power;
        event.mPreviousAmount = (double) previousPower;
        plugins->dispatch(event);
        return std::clamp((int) event.mAmount, 0, RedstoneSystem::MAX_SIGNAL);
    }

    template<typename T>
    const T *blockAs(const BlockState &state)
    {
        return VanillaBlocks::getAs<T>(state.mName);
    }

    template<typename T>
    bool isA(const BlockState &state)
    {
        return blockAs<T>(state) != nullptr;
    }

    int trappedChestSignal(Level &level, const Vector3i &position)
    {
        const ChestBlockActor *chest = level.getBlockActors().find<ChestBlockActor>(position);
        if (chest == nullptr)
            return 0;

        return std::min(chest->getViewerCount(), RedstoneSystem::MAX_SIGNAL);
    }

    bool isChunkReady(Level &level, const Vector3i &position)
    {
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return false;

        return level.isChunkResident(position.x >> 4, position.z >> 4);
    }

    BlockState stateAt(Level &level, const Vector3i &position)
    {
        if (!isChunkReady(level, position))
            return BlockState("minecraft:air");

        return level.getBlockState(position.x, position.y, position.z);
    }

    int stateInt(const BlockState &state, const std::string &key, int fallback)
    {
        const Tag *tag = state.mStates.get(key);
        if (tag == nullptr)
            return fallback;

        if (tag->getType() == Tag::Type::Int)
            return tag->asInt();

        if (tag->getType() == Tag::Type::Byte)
            return (int) tag->asByte();

        if (tag->getType() == Tag::Type::Short)
            return (int) tag->asShort();

        return fallback;
    }

    bool stateBool(const BlockState &state, const std::string &key, bool fallback)
    {
        return state.mStates.getBool(key, fallback);
    }

    std::string stateString(const BlockState &state, const std::string &key, const std::string &fallback)
    {
        return state.mStates.getString(key, fallback);
    }

    int torchFacing(const BlockState &state)
    {
        const std::string attachment = stateString(state, "torch_facing_direction", "unknown");
        if (attachment == "west")
            return RedstoneFace::EAST;
        if (attachment == "east")
            return RedstoneFace::WEST;
        if (attachment == "north")
            return RedstoneFace::SOUTH;
        if (attachment == "south")
            return RedstoneFace::NORTH;

        return RedstoneFace::UP;
    }

    int leverFacing(const BlockState &state)
    {
        const std::string direction = stateString(state, "lever_direction", "down_east_west");
        if (direction == "down_east_west" || direction == "down_north_south")
            return RedstoneFace::DOWN;
        if (direction == "up_east_west" || direction == "up_north_south")
            return RedstoneFace::UP;

        const int named = RedstoneFace::fromName(direction);
        return named == RedstoneFace::NONE ? RedstoneFace::DOWN : named;
    }

    int cardinalFacing(const BlockState &state)
    {
        const int named = RedstoneFace::fromName(stateString(state, "minecraft:cardinal_direction", "south"));
        return named == RedstoneFace::NONE ? RedstoneFace::SOUTH : named;
    }

    int observerFacing(const BlockState &state)
    {
        const int named = RedstoneFace::fromName(stateString(state, "minecraft:facing_direction", "down"));
        return named == RedstoneFace::NONE ? RedstoneFace::DOWN : named;
    }

    int buttonFacing(const BlockState &state)
    {
        return std::clamp(stateInt(state, "facing_direction", 0), 0, RedstoneFace::COUNT - 1);
    }

    bool isDiodePowered(const BlockState &state)
    {
        const RedstoneDiodeBlock *diode = blockAs<RedstoneDiodeBlock>(state);
        if (diode == nullptr)
            return false;

        return diode->isPowered(state);
    }

    int diodeFacing(const BlockState &state)
    {
        return cardinalFacing(state);
    }

    int diodeDelay(const BlockState &state)
    {
        if (isA<RedstoneComparatorBlock>(state))
            return COMPARATOR_DELAY;

        return (1 + stateInt(state, "repeater_delay", 0)) * 2;
    }

    BlockState diodePoweredState(const BlockState &state)
    {
        return blockAs<RedstoneDiodeBlock>(state)->getPoweredState(state);
    }

    BlockState diodeUnpoweredState(const BlockState &state)
    {
        return blockAs<RedstoneDiodeBlock>(state)->getUnpoweredState(state);
    }

    Vector3f centerOf(const Vector3i &position)
    {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    int wireSignal(const BlockState &state)
    {
        return std::clamp(stateInt(state, "redstone_signal", 0), 0, RedstoneSystem::MAX_SIGNAL);
    }

    bool canConnectTo(Level &level, const Vector3i &position, int side)
    {
        const BlockState state = stateAt(level, position);
        if (isA<RedStoneWireBlock>(state))
            return true;

        if (isA<RedstoneDiodeBlock>(state)) {
            const int facing = diodeFacing(state);
            return facing == side || RedstoneFace::opposite(facing) == side;
        }

        return RedstoneSystem::isPowerSource(state) && side != RedstoneFace::NONE;
    }

    bool canConnectUpwardsTo(Level &level, const Vector3i &position)
    {
        return canConnectTo(level, position, RedstoneFace::NONE);
    }

    bool wireIsPowerSourceAt(Level &level, const Vector3i &position, int side)
    {
        const Vector3i sidePosition = RedstoneFace::relative(position, side);
        const BlockState sideState = stateAt(level, sidePosition);
        const bool sideIsNormal = RedstoneSystem::isNormalBlock(sideState);
        const bool aboveIsNormal = RedstoneSystem::isNormalBlock(
                stateAt(level, RedstoneFace::relative(position, RedstoneFace::UP)));

        if (!aboveIsNormal && sideIsNormal
            && canConnectUpwardsTo(level, RedstoneFace::relative(sidePosition, RedstoneFace::UP)))
            return true;

        if (canConnectTo(level, sidePosition, side))
            return true;

        return !sideIsNormal
               && canConnectUpwardsTo(level, RedstoneFace::relative(sidePosition, RedstoneFace::DOWN));
    }

    int wireStrongPowerAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int direction)
    {
        const BlockState state = stateAt(level, position);
        if (isA<RedStoneWireBlock>(state))
            return 0;

        return RedstoneSystem::getStrongPower(owner, level, position, direction);
    }

    int wireStrongPowerAround(ServerNetworkHandler &owner, Level &level, const Vector3i &position)
    {
        int result = 0;

        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            result = std::max(result,
                              wireStrongPowerAt(owner, level, RedstoneFace::relative(position, face), face));

            if (result >= RedstoneSystem::MAX_SIGNAL)
                return result;
        }

        return result;
    }

    int wireIndirectPowerAt(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face)
    {
        const BlockState state = stateAt(level, position);
        if (isA<RedStoneWireBlock>(state))
            return 0;

        if (RedstoneSystem::isNormalBlock(state))
            return wireStrongPowerAround(owner, level, position);

        return RedstoneSystem::getWeakPower(owner, level, position, face);
    }

    int wireIndirectPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position)
    {
        int power = 0;

        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            const int blockPower = wireIndirectPowerAt(owner, level, RedstoneFace::relative(position, face), face);

            if (blockPower >= RedstoneSystem::MAX_SIGNAL)
                return RedstoneSystem::MAX_SIGNAL;

            if (blockPower > power)
                power = blockPower;
        }

        return power;
    }

    bool isTopSlab(const BlockState &state)
    {
        return VanillaBlocks::getAs<SlabBlock>(state.mName) != nullptr && SlabBlock::isTopSlab(state);
    }

    int maxCurrentStrength(Level &level, const Vector3i &position, int maxStrength)
    {
        const BlockState state = stateAt(level, position);
        if (!isA<RedStoneWireBlock>(state))
            return maxStrength;

        return std::max(wireSignal(state), maxStrength);
    }

    void updateSurroundingRedstone(ServerNetworkHandler &owner, Level &level, const Vector3i &position, bool force)
    {
        const BlockState state = stateAt(level, position);
        if (!isA<RedStoneWireBlock>(state))
            return;

        const int meta = wireSignal(state);
        int maxStrength = meta;
        const int power = wireIndirectPower(owner, level, position);

        if (power > 0 && power > maxStrength - 1)
            maxStrength = power;

        int strength = 0;

        for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
            const Vector3i adjacent = RedstoneFace::relative(position, face);

            strength = maxCurrentStrength(level, adjacent, strength);

            const Vector3i above = RedstoneFace::relative(position, RedstoneFace::UP);
            const Vector3i adjacentAbove = RedstoneFace::relative(adjacent, RedstoneFace::UP);
            if (maxCurrentStrength(level, adjacentAbove, strength) > strength
                && !RedstoneSystem::isNormalBlock(stateAt(level, above))
                && !isTopSlab(stateAt(level, adjacent)))
                strength = maxCurrentStrength(level, adjacentAbove, strength);

            const Vector3i adjacentBelow = RedstoneFace::relative(adjacent, RedstoneFace::DOWN);
            if (maxCurrentStrength(level, adjacentBelow, strength) > strength
                && !RedstoneSystem::isNormalBlock(stateAt(level, adjacent)))
                strength = maxCurrentStrength(level, adjacentBelow, strength);
        }

        if (strength > maxStrength)
            maxStrength = strength - 1;
        else if (maxStrength > 0)
            --maxStrength;
        else
            maxStrength = 0;

        if (power > maxStrength - 1)
            maxStrength = power;
        else if (power < maxStrength && strength <= maxStrength)
            maxStrength = std::max(power, strength - 1);

        if (meta != maxStrength)
            maxStrength = redstoneChange(level, position, meta, maxStrength);

        if (meta != maxStrength) {
            Tag states = state.mStates;
            states.putInt("redstone_signal", maxStrength);
            level.setBlock(position, BlockState(state.mName, states), false);

            RedstoneSystem::updateAllAroundRedstone(owner, level, position);
            return;
        }

        if (!force)
            return;

        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            RedstoneSystem::updateAroundRedstone(owner, level, RedstoneFace::relative(position, face),
                                                 RedstoneFace::opposite(face));
        }
    }

    void wireUpdateAround(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face)
    {
        const BlockState state = stateAt(level, position);
        if (!isA<RedStoneWireBlock>(state))
            return;

        RedstoneSystem::updateAroundRedstone(owner, level, position, face);

        for (int side = 0; side < RedstoneFace::COUNT; ++side) {
            RedstoneSystem::updateAroundRedstone(owner, level, RedstoneFace::relative(position, side),
                                                 RedstoneFace::opposite(side));
        }
    }

    bool isTorchPoweredFromSide(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                const BlockState &state)
    {
        const int face = RedstoneFace::opposite(torchFacing(state));
        return RedstoneSystem::isSidePowered(owner, level, RedstoneFace::relative(position, face), face);
    }

    int diodeCalculateInputStrength(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                    const BlockState &state)
    {
        const int face = diodeFacing(state);
        const Vector3i front = RedstoneFace::relative(position, face);
        const int power = RedstoneSystem::getRedstonePower(owner, level, front, face);

        if (power >= RedstoneSystem::MAX_SIGNAL)
            return power;

        const BlockState frontState = stateAt(level, front);
        return std::max(power, isA<RedStoneWireBlock>(frontState) ? wireSignal(frontState) : 0);
    }

    int comparatorCalculateInputStrength(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state)
    {
        return diodeCalculateInputStrength(owner, level, position, state);
    }

    int diodePowerOnSide(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int side,
                         bool repeater)
    {
        const BlockState state = stateAt(level, position);
        const bool alternate = repeater ? isA<RedstoneDiodeBlock>(state) : RedstoneSystem::isPowerSource(state);
        if (!alternate)
            return 0;

        if (isA<RedstoneBlock>(state))
            return RedstoneSystem::MAX_SIGNAL;

        if (isA<RedStoneWireBlock>(state))
            return wireSignal(state);

        return RedstoneSystem::getStrongPower(owner, level, position, side);
    }

    int diodePowerOnSides(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                          const BlockState &state)
    {
        const bool repeater = isA<RedstoneRepeaterBlock>(state);
        const int face = diodeFacing(state);
        const int left = RedstoneFace::rotateY(face);
        const int right = RedstoneFace::rotateYCounterClockwise(face);

        return std::max(diodePowerOnSide(owner, level, RedstoneFace::relative(position, left), left, repeater),
                        diodePowerOnSide(owner, level, RedstoneFace::relative(position, right), right, repeater));
    }

    bool diodeShouldBePowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                              const BlockState &state)
    {
        if (!isA<RedstoneComparatorBlock>(state))
            return diodeCalculateInputStrength(owner, level, position, state) > 0;

        const int input = comparatorCalculateInputStrength(owner, level, position, state);
        if (input >= RedstoneSystem::MAX_SIGNAL)
            return true;

        if (input == 0)
            return false;

        const int sidePower = diodePowerOnSides(owner, level, position, state);
        return sidePower == 0 || input >= sidePower;
    }

    bool diodeIsLocked(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state)
    {
        if (!isA<RedstoneRepeaterBlock>(state))
            return false;

        return diodePowerOnSides(owner, level, position, state) > 0;
    }

    int comparatorCalculateOutput(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  const BlockState &state)
    {
        const int input = comparatorCalculateInputStrength(owner, level, position, state);
        if (!stateBool(state, "output_subtract_bit", false))
            return input;

        return std::max(input - diodePowerOnSides(owner, level, position, state), 0);
    }

    void diodeUpdateState(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                          const BlockState &state)
    {
        if (isA<RedstoneComparatorBlock>(state)) {
            if (level.isUpdateScheduled(position))
                return;

            const int output = comparatorCalculateOutput(owner, level, position, state);
            const int power = RedstoneSystem::getComparatorOutput(level, position);

            if (output != power || isDiodePowered(state) != diodeShouldBePowered(owner, level, position, state))
                level.scheduleUpdate(position, COMPARATOR_DELAY);

            return;
        }

        if (diodeIsLocked(owner, level, position, state))
            return;

        const bool shouldBePowered = diodeShouldBePowered(owner, level, position, state);
        if (isDiodePowered(state) != shouldBePowered)
            level.scheduleUpdate(position, diodeDelay(state));
    }

    void comparatorOnChange(ServerNetworkHandler &owner, Level &level, const Vector3i &position)
    {
        const BlockState state = stateAt(level, position);
        if (!isA<RedstoneComparatorBlock>(state))
            return;

        const int output = comparatorCalculateOutput(owner, level, position, state);
        const int currentOutput = RedstoneSystem::getComparatorOutput(level, position);
        RedstoneSystem::setComparatorOutput(level, position, output);

        const bool subtractMode = stateBool(state, "output_subtract_bit", false);
        if (currentOutput == output && subtractMode)
            return;

        const bool shouldBePowered = diodeShouldBePowered(owner, level, position, state);
        const bool powered = isDiodePowered(state);

        if (powered && !shouldBePowered) {
            level.setBlock(position, diodeUnpoweredState(state), false);
            RedstoneSystem::updateComparatorOutputLevel(owner, level, position, true);
        } else if (!powered && shouldBePowered) {
            level.setBlock(position, diodePoweredState(state), false);
            RedstoneSystem::updateComparatorOutputLevel(owner, level, position, true);
        }

        const Vector3i behind = RedstoneFace::relative(position, RedstoneFace::opposite(diodeFacing(state)));
        level.updateAt(behind, BlockUpdateType::Redstone);
        RedstoneSystem::updateAroundRedstone(owner, level, behind);
    }

    int pressurePlateComputeStrength(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state)
    {
        const float minX = (float) position.x + 0.125f;
        const float maxX = (float) position.x + 0.875f;
        const float minY = (float) position.y;
        const float maxY = (float) position.y + 0.25f;
        const float minZ = (float) position.z + 0.125f;
        const float maxZ = (float) position.z + 0.875f;

        int count = 0;

        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || player.isDead() || player.getDimension() != level.getDimensionType())
                continue;

            const Vector3f &feet = player.getPosition();
            if (feet.x >= minX && feet.x <= maxX && feet.y >= minY && feet.y <= maxY
                && feet.z >= minZ && feet.z <= maxZ)
                ++count;
        }

        for (auto &entry: owner.getActors()) {
            ServerActor *actor = entry.second.get();
            if (actor == nullptr || actor->isDead() || actor->getDimension() != level.getDimensionType())
                continue;

            const Vector3f &feet = actor->getPosition();
            if (feet.x >= minX && feet.x <= maxX && feet.y >= minY && feet.y <= maxY
                && feet.z >= minZ && feet.z <= maxZ)
                ++count;
        }

        if (count == 0)
            return 0;

        return blockAs<PressurePlateBlock>(state)->getSignalForEntityCount(count);
    }

    void pressurePlateUpdateState(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                  int oldStrength)
    {
        const BlockState state = stateAt(level, position);
        if (!isA<PressurePlateBlock>(state))
            return;

        int strength = pressurePlateComputeStrength(owner, level, position, state);
        if (oldStrength != strength)
            strength = redstoneChange(level, position, oldStrength, strength);
        const bool wasPowered = oldStrength > 0;
        const bool powered = strength > 0;

        if (oldStrength != strength) {
            Tag states = state.mStates;
            states.putInt("redstone_signal", strength);
            const BlockState updated = BlockState(state.mName, states);
            level.setBlock(position, updated, false);

            RedstoneSystem::updateAroundRedstone(owner, level, position);
            RedstoneSystem::updateAroundRedstone(owner, level,
                                                 RedstoneFace::relative(position, RedstoneFace::DOWN));

            const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.1f,
                                  (float) position.z + 0.5f);
            if (!powered && wasPowered)
                owner.playLevelSound(level, SOUND_POWER_OFF, center, "", updated.getHash());
            else if (powered && !wasPowered)
                owner.playLevelSound(level, SOUND_POWER_ON, center, "", updated.getHash());
        }

        if (powered)
            level.scheduleUpdate(position, PRESSURE_PLATE_RECHECK_TICKS);
    }

    void observerOnScheduled(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state)
    {
        const int facing = observerFacing(state);
        const Vector3i behind = RedstoneFace::relative(position, RedstoneFace::opposite(facing));

        Tag states = state.mStates;

        if (!stateBool(state, "powered_bit", false)) {
            states.putByte("powered_bit", 1);
            level.setBlock(position, BlockState(state.mName, states), false);

            level.updateAt(behind, BlockUpdateType::Redstone);
            RedstoneSystem::updateAroundRedstone(owner, level, behind);
            level.scheduleUpdate(position, OBSERVER_PULSE_TICKS);
            return;
        }

        states.putByte("powered_bit", 0);
        level.setBlock(position, BlockState(state.mName, states), false);

        level.updateAt(behind, BlockUpdateType::Redstone);
        RedstoneSystem::updateAroundRedstone(owner, level, behind);
    }

    void observerOnNeighborChange(Level &level, const Vector3i &position, int side)
    {
        const BlockState state = stateAt(level, position);
        if (!isA<ObserverBlock>(state))
            return;

        if (side != observerFacing(state) || level.isUpdateScheduled(position))
            return;

        level.cancelUpdate(position);
        level.scheduleUpdate(position, OBSERVER_PULSE_TICKS);
    }
}

int64_t RedstoneSystem::packPosition(const Vector3i &position)
{
    const int64_t x = (int64_t) (position.x & 0x3ffffff);
    const int64_t y = (int64_t) (position.y & 0xfff);
    const int64_t z = (int64_t) (position.z & 0x3ffffff);
    return (x << 38) | (y << 26) | z;
}

bool RedstoneSystem::isNormalBlock(const BlockState &state)
{
    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return false;

    return !data->mTransparent && data->mSolid && !isPowerSource(state);
}

bool RedstoneSystem::isPowerSource(const BlockState &state)
{
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    if (block == nullptr)
        return false;

    if (dynamic_cast<const RedStoneWireBlock *>(block) != nullptr)
        return wireSignal(state) > 0;

    return block->isSignalSource();
}

int RedstoneSystem::getWeakPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face)
{
    const BlockState state = stateAt(level, position);
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);

    if (dynamic_cast<const RedstoneBlock *>(block) != nullptr)
        return MAX_SIGNAL;

    if (const RedstoneTorchBlock *torch = dynamic_cast<const RedstoneTorchBlock *>(block)) {
        if (!torch->isLit())
            return 0;

        return torchFacing(state) != face ? MAX_SIGNAL : 0;
    }

    if (dynamic_cast<const LeverBlock *>(block) != nullptr)
        return stateBool(state, "open_bit", false) ? MAX_SIGNAL : 0;

    if (dynamic_cast<const ButtonBlock *>(block) != nullptr)
        return stateBool(state, "button_pressed_bit", false) ? MAX_SIGNAL : 0;

    if (dynamic_cast<const PressurePlateBlock *>(block) != nullptr
        || dynamic_cast<const DaylightDetectorBlock *>(block) != nullptr)
        return std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL);

    if (dynamic_cast<const TrappedChestBlock *>(block) != nullptr)
        return trappedChestSignal(level, position);

    if (dynamic_cast<const ObserverBlock *>(block) != nullptr)
        return getStrongPower(owner, level, position, face);

    if (const RedstoneDiodeBlock *diode = dynamic_cast<const RedstoneDiodeBlock *>(block)) {
        if (!diode->isPowered(state))
            return 0;

        if (diodeFacing(state) != face)
            return 0;

        if (dynamic_cast<const RedstoneComparatorBlock *>(diode) != nullptr)
            return getComparatorOutput(level, position);

        return MAX_SIGNAL;
    }

    if (dynamic_cast<const RedStoneWireBlock *>(block) == nullptr)
        return 0;

    if (!isPowerSource(state))
        return 0;

    const int power = wireSignal(state);
    if (power == 0)
        return 0;

    if (face == RedstoneFace::UP)
        return power;

    bool connected[RedstoneFace::COUNT] = {false, false, false, false, false, false};
    bool anyConnected = false;

    for (int side = RedstoneFace::NORTH; side <= RedstoneFace::EAST; ++side) {
        if (wireIsPowerSourceAt(level, position, side)) {
            connected[side] = true;
            anyConnected = true;
        }
    }

    if (RedstoneFace::isHorizontal(face) && !anyConnected)
        return power;

    if (face < 0 || face >= RedstoneFace::COUNT)
        return 0;

    const int left = RedstoneFace::rotateYCounterClockwise(face);
    const int right = RedstoneFace::rotateY(face);
    if (connected[face] && (left == RedstoneFace::NONE || !connected[left])
        && (right == RedstoneFace::NONE || !connected[right]))
        return power;

    return 0;
}

int RedstoneSystem::getStrongPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face)
{
    const BlockState state = stateAt(level, position);
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);

    if (const RedstoneTorchBlock *torch = dynamic_cast<const RedstoneTorchBlock *>(block)) {
        if (!torch->isLit())
            return 0;

        return face == RedstoneFace::DOWN ? getWeakPower(owner, level, position, face) : 0;
    }

    if (dynamic_cast<const RedstoneBlock *>(block) != nullptr)
        return 0;

    if (dynamic_cast<const LeverBlock *>(block) != nullptr) {
        if (!stateBool(state, "open_bit", false))
            return 0;

        return leverFacing(state) == face ? MAX_SIGNAL : 0;
    }

    if (dynamic_cast<const ButtonBlock *>(block) != nullptr) {
        if (!stateBool(state, "button_pressed_bit", false))
            return 0;

        return buttonFacing(state) == face ? MAX_SIGNAL : 0;
    }

    if (dynamic_cast<const PressurePlateBlock *>(block) != nullptr)
        return face == RedstoneFace::UP ? std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL) : 0;

    if (dynamic_cast<const TrappedChestBlock *>(block) != nullptr)
        return face == RedstoneFace::UP ? trappedChestSignal(level, position) : 0;

    if (dynamic_cast<const ObserverBlock *>(block) != nullptr) {
        return stateBool(state, "powered_bit", false) && face == observerFacing(state) ? MAX_SIGNAL : 0;
    }

    if (dynamic_cast<const RedstoneDiodeBlock *>(block) != nullptr)
        return getWeakPower(owner, level, position, face);

    if (dynamic_cast<const RedStoneWireBlock *>(block) != nullptr)
        return isPowerSource(state) ? getWeakPower(owner, level, position, face) : 0;

    return 0;
}

int RedstoneSystem::getStrongPowerAround(ServerNetworkHandler &owner, Level &level, const Vector3i &position)
{
    int result = 0;

    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        result = std::max(result, getStrongPower(owner, level, RedstoneFace::relative(position, face), face));

        if (result >= MAX_SIGNAL)
            return result;
    }

    return result;
}

int RedstoneSystem::getRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face)
{
    const BlockState state = stateAt(level, position);
    return isNormalBlock(state) ? getStrongPowerAround(owner, level, position)
                                : getWeakPower(owner, level, position, face);
}

bool RedstoneSystem::isSidePowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face)
{
    return getRedstonePower(owner, level, position, face) > 0;
}

bool RedstoneSystem::isBlockPowered(ServerNetworkHandler &owner, Level &level, const Vector3i &position)
{
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (getRedstonePower(owner, level, RedstoneFace::relative(position, face), face) > 0)
            return true;
    }

    return false;
}

int RedstoneSystem::isBlockIndirectlyGettingPowered(ServerNetworkHandler &owner, Level &level,
                                                    const Vector3i &position)
{
    int power = 0;

    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        const int blockPower = getRedstonePower(owner, level, RedstoneFace::relative(position, face), face);

        if (blockPower >= MAX_SIGNAL)
            return MAX_SIGNAL;

        if (blockPower > power)
            power = blockPower;
    }

    return power;
}

bool RedstoneSystem::isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position)
{
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (isSidePowered(owner, level, RedstoneFace::relative(position, face), face))
            return true;
    }

    return isBlockPowered(owner, level, position);
}

void RedstoneSystem::updateAroundRedstone(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                          int ignoredFace)
{
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (face == ignoredFace)
            continue;

        level.updateAt(RedstoneFace::relative(position, face), BlockUpdateType::Redstone);
    }
}

void RedstoneSystem::updateAllAroundRedstone(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                             int ignoredFace)
{
    updateAroundRedstone(owner, level, position, ignoredFace);

    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (face == ignoredFace)
            continue;

        updateAroundRedstone(owner, level, RedstoneFace::relative(position, face), RedstoneFace::opposite(face));
    }
}

void RedstoneSystem::updateComparatorOutputLevel(ServerNetworkHandler &owner, Level &level,
                                                 const Vector3i &position, bool observer)
{
    for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
        const Vector3i side = RedstoneFace::relative(position, face);
        if (!isChunkReady(level, side))
            continue;

        const BlockState sideState = stateAt(level, side);

        if (isA<ObserverBlock>(sideState)) {
            if (observer)
                observerOnNeighborChange(level, side, RedstoneFace::opposite(face));
            continue;
        }

        if (isA<RedstoneDiodeBlock>(sideState)) {
            level.updateAt(side, BlockUpdateType::Redstone);
            continue;
        }

        if (!isNormalBlock(sideState))
            continue;

        const Vector3i beyond = RedstoneFace::relative(side, face);
        if (isA<RedstoneDiodeBlock>(stateAt(level, beyond)))
            level.updateAt(beyond, BlockUpdateType::Redstone);
    }

    if (!observer)
        return;

    for (int face = RedstoneFace::DOWN; face <= RedstoneFace::UP; ++face) {
        const Vector3i side = RedstoneFace::relative(position, face);
        if (isA<ObserverBlock>(stateAt(level, side)))
            observerOnNeighborChange(level, side, RedstoneFace::opposite(face));
    }
}

void RedstoneSystem::onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state, BlockUpdateType type)
{
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    const RedstoneTorchBlock *torch = dynamic_cast<const RedstoneTorchBlock *>(block);
    const RedstoneLampBlock *lamp = dynamic_cast<const RedstoneLampBlock *>(block);

    if (dynamic_cast<const RedStoneWireBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            updateSurroundingRedstone(owner, level, position, false);
    } else if (torch != nullptr && torch->isLit()) {
        if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone) {
            level.scheduleUpdate(position, TORCH_TICK_RATE);
        } else if (type == BlockUpdateType::Scheduled && isTorchPoweredFromSide(owner, level, position, state)) {
            level.setBlock(position, RedstoneTorchBlock::getUnlitState(state), false);
            updateAllAroundRedstone(owner, level, position, RedstoneFace::opposite(torchFacing(state)));
        }
    } else if (torch != nullptr) {
        if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone) {
            level.scheduleUpdate(position, TORCH_TICK_RATE);
        } else if (type == BlockUpdateType::Scheduled
                   && !isTorchPoweredFromSide(owner, level, position, state)) {
            level.setBlock(position, RedstoneTorchBlock::getLitState(state), false);
            updateAllAroundRedstone(owner, level, position, RedstoneFace::opposite(torchFacing(state)));
        }
    } else if (dynamic_cast<const RedstoneComparatorBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Scheduled)
            comparatorOnChange(owner, level, position);
        else if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            diodeUpdateState(owner, level, position, state);
    } else if (dynamic_cast<const RedstoneRepeaterBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Scheduled) {
            if (!diodeIsLocked(owner, level, position, state)) {
                const bool shouldBePowered = diodeShouldBePowered(owner, level, position, state);
                const bool powered = isDiodePowered(state);
                bool changed = false;

                if (powered && !shouldBePowered) {
                    level.setBlock(position, diodeUnpoweredState(state), false);
                    changed = true;
                } else if (!powered) {
                    level.setBlock(position, diodePoweredState(state), false);
                    changed = true;
                }

                if (changed) {
                    const Vector3i behind = RedstoneFace::relative(position,
                                                                   RedstoneFace::opposite(diodeFacing(state)));
                    level.updateAt(behind, BlockUpdateType::Redstone);
                    updateAroundRedstone(owner, level, behind);
                }
            }
        } else if (type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone) {
            diodeUpdateState(owner, level, position, state);
        }
    } else if (lamp != nullptr && !lamp->isLit()) {
        if ((type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            && isGettingPower(owner, level, position)) {
            updateComparatorOutputLevel(owner, level, position, true);
            level.setBlock(position, RedstoneLampBlock::getLitState(state), false);
        }
    } else if (lamp != nullptr) {
        if ((type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            && !isGettingPower(owner, level, position)) {
            level.scheduleUpdate(position, LIT_LAMP_TURN_OFF_DELAY);
        } else if (type == BlockUpdateType::Scheduled && !isGettingPower(owner, level, position)) {
            updateComparatorOutputLevel(owner, level, position, true);
            level.setBlock(position, RedstoneLampBlock::getUnlitState(state), false);
        }
    } else if (dynamic_cast<const ObserverBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Scheduled)
            observerOnScheduled(owner, level, position, state);
    } else if (dynamic_cast<const ButtonBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Scheduled && stateBool(state, "button_pressed_bit", false)) {
            Tag states = state.mStates;
            states.putByte("button_pressed_bit", 0);
            const BlockState released = BlockState(state.mName, states);
            level.setBlock(position, released, false);
            owner.playLevelSound(level, SOUND_POWER_OFF, centerOf(position), "", released.getHash());

            const int facing = buttonFacing(state);
            updateAroundRedstone(owner, level, position);
            updateAroundRedstone(owner, level, RedstoneFace::relative(position, RedstoneFace::opposite(facing)),
                                 facing);
        }
    } else if (dynamic_cast<const PressurePlateBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Scheduled) {
            const int power = std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL);
            if (power > 0)
                pressurePlateUpdateState(owner, level, position, power);
        }
    } else if (dynamic_cast<const DoorOrientationBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Redstone)
            DoorBlock::onRedstoneUpdate(owner, level, position, state);
    } else if (dynamic_cast<const TrapdoorOrientationBlock *>(block) != nullptr
               || dynamic_cast<const FenceGateOrientationBlock *>(block) != nullptr) {
        if (type == BlockUpdateType::Redstone)
            OpenableBlock::onRedstoneUpdate(owner, level, position, state);
    } else if (dynamic_cast<const TntBlock *>(block) != nullptr) {
        if ((type == BlockUpdateType::Normal || type == BlockUpdateType::Redstone)
            && isGettingPower(owner, level, position))
            TntBlock::prime(owner, level, position, PrimedTntActor::DEFAULT_FUSE);
    }
}

void RedstoneSystem::onRedstonePlaced(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state)
{
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    const RedstoneTorchBlock *torch = dynamic_cast<const RedstoneTorchBlock *>(block);
    const RedstoneLampBlock *lamp = dynamic_cast<const RedstoneLampBlock *>(block);

    if (dynamic_cast<const RedStoneWireBlock *>(block) != nullptr) {
        updateSurroundingRedstone(owner, level, position, true);

        for (int face = RedstoneFace::DOWN; face <= RedstoneFace::UP; ++face) {
            updateAroundRedstone(owner, level, RedstoneFace::relative(position, face),
                                 RedstoneFace::opposite(face));
        }

        for (int face = RedstoneFace::DOWN; face <= RedstoneFace::UP; ++face) {
            wireUpdateAround(owner, level, RedstoneFace::relative(position, face), RedstoneFace::opposite(face));
        }

        for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
            const Vector3i side = RedstoneFace::relative(position, face);

            if (isNormalBlock(stateAt(level, side)))
                wireUpdateAround(owner, level, RedstoneFace::relative(side, RedstoneFace::UP), RedstoneFace::DOWN);
            else
                wireUpdateAround(owner, level, RedstoneFace::relative(side, RedstoneFace::DOWN), RedstoneFace::UP);
        }
        return;
    }

    if (torch != nullptr && torch->isLit()) {
        if (isTorchPoweredFromSide(owner, level, position, state)) {
            level.setBlock(position, RedstoneTorchBlock::getUnlitState(state), false);
            updateAllAroundRedstone(owner, level, position, RedstoneFace::opposite(torchFacing(state)));
        } else {
            updateAllAroundRedstone(owner, level, position, RedstoneFace::opposite(torchFacing(state)));
        }
        return;
    }

    if (dynamic_cast<const RedstoneBlock *>(block) != nullptr) {
        updateAroundRedstone(owner, level, position);
        return;
    }

    if (dynamic_cast<const RedstoneDiodeBlock *>(block) != nullptr) {
        if (dynamic_cast<const RedstoneComparatorBlock *>(block) != nullptr)
            setComparatorOutput(level, position, comparatorCalculateOutput(owner, level, position, state));

        if (diodeShouldBePowered(owner, level, position, state))
            level.scheduleUpdate(position, DIODE_PLACE_DELAY);

        level.updateAround(position);
        updateAroundRedstone(owner, level, position);
        return;
    }

    if (lamp != nullptr && !lamp->isLit()) {
        if (isGettingPower(owner, level, position))
            level.setBlock(position, RedstoneLampBlock::getLitState(state), false);
        return;
    }

    level.updateAround(position);
    updateAroundRedstone(owner, level, position);
    updateComparatorOutputLevel(owner, level, position, true);
}

void RedstoneSystem::onRedstoneBroken(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &previous)
{
    const Block *block = VanillaBlocks::fromIdentifier(previous.mName);

    RedstoneState &redstone = stateOf(level);
    redstone.mComparatorOutputs.erase(packPosition(position));
    OpenableBlock::setManualOverride(level, position, false);

    if (dynamic_cast<const RedStoneWireBlock *>(block) != nullptr) {
        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            updateAroundRedstone(owner, level, RedstoneFace::relative(position, face));
        }

        for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
            const Vector3i side = RedstoneFace::relative(position, face);

            if (isNormalBlock(stateAt(level, side)))
                wireUpdateAround(owner, level, RedstoneFace::relative(side, RedstoneFace::UP), RedstoneFace::DOWN);
            else
                wireUpdateAround(owner, level, RedstoneFace::relative(side, RedstoneFace::DOWN), RedstoneFace::UP);
        }
        return;
    }

    if (dynamic_cast<const RedstoneTorchBlock *>(block) != nullptr) {
        updateAllAroundRedstone(owner, level, position, RedstoneFace::opposite(torchFacing(previous)));
        return;
    }

    if (dynamic_cast<const RedstoneDiodeBlock *>(block) != nullptr) {
        updateAllAroundRedstone(owner, level, position);
        return;
    }

    if (dynamic_cast<const LeverBlock *>(block) != nullptr) {
        if (stateBool(previous, "open_bit", false)) {
            const int facing = leverFacing(previous);
            level.updateAround(RedstoneFace::relative(position, RedstoneFace::opposite(facing)));
            updateAroundRedstone(owner, level, position);
            updateAroundRedstone(owner, level, RedstoneFace::relative(position, RedstoneFace::opposite(facing)),
                                 facing);
        }
        return;
    }

    if (dynamic_cast<const ButtonBlock *>(block) != nullptr) {
        if (stateBool(previous, "button_pressed_bit", false)) {
            const int facing = buttonFacing(previous);
            level.updateAround(RedstoneFace::relative(position, RedstoneFace::opposite(facing)));
        }
        updateAroundRedstone(owner, level, position);
        return;
    }

    if (dynamic_cast<const PressurePlateBlock *>(block) != nullptr) {
        if (std::clamp(stateInt(previous, "redstone_signal", 0), 0, MAX_SIGNAL) > 0) {
            updateAroundRedstone(owner, level, position);
            updateAroundRedstone(owner, level, RedstoneFace::relative(position, RedstoneFace::DOWN));
        }
        return;
    }

    level.updateAround(position);
    updateAroundRedstone(owner, level, position);
    updateComparatorOutputLevel(owner, level, position, true);
}

void RedstoneSystem::onLeverActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state)
{
    const bool powered = !stateBool(state, "open_bit", false);

    Tag states = state.mStates;
    states.putByte("open_bit", powered ? 1 : 0);
    const BlockState toggled = BlockState(state.mName, states);
    level.setBlock(position, toggled, false);

    owner.playLevelSound(level, powered ? SOUND_POWER_ON : SOUND_POWER_OFF, centerOf(position), "",
                         toggled.getHash());

    const int facing = leverFacing(toggled);
    updateAroundRedstone(owner, level, position);
    updateAroundRedstone(owner, level, RedstoneFace::relative(position, RedstoneFace::opposite(facing)), facing);
}

void RedstoneSystem::onButtonActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state)
{
    if (stateBool(state, "button_pressed_bit", false))
        return;

    level.scheduleUpdate(position, BUTTON_HOLD_TICKS);

    Tag states = state.mStates;
    states.putByte("button_pressed_bit", 1);
    const BlockState pressed = BlockState(state.mName, states);
    level.setBlock(position, pressed, false);

    owner.playLevelSound(level, SOUND_POWER_ON, centerOf(position), "", pressed.getHash());

    const int facing = buttonFacing(pressed);
    updateAroundRedstone(owner, level, position);
    updateAroundRedstone(owner, level, RedstoneFace::relative(position, RedstoneFace::opposite(facing)), facing);
}

void RedstoneSystem::onRepeaterActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state)
{
    const int delay = stateInt(state, "repeater_delay", 0);

    Tag states = state.mStates;
    states.putInt("repeater_delay", delay == 3 ? 0 : delay + 1);
    level.setBlock(position, BlockState(state.mName, states), false);
}

void RedstoneSystem::onComparatorActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                           const BlockState &state)
{
    Tag states = state.mStates;
    states.putByte("output_subtract_bit", stateBool(state, "output_subtract_bit", false) ? 0 : 1);
    level.setBlock(position, BlockState(state.mName, states), false);

    updateComparatorOutputLevel(owner, level, position, true);
    comparatorOnChange(owner, level, position);
}

int RedstoneSystem::getComparatorOutput(Level &level, const Vector3i &position)
{
    const std::unordered_map<int64_t, int32_t> &outputs = stateOf(level).mComparatorOutputs;
    const auto it = outputs.find(packPosition(position));
    if (it == outputs.end())
        return 0;

    return it->second;
}

void RedstoneSystem::setComparatorOutput(Level &level, const Vector3i &position, int output)
{
    stateOf(level).mComparatorOutputs[packPosition(position)] = std::clamp(output, 0, MAX_SIGNAL);
}

void RedstoneSystem::queueRedstoneNotification(Level &level, const Vector3i &position)
{
    stateOf(level).mPendingNotifications.push_back(position);
}

void RedstoneSystem::tick(ServerNetworkHandler &owner, Level &level)
{
    RedstoneState &redstone = stateOf(level);

    if (!redstone.mPendingNotifications.empty()) {
        const std::vector<Vector3i> notifications = std::move(redstone.mPendingNotifications);
        redstone.mPendingNotifications.clear();

        for (const Vector3i &position: notifications)
            updateAroundRedstone(owner, level, position);
    }

    std::unordered_set<int64_t> visited;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != level.getDimensionType())
            continue;

        _touchPressurePlate(owner, level, player.getPosition(), visited);
    }

    for (auto &entry: owner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor == nullptr || actor->isDead() || actor->getDimension() != level.getDimensionType())
            continue;

        _touchPressurePlate(owner, level, actor->getPosition(), visited);
    }
}

void RedstoneSystem::_touchPressurePlate(ServerNetworkHandler &owner, Level &level, const Vector3f &feet,
                                         std::unordered_set<int64_t> &visited)
{
    const Vector3i position((int32_t) std::floor(feet.x), (int32_t) std::floor(feet.y),
                            (int32_t) std::floor(feet.z));

    const int64_t key = packPosition(position);
    if (!visited.insert(key).second)
        return;

    if (!isChunkReady(level, position))
        return;

    const BlockState state = stateAt(level, position);
    if (!isA<PressurePlateBlock>(state))
        return;

    if (std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL) != 0)
        return;

    pressurePlateUpdateState(owner, level, position, 0);
}
