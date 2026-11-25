#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginServerApi.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
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

    struct Fixture {
        ServerNetworkHandler mOwner{"Brewing stand", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();
        BrewingStandBlockActor *mStand = nullptr;

        Fixture() {
            mLevel.insertChunk(LevelChunk(0, 0));
            mLevel.setBlock(AT, VanillaBlocks::fromIdentifier("minecraft:brewing_stand")->toBlockState(), false);
            mStand = &mLevel.getBlockActors().getOrCreate<BrewingStandBlockActor>(AT);
            set(BrewingStandBlockActor::SLOT_INGREDIENT, "minecraft:nether_wart", 2);
            set(BrewingStandBlockActor::SLOT_FUEL, "minecraft:blaze_powder", 1);
            for (int slot = 1; slot <= 3; ++slot)
                set(slot, "minecraft:potion", 1);
            const BrewingMixData *mix = CraftingRecipeTable::findBrewingMix("minecraft:potion", 0,
                                                                            "minecraft:nether_wart", 0);
            require(mix != nullptr && mix->mOutputMeta == 4, "water and nether wart recipe is missing");
        }

        void set(int slot, const char *identifier, int count) {
            mStand->getInventory().setContainerItem(slot, mOwner.createItemStack(identifier, count));
        }

        void tick(int count = 1) {
            for (int tick = 0; tick < count; ++tick)
                mStand->tick(mOwner);
        }

        int time() const {
            return mStand->saveNbt().getShort("CookTime");
        }

        int fuel() const {
            return mStand->saveNbt().getShort("FuelAmount");
        }

        void loadTime(int ticks) {
            Tag saved = mStand->saveNbt();
            saved.putShort("CookTime", (int16_t) ticks);
            mStand->loadNbt(saved, mOwner.getCodecContext());
        }

        void expectPotions(int meta) const {
            for (int slot = 1; slot <= 3; ++slot) {
                const ItemStack &potion = mStand->getInventory().getContainerItem(slot);
                require(!potion.isAir() && potion.mDefinition->getIdentifier() == "minecraft:potion"
                                && potion.mDamage == meta && potion.mCount == 1,
                        "unexpected potion in slot " + std::to_string(slot));
            }
        }
    };

    void idleSave() {
        Fixture fixture;
        fixture.loadTime(0);
        fixture.tick();
        fixture.expectPotions(0);
        require(fixture.time() == 400 && fixture.fuel() == 20,
                "a vanilla idle save did not start a full brew without spending fuel");

        Fixture fresh;
        require(fresh.time() == 0, "an idle stand was saved with an active countdown");
        Tag saved = fresh.mStand->saveNbt();
        saved.remove("CookTime");
        fresh.mStand->loadNbt(saved, fresh.mOwner.getCodecContext());
        fresh.tick();
        require(fresh.time() == 400, "a save without CookTime did not start a full brew");
        fresh.expectPotions(0);

        Fixture resumed;
        resumed.loadTime(200);
        resumed.tick();
        require(resumed.time() == 199, "a saved running brew lost its progress on load");
        resumed.tick(199);
        resumed.expectPotions(4);
        require(resumed.time() == 0 && resumed.fuel() == 19, "a restored brew did not finish once");
    }

    void ingredientChange() {
        Fixture fixture;
        fixture.loadTime(200);
        fixture.set(BrewingStandBlockActor::SLOT_INGREDIENT, "minecraft:sugar", 2);
        fixture.tick();
        require(fixture.time() == 0 && fixture.fuel() == 20,
                "changing to another valid reagent kept the previous brew's progress");
        fixture.expectPotions(0);
        fixture.tick();
        require(fixture.time() == 400, "a changed reagent did not start its own full countdown");
        fixture.tick(399);
        fixture.expectPotions(0);
        fixture.tick();
        fixture.expectPotions(1);
        require(fixture.fuel() == 19, "restarting a brew spent fuel more than once");

        Fixture sameType;
        sameType.loadTime(200);
        ItemStack ingredient = sameType.mStand->getInventory().getContainerItem(0);
        ingredient.mCount = 3;
        ingredient.mTag = Tag::ofCompound();
        ingredient.mTag.putString("test", "same reagent");
        sameType.mStand->getInventory().setContainerItem(0, ingredient);
        sameType.tick();
        require(sameType.time() == 199, "changing only a reagent's count or tag restarted the brew");

        sameType.mStand->getInventory().setContainerItem(0, ItemStack::air());
        sameType.tick();
        require(sameType.time() == 0 && sameType.fuel() == 20, "removing the reagent did not cancel the brew");

        Fixture completing;
        completing.loadTime(1);
        completing.set(0, "minecraft:sugar", 2);
        completing.tick();
        completing.expectPotions(1);
        require(completing.time() == 0, "the completing tick did not use the current valid recipe");
    }

    void countdown() {
        Fixture fixture;
        fixture.tick();
        require(fixture.time() == 400, "starting a brew decremented its countdown on the same tick");
        require(fixture.mStand->getSpawnCompound().getShort("CookTime") == 400,
                "the client spawn data omitted a newly started brew");
        fixture.tick(399);
        require(fixture.time() == 1 && fixture.fuel() == 20, "a brew finished before its 400 countdown ticks");
        fixture.expectPotions(0);
        fixture.tick();
        fixture.expectPotions(4);
        require(fixture.time() == 0 && fixture.fuel() == 19
                        && fixture.mStand->getInventory().getContainerItem(0).mCount == 1,
                "a batch did not consume exactly one ingredient and fuel charge");
        fixture.tick(20);
        require(fixture.time() == 0 && fixture.fuel() == 19, "a stand with no valid mix kept brewing");

        Fixture noFuel;
        noFuel.mStand->getInventory().setContainerItem(BrewingStandBlockActor::SLOT_FUEL, ItemStack::air());
        noFuel.tick();
        require(noFuel.time() == 0, "an unfuelled stand started a countdown");
        noFuel.expectPotions(0);
    }

    void pluginEvents() {
        Fixture fixture;
        BeforeEvents &before = fixture.mOwner.getEventBus().before();
        const FalconServerApi &api = PluginServerApi::get();
        LoadedPlugin listener;
        listener.mDescription.mName = "Brewing fixture";
        listener.mEnabled = true;
        const auto fuel = api.subscribe(
                PluginManager::toHandle(listener), FALCON_EVENT_BREWING_FUEL_USE, FALCON_PRIORITY_NORMAL, 0,
                [](FalconEvent *event, void *) { PluginServerApi::get().eventSetAmount(event, 5.0); }, nullptr);
        const auto potion = api.subscribe(
                PluginManager::toHandle(listener), FALCON_EVENT_BREW_ITEM, FALCON_PRIORITY_NORMAL, 0,
                [](FalconEvent *event, void *) {
                    if (PluginServerApi::get().eventSourceSlot(event) == 2)
                        PluginServerApi::get().eventSetCancelled(event, 1);
                },
                nullptr);
        require(before.mBrewingFuelUse.hasSubscribers() && before.mBrewItem.hasSubscribers()
                        && !before.mBrew.hasSubscribers(),
                "the plugin layer did not hook exactly the brewing events a plugin listens to");

        fixture.tick();
        require(fixture.fuel() == 5, "the stand ignored the plugin's fuel time");
        fixture.tick(400);
        const auto meta = [&](int slot) { return fixture.mStand->getInventory().getContainerItem(slot).mDamage; };
        require(meta(1) == 4 && meta(2) == 0 && meta(3) == 4, "a cancelled potion slot still brewed");
        require(fixture.fuel() == 4, "a partly cancelled brew did not spend one fuel charge");

        api.unsubscribe(fuel);
        api.unsubscribe(potion);
        require(!before.mBrewingFuelUse.hasSubscribers() && !before.mBrewItem.hasSubscribers(),
                "the plugin layer stayed hooked with no plugin listening");
    }
}

int main(int argc, char **argv) {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-brewing-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        require(argc == 2, "expected idle-save, ingredient-change, countdown or plugin-events");
        const std::string scenario = argv[1];
        if (scenario == "idle-save")
            idleSave();
        else if (scenario == "ingredient-change")
            ingredientChange();
        else if (scenario == "countdown")
            countdown();
        else if (scenario == "plugin-events")
            pluginEvents();
        else
            throw std::runtime_error("unknown test case");
    } catch (const std::exception &error) {
        std::clog << "Brewing regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
