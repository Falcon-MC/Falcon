#include "Network/Handler/InventoryHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/ChargeHeldItemGoal.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityFilter.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const int32_t TICKS_PER_SECOND = 20;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Piglins", "", 1};
        Level &mLevel;

        explicit Arena(DimensionType dimension) : mLevel(mOwner.getWorlds().getDefault().getLevel(dimension)) {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -8; x <= 8; x++) {
                for (int32_t z = -8; z <= 8; z++)
                    mLevel.setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }

        void ticks(int32_t count) {
            // the world tick moves with the overworld, as a server tick does
            for (int32_t tick = 0; tick < count; tick++) {
                mOwner.getWorlds().getDefault().getOverworld().tickTime();
                mOwner.tickActors();
            }
        }

        size_t countOf(const char *identifier) {
            size_t found = 0;
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    ++found;
            }
            return found;
        }

        MobActor *first(const char *identifier) {
            for (auto &entry: mOwner.getActors()) {
                if (entry.second->getTypeId() == identifier && !entry.second->isDead())
                    return dynamic_cast<MobActor *>(entry.second.get());
            }
            return nullptr;
        }
    };

    std::string heldName(const MobActor &mob, int32_t slot) {
        const ItemStack &item = mob.getEquipment().getSlot(slot);
        return item.isAir() || item.mDefinition == nullptr ? std::string() : item.mDefinition->getIdentifier();
    }

    // the player entry of alert_for_attack_targets: no golden armor piece anywhere
    const json::Value &playerTargetFilter(size_t entry) {
        const json::Value *goal = EntityDefinitions::find("minecraft:piglin")
                                          ->get("component_groups")
                                          ->get("alert_for_attack_targets")
                                          ->get("minecraft:behavior.nearest_attackable_target");
        return *goal->get("entity_types")->mArray[entry]->get("filters");
    }

    void goldArmorNeutrality() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 0.5f));
        MobActor &piglin = arena.spawn("minecraft:piglin", Vector3f(0.5f, 64.0f, 6.5f), "spawn_adult_melee");
        const json::Value &noGold = playerTargetFilter(2);
        require(EntityFilter::test(noGold, arena.mOwner, piglin, &player), "a bare player is not a piglin target");
        for (int32_t slot = 0; slot < 4; slot++) {
            const char *pieces[] = {"minecraft:golden_helmet", "minecraft:golden_chestplate",
                                    "minecraft:golden_leggings", "minecraft:golden_boots"};
            player.getInventory().setArmor(slot, arena.mOwner.createItemStack(pieces[slot], 1));
            require(!EntityFilter::test(noGold, arena.mOwner, piglin, &player),
                    std::string("a player wearing ") + pieces[slot] + " is still a target");
            player.getInventory().setArmor(slot, ItemStack::air());
        }

        // a player with a container open is a target, gold or not
        const json::Value &containerOpen = playerTargetFilter(3);
        player.getInventory().setArmor(0, arena.mOwner.createItemStack("minecraft:golden_helmet", 1));
        require(!EntityFilter::test(containerOpen, arena.mOwner, piglin, &player), "a closed inventory angers");
        const Vector3i chestAt(2, 64, 2);
        arena.mLevel.setBlock(chestAt, VanillaBlocks::fromIdentifier("minecraft:chest")->toBlockState(), false);
        arena.mLevel.getBlockActors().getOrCreate<ChestBlockActor>(chestAt);
        require(player.getInventoryManager().onClientOpenChest(chestAt), "the chest did not open");
        require(EntityFilter::test(containerOpen, arena.mOwner, piglin, &player),
                "a player with an open chest is not a piglin target");
    }

    // the angry filter wants no attack cooldown; checked every tick, not only when the anger starts
    void angerFollowsTheCooldown() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 0.5f));
        // golden armor keeps the piglin's own target goal away: only the target set here counts
        player.getInventory().setArmor(0, arena.mOwner.createItemStack("minecraft:golden_helmet", 1));
        MobActor &piglin = arena.spawn("minecraft:piglin", Vector3f(0.5f, 64.0f, 6.5f), "spawn_adult_melee");
        require(piglin.hasComponentGroup("attack_cooldown"), "an adult spawns without its attack cooldown");

        require(piglin.setTarget(arena.mOwner, player.getRuntimeId()), "the piglin took no target");
        arena.ticks(2);
        require(!piglin.hasComponentGroup("angry") && piglin.getTarget(arena.mOwner) == nullptr,
                "a piglin on attack cooldown stayed angry");

        piglin.fireEvent(arena.mOwner, "attack_cooldown_complete_event");
        require(piglin.setTarget(arena.mOwner, player.getRuntimeId()), "the piglin took no target");
        arena.ticks(2);
        require(piglin.hasComponentGroup("angry") && piglin.getFlags().get(ActorFlag::Angry),
                "a piglin past its cooldown did not get angry");
        require(piglin.getTarget(arena.mOwner) == &player, "the angry piglin dropped its target");

        piglin.addComponentGroup("attack_cooldown");
        arena.ticks(2);
        require(!piglin.hasComponentGroup("angry") && !piglin.getFlags().get(ActorFlag::Angry),
                "the anger outlived a new attack cooldown");
        require(piglin.getTarget(arena.mOwner) == nullptr, "the calmed piglin kept its target");
    }

    void barter() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 4.5f));
        MobActor &piglin = arena.spawn("minecraft:piglin", Vector3f(0.5f, 64.0f, 6.5f), "spawn_adult_melee");
        player.getInventory().setArmor(0, arena.mOwner.createItemStack("minecraft:golden_helmet", 1));
        player.getInventory().setItemInHand(arena.mOwner.createItemStack("minecraft:gold_ingot", 2));
        require(InventoryHandler::interactWithEntity(arena.mOwner, player, piglin.getRuntimeId(), Vector3f()),
                "the gold ingot did not reach the piglin");
        require(piglin.getFlags().get(ActorFlag::Admiring), "the piglin does not admire the ingot");
        require(heldName(piglin, MobEquipment::OFFHAND) == "minecraft:gold_ingot", "the ingot is not in the off hand");
        require(player.getInventory().getItemInHand().mCount == 1, "the piglin took more than one ingot");

        // admire_item duration 8 s, then the barter goal rolls the table once
        ServerNetworkHandler::DropCapture capture;
        arena.mOwner.setDropCapture(&capture);
        arena.ticks(8 * TICKS_PER_SECOND + 20);
        arena.mOwner.setDropCapture(nullptr);
        require(!piglin.getFlags().get(ActorFlag::Admiring), "the piglin still admires after 8 seconds");
        require(!capture.mItems.empty(), "the barter dropped nothing");
        require(piglin.getEquipment().getSlot(MobEquipment::OFFHAND).isAir(), "the ingot was not spent");
    }

    // 15 s of shaking outside the Nether, then a zombified piglin with the same gear; the countdown survives a reload
    void zombification() {
        Arena arena(DimensionType::Overworld);
        MobActor &piglin = arena.spawn("minecraft:piglin", Vector3f(0.5f, 64.0f, 0.5f), "spawn_adult_melee");
        const std::string weapon = heldName(piglin, MobEquipment::MAINHAND);
        require(!weapon.empty(), "a melee piglin spawned with nothing in hand");
        arena.ticks(5 * TICKS_PER_SECOND);
        require(piglin.hasComponentGroup("start_zombification") && piglin.getFlags().get(ActorFlag::Shaking),
                "the piglin does not shake in the Overworld");

        const Tag data = piglin.saveNbt();
        require(data.contains("TimeStamp") && data.getByte("HasExecuted", 1) == 0, "the zombification timer was not saved");
        arena.mOwner.removeActor(piglin.getUniqueId());
        MobActor *loaded = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(arena.mLevel, "minecraft:piglin", Vector3f(0.5f, 64.0f, 0.5f)));
        require(loaded != nullptr, "no piglin to load into");
        // the storage loads the equipment beside the actor
        loaded->loadNbt(data);
        loaded->getEquipment().loadNbt(data, arena.mOwner.getCodecContext());

        arena.ticks(9 * TICKS_PER_SECOND);
        require(arena.countOf("minecraft:zombie_pigman") == 0, "the reloaded piglin converted early");
        arena.ticks(2 * TICKS_PER_SECOND);
        require(arena.countOf("minecraft:zombie_pigman") == 1, "the reloaded piglin restarted its 15 s countdown");
        MobActor *zombified = arena.first("minecraft:zombie_pigman");
        require(heldName(*zombified, MobEquipment::MAINHAND) == weapon, "the zombified piglin lost the weapon");
    }

    void babiesAndBrutes() {
        Arena arena(DimensionType::Nether);
        MobActor &baby = arena.spawn("minecraft:piglin", Vector3f(0.5f, 64.0f, 0.5f), "spawn_baby");
        arena.ticks(1);
        require(baby.getFlags().get(ActorFlag::Baby), "a baby piglin has no baby flag");
        require(baby.getComponent("minecraft:barter") == nullptr, "a baby piglin barters");

        MobActor &brute = arena.spawn("minecraft:piglin_brute", Vector3f(4.5f, 64.0f, 0.5f), "minecraft:entity_spawned");
        require(heldName(brute, MobEquipment::MAINHAND) == "minecraft:golden_axe", "the brute holds no golden axe");
        require(brute.getComponent("minecraft:admire_item") == nullptr, "the brute admires gold");

        MobActor &ranged = arena.spawn("minecraft:piglin", Vector3f(-4.5f, 64.0f, 0.5f), "spawn_adult_ranged");
        require(ChargeHeldItemGoal::holdsCrossbow(ranged), "a ranged piglin spawned without a crossbow");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        goldArmorNeutrality();
        angerFollowsTheCooldown();
        barter();
        zombification();
        babiesAndBrutes();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "piglin tests passed" << std::endl;
    return 0;
}
