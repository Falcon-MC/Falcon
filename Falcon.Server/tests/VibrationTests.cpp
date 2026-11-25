#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventGenericPacket.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/SculkSensorBlockActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/SculkSensorBlock.h"
#include "World/Level/Block/Systems/BlockContactSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
#include "World/Level/Vibration/MovementGameEvents.h"
#include "World/Level/Vibration/VibrationListener.h"
#include "World/Level/Vibration/VibrationProperties.h"
#include "World/Level/World.h"

#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

// the tests have no server loop: the tick counter moves by hand
class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner) {
        ++owner.mCurrentTick;
    }

    // the steps of a server tick a sensor needs, in its order
    static void tick(ServerNetworkHandler &owner, Level &level) {
        ++owner.mCurrentTick;
        level.tickBlockUpdates();
        level.getBlockActors().tick(owner);
    }
};

namespace {
    const Vector3i LISTENER(8, 64, 8);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Received {
        Vector3i mPosition;
        const GameEvent *mEvent = nullptr;
        float mDistance = 0.0f;
        uint32_t mRange = 0;
    };

    class RecordingBlockActor final : public BlockActor, public VibrationListenerConfig {
    public:
        explicit RecordingBlockActor(uint32_t range) : mListener(*this, LISTENER, range) {
            setPosition(LISTENER);
        }

        const char *getBlockActorId() const override {
            return "VibrationProbe";
        }

        Tag saveNbt() const override {
            Tag data = Tag::ofCompound();
            data.put("VibrationListener", mListener.save());
            return data;
        }

        void loadNbt(const Tag &data, const PacketCodecContext &context) override {
            (void) context;
            const Tag *listener = data.get("VibrationListener");
            if (listener != nullptr)
                mListener.load(*listener);
        }

        GameEventListener *getGameEventListener() override {
            return &mListener;
        }

        bool shouldListen(Level &level, const GameEvent &event, const GameEventContext &context) override {
            (void) level;
            (void) event;
            (void) context;
            return true;
        }

        void onSignalReceive(Level &level, const Vector3i &position, const GameEvent &event, Actor *source,
                             float distance, uint32_t range, Actor *projectileOwner) override {
            (void) level;
            (void) source;
            (void) projectileOwner;
            mReceived.push_back({position, &event, distance, range});
        }

        VibrationListener mListener;
        std::vector<Received> mReceived;
    };

    struct VibrationFixture {
        ServerNetworkHandler mOwner{"Vibration", "", 1};
        World &mWorld = mOwner.getWorlds().getDefault();
        Level &mLevel;
        RecordingBlockActor *mProbe = nullptr;
        std::vector<int32_t> mSignals;

        explicit VibrationFixture(uint32_t range = 8) : mLevel(mWorld.getLevel(DimensionType::Overworld)) {
            // the server sets it when it opens the world, which the tests never do
            mLevel.setOwner(&mOwner);
            for (int32_t chunkX = 0; chunkX <= 1; ++chunkX) {
                for (int32_t chunkZ = 0; chunkZ <= 1; ++chunkZ)
                    mLevel.getChunk(chunkX, chunkZ);
            }
            for (int x = 0; x < 32; ++x) {
                for (int z = 0; z < 32; ++z) {
                    for (int y = 60; y <= 72; ++y)
                        mLevel.setBlockState(x, y, z, BlockState(BlockNames::AIR));
                    mLevel.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
                }
            }

            std::unique_ptr<RecordingBlockActor> probe(new RecordingBlockActor(range));
            mProbe = probe.get();
            mLevel.getBlockActors().insert(std::move(probe));
            mLevel.setPacketBroadcaster([this](Level &, const Vector3f &, const Packet &packet) {
                const LevelEventGenericPacket *generic = dynamic_cast<const LevelEventGenericPacket *>(&packet);
                if (generic != nullptr)
                    mSignals.push_back(generic->mEventId);
            });
        }

        void place(const Vector3i &position, const BlockState &state) {
            mLevel.setBlockState(position.x, position.y, position.z, state);
        }

        // the server tick: the counter first, the block actors after the events of the tick
        void tick() {
            ServerTickProbe::advance(mOwner);
            mProbe->mListener.tick(mLevel);
        }

