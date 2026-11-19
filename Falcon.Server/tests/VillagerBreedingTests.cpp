#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"
#include "World/Actor/AI/Goal/FertilizeFarmBlockGoal.h"
#include "World/Actor/AI/Goal/HarvestFarmBlockGoal.h"
#include "World/Actor/AI/Goal/MakeLoveGoal.h"
#include "World/Actor/AI/Goal/ReceiveLoveGoal.h"
#include "World/Actor/AI/Goal/ShareItemsGoal.h"
#include "World/Actor/AI/Goal/WorkComposterGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Mob/Component/MobShareables.h"
#include "World/Actor/Mob/Passive/VillagerV2Actor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockShape.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Block/ComposterBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/World.h"
#include "World/Village/PoiBlueprint.h"
#include "World/Village/PoiInstance.h"
#include "World/Village/Village.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *const SCHEDULE_GROUPS[] = {"basic_schedule",
                                           "child_schedule",
                                           "work_schedule",
                                           "farmer_schedule",
                                           "fisher_schedule",
                                           "librarian_schedule",
                                           "jobless_schedule",
                                           "wander_schedule_villager",
                                           "gather_schedule_villager",
                                           "home_schedule_villager",
                                           "bed_schedule_villager",
                                           "play_schedule_villager",
                                           "work_schedule_villager",
                                           "work_schedule_farmer",
                                           "baby"};

    struct Arena {
        ServerNetworkHandler mOwner{"VillagerBreeding", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -4; x <= 4; x++) {
                for (int32_t z = -4; z <= 4; z++) {
                    LevelChunk &chunk = mLevel->insertChunk(LevelChunk(x, z));
                    chunk.setPopulated(true);
                    chunk.setBiome(BiomeIds::PLAINS);
                }
            }
        }

        void floor(int32_t minX, int32_t maxX, int32_t y, int32_t minZ, int32_t maxZ) {
            for (int32_t x = minX; x <= maxX; ++x) {
                for (int32_t z = minZ; z <= maxZ; ++z)
                    mLevel->setBlockState(x, y, z, BlockState("minecraft:stone"));
            }
        }

        // an adult with no schedule; group: its job's behaviours
        VillagerV2Actor &adult(const Vector3f &position, const char *group) {
            VillagerV2Actor *villager = dynamic_cast<VillagerV2Actor *>(
                    mOwner.spawnActor(*mLevel, "minecraft:villager_v2", position));
            require(villager != nullptr, "the villager did not spawn");
            villager->startDefinition(mOwner);
            for (const char *schedule: SCHEDULE_GROUPS)
                villager->removeComponentGroup(schedule);
            villager->addComponentGroup("adult");
            villager->addComponentGroup(group);
            villager->tick(mOwner);
            return *villager;
        }

        void give(MobActor &mob, int slot, const char *item, int32_t count) {
            mob.getEquipment().setInventoryItem(slot, mOwner.createItemStack(item, count));
        }

        int32_t countVillagers() {
            int32_t count = 0;
            for (const auto &entry: mOwner.getActors()) {
                if (dynamic_cast<VillagerV2Actor *>(entry.second.get()) != nullptr && entry.second->isAlive())
                    ++count;
            }
            return count;
        }

        std::vector<ItemActor *> items() {
            std::vector<ItemActor *> found;
            for (const std::unique_ptr<ItemActor> &item: mOwner.getItemEntities())
                found.push_back(item.get());
            return found;
        }

        void clearItems() {
            mOwner.getItemEntities().clear();
        }
    };

    BlockState stateOf(const char *name) {
        const Tag *states = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        require(states != nullptr, "a block has no default state");
        return BlockState(name, *states);
    }

    BlockState wheat(int32_t growth) {
        BlockState state = stateOf("minecraft:wheat");
        state.mStates.putInt("growth", growth);
        return state;
    }

    void foodDecidesBreeding(Arena &arena) {
        VillagerV2Actor &villager = arena.adult(Vector3f(-40.5f, 64.0f, -40.5f), "unskilled");
        require(villager.findBreedingFoodSlot() < 0, "an empty inventory counted as food");

        arena.give(villager, 0, "minecraft:carrot", 11);
        arena.give(villager, 1, "minecraft:bread", 2);
        require(villager.findBreedingFoodSlot() < 0, "11 carrots and 2 bread counted as food");

        // two stacks never add up
        arena.give(villager, 2, "minecraft:carrot", 1);
        require(villager.findBreedingFoodSlot() < 0, "two carrot stacks were added up");

        arena.give(villager, 3, "minecraft:potato", 12);
        require(villager.findBreedingFoodSlot() == 3, "12 potatoes were not food");
        villager.consumeBreedingFood();
        require(villager.getEquipment().getInventoryItem(3).isAir(), "breeding did not eat the 12 potatoes");

        arena.give(villager, 1, "minecraft:bread", 5);
        require(villager.findBreedingFoodSlot() == 1, "5 bread were not food");
        villager.consumeBreedingFood();
        require(villager.getEquipment().getInventoryItem(1).mCount == 2, "breeding did not eat 3 bread");
    }

    void bedsDecideRoom() {
        Village village;
        const PoiBlueprint *bed = PoiBlueprints::find(BlockNames::BED);
        require(bed != nullptr, "the bed has no blueprint");
        require(village.hasRoomForBaby(), "an empty village had no room");

        std::vector<std::shared_ptr<PoiInstance>> beds;
        for (int32_t index = 0; index < 2; ++index) {
            beds.push_back(std::make_shared<PoiInstance>(Vector3i(200 + index * 2, 64, 200), *bed));
            require(village.addPoi(beds.back()), "a bed was refused");
        }
        require(village.hasRoomForBaby(), "two beds and nobody had no room");
    }

    void villagersBreed(Arena &arena) {
        arena.floor(-10, 10, 63, -10, 10);
        auto village = std::make_shared<Village>();
        const PoiBlueprint *bed = PoiBlueprints::find(BlockNames::BED);
        std::vector<std::shared_ptr<PoiInstance>> beds;
        for (int32_t index = 0; index < 3; ++index) {
            beds.push_back(std::make_shared<PoiInstance>(Vector3i(-8 + index * 2, 64, -8), *bed));
            require(village->addPoi(beds.back()), "a bed was refused");
        }

        VillagerV2Actor &first = arena.adult(Vector3f(0.5f, 64.0f, 0.5f), "unskilled");
        VillagerV2Actor &second = arena.adult(Vector3f(1.5f, 64.0f, 0.5f), "unskilled");
        first.getDweller().joinVillage(arena.mOwner, first, *village);
        second.getDweller().joinVillage(arena.mOwner, second, *village);
        require(village->getInhabitantCount() == 2 && village->hasRoomForBaby(), "three beds left no room for two");

        MakeLoveGoal love;
        ReceiveLoveGoal receive;
        bool started = false;
        for (int32_t call = 0; call < 20000 && !started; ++call)
            started = love.canUse(arena.mOwner, first);
        require(!started, "a villager without food in the pair was picked");

        arena.give(second, 0, "minecraft:bread", 3);
        arena.give(first, 0, "minecraft:carrot", 12);
        for (int32_t call = 0; call < 20000 && !started; ++call)
            started = love.canUse(arena.mOwner, first);
        require(started, "two fed villagers never started breeding");
        require(first.isLoveInitiator() && !second.isLoveInitiator(), "the wrong villager started");
        require(first.getLovePartnerId() == second.getUniqueId() && second.getLovePartnerId() == first.getUniqueId(),
                "the pair did not name each other");
        require(second.getFlags().get(ActorFlag::InLove) && receive.canUse(arena.mOwner, second),
                "the partner was not in love");
        // the one already in love does not look for another partner
        require(!love.canUse(arena.mOwner, second), "the partner started its own breeding");

        const int32_t before = arena.countVillagers();
        love.start(arena.mOwner, first);
        int32_t ticks = 0;
        while (love.canContinueToUse(arena.mOwner, first) && ticks < 400) {
            love.tick(arena.mOwner, first);
            receive.tick(arena.mOwner, second);
            ++ticks;
        }
        // the baby comes on the 300th tick; the eaten food ends the goal
        require(ticks == 300, "breeding did not last its 300 ticks");
        love.stop(arena.mOwner, first);
        require(arena.countVillagers() == before + 1, "no baby was born");
        require(first.getEquipment().getInventoryItem(0).isAir() && second.getEquipment().getInventoryItem(0).isAir(),
                "the parents kept their food");
        require(!first.getFlags().get(ActorFlag::InLove) && !second.getFlags().get(ActorFlag::InLove)
                        && !first.hasLovePartner() && !second.hasLovePartner(),
                "the pair stayed in love");

        // the baby took the last bed: three beds, three villagers
        VillagerV2Actor *baby = nullptr;
        for (const auto &entry: arena.mOwner.getActors()) {
            VillagerV2Actor *candidate = dynamic_cast<VillagerV2Actor *>(entry.second.get());
            if (candidate != nullptr && candidate != &first && candidate != &second && candidate->isAlive()
                && candidate->getFlags().get(ActorFlag::Baby))
                baby = candidate;
        }
        require(baby != nullptr, "the new villager is not a baby");
        village->addDweller(arena.mOwner, DwellerRole::Inhabitant, baby->getUniqueId());
        require(!village->hasRoomForBaby(), "a full village still had room");
    }

    void surplusIsShared(Arena &arena) {
        arena.floor(20, 30, 63, 20, 30);
        VillagerV2Actor &giver = arena.adult(Vector3f(21.5f, 64.0f, 21.5f), "behavior_peasant");
        VillagerV2Actor &taker = arena.adult(Vector3f(23.5f, 64.0f, 21.5f), "behavior_peasant");

        // bread: want 3, surplus 6; a third of the 9 kept at most
        const ItemStack bread = arena.mOwner.createItemStack("minecraft:bread", 1);
        arena.give(giver, 0, "minecraft:bread", 10);
        require(MobShareables::surplusOf(giver, bread) == 1, "10 bread did not leave 1 to share");
        arena.give(giver, 0, "minecraft:bread", 20);
        require(MobShareables::surplusOf(giver, bread) == 3, "the share was not capped at a third of 9");
        require(MobShareables::wantedCountOf(taker, bread) == 3, "an empty villager did not want 3 bread");

        // wheat is shared as bread, three for one
        arena.give(giver, 1, "minecraft:wheat", 64);
        const ItemStack wheatItem = arena.mOwner.createItemStack("minecraft:wheat", 1);
        int32_t consumed = 0;
        const ItemStack shared = MobShareables::sharedStack(arena.mOwner, giver, wheatItem, 1, consumed);
        require(shared.isAir() || shared.mCount == 0, "one wheat made bread");
        const ItemStack crafted = MobShareables::sharedStack(arena.mOwner, giver, wheatItem, 7, consumed);
        require(!crafted.isAir() && crafted.mDefinition->getIdentifier() == "minecraft:bread" && crafted.mCount == 2
                        && consumed == 6,
                "7 wheat did not make 2 bread from 6");
        giver.getEquipment().setInventoryItem(1, ItemStack::air());

        ShareItemsGoal::Settings settings;
        settings.mSpeed = 0.25f;
        settings.mMaxDistance = 3;
        settings.mFilters.push_back(json::parse(R"({"subject": "other", "test": "is_family", "value": "villager"})"));
        ShareItemsGoal goal(settings);
        arena.clearItems();
        require(goal.canUse(arena.mOwner, giver), "a villager with spare bread did not share");
        require(giver.getEquipment().getInventoryItem(0).mCount == 17, "the shared bread stayed in the inventory");
        goal.start(arena.mOwner, giver);
        require(giver.getEquipment().getSlot(MobEquipment::MAINHAND).mCount == 3, "the bread was not carried");
        for (int32_t tick = 0; tick < 19; ++tick)
            goal.tick(arena.mOwner, giver);
        require(arena.items().empty(), "the bread was thrown early");
        goal.tick(arena.mOwner, giver);
        const std::vector<ItemActor *> thrown = arena.items();
        require(thrown.size() == 1 && thrown[0]->getItem().mCount == 3, "the bread was not thrown");
        goal.stop(arena.mOwner, giver);
        require(giver.getEquipment().getSlot(MobEquipment::MAINHAND).isAir(), "the hand kept the bread");

        // a mob that has what it wants takes nothing
        arena.give(taker, 0, "minecraft:bread", 3);
        require(!goal.canUse(arena.mOwner, giver), "bread went to a villager that has enough");
        arena.clearItems();
    }

    BaseMoveToBlockGoal::Settings farmSettings(int32_t range, int32_t count) {
        BaseMoveToBlockGoal::Settings settings;
        settings.mSpeed = 0.25f;
        settings.mSearchRange = range;
        settings.mSearchHeight = 1;
        settings.mSearchCount = count;
        settings.mGoalRadius = 1.5f;
        return settings;
    }

    void farmerHarvestsAndReplants(Arena &arena) {
        arena.floor(40, 50, 63, 40, 50);
        const Vector3i farmland(44, 63, 44);
        arena.mLevel->setBlockState(farmland.x, farmland.y, farmland.z, stateOf(BlockNames::FARMLAND));
        arena.mLevel->setBlockState(farmland.x, farmland.y + 1, farmland.z, wheat(6));
        VillagerV2Actor &farmer = arena.adult(Vector3f(44.5f, 64.0f, 46.5f), "behavior_peasant");
        arena.give(farmer, 0, "minecraft:wheat_seeds", 5);
        require(HarvestFarmBlockGoal::findSeedSlot(farmer) == 0, "wheat seeds are not a seed");

        HarvestFarmBlockGoal::Timing timing;
        timing.mMaxTicksBeforeSearch = 20;
        timing.mSearchCooldownTicks = 160;
        timing.mTicksUntilNewTask = 10;
        HarvestFarmBlockGoal goal(farmSettings(16, 0), timing);

        // an unripe crop with no bare farmland gives the goal nothing to do
        bool started = false;
        for (int32_t call = 0; call < 400 && !started; ++call)
            started = goal.canUse(arena.mOwner, farmer);
        require(!started, "an unripe crop was harvested");

        arena.mLevel->setBlockState(farmland.x, farmland.y + 1, farmland.z, wheat(7));
        HarvestFarmBlockGoal ripe(farmSettings(16, 0), timing);
        for (int32_t call = 0; call < 40 && !started; ++call)
            started = ripe.canUse(arena.mOwner, farmer);
        require(started, "a ripe crop was not found");
        ripe.start(arena.mOwner, farmer);
        // within 1.5 of the low corner above the farmland, measured from the middle of the body
        farmer.setPosition(Vector3f(44.5f, 64.0f, 44.9f));
        ripe.tick(arena.mOwner, farmer);
        const BlockState after = arena.mLevel->getBlockState(farmland.x, farmland.y + 1, farmland.z);
        require(after.mName == "minecraft:wheat" && after.mStates.getInt("growth", -1) == 0,
                "the crop was not harvested and replanted");
        require(farmer.getEquipment().getInventoryItem(0).mCount == 4, "replanting did not use a seed");
        require(!arena.items().empty(), "the harvest dropped nothing");
        require(!ripe.canContinueToUse(arena.mOwner, farmer), "the goal kept going after its task");
        ripe.stop(arena.mOwner, farmer);
        arena.clearItems();

        // bare farmland is sown
        arena.mLevel->setBlockState(farmland.x, farmland.y + 1, farmland.z, BlockState(BlockNames::AIR));
        HarvestFarmBlockGoal sow(farmSettings(16, 0), timing);
        started = false;
        for (int32_t call = 0; call < 40 && !started; ++call)
            started = sow.canUse(arena.mOwner, farmer);
        require(started, "bare farmland was not sown");
        sow.start(arena.mOwner, farmer);
        sow.tick(arena.mOwner, farmer);
        require(arena.mLevel->getBlockState(farmland.x, farmland.y + 1, farmland.z).mName == "minecraft:wheat",
                "the seed was not planted");
        sow.stop(arena.mOwner, farmer);
    }

    void farmerFertilizes(Arena &arena) {
        arena.floor(60, 70, 63, 40, 50);
        const Vector3i farmland(64, 63, 44);
        arena.mLevel->setBlockState(farmland.x, farmland.y, farmland.z, stateOf(BlockNames::FARMLAND));
        arena.mLevel->setBlockState(farmland.x, farmland.y + 1, farmland.z, wheat(0));
        VillagerV2Actor &farmer = arena.adult(Vector3f(64.5f, 64.0f, 45.5f), "behavior_peasant");

        FertilizeFarmBlockGoal goal(farmSettings(1, 9), 1, 160);
        require(!goal.canUse(arena.mOwner, farmer), "a farmer without bone meal fertilized");

        arena.give(farmer, 2, "minecraft:bone_meal", 2);
        bool started = false;
        for (int32_t call = 0; call < 400 && !started; ++call) {
            ServerTickProbe::advance(arena.mOwner, 1);
            started = goal.canUse(arena.mOwner, farmer);
        }
        require(started, "a farmer with bone meal next to a crop did not fertilize");
        goal.start(arena.mOwner, farmer);
        farmer.setPosition(Vector3f(64.5f, 64.0f, 44.9f));
        goal.tick(arena.mOwner, farmer);
        require(arena.mLevel->getBlockState(farmland.x, farmland.y + 1, farmland.z).mStates.getInt("growth", 0) >= 2,
                "the bone meal did not grow the crop");
        require(farmer.getEquipment().getInventoryItem(2).mCount == 1, "the bone meal was not used");
        require(!goal.canContinueToUse(arena.mOwner, farmer), "the farmer used more than its one fertilizer");
        goal.stop(arena.mOwner, farmer);
    }

    void farmerEmptiesComposter(Arena &arena) {
        arena.floor(80, 90, 63, 40, 50);
        const Vector3i site(84, 64, 44);
        auto village = std::make_shared<Village>();
        const PoiBlueprint *blueprint = PoiBlueprints::find(BlockNames::COMPOSTER);
        require(blueprint != nullptr, "the composter has no blueprint");
        auto poi = std::make_shared<PoiInstance>(site, *blueprint);
        require(village->addPoi(poi), "the composter was refused");
        BlockState composter = stateOf(BlockNames::COMPOSTER);
        composter.mStates.putInt("composter_fill_level", 8);
        arena.mLevel->setBlockState(site.x, site.y, site.z, composter);

        VillagerV2Actor &farmer = arena.adult(Vector3f(84.5f, 64.0f, 46.5f), "behavior_peasant");
        farmer.getDweller().joinVillage(arena.mOwner, farmer, *village);
        // claims run on the ticks where tick % 30 == 15
        village->tick(arena.mOwner, *arena.mLevel, 15);
        require(village->getClaimedPoi(farmer.getUniqueId(), PoiType::JobSite) == poi, "the composter was not claimed");

        WorkGoal::Settings settings;
        settings.mSpeed = 0.25f;
        settings.mActiveTicks = 250;
        WorkComposterGoal::Composting composting;
        composting.mUseBlockMin = 100;
        composting.mUseBlockMax = 200;
        composting.mBlockInteractionMax = 1;
        composting.mCanEmpty = true;
        WorkComposterGoal goal(settings, composting);
        bool started = false;
        for (int32_t call = 0; call < 125 && !started; ++call)
            started = goal.canUse(arena.mOwner, farmer);
        require(started, "the farmer did not go to its composter");
        goal.start(arena.mOwner, farmer);
        farmer.setPosition(Vector3f(84.5f, 64.0f, 45.5f));
        goal.tick(arena.mOwner, farmer);
        require(arena.mLevel->getBlockState(site.x, site.y, site.z).mStates.getInt("composter_fill_level", 0) == 8,
                "the composter was used before its delay");

        ServerTickProbe::advance(arena.mOwner, 200);
        goal.tick(arena.mOwner, farmer);
        require(arena.mLevel->getBlockState(site.x, site.y, site.z).mStates.getInt("composter_fill_level", -1) == 0,
                "the full composter was not emptied");
        bool boneMeal = false;
        for (int slot = 0; slot < farmer.getEquipment().getInventorySize(); ++slot) {
            const ItemStack &item = farmer.getEquipment().getInventoryItem(slot);
            boneMeal = boneMeal || (!item.isAir() && item.mDefinition->getIdentifier() == "minecraft:bone_meal");
        }
        require(boneMeal, "the bone meal did not reach the inventory");
        goal.stop(arena.mOwner, farmer);
    }
}

