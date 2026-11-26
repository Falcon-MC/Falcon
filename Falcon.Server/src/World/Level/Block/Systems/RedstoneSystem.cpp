#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/Circuit/CircuitSystem.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"
#include "World/Level/Block/DetectorRailBlock.h"
#include "World/Level/Block/LightningRodBlock.h"
#include "World/Level/Block/SculkSensorBlock.h"
#include "World/Level/Block/BlockData.h"
#include "World/Level/Chunk/SubChunk.h"
#include "World/Level/Block/Systems/DetectorSystem.h"
#include "World/Level/Chunk/LevelChunk.h"

#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Actor/Misc/PrimedTntActor.h"
#include "World/Level/Block/ButtonBlock.h"
#include "World/Level/Block/CommandBlock.h"
#include "World/Level/Block/DaylightDetectorBlock.h"
#include "World/Level/Block/DispenserBlock.h"
#include "World/Level/Block/DoorBlock.h"
#include "World/Level/Block/LeverBlock.h"
#include "World/Level/Block/OpenableBlock.h"
#include "World/Level/Block/DoorOrientationBlock.h"
#include "World/Level/Block/FenceGateBlock.h"
#include "World/Level/Block/TrapDoorBlock.h"
#include "World/Level/Block/PressurePlateBlock.h"
#include "World/Level/Block/RedStoneWireBlock.h"
#include "World/Level/Block/ObserverBlock.h"
#include "World/Level/Block/RedstoneBlock.h"
#include "World/Level/Block/CrafterBlock.h"
#include "World/Level/Block/LecternBlock.h"
#include "World/Level/Block/RedstoneComparatorBlock.h"
#include "World/Level/Block/RedstoneDiodeBlock.h"
#include "World/Level/Block/RedstoneLampBlock.h"
#include "World/Level/Block/RedstoneRepeaterBlock.h"
#include "World/Level/Block/RedstoneTorchBlock.h"
#include "World/Level/Block/TrappedChestBlock.h"
#include "World/Level/Block/TntBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/CommandBlockSystem.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Core/Math/VectorMath.h"
#include "World/Level/Block/BlockNames.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