        void post(const GameEvent &event, const Vector3i &block, Actor *source = nullptr,
                  const BlockState *state = nullptr) {
            mLevel.postGameEvent(event,
                                 Vector3f((float) block.x + 0.5f, (float) block.y + 0.5f, (float) block.z + 0.5f),
                                 source, state);
        }

        // ticks until the signal lands, 0 when it never does within the limit
        int ticksUntilReceived(int limit) {
            for (int ticks = 1; ticks <= limit; ++ticks) {
                tick();
                if (!mProbe->mReceived.empty())
                    return ticks;
            }
            return 0;
        }
    };

    void eventTable() {
        require(GameEvents::BLOCK_PLACE.getId() == 9 && GameEvents::BLOCK_PLACE.getFrequency() == 13,
                "block_place is not id 9 at frequency 13");
        require(GameEvents::ENTITY_MOVE.getFrequency() == 1 && GameEvents::ENTITY_DIE.getFrequency() == 15,
                "entity_move or entity_die has the wrong frequency");
        require(GameEvents::SHRIEK.getFrequency() == 0 && !GameEvents::SHRIEK.has(GameEvent::VIBRATION),
                "shriek counts as a vibration");
        require(GameEvents::ENTITY_MOVE.has(GameEvent::DAMPENED_UNDER_SOURCE)
                        && GameEvents::ENTITY_MOVE.has(GameEvent::IGNORED_WHILE_SNEAKING),
                "entity_move lost its wool and sneaking rules");
        for (int32_t frequency = 1; frequency <= 15; ++frequency) {
            const GameEvent &resonance = GameEvents::resonate(frequency);
            require(resonance.getFrequency() == frequency
                            && std::string(resonance.getName()) == "resonate_" + std::to_string(frequency),
                    "resonate_" + std::to_string(frequency) + " is out of place");
        }
        require(&GameEvents::fromId(64) == &GameEvents::TELEPORT && &GameEvents::fromId(99) == &GameEvents::INVALID,
                "ids do not map back to their events");
        require(GameEvents::fromName("sculk_touch") == &GameEvents::SCULK_TOUCH, "sculk_touch is not found by name");
    }

    void blockProperties() {
        require(VibrationProperties::of(VanillaBlocks::WHITE_WOOL().toBlockState())
                        == (VibrationProperties::DAMPENS | VibrationProperties::OCCLUDES),
                "wool does not dampen and occlude");
        require(VibrationProperties::of(VanillaBlocks::WHITE_CARPET().toBlockState()) == VibrationProperties::DAMPENS,
                "carpet does more than dampen");
        require(VibrationProperties::of(VanillaBlocks::STONE().toBlockState()) == 0, "stone has vibration properties");
    }

    void travelsOneBlockATick() {
        VibrationFixture fixture;
        const BlockState stone = VanillaBlocks::STONE().toBlockState();
        fixture.post(GameEvents::BLOCK_PLACE, Vector3i(12, 64, 8), nullptr, &stone);
        require(fixture.ticksUntilReceived(10) == 4, "a vibration 4 blocks away did not land on the 4th tick");
        const Received &received = fixture.mProbe->mReceived.front();
        require(received.mEvent == &GameEvents::BLOCK_PLACE && received.mDistance == 4.0f && received.mRange == 8
                        && received.mPosition == Vector3i(12, 64, 8),
                "the signal did not carry its event, distance and range");
        require(fixture.mSignals.size() == 1 && fixture.mSignals.front() == 2027, "no vibration signal particle");
        require(&fixture.mProbe->mListener.getLastEvent() == &GameEvents::BLOCK_PLACE, "the last event was not kept");
    }

    void rangeLimits() {
        {
            VibrationFixture fixture;
            fixture.post(GameEvents::BLOCK_CHANGE, Vector3i(17, 64, 8));
            require(fixture.ticksUntilReceived(20) == 0, "a listener of range 8 heard a vibration 9 blocks away");
        }

        VibrationFixture wide(32);
        wide.post(GameEvents::BLOCK_CHANGE, Vector3i(25, 64, 8));
        require(wide.ticksUntilReceived(30) == 0, "a vibration past the 16 block notification radius was heard");
    }

