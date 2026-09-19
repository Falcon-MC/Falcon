#include "Block/Systems/RedstoneSystem.h"

#include "Block/Actor/ChestBlockActor.h"
#include "Block/BlockActorStore.h"
#include "Block/BlockData.h"
#include "Actor/PrimedTntActor.h"
#include "Block/Blocks/CommandBlock.h"
#include "Block/Blocks/TntBlock.h"
#include "Block/Systems/CommandBlockSystem.h"
#include "Block/Systems/FallingBlockSystem.h"
#include "Block/Systems/FireSystem.h"
#include "Block/Systems/PistonSystem.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/BlockStateHasher.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Packets/UpdateBlockPacket.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"

#include <algorithm>
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
    const int MAX_UPDATE_DEPTH = 512;
    const int TORCH_TICK_RATE = 2;
    const int BUTTON_HOLD_TICKS = 30;
    const int PRESSURE_PLATE_RECHECK_TICKS = 20;
    const int LIT_LAMP_TURN_OFF_DELAY = 4;
    const int OBSERVER_PULSE_TICKS = 2;
    const int COMPARATOR_DELAY = 2;
    const int DIODE_PLACE_DELAY = 1;
    const int LIGHT_WEIGHTED_MAX_WEIGHT = 15;
    const int HEAVY_WEIGHTED_MAX_WEIGHT = 150;
    const char *SOUND_POWER_ON = LevelSoundEvent::POWER_ON;
    const char *SOUND_POWER_OFF = LevelSoundEvent::POWER_OFF;

    struct ScheduledPosition {
        int32_t x;
        int32_t y;
        int32_t z;
    };

    std::map<int64_t, std::vector<ScheduledPosition>> gBuckets;
    std::unordered_map<int64_t, int64_t> gScheduled;
    std::unordered_map<int64_t, int32_t> gComparatorOutputs;
    std::unordered_set<int64_t> gManualOverrides;
    std::vector<Vector3i> gPendingNotifications;
    int64_t gTick = 0;
    int gDepth = 0;

    bool isTrappedChest(const std::string &identifier)
    {
        return identifier == "minecraft:trapped_chest";
    }

    int trappedChestSignal(const Vector3i &position)
    {
        const ChestBlockActor *chest = BlockActorStore::getInstance().find<ChestBlockActor>(position);
        if (chest == nullptr)
            return 0;

        return std::min(chest->getViewerCount(), RedstoneSystem::MAX_SIGNAL);
    }

    bool endsWith(const std::string &value, const std::string &suffix)
    {
        if (value.size() < suffix.size())
            return false;

        return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    bool isWire(const std::string &identifier)
    {
        return identifier == "minecraft:redstone_wire";
    }

    bool isLitTorch(const std::string &identifier)
    {
        return identifier == "minecraft:redstone_torch";
    }

    bool isUnlitTorch(const std::string &identifier)
    {
        return identifier == "minecraft:unlit_redstone_torch";
    }

    bool isRedstoneBlock(const std::string &identifier)
    {
        return identifier == "minecraft:redstone_block";
    }

    bool isLever(const std::string &identifier)
    {
        return identifier == "minecraft:lever";
    }

    bool isButton(const std::string &identifier)
    {
        return endsWith(identifier, "_button");
    }

    bool isPressurePlate(const std::string &identifier)
    {
        return endsWith(identifier, "_pressure_plate");
    }

    bool isRepeater(const std::string &identifier)
    {
        return identifier == "minecraft:unpowered_repeater" || identifier == "minecraft:powered_repeater";
    }

    bool isComparator(const std::string &identifier)
    {
        return identifier == "minecraft:unpowered_comparator" || identifier == "minecraft:powered_comparator";
    }

    bool isDiode(const std::string &identifier)
    {
        return isRepeater(identifier) || isComparator(identifier);
    }

    bool isObserver(const std::string &identifier)
    {
        return identifier == "minecraft:observer";
    }

    bool isLamp(const std::string &identifier)
    {
        return identifier == "minecraft:redstone_lamp";
    }

    bool isLitLamp(const std::string &identifier)
    {
        return identifier == "minecraft:lit_redstone_lamp";
    }

    bool isDaylightDetector(const std::string &identifier)
    {
        return identifier == "minecraft:daylight_detector"
               || identifier == "minecraft:daylight_detector_inverted";
    }

    bool isDoor(const std::string &identifier)
    {
        return endsWith(identifier, "_door");
    }

    bool isTrapdoor(const std::string &identifier)
    {
        return endsWith(identifier, "trapdoor");
    }

    bool isFenceGate(const std::string &identifier)
    {
        return endsWith(identifier, "fence_gate");
    }

    bool isChunkReady(ServerNetworkHandler &owner, const Vector3i &position)
    {
        if (position.y < LevelChunk::MIN_Y || position.y > LevelChunk::MAX_Y)
            return false;

        return owner.getLevel().isChunkResident(position.x >> 4, position.z >> 4);
    }

    BlockState stateAt(ServerNetworkHandler &owner, const Vector3i &position)
    {
        if (!isChunkReady(owner, position))
            return BlockState("minecraft:air");

        return owner.getLevel().getBlockState(position.x, position.y, position.z);
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
        if (state.mName == "minecraft:powered_repeater")
            return true;

        if (state.mName == "minecraft:powered_comparator")
            return true;

        if (isComparator(state.mName))
            return stateBool(state, "output_lit_bit", false);

        return false;
    }

    int diodeFacing(const BlockState &state)
    {
        return cardinalFacing(state);
    }

    int diodeDelay(const BlockState &state)
    {
        if (isComparator(state.mName))
            return COMPARATOR_DELAY;

        return (1 + stateInt(state, "repeater_delay", 0)) * 2;
    }

    BlockState withName(const BlockState &state, const std::string &name)
    {
        return BlockState(name, state.mStates);
    }

    BlockState diodePoweredState(const BlockState &state)
    {
        if (isComparator(state.mName))
            return withName(state, "minecraft:powered_comparator");

        return withName(state, "minecraft:powered_repeater");
    }

    BlockState diodeUnpoweredState(const BlockState &state)
    {
        if (isComparator(state.mName))
            return withName(state, "minecraft:unpowered_comparator");

        return withName(state, "minecraft:unpowered_repeater");
    }

    Vector3f centerOf(const Vector3i &position)
    {
        return Vector3f((float) position.x + 0.5f, (float) position.y + 0.5f, (float) position.z + 0.5f);
    }

    int wireSignal(const BlockState &state)
    {
        return std::clamp(stateInt(state, "redstone_signal", 0), 0, RedstoneSystem::MAX_SIGNAL);
    }

    bool canConnectTo(ServerNetworkHandler &owner, const Vector3i &position, int side)
    {
        const BlockState state = stateAt(owner, position);
        if (isWire(state.mName))
            return true;

        if (isDiode(state.mName)) {
            const int facing = diodeFacing(state);
            return facing == side || RedstoneFace::opposite(facing) == side;
        }

        return RedstoneSystem::isPowerSource(state) && side != RedstoneFace::NONE;
    }

    bool canConnectUpwardsTo(ServerNetworkHandler &owner, const Vector3i &position)
    {
        return canConnectTo(owner, position, RedstoneFace::NONE);
    }

    bool wireIsPowerSourceAt(ServerNetworkHandler &owner, const Vector3i &position, int side)
    {
        const Vector3i sidePosition = RedstoneFace::relative(position, side);
        const BlockState sideState = stateAt(owner, sidePosition);
        const bool sideIsNormal = RedstoneSystem::isNormalBlock(sideState);
        const bool aboveIsNormal = RedstoneSystem::isNormalBlock(
                stateAt(owner, RedstoneFace::relative(position, RedstoneFace::UP)));

        if (!aboveIsNormal && sideIsNormal
            && canConnectUpwardsTo(owner, RedstoneFace::relative(sidePosition, RedstoneFace::UP)))
            return true;

        if (canConnectTo(owner, sidePosition, side))
            return true;

        return !sideIsNormal
               && canConnectUpwardsTo(owner, RedstoneFace::relative(sidePosition, RedstoneFace::DOWN));
    }

    int wireStrongPowerAt(ServerNetworkHandler &owner, const Vector3i &position, int direction)
    {
        const BlockState state = stateAt(owner, position);
        if (isWire(state.mName))
            return 0;

        return RedstoneSystem::getStrongPower(owner, position, direction);
    }

    int wireStrongPowerAround(ServerNetworkHandler &owner, const Vector3i &position)
    {
        int result = 0;

        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            result = std::max(result, wireStrongPowerAt(owner, RedstoneFace::relative(position, face), face));

            if (result >= RedstoneSystem::MAX_SIGNAL)
                return result;
        }

        return result;
    }

    int wireIndirectPowerAt(ServerNetworkHandler &owner, const Vector3i &position, int face)
    {
        const BlockState state = stateAt(owner, position);
        if (isWire(state.mName))
            return 0;

        if (RedstoneSystem::isNormalBlock(state))
            return wireStrongPowerAround(owner, position);

        return RedstoneSystem::getWeakPower(owner, position, face);
    }

    int wireIndirectPower(ServerNetworkHandler &owner, const Vector3i &position)
    {
        int power = 0;

        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            const int blockPower = wireIndirectPowerAt(owner, RedstoneFace::relative(position, face), face);

            if (blockPower >= RedstoneSystem::MAX_SIGNAL)
                return RedstoneSystem::MAX_SIGNAL;

            if (blockPower > power)
                power = blockPower;
        }

        return power;
    }

    bool isTopSlab(const BlockState &state)
    {
        if (!endsWith(state.mName, "_slab"))
            return false;

        return stateString(state, "minecraft:vertical_half", "bottom") == "top";
    }

    int maxCurrentStrength(ServerNetworkHandler &owner, const Vector3i &position, int maxStrength)
    {
        const BlockState state = stateAt(owner, position);
        if (!isWire(state.mName))
            return maxStrength;

        return std::max(wireSignal(state), maxStrength);
    }

    void updateSurroundingRedstone(ServerNetworkHandler &owner, const Vector3i &position, bool force)
    {
        const BlockState state = stateAt(owner, position);
        if (!isWire(state.mName))
            return;

        const int meta = wireSignal(state);
        int maxStrength = meta;
        const int power = wireIndirectPower(owner, position);

        if (power > 0 && power > maxStrength - 1)
            maxStrength = power;

        int strength = 0;

        for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
            const Vector3i adjacent = RedstoneFace::relative(position, face);

            strength = maxCurrentStrength(owner, adjacent, strength);

            const Vector3i above = RedstoneFace::relative(position, RedstoneFace::UP);
            const Vector3i adjacentAbove = RedstoneFace::relative(adjacent, RedstoneFace::UP);
            if (maxCurrentStrength(owner, adjacentAbove, strength) > strength
                && !RedstoneSystem::isNormalBlock(stateAt(owner, above))
                && !isTopSlab(stateAt(owner, adjacent)))
                strength = maxCurrentStrength(owner, adjacentAbove, strength);

            const Vector3i adjacentBelow = RedstoneFace::relative(adjacent, RedstoneFace::DOWN);
            if (maxCurrentStrength(owner, adjacentBelow, strength) > strength
                && !RedstoneSystem::isNormalBlock(stateAt(owner, adjacent)))
                strength = maxCurrentStrength(owner, adjacentBelow, strength);
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

        if (meta != maxStrength) {
            Tag states = state.mStates;
            states.putInt("redstone_signal", maxStrength);
            RedstoneSystem::setBlockState(owner, position, BlockState(state.mName, states));

            RedstoneSystem::updateAllAroundRedstone(owner, position);
            return;
        }

        if (!force)
            return;

        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            RedstoneSystem::updateAroundRedstone(owner, RedstoneFace::relative(position, face),
                                                 RedstoneFace::opposite(face));
        }
    }

    void wireUpdateAround(ServerNetworkHandler &owner, const Vector3i &position, int face)
    {
        const BlockState state = stateAt(owner, position);
        if (!isWire(state.mName))
            return;

        RedstoneSystem::updateAroundRedstone(owner, position, face);

        for (int side = 0; side < RedstoneFace::COUNT; ++side) {
            RedstoneSystem::updateAroundRedstone(owner, RedstoneFace::relative(position, side),
                                                 RedstoneFace::opposite(side));
        }
    }

    bool isTorchPoweredFromSide(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        const int face = RedstoneFace::opposite(torchFacing(state));
        return RedstoneSystem::isSidePowered(owner, RedstoneFace::relative(position, face), face);
    }

    int diodeCalculateInputStrength(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        const int face = diodeFacing(state);
        const Vector3i front = RedstoneFace::relative(position, face);
        const int power = RedstoneSystem::getRedstonePower(owner, front, face);

        if (power >= RedstoneSystem::MAX_SIGNAL)
            return power;

        const BlockState frontState = stateAt(owner, front);
        return std::max(power, isWire(frontState.mName) ? wireSignal(frontState) : 0);
    }

    int comparatorCalculateInputStrength(ServerNetworkHandler &owner, const Vector3i &position,
                                         const BlockState &state)
    {
        return diodeCalculateInputStrength(owner, position, state);
    }

    int diodePowerOnSide(ServerNetworkHandler &owner, const Vector3i &position, int side, bool repeater)
    {
        const BlockState state = stateAt(owner, position);
        const bool alternate = repeater ? isDiode(state.mName) : RedstoneSystem::isPowerSource(state);
        if (!alternate)
            return 0;

        if (isRedstoneBlock(state.mName))
            return RedstoneSystem::MAX_SIGNAL;

        if (isWire(state.mName))
            return wireSignal(state);

        return RedstoneSystem::getStrongPower(owner, position, side);
    }

    int diodePowerOnSides(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        const bool repeater = isRepeater(state.mName);
        const int face = diodeFacing(state);
        const int left = RedstoneFace::rotateY(face);
        const int right = RedstoneFace::rotateYCounterClockwise(face);

        return std::max(diodePowerOnSide(owner, RedstoneFace::relative(position, left), left, repeater),
                        diodePowerOnSide(owner, RedstoneFace::relative(position, right), right, repeater));
    }

    bool diodeShouldBePowered(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        if (!isComparator(state.mName))
            return diodeCalculateInputStrength(owner, position, state) > 0;

        const int input = comparatorCalculateInputStrength(owner, position, state);
        if (input >= RedstoneSystem::MAX_SIGNAL)
            return true;

        if (input == 0)
            return false;

        const int sidePower = diodePowerOnSides(owner, position, state);
        return sidePower == 0 || input >= sidePower;
    }

    bool diodeIsLocked(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        if (!isRepeater(state.mName))
            return false;

        return diodePowerOnSides(owner, position, state) > 0;
    }

    int comparatorCalculateOutput(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        const int input = comparatorCalculateInputStrength(owner, position, state);
        if (!stateBool(state, "output_subtract_bit", false))
            return input;

        return std::max(input - diodePowerOnSides(owner, position, state), 0);
    }

    void diodeUpdateState(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        if (isComparator(state.mName)) {
            if (RedstoneSystem::isUpdateScheduled(position))
                return;

            const int output = comparatorCalculateOutput(owner, position, state);
            const int power = RedstoneSystem::getComparatorOutput(position);

            if (output != power || isDiodePowered(state) != diodeShouldBePowered(owner, position, state))
                RedstoneSystem::scheduleUpdate(owner, position, COMPARATOR_DELAY);

            return;
        }

        if (diodeIsLocked(owner, position, state))
            return;

        const bool shouldBePowered = diodeShouldBePowered(owner, position, state);
        if (isDiodePowered(state) != shouldBePowered)
            RedstoneSystem::scheduleUpdate(owner, position, diodeDelay(state));
    }

    void comparatorOnChange(ServerNetworkHandler &owner, const Vector3i &position)
    {
        const BlockState state = stateAt(owner, position);
        if (!isComparator(state.mName))
            return;

        const int output = comparatorCalculateOutput(owner, position, state);
        const int currentOutput = RedstoneSystem::getComparatorOutput(position);
        RedstoneSystem::setComparatorOutput(position, output);

        const bool subtractMode = stateBool(state, "output_subtract_bit", false);
        if (currentOutput == output && subtractMode)
            return;

        const bool shouldBePowered = diodeShouldBePowered(owner, position, state);
        const bool powered = isDiodePowered(state);

        if (powered && !shouldBePowered) {
            RedstoneSystem::setBlockState(owner, position, diodeUnpoweredState(state));
            RedstoneSystem::updateComparatorOutputLevel(owner, position, true);
        } else if (!powered && shouldBePowered) {
            RedstoneSystem::setBlockState(owner, position, diodePoweredState(state));
            RedstoneSystem::updateComparatorOutputLevel(owner, position, true);
        }

        const Vector3i behind = RedstoneFace::relative(position, RedstoneFace::opposite(diodeFacing(state)));
        RedstoneSystem::onUpdate(owner, behind, RedstoneUpdateType::Redstone);
        RedstoneSystem::updateAroundRedstone(owner, behind);
    }

    int pressurePlateComputeStrength(ServerNetworkHandler &owner, const Vector3i &position,
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
            if (!player.isSpawned() || player.isDead())
                continue;

            const Vector3f &feet = player.getPosition();
            if (feet.x >= minX && feet.x <= maxX && feet.y >= minY && feet.y <= maxY
                && feet.z >= minZ && feet.z <= maxZ)
                ++count;
        }

        for (auto &entry: owner.getActors()) {
            ServerActor *actor = entry.second.get();
            if (actor == nullptr || actor->isDead())
                continue;

            const Vector3f &feet = actor->getPosition();
            if (feet.x >= minX && feet.x <= maxX && feet.y >= minY && feet.y <= maxY
                && feet.z >= minZ && feet.z <= maxZ)
                ++count;
        }

        if (count == 0)
            return 0;

        if (state.mName == "minecraft:light_weighted_pressure_plate") {
            const int weight = std::min(count, LIGHT_WEIGHTED_MAX_WEIGHT);
            const float ratio = (float) weight / (float) LIGHT_WEIGHTED_MAX_WEIGHT;
            return (int) std::ceil(ratio * 15.0f);
        }

        if (state.mName == "minecraft:heavy_weighted_pressure_plate") {
            const int weight = std::min(count, HEAVY_WEIGHTED_MAX_WEIGHT);
            const float ratio = (float) weight / (float) HEAVY_WEIGHTED_MAX_WEIGHT;
            return std::max(1, (int) std::ceil(ratio * 15.0f));
        }

        return RedstoneSystem::MAX_SIGNAL;
    }

    void pressurePlateUpdateState(ServerNetworkHandler &owner, const Vector3i &position, int oldStrength)
    {
        const BlockState state = stateAt(owner, position);
        if (!isPressurePlate(state.mName))
            return;

        const int strength = pressurePlateComputeStrength(owner, position, state);
        const bool wasPowered = oldStrength > 0;
        const bool powered = strength > 0;

        if (oldStrength != strength) {
            Tag states = state.mStates;
            states.putInt("redstone_signal", strength);
            const BlockState updated = BlockState(state.mName, states);
            RedstoneSystem::setBlockState(owner, position, updated);

            RedstoneSystem::updateAroundRedstone(owner, position);
            RedstoneSystem::updateAroundRedstone(owner,
                                                 RedstoneFace::relative(position, RedstoneFace::DOWN));

            const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.1f,
                                  (float) position.z + 0.5f);
            if (!powered && wasPowered)
                owner.playLevelSound(SOUND_POWER_OFF, center, "", updated.getHash());
            else if (powered && !wasPowered)
                owner.playLevelSound(SOUND_POWER_ON, center, "", updated.getHash());
        }

        if (powered)
            RedstoneSystem::scheduleUpdate(owner, position, PRESSURE_PLATE_RECHECK_TICKS);
    }

    bool doorIsOpen(const BlockState &state)
    {
        return stateBool(state, "open_bit", false);
    }

    Vector3i doorLowerPosition(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        if (!isDoor(state.mName))
            return position;

        if (!stateBool(state, "upper_block_bit", false))
            return position;

        const Vector3i below = RedstoneFace::relative(position, RedstoneFace::DOWN);
        return stateAt(owner, below).mName == state.mName ? below : position;
    }

    bool doorIsGettingPower(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        if (!isDoor(state.mName))
            return RedstoneSystem::isGettingPower(owner, position);

        const Vector3i lower = doorLowerPosition(owner, position, state);
        const Vector3i upper = RedstoneFace::relative(lower, RedstoneFace::UP);

        return RedstoneSystem::isGettingPower(owner, lower) || RedstoneSystem::isGettingPower(owner, upper);
    }

    void setOpenState(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state, bool open)
    {
        Tag states = state.mStates;
        states.putByte("open_bit", open ? 1 : 0);
        RedstoneSystem::setBlockState(owner, position, BlockState(state.mName, states));

        const Vector3f center = centerOf(position);
        owner.playLevelSound(open ? SOUND_POWER_ON : SOUND_POWER_OFF, center, "", state.getHash());
    }

    void openableOnRedstoneUpdate(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        const int64_t key = RedstoneSystem::packPosition(position);
        const bool manualOverride = gManualOverrides.count(key) != 0;
        const bool gettingPower = doorIsGettingPower(owner, position, state);
        const bool open = doorIsOpen(state);

        if (open != gettingPower && !manualOverride) {
            setOpenState(owner, position, state, gettingPower);

            if (isDoor(state.mName)) {
                const Vector3i lower = doorLowerPosition(owner, position, state);
                const Vector3i upper = RedstoneFace::relative(lower, RedstoneFace::UP);
                const Vector3i other = lower == position ? upper : lower;
                const BlockState otherState = stateAt(owner, other);
                if (otherState.mName == state.mName && doorIsOpen(otherState) != gettingPower)
                    setOpenState(owner, other, otherState, gettingPower);
            }
            return;
        }

        if (manualOverride && gettingPower == open)
            gManualOverrides.erase(key);
    }

    void observerOnScheduled(ServerNetworkHandler &owner, const Vector3i &position, const BlockState &state)
    {
        const int facing = observerFacing(state);
        const Vector3i behind = RedstoneFace::relative(position, RedstoneFace::opposite(facing));

        Tag states = state.mStates;

        if (!stateBool(state, "powered_bit", false)) {
            states.putByte("powered_bit", 1);
            RedstoneSystem::setBlockState(owner, position, BlockState(state.mName, states));

            RedstoneSystem::onUpdate(owner, behind, RedstoneUpdateType::Redstone);
            RedstoneSystem::updateAroundRedstone(owner, behind);
            RedstoneSystem::scheduleUpdate(owner, position, OBSERVER_PULSE_TICKS);
            return;
        }

        states.putByte("powered_bit", 0);
        RedstoneSystem::setBlockState(owner, position, BlockState(state.mName, states));

        RedstoneSystem::onUpdate(owner, behind, RedstoneUpdateType::Redstone);
        RedstoneSystem::updateAroundRedstone(owner, behind);
    }

    void observerOnNeighborChange(ServerNetworkHandler &owner, const Vector3i &position, int side)
    {
        const BlockState state = stateAt(owner, position);
        if (!isObserver(state.mName))
            return;

        if (side != observerFacing(state) || RedstoneSystem::isUpdateScheduled(position))
            return;

        RedstoneSystem::cancelScheduledUpdate(position);
        RedstoneSystem::scheduleUpdate(owner, position, OBSERVER_PULSE_TICKS);
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
    const std::string &identifier = state.mName;

    if (isWire(identifier))
        return wireSignal(state) > 0;

    if (isRedstoneBlock(identifier) || isLitTorch(identifier) || isLever(identifier)
        || isButton(identifier) || isPressurePlate(identifier) || isObserver(identifier)
        || isDiode(identifier) || isDaylightDetector(identifier) || isTrappedChest(identifier))
        return true;

    return false;
}

