#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Inventory/ItemStackNbt.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ContainerBlockActor.h"
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

    std::string key(const char *prefix, int slot) {
        return std::string(prefix) + std::to_string(slot + 1);
    }

    struct Fixture {
        ServerNetworkHandler mOwner{"Campfire", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        CampfireBlockActor *mCampfire = nullptr;

        explicit Fixture(bool soul = false) {
            mLevel.insertChunk(LevelChunk(0, 0));
            BlockState state = VanillaBlocks::fromIdentifier(soul ? "minecraft:soul_campfire" : "minecraft:campfire")
                                       ->toBlockState();
            state.mStates.putByte("extinguished", 0);
            mLevel.setBlock(AT, state, false);
            mCampfire = &mLevel.getBlockActors().getOrCreate<CampfireBlockActor>(AT);
            const FurnaceRecipeData *recipe = CraftingRecipeTable::findCampfireRecipe("minecraft:beef", 0, soul);
            require(recipe != nullptr && std::string(recipe->mOutputItemId) == "minecraft:cooked_beef",
                    "the campfire beef recipe is missing");
        }

        ItemStack food(int slot = 0) {
            ItemStack item = mOwner.createItemStack("minecraft:beef", 1);
            item.mTag = Tag::ofCompound();
            item.mTag.putInt("testSlot", slot);
            return item;
        }

        void tick(int count = 1) {
            for (int tick = 0; tick < count; ++tick)
                mCampfire->tick(mOwner);
        }

        void setLit(bool lit) {
            BlockState state = mLevel.getBlockState(AT.x, AT.y, AT.z);
            state.mStates.putByte("extinguished", lit ? 0 : 1);
            mLevel.setBlockState(AT.x, AT.y, AT.z, state);
        }

        int progress(int slot = 0) const {
            return mCampfire->saveNbt().getInt(key("ItemTime", slot));
        }

        int cookedDrops() {
            int count = 0;
            for (const auto &actor: mOwner.getItemEntities()) {
                if (actor->isRemoved())
                    continue;
                const ItemStack &item = actor->getItem();
                require(!item.isAir() && item.mDefinition->getIdentifier() == "minecraft:cooked_beef",
                        "the campfire dropped an unexpected item");
                count += item.mCount;
            }
            return count;
        }
    };

    void nativeSave() {
        Fixture fixture;
        Tag native = Tag::ofCompound();
        for (int slot = 0; slot < 4; ++slot) {
            native.put(key("Item", slot), ItemStackNbt::write(fixture.food(slot)));
            native.putInt(key("ItemTime", slot), slot * 100);
        }
        fixture.mCampfire->loadNbt(native, fixture.mOwner.getCodecContext());
        for (int slot = 0; slot < 4; ++slot) {
            const ItemStack &item = fixture.mCampfire->getInventory().getContainerItem(slot);
            require(!item.isAir() && item.mTag.getInt("testSlot") == slot,
                    "a native campfire save lost food from slot " + std::to_string(slot));
            require(fixture.progress(slot) == slot * 100, "a native cooking progress changed on load");
        }

        const Tag saved = fixture.mCampfire->saveNbt();
        require(!saved.contains("Items"), "a campfire was saved as a generic container");
        CampfireBlockActor restored;
        restored.loadNbt(saved, fixture.mOwner.getCodecContext());
        for (int slot = 0; slot < 4; ++slot) {
            require(saved.contains(key("Item", slot), Tag::Type::Compound), "a native food compound was not saved");
            require(!restored.getInventory().getContainerItem(slot).isAir()
                            && restored.getInventory().getContainerItem(slot).mTag.getInt("testSlot") == slot,
                    "food did not survive a native save round trip");
        }
        fixture.tick(299);
        require(fixture.cookedDrops() == 0 && fixture.progress(3) == 599,
                "a native saved progress was treated as remaining ticks");
        fixture.tick();
        require(fixture.cookedDrops() == 1 && fixture.mCampfire->getInventory().getContainerItem(3).isAir(),
                "a native food did not finish after its remaining 300 ticks");
        for (int slot = 0; slot < 3; ++slot)
            require(!fixture.mCampfire->getInventory().getContainerItem(slot).isAir(),
                    "another native food finished too early");
        restored.loadNbt(Tag::ofCompound(), fixture.mOwner.getCodecContext());
        for (int slot = 0; slot < 4; ++slot)
            require(restored.getInventory().getContainerItem(slot).isAir(), "loading an empty save kept stale food");

        Fixture legacy;
        Tag old = Tag::ofCompound();
        Tag items = Tag::ofList(Tag::Type::Compound);
        items.addToList(ItemStackNbt::write(legacy.food(9), 0));
        old.put("Items", items);
        old.putInt("ItemTime1", 450);
        legacy.mCampfire->loadNbt(old, legacy.mOwner.getCodecContext());
        require(!legacy.mCampfire->getInventory().getContainerItem(0).isAir() && legacy.progress() == 150,
                "an older Falcon save lost its food or remaining cook time");
        require(legacy.progress(1) == 0, "an empty legacy slot acquired cooking progress");
        legacy.tick(449);
        require(legacy.cookedDrops() == 0, "a migrated food cooked too early");
        legacy.tick();
        require(legacy.cookedDrops() == 1, "a migrated food did not finish after its saved remaining time");

        native.put("Items", items);
        fixture.mCampfire->loadNbt(native, fixture.mOwner.getCodecContext());
        require(fixture.mCampfire->getInventory().getContainerItem(0).mTag.getInt("testSlot") == 0,
                "a legacy Items list overrode the native campfire fields");
    }

    void cooking() {
        for (bool soul: {false, true}) {
            Fixture fixture(soul);
            for (int slot = 0; slot < 4; ++slot) {
                require(fixture.mCampfire->addFood(fixture.food(slot)), "a campfire refused food in a free slot");
                require(fixture.progress(slot) == 0, "new food did not start with zero elapsed cooking ticks");
            }
            require(!fixture.mCampfire->addFood(fixture.food()), "a full campfire accepted a fifth food");
            fixture.tick(599);
            require(fixture.cookedDrops() == 0, "food cooked before 600 lit ticks");
            for (int slot = 0; slot < 4; ++slot)
                require(fixture.progress(slot) == 599, "elapsed cooking ticks did not count up");
            fixture.tick();
            require(fixture.cookedDrops() == 4, "the campfire did not cook all four foods on tick 600");
            for (int slot = 0; slot < 4; ++slot)
                require(fixture.mCampfire->getInventory().getContainerItem(slot).isAir() && fixture.progress(slot) == 0,
                        "a cooked slot was not cleared");
            fixture.tick(10);
            require(fixture.cookedDrops() == 4, "an empty campfire dropped more food");
        }
    }

    void extinguished() {
        for (bool soul: {false, true}) {
            Fixture fixture(soul);
            require(fixture.mCampfire->addFood(fixture.food()), "a campfire refused raw beef");
            fixture.tick(599);
            fixture.setLit(false);
            fixture.tick();
            require(fixture.progress() == 0 && fixture.cookedDrops() == 0,
                    "extinguishing did not clear the cooking progress without cooking the food");
            fixture.tick(20);
            require(!fixture.mCampfire->getInventory().getContainerItem(0).isAir()
                            && fixture.progress() == 0 && fixture.cookedDrops() == 0,
                    "an extinguished campfire cooked its food");
            fixture.setLit(true);
            fixture.tick(599);
            require(fixture.cookedDrops() == 0, "relighting kept the previous cooking progress");
            fixture.tick();
            require(fixture.cookedDrops() == 1, "relit food did not cook after 600 new lit ticks");
        }

        Fixture ready;
        Tag old = Tag::ofCompound();
        Tag items = Tag::ofList(Tag::Type::Compound);
        items.addToList(ItemStackNbt::write(ready.food(), 0));
        old.put("Items", items);
        old.putInt("ItemTime1", 0);
        ready.mCampfire->loadNbt(old, ready.mOwner.getCodecContext());
        ready.setLit(false);
        ready.tick();
        require(ready.cookedDrops() == 0 && !ready.mCampfire->getInventory().getContainerItem(0).isAir(),
                "a completed legacy timer cooked on an extinguished campfire");
    }
}

int main(int argc, char **argv) {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-campfire-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        require(argc == 2, "expected native-save, cooking or extinguished");
        const std::string scenario = argv[1];
        if (scenario == "native-save")
            nativeSave();
        else if (scenario == "cooking")
            cooking();
        else if (scenario == "extinguished")
            extinguished();
        else
            throw std::runtime_error("unknown test case");
    } catch (const std::exception &error) {
        std::clog << "Campfire regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
