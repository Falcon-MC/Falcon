#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Circuit/CircuitSystem.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/BlockTickingQueue.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <functional>
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const Vector3i ORIGIN(4, 64, 8);

    Vector3i east(const Vector3i &position, int steps) {
        return Vector3i(position.x + steps, position.y, position.z);
    }

    // a cleared slab of the spawn chunk, stone at y 63, ticked the way the server ticks a level
    struct Fixture {
        ServerNetworkHandler mOwner{"Circuit", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Fixture() {
            mLevel.setOwner(&mOwner);
            mLevel.setActiveColumns({BlockTickingQueue::column(0, 0)});
            mLevel.getChunk(0, 0);
            mLevel.getBlockActors().loadChunk(0, 0, {}, mOwner.getCodecContext());
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 60; y <= 72; ++y)
                        mLevel.setBlockState(x, y, z, BlockState(BlockNames::AIR));
                    mLevel.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
                }
            }
            run(4);
        }

        BlockState block(const char *identifier) {
            const Block *value = VanillaBlocks::fromIdentifier(identifier);
            require(value != nullptr, std::string("no block ") + identifier);
            return value->toBlockState();
        }

        void set(const Vector3i &position, const BlockState &state) {
            mLevel.setBlock(position, state, true);
        }

        BlockState at(const Vector3i &position) {
            return mLevel.getBlockState(position.x, position.y, position.z);
        }

        void step() {
            RedstoneSystem::tickCircuit(mOwner, mLevel);
            mLevel.tickBlockUpdates();
            RedstoneSystem::tick(mOwner, mLevel);
            PistonSystem::tick(mOwner, mLevel);
        }

        void run(int ticks) {
            for (int tick = 0; tick < ticks; ++tick)
                step();
        }

        // the tick a condition first held, counted from 1; 0 when it never did
        int within(int ticks, const std::function<bool()> &condition) {
            for (int tick = 1; tick <= ticks; ++tick) {
                step();
                if (condition())
                    return tick;
            }
            return 0;
        }

        int signal(const Vector3i &position) {
            return at(position).mStates.getInt("redstone_signal", 0);
        }

        BlockState lever(bool on) {
            BlockState state = block("minecraft:lever");
            state.mStates.putString("lever_direction", "up_north_south");
            state.mStates.putByte("open_bit", on ? 1 : 0);
            return state;
        }

        void flip(const Vector3i &position) {
            RedstoneSystem::onLeverActivated(mOwner, mLevel, position, at(position));
        }

        BlockState repeater(const char *facing, int delay) {
            BlockState state = block("minecraft:unpowered_repeater");
            state.mStates.putString("minecraft:cardinal_direction", facing);
            state.mStates.putInt("repeater_delay", delay);
            return state;
        }
    };

    // the game evaluates the circuit every second tick
    void evaluatesEverySecondTick() {
        CircuitSystem fresh;
        int count = 0;
        struct Loaded : public CircuitBlockSource {
            bool isLoaded(const Vector3i &) const override {
                return true;
            }
            bool isAir(const Vector3i &) const override {
                return true;
            }
            RedstoneConductivity getConductivity(const Vector3i &) const override {
                return {};
            }
        } region;
        for (int tick = 0; tick < 20; ++tick)
            count += fresh.tickRedstone(&region) ? 1 : 0;
        require(count == 10, "the circuit evaluated " + std::to_string(count) + " times in 20 ticks, not 10");
        require(fresh.tickRedstone(&region), "the first tick after an even count did not evaluate");
    }

    // dust loses one per block, and the whole line takes the signal in one evaluation
    void dustDampening() {
        Fixture fixture;
        const Vector3i source(0, ORIGIN.y, ORIGIN.z);
        fixture.set(source, fixture.lever(false));
        for (int step = 1; step <= 15; ++step)
            fixture.set(east(source, step), fixture.block("minecraft:redstone_wire"));
        fixture.run(6);

        fixture.flip(source);
        const int first = fixture.within(6, [&]() { return fixture.signal(east(source, 1)) != 0; });
        require(first != 0, "the first dust never got the signal");
        require(fixture.signal(east(source, 15)) == 1, "the fifteenth dust did not get the signal in the same tick");
        for (int step = 1; step <= 15; ++step) {
            const int expected = 16 - step;
            require(fixture.signal(east(source, step)) == expected,
                    "dust " + std::to_string(step) + " held " + std::to_string(fixture.signal(east(source, step)))
                            + ", not " + std::to_string(expected));
        }
    }

    // one evaluation per repeater step: delay 4 lags delay 1 by three evaluations, six ticks
    void repeaterDelay() {
        int ticks[4] = {};
        for (int delay = 0; delay < 4; ++delay) {
            Fixture fixture;
            const Vector3i source = ORIGIN;
            const Vector3i repeater = east(source, 1);
            fixture.set(source, fixture.block("minecraft:redstone_block"));
            fixture.set(source, BlockState(BlockNames::AIR));
            // cardinal_direction west: the repeater takes its input from the west, where the source is
            fixture.set(repeater, fixture.repeater("west", delay));
            fixture.set(east(source, 2), fixture.block("minecraft:redstone_wire"));
            fixture.run(6);
            fixture.set(source, fixture.block("minecraft:redstone_block"));
            ticks[delay] = fixture.within(20, [&]() { return fixture.at(repeater).mName == "minecraft:powered_repeater"; });
            require(ticks[delay] != 0, "a repeater with delay " + std::to_string(delay) + " never powered");
        }
        for (int delay = 1; delay < 4; ++delay)
            require(ticks[delay] - ticks[delay - 1] == 2,
                    "repeater delay step " + std::to_string(delay) + " took " +
                            std::to_string(ticks[delay] - ticks[delay - 1]) + " ticks, not one evaluation");
    }

    // a torch turns off one evaluation after its block is powered, and back on one after
    void torchInverts() {
        Fixture fixture;
        const Vector3i block = ORIGIN;
        const Vector3i torch(block.x, block.y + 1, block.z);
        fixture.set(block, fixture.block("minecraft:stone"));
        BlockState lit = fixture.block("minecraft:redstone_torch");
        lit.mStates.putString("torch_facing_direction", "top");
        fixture.set(torch, lit);
        const Vector3i lever = east(block, 1);
        BlockState onWall = fixture.lever(false);
        onWall.mStates.putString("lever_direction", "east");
        fixture.set(lever, onWall);
        fixture.run(6);
        require(fixture.at(torch).mName == "minecraft:redstone_torch", "an unpowered torch was not lit");

        fixture.flip(lever);
        const int off = fixture.within(10, [&]() { return fixture.at(torch).mName == "minecraft:unlit_redstone_torch"; });
        require(off != 0 && off <= 4, "a torch on a powered block did not go out within two evaluations");
        fixture.flip(lever);
        const int on = fixture.within(10, [&]() { return fixture.at(torch).mName == "minecraft:redstone_torch"; });
        require(on != 0 && on <= 4, "a torch did not light again within two evaluations");
    }

    // the lamp lights with the evaluation and goes out four ticks after it loses power
    void lampTurnsOffLate() {
        Fixture fixture;
        const Vector3i lever = ORIGIN;
        const Vector3i lamp = east(lever, 1);
        fixture.set(lever, fixture.lever(false));
        fixture.set(lamp, fixture.block("minecraft:redstone_lamp"));
        fixture.run(6);

        fixture.flip(lever);
        const int on = fixture.within(6, [&]() { return fixture.at(lamp).mName == "minecraft:lit_redstone_lamp"; });
        require(on != 0 && on <= 2, "a lamp beside a lever did not light within an evaluation");
        fixture.flip(lever);
        const int off = fixture.within(12, [&]() { return fixture.at(lamp).mName == "minecraft:redstone_lamp"; });
        require(off >= 5 && off <= 6, "a lamp went out " + std::to_string(off) + " ticks after losing power, not 4 after the evaluation");
    }

    // a lamp lit from the next chunk stays lit while that chunk is unloaded, and through its reload
    void chunkBorder() {
        Fixture fixture;
        fixture.mLevel.getChunk(1, 0);
        fixture.mLevel.setActiveColumns({BlockTickingQueue::column(0, 0), BlockTickingQueue::column(1, 0)});
        const Vector3i lamp(15, 64, 8);
        const Vector3i power(16, 64, 8);
        fixture.set(lamp, fixture.block("minecraft:redstone_lamp"));
        fixture.set(power, fixture.block("minecraft:redstone_block"));
        fixture.run(6);
        require(fixture.at(lamp).mName == "minecraft:lit_redstone_lamp", "a lamp beside a redstone block was not lit");

        LevelChunk east = fixture.mLevel.extractChunk(1, 0);
        fixture.run(12);
        require(fixture.at(lamp).mName == "minecraft:lit_redstone_lamp",
                "a lamp lit from an unloaded chunk went out");
        fixture.mLevel.insertChunk(std::move(east));
        fixture.run(12);
        require(fixture.at(lamp).mName == "minecraft:lit_redstone_lamp",
                "a lamp lit from the next chunk went out after that chunk loaded again");
    }

    // a powered piston blocked by obsidian extends once the obsidian is gone, with no change of power
    void pistonRetry() {
        Fixture fixture;
        const Vector3i piston = ORIGIN;
        BlockState state = fixture.block("minecraft:piston");
        state.mStates.putInt("facing_direction", 1);
        fixture.set(piston, state);
        const Vector3i front(piston.x, piston.y + 1, piston.z);
        fixture.set(front, fixture.block("minecraft:obsidian"));
        fixture.set(Vector3i(piston.x + 1, piston.y, piston.z), fixture.block("minecraft:redstone_block"));
        fixture.run(10);
        require(!PistonSystem::isExtended(fixture.mLevel, piston, fixture.at(piston)),
                "a piston pushed obsidian");
        fixture.set(front, BlockState(BlockNames::AIR));
        fixture.run(10);
        require(PistonSystem::isExtended(fixture.mLevel, piston, fixture.at(piston)),
                "a powered piston did not extend once the obsidian in front was gone");
    }

    // a cell with no component, such as the air an armor stand stands in, takes the power its neighbours give it
    // from their side: a lever powers it, a repeater running past it does not
    void powerAtAirCell() {
        Fixture fixture;
        const Vector3i cell = ORIGIN;
        BlockState lever = fixture.lever(true);
        fixture.set(Vector3i(cell.x - 1, cell.y, cell.z), lever);
        fixture.run(6);
        require(RedstoneSystem::isBlockIndirectlyGettingPowered(fixture.mOwner, fixture.mLevel, cell) == 15,
                "an air cell beside a lever got no power");

        Fixture other;
        // a repeater east of the cell, running north to south: the cell is beside it, not in front
        const Vector3i repeater = east(cell, 1);
        other.set(Vector3i(repeater.x, repeater.y, repeater.z - 1), other.block("minecraft:redstone_block"));
        other.set(repeater, other.repeater("north", 0));
        other.run(6);
        require(other.at(repeater).mName == "minecraft:powered_repeater", "the repeater beside the cell was not powered");
        const int fromRepeater = RedstoneSystem::getRedstonePower(other.mOwner, other.mLevel, repeater, RedstoneFace::EAST);
        require(fromRepeater == 0, "a repeater beside a cell powered it");
    }

    // no quasi-connectivity: power above and beside a piston's top does nothing
    void pistonNoQuasiConnectivity() {
        Fixture fixture;
        const Vector3i piston = ORIGIN;
        BlockState state = fixture.block("minecraft:piston");
        state.mStates.putInt("facing_direction", 1);
        fixture.set(piston, state);
        fixture.set(Vector3i(piston.x + 1, piston.y + 1, piston.z), fixture.block("minecraft:redstone_block"));
        fixture.run(10);
        require(!PistonSystem::isExtended(fixture.mLevel, piston, fixture.at(piston)),
                "a piston powered diagonally above extended");
        fixture.set(Vector3i(piston.x + 1, piston.y, piston.z), fixture.block("minecraft:redstone_block"));
        fixture.run(10);
        require(PistonSystem::isExtended(fixture.mLevel, piston, fixture.at(piston)),
                "a piston beside a redstone block did not extend");
    }
}

int main() {
    try {
        evaluatesEverySecondTick();
        dustDampening();
        repeaterDelay();
        torchInverts();
        lampTurnsOffLate();
        pistonNoQuasiConnectivity();
        chunkBorder();
        pistonRetry();
        powerAtAirCell();
        std::cout << "circuit checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