int RedstoneSystem::getWeakPower(ServerNetworkHandler &owner, const Vector3i &position, int face)
{
    const BlockState state = stateAt(owner, position);
    const std::string &identifier = state.mName;

    if (isRedstoneBlock(identifier))
        return MAX_SIGNAL;

    if (isLitTorch(identifier))
        return torchFacing(state) != face ? MAX_SIGNAL : 0;

    if (isUnlitTorch(identifier))
        return 0;

    if (isLever(identifier))
        return stateBool(state, "open_bit", false) ? MAX_SIGNAL : 0;

    if (isButton(identifier))
        return stateBool(state, "button_pressed_bit", false) ? MAX_SIGNAL : 0;

    if (isPressurePlate(identifier) || isDaylightDetector(identifier))
        return std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL);

    if (isTrappedChest(identifier))
        return trappedChestSignal(position);

    if (isObserver(identifier))
        return getStrongPower(owner, position, face);

    if (isDiode(identifier)) {
        if (!isDiodePowered(state))
            return 0;

        if (diodeFacing(state) != face)
            return 0;

        return isComparator(identifier) ? getComparatorOutput(position) : MAX_SIGNAL;
    }

    if (!isWire(identifier))
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
        if (wireIsPowerSourceAt(owner, position, side)) {
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

int RedstoneSystem::getStrongPower(ServerNetworkHandler &owner, const Vector3i &position, int face)
{
    const BlockState state = stateAt(owner, position);
    const std::string &identifier = state.mName;

    if (isLitTorch(identifier))
        return face == RedstoneFace::DOWN ? getWeakPower(owner, position, face) : 0;

    if (isUnlitTorch(identifier) || isRedstoneBlock(identifier))
        return 0;

    if (isLever(identifier)) {
        if (!stateBool(state, "open_bit", false))
            return 0;

        return leverFacing(state) == face ? MAX_SIGNAL : 0;
    }

    if (isButton(identifier)) {
        if (!stateBool(state, "button_pressed_bit", false))
            return 0;

        return buttonFacing(state) == face ? MAX_SIGNAL : 0;
    }

    if (isPressurePlate(identifier))
        return face == RedstoneFace::UP ? std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL) : 0;

    if (isTrappedChest(identifier))
        return face == RedstoneFace::UP ? trappedChestSignal(position) : 0;

    if (isObserver(identifier)) {
        return stateBool(state, "powered_bit", false) && face == observerFacing(state) ? MAX_SIGNAL : 0;
    }

    if (isDiode(identifier))
        return getWeakPower(owner, position, face);

    if (isWire(identifier))
        return isPowerSource(state) ? getWeakPower(owner, position, face) : 0;

    return 0;
}

