#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
#include "World/Level/Block/Actor/HopperBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const Vector3i SOURCE(8, 100, 8);
    const int OFFSETS[6][3] = {{0, -1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {-1, 0, 0}, {1, 0, 0}};

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Fixture {
        ServerNetworkHandler mOwner{"Hopper transfers", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        HopperBlockActor *mSource = nullptr;
        Vector3i mTarget;

        explicit Fixture(int facing) {
            mLevel.insertChunk(LevelChunk(0, 0));
            BlockState state = VanillaBlocks::HOPPER().toBlockState();
            state.mStates.putInt("facing_direction", facing);
            state.mStates.putByte("toggle_bit", 0);
            mLevel.setBlock(SOURCE, state, false);
            mSource = &mLevel.getBlockActors().getOrCreate<HopperBlockActor>(SOURCE);
            mSource->mTransferCooldown = 0;
            mTarget = Vector3i(SOURCE.x + OFFSETS[facing][0], SOURCE.y + OFFSETS[facing][1],
                               SOURCE.z + OFFSETS[facing][2]);
        }

        void load(const char *identifier, int count = 2) {
            mSource->getInventory().setContainerItem(0, mOwner.createItemStack(identifier, count));
        }

        void tick() {
            mSource->mTransferCooldown = 0;
            mSource->tick(mOwner);
        }

        int remaining() const {
            const ItemStack &item = mSource->getInventory().getContainerItem(0);
            return item.isAir() ? 0 : item.mCount;
        }
    };

    void horizontal() {
        for (int facing = 2; facing < 6; ++facing) {
            Fixture fixture(facing);
            fixture.mLevel.setBlock(fixture.mTarget, VanillaBlocks::HOPPER().toBlockState(), false);
            HopperBlockActor &target = fixture.mLevel.getBlockActors().getOrCreate<HopperBlockActor>(fixture.mTarget);
            fixture.load("minecraft:dirt");
            fixture.tick();
            require(fixture.remaining() == 1 && target.getInventory().getContainerItem(0).mCount == 1,
                    "a horizontal hopper did not move exactly one item, facing " + std::to_string(facing));
            require(fixture.mSource->mTransferCooldown == HopperBlockActor::COOLDOWN_TICKS,
                    "a successful transfer did not start the source cooldown");
            fixture.mSource->tick(fixture.mOwner);
            require(fixture.remaining() == 1, "the source transferred during its cooldown");

            for (int slot = 0; slot < HopperBlockActor::SIZE; ++slot)
                target.getInventory().setContainerItem(slot, fixture.mOwner.createItemStack("minecraft:dirt", 64));
            fixture.tick();
            require(fixture.remaining() == 1, "a full destination consumed an item");
            target.getInventory().clear();
            BlockState locked = fixture.mLevel.getBlockState(SOURCE.x, SOURCE.y, SOURCE.z);
            locked.mStates.putByte("toggle_bit", 1);
            fixture.mLevel.setBlockState(SOURCE.x, SOURCE.y, SOURCE.z, locked);
            fixture.tick();
            require(fixture.remaining() == 1 && target.getInventory().isEmpty(),
                    "a powered source transferred an item");
        }
    }

    void fuel() {
        for (const char *identifier: {"minecraft:furnace", "minecraft:blast_furnace", "minecraft:smoker"}) {
            Fixture fixture(5);
            fixture.mLevel.setBlock(fixture.mTarget, VanillaBlocks::fromIdentifier(identifier)->toBlockState(), false);
            FurnaceBlockActor &target = fixture.mLevel.getBlockActors().getOrCreate<FurnaceBlockActor>(fixture.mTarget);
            fixture.load("minecraft:dirt");
            fixture.tick();
            require(fixture.remaining() == 2 && target.getInventory().getContainerItem(FurnaceInventory::SLOT_FUEL).isAir(),
                    std::string(identifier) + " accepted dirt as fuel");
            fixture.load("minecraft:coal");
            fixture.tick();
            require(fixture.remaining() == 1 && target.getInventory().getContainerItem(FurnaceInventory::SLOT_FUEL).mCount == 1,
                    std::string(identifier) + " rejected coal from its side");
            fixture.tick();
            require(fixture.remaining() == 0 && target.getInventory().getContainerItem(FurnaceInventory::SLOT_FUEL).mCount == 2,
                    "a second coal did not stack in the fuel slot");
            target.getInventory().setContainerItem(FurnaceInventory::SLOT_FUEL,
                                                   fixture.mOwner.createItemStack("minecraft:coal", 64));
            fixture.load("minecraft:coal");
            fixture.tick();
            require(fixture.remaining() == 2, "a full fuel slot consumed coal from the hopper");
            target.getInventory().clear();
            fixture.mSource->getInventory().clear();
            fixture.load("minecraft:dirt");
            fixture.mSource->getInventory().setContainerItem(1, fixture.mOwner.createItemStack("minecraft:coal", 2));
            fixture.tick();
            require(fixture.remaining() == 2
                            && fixture.mSource->getInventory().getContainerItem(1).mCount == 1
                            && target.getInventory().getContainerItem(FurnaceInventory::SLOT_FUEL).mCount == 1,
                    "an incompatible first stack blocked a later fuel stack");
            target.getInventory().clear();
            fixture.mSource->getInventory().clear();
            fixture.load("minecraft:lava_bucket", 1);
            fixture.tick();
            require(fixture.remaining() == 0
                            && target.getInventory().getContainerItem(FurnaceInventory::SLOT_FUEL).mDefinition->getIdentifier()
                                       == "minecraft:lava_bucket",
                    "a lava bucket was rejected as fuel");
            require(target.getInventory().getContainerItem(FurnaceInventory::SLOT_INPUT).isAir()
                            && target.getInventory().getContainerItem(FurnaceInventory::SLOT_OUTPUT).isAir(),
                    "a lateral transfer wrote outside the fuel slot");
        }

        Fixture above(0);
        above.mLevel.setBlock(above.mTarget, VanillaBlocks::FURNACE().toBlockState(), false);
        FurnaceBlockActor &target = above.mLevel.getBlockActors().getOrCreate<FurnaceBlockActor>(above.mTarget);
        above.load("minecraft:dirt");
        above.tick();
        require(above.remaining() == 1 && target.getInventory().getContainerItem(FurnaceInventory::SLOT_INPUT).mCount == 1,
                "the fuel filter rejected a top insertion into the input slot");
    }

    int composterLevel(Level &level, const Vector3i &position) {
        return level.getBlockState(position.x, position.y, position.z).mStates.getInt("composter_fill_level", -1);
    }

    void composter() {
        const Vector3i top(SOURCE.x, SOURCE.y + 1, SOURCE.z);
        for (const int fill: {7, 8}) {
            Fixture fixture(0);
            BlockState state = VanillaBlocks::COMPOSTER().toBlockState();
            state.mStates.putInt("composter_fill_level", fill);
            fixture.mLevel.setBlock(top, state, false);
            fixture.tick();
            const ItemStack &pulled = fixture.mSource->getInventory().getContainerItem(0);
            if (fill == 7) {
                require(pulled.isAir() && composterLevel(fixture.mLevel, top) == 7,
                        "a composter that was not ready gave bone meal");
                continue;
            }
            require(!pulled.isAir() && pulled.mDefinition->getIdentifier() == "minecraft:bone_meal"
                            && pulled.mCount == 1,
                    "a ready composter did not give one bone meal");
            require(composterLevel(fixture.mLevel, top) == 0, "the composter was not emptied");
        }

        Fixture full(0);
        BlockState ready = VanillaBlocks::COMPOSTER().toBlockState();
        ready.mStates.putInt("composter_fill_level", 8);
        full.mLevel.setBlock(top, ready, false);
        for (int slot = 0; slot < HopperBlockActor::SIZE; ++slot)
            full.mSource->getInventory().setContainerItem(slot, full.mOwner.createItemStack("minecraft:dirt", 64));
        full.tick();
        require(composterLevel(full.mLevel, top) == 8, "a full hopper emptied the composter");
    }
}

int main(int argc, char **argv) {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-hopper-transfers-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        require(argc == 2, "expected horizontal, fuel or composter");
        if (std::string(argv[1]) == "horizontal")
            horizontal();
        else if (std::string(argv[1]) == "fuel")
            fuel();
        else if (std::string(argv[1]) == "composter")
            composter();
        else
            throw std::runtime_error("unknown test case");
    } catch (const std::exception &error) {
        std::clog << "Hopper transfer regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
