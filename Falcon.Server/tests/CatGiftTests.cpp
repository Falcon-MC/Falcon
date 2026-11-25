#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/DropItemForGoal.h"
#include "World/Actor/AI/Goal/PetSleepWithOwnerGoal.h"
#include "World/Actor/ActorFlags.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BedBlockActor.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>

namespace {
    const char *const GIFT_GROUP = "minecraft:cat_gift_for_owner";
    // direction 0: the head lies south of the foot
    const Vector3i BED_HEAD(4, 100, 1);
    const Vector3i BED_FOOT(4, 100, 0);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState bedPart(bool head) {
        BlockState state(BlockNames::BED);
        state.mStates.putInt("direction", 0);
        state.mStates.putByte("head_piece_bit", head ? 1 : 0);
        state.mStates.putByte("occupied_bit", 0);
        return state;
    }

    // stone ground at y 99 around the origin, a bed on it
    struct Arena {
        ServerNetworkHandler mOwner{"CatGift", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getOverworld();

        Arena() {
            require(mLevel.openStorage(ServerPaths::getRoot().string()), "could not open cat gift storage");
            for (int x = -1; x <= 1; ++x) {
                for (int z = -1; z <= 1; ++z)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int x = -10; x <= 10; ++x) {
                for (int z = -10; z <= 10; ++z)
                    mLevel.setBlockState(x, 99, z, VanillaBlocks::STONE().toBlockState());
            }
            mLevel.setBlockState(BED_HEAD.x, BED_HEAD.y, BED_HEAD.z, bedPart(true));
            mLevel.setBlockState(BED_FOOT.x, BED_FOOT.y, BED_FOOT.z, bedPart(false));
            mLevel.getBlockActors().getOrCreate<BedBlockActor>(BED_HEAD);
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            auto &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.setGameType((int32_t) GameType::Survival);
            return player;
        }

        MobActor &tameCat(ServerPlayer &player, const Vector3f &position) {
            auto *cat = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, "minecraft:cat", position,
                    [](ServerActor &actor) {
                        static_cast<MobActor &>(actor).setSpawnEvent("minecraft:spawn_wild_adult");
                    }));
            require(cat != nullptr, "the cat did not spawn");
            cat->startDefinition(mOwner);
            require(cat->tameTo(mOwner, player, "minecraft:tameable", false), "the cat was not tamed");
            require(cat->hasComponentGroup("minecraft:cat_tame"), "the tamed cat has no tame group");
            return *cat;
        }