int RedstoneSystem::getStrongPowerAround(ServerNetworkHandler &owner, const Vector3i &position)
{
    int result = 0;

    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        result = std::max(result, getStrongPower(owner, RedstoneFace::relative(position, face), face));

        if (result >= MAX_SIGNAL)
            return result;
    }

    return result;
}

int RedstoneSystem::getRedstonePower(ServerNetworkHandler &owner, const Vector3i &position, int face)
{
    const BlockState state = stateAt(owner, position);
    return isNormalBlock(state) ? getStrongPowerAround(owner, position) : getWeakPower(owner, position, face);
}

bool RedstoneSystem::isSidePowered(ServerNetworkHandler &owner, const Vector3i &position, int face)
{
    return getRedstonePower(owner, position, face) > 0;
}

bool RedstoneSystem::isBlockPowered(ServerNetworkHandler &owner, const Vector3i &position)
{
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (getRedstonePower(owner, RedstoneFace::relative(position, face), face) > 0)
            return true;
    }

    return false;
}

int RedstoneSystem::isBlockIndirectlyGettingPowered(ServerNetworkHandler &owner, const Vector3i &position)
{
    int power = 0;

    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        const int blockPower = getRedstonePower(owner, RedstoneFace::relative(position, face), face);

        if (blockPower >= MAX_SIGNAL)
            return MAX_SIGNAL;

        if (blockPower > power)
            power = blockPower;
    }

    return power;
}

