#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/UpdateTradePacket.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/InspectBookshelfGoal.h"
#include "World/Actor/AI/Goal/LookAtTradingPlayerGoal.h"
#include "World/Actor/AI/Goal/TradeInterestGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Trade/MerchantOffers.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockState.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *const SCHEDULE_GROUPS[] = {"basic_schedule", "child_schedule", "work_schedule", "farmer_schedule",
                                           "fisher_schedule", "librarian_schedule", "jobless_schedule",
                                           "wander_schedule_villager", "gather_schedule_villager",
                                           "home_schedule_villager", "bed_schedule_villager", "play_schedule_villager",
                                           "work_schedule_villager", "baby"};

    struct Arena {
        ServerNetworkHandler mOwner{"VillagerInteraction", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++) {
                    // the world may have generated its spawn chunks already, and an existing chunk wins the insert:
                    // its terrain, from a random seed, would stand on the floor
                    mLevel->dropChunk(x, z);
                    LevelChunk &chunk = mLevel->insertChunk(LevelChunk(x, z));
                    chunk.setPopulated(true);
                    chunk.setBiome(BiomeIds::PLAINS);
                }
            }
            for (int32_t x = -10; x <= 10; ++x) {
                for (int32_t z = -10; z <= 10; ++z)
                    mLevel->setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &villager(const char *profession, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, "minecraft:villager_v2", position));
            require(mob != nullptr, "the villager did not spawn");
            mob->startDefinition(mOwner);
            for (const char *group: SCHEDULE_GROUPS)
                mob->removeComponentGroup(group);
            mob->addComponentGroup("adult");
            mob->addComponentGroup(profession);
            mob->tick(mOwner);
            return *mob;
        }
    };

    void traderShowsItsOffer(Arena &arena) {
        MobActor &farmer = arena.villager("farmer", Vector3f(0.5f, 64.0f, 0.5f));
        const MerchantOffers *offers = farmer.getEconomy().getOffers(arena.mOwner, farmer);
        require(offers != nullptr && !offers->mOffers.empty(), "the farmer has no offers");
        const MerchantOffer &first = offers->mOffers.front();

        ServerPlayer &player = arena.player(41, Vector3f(3.5f, 64.0f, 0.5f));
        TradeInterestGoal::Settings settings;
        settings.mRadius = 6.0f;
        settings.mInterestTicks = 900;
        settings.mRemoveItemTicks = 20;
        settings.mSwitchTicks = 40;
        settings.mCooldownTicks = 40;
        TradeInterestGoal goal(settings);
        require(!goal.canUse(arena.mOwner, farmer), "an empty hand caught the trader's interest");

        ItemStack held = first.mBuyA;
        held.mCount = 1;
        player.getInventory().setItemInHand(held);
        require(goal.canUse(arena.mOwner, farmer), "a wanted item did not catch the trader's interest");
        goal.start(arena.mOwner, farmer);
        require(farmer.getFlags().get(ActorFlag::TradeInterest), "the interest flag was not raised");
        const ItemStack &shown = farmer.getEquipment().getSlot(MobEquipment::MAINHAND);
        require(!shown.isAir() && shown.mDefinition == first.mSell.mDefinition, "the trader did not show its offer");

        player.setPosition(Vector3f(8.5f, 64.0f, 0.5f));
        require(!goal.canContinueToUse(arena.mOwner, farmer), "the interest outlived the distance");
        player.setPosition(Vector3f(3.5f, 64.0f, 0.5f));

        // an emptied hand keeps the item up for the remove time
        player.getInventory().setItemInHand(ItemStack::air());
        for (int32_t tick = 0; tick < 20; ++tick)
            require(goal.canContinueToUse(arena.mOwner, farmer), "the item went down before its remove time");
        require(!goal.canContinueToUse(arena.mOwner, farmer), "the item stayed up past its remove time");
        goal.stop(arena.mOwner, farmer);
        require(farmer.getEquipment().getSlot(MobEquipment::MAINHAND).isAir(), "the trader kept the item up");
        require(!farmer.getFlags().get(ActorFlag::TradeInterest), "the interest flag stayed up");

        // the cooldown holds the goal back
        player.getInventory().setItemInHand(held);
        require(!goal.canUse(arena.mOwner, farmer), "the interest came back during its cooldown");
    }

    void traderWatchesItsCustomer(Arena &arena) {
        MobActor &armorer = arena.villager("armorer", Vector3f(-5.5f, 64.0f, -5.5f));
        ServerPlayer &player = arena.player(42, Vector3f(-5.5f, 64.0f, -3.5f));
        LookAtTradingPlayerGoal goal;
        require(!goal.canUse(arena.mOwner, armorer), "a trader looked at a player not trading");
        require(armorer.onInteract(arena.mOwner, player), "interacting did not open the trade");
        UpdateTradePacket packet;
        require(armorer.getEconomy().startTrading(arena.mOwner, armorer, player, packet), "the trade did not start");
        require(goal.canUse(arena.mOwner, armorer), "the trader did not watch its customer");
    }

    void librarianFindsTheBookshelf(Arena &arena) {
        MobActor &librarian = arena.villager("librarian", Vector3f(5.5f, 64.0f, 5.5f));
        const Vector3i shelf(7, 63, 5);
        BaseMoveToBlockGoal::Settings settings;
        settings.mSpeed = 0.3f;
        settings.mSearchRange = 4;
        settings.mSearchHeight = 3;
        settings.mSearchCount = 0;
        settings.mGoalRadius = 0.8f;
        InspectBookshelfGoal goal(settings);
        require(!goal.canUse(arena.mOwner, librarian), "a librarian found a bookshelf in the stone");

        arena.mLevel->setBlockState(shelf.x, shelf.y, shelf.z, BlockState(BlockNames::BOOKSHELF));
        bool started = false;
        for (int32_t call = 0; call < 500 && !started; ++call)
            started = goal.canUse(arena.mOwner, librarian);
        require(started, "the librarian never looked for the bookshelf");
        goal.start(arena.mOwner, librarian);
        require(goal.canContinueToUse(arena.mOwner, librarian), "the bookshelf was dropped at once");
        arena.mLevel->setBlockState(shelf.x, shelf.y, shelf.z, BlockState("minecraft:stone"));
        require(!goal.canContinueToUse(arena.mOwner, librarian), "a broken bookshelf kept the librarian");
        goal.stop(arena.mOwner, librarian);
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-villager-interaction-"
               + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        traderShowsItsOffer(arena);
        traderWatchesItsCustomer(arena);
        librarianFindsTheBookshelf(arena);
        std::cout << "villager interaction checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