Vector3i RedstoneFace::offset(int face) {
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

Vector3i RedstoneFace::relative(const Vector3i &position, int face) {
    const Vector3i delta = RedstoneFace::offset(face);
    return Vector3i(position.x + delta.x, position.y + delta.y, position.z + delta.z);
}

int RedstoneFace::opposite(int face) {
    if (face < 0 || face >= RedstoneFace::COUNT)
        return RedstoneFace::NONE;

    // faces in opposite pairs (down/up, north/south, west/east)
    return face ^ 1;
}

bool RedstoneFace::isHorizontal(int face) {
    return face >= RedstoneFace::NORTH && face <= RedstoneFace::EAST;
}

int RedstoneFace::rotateY(int face) {
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

int RedstoneFace::rotateYCounterClockwise(int face) {
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

const char *RedstoneFace::name(int face) {
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

int RedstoneFace::fromName(const std::string &name) {
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
    using VectorMath::centerOf;

    // a wooden button stays pressed 30 ticks, a stone one 20
    const int WOODEN_BUTTON_TICKS = 30;
    const int STONE_BUTTON_TICKS = 20;
    const int PRESSURE_PLATE_RECHECK_TICKS = 20;
    // the game's lamp: an unpowered lamp goes dark four ticks later
    const int LIT_LAMP_TURN_OFF_DELAY = 4;
    const int OBSERVER_PULSE_TICKS = 2;
    const char *const OBSERVER_NAME = "minecraft:observer";
    const char *SOUND_POWER_ON = LevelSoundEvent::POWER_ON;
    const char *SOUND_POWER_OFF = LevelSoundEvent::POWER_OFF;

    template <typename T>
    const T *blockAs(const BlockState &state) {
        return VanillaBlocks::getAs<T>(state.mName);
    }

    template <typename T>
    bool isA(const BlockState &state) {
        return blockAs<T>(state) != nullptr;
    }

    bool isChunkReady(Level &level, const Vector3i &position) {
        if (position.y < level.getMinY() || position.y > level.getMaxY())
            return false;

        return level.isChunkResident(position.x >> 4, position.z >> 4);
    }

    // unloaded reads as air: no forced chunk load
    BlockState stateAt(Level &level, const Vector3i &position) {
        if (!isChunkReady(level, position))
            return BlockState(BlockNames::AIR);

        return level.getBlockState(position.x, position.y, position.z);
    }

    int stateInt(const BlockState &state, const std::string &key, int fallback) {
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

    bool stateBool(const BlockState &state, const std::string &key, bool fallback) {
        return state.mStates.getBool(key, fallback);
    }

    std::string stateString(const BlockState &state, const std::string &key, const std::string &fallback) {
        return state.mStates.getString(key, fallback);
    }

    // the game's torch component faces its support: west, east, north, south, down for a standing torch
    uint8_t torchDirection(const BlockState &state) {
        const std::string attachment = stateString(state, "torch_facing_direction", "top");
        const int named = RedstoneFace::fromName(attachment);
        return named >= RedstoneFace::NORTH ? (uint8_t) named : CircuitFace::DOWN;
    }

    // lever_direction names where the handle points: the support is behind it
    int leverPointing(const BlockState &state) {
        const std::string direction = stateString(state, "lever_direction", "up_east_west");
        if (direction == "down_east_west" || direction == "down_north_south")
            return RedstoneFace::DOWN;
        if (direction == "up_east_west" || direction == "up_north_south")
            return RedstoneFace::UP;
        const int named = RedstoneFace::fromName(direction);
        return named == RedstoneFace::NONE ? RedstoneFace::UP : named;
    }

    int cardinalFacing(const BlockState &state) {
        const int named = RedstoneFace::fromName(stateString(state, "minecraft:cardinal_direction", "south"));
        return named == RedstoneFace::NONE ? RedstoneFace::SOUTH : named;
    }

    int observerFacing(const BlockState &state) {
        const int named = RedstoneFace::fromName(stateString(state, "minecraft:facing_direction", "down"));
        return named == RedstoneFace::NONE ? RedstoneFace::DOWN : named;
    }

    int buttonFacing(const BlockState &state) {
        return std::clamp(stateInt(state, "facing_direction", 0), 0, RedstoneFace::COUNT - 1);
    }

    int wireSignal(const BlockState &state) {
        return std::clamp(stateInt(state, "redstone_signal", 0), 0, RedstoneSystem::MAX_SIGNAL);
    }

    bool isDiodePowered(const BlockState &state) {
        const RedstoneDiodeBlock *diode = blockAs<RedstoneDiodeBlock>(state);
        return diode != nullptr && diode->isPowered(state);
    }

    int trappedChestSignal(Level &level, const Vector3i &position) {
        const ChestBlockActor *chest = level.getBlockActors().find<ChestBlockActor>(position);
        if (chest == nullptr)
            return 0;
        return std::min(chest->getViewerCount(), RedstoneSystem::MAX_SIGNAL);
    }

    class LevelCircuitSource : public CircuitBlockSource {
    public:
        explicit LevelCircuitSource(Level &level) : mLevel(level) {}

        bool isLoaded(const Vector3i &position) const override {
            return isChunkReady(mLevel, position);
        }

        bool isAir(const Vector3i &position) const override {
            const BlockState *state = mLevel.peekBlockPtr(position.x, position.y, position.z);
            return state == nullptr || state->mName == BlockNames::AIR;
        }

        RedstoneConductivity getConductivity(const Vector3i &position) const override {
            const BlockState *state = mLevel.peekBlockPtr(position.x, position.y, position.z);
            return state == nullptr ? RedstoneConductivity() : RedstoneConductivity::of(*state);
        }

    private:
        Level &mLevel;
    };

    struct RedstoneState {
        explicit RedstoneState(Level &level) : mSource(std::make_unique<LevelCircuitSource>(level)) {}

        std::unique_ptr<CircuitSystem> mCircuit = std::make_unique<CircuitSystem>();
        std::unique_ptr<LevelCircuitSource> mSource;
        std::vector<Vector3i> mPendingNotifications;
    };

    std::unordered_map<const Level *, RedstoneState> gStates;

    RedstoneState &stateOf(Level &level) {
        auto found = gStates.find(&level);
        if (found == gStates.end())
            found = gStates.emplace(&level, RedstoneState(level)).first;
        return found->second;
    }

    enum class CircuitKind {
        None, Wire, Torch, Repeater, Comparator, Observer, Producer, Consumer, Piston, Lamp, Door
    };

    // the blocks given a consumer by hooks of their own
    bool isConsumerName(const std::string &name) {
        static const std::unordered_set<std::string> names = {
                "minecraft:activator_rail",
                "minecraft:golden_rail",
                "minecraft:dispenser",
                "minecraft:dropper",
                "minecraft:noteblock",
                "minecraft:tnt",
                "minecraft:underwater_tnt",
                "minecraft:command_block",
                "minecraft:repeating_command_block",
                "minecraft:chain_command_block",
                "minecraft:bell",
                "minecraft:big_dripleaf",
                "minecraft:sculk_shrieker",
                "minecraft:crafter",
                "minecraft:skeleton_skull",
                "minecraft:wither_skeleton_skull",
                "minecraft:zombie_head",
                "minecraft:player_head",
                "minecraft:creeper_head",
                "minecraft:dragon_head",
                "minecraft:piglin_head",
                "minecraft:hopper",
                "minecraft:structure_block",
        };
        return names.count(name) != 0 || BlockIdentifier::endsWith(name, "copper_bulb")
               || BlockIdentifier::endsWith(name, "_shelf") || BlockIdentifier::endsWith(name, "shulker_box");
    }

    CircuitKind kindOf(const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        if (dynamic_cast<const RedStoneWireBlock *>(block) != nullptr)
            return CircuitKind::Wire;
        if (dynamic_cast<const RedstoneTorchBlock *>(block) != nullptr)
            return CircuitKind::Torch;
        if (dynamic_cast<const RedstoneRepeaterBlock *>(block) != nullptr)
            return CircuitKind::Repeater;
        if (dynamic_cast<const RedstoneComparatorBlock *>(block) != nullptr)
            return CircuitKind::Comparator;
        if (dynamic_cast<const ObserverBlock *>(block) != nullptr)
            return CircuitKind::Observer;
        if (dynamic_cast<const RedstoneLampBlock *>(block) != nullptr)
            return CircuitKind::Lamp;
        if (PistonSystem::isPiston(state.mName))
            return CircuitKind::Piston;
        if (dynamic_cast<const DoorOrientationBlock *>(block) != nullptr)
            return CircuitKind::Door;
        if (RedstoneSystem::isPowerSource(state) || DetectorSystem::isHook(state) || DetectorSystem::isTarget(state)
            || dynamic_cast<const RedstoneBlock *>(block) != nullptr || dynamic_cast<const LeverBlock *>(block) != nullptr
            || dynamic_cast<const ButtonBlock *>(block) != nullptr
            || dynamic_cast<const PressurePlateBlock *>(block) != nullptr
            || dynamic_cast<const DaylightDetectorBlock *>(block) != nullptr
            || dynamic_cast<const TrappedChestBlock *>(block) != nullptr
            || dynamic_cast<const LecternBlock *>(block) != nullptr)
            return CircuitKind::Producer;
        if (dynamic_cast<const TrapDoorBlock *>(block) != nullptr || dynamic_cast<const FenceGateBlock *>(block) != nullptr
            || dynamic_cast<const TntBlock *>(block) != nullptr || dynamic_cast<const DispenserBlock *>(block) != nullptr
            || dynamic_cast<const CrafterBlock *>(block) != nullptr || CommandBlock::matches(state.mName)
            || isConsumerName(state.mName))
            return CircuitKind::Consumer;
        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName);
        if (custom != nullptr && custom->mRedstoneConsumer)
            return CircuitKind::Consumer;
        return CircuitKind::None;
    }

    bool hasCircuitKind(const BlockState &state) {
        return kindOf(state) != CircuitKind::None;
    }

    // the signal a producer block gives out, the strongest of its faces
    int producerSignal(Level &level, const Vector3i &position, const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        int strength = 0;
        for (int face = 0; face < RedstoneFace::COUNT; ++face) {
            int emitted = DetectorSystem::weakPower(level, position, state, face);
            if (emitted < 0 && block != nullptr)
                emitted = block->getEmittedSignal(level, position, state, face, false);
            strength = std::max(strength, emitted);
        }
        if (dynamic_cast<const RedstoneBlock *>(block) != nullptr)
            return RedstoneSystem::MAX_SIGNAL;
        if (dynamic_cast<const LeverBlock *>(block) != nullptr)
            return stateBool(state, "open_bit", false) ? RedstoneSystem::MAX_SIGNAL : 0;
        if (dynamic_cast<const ButtonBlock *>(block) != nullptr)
            return stateBool(state, "button_pressed_bit", false) ? RedstoneSystem::MAX_SIGNAL : 0;
        if (dynamic_cast<const LecternBlock *>(block) != nullptr)
            return stateBool(state, "powered_bit", false) ? RedstoneSystem::MAX_SIGNAL : 0;
        if (dynamic_cast<const PressurePlateBlock *>(block) != nullptr
            || dynamic_cast<const DaylightDetectorBlock *>(block) != nullptr)
            return std::clamp(stateInt(state, "redstone_signal", 0), 0, RedstoneSystem::MAX_SIGNAL);
        if (dynamic_cast<const TrappedChestBlock *>(block) != nullptr)
            return trappedChestSignal(level, position);
        return std::clamp(strength, 0, RedstoneSystem::MAX_SIGNAL);
    }

    struct ProducerShape {
        uint8_t mDirection = CircuitFace::NONE;
        bool mAllowAttachments = true;
        bool mNoWireStepDown = true;
    };

    // the face a producer strongly powers, the block it hangs on: the game's setup of each block
    ProducerShape producerShape(const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        ProducerShape shape;
        if (dynamic_cast<const LeverBlock *>(block) != nullptr) {
            shape.mDirection = (uint8_t) RedstoneFace::opposite(leverPointing(state));
        } else if (dynamic_cast<const ButtonBlock *>(block) != nullptr) {
            shape.mDirection = (uint8_t) RedstoneFace::opposite(buttonFacing(state));
        } else if (dynamic_cast<const PressurePlateBlock *>(block) != nullptr) {
            // a plate strongly powers the block under it and, unlike a lever or button, lets dust step down past it
            shape.mDirection = CircuitFace::DOWN;
            shape.mNoWireStepDown = false;
        } else if (dynamic_cast<const TrappedChestBlock *>(block) != nullptr
                   || dynamic_cast<const DetectorRailBlock *>(block) != nullptr
                   || dynamic_cast<const SculkSensorBlock *>(block) != nullptr) {
            shape.mDirection = CircuitFace::DOWN;
        } else if (dynamic_cast<const LecternBlock *>(block) != nullptr) {
            shape.mDirection = CircuitFace::DOWN;
            shape.mNoWireStepDown = false;
        } else if (dynamic_cast<const DaylightDetectorBlock *>(block) != nullptr) {
            shape.mDirection = CircuitFace::UP;
            shape.mAllowAttachments = false;
        } else if (DetectorSystem::isHook(state)) {
            shape.mDirection = (uint8_t) RedstoneFace::opposite(DetectorSystem::hookFacing(state));
        } else if (dynamic_cast<const LightningRodBlock *>(block) != nullptr) {
            const int facing = std::clamp(stateInt(state, "facing_direction", RedstoneFace::UP), 0, RedstoneFace::COUNT - 1);
            shape.mDirection = (uint8_t) RedstoneFace::opposite(facing);
        } else if (DetectorSystem::isTarget(state)) {
            shape.mAllowAttachments = false;
        } else if (dynamic_cast<const RedstoneBlock *>(block) != nullptr) {
            shape.mAllowAttachments = false;
            shape.mNoWireStepDown = false;
        }
        return shape;
    }

    // the game's setup of a consumer: whether it passes power on to the dust beyond it, takes a torch or diode
    // pointing into it from its own side, reads a one-evaluation pulse at its own strength, stops dust stepping down
    // past it and skips the update a new component gets
    struct ConsumerShape {
        bool mPropagatePower = false;
        bool mAcceptFacingCapacitor = true;
        bool mFullHalfPulse = false;
        bool mNoWireStepDown = false;
        bool mIgnoreFirstUpdate = false;
        int mStrength = 0;
    };

    ConsumerShape consumerShape(const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        const std::string &name = state.mName;
        ConsumerShape shape;
        // the blocks set up from the game's block table
        if (name == "minecraft:dispenser" || name == "minecraft:dropper") {
            shape.mPropagatePower = true;
            shape.mAcceptFacingCapacitor = false;
            shape.mFullHalfPulse = true;
        } else if (dynamic_cast<const RedstoneLampBlock *>(block) != nullptr || name == "minecraft:noteblock"
                   || name == "minecraft:bell" || name == "minecraft:big_dripleaf" || CommandBlock::matches(name)) {
            shape.mPropagatePower = true;
            shape.mAcceptFacingCapacitor = false;
        } else if (dynamic_cast<const TntBlock *>(block) != nullptr || name == "minecraft:underwater_tnt"
                   || BlockIdentifier::endsWith(name, "copper_bulb") || BlockIdentifier::endsWith(name, "_shelf")) {
            shape.mAcceptFacingCapacitor = false;
        } else if (BlockIdentifier::endsWith(name, "_skull") || BlockIdentifier::endsWith(name, "_head")) {
            shape.mFullHalfPulse = true;
        } else if (BlockIdentifier::endsWith(name, "shulker_box")) {
            shape.mPropagatePower = true;
            shape.mAcceptFacingCapacitor = false;
        } else if (dynamic_cast<const TrapDoorBlock *>(block) != nullptr) {
            // the only block the game marks to skip its first update
            shape.mNoWireStepDown = true;
            shape.mFullHalfPulse = true;
            shape.mIgnoreFirstUpdate = true;
            shape.mStrength = OpenableBlock::isOpen(state) ? RedstoneSystem::MAX_SIGNAL : 0;
        } else if (dynamic_cast<const FenceGateBlock *>(block) != nullptr) {
            shape.mNoWireStepDown = true;
            shape.mStrength = OpenableBlock::isOpen(state) ? RedstoneSystem::MAX_SIGNAL : 0;
        } else if (dynamic_cast<const DoorOrientationBlock *>(block) != nullptr) {
            shape.mFullHalfPulse = true;
            shape.mStrength = OpenableBlock::isOpen(state) ? RedstoneSystem::MAX_SIGNAL : 0;
        }
        return shape;
    }

    // a door or fence gate itself skips the update its new component gets. The note block's own handling of it is
    // not read yet: it keeps skipping it, so a note block placed beside power does not play
    bool skipsFirstUpdateItself(const BlockState &state) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        return dynamic_cast<const DoorOrientationBlock *>(block) != nullptr
               || dynamic_cast<const FenceGateBlock *>(block) != nullptr || state.mName == "minecraft:noteblock";
    }

    void setupComponent(Level &level, const Vector3i &position, const BlockState &state) {
        CircuitSystem &circuit = *stateOf(level).mCircuit;
        switch (kindOf(state)) {
            case CircuitKind::Wire:
                circuit.create<TransporterComponent>(position, CircuitFace::DOWN);
                break;
            case CircuitKind::Torch: {
                auto *torch = circuit.create<RedstoneTorchCapacitor>(position, torchDirection(state));
                if (torch != nullptr)
                    torch->setOn(blockAs<RedstoneTorchBlock>(state)->isLit());
                break;
            }
            case CircuitKind::Repeater: {
                auto *repeater = circuit.create<RepeaterCapacitor>(position, (uint8_t) cardinalFacing(state));
                if (repeater != nullptr) {
                    repeater->setDelay(stateInt(state, "repeater_delay", 0));
                    repeater->setStrength(isDiodePowered(state) ? RedstoneSystem::MAX_SIGNAL : 0);
                }
                break;
            }
            case CircuitKind::Comparator: {
                auto *comparator = circuit.create<ComparatorCapacitor>(position, (uint8_t) cardinalFacing(state));
                if (comparator != nullptr)
                    comparator->setSubtractMode(stateBool(state, "output_subtract_bit", false));
                break;
            }
            case CircuitKind::Observer: {
                auto *observer = circuit.create<PulseCapacitor>(position, (uint8_t) observerFacing(state));
                if (observer != nullptr) {
                    observer->setAllowAttachments(true);
                    observer->setBlocksWireStepDown(true);
                    if (stateBool(state, "powered_bit", false))
                        observer->setStrength(RedstoneSystem::MAX_SIGNAL);
                }
                break;
            }
            case CircuitKind::Producer: {
                const ProducerShape shape = producerShape(state);
                auto *producer = circuit.create<ProducerComponent>(position, shape.mDirection);
                if (producer != nullptr) {
                    producer->setAllowAttachments(shape.mAllowAttachments);
                    producer->setBlocksWireStepDown(shape.mNoWireStepDown);
                    producer->setStrength(producerSignal(level, position, state));
                }
                break;
            }
            case CircuitKind::Piston: {
                // the search reaches the head from in front moving against the push: that face takes no power
                const auto face = (uint8_t) RedstoneFace::opposite(PistonSystem::getPistonFace(state));
                auto *piston = circuit.create<PistonConsumerComponent>(position, face);
                if (piston != nullptr)
                    piston->setBlockedFace(face);
                break;
            }
            case CircuitKind::Lamp:
            case CircuitKind::Consumer:
            case CircuitKind::Door: {
                auto *consumer = circuit.create<ConsumerComponent>(position, CircuitFace::NONE);
                if (consumer != nullptr) {
                    const ConsumerShape shape = consumerShape(state);
                    consumer->setPropagatePower(shape.mPropagatePower);
                    consumer->setAcceptFacingCapacitor(shape.mAcceptFacingCapacitor);
                    consumer->setFullHalfPulse(shape.mFullHalfPulse);
                    consumer->setBlocksWireStepDown(shape.mNoWireStepDown);
                    consumer->setIgnoreFirstUpdate(shape.mIgnoreFirstUpdate);
                    consumer->setStrength(shape.mStrength);
                }
                break;
            }
            case CircuitKind::None:
                break;
        }
    }

    // a block change that kept the block: what the component reads off the state follows it
    void refreshComponent(Level &level, const Vector3i &position, const BlockState &state) {
        CircuitSystem &circuit = *stateOf(level).mCircuit;
        BaseCircuitComponent *component = circuit.findComponent(position);
        if (component == nullptr)
            return;
        switch (kindOf(state)) {
            case CircuitKind::Repeater:
                static_cast<RepeaterCapacitor *>(component)->setDelay(stateInt(state, "repeater_delay", 0));
                break;
            case CircuitKind::Comparator:
                static_cast<ComparatorCapacitor *>(component)
                        ->setSubtractMode(stateBool(state, "output_subtract_bit", false));
                break;
            case CircuitKind::Producer:
                component->setStrength(producerSignal(level, position, state));
                break;
            default:
                break;
        }
    }

    int redstoneChange(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int previousPower,
                       int power) {
        EventSignal<RedstoneChangeBeforeEvent> &redstone = owner.getEventBus().before().mRedstoneChange;
        if (!redstone.hasSubscribers())
            return power;

        RedstoneChangeBeforeEvent event(level, position, previousPower, power);
        redstone.emit(event);
        return event.mPower;
    }

    // the circuit's own state changes keep the graph as it is
    void setLockedBlock(Level &level, const Vector3i &position, const BlockState &state) {
        CircuitSystem &circuit = *stateOf(level).mCircuit;
        const bool locked = circuit.isLocked();
        circuit.setLocked(true);
        level.setBlock(position, state, false);
        circuit.setLocked(locked);
    }

    // the block the comparator reads: the one behind, or past it through a solid block
    int comparatorAnalogInput(Level &level, const Vector3i &position, const BlockState &state) {
        const int facing = cardinalFacing(state);
        const Vector3i front = RedstoneFace::relative(position, facing);
        const auto signalAt = [&](const Vector3i &at) {
            const BlockState found = stateAt(level, at);
            const Block *block = VanillaBlocks::fromIdentifier(found.mName);
            return block == nullptr ? -1 : block->getComparatorSignal(level, at, found);
        };
        const int signal = signalAt(front);
        if (signal >= 0 || !RedstoneSystem::isNormalBlock(stateAt(level, front)))
            return signal;
        return signalAt(RedstoneFace::relative(front, facing));
    }

    void onConsumerUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position, const BlockState &state,
                          int strength) {
        const Block *block = VanillaBlocks::fromIdentifier(state.mName);
        if (PistonSystem::isPiston(state.mName)) {
            PistonSystem::onRedstoneUpdate(owner, level, position, state);
        } else if (CommandBlock::matches(state.mName)) {
            CommandBlockSystem::setPowered(owner, level, position, strength > 0);
        } else if (dynamic_cast<const DoorOrientationBlock *>(block) != nullptr) {
            DoorBlock::onRedstoneUpdate(owner, level, position, state);
        } else if (dynamic_cast<const TrapDoorBlock *>(block) != nullptr
                   || dynamic_cast<const FenceGateBlock *>(block) != nullptr) {
            OpenableBlock::onRedstoneUpdate(owner, level, position, state);
        } else if (dynamic_cast<const TntBlock *>(block) != nullptr) {
            if (strength > 0)
                TntBlock::prime(owner, level, position, PrimedTntActor::DEFAULT_FUSE);
        } else if (dynamic_cast<const DispenserBlock *>(block) != nullptr) {
            DispenserBlock::onRedstoneUpdate(owner, level, position, state);
        } else if (dynamic_cast<const CrafterBlock *>(block) != nullptr) {
            CrafterBlock::onRedstoneUpdate(owner, level, position, state);
        } else if (block != nullptr) {
            block->onRedstonePower(owner, level, position, state);
        }

        const CustomBlockDefinition *custom = CustomContentRegistry::getInstance().findBlock(state.mName);
        if (custom != nullptr && custom->mRedstoneConsumer)
            owner.getScriptEngine().onCustomBlockRedstone(level, state.mName, position, strength);
    }

    // the circuit handed the component at `position` its new strength
    void onComponentUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int strength,
                           bool firstTime) {
        const BlockState state = stateAt(level, position);
        switch (kindOf(state)) {
            case CircuitKind::Wire: {
                const int previous = wireSignal(state);
                const int shown = previous == strength ? strength : redstoneChange(owner, level, position, previous,
                                                                                   strength);
                if (shown == previous)
                    return;
                Tag states = state.mStates;
                states.putInt("redstone_signal", std::clamp(shown, 0, RedstoneSystem::MAX_SIGNAL));
                setLockedBlock(level, position, BlockState(state.mName, states));
                return;
            }
            case CircuitKind::Torch: {
                const bool lit = strength > 0;
                if (blockAs<RedstoneTorchBlock>(state)->isLit() != lit)
                    setLockedBlock(level, position, lit ? RedstoneTorchBlock::getLitState(state)
                                                        : RedstoneTorchBlock::getUnlitState(state));
                return;
            }
            case CircuitKind::Repeater:
            case CircuitKind::Comparator: {
                const bool powered = strength > 0;
                if (isDiodePowered(state) == powered)
                    return;
                const RedstoneDiodeBlock *diode = blockAs<RedstoneDiodeBlock>(state);
                setLockedBlock(level, position, powered ? diode->getPoweredState(state) : diode->getUnpoweredState(state));
                return;
            }
            case CircuitKind::Lamp:
                if (strength > 0) {
                    level.cancelUpdate(position);
                    if (!blockAs<RedstoneLampBlock>(state)->isLit()) {
                        setLockedBlock(level, position, RedstoneLampBlock::getLitState(state));
                    }
                } else {
                    level.scheduleUpdate(position, LIT_LAMP_TURN_OFF_DELAY);
                }
                return;
            case CircuitKind::Consumer:
            case CircuitKind::Door:
            case CircuitKind::Piston:
                if (firstTime && skipsFirstUpdateItself(state))
                    return;
                onConsumerUpdate(owner, level, position, state, strength);
                return;
            default:
                return;
        }
    }

    void scanChunk(Level &level, const LevelChunk &chunk) {
        for (int section = 0; section < LevelChunk::SUB_CHUNK_COUNT; ++section) {
            const SubChunk &subChunk = chunk.getSubChunk(section);
            if (!subChunk.paletteMatches(SubChunk::PaletteQuery::Circuit, hasCircuitKind))
                continue;

            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 0; y < 16; ++y) {
                        const BlockState &state = subChunk.getBlock(x, y, z);
                        if (!hasCircuitKind(state))
                            continue;
                        const Vector3i position(chunk.getX() * 16 + x,
                                                (section + LevelChunk::LOWEST_SUB_CHUNK_Y) * 16 + y,
                                                chunk.getZ() * 16 + z);
                        setupComponent(level, position, state);
                    }
                }
            }
        }
    }

    int pressurePlateComputeStrength(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state, std::vector<uint64_t> *activators) {
        const float minX = (float) position.x + 0.125f;
        const float maxX = (float) position.x + 0.875f;
        const float minY = (float) position.y;
        const float maxY = (float) position.y + 0.25f;
        const float minZ = (float) position.z + 0.125f;
        const float maxZ = (float) position.z + 0.875f;

        int count = 0;

        for (auto &entry: owner.getPlayers()) {
            ServerPlayer &player = entry.second;
            if (!player.isSpawned() || player.isDead() || !player.isIn(level))
                continue;

            const Vector3f &feet = player.getPosition();
            if (feet.x >= minX && feet.x <= maxX && feet.y >= minY && feet.y <= maxY && feet.z >= minZ
                && feet.z <= maxZ) {
                ++count;
                if (activators != nullptr)
                    activators->push_back(player.getRuntimeId());
            }
        }

        for (auto &entry: owner.getActors()) {
            ServerActor *actor = entry.second.get();
            if (actor == nullptr || actor->isDead() || !actor->isIn(level))
                continue;

            const Vector3f &feet = actor->getPosition();
            if (feet.x >= minX && feet.x <= maxX && feet.y >= minY && feet.y <= maxY && feet.z >= minZ
                && feet.z <= maxZ) {
                ++count;
                if (activators != nullptr)
                    activators->push_back(actor->getRuntimeId());
            }
        }

        if (count == 0)
            return 0;

        return blockAs<PressurePlateBlock>(state)->getSignalForEntityCount(count);
    }

    void pressurePlateUpdateState(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int oldStrength,
                                  Actor *source = nullptr) {
        const BlockState state = stateAt(level, position);
        if (!isA<PressurePlateBlock>(state))
            return;

        EventSignal<PressurePlateUpdateBeforeEvent> &plateUpdate = owner.getEventBus().before().mPressurePlateUpdate;
        const bool notify = plateUpdate.hasSubscribers();
        std::vector<uint64_t> activators;
        int strength = pressurePlateComputeStrength(owner, level, position, state, notify ? &activators : nullptr);
        Tag states = state.mStates;
        states.putInt("redstone_signal", strength);
        BlockState updated(state.mName, states);
        if (notify && (oldStrength > 0 || !activators.empty())) {
            PressurePlateUpdateBeforeEvent event(level, position, oldStrength, strength, updated, activators);
            plateUpdate.emit(event);
            if (event.isCancelled()) {
                if (event.mRecheck && oldStrength > 0)
                    level.scheduleUpdate(position, PRESSURE_PLATE_RECHECK_TICKS);
                return;
            }
            // the state a handler left: unchanged, it still holds the computed strength
            strength = isA<PressurePlateBlock>(updated) ? updated.mStates.getInt("redstone_signal") : 0;
        }

        if (oldStrength != strength) {
            strength = redstoneChange(owner, level, position, oldStrength, strength);
            if (isA<PressurePlateBlock>(updated)) {
                Tag adjusted = updated.mStates;
                adjusted.putInt("redstone_signal", strength);
                updated = BlockState(updated.mName, adjusted);
            }
        }
        const bool wasPowered = oldStrength > 0;
        const bool powered = strength > 0;

        if (!(updated == state)) {
            level.setBlock(position, updated, false);
            owner.getScriptEngine().onPressurePlate(level, position, oldStrength, strength, source);

            const Vector3f center((float) position.x + 0.5f, (float) position.y + 0.1f, (float) position.z + 0.5f);
            if (!powered && wasPowered)
                owner.playBlockSound(level, SOUND_POWER_OFF, center, updated.getHash());
            else if (powered && !wasPowered)
                owner.playBlockSound(level, SOUND_POWER_ON, center, updated.getHash());
        }

        if (powered)
            level.scheduleUpdate(position, PRESSURE_PLATE_RECHECK_TICKS);
    }


    int buttonHoldTicks(const BlockState &state) {
        const std::string &name = state.mName;
        return name == "minecraft:stone_button" || name == "minecraft:polished_blackstone_button" ? STONE_BUTTON_TICKS
                                                                                                    : WOODEN_BUTTON_TICKS;
    }

    // the game's observer: the pulse is the component's, the block only shows it
    void observerOnScheduled(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                             const BlockState &state) {
        (void) owner;
        Tag states = state.mStates;
        if (!stateBool(state, "powered_bit", false)) {
            states.putByte("powered_bit", 1);
            setLockedBlock(level, position, BlockState(state.mName, states));
            stateOf(level).mCircuit->setStrength(position, RedstoneSystem::MAX_SIGNAL);
            level.scheduleUpdate(position, OBSERVER_PULSE_TICKS);
            return;
        }

        states.putByte("powered_bit", 0);
        setLockedBlock(level, position, BlockState(state.mName, states));
    }
}

