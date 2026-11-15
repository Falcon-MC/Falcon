#include "Network/ServerNetworkHandler.h"
#include "Server/Commands/ReplaceItemCommand.h"
#include "Server/Commands/ServerCommandOrigin.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/ItemProperties.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Inventory/EnderChestInventoryStore.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    const char *const SUCCESS = "commands.replaceitem.success";
    const char *const SUCCESS_ENTITY = "commands.replaceitem.success.entity";
    const char *const FAILED = "commands.replaceitem.failed";
    const char *const BAD_SLOT = "commands.replaceitem.badSlotNumber";
    const char *const NO_CONTAINER = "commands.replaceitem.noContainer";
    const char *const NO_TARGET = "commands.generic.noTargetMatch";
    const char *const EXCEPTION = "commands.generic.exception";
    const Vector3i CHEST(2, 64, 2);

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    class RecordingOrigin : public ServerCommandOrigin {
    public:
        explicit RecordingOrigin(ServerNetworkHandler *handler) : ServerCommandOrigin(handler) {}

        void sendTranslation(const std::string &key, const std::vector<std::string> &parameters) override {
            mKeys.push_back(key);
            mParameters.push_back(parameters);
        }

        std::vector<std::string> mKeys;
        std::vector<std::vector<std::string>> mParameters;
    };

    struct Arena {
        ServerNetworkHandler mOwner{"Replace item", "", 1};
        Level *mLevel = nullptr;
        ReplaceItemCommand mCommand{mOwner};

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        // the answers of one command line
        RecordingOrigin run(const std::string &line, bool expected) {
            RecordingOrigin origin(&mOwner);
            require(mCommand.execute(origin, {line}) == expected,
                    "'" + line + "' " + (expected ? "failed" : "passed") + ": "
                    + (origin.mKeys.empty() ? "no answer" : origin.mKeys.back()));
            return origin;
        }

        void answers(const std::string &line, bool expected, const char *key) {
            const RecordingOrigin origin = run(line, expected);
            require(std::find(origin.mKeys.begin(), origin.mKeys.end(), key) != origin.mKeys.end(),
                    "'" + line + "' answered " + (origin.mKeys.empty() ? "nothing" : origin.mKeys.back())
                    + " instead of " + key);
        }

        MobActor &spawn(const char *identifier, const char *event) {
            ServerActor *actor = mOwner.spawnActor(*mLevel, identifier, Vector3f(0.5f, 100.0f, 0.5f),
                                                   [event](ServerActor &spawned) {
                                                       static_cast<MobActor &>(spawned).setSpawnEvent(event);
                                                   });
            MobActor *mob = dynamic_cast<MobActor *>(actor);
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            mob->startDefinition(mOwner);
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(1.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }
    };

    bool holds(const ItemStack &item, const char *identifier, int count) {
        return item.mDefinition != nullptr && item.mDefinition->getIdentifier() == identifier && item.mCount == count;
    }

    void testBlock(Arena &arena) {
        Container *chest = arena.mLevel->getBlockActors().getOrCreate<ChestBlockActor>(CHEST).getContainer();
        require(chest != nullptr, "the chest holds no container");
        const int last = chest->getContainerSize() - 1;

        const RecordingOrigin placed = arena.run("block 2 64 2 slot.container 3 diamond 5", true);
        require(placed.mKeys.back() == SUCCESS, "a block slot answered " + placed.mKeys.back());
        require(placed.mParameters.back()
                == std::vector<std::string>({"slot.container", "3", "5", "Diamond"}),
                "the block answer names the wrong slot, amount or item");
        require(holds(chest->getContainerItem(3), "minecraft:diamond", 5), "the chest slot holds no 5 diamonds");

        // keep leaves an occupied slot alone and fills an empty one; destroy replaces
        arena.answers("block 2 64 2 slot.container 3 keep apple", false, FAILED);
        require(holds(chest->getContainerItem(3), "minecraft:diamond", 5), "keep replaced an occupied slot");
        arena.answers("block 2 64 2 slot.container 4 keep apple 2", true, SUCCESS);
        require(holds(chest->getContainerItem(4), "minecraft:apple", 2), "keep did not fill an empty slot");
        arena.answers("block 2 64 2 slot.container 3 destroy apple", true, SUCCESS);
        require(holds(chest->getContainerItem(3), "minecraft:apple", 1), "an amount left out is not one item");

        arena.answers("block 2 64 2 slot.container " + std::to_string(last) + " stick", true, SUCCESS);
        const RecordingOrigin beyond = arena.run("block 2 64 2 slot.container " + std::to_string(last + 1) + " stick",
                                                 false);
        require(beyond.mKeys.back() == BAD_SLOT, "a slot past the chest answered " + beyond.mKeys.back());
        require(beyond.mParameters.back()
                == std::vector<std::string>({std::to_string(last + 1), "0", std::to_string(last)}),
                "the bad slot answer gives the wrong range");
        arena.answers("block 2 64 2 slot.container -1 stick", false, BAD_SLOT);

        // air empties the slot
        arena.answers("block 2 64 2 slot.container 4 air", true, SUCCESS);
        require(chest->getContainerItem(4).isAir(), "air did not empty the slot");

        // a stack is cut to what the item stacks to; the amount itself stays between 1 and 64
        arena.answers("block 2 64 2 slot.container 5 diamond_sword 3", true, SUCCESS);
        require(holds(chest->getContainerItem(5), "minecraft:diamond_sword", 1), "a sword stacked");
        arena.answers("block 2 64 2 slot.container 5 stick 65", false, "commands.generic.num.tooBig");
        arena.answers("block 2 64 2 slot.container 5 stick 0", false, "commands.generic.num.tooSmall");
        arena.answers("block 2 64 2 slot.container 5 no_such_item", false, "commands.give.item.notFound");

        const RecordingOrigin bare = arena.run("block 5 64 5 slot.container 0 stick", false);
        require(bare.mKeys.back() == NO_CONTAINER && bare.mParameters.back()[0] == "Pos(5,64,5)",
                "a block without a container was not answered with its position");
        // far from any loaded chunk: refused without generating the column
        arena.answers("block 100000 64 100000 slot.container 0 stick", false, NO_CONTAINER);
        require(arena.mLevel->peekChunkPtr(100000 >> 4, 100000 >> 4) == nullptr, "the command loaded a chunk");
    }

    void testComponents(Arena &arena) {
        Container *chest = arena.mLevel->getBlockActors().find(CHEST)->getContainer();

        arena.answers("block 2 64 2 slot.container 6 stick 1 0 "
                      R"({"minecraft:item_lock":{"mode":"lock_in_slot"},"minecraft:keep_on_death":{}})",
                      true, SUCCESS);
        require(ItemProperties::getLockMode(chest->getContainerItem(6)) == ItemLockMode::LockInSlot,
                "the item is not locked in its slot");
        require(ItemProperties::keepsOnDeath(chest->getContainerItem(6)), "the item is not kept on death");

        arena.answers("block 2 64 2 slot.container 7 iron_pickaxe 1 0 "
                      R"({"minecraft:can_destroy": {"blocks": ["stone", "minecraft:dirt"]}})",
                      true, SUCCESS);
        require(chest->getContainerItem(7).mCanBreak == std::vector<std::string>({"minecraft:stone", "minecraft:dirt"}),
                "the blocks the item can destroy were not kept, spaces in the json included");

        arena.answers(R"(block 2 64 2 slot.container 8 stick 1 0 {"minecraft:glow":{}})", false,
                      "'minecraft:glow' is not a supported component.");
        arena.answers(R"(block 2 64 2 slot.container 8 stick 1 0 {"minecraft:item_lock":1})", false,
                      "'minecraft:item_lock' was not an object.");
        arena.answers("block 2 64 2 slot.container 8 stick 1 0 {broken", false, "commands.generic.componentError");
        require(chest->getContainerItem(8).isAir(), "a refused component list still placed the item");
    }

    void testMob(Arena &arena) {
        arena.answers("entity @e[type=zombie] slot.weapon.mainhand 0 stick", false, NO_TARGET);

        MobActor &zombie = arena.spawn("minecraft:zombie", "minecraft:entity_spawned");
        const RecordingOrigin armed = arena.run("entity @e[type=zombie] slot.weapon.mainhand 0 iron_sword", true);
        require(armed.mKeys.back() == SUCCESS_ENTITY && armed.mParameters.back().size() == 5,
                "a mob slot was not answered with the entity form");
        require(holds(zombie.getEquipment().getSlot(MobEquipment::MAINHAND), "minecraft:iron_sword", 1),
                "the zombie holds no sword");

        arena.answers("entity @e[type=zombie] slot.weapon.offhand 0 shield", true, SUCCESS_ENTITY);
        require(holds(zombie.getEquipment().getSlot(MobEquipment::OFFHAND), "minecraft:shield", 1),
                "the zombie holds no shield");

        const char *const armor[] = {"slot.armor.head", "slot.armor.chest", "slot.armor.legs", "slot.armor.feet"};
        for (int piece = 0; piece < 4; piece++) {
            arena.answers(std::string("entity @e[type=zombie] ") + armor[piece] + " 0 dirt", true, SUCCESS_ENTITY);
            require(holds(zombie.getEquipment().getSlot(MobEquipment::HEAD + piece), "minecraft:dirt", 1),
                    std::string(armor[piece]) + " did not take the item");
        }
        arena.answers("entity @e[type=zombie] slot.armor.head 0 keep stone", false, FAILED);

        // slots only a player has, and the one the game lists without handling it
        arena.answers("entity @e[type=zombie] slot.hotbar 0 stick", false, EXCEPTION);
        arena.answers("entity @e[type=zombie] slot.enderchest 0 stick", false, EXCEPTION);
        arena.answers("entity @e[type=zombie] slot.saddle 0 saddle", false, EXCEPTION);
        arena.answers("entity @e[type=zombie] slot.equippable 0 stick", false, EXCEPTION);
    }

    void testMounts(Arena &arena) {
        MobActor &horse = arena.spawn("minecraft:horse", "minecraft:spawn_tame_adult");
        require(horse.getInventoryCapacity() == 2, "a tamed horse has no two slots");

        // anything but a saddle is answered as done and left out
        arena.answers("entity @e[type=horse] slot.saddle 0 stick", true, SUCCESS_ENTITY);
        require(horse.getInventoryContainer().getContainerItem(0).isAir(), "a stick went into the saddle slot");
        arena.answers("entity @e[type=horse] slot.saddle 0 saddle", true, SUCCESS_ENTITY);
        require(holds(horse.getInventoryContainer().getContainerItem(0), "minecraft:saddle", 1),
                "the horse wears no saddle");
        arena.answers("entity @e[type=horse] slot.saddle 0 keep saddle", false, FAILED);

        arena.answers("entity @e[type=horse] slot.armor 0 stick", true, SUCCESS_ENTITY);
        require(horse.getEquipment().getSlot(MobEquipment::BODY).isAir(), "a stick went into the armor slot");
        arena.answers("entity @e[type=horse] slot.armor 0 iron_horse_armor", true, SUCCESS_ENTITY);
        require(holds(horse.getEquipment().getSlot(MobEquipment::BODY), "minecraft:iron_horse_armor", 1),
                "the horse wears no armor");
        arena.answers("entity @e[type=horse] slot.chest 0 stick", false, EXCEPTION);

        MobActor &donkey = arena.spawn("minecraft:donkey", "minecraft:spawn_tame_adult");
        const int capacity = donkey.getInventoryCapacity();
        require(capacity == 16, "a tamed donkey has no sixteen slots");
        arena.answers("entity @e[type=donkey] slot.armor 0 iron_horse_armor", false, EXCEPTION);
        // no chest yet: every chest slot is out of range
        arena.answers("entity @e[type=donkey] slot.chest 0 stick", false, BAD_SLOT);

        donkey.fireEvent(arena.mOwner, "minecraft:on_chest");
        arena.answers("entity @e[type=donkey] slot.chest 0 stick 7", true, SUCCESS_ENTITY);
        require(holds(donkey.getInventoryContainer().getContainerItem(1), "minecraft:stick", 7),
                "the first chest slot is not the one behind the saddle");
        require(donkey.getInventoryContainer().getContainerItem(0).isAir(), "a chest slot reached the saddle slot");

        // fifteen chest slots, the slot number compared once shifted behind the saddle: 0 to 13 pass
        arena.answers("entity @e[type=donkey] slot.chest 13 stick", true, SUCCESS_ENTITY);
        const RecordingOrigin beyond = arena.run("entity @e[type=donkey] slot.chest 14 stick", false);
        require(beyond.mKeys.back() == BAD_SLOT
                && beyond.mParameters.back() == std::vector<std::string>({"14", "0", "14"}),
                "the last chest slot was not refused with the game's range");

        // a mob inventory through slot.inventory
        arena.answers("entity @e[type=donkey] slot.inventory 2 apple", true, SUCCESS_ENTITY);
        require(holds(donkey.getInventoryContainer().getContainerItem(2), "minecraft:apple", 1),
                "slot.inventory did not reach the donkey's inventory");
        arena.answers("entity @e[type=donkey] slot.inventory 16 apple", false, BAD_SLOT);
    }

    void testPlayer(Arena &arena) {
        ServerPlayer &player = arena.player(4001);
        PlayerInventory &inventory = player.getInventory();
        inventory.setSelectedSlot(2);

        arena.answers("entity @a slot.weapon.mainhand 0 diamond_sword", true, SUCCESS_ENTITY);
        require(holds(inventory.getItem(2), "minecraft:diamond_sword", 1), "the held slot took no sword");
        arena.answers("entity @a slot.weapon.offhand 0 shield", true, SUCCESS_ENTITY);
        require(holds(inventory.getOffhand(), "minecraft:shield", 1), "the offhand took no shield");
        arena.answers("entity @a slot.armor.feet 0 diamond_boots", true, SUCCESS_ENTITY);
        require(holds(inventory.getArmor(PlayerInventory::ARMOR_FEET), "minecraft:diamond_boots", 1),
                "the feet took no boots");

        arena.answers("entity @a slot.hotbar 8 apple 3", true, SUCCESS_ENTITY);
        require(holds(inventory.getItem(8), "minecraft:apple", 3), "the last hotbar slot took no apples");
        const RecordingOrigin hotbar = arena.run("entity @a slot.hotbar 9 apple", false);
        require(hotbar.mKeys.back() == BAD_SLOT
                && hotbar.mParameters.back() == std::vector<std::string>({"9", "0", "8"}),
                "a tenth hotbar slot was not refused");

        // slot.inventory counts from behind the hotbar
        arena.answers("entity @a slot.inventory 0 stick", true, SUCCESS_ENTITY);
        require(holds(inventory.getItem(9), "minecraft:stick", 1), "slot.inventory 0 is not the slot behind the hotbar");
        arena.answers("entity @a slot.inventory 26 stick", true, SUCCESS_ENTITY);
        require(holds(inventory.getItem(35), "minecraft:stick", 1), "slot.inventory 26 is not the last slot");
        arena.answers("entity @a slot.inventory 27 stick", false, BAD_SLOT);

        arena.answers("entity @a slot.enderchest 26 emerald 4", true, SUCCESS_ENTITY);
        require(holds(EnderChestInventoryStore::getInstance().get(player.getUniqueId()).getContainerItem(26),
                      "minecraft:emerald", 4),
                "the ender chest took no emeralds");
        arena.answers("entity @a slot.enderchest 27 emerald", false, BAD_SLOT);
        arena.answers("entity @a slot.saddle 0 saddle", false, EXCEPTION);
    }

    void run(const std::filesystem::path &directory) {
        Arena arena(directory);

        arena.answers("", false, "commands.generic.usage");
        arena.answers("block 2 64 2 slot.hotbar 0 stick", false, "commands.generic.usage");
        arena.answers("entity @a slot.nothing 0 stick", false, "commands.generic.usage");

        testBlock(arena);
        testComponents(arena);
        testMob(arena);
        testMounts(arena);
        testPlayer(arena);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-replaceitem-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Replace item regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