bool RedstoneSystem::isGettingPower(ServerNetworkHandler &owner, const Vector3i &position)
{
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (isSidePowered(owner, RedstoneFace::relative(position, face), face))
            return true;
    }

    return isBlockPowered(owner, position);
}

void RedstoneSystem::updateAroundRedstone(ServerNetworkHandler &owner, const Vector3i &position, int ignoredFace)
{
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (face == ignoredFace)
            continue;

        onUpdate(owner, RedstoneFace::relative(position, face), RedstoneUpdateType::Redstone);
    }
}

void RedstoneSystem::updateAllAroundRedstone(ServerNetworkHandler &owner, const Vector3i &position,
                                             int ignoredFace)
{
    updateAroundRedstone(owner, position, ignoredFace);

    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        if (face == ignoredFace)
            continue;

        updateAroundRedstone(owner, RedstoneFace::relative(position, face), RedstoneFace::opposite(face));
    }
}

void RedstoneSystem::updateAroundNormal(ServerNetworkHandler &owner, const Vector3i &position)
{
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        onUpdate(owner, RedstoneFace::relative(position, face), RedstoneUpdateType::Normal);
    }
}

void RedstoneSystem::updateComparatorOutputLevel(ServerNetworkHandler &owner, const Vector3i &position,
                                                 bool observer)
{
    for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
        const Vector3i side = RedstoneFace::relative(position, face);
        if (!isChunkReady(owner, side))
            continue;

        const BlockState sideState = stateAt(owner, side);

        if (isObserver(sideState.mName)) {
            if (observer)
                observerOnNeighborChange(owner, side, RedstoneFace::opposite(face));
            continue;
        }

        if (isDiode(sideState.mName)) {
            onUpdate(owner, side, RedstoneUpdateType::Redstone);
            continue;
        }

        if (!isNormalBlock(sideState))
            continue;

        const Vector3i beyond = RedstoneFace::relative(side, face);
        if (isDiode(stateAt(owner, beyond).mName))
            onUpdate(owner, beyond, RedstoneUpdateType::Redstone);
    }

    if (!observer)
        return;

    for (int face = RedstoneFace::DOWN; face <= RedstoneFace::UP; ++face) {
        const Vector3i side = RedstoneFace::relative(position, face);
        if (isObserver(stateAt(owner, side).mName))
            observerOnNeighborChange(owner, side, RedstoneFace::opposite(face));
    }
}