int64_t RedstoneSystem::packPosition(const Vector3i &position) {
    return CircuitSceneGraph::key(position);
}

Vector3i RedstoneSystem::unpackPosition(int64_t packed) {
    const auto signExtend = [](int64_t value, int bits) {
        const int64_t sign = (int64_t) 1 << (bits - 1);
        return (int32_t) ((value ^ sign) - sign);
    };

    return Vector3i(signExtend((packed >> 38) & 0x3ffffff, 26), signExtend((packed >> 26) & 0xfff, 12),
                    signExtend(packed & 0x3ffffff, 26));
}

bool RedstoneSystem::isNormalBlock(const BlockState &state) {
    // told by the state alone, and asked around every block an explosion breaks: kept per state
    thread_local std::unordered_map<int32_t, bool> known;
    const int32_t hash = state.getHash();
    const auto found = known.find(hash);
    if (found != known.end())
        return found->second;

    const BlockData *data = BlockDataTable::find(state.mName.c_str());
    if (data == nullptr)
        return false;

    const bool normal = !data->mTransparent && data->mSolid && !isPowerSource(state);
    known.emplace(hash, normal);
    return normal;
}

bool RedstoneSystem::canConnectWireTo(Level &level, const Vector3i &position, int side) {
    const BlockState state = stateAt(level, position);
    if (isA<RedStoneWireBlock>(state))
        return true;

    if (isA<RedstoneDiodeBlock>(state)) {
        const int facing = cardinalFacing(state);
        return facing == side || RedstoneFace::opposite(facing) == side;
    }

    return isPowerSource(state) && side != RedstoneFace::NONE;
}

