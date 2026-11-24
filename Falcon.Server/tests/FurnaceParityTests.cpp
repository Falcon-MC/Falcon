#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Inventory/InventoryManager.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/FurnaceBlockActor.h"
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
    const Vector3i AT(8, 100, 8);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Fixture {
        ServerNetworkHandler mOwner{"Furnace parity", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        FurnaceBlockActor *mFurnace = nullptr;

        explicit Fixture(FurnaceKind kind = FurnaceKind::Furnace) {
            mLevel.insertChunk(LevelChunk(0, 0));
            const char *identifier = kind == FurnaceKind::BlastFurnace ? "minecraft:blast_furnace"
                                      : kind == FurnaceKind::Smoker ? "minecraft:smoker"
                                                                    : "minecraft:furnace";
            mLevel.setBlock(AT, VanillaBlocks::fromIdentifier(identifier)->toBlockState(), false);
            mFurnace = &mLevel.getBlockActors().getOrCreate<FurnaceBlockActor>(AT);
            mFurnace->mKind = kind;
        }

        void load(const char *input, const char *fuel = "minecraft:coal", int count = 1) {
            mFurnace->getInventory().setContainerItem(0, mOwner.createItemStack(input, count));
            mFurnace->getInventory().setContainerItem(1, mOwner.createItemStack(fuel, 1));
        }

        void tick(int count = 1) {
            for (int tick = 0; tick < count; ++tick)
                mFurnace->tick(mOwner);
        }

        int outputCount() const {
            const ItemStack &item = mFurnace->getInventory().getContainerItem(2);
            return item.isAir() ? 0 : item.mCount;
        }
    };

    void fuels() {
        Fixture fixture;
        for (const char *family: {"crimson", "warped"}) {
            for (const char *part: {"planks", "stem", "hyphae", "slab", "stairs", "door", "trapdoor",
                                    "button", "pressure_plate", "fence", "fence_gate"}) {
                const std::string identifier = std::string("minecraft:") + family + "_" + part;
                ItemStack item = fixture.mOwner.createItemStack(identifier, 1);
                require(!item.isAir(), "missing test item " + identifier);
                require(InventoryManager::getFuelTime(item) == 0,
                        identifier + " was accepted as fuel");
                require(!fixture.mFurnace->getInventory().canHoldInSlot(1, item),
                        "the fuel slot accepted " + identifier);
            }
        }
        fixture.load("minecraft:beef", "minecraft:warped_planks");
        fixture.tick(200);
        require(fixture.outputCount() == 0 && fixture.mFurnace->mBurnTime == 0
                        && !fixture.mFurnace->getInventory().getContainerItem(1).isAir(),
                "a furnace consumed nether wood or cooked with it");
        require(InventoryManager::getFuelTime(fixture.mOwner.createItemStack("minecraft:oak_planks", 1)) == 300,
                "ordinary planks stopped burning");
        require(InventoryManager::getFuelTime(fixture.mOwner.createItemStack("minecraft:coal", 1)) == 1600,
                "coal's normal duration changed");
    }

    void plantFuels() {
        Fixture fixture;
        struct Fuel { const char *mIdentifier; int mTicks; };
        const Fuel fuels[] = {{"minecraft:scaffolding", 50}, {"minecraft:ladder", 300},
                              {"minecraft:mangrove_propagule", 100}, {"minecraft:azalea", 100},
                              {"minecraft:flowering_azalea", 100}, {"minecraft:deadbush", 100},
                              {"minecraft:leaf_litter", 100}, {"minecraft:short_dry_grass", 100},
                              {"minecraft:tall_dry_grass", 100}};
        for (const Fuel &fuel: fuels) {
            ItemStack item = fixture.mOwner.createItemStack(fuel.mIdentifier, 1);
            require(!item.isAir(), std::string("missing test fuel ") + fuel.mIdentifier);
            require(InventoryManager::getFuelTime(item) == fuel.mTicks,
                    std::string(fuel.mIdentifier) + " has the wrong burn duration");
            require(fixture.mFurnace->getInventory().canHoldInSlot(1, item), "the fuel slot rejected a valid fuel");
        }
        fixture.load("minecraft:beef", "minecraft:scaffolding");
        fixture.tick();
        require(fixture.mFurnace->mMaxBurnTime == 50 && fixture.mFurnace->mCookTime == 1,
                "scaffolding did not start a 50-tick burn");
    }


}

int main(int argc, char **argv) {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-furnace-parity-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        require(argc == 2, "expected fuels or plants");
        const std::string scenario = argv[1];
        if (scenario == "fuels")
            fuels();
        else if (scenario == "plants")
            plantFuels();
        else
            throw std::runtime_error("unknown test case");
    } catch (const std::exception &error) {
        std::clog << "Furnace parity regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
