#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/Block.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/DetectorRailBlock.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/Container.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i ORIGIN(8, 64, 8);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState withInt(Block block, const char *key, int value) {
        BlockState state = block.toBlockState();
        state.mStates.putInt(key, value);
        return state;
    }

    struct RedstoneFixture {
        ServerNetworkHandler mOwner{"Redstone", "", 1};
        World &mWorld = mOwner.getWorlds().getDefault();
        Level &mLevel;

        RedstoneFixture() : mLevel(mWorld.getLevel(DimensionType::Overworld)) {
            mLevel.getChunk(0, 0);
            for (int x = 0; x < 16; ++x) {
                for (int z = 0; z < 16; ++z) {
                    for (int y = 60; y <= 72; ++y)
                        mLevel.setBlockState(x, y, z, BlockState(BlockNames::AIR));
                    mLevel.setBlockState(x, 63, z, VanillaBlocks::STONE().toBlockState());
                }
            }
        }

        void set(const Vector3i &position, const BlockState &state) {
            mLevel.setBlockState(position.x, position.y, position.z, state);
        }

        BlockState at(const Vector3i &position) {
            return mLevel.getBlockState(position.x, position.y, position.z);
        }

        int signal(const Vector3i &position) {
            const BlockState state = at(position);
            const Block *block = VanillaBlocks::fromIdentifier(state.mName);
            require(block != nullptr, state.mName + " has no block class");
            return block->getComparatorSignal(mLevel, position, state);
        }
    };

    void stateSignals() {
        RedstoneFixture fixture;

        const int cake[7] = {14, 12, 10, 8, 6, 4, 2};
        for (int bites = 0; bites <= 6; ++bites) {
            fixture.set(ORIGIN, withInt(VanillaBlocks::CAKE(), "bite_counter", bites));
            require(fixture.signal(ORIGIN) == cake[bites], "a cake did not lose two per slice");
        }

        for (int level = 0; level <= 8; ++level) {
            fixture.set(ORIGIN, withInt(VanillaBlocks::COMPOSTER(), "composter_fill_level", level));
            require(fixture.signal(ORIGIN) == level, "a composter did not give its fill level");
        }

        const int cauldron[7] = {0, 1, 1, 2, 2, 3, 3};
        for (int level = 0; level <= 6; ++level) {
            fixture.set(ORIGIN, withInt(VanillaBlocks::CAULDRON(), "fill_level", level));
            require(fixture.signal(ORIGIN) == cauldron[level], "a cauldron did not give half its fill level");
        }

        const int anchor[5] = {0, 3, 7, 11, 15};
        for (int charge = 0; charge <= 4; ++charge) {
            fixture.set(ORIGIN, withInt(VanillaBlocks::RESPAWN_ANCHOR(), "respawn_anchor_charge", charge));
            require(fixture.signal(ORIGIN) == anchor[charge], "a respawn anchor did not scale its charge to 15");
        }

        BlockState frame = VanillaBlocks::END_PORTAL_FRAME().toBlockState();
        frame.mStates.putByte("end_portal_eye_bit", 0);
        fixture.set(ORIGIN, frame);
        require(fixture.signal(ORIGIN) == 0, "an empty end portal frame gave a signal");
        frame.mStates.putByte("end_portal_eye_bit", 1);
        fixture.set(ORIGIN, frame);
        require(fixture.signal(ORIGIN) == 15, "an end portal frame with an eye did not give 15");

        fixture.set(ORIGIN, VanillaBlocks::STONE().toBlockState());
        require(fixture.signal(ORIGIN) == -1, "stone gave a comparator signal");
    }

    void containerSignals() {
        RedstoneFixture fixture;

        fixture.set(ORIGIN, VanillaBlocks::HOPPER().toBlockState());
        HopperBlockActor &hopper = fixture.mLevel.getBlockActors().getOrCreate<HopperBlockActor>(ORIGIN);
        require(fixture.signal(ORIGIN) == 0, "an empty hopper gave a signal");

        hopper.getContainer()->setContainerItem(0, fixture.mOwner.createItemStack("minecraft:dirt", 1));
        require(fixture.signal(ORIGIN) == 1, "one item in a hopper did not give 1");

        hopper.getContainer()->setContainerItem(0, fixture.mOwner.createItemStack("minecraft:dirt", 64));
        require(fixture.signal(ORIGIN) == 3, "a full stack in one of five hopper slots did not give 3");

        for (int slot = 0; slot < 5; ++slot)
            hopper.getContainer()->setContainerItem(slot, fixture.mOwner.createItemStack("minecraft:dirt", 64));
        require(fixture.signal(ORIGIN) == 15, "a full hopper did not give 15");

        const Vector3i chestPosition(ORIGIN.x + 2, ORIGIN.y, ORIGIN.z);
        fixture.set(chestPosition, VanillaBlocks::CHEST().toBlockState());
        ChestBlockActor &chest = fixture.mLevel.getBlockActors().getOrCreate<ChestBlockActor>(chestPosition);
        chest.getContainer()->setContainerItem(0, fixture.mOwner.createItemStack("minecraft:ender_pearl", 16));
        require(fixture.signal(chestPosition) == 1, "a full stack of sixteen in a chest did not give 1");
    }

    void comparatorThroughBlock() {
        RedstoneFixture fixture;

        const Vector3i solid(ORIGIN.x + 1, ORIGIN.y, ORIGIN.z);
        const Vector3i hopperPosition(ORIGIN.x + 2, ORIGIN.y, ORIGIN.z);
        fixture.set(solid, VanillaBlocks::STONE().toBlockState());
        fixture.set(hopperPosition, VanillaBlocks::HOPPER().toBlockState());
        HopperBlockActor &hopper = fixture.mLevel.getBlockActors().getOrCreate<HopperBlockActor>(hopperPosition);
        for (int slot = 0; slot < 5; ++slot)
            hopper.getContainer()->setContainerItem(slot, fixture.mOwner.createItemStack("minecraft:dirt", 64));

        BlockState comparator = VanillaBlocks::UNPOWERED_COMPARATOR().toBlockState();
        comparator.mStates.putString("minecraft:cardinal_direction", "east");
        fixture.set(ORIGIN, comparator);
        RedstoneSystem::onRedstonePlaced(fixture.mOwner, fixture.mLevel, ORIGIN, comparator);
        require(RedstoneSystem::getComparatorOutput(fixture.mLevel, ORIGIN) == 15,
                "a comparator did not read a full hopper through a solid block");

        fixture.set(solid, BlockState(BlockNames::AIR));
        RedstoneSystem::onRedstonePlaced(fixture.mOwner, fixture.mLevel, ORIGIN, comparator);
        require(RedstoneSystem::getComparatorOutput(fixture.mLevel, ORIGIN) == 0,
                "a comparator read a hopper two blocks away through air");
    }

    void hopperLock() {
        RedstoneFixture fixture;

        fixture.set(ORIGIN, VanillaBlocks::HOPPER().toBlockState());
        const Block *block = VanillaBlocks::fromIdentifier(BlockNames::HOPPER);
        require(block != nullptr, "the hopper has no block class");

        const Vector3i source(ORIGIN.x + 1, ORIGIN.y, ORIGIN.z);
        fixture.set(source, VanillaBlocks::REDSTONE_BLOCK().toBlockState());
        block->onRedstonePower(fixture.mOwner, fixture.mLevel, ORIGIN, fixture.at(ORIGIN));
        require(fixture.at(ORIGIN).mStates.getByte("toggle_bit", 0) == 1, "a powered hopper was not locked");

        fixture.set(source, BlockState(BlockNames::AIR));
        block->onRedstonePower(fixture.mOwner, fixture.mLevel, ORIGIN, fixture.at(ORIGIN));
        require(fixture.at(ORIGIN).mStates.getByte("toggle_bit", 0) == 0, "an unpowered hopper stayed locked");
    }

    void copperBulb() {
        RedstoneFixture fixture;

        fixture.set(ORIGIN, BlockState(BlockNames::COPPER_BULB));
        const Block *block = VanillaBlocks::fromIdentifier(BlockNames::COPPER_BULB);
        require(block != nullptr, "the copper bulb has no block class");

        const Vector3i source(ORIGIN.x + 1, ORIGIN.y, ORIGIN.z);
        const auto pulse = [&](bool powered) {
            fixture.set(source, powered ? VanillaBlocks::REDSTONE_BLOCK().toBlockState() : BlockState(BlockNames::AIR));
            block->onScheduledUpdate(fixture.mOwner, fixture.mLevel, ORIGIN, fixture.at(ORIGIN));
        };
        const auto lit = [&]() { return fixture.at(ORIGIN).mStates.getByte("lit", 0) != 0; };

        pulse(true);
        require(lit() && fixture.at(ORIGIN).mStates.getByte("powered_bit", 0) == 1, "a powered bulb did not light");
        require(fixture.signal(ORIGIN) == 15, "a lit bulb did not give 15");

        pulse(false);
        require(lit() && fixture.at(ORIGIN).mStates.getByte("powered_bit", 0) == 0,
                "a bulb losing power did not stay lit");

        pulse(true);
        require(!lit(), "a second pulse did not turn the bulb off");
        require(fixture.signal(ORIGIN) == 0, "an unlit bulb gave a signal");
    }

    void detectorRail() {
        RedstoneFixture fixture;

        fixture.set(ORIGIN, VanillaBlocks::DETECTOR_RAIL().toBlockState());
        const auto powered = [&]() { return fixture.at(ORIGIN).mStates.getByte("rail_data_bit", 0) != 0; };
        const auto check = [&]() {
            DetectorRailBlock::checkPressed(fixture.mOwner, fixture.mLevel, ORIGIN, fixture.at(ORIGIN));
        };

        check();
        require(!powered(), "an empty detector rail was powered");

        ServerActor *minecart = fixture.mOwner.spawnActor(fixture.mLevel, "minecraft:chest_minecart",
                                                          Vector3f(8.5f, 64.0f, 8.5f));
        require(minecart != nullptr && minecart->isMinecart(), "a chest minecart did not spawn as a minecart");
        check();
        require(powered(), "a minecart on a detector rail did not power it");

        const Block *rail = VanillaBlocks::fromIdentifier(BlockNames::DETECTOR_RAIL);
        require(rail != nullptr, "the detector rail has no block class");
        const BlockState state = fixture.at(ORIGIN);
        require(rail->getEmittedSignal(fixture.mLevel, ORIGIN, state, RedstoneFace::NORTH, false) == 15
                        && rail->getEmittedSignal(fixture.mLevel, ORIGIN, state, RedstoneFace::NORTH, true) == 0
                        && rail->getEmittedSignal(fixture.mLevel, ORIGIN, state, RedstoneFace::UP, true) == 15,
                "a powered detector rail did not give 15 around and charge only the block below");

        require(fixture.signal(ORIGIN) == 0, "an empty chest minecart gave a signal");
        minecart->getEntityContainer()->setContainerItem(0, fixture.mOwner.createItemStack("minecraft:dirt", 64));
        require(fixture.signal(ORIGIN) == 1, "a stack in a chest minecart did not give 1");

        minecart->setPosition(Vector3f(8.5f, 64.0f, 12.5f));
        rail->onScheduledUpdate(fixture.mOwner, fixture.mLevel, ORIGIN, fixture.at(ORIGIN));
        require(!powered(), "a detector rail stayed powered after the minecart left");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        stateSignals();
        containerSignals();
        comparatorThroughBlock();
        hopperLock();
        copperBulb();
        detectorRail();
        std::cout << "redstone checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
