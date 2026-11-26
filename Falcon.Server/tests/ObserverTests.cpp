#include "Network/ServerNetworkHandler.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/BlockTickingQueue.h"
#include "World/Level/Block/Systems/PistonSystem.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // a cleared slab of the spawn chunk, stone at y 63, with its block actors loaded for the piston arm
    struct Fixture {
        ServerNetworkHandler mOwner{"Observers", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        Fixture() {
            // a running server binds its levels and ticks the columns players are near; a test without one does
            // both itself
            mLevel.setOwner(&mOwner);
            mLevel.setActiveColumns({BlockTickingQueue::column(0, 0)});
            mLevel.getChunk(0, 0);
            mLevel.getBlockActors().loadChunk(0, 0, {}, mOwner.getCodecContext());
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 60; y <= 75; ++y)
                        mLevel.setBlockState(x, y, z, BlockState(BlockNames::AIR));
                    mLevel.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
                }
            }
            // the clearing above is no change an observer should carry into the cases
            for (int tick = 0; tick < 10; ++tick)
                step();
        }

        BlockState block(const char *identifier) {
            const Block *value = VanillaBlocks::fromIdentifier(identifier);
            require(value != nullptr, std::string("no block ") + identifier);
            return value->toBlockState();
        }

        // looking at the cell its facing names
        void observer(const Vector3i &at, const char *facing) {
            BlockState state = block("minecraft:observer");
            state.mStates.putString("minecraft:facing_direction", facing);
            state.mStates.putByte("powered_bit", 0);
            mLevel.setBlock(at, state, false);
        }

        bool lit(const Vector3i &at) {
            const BlockState state = mLevel.getBlockState(at.x, at.y, at.z);
            require(state.mName == "minecraft:observer", "no observer at the cell, found " + state.mName);
            return state.mStates.getByte("powered_bit", 0) != 0;
        }

        void step() {
            RedstoneSystem::tickCircuit(mOwner, mLevel);
            mLevel.tickBlockUpdates();
            RedstoneSystem::tick(mOwner, mLevel);
            PistonSystem::tick(mOwner, mLevel);
        }

        // the tick it lit on, counted from 1; 0 when it stayed dark
        int litWithin(const Vector3i &at, int ticks) {
            for (int tick = 1; tick <= ticks; ++tick) {
                step();
                if (lit(at))
                    return tick;
            }
            return 0;
        }

        void settle() {
            for (int tick = 0; tick < 10; ++tick)
                step();
        }
    };

    // a crop growing in front of it: a pulse of two ticks, two ticks on
    void cropGrowth(Fixture &fixture) {
        const Vector3i observer(2, 64, 2);
        const Vector3i crop(2, 65, 2);
        fixture.mLevel.setBlock(crop, fixture.block("minecraft:wheat"), false);
        fixture.observer(observer, "up");
        fixture.settle();
        require(!fixture.lit(observer), "a placed observer lit without a change in front of it");

        BlockState grown = fixture.block("minecraft:wheat");
        grown.mStates.putInt("growth", 3);
        fixture.mLevel.setBlock(crop, grown, false);
        require(fixture.litWithin(observer, 4) == 2, "a crop growing did not light the observer after two ticks");
        fixture.step();
        require(fixture.lit(observer), "the pulse was shorter than two ticks");
        fixture.step();
        require(!fixture.lit(observer), "the pulse was longer than two ticks");
    }

    // a door opening counts; a change beside the observer, not in front, does not
    void doorAndSides(Fixture &fixture) {
        const Vector3i observer(4, 64, 4);
        const Vector3i door(5, 64, 4);
        fixture.mLevel.setBlock(door, fixture.block("minecraft:wooden_door"), false);
        fixture.observer(observer, "east");
        fixture.settle();

        fixture.mLevel.setBlock(Vector3i(4, 64, 5), fixture.block("minecraft:stone"), false);
        require(fixture.litWithin(observer, 4) == 0, "a block placed beside the observer lit it");

        BlockState open = fixture.block("minecraft:wooden_door");
        open.mStates.putByte("open_bit", 1);
        fixture.mLevel.setBlock(door, open, false);
        require(fixture.litWithin(observer, 4) != 0, "a door opening did not light the observer");
    }

    // a piston pushing a block into the watched cell lights it, and a pushed observer lights itself
    void pistonPush(Fixture &fixture) {
        const Vector3i piston(8, 64, 2);
        // stored facing_direction 4 is east: the horizontal ones are stored reversed
        BlockState state = fixture.block("minecraft:piston");
        state.mStates.putInt("facing_direction", 4);
        fixture.mLevel.setBlock(piston, state, false);
        fixture.mLevel.setBlock(Vector3i(9, 64, 2), fixture.block("minecraft:stone"), false);
        const Vector3i watcher(10, 64, 3);
        fixture.observer(watcher, "north");
        fixture.settle();

        fixture.mLevel.setBlock(Vector3i(7, 64, 2), fixture.block("minecraft:redstone_block"), false);
        PistonSystem::onRedstoneUpdate(fixture.mOwner, fixture.mLevel, piston,
                                       fixture.mLevel.getBlockState(piston.x, piston.y, piston.z));
        require(fixture.litWithin(watcher, 6) != 0, "a block pushed in front of an observer did not light it");
        fixture.settle();

        // the same piston pushes an observer along: it lights at once on landing
        const Vector3i second(8, 64, 8);
        fixture.mLevel.setBlock(second, state, false);
        fixture.observer(Vector3i(9, 64, 8), "up");
        fixture.settle();
        fixture.mLevel.setBlock(Vector3i(7, 64, 8), fixture.block("minecraft:redstone_block"), false);
        PistonSystem::onRedstoneUpdate(fixture.mOwner, fixture.mLevel, second,
                                       fixture.mLevel.getBlockState(second.x, second.y, second.z));
        const Vector3i landed(10, 64, 8);
        bool arrived = false;
        for (int tick = 0; tick < 6 && !arrived; ++tick) {
            fixture.step();
            arrived = fixture.mLevel.getBlockState(landed.x, landed.y, landed.z).mName == "minecraft:observer";
        }
        require(arrived, "the piston did not move the observer");
        require(fixture.lit(landed), "an observer a piston moved did not light on landing");
        fixture.settle();
        require(!fixture.lit(landed), "a moved observer never went dark again");
    }

    // the second observer watches the first: its pulse follows the first one's
    void chain(Fixture &fixture) {
        const Vector3i first(12, 64, 12);
        const Vector3i second(12, 64, 13);
        fixture.observer(first, "west");
        fixture.observer(second, "north");
        fixture.settle();

        fixture.mLevel.setBlock(Vector3i(11, 64, 12), fixture.block("minecraft:stone"), false);
        const int firstLit = fixture.litWithin(first, 4);
        require(firstLit == 2, "the first observer did not light after two ticks");
        int secondLit = 0;
        for (int tick = 1; tick <= 4 && secondLit == 0; ++tick) {
            fixture.step();
            if (fixture.lit(second))
                secondLit = tick;
        }
        require(secondLit == 2, "the second observer did not follow the first two ticks later, after "
                                        + std::to_string(secondLit));
        fixture.settle();
    }

    // two observers facing each other keep pulsing each other
    void clock(Fixture &fixture) {
        const Vector3i first(2, 64, 12);
        const Vector3i second(3, 64, 12);
        fixture.observer(first, "east");
        fixture.observer(second, "west");
        fixture.mLevel.setBlock(first, fixture.mLevel.getBlockState(first.x, first.y, first.z), false);
        // start it: a change in front of the first
        BlockState flipped = fixture.mLevel.getBlockState(second.x, second.y, second.z);
        flipped.mStates.putByte("powered_bit", 1);
        fixture.mLevel.setBlock(second, flipped, false);
        fixture.mLevel.scheduleUpdate(second, 2);

        int changes = 0;
        bool last = fixture.lit(first);
        for (int tick = 0; tick < 60; ++tick) {
            fixture.step();
            if (fixture.lit(first) != last) {
                last = !last;
                ++changes;
            }
        }
        require(changes >= 10, "two facing observers did not keep clocking, " + std::to_string(changes)
                                       + " changes in 60 ticks");
    }

    // a piston takes power only from its own neighbours: none comes through the block above it
    void noQuasiConnectivity(Fixture &fixture) {
        const Vector3i piston(14, 64, 4);
        BlockState state = fixture.block("minecraft:piston");
        state.mStates.putInt("facing_direction", 1);
        fixture.mLevel.setBlock(piston, state, false);
        // powers the cell above the piston, never the piston itself
        fixture.mLevel.setBlock(Vector3i(14, 65, 5), fixture.block("minecraft:redstone_block"), false);
        fixture.settle();
        require(!PistonSystem::isGettingPower(fixture.mOwner, fixture.mLevel, piston, state),
                "a piston took power through the block above it");
        fixture.mLevel.setBlock(Vector3i(14, 64, 5), fixture.block("minecraft:redstone_block"), false);
        fixture.settle();
        require(PistonSystem::isGettingPower(fixture.mOwner, fixture.mLevel, piston, state),
                "a piston beside a redstone block took no power");
    }
}

int main() {
    int result = 0;
    try {
        Fixture fixture;
        cropGrowth(fixture);
        doorAndSides(fixture);
        pistonPush(fixture);
        chain(fixture);
        clock(fixture);
        noQuasiConnectivity(fixture);
    } catch (const std::exception &error) {
        std::cout << "Observer regression failed: " << error.what() << '\n';
        result = 1;
    }
    return result;
}