bool RedstoneSystem::isPowerSource(const BlockState &state) {
    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    if (block == nullptr)
        return false;

    if (dynamic_cast<const RedStoneWireBlock *>(block) != nullptr)
        return wireSignal(state) > 0;
    if (DetectorSystem::isHook(state) || DetectorSystem::isTarget(state))
        return true;

    return block->isSignalSource();
}

CircuitSystem &RedstoneSystem::circuitOf(Level &level) {
    return *stateOf(level).mCircuit;
}

int RedstoneSystem::getStrength(Level &level, const Vector3i &position) {
    return std::max(stateOf(level).mCircuit->getStrength(position), 0);
}

// what the component at `position` gives the cell on its `face` side, from that cell's side: a producer powers every
// side, a torch every side but its support, a diode or observer only the side it points to, and dust the block under it
// and the sides it points into. Consumers, powered blocks and rails give no power
int RedstoneSystem::getWeakPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face) {
    (void) owner;
    if (face < 0 || face >= RedstoneFace::COUNT)
        return 0;
    CircuitSystem &circuit = *stateOf(level).mCircuit;
    const BaseCircuitComponent *component = circuit.findComponent(position);
    if (component == nullptr)
        return 0;
    // the direction from the source to the cell asking
    const auto towards = (uint8_t) RedstoneFace::opposite(face);
    const int strength = std::clamp(component->getStrength(), 0, MAX_SIGNAL);
    switch (component->getBaseType()) {
        case CircuitComponentType::Producer:
            return strength;
        case CircuitComponentType::Capacitor:
            if (component->getInstanceType() == CircuitComponentType::Torch)
                return component->getDirection() != towards ? strength : 0;
            return component->getPoweroutDirection() == towards ? strength : 0;
        case CircuitComponentType::Transporter: {
            if (towards == CircuitFace::DOWN)
                return strength;
            if (towards == CircuitFace::UP)
                return 0;
            // a dot powers every side; a line only the side it points straight into
            bool connected[CircuitFace::NONE] = {};
            bool any = false;
            for (uint8_t side = CircuitFace::NORTH; side <= CircuitFace::EAST; ++side) {
                const Vector3i next = CircuitFace::relative(position, side);
                for (int dy = -1; dy <= 1 && !connected[side]; ++dy) {
                    const BaseCircuitComponent *other = circuit.findComponent(Vector3i(next.x, next.y + dy, next.z));
                    if (other == nullptr)
                        continue;
                    const CircuitComponentType base = other->getBaseType();
                    connected[side] = dy == 0 ? base == CircuitComponentType::Transporter
                                                        || base == CircuitComponentType::Producer
                                                        || base == CircuitComponentType::Capacitor
                                              : base == CircuitComponentType::Transporter;
                }
                any = any || connected[side];
            }
            if (!any)
                return strength;
            const auto left = (uint8_t) RedstoneFace::rotateYCounterClockwise(towards);
            const auto right = (uint8_t) RedstoneFace::rotateY(towards);
            return connected[towards] && !connected[left] && !connected[right] ? strength : 0;
        }
        default:
            return 0;
    }
}