    void woolOccludesAndDampens() {
        {
            VibrationFixture fixture;
            for (int y = 63; y <= 65; ++y) {
                for (int z = 7; z <= 9; ++z)
                    fixture.place(Vector3i(10, y, z), VanillaBlocks::WHITE_WOOL().toBlockState());
            }
            fixture.post(GameEvents::BLOCK_CHANGE, Vector3i(12, 64, 8));
            require(fixture.ticksUntilReceived(10) == 0, "a vibration went through a wool wall");
        }

        VibrationFixture dampened;
        const BlockState carpet = VanillaBlocks::WHITE_CARPET().toBlockState();
        dampened.post(GameEvents::BLOCK_PLACE, Vector3i(11, 64, 8), nullptr, &carpet);
        require(dampened.ticksUntilReceived(10) == 0, "placing a carpet made a vibration");
        dampened.post(GameEvents::BLOCK_CHANGE, Vector3i(11, 64, 8), nullptr, &carpet);
        require(dampened.ticksUntilReceived(10) == 3, "block_change of a carpet is not dampened by the game");
    }

    void sourceRules() {
        VibrationFixture fixture;
        ServerActor *cow = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:cow", Vector3f(11.5f, 64.0f, 8.5f));
        require(cow != nullptr, "no fixture cow");
        cow->getFlags().set(ActorFlag::Sneaking, true);
        fixture.post(GameEvents::ENTITY_MOVE, Vector3i(11, 64, 8), cow);
        require(fixture.ticksUntilReceived(10) == 0, "a sneaking source made a step vibration");
        fixture.post(GameEvents::ENTITY_DAMAGE, Vector3i(11, 64, 8), cow);
        require(fixture.ticksUntilReceived(10) == 3, "sneaking also silenced a damage vibration");

        cow->getFlags().set(ActorFlag::Sneaking, false);
        fixture.mProbe->mReceived.clear();
        fixture.place(Vector3i(11, 63, 8), VanillaBlocks::WHITE_WOOL().toBlockState());
        fixture.post(GameEvents::ENTITY_MOVE, Vector3i(11, 64, 8), cow);
        require(fixture.ticksUntilReceived(10) == 0, "walking on wool made a vibration");
    }

    void closestThenFrequency() {
        VibrationFixture fixture;
        fixture.post(GameEvents::BLOCK_CHANGE, Vector3i(13, 64, 8));
        fixture.post(GameEvents::FLAP, Vector3i(10, 64, 8));
        fixture.post(GameEvents::ENTITY_DIE, Vector3i(6, 64, 8));
        require(fixture.ticksUntilReceived(10) == 2, "the closest vibration of the tick did not win");
        require(fixture.mProbe->mReceived.front().mEvent == &GameEvents::ENTITY_DIE,
                "a tie did not go to the higher frequency");
    }

    void savesTheSignalInFlight() {
        Tag listener;
        {
            VibrationFixture fixture;
            fixture.post(GameEvents::EXPLODE, Vector3i(14, 64, 8));
            fixture.tick();
            fixture.tick();
            require(fixture.mProbe->mListener.hasPendingVibration(), "the signal is not on its way");
            const Tag saved = fixture.mProbe->saveNbt();
            const Tag *written = saved.get("VibrationListener");
            require(written != nullptr && written->getInt("ticks") == 4, "the travel ticks were not saved");
            listener = *written;
        }
        const Tag *pending = listener.get("pending");
        require(pending != nullptr && pending->getInt("vibration") == GameEvents::EXPLODE.getId()
                        && pending->getFloat("distance") == 6.0f && pending->getInt("x") == 14,
                "the pending vibration was not saved");
        require(listener.getInt("event") == 0 && listener.get("selector") != nullptr,
                "the listener wrote an event before any landed, or no selector");

        VibrationFixture restored;
        restored.mProbe->mListener.load(listener);
        require(restored.ticksUntilReceived(10) == 4, "the restored signal did not land where it left off");
        require(restored.mProbe->mReceived.front().mEvent == &GameEvents::EXPLODE, "the restored event changed");
    }