void RedstoneSystem::onUpdate(ServerNetworkHandler &owner, const Vector3i &position, RedstoneUpdateType type)
{
    if (!isChunkReady(owner, position))
        return;

    if (gDepth >= MAX_UPDATE_DEPTH) {
        if (type != RedstoneUpdateType::Scheduled)
            scheduleUpdate(owner, position, 1);
        return;
    }

    ++gDepth;

    const BlockState state = stateAt(owner, position);
    const std::string identifier = state.mName;

    if (isWire(identifier)) {
        if (type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone)
            updateSurroundingRedstone(owner, position, false);
    } else if (isLitTorch(identifier)) {
        if (type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone) {
            scheduleUpdate(owner, position, TORCH_TICK_RATE);
        } else if (type == RedstoneUpdateType::Scheduled && isTorchPoweredFromSide(owner, position, state)) {
            setBlockState(owner, position, withName(state, "minecraft:unlit_redstone_torch"));
            updateAllAroundRedstone(owner, position, RedstoneFace::opposite(torchFacing(state)));
        }
    } else if (isUnlitTorch(identifier)) {
        if (type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone) {
            scheduleUpdate(owner, position, TORCH_TICK_RATE);
        } else if (type == RedstoneUpdateType::Scheduled && !isTorchPoweredFromSide(owner, position, state)) {
            setBlockState(owner, position, withName(state, "minecraft:redstone_torch"));
            updateAllAroundRedstone(owner, position, RedstoneFace::opposite(torchFacing(state)));
        }
    } else if (isComparator(identifier)) {
        if (type == RedstoneUpdateType::Scheduled)
            comparatorOnChange(owner, position);
        else if (type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone)
            diodeUpdateState(owner, position, state);
    } else if (isRepeater(identifier)) {
        if (type == RedstoneUpdateType::Scheduled) {
            if (!diodeIsLocked(owner, position, state)) {
                const bool shouldBePowered = diodeShouldBePowered(owner, position, state);
                const bool powered = isDiodePowered(state);
                bool changed = false;

                if (powered && !shouldBePowered) {
                    setBlockState(owner, position, diodeUnpoweredState(state));
                    changed = true;
                } else if (!powered) {
                    setBlockState(owner, position, diodePoweredState(state));
                    changed = true;
                }

                if (changed) {
                    const Vector3i behind = RedstoneFace::relative(position,
                                                                   RedstoneFace::opposite(diodeFacing(state)));
                    onUpdate(owner, behind, RedstoneUpdateType::Redstone);
                    updateAroundRedstone(owner, behind);
                }
            }
        } else if (type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone) {
            diodeUpdateState(owner, position, state);
        }
    } else if (isLamp(identifier)) {
        if ((type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone)
            && isGettingPower(owner, position)) {
            updateComparatorOutputLevel(owner, position, true);
            setBlockState(owner, position, withName(state, "minecraft:lit_redstone_lamp"));
        }
    } else if (isLitLamp(identifier)) {
        if ((type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone)
            && !isGettingPower(owner, position)) {
            scheduleUpdate(owner, position, LIT_LAMP_TURN_OFF_DELAY);
        } else if (type == RedstoneUpdateType::Scheduled && !isGettingPower(owner, position)) {
            updateComparatorOutputLevel(owner, position, true);
            setBlockState(owner, position, withName(state, "minecraft:redstone_lamp"));
        }
    } else if (isObserver(identifier)) {
        if (type == RedstoneUpdateType::Scheduled)
            observerOnScheduled(owner, position, state);
    } else if (isButton(identifier)) {
        if (type == RedstoneUpdateType::Scheduled && stateBool(state, "button_pressed_bit", false)) {
            Tag states = state.mStates;
            states.putByte("button_pressed_bit", 0);
            const BlockState released = BlockState(state.mName, states);
            setBlockState(owner, position, released);
            owner.playLevelSound(SOUND_POWER_OFF, centerOf(position), "", released.getHash());

            const int facing = buttonFacing(state);
            updateAroundRedstone(owner, position);
            updateAroundRedstone(owner, RedstoneFace::relative(position, RedstoneFace::opposite(facing)),
                                 facing);
        }
    } else if (isPressurePlate(identifier)) {
        if (type == RedstoneUpdateType::Scheduled) {
            const int power = std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL);
            if (power > 0)
                pressurePlateUpdateState(owner, position, power);
        }
    } else if (isDoor(identifier) || isTrapdoor(identifier) || isFenceGate(identifier)) {
        if (type == RedstoneUpdateType::Redstone)
            openableOnRedstoneUpdate(owner, position, state);
    } else if (CommandBlock::matches(identifier)) {
        if (type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone)
            CommandBlockSystem::setPowered(owner, position, isGettingPower(owner, position));
    } else if (PistonSystem::isPiston(identifier)) {
        if (type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone)
            PistonSystem::onRedstoneUpdate(owner, position, state);
    } else if (FireSystem::matches(identifier)) {
        if (type == RedstoneUpdateType::Normal)
            FireSystem::onNormalUpdate(owner, position, state);
    } else if (FallingBlockSystem::matches(identifier)) {
        if (type == RedstoneUpdateType::Normal)
            FallingBlockSystem::onNormalUpdate(owner, position, state);
    } else if (TntBlock::matches(identifier)) {
        if ((type == RedstoneUpdateType::Normal || type == RedstoneUpdateType::Redstone)
            && isGettingPower(owner, position))
            TntBlock::prime(owner, owner.getLevel(), position, PrimedTntActor::DEFAULT_FUSE);
    }

    --gDepth;
}

