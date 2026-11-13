#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventGenericPacket.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/ServerActor.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/Vibration/GameEvent.h"
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
        std::cout << "vibration checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