    void sculkSensorActivates() {
        ServerNetworkHandler owner("Sculk", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        std::vector<int64_t> columns;
        for (int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                level.getChunk(chunkX, chunkZ);
                columns.push_back(((int64_t) chunkX << 32) | (uint32_t) chunkZ);
            }
        }
        level.setActiveColumns(columns);
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                for (int y = 60; y <= 72; ++y)
                    level.setBlockState(x, y, z, BlockState(BlockNames::AIR));
                level.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
            }
        }

        const BlockState sensor = VanillaBlocks::SCULK_SENSOR().toBlockState();
        level.setBlockState(LISTENER.x, LISTENER.y, LISTENER.z, sensor);
        level.onBlockPlaced(LISTENER, sensor);
        require(level.getBlockActors().find(LISTENER) != nullptr, "a placed sculk sensor has no block actor");

        const auto phase = [&level]() {
            return SculkSensorBlock::phaseOf(level.getBlockState(LISTENER.x, LISTENER.y, LISTENER.z));
        };
        level.postGameEvent(GameEvents::BLOCK_CHANGE, Vector3f(12.5f, 64.5f, 8.5f));
        int ticks = 0;
        while (phase() != SculkSensorBlock::PHASE_ACTIVE && ticks < 10) {
            ServerTickProbe::tick(owner, level);
            ++ticks;
        }
        require(ticks == 4, "the sensor did not turn active when the signal landed");

        const BlockState active = level.getBlockState(LISTENER.x, LISTENER.y, LISTENER.z);
        const Block *block = VanillaBlocks::fromState(active);
        require(block != nullptr && block->getComparatorSignal(level, LISTENER, active) == 11,
                "the comparator does not read block_change's frequency");
        require(block->getEmittedSignal(level, LISTENER, active, RedstoneFace::NORTH, false) == 8
                        && block->getEmittedSignal(level, LISTENER, active, RedstoneFace::DOWN, true) == 8
                        && block->getEmittedSignal(level, LISTENER, active, RedstoneFace::UP, true) == 0,
                "4 blocks away in range 8 did not give 8, strong below only");

        ticks = 0;
        while (phase() == SculkSensorBlock::PHASE_ACTIVE && ticks < 40) {
            ServerTickProbe::tick(owner, level);
            ++ticks;
        }
        require(ticks == SculkSensorBlock::ACTIVE_TICKS && phase() == SculkSensorBlock::PHASE_COOLDOWN,
                "the sensor did not cool down after 30 ticks");
        const BlockState cooling = level.getBlockState(LISTENER.x, LISTENER.y, LISTENER.z);
        require(block->getEmittedSignal(level, LISTENER, cooling, RedstoneFace::NORTH, false) == 0,
                "a cooling sensor still gives power");

        level.postGameEvent(GameEvents::BLOCK_CHANGE, Vector3f(10.5f, 64.5f, 8.5f));
        ticks = 0;
        while (phase() == SculkSensorBlock::PHASE_COOLDOWN && ticks < 20) {
            ServerTickProbe::tick(owner, level);
            ++ticks;
        }
        require(ticks == SculkSensorBlock::COOLDOWN_TICKS && phase() == SculkSensorBlock::PHASE_INACTIVE,
                "the cooldown did not last 10 ticks");
        for (int i = 0; i < 10; ++i)
            ServerTickProbe::tick(owner, level);
        require(phase() == SculkSensorBlock::PHASE_INACTIVE, "a vibration heard while cooling down activated it");
    }

    void calibratedSensorFilters() {
        ServerNetworkHandler owner("Calibrated", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        level.setOwner(&owner);
        std::vector<int64_t> columns;
        for (int32_t chunkX = -1; chunkX <= 1; ++chunkX) {
            for (int32_t chunkZ = -1; chunkZ <= 1; ++chunkZ) {
                level.getChunk(chunkX, chunkZ);
                columns.push_back(((int64_t) chunkX << 32) | (uint32_t) chunkZ);
            }
        }
        level.setActiveColumns(columns);
        for (int x = 0; x < 16; ++x) {
            for (int z = 0; z < 16; ++z) {
                for (int y = 60; y <= 72; ++y)
                    level.setBlockState(x, y, z, BlockState(BlockNames::AIR));
                level.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
            }
        }

        // facing south: the input is the block north of it
        const BlockState base = VanillaBlocks::CALIBRATED_SCULK_SENSOR().toBlockState();
        Tag states = base.mStates;
        states.putString("minecraft:cardinal_direction", "south");
        const BlockState sensor(base.mName, states);
        require(CalibratedSculkSensorBlock::inputFace(sensor) == RedstoneFace::NORTH,
                "a sensor facing south does not take its input from the north");
        level.setBlockState(LISTENER.x, LISTENER.y, LISTENER.z, sensor);
        level.onBlockPlaced(LISTENER, sensor);
        require(level.getBlockActors().find<CalibratedSculkSensorBlockActor>(LISTENER) != nullptr,
                "a placed calibrated sensor has no block actor of its own");

        const auto phase = [&level]() {
            return SculkSensorBlock::phaseOf(level.getBlockState(LISTENER.x, LISTENER.y, LISTENER.z));
        };
        const auto ticksUntilActive = [&owner, &level, &phase](int limit) {
            for (int ticks = 1; ticks <= limit; ++ticks) {
                ServerTickProbe::tick(owner, level);
                if (phase() == SculkSensorBlock::PHASE_ACTIVE)
                    return ticks;
            }
            return 0;
        };

        level.setBlockState(LISTENER.x, LISTENER.y, LISTENER.z - 1, VanillaBlocks::REDSTONE_BLOCK().toBlockState());
        require(CalibratedSculkSensorBlock::inputSignal(owner, level, LISTENER, sensor) == 15,
                "the redstone block at the input does not read 15");
        level.postGameEvent(GameEvents::BLOCK_CHANGE, Vector3f(12.5f, 64.5f, 8.5f));
        require(ticksUntilActive(10) == 0, "an input of 15 let block_change (11) through");
        level.postGameEvent(GameEvents::ENTITY_DIE, Vector3f(12.5f, 64.5f, 8.5f));
        require(ticksUntilActive(10) == 4, "an input of 15 kept entity_die (15) out");

        const BlockState active = level.getBlockState(LISTENER.x, LISTENER.y, LISTENER.z);
        const Block *block = VanillaBlocks::fromState(active);
        require(block != nullptr && block->getEmittedSignal(level, LISTENER, active, RedstoneFace::SOUTH, false) == 0
                        && block->getEmittedSignal(level, LISTENER, active, RedstoneFace::NORTH, false) > 0,
                "the calibrated sensor powers its own input side");

        for (int i = 0; i < 60 && phase() != SculkSensorBlock::PHASE_INACTIVE; ++i)
            ServerTickProbe::tick(owner, level);
        level.setBlockState(LISTENER.x, LISTENER.y, LISTENER.z - 1, BlockState(BlockNames::AIR));
        level.postGameEvent(GameEvents::BLOCK_CHANGE, Vector3f(12.5f, 64.5f, 8.5f));
        require(ticksUntilActive(10) == 4, "with no input the calibrated sensor did not hear every frequency");
    }

    void dampersMakeNoVibration() {
        VibrationFixture fixture;
        ServerActor *warden = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:warden", Vector3f(11.5f, 64.0f, 8.5f));
        require(warden != nullptr, "no fixture warden");
        fixture.post(GameEvents::ENTITY_MOVE, Vector3i(11, 64, 8), warden);
        fixture.post(GameEvents::BLOCK_CHANGE, Vector3i(11, 64, 8), warden);
        require(fixture.ticksUntilReceived(10) == 0, "a warden's own vibration was heard");
        fixture.post(GameEvents::SCULK_TOUCH, Vector3i(11, 64, 8), warden);
        require(fixture.ticksUntilReceived(10) == 3, "sculk_touch ignores the damper but was not heard");
    }

    void movementSteps() {
        VibrationFixture fixture;
        ServerActor *cow = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:cow", Vector3f(11.5f, 64.0f, 8.5f));
        require(cow != nullptr, "no fixture cow");
        cow->setOnGround(true);
        MovementGameEvents::track(fixture.mOwner, *cow);
        MovementGameEvents::track(fixture.mOwner, *cow);
        require(fixture.ticksUntilReceived(5) == 0, "standing still made a step");

        cow->setPosition(Vector3f(11.0f, 64.0f, 8.5f));
        MovementGameEvents::track(fixture.mOwner, *cow);
        require(fixture.ticksUntilReceived(5) == 3, "a step on the ground made no entity_move");
        require(fixture.mProbe->mReceived.front().mEvent == &GameEvents::ENTITY_MOVE, "the step was not entity_move");
        require(cow->getMovementGameEventState().mDistance == 1.5f, "half a block did not add up to 1.5");
        // the default offset of 1: the next step is due at 2, a tenth of a block later is not one
        fixture.mProbe->mReceived.clear();
        cow->setPosition(Vector3f(10.9f, 64.0f, 8.5f));
        MovementGameEvents::track(fixture.mOwner, *cow);
        require(fixture.ticksUntilReceived(5) == 0, "moving on right after a step made another one");

        fixture.mProbe->mReceived.clear();
        cow->setOnGround(false);
        cow->setPosition(Vector3f(11.0f, 66.0f, 8.5f));
        MovementGameEvents::track(fixture.mOwner, *cow);
        require(fixture.ticksUntilReceived(5) == 0, "a cow in the air made a vibration");

        ServerActor *chicken =
                fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:chicken", Vector3f(10.5f, 66.0f, 8.5f));
        require(chicken != nullptr, "no fixture chicken");
        chicken->setOnGround(false);
        MovementGameEvents::track(fixture.mOwner, *chicken);
        MovementGameEvents::track(fixture.mOwner, *chicken);
        require(fixture.ticksUntilReceived(5) > 0 && fixture.mProbe->mReceived.front().mEvent == &GameEvents::FLAP,
                "a chicken in the air did not flap");
    }

    void landingAndSculkTouch() {
        VibrationFixture fixture;
        ServerActor *cow = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:cow", Vector3f(11.5f, 64.0f, 8.5f));
        require(cow != nullptr, "no fixture cow");
        BlockContactSystem::land(fixture.mOwner, *cow, 0.0f);
        require(fixture.ticksUntilReceived(5) == 0, "landing without a fall made hit_ground");
        BlockContactSystem::land(fixture.mOwner, *cow, 2.0f);
        require(fixture.ticksUntilReceived(5) == 3 && fixture.mProbe->mReceived.front().mEvent == &GameEvents::HIT_GROUND,
                "a fall did not make hit_ground");

        fixture.mProbe->mReceived.clear();
        const Vector3i sensor(10, 64, 8);
        const BlockState state = VanillaBlocks::SCULK_SENSOR().toBlockState();
        fixture.place(sensor, state);
        const Block *block = VanillaBlocks::fromState(state);
        require(block != nullptr, "no sculk sensor block");
        ServerActor *warden = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:warden", Vector3f(10.5f, 65.0f, 8.5f));
        require(warden != nullptr, "no fixture warden");
        block->onStepOn(fixture.mOwner, *warden, sensor, state);
        require(fixture.ticksUntilReceived(5) == 0, "a warden touched the sculk");
        block->onStepOn(fixture.mOwner, *cow, sensor, state);
        require(fixture.ticksUntilReceived(5) == 2 && fixture.mProbe->mReceived.front().mEvent == &GameEvents::SCULK_TOUCH,
                "stepping on a sensor made no sculk_touch");
    }

    void playerBreaksBlock() {
        VibrationFixture fixture;
        NetworkIdentifier id;
        ServerPlayer &player = fixture.mOwner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.setPosition(Vector3f(12.5f, 64.0f, 10.5f));
        fixture.place(Vector3i(12, 64, 8), VanillaBlocks::STONE().toBlockState());
        BlockActionHandler::breakBlock(fixture.mOwner, player, Vector3i(12, 64, 8));
        require(fixture.ticksUntilReceived(10) == 4 && fixture.mProbe->mReceived.front().mEvent == &GameEvents::BLOCK_DESTROY,
                "breaking a block made no block_destroy");

        fixture.mProbe->mReceived.clear();
        fixture.place(Vector3i(12, 64, 8), VanillaBlocks::WHITE_WOOL().toBlockState());
        BlockActionHandler::breakBlock(fixture.mOwner, player, Vector3i(12, 64, 8));
        require(fixture.ticksUntilReceived(10) == 0, "breaking wool made a vibration");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        eventTable();
        blockProperties();
        travelsOneBlockATick();
        rangeLimits();
        woolOccludesAndDampens();
        sourceRules();
        closestThenFrequency();
        savesTheSignalInFlight();
        dampersMakeNoVibration();
        movementSteps();
        landingAndSculkTouch();
        playerBreaksBlock();
        sculkSensorActivates();
        calibratedSensorFilters();
        std::cout << "vibration checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