void RedstoneSystem::scheduleUpdate(ServerNetworkHandler &owner, const Vector3i &position, int64_t delay)
{
    (void) owner;

    if (delay < 1)
        delay = 1;

    const int64_t key = packPosition(position);
    const int64_t target = gTick + delay;

    const auto it = gScheduled.find(key);
    if (it != gScheduled.end() && it->second <= target)
        return;

    gScheduled[key] = target;

    ScheduledPosition entry;
    entry.x = position.x;
    entry.y = position.y;
    entry.z = position.z;
    gBuckets[target].push_back(entry);
}

bool RedstoneSystem::isUpdateScheduled(const Vector3i &position)
{
    return gScheduled.count(packPosition(position)) != 0;
}

void RedstoneSystem::cancelScheduledUpdate(const Vector3i &position)
{
    gScheduled.erase(packPosition(position));
}

void RedstoneSystem::setBlockState(ServerNetworkHandler &owner, const Vector3i &position,
                                   const BlockState &state)
{
    if (!isChunkReady(owner, position))
        return;

    owner.getLevel().setBlockState(position.x, position.y, position.z, state);

    UpdateBlockPacket update;
    update.mBlockPosition = position;
    update.mRuntimeId = (uint32_t) BlockStateHasher::hash(state.mName, state.mStates);
    update.mFlags = UpdateBlockPacket::Flag::All;
    update.mDataLayer = 0;

    BlockActionHandler::broadcastToViewers(owner, centerOf(position), update);
}