// only through the block a producer hangs on, or the one a capacitor or wire points into
int RedstoneSystem::getStrongPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face) {
    const BaseCircuitComponent *component = stateOf(level).mCircuit->findComponent(position);
    if (component == nullptr || face < 0 || face >= RedstoneFace::COUNT)
        return 0;
    const auto towards = (uint8_t) RedstoneFace::opposite(face);
    switch (component->getBaseType()) {
        case CircuitComponentType::Producer: {
            const auto *producer = static_cast<const ProducerComponent *>(component);
            return producer->allowsAttachments() && producer->getDirection() == towards ? getWeakPower(owner, level,
                                                                                                         position, face)
                                                                                       : 0;
        }
        case CircuitComponentType::Capacitor:
            return component->getPoweroutDirection() == towards ? getWeakPower(owner, level, position, face) : 0;
        default:
            return 0;
    }
}

int RedstoneSystem::getRedstonePower(ServerNetworkHandler &owner, Level &level, const Vector3i &position, int face) {
    const BaseCircuitComponent *component = stateOf(level).mCircuit->getComponent(position);
    if (component != nullptr && component->getBaseType() == CircuitComponentType::PoweredBlock)
        return std::clamp(component->getStrength(), 0, MAX_SIGNAL);
    return getWeakPower(owner, level, position, face);
}