namespace {
    void composterTakesItsTable(Arena &arena) {
        require(ComposterBlock::compostChance(arena.mOwner.createItemStack("minecraft:cake", 1)) == 100,
                "a cake does not always compost");
        require(ComposterBlock::compostChance(arena.mOwner.createItemStack("minecraft:wheat_seeds", 1)) == 30,
                "wheat seeds lost their 30 percent");
        require(ComposterBlock::compostChance(arena.mOwner.createItemStack("minecraft:melon_block", 1)) == 65,
                "a melon composts as its block");
        require(ComposterBlock::compostChance(arena.mOwner.createItemStack("minecraft:red_shrub", 1)) == 30,
                "the compostable component of a data item was not read");
        require(ComposterBlock::compostChance(arena.mOwner.createItemStack("minecraft:stone", 1)) == 0,
                "stone composts");
        require(VanillaItems::fromIdentifier("minecraft:wheat_seeds")->isFarmerCompost()
                        && !VanillaItems::fromIdentifier("minecraft:carrot")->isFarmerCompost(),
                "the farmer composts the wrong items");

        const Vector3i site(100, 64, 100);
        arena.mLevel->setBlockState(site.x, site.y, site.z, stateOf(BlockNames::COMPOSTER));
        const ItemStack stone = arena.mOwner.createItemStack("minecraft:stone", 1);
        require(ComposterBlock::addItems(arena.mOwner, *arena.mLevel, site, stone, 1) == 0, "stone went in");

        const ItemStack cakes = arena.mOwner.createItemStack("minecraft:cake", 10);
        require(ComposterBlock::addItems(arena.mOwner, *arena.mLevel, site, cakes, 10) == 7,
                "the composter did not stop at its seventh layer");
        require(arena.mLevel->getBlockState(site.x, site.y, site.z).mStates.getInt("composter_fill_level", 0) == 7,
                "seven cakes did not fill the composter");
        require(ComposterBlock::addItems(arena.mOwner, *arena.mLevel, site, cakes, 1) == 0, "a full composter took more");
        // the floor inside rises with the layers: 15 pixels at the seventh
        std::vector<AxisAlignedBB> boxes;
        BlockShape::appendShapesAt(arena.mLevel->getBlockState(site.x, site.y, site.z), site.x, site.y, site.z, nullptr,
                                   [](int32_t, int32_t, int32_t) -> const BlockState * { return nullptr; }, boxes);
        bool floor = false;
        for (const AxisAlignedBB &box: boxes)
            floor = floor || std::abs(box.mMaxY - ((float) site.y + 0.9375f)) < 1e-4f;
        require(floor, "the composter floor did not follow its layers");

        require(arena.mLevel->isUpdateScheduled(site), "the full composter scheduled no tick");
        const auto *block = VanillaBlocks::getAs<ComposterBlock>(BlockNames::COMPOSTER);
        require(block != nullptr, "the composter has no class");
        block->onScheduledUpdate(arena.mOwner, *arena.mLevel, site, arena.mLevel->getBlockState(site.x, site.y, site.z));
        require(arena.mLevel->getBlockState(site.x, site.y, site.z).mStates.getInt("composter_fill_level", 0) == 8,
                "the full composter did not turn ready");
        require(ComposterBlock::takeOutput(arena.mOwner, *arena.mLevel, site), "the ready composter was not emptied");
        require(arena.mLevel->getBlockState(site.x, site.y, site.z).mStates.getInt("composter_fill_level", -1) == 0,
                "the emptied composter kept its layers");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-villager-breeding-"
               + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        BlockPaletteRegistry::getInstance().initialize();
        Arena arena(directory);
        foodDecidesBreeding(arena);
        bedsDecideRoom();
        villagersBreed(arena);
        surplusIsShared(arena);
        farmerHarvestsAndReplants(arena);
        farmerFertilizes(arena);
        farmerEmptiesComposter(arena);
        composterTakesItsTable(arena);
        std::cout << "villager breeding checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