void RedstoneSystem::onBlockPlaced(ServerNetworkHandler &owner, const Vector3i &position,
                                   const BlockState &state)
{
    const std::string &identifier = state.mName;

    if (FallingBlockSystem::matches(identifier)) {
        updateAroundNormal(owner, position);
        updateAroundRedstone(owner, position);
        FallingBlockSystem::onBlockPlaced(owner, position, state);
        return;
    }

    if (isWire(identifier)) {
        updateSurroundingRedstone(owner, position, true);

        for (int face = RedstoneFace::DOWN; face <= RedstoneFace::UP; ++face) {
            updateAroundRedstone(owner, RedstoneFace::relative(position, face), RedstoneFace::opposite(face));
        }

        for (int face = RedstoneFace::DOWN; face <= RedstoneFace::UP; ++face) {
            wireUpdateAround(owner, RedstoneFace::relative(position, face), RedstoneFace::opposite(face));
        }

        for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
            const Vector3i side = RedstoneFace::relative(position, face);

            if (isNormalBlock(stateAt(owner, side)))
                wireUpdateAround(owner, RedstoneFace::relative(side, RedstoneFace::UP), RedstoneFace::DOWN);
            else
                wireUpdateAround(owner, RedstoneFace::relative(side, RedstoneFace::DOWN), RedstoneFace::UP);
        }
        return;
    }

    if (isLitTorch(identifier)) {
        if (isTorchPoweredFromSide(owner, position, state)) {
            setBlockState(owner, position, withName(state, "minecraft:unlit_redstone_torch"));
            updateAllAroundRedstone(owner, position, RedstoneFace::opposite(torchFacing(state)));
        } else {
            updateAllAroundRedstone(owner, position, RedstoneFace::opposite(torchFacing(state)));
        }
        return;
    }

    if (isRedstoneBlock(identifier)) {
        updateAroundRedstone(owner, position);
        return;
    }

    if (isDiode(identifier)) {
        if (isComparator(identifier))
            setComparatorOutput(position, comparatorCalculateOutput(owner, position, state));

        if (diodeShouldBePowered(owner, position, state))
            scheduleUpdate(owner, position, DIODE_PLACE_DELAY);

        updateAroundNormal(owner, position);
        updateAroundRedstone(owner, position);
        return;
    }

    if (isLamp(identifier)) {
        if (isGettingPower(owner, position))
            setBlockState(owner, position, withName(state, "minecraft:lit_redstone_lamp"));
        return;
    }

    updateAroundNormal(owner, position);
    updateAroundRedstone(owner, position);
    updateComparatorOutputLevel(owner, position, true);
}

void RedstoneSystem::onBlockBroken(ServerNetworkHandler &owner, const Vector3i &position,
                                   const BlockState &previous)
{
    const std::string &identifier = previous.mName;

    gComparatorOutputs.erase(packPosition(position));
    gManualOverrides.erase(packPosition(position));
    cancelScheduledUpdate(position);

    if (isWire(identifier)) {
        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            updateAroundRedstone(owner, RedstoneFace::relative(position, face));
        }

        for (int face = RedstoneFace::NORTH; face <= RedstoneFace::EAST; ++face) {
            const Vector3i side = RedstoneFace::relative(position, face);

            if (isNormalBlock(stateAt(owner, side)))
                wireUpdateAround(owner, RedstoneFace::relative(side, RedstoneFace::UP), RedstoneFace::DOWN);
            else
                wireUpdateAround(owner, RedstoneFace::relative(side, RedstoneFace::DOWN), RedstoneFace::UP);
        }
        return;
    }

    if (isLitTorch(identifier) || isUnlitTorch(identifier)) {
        updateAllAroundRedstone(owner, position, RedstoneFace::opposite(torchFacing(previous)));
        return;
    }

    if (isDiode(identifier)) {
        updateAllAroundRedstone(owner, position);
        return;
    }

    if (isLever(identifier)) {
        if (stateBool(previous, "open_bit", false)) {
            const int facing = leverFacing(previous);
            updateAroundNormal(owner, RedstoneFace::relative(position, RedstoneFace::opposite(facing)));
            updateAroundRedstone(owner, position);
            updateAroundRedstone(owner, RedstoneFace::relative(position, RedstoneFace::opposite(facing)),
                                 facing);
        }
        return;
    }

    if (isButton(identifier)) {
        if (stateBool(previous, "button_pressed_bit", false)) {
            const int facing = buttonFacing(previous);
            updateAroundNormal(owner, RedstoneFace::relative(position, RedstoneFace::opposite(facing)));
        }
        updateAroundRedstone(owner, position);
        return;
    }

    if (isPressurePlate(identifier)) {
        if (std::clamp(stateInt(previous, "redstone_signal", 0), 0, MAX_SIGNAL) > 0) {
            updateAroundRedstone(owner, position);
            updateAroundRedstone(owner, RedstoneFace::relative(position, RedstoneFace::DOWN));
        }
        return;
    }

    updateAroundNormal(owner, position);
    updateAroundRedstone(owner, position);
    updateComparatorOutputLevel(owner, position, true);
}