// a consumer or powered block takes the strength the circuit gave it; any other cell, such as the air an armor stand
// stands in, the strongest power its six neighbours give it from their side
int RedstoneSystem::isBlockIndirectlyGettingPowered(ServerNetworkHandler &owner, Level &level,
                                                    const Vector3i &position) {
    // placed or still waiting for the graph: a new consumer has no strength yet
    const BaseCircuitComponent *component = stateOf(level).mCircuit->findComponent(position);
    if (component != nullptr && (component->getBaseType() == CircuitComponentType::Consumer
                                 || component->getBaseType() == CircuitComponentType::PoweredBlock))
        return std::clamp(component->getStrength(), 0, MAX_SIGNAL);
    int power = 0;
    for (int face = 0; face < RedstoneFace::COUNT && power < MAX_SIGNAL; ++face)
        power = std::max(power, getRedstonePower(owner, level, RedstoneFace::relative(position, face), face));
    return power;
}

bool RedstoneSystem::isGettingPower(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    return isBlockIndirectlyGettingPowered(owner, level, position) > 0;
}

void RedstoneSystem::updateAroundRedstone(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    (void) owner;
    if (!isChunkReady(level, position))
        return;
    refreshComponent(level, position, stateAt(level, position));
}

void RedstoneSystem::onBlockChanged(Level &level, const Vector3i &position, const BlockState &previous,
                                    const BlockState &current) {
    for (int face = 0; face < RedstoneFace::COUNT; ++face) {
        const Vector3i side = RedstoneFace::relative(position, face);
        const BlockState *watcher = level.peekBlockPtr(side.x, side.y, side.z);
        if (watcher == nullptr || watcher->mName != OBSERVER_NAME)
            continue;
        // it watches the cell its facing points to
        if (observerFacing(*watcher) != RedstoneFace::opposite(face))
            continue;
        if (stateBool(*watcher, "powered_bit", false) || level.isUpdateScheduled(side))
            continue;
        level.scheduleUpdate(side, OBSERVER_PULSE_TICKS);
    }

    CircuitSystem &circuit = *stateOf(level).mCircuit;
    if (circuit.isLocked())
        return;
    if (previous.mName != current.mName) {
        circuit.removeComponents(position);
        if (hasCircuitKind(current))
            setupComponent(level, position, current);
    } else {
        refreshComponent(level, position, current);
    }
    circuit.getSceneGraph().invalidatePosition(position);
}