        std::set<std::string> droppedItems() {
            std::set<std::string> items;
            for (auto &entry: mOwner.getActors()) {
                if (const auto *item = dynamic_cast<const ItemActor *>(entry.second.get()))
                    items.insert(item->getItem().mDefinition->getIdentifier());
            }
            return items;
        }
    };

    // the cat's definition values
    PetSleepWithOwnerGoal::Settings sleepSettings() {
        PetSleepWithOwnerGoal::Settings settings;
        settings.mSpeed = 1.2f;
        settings.mSearchHeight = 10;
        settings.mGoalRadius = 1.0f;
        return settings;
    }

    DropItemForGoal giftGoal(const MobActor &cat, float dropChance) {
        BaseMoveToBlockGoal::Settings settings;
        settings.mSpeed = 1.0f;
        settings.mSearchRange = 5;
        settings.mSearchHeight = 2;
        settings.mGoalRadius = 1.0f;
        DropItemForGoal::Offer offer;
        offer.mCooldownTicks = 5;
        offer.mDropChance = dropChance;
        offer.mOfferingDistance = 5.0f;
        offer.mMinimumTeleportDistance = 2.0f;
        offer.mTimeOfDayMin = 0.74999f;
        offer.mTimeOfDayMax = 0.8f;
        offer.mTargetRange = Vector3f(5.0f, 5.0f, 5.0f);
        offer.mTeleportOffset = Vector3f(0.0f, 1.0f, 0.0f);
        offer.mEntries.push_back(NearestAttackableTargetGoal::Entry{});
        offer.mLootTable = "loot_tables/entities/cat_gift.json";
        const json::Value *drop = cat.getComponent("minecraft:behavior.drop_item_for");
        require(drop != nullptr && drop->get("on_drop_attempt") != nullptr, "the gift group has no drop_item_for");
        offer.mOnDropAttempt = std::make_shared<json::Value>(*drop->get("on_drop_attempt"));
        return DropItemForGoal(settings, std::move(offer));
    }

    // the cat lies on the sleeping owner's bed; the owner getting up gives it the gift group
    void catSleepsWithOwner() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        MobActor &cat = arena.tameCat(player, Vector3f(-2.5f, 100.0f, 0.5f));
        MobActor &other = arena.tameCat(player, Vector3f(-3.5f, 100.0f, 0.5f));

        PetSleepWithOwnerGoal goal(sleepSettings());
        PetSleepWithOwnerGoal otherGoal(sleepSettings());
        require(!goal.canUse(arena.mOwner, cat), "the cat went to bed with its owner awake");

        player.setSleeping(BED_HEAD);
        player.setPosition(Vector3f(4.5f, 100.56f, 1.5f));
        require(goal.canUse(arena.mOwner, cat), "the cat did not follow its sleeping owner to bed");
        require(!otherGoal.canUse(arena.mOwner, other), "a second cat took the same bed");
        goal.start(arena.mOwner, cat);

        cat.setPosition(Vector3f(4.5f, 100.56f, 1.5f));
        for (int tick = 0; tick < 40 && !cat.getFlags().get(ActorFlag::Resting); ++tick)
            goal.tick(arena.mOwner, cat);
        require(cat.getFlags().get(ActorFlag::Resting), "the cat did not lie down on the bed");
        require(goal.canContinueToUse(arena.mOwner, cat), "the cat left the bed while its owner sleeps");

        player.clearSleeping();
        require(!goal.canContinueToUse(arena.mOwner, cat), "the cat stayed in bed after its owner woke up");
        goal.stop(arena.mOwner, cat);
        require(!cat.getFlags().get(ActorFlag::Resting), "the cat still lies down after waking");
        require(cat.hasComponentGroup(GIFT_GROUP), "waking with its owner did not give the cat a gift to bring");

        // the bed is free again
        player.setSleeping(BED_HEAD);
        require(otherGoal.canUse(arena.mOwner, other), "the bed was not released for the next cat");
    }

    // an owner who gets up before the cat lies down gets no gift
    void catOnTheWayGetsNoGift() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        MobActor &cat = arena.tameCat(player, Vector3f(-2.5f, 100.0f, 0.5f));
        PetSleepWithOwnerGoal goal(sleepSettings());

        player.setSleeping(BED_HEAD);
        require(goal.canUse(arena.mOwner, cat), "the cat did not follow its sleeping owner to bed");
        goal.start(arena.mOwner, cat);
        goal.tick(arena.mOwner, cat);
        player.clearSleeping();
        goal.stop(arena.mOwner, cat);
        require(!cat.hasComponentGroup(GIFT_GROUP), "a cat that never lay down got a gift to bring");
    }

    // in the morning the cat is put beside its owner and drops a roll of the gift table
    void catBringsMorningGift() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        MobActor &cat = arena.tameCat(player, Vector3f(-4.5f, 100.0f, -3.5f));
        cat.addComponentGroup(GIFT_GROUP);
        arena.mLevel.setTime(0);

        DropItemForGoal goal = giftGoal(cat, 1.0f);
        require(goal.canUse(arena.mOwner, cat), "the gift goal did not start");
        goal.start(arena.mOwner, cat);
        require(goal.canContinueToUse(arena.mOwner, cat), "the cat found no owner to bring its gift to");
        goal.tick(arena.mOwner, cat);

        // the spot two blocks from the owner: squared distance 5 to its feet block
        const Vector3f &position = cat.getPosition();
        const float dx = std::floor(position.x) - 0.0f;
        const float dz = std::floor(position.z) - 0.0f;
        require(dx * dx + dz * dz == 4.0f && position.y == 100.0f, "the cat was not put beside its owner");

        const std::set<std::string> items = arena.droppedItems();
        require(items.size() == 1, "the cat did not drop exactly one gift");
        const std::set<std::string> gifts = {"minecraft:rabbit_hide", "minecraft:rabbit_foot", "minecraft:chicken",
                                             "minecraft:feather", "minecraft:rotten_flesh", "minecraft:string",
                                             "minecraft:phantom_membrane"};
        require(gifts.count(*items.begin()) == 1, "the gift is not from the cat gift table: " + *items.begin());

        require(!goal.canContinueToUse(arena.mOwner, cat), "the gift goal kept running after the drop");
        goal.stop(arena.mOwner, cat);
        require(!cat.hasComponentGroup(GIFT_GROUP), "the drop attempt did not end the gift");
    }

    // past the morning window the try fails and still ends the gift
    void noGiftAtNoon() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        MobActor &cat = arena.tameCat(player, Vector3f(-2.5f, 100.0f, 0.5f));
        cat.addComponentGroup(GIFT_GROUP);
        arena.mLevel.setTime(6000);

        DropItemForGoal goal = giftGoal(cat, 1.0f);
        require(goal.canUse(arena.mOwner, cat), "the gift goal did not try");
        goal.start(arena.mOwner, cat);
        require(!goal.canContinueToUse(arena.mOwner, cat), "the cat brought a gift at noon");
        goal.stop(arena.mOwner, cat);
        require(arena.droppedItems().empty(), "an item was dropped at noon");
        require(!cat.hasComponentGroup(GIFT_GROUP), "the failed try did not end the gift");
        require(!goal.canUse(arena.mOwner, cat), "the cooldown did not hold the next try off");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-cat-gift-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        catSleepsWithOwner();
        catOnTheWayGetsNoGift();
        catBringsMorningGift();
        noGiftAtNoon();
        std::cout << "cat gift checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