void RedstoneSystem::onLeverActivated(ServerNetworkHandler &owner, const Vector3i &position,
                                      const BlockState &state)
{
    const bool powered = !stateBool(state, "open_bit", false);

    Tag states = state.mStates;
    states.putByte("open_bit", powered ? 1 : 0);
    const BlockState toggled = BlockState(state.mName, states);
    setBlockState(owner, position, toggled);

    owner.playLevelSound(powered ? SOUND_POWER_ON : SOUND_POWER_OFF, centerOf(position), "",
                         toggled.getHash());

    const int facing = leverFacing(toggled);
    updateAroundRedstone(owner, position);
    updateAroundRedstone(owner, RedstoneFace::relative(position, RedstoneFace::opposite(facing)), facing);
}

void RedstoneSystem::onButtonActivated(ServerNetworkHandler &owner, const Vector3i &position,
                                       const BlockState &state)
{
    if (stateBool(state, "button_pressed_bit", false))
        return;

    scheduleUpdate(owner, position, BUTTON_HOLD_TICKS);

    Tag states = state.mStates;
    states.putByte("button_pressed_bit", 1);
    const BlockState pressed = BlockState(state.mName, states);
    setBlockState(owner, position, pressed);

    owner.playLevelSound(SOUND_POWER_ON, centerOf(position), "", pressed.getHash());

    const int facing = buttonFacing(pressed);
    updateAroundRedstone(owner, position);
    updateAroundRedstone(owner, RedstoneFace::relative(position, RedstoneFace::opposite(facing)), facing);
}

void RedstoneSystem::onRepeaterActivated(ServerNetworkHandler &owner, const Vector3i &position,
                                         const BlockState &state)
{
    const int delay = stateInt(state, "repeater_delay", 0);

    Tag states = state.mStates;
    states.putInt("repeater_delay", delay == 3 ? 0 : delay + 1);
    setBlockState(owner, position, BlockState(state.mName, states));
}

void RedstoneSystem::onComparatorActivated(ServerNetworkHandler &owner, const Vector3i &position,
                                           const BlockState &state)
{
    Tag states = state.mStates;
    states.putByte("output_subtract_bit", stateBool(state, "output_subtract_bit", false) ? 0 : 1);
    setBlockState(owner, position, BlockState(state.mName, states));

    updateComparatorOutputLevel(owner, position, true);
    comparatorOnChange(owner, position);
}

int RedstoneSystem::getComparatorOutput(const Vector3i &position)
{
    const auto it = gComparatorOutputs.find(packPosition(position));
    if (it == gComparatorOutputs.end())
        return 0;

    return it->second;
}

void RedstoneSystem::setComparatorOutput(const Vector3i &position, int output)
{
    gComparatorOutputs[packPosition(position)] = std::clamp(output, 0, MAX_SIGNAL);
}

void RedstoneSystem::queueRedstoneNotification(const Vector3i &position)
{
    gPendingNotifications.push_back(position);
}

void RedstoneSystem::tick(ServerNetworkHandler &owner)
{
    ++gTick;

    if (!gPendingNotifications.empty()) {
        const std::vector<Vector3i> notifications = std::move(gPendingNotifications);
        gPendingNotifications.clear();

        for (const Vector3i &position: notifications)
            updateAroundRedstone(owner, position);
    }

    std::vector<ScheduledPosition> due;

    while (!gBuckets.empty()) {
        const auto it = gBuckets.begin();
        if (it->first > gTick)
            break;

        for (const ScheduledPosition &entry: it->second)
            due.push_back(entry);

        gBuckets.erase(it);
    }

    for (const ScheduledPosition &entry: due) {
        const Vector3i position(entry.x, entry.y, entry.z);
        const int64_t key = packPosition(position);

        const auto scheduledIt = gScheduled.find(key);
        if (scheduledIt == gScheduled.end() || scheduledIt->second > gTick)
            continue;

        gScheduled.erase(scheduledIt);
        onUpdate(owner, position, RedstoneUpdateType::Scheduled);
    }

    std::unordered_set<int64_t> visited;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead())
            continue;

        _touchPressurePlate(owner, player.getPosition(), visited);
    }

    for (auto &entry: owner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor == nullptr || actor->isDead())
            continue;

        _touchPressurePlate(owner, actor->getPosition(), visited);
    }
}

void RedstoneSystem::_touchPressurePlate(ServerNetworkHandler &owner, const Vector3f &feet,
                                         std::unordered_set<int64_t> &visited)
{
    const Vector3i position((int32_t) std::floor(feet.x), (int32_t) std::floor(feet.y),
                            (int32_t) std::floor(feet.z));

    const int64_t key = packPosition(position);
    if (!visited.insert(key).second)
        return;

    if (!isChunkReady(owner, position))
        return;

    const BlockState state = stateAt(owner, position);
    if (!isPressurePlate(state.mName))
        return;

    if (std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL) != 0)
        return;

    pressurePlateUpdateState(owner, position, 0);
}

size_t RedstoneSystem::getScheduledCount()
{
    return gScheduled.size();
}