// the game lights a moved observer; one already lit keeps its light and gets a tick here to put it out, since
// the one it had stayed behind with its old cell
void RedstoneSystem::onObserverMoved(ServerNetworkHandler &owner, Level &level, const Vector3i &position) {
    const BlockState state = stateAt(level, position);
    if (!isA<ObserverBlock>(state))
        return;
    if (stateBool(state, "powered_bit", false)) {
        if (!level.isUpdateScheduled(position))
            level.scheduleUpdate(position, OBSERVER_PULSE_TICKS);
        return;
    }
    level.cancelUpdate(position);
    observerOnScheduled(owner, level, position, state);
}

// the circuit drives every power change: only the blocks' own ticks are left here
void RedstoneSystem::onRedstoneUpdate(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state, BlockUpdateType type) {
    if (type != BlockUpdateType::Scheduled)
        return;

    const Block *block = VanillaBlocks::fromIdentifier(state.mName);
    if (const RedstoneLampBlock *lamp = dynamic_cast<const RedstoneLampBlock *>(block)) {
        if (lamp->isLit() && getStrength(level, position) == 0)
            setLockedBlock(level, position, RedstoneLampBlock::getUnlitState(state));
    } else if (dynamic_cast<const ObserverBlock *>(block) != nullptr) {
        observerOnScheduled(owner, level, position, state);
    } else if (dynamic_cast<const ButtonBlock *>(block) != nullptr) {
        if (!stateBool(state, "button_pressed_bit", false))
            return;
        Tag states = state.mStates;
        states.putByte("button_pressed_bit", 0);
        const BlockState released = BlockState(state.mName, states);
        level.setBlock(position, released, false);
        owner.playBlockSound(level, SOUND_POWER_OFF, centerOf(position), released.getHash());
        level.postGameEvent(GameEvents::BLOCK_DEACTIVATE, centerOf(position));
    } else if (dynamic_cast<const PressurePlateBlock *>(block) != nullptr) {
        const int power = std::clamp(stateInt(state, "redstone_signal", 0), 0, MAX_SIGNAL);
        if (power > 0)
            pressurePlateUpdateState(owner, level, position, power);
    } else if (DetectorSystem::isWire(state)) {
        DetectorSystem::onWireScheduled(owner, level, position, state);
    }
}

void RedstoneSystem::onChunkLoaded(ServerNetworkHandler &owner, Level &level, const LevelChunk &chunk) {
    (void) owner;
    scanChunk(level, chunk);
    stateOf(level).mCircuit->getSceneGraph().onChunkLoaded(chunk.getX(), chunk.getZ());
    stateOf(level).mCircuit->onChunkLoaded(chunk.getX(), chunk.getZ());
}

void RedstoneSystem::onChunkUnloaded(Level &level, int32_t chunkX, int32_t chunkZ) {
    const auto found = gStates.find(&level);
    if (found == gStates.end())
        return;
    found->second.mCircuit->removeChunk(chunkX, chunkZ);
}

void RedstoneSystem::forgetLevel(Level &level) {
    gStates.erase(&level);
}

void RedstoneSystem::moveLevelState(Level &from, Level &to) {
    if (&from == &to)
        return;

    gStates.erase(&to);
    auto state = gStates.extract(&from);
    if (state.empty())
        return;
    state.key() = &to;
    state.mapped().mSource = std::make_unique<LevelCircuitSource>(to);
    gStates.insert(std::move(state));
}

void RedstoneSystem::onLeverActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                      const BlockState &state) {
    const bool powered = !stateBool(state, "open_bit", false);

    Tag states = state.mStates;
    states.putByte("open_bit", powered ? 1 : 0);
    const BlockState toggled = BlockState(state.mName, states);
    level.setBlock(position, toggled, false);

    owner.playBlockSound(level, powered ? SOUND_POWER_ON : SOUND_POWER_OFF, centerOf(position), toggled.getHash());
}

void RedstoneSystem::onButtonActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state, Actor *source) {
    if (stateBool(state, "button_pressed_bit", false))
        return;

    level.scheduleUpdate(position, buttonHoldTicks(state));

    Tag states = state.mStates;
    states.putByte("button_pressed_bit", 1);
    const BlockState pressed = BlockState(state.mName, states);
    level.setBlock(position, pressed, false);

    owner.playBlockSound(level, SOUND_POWER_ON, centerOf(position), pressed.getHash());
    level.postGameEvent(GameEvents::BLOCK_ACTIVATE, centerOf(position), source);
}

void RedstoneSystem::onRepeaterActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                         const BlockState &state) {
    (void) owner;
    const int delay = stateInt(state, "repeater_delay", 0);

    Tag states = state.mStates;
    states.putInt("repeater_delay", delay == 3 ? 0 : delay + 1);
    level.setBlock(position, BlockState(state.mName, states), false);
}

void RedstoneSystem::onComparatorActivated(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                           const BlockState &state) {
    (void) owner;
    Tag states = state.mStates;
    states.putByte("output_subtract_bit", stateBool(state, "output_subtract_bit", false) ? 0 : 1);
    level.setBlock(position, BlockState(state.mName, states), false);
}

int RedstoneSystem::getComparatorOutput(Level &level, const Vector3i &position) {
    const BaseCircuitComponent *component = stateOf(level).mCircuit->getComponent(position);
    if (component == nullptr || component->getInstanceType() != CircuitComponentType::Comparator)
        return 0;
    return component->getStrength();
}

void RedstoneSystem::queueRedstoneNotification(Level &level, const Vector3i &position) {
    stateOf(level).mPendingNotifications.push_back(position);
}

void RedstoneSystem::tickCircuit(ServerNetworkHandler &owner, Level &level) {
    (void) owner;
    RedstoneState &redstone = stateOf(level);
    redstone.mCircuit->tickRedstone(redstone.mSource.get());
}

void RedstoneSystem::tick(ServerNetworkHandler &owner, Level &level) {
    RedstoneState &redstone = stateOf(level);
    CircuitSystem &circuit = *redstone.mCircuit;

    if (!redstone.mPendingNotifications.empty()) {
        const std::vector<Vector3i> notifications = std::move(redstone.mPendingNotifications);
        redstone.mPendingNotifications.clear();
        for (const Vector3i &position: notifications)
            updateAroundRedstone(owner, level, position);
    }

    std::unordered_set<int64_t> visited;
    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.isIn(level))
            continue;

        _touchPressurePlate(owner, level, player, visited);
    }

    // nearly no actor stands in a plate: the block is asked by address first, its answer kept per state
    static std::unordered_map<const BlockState *, bool> plates;
    bool actorInWire = false;
    for (auto &entry: owner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor == nullptr)
            continue;

        const Vector3f &feet = actor->getPosition();
        const BlockState *under = level.peekBlockPtr((int32_t) std::floor(feet.x), (int32_t) std::floor(feet.y),
                                                     (int32_t) std::floor(feet.z));
        // nearly every actor stands in air, neither wire nor plate
        if (under != nullptr && under->mName == BlockNames::AIR)
            continue;
        if (under != nullptr) {
            if (DetectorSystem::isWire(*under))
                actorInWire = true;
            auto known = plates.find(under);
            if (known == plates.end())
                known = plates.emplace(under, isA<PressurePlateBlock>(*under)).first;
            if (!known->second)
                continue;
        }

        if (actor->isDead() || !actor->isIn(level))
            continue;

        _touchPressurePlate(owner, level, *actor, visited);
    }

    DetectorSystem::tick(owner, level, actorInWire);

    // the comparator block reads the block behind it every tick
    for (const auto &entry: circuit.getSceneGraph().getComparators()) {
        BaseCircuitComponent *comparator = entry.second;
        const Vector3i &position = comparator->getPosition();
        if (comparator->isRemoved() || !isChunkReady(level, position))
            continue;
        const BlockState state = stateAt(level, position);
        if (isA<RedstoneComparatorBlock>(state))
            static_cast<ComparatorCapacitor *>(comparator)
                    ->setRearAnalogStrength(comparatorAnalogInput(level, position, state));
    }

    std::vector<int64_t> chunks;
    for (const auto &chunk: circuit.getSceneGraph().getChunkLists())
        chunks.push_back(chunk.first);
    const CircuitBlockUpdate update = [&](const Vector3i &position, int strength, int previous, bool firstTime) {
        (void) previous;
        onComponentUpdate(owner, level, position, strength, firstTime);
    };
    for (const int64_t key: chunks) {
        const auto chunkX = (int32_t) (key >> 32);
        const auto chunkZ = (int32_t) (uint32_t) key;
        if (level.isChunkResident(chunkX, chunkZ))
            circuit.updateBlocks(chunkX, chunkZ, update);
    }

    circuit.updateDependencies(redstone.mSource.get());
}

void RedstoneSystem::_touchPressurePlate(ServerNetworkHandler &owner, Level &level, Actor &actor,
                                         std::unordered_set<int64_t> &visited) {
    const Vector3f &feet = actor.getPosition();
    const Vector3i position((int32_t) std::floor(feet.x), (int32_t) std::floor(feet.y), (int32_t) std::floor(feet.z));

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

    pressurePlateUpdateState(owner, level, position, 0, &actor);
}
