#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginActor.h"
#include "Falcon/Plugin/PluginCommand.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginEvents.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginServerApi.h"
#include "Falcon/Plugin/WasmHost.h"
#include "Core/Archive/ZipArchive.h"
#include "Network/Handler/ChunkStreamHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/AvailableCommandsPacket.h"
#include "Protocol/Types/ItemDefinition.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Server/Commands/CommandMap.h"
#include "Server/Commands/ExecuteCommandOrigin.h"
#include "Server/Commands/PlayerCommandOrigin.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/ObsidianBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <wasmtime.h>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class ServerTickProbe {
public:
    static void removeSimulated(ServerNetworkHandler &owner) {
        owner._removeSimulatedPlayers();
    }

    // what the start of a tick does for the spawns that tick may make
    static void reserveSpawns(ServerNetworkHandler &owner) {
        owner._reserveSimulatedSpawns();
    }

    static void flushPostedTasks(ServerNetworkHandler &owner) {
        std::vector<std::function<void()>> tasks;
        tasks.swap(owner.mMainThreadTasks);
        for (const auto &task: tasks)
            task();
    }

    static EntityDataMap playerMetadata(ServerNetworkHandler &owner, ServerPlayer &player) {
        return owner._buildPlayerData(player);
    }

    static void useItem(ServerNetworkHandler &owner, ServerPlayer &player) {
        owner.mCurrentTick += 10;
        owner._useHeldItem(player);
    }

    static void advanceItemUse(ServerNetworkHandler &owner, ServerPlayer &player, int64_t ticks) {
        owner.mCurrentTick += ticks;
        owner._tickItemUse(player);
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Fixture {
        ServerNetworkHandler owner{"Plugin api", "", 1};
        Level *level = nullptr;
        ServerPlayer *player = nullptr;

        Fixture() {
            level = &owner.getWorlds().getDefault().getOverworld();
            level->insertChunk(LevelChunk(0, 0));
            NetworkIdentifier id;
            player = &owner.getPlayers().try_emplace(id, id, 7001).first->second;
            player->setName("Tester");
            player->getAttributes() = ActorAttributes::createPlayerDefaults();
            player->moveToLevel(*level);
            player->setLoginState(ServerPlayer::LoginState::Spawned);
            player->setPosition(Vector3f(2.5f, 100.0f, 2.5f));
            player->getInventoryManager().attach(player, &owner);
        }

        FalconPlayer *playerHandle() const {
            return reinterpret_cast<FalconPlayer *>(player);
        }

        FalconLevel *levelHandle() const {
            return reinterpret_cast<FalconLevel *>(level);
        }
    };

    void customItems(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin plugin;
        plugin.mDescription.mName = "CustomItems";
        plugin.mEnabled = true;
        FalconPlugin *handle = PluginManager::toHandle(plugin);
        FalconCustomItemDescriptor legacy{};
        legacy.identifier = "test:legacy";
        legacy.maxStackSize = 12;
        require(api.registerCustomItem(handle, &legacy) == 1, "legacy descriptor was rejected");
        require(CustomContentRegistry::getInstance().findItem(legacy.identifier)->mMaxStackSize == 12,
                "legacy descriptor defaults changed");

        FalconCustomItemDescriptorV2 snack{};
        snack.item.identifier = "test:snack";
        snack.item.maxStackSize = 16;
        snack.components = "[]";
        snack.componentsSize = 2;
        require(api.registerCustomItemWithComponents(handle, &snack) == 0, "a non-object was accepted");
        snack.components = "{";
        snack.componentsSize = 1;
        require(api.registerCustomItemWithComponents(handle, &snack) == 0, "malformed components were accepted");
        snack.componentsSize = FALCON_CUSTOM_ITEM_COMPONENTS_MAX_BYTES + 1;
        require(api.registerCustomItemWithComponents(handle, &snack) == 0, "oversized component bytes were accepted");
        require(CustomContentRegistry::getInstance().findItem(snack.item.identifier) == nullptr,
                "a rejected registration reserved the identifier");

        constexpr char components[] = R"({
            "minecraft:food": {"nutrition": 4, "saturation_modifier": 0.5},
            "minecraft:use_modifiers": {"use_duration": 1.0},
            "minecraft:use_animation": "eat",
            "minecraft:max_stack_size": 8
        })";
        snack.components = components;
        snack.componentsSize = sizeof(components) - 1;
        require(api.registerCustomItemWithComponents(handle, &snack) == 1, "the component descriptor was rejected");
        const ItemStack stack = fixture.owner.createItemStack(snack.item.identifier, 2);
        require(!stack.isAir(), "the component item is unavailable in the inventory");
        const Tag *wireComponents = stack.mDefinition->getComponentData().get("components");
        const Tag *food = wireComponents == nullptr ? nullptr : wireComponents->get("minecraft:food");
        require(food != nullptr && food->getInt("nutrition", 0) == 4, "food was omitted from the client definition");
        require(ServerNetworkHandler::useDurationTicksFor(stack) == 20, "use duration did not come from the component");
        require(CustomContentRegistry::getInstance().findItem(snack.item.identifier)->mMaxStackSize == 8,
                "the explicit stack-size component did not override the descriptor");

        ServerPlayer &player = *fixture.player;
        player.getInventory().setItemInHand(stack);
        player.setFood(20.0f);
        ServerTickProbe::useItem(fixture.owner, player);
        require(!player.getFlags().get(ActorFlag::UsingItem), "the snack could be eaten at full hunger");
        player.setFood(12.0f);
        player.setSaturation(0.0f);
        ServerTickProbe::useItem(fixture.owner, player);
        require(player.getFlags().get(ActorFlag::UsingItem), "the snack did not enter the normal use path");
        ServerTickProbe::advanceItemUse(fixture.owner, player, 19);
        require(player.getFood() == 12.0f && player.getInventory().getItemInHand().mCount == 2,
                "the snack was consumed before its use duration");
        ServerTickProbe::advanceItemUse(fixture.owner, player, 1);
        require(player.getFood() == 16.0f && player.getSaturation() == 4.0f
                        && player.getInventory().getItemInHand().mCount == 1,
                "the snack did not apply its food component and consume one item");
        player.getInventory().clear();
    }

    void wasmCustomItems() {
        constexpr char module[] = R"((module
          (import "falcon:plugin/content@1.0.6" "register-custom-item-with-components"
            (func $register (param i32) (result i32)))
          (memory (export "memory") 1)
          (data (i32.const 128) "test:wasm_item")
          (data (i32.const 160) "{\22minecraft:food\22:{\22nutrition\22:3}}")
          (func (export "falcon:plugin/plugin@1.0.6#on-enable") (result i32)
            (i32.store (i32.const 0) (i32.const 128))
            (i32.store (i32.const 4) (i32.const 14))
            (i32.store (i32.const 60) (i32.const 160))
            (i32.store (i32.const 64) (i32.const 34))
            (call $register (i32.const 0))))
        )";
        LoadedPlugin plugin;
        plugin.mDescription.mName = "WasmItems";
        plugin.mDescription.mVersion = "1.0.0";
        plugin.mDescription.mMain = "items.wasm";
        const auto directory = ServerPaths::getRoot() / "plugins" / "WasmItems";
        plugin.mDirectory = directory.string();
        plugin.mDataFolder = (directory / "data").string();
        std::filesystem::create_directories(plugin.mDataFolder);

        wasm_byte_vec_t wasm;
        require(wasmtime_wat2wasm(module, sizeof(module) - 1, &wasm) == nullptr, "the item module did not assemble");
        std::ofstream(directory / plugin.mDescription.mMain, std::ios::binary)
                .write(wasm.data, (std::streamsize) wasm.size);
        wasm_byte_vec_delete(&wasm);
        require(WasmHost::load(plugin), "the item module did not load");
        require(plugin.mCallbacks.onEnable(plugin.mCallbacks.userData) == 1, "the Wasm components were rejected");
        const auto *item = CustomContentRegistry::getInstance().findItem("test:wasm_item");
        require(item != nullptr && item->mIsFood && item->mNutrition == 3, "the Wasm food payload was not registered");
    }

    void effects(const FalconServerApi &api, Fixture &fixture) {
        FalconEntity *body = api.playerEntity(fixture.playerHandle());
        require(api.entityAddEffect(body, FALCON_EFFECT_SPEED, 200, 1, 1) == 1, "speed was not added");
        require(api.entityAddEffect(body, FALCON_EFFECT_SPEED, 100, 0, 1) == 0,
                "a weaker speed replaced a stronger one");
        require(api.entityAddEffect(body, 255, 100, 0, 1) == 0, "an unknown effect id was accepted");
        require(api.entityAddEffect(body, FALCON_EFFECT_HASTE, 0, 0, 1) == 0, "a zero duration was accepted");
        require(api.entityAddEffect(body, FALCON_EFFECT_HASTE, 100, 256, 1) == 0, "amplifier 256 was accepted");
        require(api.entityAddEffect(body, FALCON_EFFECT_NIGHT_VISION, FALCON_EFFECT_INFINITE, 0, 0) == 1,
                "an endless effect was refused");

        FalconEffect effect{};
        require(api.entityEffect(body, FALCON_EFFECT_SPEED, &effect) == 1, "speed is not readable");
        require(effect.type == FALCON_EFFECT_SPEED && effect.amplifier == 1 && effect.duration == 200
                        && effect.showParticles == 1,
                "speed came back as " + std::to_string(effect.amplifier) + "/" + std::to_string(effect.duration));
        require(api.entityEffect(body, FALCON_EFFECT_NIGHT_VISION, &effect) == 1
                        && effect.duration == FALCON_EFFECT_INFINITE && effect.showParticles == 0,
                "night vision is not endless");

        require(api.entityEffectCount(body) == 2, "two effects run");
        require(api.entityEffectAt(body, 0, &effect) == 1 && effect.type == FALCON_EFFECT_SPEED,
                "effects are listed by id");
        require(api.entityEffectAt(body, 2, &effect) == 0, "an index past the end found an effect");

        require(api.entityRemoveEffect(body, FALCON_EFFECT_SPEED) == 1, "speed was not removed");
        require(api.entityRemoveEffect(body, FALCON_EFFECT_SPEED) == 0, "a missing effect was removed");
        api.entityClearEffects(body);
        require(api.entityEffectCount(body) == 0, "clearing left effects");
    }

    void attributes(const FalconServerApi &api, Fixture &fixture) {
        FalconEntity *body = api.playerEntity(fixture.playerHandle());
        const uint32_t count = api.entityAttributeCount(body);
        require(count == fixture.player->getAttributes().getAll().size(), "the attribute count differs");
        require(api.entityAttributeName(body, count) == nullptr, "an index past the end has a name");

        bool listed = false;
        for (uint32_t index = 0; index < count; ++index)
            listed = listed || std::string(api.entityAttributeName(body, index)) == "minecraft:movement";
        require(listed, "movement is not listed");

        FalconAttribute attribute{};
        require(api.entityAttribute(body, "minecraft:movement", &attribute) == 1, "movement is not readable");
        require(std::fabs(attribute.value - 0.1f) < 1e-6f && attribute.defaultValue == attribute.value,
                "movement starts at " + std::to_string(attribute.value));
        require(api.entityAttribute(body, "minecraft:nothing", &attribute) == 0, "an unknown attribute was found");

        require(api.entitySetAttribute(body, "minecraft:movement", 0.2f) == 1, "movement was not set");
        require(std::fabs(fixture.player->getAttributes().get("minecraft:movement") - 0.2f) < 1e-6f,
                "the new movement did not reach the player");
        require(api.entitySetAttribute(body, "minecraft:knockback_resistance", 5.0f) == 0,
                "a value above the maximum was accepted");
        require(api.entitySetAttribute(body, "minecraft:movement", std::nanf("")) == 0, "nan was accepted");
        require(api.entitySetAttribute(body, "minecraft:nothing", 1.0f) == 0, "an unknown attribute was set");
        FalconAttribute previousHealth{};
        require(api.entityAttribute(body, "minecraft:health", &previousHealth) == 1, "health was not reported");
        require(api.entitySetMaxHealth(body, 22) == 1, "maximum health was not set");
        FalconAttribute health{};
        require(api.entityAttribute(body, "minecraft:health", &health) == 1 && health.maximum == 22.0f,
                "increased maximum health was not reported");
        require(api.entitySetMaxHealth(body, 10) == 1, "maximum health was not lowered");
        require(api.entityAttribute(body, "minecraft:health", &health) == 1 && health.maximum == 10.0f &&
                        health.value <= 10.0f,
                "current health was not clamped after lowering the maximum");
        require(api.entitySetMaxHealth(body, std::nanf("")) == 0, "nan maximum health was accepted");
        require(api.entitySetMaxHealth(body, previousHealth.maximum) == 1, "maximum health was not restored");
        require(api.entitySetAttribute(body, "minecraft:health", previousHealth.value) == 1, "health was not restored");
    }

    // nobody listens on the fixture's connection: these only have to reach the send path without failing
    void soundsAndParticles(const FalconServerApi &api, Fixture &fixture) {
        const FalconVec3 at{2.5, 100.0, 2.5};
        api.playerPlaySound(fixture.playerHandle(), "random.levelup", at, 1.0f, 1.0f);
        api.playerPlaySound(fixture.playerHandle(), nullptr, at, 1.0f, 1.0f);
        api.levelPlaySound(fixture.levelHandle(), "mob.wolf.bark", at, 1.0f, 1.2f);
        api.playerStopSound(fixture.playerHandle(), "random.levelup");
        api.playerStopSound(fixture.playerHandle(), nullptr);
        api.playerSpawnParticle(fixture.playerHandle(), "minecraft:heart_particle", at);
        api.levelSpawnParticle(fixture.levelHandle(), "minecraft:heart_particle", at);
        api.levelSpawnParticle(fixture.levelHandle(), "", at);
    }

    void scoreboard(const FalconServerApi &api, Fixture &fixture) {
        FalconEntity *body = api.playerEntity(fixture.playerHandle());
        require(api.scoreboardAddObjective("kills", "Kills") == 1, "the objective was not added");
        require(api.scoreboardAddObjective("kills", "Again") == 0, "a taken name was added twice");
        require(api.scoreboardObjectiveCount() == 1 && std::string(api.scoreboardObjectiveName(0)) == "kills",
                "the objective is not listed");
        require(std::string(api.scoreboardObjectiveDisplayName("kills")) == "Kills", "the display name was lost");

        int32_t score = 0;
        require(api.scoreboardEntityScore("kills", body, &score) == 0, "a new participant has a score");
        require(api.scoreboardSetEntityScore("kills", body, 3) == 1, "the player score was not set");
        require(api.scoreboardSetEntityScore("missing", body, 3) == 0, "a missing objective took a score");
        require(api.scoreboardEntityScore("kills", body, &score) == 1 && score == 3, "the player score is not 3");
        require(api.scoreboardSetFakePlayerScore("kills", "Herobrine", 99) == 1, "the fake score was not set");
        require(api.scoreboardFakePlayerScore("kills", "Herobrine", &score) == 1 && score == 99,
                "the fake score is not 99");

        require(api.scoreboardScoreCount("kills") == 2, "two participants have scores");
        require(std::string(api.scoreboardScoreAt("kills", 0, &score)) == "Tester" && score == 3,
                "the first score is not the player's");
        require(api.scoreboardScoreEntity("kills", 0) == body, "the first score does not lead to the player");
        require(std::string(api.scoreboardScoreAt("kills", 1, &score)) == "Herobrine" && score == 99,
                "the second score is not the fake player's");
        require(api.scoreboardScoreEntity("kills", 1) == nullptr, "a fake player has an entity");

        require(api.scoreboardSetDisplay(FALCON_DISPLAY_SLOT_SIDEBAR, "kills", 0) == 1, "the sidebar was not set");
        require(std::string(api.scoreboardDisplayObjective(FALCON_DISPLAY_SLOT_SIDEBAR)) == "kills",
                "the sidebar does not show the objective");
        require(api.scoreboardSetDisplay(7, "kills", 0) == 0, "an unknown slot was accepted");
        require(api.scoreboardSetDisplay(FALCON_DISPLAY_SLOT_SIDEBAR, nullptr, 0) == 1
                        && api.scoreboardDisplayObjective(FALCON_DISPLAY_SLOT_SIDEBAR) == nullptr,
                "the sidebar was not cleared");

        require(api.scoreboardResetFakePlayerScore("kills", "Herobrine") == 1, "the fake score was not reset");
        require(api.scoreboardFakePlayerScore("kills", "Herobrine", &score) == 0, "a reset score is still there");
        require(api.scoreboardResetEntityScore("kills", body) == 1, "the player score was not reset");
        require(api.scoreboardRemoveObjective("kills") == 1 && api.scoreboardObjectiveCount() == 0,
                "the objective was not removed");
    }

    void containers(const FalconServerApi &api, Fixture &fixture) {
        const Vector3i chestAt(4, 100, 4);
        fixture.level->setBlock(chestAt, VanillaBlocks::fromIdentifier("minecraft:chest")->toBlockState(), false);
        fixture.level->getBlockActors().getOrCreate<ChestBlockActor>(chestAt);
        const FalconBlockPos chest{4, 100, 4};
        const FalconBlockPos nothing{6, 100, 6};

        require(api.levelContainerSize(fixture.levelHandle(), chest) == 27, "a chest does not have 27 slots");
        require(api.levelContainerSize(fixture.levelHandle(), nothing) == 0, "an empty spot has slots");
        FalconItem *diamonds = api.itemCreate("minecraft:diamond", 3);
        require(diamonds != nullptr, "no diamond item");
        require(api.levelSetContainerItem(fixture.levelHandle(), chest, 5, diamonds) == 1, "the chest took nothing");
        require(api.levelSetContainerItem(fixture.levelHandle(), chest, 27, diamonds) == 0, "slot 27 was written");
        require(api.levelSetContainerItem(fixture.levelHandle(), nothing, 0, diamonds) == 0, "air took an item");

        FalconItem *stored = api.levelContainerItem(fixture.levelHandle(), chest, 5);
        require(stored != nullptr && std::string(api.itemIdentifier(stored)) == "minecraft:diamond"
                        && api.itemCount(stored) == 3,
                "the chest does not hold the diamonds");
        api.itemDestroy(stored);
        FalconItem *empty = api.levelContainerItem(fixture.levelHandle(), chest, 6);
        require(empty != nullptr && api.itemIsEmpty(empty) != 0, "an untouched slot is not empty");
        api.itemDestroy(empty);
        require(api.levelSetContainerItem(fixture.levelHandle(), chest, 5, nullptr) == 1, "the slot was not cleared");
        stored = api.levelContainerItem(fixture.levelHandle(), chest, 5);
        require(api.itemIsEmpty(stored) != 0, "a cleared slot kept its item");
        api.itemDestroy(stored);

        require(api.playerEnderChestSize(fixture.playerHandle()) == 27, "the ender chest does not have 27 slots");
        require(api.playerSetEnderChestItem(fixture.playerHandle(), 0, diamonds) == 1, "the ender chest took nothing");
        stored = api.playerEnderChestItem(fixture.playerHandle(), 0);
        require(api.itemCount(stored) == 3, "the ender chest does not hold the diamonds");
        api.itemDestroy(stored);
        require(api.playerEnderChestItem(fixture.playerHandle(), 27) == nullptr, "ender chest slot 27 exists");
        api.itemDestroy(diamonds);

        const Vector3i signAt(8, 100, 8);
        const Block *standingSign = VanillaBlocks::fromIdentifier("minecraft:standing_sign");
        fixture.level->setBlock(signAt, standingSign->toBlockState(), false);
        fixture.level->getBlockActors().getOrCreate<SignBlockActor>(signAt);
        const FalconBlockPos sign{8, 100, 8};
        require(api.levelSignText(fixture.levelHandle(), nothing, FALCON_SIGN_FRONT) == nullptr, "air has text");
        require(std::string(api.levelSignText(fixture.levelHandle(), sign, FALCON_SIGN_FRONT)).empty(),
                "a new sign has text");
        require(api.levelSetSignText(fixture.levelHandle(), sign, FALCON_SIGN_BACK, "Hello") == 1,
                "the sign was not written");
        require(std::string(api.levelSignText(fixture.levelHandle(), sign, FALCON_SIGN_BACK)) == "Hello",
                "the back of the sign does not read Hello");
        require(std::string(api.levelSignText(fixture.levelHandle(), sign, FALCON_SIGN_FRONT)).empty(),
                "writing the back changed the front");
        require(api.levelSetSignText(fixture.levelHandle(), sign, 2, "Hello") == 0, "an unknown side was written");
    }

    void persistentData(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin first;
        first.mDescription.mName = "First";
        LoadedPlugin second;
        second.mDescription.mName = "Second";
        FalconPlugin *one = PluginManager::toHandle(first);
        FalconPlugin *two = PluginManager::toHandle(second);

        FalconEntity *body = api.playerEntity(fixture.playerHandle());
        require(api.entityData(one, body, "rank") == nullptr, "a new player has data");
        require(api.entitySetData(one, body, "rank", "builder") == 1, "the player data was not set");
        require(api.entitySetData(two, body, "rank", "guest") == 1, "the second plugin could not set its key");
        require(std::string(api.entityData(one, body, "rank")) == "builder", "a plugin read another's value");
        require(std::string(api.entityData(two, body, "rank")) == "guest", "the second value was lost");
        require(api.entitySetData(one, body, "", "x") == 0, "an empty key was accepted");
        require(api.entityDataCount(one, body) == 1 && std::string(api.entityDataKey(one, body, 0)) == "rank",
                "the keys are not listed per plugin");

        const Tag saved = fixture.player->saveNbt("world");
        NetworkIdentifier id;
        ServerPlayer loaded(id, 7002);
        loaded.loadNbt(saved, fixture.owner.getCodecContext());
        const auto stored = loaded.getDynamicProperties().find("First:rank");
        require(stored != loaded.getDynamicProperties().end() && stored->second.mString == "builder",
                "the player data did not survive the save");

        require(api.entitySetData(one, body, "rank", nullptr) == 1 && api.entityData(one, body, "rank") == nullptr,
                "the player data was not removed");

        FalconItem *sword = api.itemCreate("minecraft:diamond_sword", 1);
        require(api.itemSetData(one, sword, "owner", "Tester") == 1, "the item data was not set");
        require(std::string(api.itemData(one, sword, "owner")) == "Tester", "the item data is not readable");
        require(api.itemData(two, sword, "owner") == nullptr, "a plugin read another's item value");
        FalconItem *copy = api.itemCopy(sword);
        require(std::string(api.itemData(one, copy, "owner")) == "Tester", "a copy lost the item data");

        uint32_t length = 0;
        const uint8_t *nbt = api.itemNbt(sword, &length);
        require(nbt != nullptr && length > 0, "an item with data has no tag");
        const std::vector<uint8_t> bytes(nbt, nbt + length);
        FalconItem *plain = api.itemCreate("minecraft:diamond_sword", 1);
        require(api.itemSetNbt(plain, bytes.data(), (uint32_t) bytes.size()) == 1, "the tag was not written");
        require(std::string(api.itemData(one, plain, "owner")) == "Tester", "the written tag lost the data");
        const uint8_t garbage[3] = {0xff, 0x01, 0x02};
        require(api.itemSetNbt(plain, garbage, 3) == 0, "garbage was taken as a tag");
        require(api.itemSetNbt(plain, nullptr, 0) == 1 && api.itemData(one, plain, "owner") == nullptr,
                "an empty tag did not clear the item");

        require(api.itemSetData(one, sword, "owner", nullptr) == 1 && api.itemData(one, sword, "owner") == nullptr,
                "the item data was not removed");
        api.itemDestroy(plain);
        api.itemDestroy(copy);
        api.itemDestroy(sword);

        require(api.entityNbt(body, &length) != nullptr && length > 0, "the player has no saved state");
        require(api.entitySetNbt(body, garbage, 3) == 0, "a player was loaded from a tag");
    }

    struct CommandProbe {
        int calls = 0;
        std::vector<std::string> arguments;
        bool player = false;
    };

    int probeHandler(FalconCommandSender *sender, const char *const *arguments, uint32_t count, void *userData) {
        auto &probe = *static_cast<CommandProbe *>(userData);
        probe.calls++;
        probe.arguments.assign(arguments, arguments + count);
        probe.player = PluginServerApi::get().senderPlayer(sender) != nullptr;
        return 1;
    }

    void commands(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin owned;
        owned.mDescription.mName = "Probe";
        owned.mEnabled = true;
        LoadedPlugin other;
        other.mDescription.mName = "Other";
        FalconPlugin *handle = PluginManager::toHandle(owned);
        CommandProbe probe;
        const FalconCommandDescriptor descriptor{"probe", "Probes", "/probe", FALCON_PERMISSION_ANY, &probeHandler,
                                                 &probe};
        require(api.registerCommand(handle, &descriptor) == 1, "the probe command was not registered");

        const char *modes[2] = {"fast", "slow"};
        const FalconCommandParameter parameters[2] = {{"mode", FALCON_COMMAND_PARAM_ENUM, 0, modes, 2},
                                                      {"count", FALCON_COMMAND_PARAM_INT, 1, nullptr, 0}};
        require(api.commandAddOverload(handle, "probe", parameters, 2) == 1, "the overload was refused");
        require(api.commandAddOverload(PluginManager::toHandle(other), "probe", parameters, 2) == 0,
                "another plugin changed the command");
        require(api.commandAddOverload(handle, "missing", parameters, 2) == 0, "a missing command took an overload");
        const FalconCommandParameter emptyEnum{"mode", FALCON_COMMAND_PARAM_ENUM, 0, nullptr, 0};
        require(api.commandAddOverload(handle, "probe", &emptyEnum, 1) == 0, "an enum without values was accepted");

        const Command *command = fixture.owner.getCommands().getCommand("probe");
        require(command != nullptr, "the command is not in the map");
        const std::vector<CommandOverloadData> overloads = command->getOverloads();
        require(overloads.size() == 1 && overloads[0].mParameters.size() == 2, "the overload replaced nothing");
        require(overloads[0].mParameters[0].mHasEnumData
                        && overloads[0].mParameters[0].mEnumData.mValues == std::vector<std::string>{"fast", "slow"},
                "the enum values were lost");
        require(overloads[0].mParameters[1].mType == CommandParamType::Int && overloads[0].mParameters[1].mOptional,
                "the int parameter was lost");
        const FalconCommandParameter rest{"message", FALCON_COMMAND_PARAM_RAWTEXT, 1, nullptr, 0};
        require(api.commandAddOverload(handle, "probe", &rest, 1) == 1, "the raw text overload was refused");
        const std::vector<CommandOverloadData> both = command->getOverloads();
        require(both.size() == 2 && both[1].mParameters.size() == 1
                        && both[1].mParameters[0].mType == CommandParamType::RawText && both[1].mParameters[0].mOptional,
                "the raw text parameter was lost");

        struct CommandPacketProbe {
            FalconPlayer *recipient = nullptr;
            std::string payload;
        } sent;
        const uint64_t sendId = api.subscribe(
                handle, FALCON_EVENT_DATA_PACKET_SEND, FALCON_PRIORITY_NORMAL, 0,
                [](FalconEvent *event, void *userData) {
                    const FalconServerApi &api = PluginServerApi::get();
                    if (api.eventPacketId(event) != static_cast<uint32_t>(MinecraftPacketIds::AvailableCommands))
                        return;
                    auto &sent = *static_cast<CommandPacketProbe *>(userData);
                    sent.recipient = api.eventPlayer(event);
                    uint32_t length = 0;
                    const uint8_t *payload = api.eventPacketData(event, &length);
                    sent.payload.assign(reinterpret_cast<const char *>(payload), length);
                    api.eventSetCancelled(event, 1);
                },
                &sent);
        const int didSend = api.playerSendCommands(fixture.playerHandle());
        api.unsubscribe(sendId);
        require(didSend == 1 && sent.recipient == fixture.playerHandle() && !sent.payload.empty(),
                "available commands did not reach the plugin send event");
        AvailableCommandsPacket packet;
        ReadOnlyBinaryStream stream(sent.payload);
        packet.read(stream, fixture.owner.getCodecContext());
        const auto advertised = std::find_if(packet.mCommands.begin(), packet.mCommands.end(),
                                             [](const CommandData &entry) { return entry.mName == "probe"; });
        require(advertised != packet.mCommands.end() && advertised->mOverloads.size() == 2
                        && advertised->mOverloads[1].mParameters.size() == 1
                        && advertised->mOverloads[1].mParameters[0].mType == CommandParamType::RawText
                        && advertised->mOverloads[1].mParameters[0].mOptional,
                "the outgoing list lost the optional raw text overload");

        fixture.player->setLoginState(ServerPlayer::LoginState::StartGameSent);
        require(api.playerSendCommands(fixture.playerHandle()) == 0, "commands went to a player not in the world");
        fixture.player->setLoginState(ServerPlayer::LoginState::Spawned);

        require(api.serverExecuteCommand("probe fast 3") == 1 && probe.calls == 1 && !probe.player,
                "the console did not run the command");
        require(probe.arguments == std::vector<std::string>{"fast", "3"}, "the console arguments were lost");
        require(api.playerExecuteCommand(fixture.playerHandle(), "probe slow") == 1 && probe.calls == 2 && probe.player,
                "the player did not run the command");
        require(api.serverExecuteCommand("") == 0, "an empty line ran");

        const char *aliases[3] = {"pr", "PROBER", "kick"};
        require(api.commandSetAliases(handle, "probe", aliases, 3) == 1, "the aliases were refused");
        require(api.commandSetAliases(PluginManager::toHandle(other), "probe", aliases, 1) == 0,
                "another plugin gave the command aliases");
        require(api.serverExecuteCommand("pr fast") == 1 && probe.calls == 3, "an alias did not run the command");
        require(api.serverExecuteCommand("prober slow") == 1 && probe.calls == 4,
                "an alias in another case did not run the command");
        require(fixture.owner.getCommands().getCommand("kick") != command, "an alias took another command's name");
        require(command->getAliases() == std::vector<std::string>{"pr", "prober"}, "the aliases were not kept");
        const char *replacement[1] = {"pb"};
        require(api.commandSetAliases(handle, "probe", replacement, 1) == 1, "the new aliases were refused");
        require(fixture.owner.getCommands().getCommand("pr") == nullptr
                        && fixture.owner.getCommands().getCommand("pb") == command,
                "the old aliases stayed in place of the new ones");
        fixture.owner.getCommands().unregisterCommand("probe");
        require(fixture.owner.getCommands().getCommand("pb") == nullptr, "an alias outlived its command");
    }

    struct MessageProbe {
        int calls = 0;
        std::string message;
        bool isDefault = false;
        std::string reason;
        const char *replacement = nullptr;
    };

    void messageHandler(FalconEvent *event, void *userData) {
        const FalconServerApi &api = PluginServerApi::get();
        auto &probe = *static_cast<MessageProbe *>(userData);
        probe.calls++;
        probe.message = api.eventMessage(event);
        probe.isDefault = api.eventMessageIsDefault(event) != 0;
        probe.reason = api.eventCause(event);
        if (probe.replacement != nullptr)
            api.eventSetMessage(event, probe.replacement);
    }

    void joinAndLeaveMessages(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin listener;
        listener.mDescription.mName = "Greeter";
        listener.mEnabled = true;
        FalconPlugin *handle = PluginManager::toHandle(listener);
        MessageProbe join;
        MessageProbe quit;
        const uint64_t joinId = api.subscribe(handle, FALCON_EVENT_PLAYER_JOIN, FALCON_PRIORITY_NORMAL, 0,
                                              &messageHandler, &join);
        const uint64_t quitId = api.subscribe(handle, FALCON_EVENT_PLAYER_QUIT, FALCON_PRIORITY_NORMAL, 0,
                                              &messageHandler, &quit);
        EventBus &bus = fixture.owner.getEventBus();

        PlayerJoinMessageBeforeEvent untouched(*fixture.player, "§eTester joined the game");
        bus.before().mPlayerJoinMessage.emit(untouched);
        require(join.calls == 1 && join.message == "§eTester joined the game" && join.isDefault,
                "the join event did not carry the game's message");
        require(untouched.mDefault, "a message left alone stopped being the game's own");

        join.replacement = "§aTester is here";
        PlayerJoinMessageBeforeEvent replaced(*fixture.player, "§eTester joined the game");
        bus.before().mPlayerJoinMessage.emit(replaced);
        require(!replaced.mDefault && replaced.mMessage == "§aTester is here", "the join message was not replaced");

        join.replacement = "";
        PlayerJoinMessageBeforeEvent silenced(*fixture.player, "§eTester joined the game");
        bus.before().mPlayerJoinMessage.emit(silenced);
        require(!silenced.mDefault && silenced.mMessage.empty(), "the join message was not emptied");

        quit.replacement = "§cTester went away";
        PlayerLeaveMessageBeforeEvent leaving(*fixture.player, "§eTester left the game", "Kicked by an operator");
        bus.before().mPlayerLeaveMessage.emit(leaving);
        require(quit.calls == 1 && quit.isDefault && quit.message == "§eTester left the game",
                "the quit event did not carry the game's message");
        require(quit.reason == "Kicked by an operator", "the quit event lost the reason");
        require(!leaving.mDefault && leaving.mMessage == "§cTester went away", "the leave message was not replaced");

        // a simulated player goes through the real join and leave, kicked with a reason
        join.replacement = nullptr;
        quit.replacement = nullptr;
        FalconPlayer *bot = api.serverSpawnSimulatedPlayer("Greeted", fixture.levelHandle(),
                                                           FalconVec3{4.5, 100.0, 4.5}, 0.0f,
                                                           FALCON_GAME_MODE_SURVIVAL);
        require(bot != nullptr, "the greeted player was refused");
        require(join.calls == 4 && join.message == "§eGreeted joined the game" && join.isDefault,
                "joining did not fire the join event before the message");
        const NetworkIdentifier botId = reinterpret_cast<ServerPlayer *>(bot)->getNetworkIdentifier();
        fixture.owner._disconnect(botId, DisconnectReason::Kicked, "Too loud");
        ServerTickProbe::removeSimulated(fixture.owner);
        require(quit.calls == 2 && quit.message == "§eGreeted left the game" && quit.reason == "Too loud",
                "leaving did not fire the quit event with the kick reason");

        api.unsubscribe(joinId);
        api.unsubscribe(quitId);

        MessageProbe chat;
        chat.replacement = "said for the player";
        const uint64_t chatId = api.subscribe(handle, FALCON_EVENT_PLAYER_CHAT, FALCON_PRIORITY_NORMAL, 0,
                                              &messageHandler, &chat);
        require(api.playerChat(fixture.playerHandle(), "hello\nsecond line") == 1 && chat.calls == 1
                        && chat.message == "hello",
                "a chat line for the player did not go through the chat event, one line only");
        require(api.playerChat(fixture.playerHandle(), "   ") == 0 && chat.calls == 1, "a blank chat line was said");
        api.unsubscribe(chatId);
    }

    bool hasOutput(ServerNetworkHandler &owner, const std::string &identifier) {
        for (const ItemStack &output: owner.getRecipeOutputs()) {
            if (output.mDefinition != nullptr && output.mDefinition->getIdentifier() == identifier)
                return true;
        }
        return false;
    }

    void recipes(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin owned;
        owned.mDescription.mName = "Chef";
        FalconPlugin *handle = PluginManager::toHandle(owned);
        const size_t before = fixture.owner.getRecipeOutputs().size();

        const char *emeralds[4] = {"minecraft:emerald", "minecraft:emerald", "minecraft:emerald", "minecraft:emerald"};
        const FalconRecipeDescriptor shaped{"chef:emerald_block", 1, 2, 2, emeralds, 4, "minecraft:emerald_block", 1};
        require(api.registerRecipe(handle, &shaped) == 1, "the shaped recipe was refused");
        require(api.registerRecipe(handle, &shaped) == 0, "a taken identifier was registered twice");
        const FalconRecipeDescriptor wrongSize{"chef:wrong", 1, 2, 2, emeralds, 3, "minecraft:emerald_block", 1};
        require(api.registerRecipe(handle, &wrongSize) == 0, "a 2 x 2 recipe with three cells was accepted");
        const char *unknown[1] = {"minecraft:not_an_item"};
        const FalconRecipeDescriptor bad{"chef:bad", 0, 0, 0, unknown, 1, "minecraft:emerald", 1};
        require(api.registerRecipe(handle, &bad) == 0, "an unknown ingredient was accepted");

        require(fixture.owner.getRecipeOutputs().size() == before + 1, "the recipe book did not grow");
        require(hasOutput(fixture.owner, "minecraft:emerald_block"), "the recipe book misses the new recipe");

        require(api.removeRecipe("chef:emerald_block") == 1, "the plugin recipe was not removed");
        require(fixture.owner.getRecipeOutputs().size() == before, "the recipe book kept the removed recipe");

        // the first recipe in the book is a vanilla one: pack and plugin recipes come after
        const uint32_t source = fixture.owner.getRecipeSourceIndices().front();
        const std::string vanilla = CraftingRecipeTable::getRecipes()[source].mRecipeId;
        require(api.removeRecipe(vanilla.c_str()) == 1, "a vanilla recipe was not removed");
        require(fixture.owner.getRecipeOutputs().size() < before, "the vanilla recipe stayed in the book");
        require(api.removeRecipe(vanilla.c_str()) == 0, "a vanilla recipe was removed twice");
        require(api.removeRecipe("chef:never") == 0, "an unknown recipe was removed");
        CustomContentRegistry::getInstance().getDisabledRecipes().clear();
    }

    void world(const FalconServerApi &api, Fixture &fixture) {
        FalconLevel *level = fixture.levelHandle();
        require(api.levelGameRuleCount(level) > 0, "the level has no game rules");
        require(std::string(api.levelGameRule(level, "keepinventory")) == "false", "keepinventory starts on");
        require(api.levelSetGameRule(level, "keepinventory", "true") == 1, "keepinventory was not set");
        require(std::string(api.levelGameRule(level, "keepinventory")) == "true", "keepinventory stayed off");
        require(api.levelSetGameRule(level, "keepinventory", "maybe") == 0, "a bad value was accepted");
        require(api.levelGameRule(level, "nothing") == nullptr, "an unknown rule has a value");

        require(api.serverSetDifficulty(FALCON_DIFFICULTY_HARD) == 1, "the difficulty was refused");
        require(api.serverDifficulty() == FALCON_DIFFICULTY_HARD, "the difficulty did not change");
        require(api.serverSetDifficulty(4) == 0, "difficulty 4 was accepted");

        const char *biome = api.levelBiome(level, FalconBlockPos{2, 64, 2});
        require(biome == nullptr || std::string(biome).rfind("minecraft:", 0) == 0, "the biome has no namespace");
        require(api.levelBiome(level, FalconBlockPos{2, 100000, 2}) == nullptr, "a biome above the world exists");

        fixture.level->setBlock(Vector3i(2, 95, 2), VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState(),
                                false);
        FalconBlockHit hit{};
        require(api.levelRaycastBlock(level, FalconVec3{2.5, 99.0, 2.5}, FalconVec3{0.0, -2.0, 0.0}, 10.0, 0, 0, &hit)
                        == 1,
                "the ray missed the stone");
        require(hit.block.x == 2 && hit.block.y == 95 && hit.block.z == 2 && hit.face == FALCON_FACE_UP,
                "the ray hit " + std::to_string(hit.block.y) + " face " + std::to_string(hit.face));
        require(std::fabs(hit.distance - 3.0) < 1e-4 && std::fabs(hit.position.y - 96.0) < 1e-4,
                "the ray stopped at " + std::to_string(hit.distance));
        require(api.levelRaycastBlock(level, FalconVec3{2.5, 99.0, 2.5}, FalconVec3{0.0, -1.0, 0.0}, 2.0, 0, 0, &hit)
                        == 0,
                "a short ray reached the stone");
        require(api.levelRaycastBlock(level, FalconVec3{2.5, 99.0, 2.5}, FalconVec3{}, 10.0, 0, 0, &hit) == 0,
                "a ray without a direction hit");

        FalconEntity *body = api.playerEntity(fixture.playerHandle());
        double distance = 0.0;
        require(api.levelRaycastEntity(level, FalconVec3{2.5, 103.0, 2.5}, FalconVec3{0.0, -1.0, 0.0}, 10.0, nullptr,
                                       &distance)
                        == body,
                "the ray missed the player");
        require(std::fabs(distance - 1.2) < 1e-3, "the player was hit at " + std::to_string(distance));
        require(api.levelRaycastEntity(level, FalconVec3{2.5, 103.0, 2.5}, FalconVec3{0.0, -1.0, 0.0}, 10.0, body,
                                       &distance)
                        == nullptr,
                "the excluded player was hit");

        FalconEntity *found[4] = {};
        require(api.levelEntitiesInBox(level, FalconVec3{0.0, 99.0, 0.0}, FalconVec3{4.0, 101.0, 4.0}, found, 4) == 1
                        && found[0] == body,
                "the box does not hold the player");
        require(api.levelEntitiesInBox(level, FalconVec3{10.0, 99.0, 10.0}, FalconVec3{12.0, 101.0, 12.0}, found, 4)
                        == 0,
                "an empty box holds entities");

        require(api.levelPlaceStructure(level, "mystructure:missing", FalconBlockPos{0, 100, 0}, 0, 0, 1, 1.0f) == 0,
                "a missing structure was placed");
        require(api.levelPlaceStructure(level, "mystructure:missing", FalconBlockPos{0, 100, 0}, 4, 0, 1, 1.0f) == 0,
                "rotation 4 was accepted");

        require(api.levelAddTickingArea(level, "", FalconBlockPos{0, 0, 0}, FalconBlockPos{15, 0, 15}, 0) == 0,
                "an unnamed ticking area was added");
        require(api.levelAddTickingArea(level, "huge", FalconBlockPos{0, 0, 0}, FalconBlockPos{400, 0, 400}, 0) == 0,
                "a ticking area past the chunk limit was added");
        require(api.levelRemoveTickingArea(level, "missing") == 0, "a missing ticking area was removed");
        require(api.levelTickingAreaCount(level) == 0, "a ticking area appeared");
    }

    void entitiesAndPlayers(const FalconServerApi &api, Fixture &fixture) {
        FalconLevel *level = fixture.levelHandle();
        FalconEntity *body = api.playerEntity(fixture.playerHandle());
        FalconEntity *zombie = api.levelSpawnEntity(level, "minecraft:zombie", FalconVec3{6.5, 100.0, 6.5});
        require(zombie != nullptr, "no zombie spawned");

        require(api.serverEntity(api.entityRuntimeId(zombie)) == zombie, "the zombie was not found by its runtime id");
        require(api.serverEntity(api.entityRuntimeId(body)) == body, "the player was not found by their runtime id");
        require(api.serverEntity(0xFFFFFFFFull) == nullptr, "an unknown runtime id found something");

        api.entitySetNameTag(zombie, "Bob");
        uint32_t savedLength = 0;
        const uint8_t *saved = api.entityNbt(zombie, &savedLength);
        require(saved != nullptr && savedLength > 0, "the zombie has no saved state");
        const std::vector<uint8_t> savedBytes(saved, saved + savedLength);
        api.entitySetNameTag(zombie, "");
        const uint8_t noTag[3] = {0xff, 0x01, 0x02};
        require(api.entitySetNbt(zombie, noTag, 3) == 0, "garbage was loaded into the zombie");
        require(api.entitySetNbt(zombie, savedBytes.data(), (uint32_t) savedBytes.size()) == 1
                        && std::string(api.entityNameTag(zombie)) == "Bob" && api.entityIsAlive(zombie) == 1,
                "the zombie was not loaded back from its saved state");

        FalconItem *helmet = api.itemCreate("minecraft:diamond_helmet", 1);
        require(api.entitySetEquipment(zombie, FALCON_EQUIPMENT_HEAD, helmet) == 1, "the zombie took no helmet");
        FalconItem *worn = api.entityEquipment(zombie, FALCON_EQUIPMENT_HEAD);
        require(worn != nullptr && std::string(api.itemIdentifier(worn)) == "minecraft:diamond_helmet",
                "the zombie does not wear the helmet");
        api.itemDestroy(worn);
        require(api.entitySetEquipment(zombie, 7, helmet) == 0, "slot 7 took an item");
        require(api.entitySetEquipment(body, FALCON_EQUIPMENT_HEAD, helmet) == 0, "a player took mob equipment");
        api.itemDestroy(helmet);

        require(api.entityAddTag(zombie, "boss") == 1 && api.entityHasTag(zombie, "boss") == 1, "the tag was lost");
        require(api.entityAddTag(zombie, "boss") == 0, "a tag was added twice");
        require(api.entityTagCount(zombie) == 1 && std::string(api.entityTag(zombie, 0)) == "boss",
                "the tag is not listed");
        require(api.entityRemoveTag(zombie, "boss") == 1 && api.entityTagCount(zombie) == 0, "the tag stayed");

        require(api.entitySetAttackTarget(zombie, body) == 1, "the zombie refused the player as target");
        require(api.entityAttackTarget(zombie) == body, "the zombie does not target the player");
        require(api.entitySetAttackTarget(zombie, zombie) == 0, "the zombie targeted itself");
        require(api.entitySetAttackTarget(zombie, nullptr) == 1 && api.entityAttackTarget(zombie) == nullptr,
                "the target was not cleared");
        require(api.entitySetAttackTarget(body, zombie) == 0, "a player took an attack target");

        require(api.entityVehicle(body) == nullptr && api.entityPassengerCount(zombie) == 0, "something rides");
        require(api.entityMount(body, body) == 0, "the player mounted itself");

        FalconBlockPos spawn{};
        FalconDimension dimension = 9;
        api.playerClearSpawnPoint(fixture.playerHandle());
        require(api.playerSpawnPoint(fixture.playerHandle(), &spawn, &dimension) == 0, "a cleared spawn point is set");
        require(api.playerSetSpawnPoint(fixture.playerHandle(), level, FalconBlockPos{3, 90, -4}) == 1,
                "the spawn point was refused");
        require(api.playerSpawnPoint(fixture.playerHandle(), &spawn, &dimension) == 1 && spawn.x == 3 && spawn.y == 90
                        && spawn.z == -4 && dimension == FALCON_DIMENSION_OVERWORLD,
                "the spawn point did not stick");
        require(api.playerSetSpawnPoint(fixture.playerHandle(), level, FalconBlockPos{3, 100000, -4}) == 0,
                "a spawn point above the world was taken");

        require(api.playerLanguage(fixture.playerHandle()) != nullptr, "the player has no language");
        require(api.playerSkinId(fixture.playerHandle()) != nullptr, "the player has no skin id");
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t length = 1;
        api.playerSkinImage(fixture.playerHandle(), &width, &height, &length);
        require(length == width * height * 4 || length == 0, "the skin size does not match its pixels");

        const std::vector<uint8_t> pixels(64 * 64 * 4, 0x7f);
        require(api.playerSetSkin(fixture.playerHandle(), "plugin.skin", pixels.data(), (uint32_t) pixels.size(), 64, 32)
                        == 0,
                "a skin that lies about its size was taken");
        require(api.playerSetSkin(fixture.playerHandle(), "plugin.skin", pixels.data(), (uint32_t) pixels.size(), 64, 64)
                        == 1,
                "the skin was not set");
        api.playerSkinImage(fixture.playerHandle(), &width, &height, &length);
        require(std::string(api.playerSkinId(fixture.playerHandle())) == "plugin.skin" && width == 64 && height == 64
                        && length == pixels.size(),
                "the new skin is not the player's");

        const FalconVec3 at{0.0, 110.0, 0.0};
        const FalconVec3 turn{30.0, 90.0, 0.0};
        require(api.playerSetCamera(fixture.playerHandle(), "minecraft:free", &at, &turn, 1.0f,
                                    FALCON_CAMERA_EASE_IN_OUT_SINE) == 1,
                "the free camera was refused");
        require(api.playerSetCamera(fixture.playerHandle(), "minecraft:nothing", nullptr, nullptr, 0.0f, 0) == 0,
                "an unknown preset was accepted");
        require(api.playerFadeCamera(fixture.playerHandle(), 0.5f, 1.0f, 0.5f, 0xff0000) == 1, "the fade was refused");
        require(api.playerFadeCamera(fixture.playerHandle(), -1.0f, 1.0f, 0.5f, 0) == 0, "a negative fade was taken");
        api.playerClearCamera(fixture.playerHandle());

        FalconItem *pearl = api.itemCreate("minecraft:ender_pearl", 1);
        require(api.playerItemCooldown(fixture.playerHandle(), pearl) == 0, "a fresh pearl is cooling down");
        require(api.playerStartItemCooldown(fixture.playerHandle(), pearl, 20) == 1, "the cooldown was refused");
        require(api.playerItemCooldown(fixture.playerHandle(), pearl) == 20,
                "the pearl cools down for " + std::to_string(api.playerItemCooldown(fixture.playerHandle(), pearl)));
        api.itemDestroy(pearl);
        api.entityRemove(zombie);
    }

    void administration(const FalconServerApi &api, Fixture &fixture) {
        require(api.serverBan("Griefer", "Breaking things", "Test") == 1, "the ban was refused");
        require(api.serverIsBanned("Griefer") == 1 && api.serverIsBanned("Friend") == 0, "the ban list is wrong");
        bool listed = false;
        for (uint32_t index = 0; index < api.serverBanCount(); ++index)
            listed = listed || std::string(api.serverBannedName(index)) == "griefer";
        // the list keeps names in lower case, as the game does
        require(listed, "the banned name is not listed");
        require(api.serverPardon("Griefer") == 1 && api.serverIsBanned("Griefer") == 0, "the pardon was lost");
        require(api.serverPardon("Griefer") == 0, "a name was pardoned twice");

        require(api.serverBanIp("203.0.113.9", "", "Test") == 1 && api.serverIsIpBanned("203.0.113.9") == 1,
                "the address was not banned");
        require(api.serverPardonIp("203.0.113.9") == 1 && api.serverIsIpBanned("203.0.113.9") == 0,
                "the address stayed banned");

        require(api.serverAllowListAdd("Friend") == 1, "the allow list took nothing");
        require(api.serverAllowListCount() >= 1, "the allow list is empty");
        require(api.serverAllowListRemove("Friend") == 1 && api.serverAllowListRemove("Friend") == 0,
                "the allow list kept the name");
        api.serverSetAllowListEnabled(1);
        require(api.serverAllowListEnabled() == 1, "the allow list did not turn on");
        api.serverSetAllowListEnabled(0);
        require(api.serverAllowListEnabled() == 0, "the allow list did not turn off");

        api.serverSetMotd("Falcon event");
        require(std::string(api.serverMotd()) == "Falcon event", "the server list entry did not change");
        require(api.serverSetMaxPlayers(40) == 1 && api.serverMaxPlayers() == 40, "the player limit did not change");
        require(api.serverSetMaxPlayers(0) == 0, "a limit of 0 was accepted");
        require(api.serverTps() > 0.0 && api.serverTps() <= 20.0, "the tick rate is out of range");

        require(api.serverPluginVersion("Listed") == nullptr, "a plugin outside the manager has a version");
        require(api.serverPluginEnabled("Missing") == 0, "a missing plugin is enabled");
        require(api.serverPluginCount() == fixture.owner.getPluginManager().getPlugins().size(),
                "the plugin count differs");
    }

    // records what a handler saw; a set destination or amount is written back, a refusal cancels
    struct Probe {
        int calls = 0;
        bool cancel = false;
        bool moveTo = false;
        FalconVec3 to{};
        double amount = -1.0;
        std::string cause;
        FalconEffect effect{};
    };

    void probeHandler(FalconEvent *event, void *userData) {
        const FalconServerApi &api = PluginServerApi::get();
        auto &probe = *static_cast<Probe *>(userData);
        probe.calls++;
        probe.cause = api.eventCause(event);
        api.eventEffect(event, &probe.effect);
        if (probe.moveTo)
            api.eventSetTo(event, probe.to);
        if (probe.amount >= 0.0)
            api.eventSetAmount(event, probe.amount);
        if (probe.cancel)
            api.eventSetCancelled(event, 1);
    }

    void playerAndEntityEvents(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin listener;
        listener.mDescription.mName = "Listener";
        listener.mEnabled = true;
        FalconPlugin *handle = PluginManager::toHandle(listener);
        FalconEntity *body = api.playerEntity(fixture.playerHandle());

        Probe teleport;
        const uint64_t teleportId = api.subscribe(handle, FALCON_EVENT_PLAYER_TELEPORT, FALCON_PRIORITY_NORMAL, 0,
                                                  &probeHandler, &teleport);
        teleport.moveTo = true;
        teleport.to = FalconVec3{3.5, 101.0, 3.5};
        Vector3f destination(1.5f, 100.0f, 1.5f);
        require(PluginEvents::teleport(*fixture.player, destination, "command"), "an allowed teleport was refused");
        require(teleport.calls == 1 && teleport.cause == "command", "the teleport event did not fire");
        require(std::fabs(destination.x - 3.5f) < 1e-4f, "the new destination was ignored");
        teleport.moveTo = false;
        teleport.cancel = true;
        require(!PluginEvents::teleport(*fixture.player, destination, "command"), "a cancelled teleport went through");
        api.unsubscribe(teleportId);

        Probe effect;
        effect.cancel = true;
        const uint64_t effectId = api.subscribe(handle, FALCON_EVENT_ENTITY_EFFECT_ADD, FALCON_PRIORITY_NORMAL, 0,
                                                &probeHandler, &effect);
        require(api.entityAddEffect(body, FALCON_EFFECT_HASTE, 100, 2, 1) == 0, "a refused effect was added");
        require(effect.calls == 1 && effect.effect.type == FALCON_EFFECT_HASTE && effect.effect.amplifier == 2
                        && effect.effect.duration == 100,
                "the effect event carried the wrong effect");
        api.unsubscribe(effectId);

        Probe heal;
        heal.amount = 1.0;
        const uint64_t healId = api.subscribe(handle, FALCON_EVENT_ENTITY_HEAL, FALCON_PRIORITY_NORMAL, 0,
                                              &probeHandler, &heal);
        fixture.player->setHealth(10.0f);
        fixture.player->heal(4.0f, HealCause::Regeneration);
        require(heal.calls == 1 && heal.cause == "regeneration", "the heal event did not fire");
        require(std::fabs(fixture.player->getHealth() - 11.0f) < 1e-4f, "the changed heal amount was ignored");
        api.unsubscribe(healId);

        Probe armor;
        const uint64_t armorId = api.subscribe(handle, FALCON_EVENT_PLAYER_ARMOR_CHANGE, FALCON_PRIORITY_NORMAL, 0,
                                               &probeHandler, &armor);
        PlayerInventory &inventory = fixture.player->getInventory();
        const ItemStack before[PlayerInventory::ARMOR_SIZE] = {inventory.getArmor(0), inventory.getArmor(1),
                                                               inventory.getArmor(2), inventory.getArmor(3)};
        inventory.setArmor(PlayerInventory::ARMOR_HEAD, fixture.owner.createItemStack("minecraft:iron_helmet", 1));
        PluginEvents::armorChanged(*fixture.player, before);
        require(armor.calls == 1, "one changed slot gave " + std::to_string(armor.calls) + " events");
        api.unsubscribe(armorId);

        Probe bucket;
        bucket.cancel = true;
        const uint64_t bucketId = api.subscribe(handle, FALCON_EVENT_PLAYER_BUCKET_FILL, FALCON_PRIORITY_NORMAL, 0,
                                                &probeHandler, &bucket);
        require(!PluginEvents::bucket(*fixture.player, *fixture.level, Vector3i(2, 100, 2), true,
                                      fixture.owner.createItemStack("minecraft:bucket", 1)),
                "a cancelled bucket fill went through");
        require(PluginEvents::bucket(*fixture.player, *fixture.level, Vector3i(2, 100, 2), false,
                                     fixture.owner.createItemStack("minecraft:water_bucket", 1)),
                "emptying fired the fill listener");
        api.unsubscribe(bucketId);
    }

    void buildPortalFrame(Level &level, int32_t z) {
        // the inside stays the fresh chunk's air
        const BlockState obsidian = VanillaBlocks::fromIdentifier("minecraft:obsidian")->toBlockState();
        for (int32_t x = 10; x <= 13; ++x) {
            for (int32_t y = 100; y <= 104; ++y) {
                if (x == 10 || x == 13 || y == 100 || y == 104)
                    level.setBlock(Vector3i(x, y, z), obsidian, false);
            }
        }
    }

    void worldEvents(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin listener;
        listener.mDescription.mName = "WorldListener";
        listener.mEnabled = true;
        FalconPlugin *handle = PluginManager::toHandle(listener);

        Probe portal;
        portal.cancel = true;
        const uint64_t portalId = api.subscribe(handle, FALCON_EVENT_PORTAL_CREATE, FALCON_PRIORITY_NORMAL, 0,
                                                &probeHandler, &portal);
        buildPortalFrame(*fixture.level, 12);
        require(!ObsidianBlock::tryLightPortal(*fixture.level, Vector3i(11, 101, 12), &fixture.owner),
                "a refused portal lit");
        require(portal.calls == 1, "the portal event did not fire");
        require(fixture.level->getBlockState(11, 102, 12).mName != "minecraft:portal", "portal blocks appeared");
        portal.cancel = false;
        require(ObsidianBlock::tryLightPortal(*fixture.level, Vector3i(11, 101, 12), &fixture.owner),
                "an allowed portal did not light");
        require(fixture.level->getBlockState(11, 102, 12).mName == "minecraft:portal", "no portal block");
        api.unsubscribe(portalId);

        Probe spawn;
        spawn.cancel = true;
        const uint64_t spawnId = api.subscribe(handle, FALCON_EVENT_ITEM_SPAWN, FALCON_PRIORITY_NORMAL, 0,
                                               &probeHandler, &spawn);
        const ItemStack stone = fixture.owner.createItemStack("minecraft:stone", 1);
        require(fixture.owner.dropItem(*fixture.level, Vector3f(2.5f, 101.0f, 2.5f), stone, Vector3f(), 10) == nullptr,
                "a refused item appeared");
        require(spawn.calls == 1, "the item spawn event did not fire");
        api.unsubscribe(spawnId);
    }

    // packets such as UpdateAbilities name the player by this id, which outlives the session's runtime id
    void windowClosed(FalconPlayer *, uint32_t window, void *userData) {
        auto &closes = *static_cast<std::vector<uint32_t> *>(userData);
        closes.push_back(window);
    }

    void pluginWindows(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin owned;
        owned.mDescription.mName = "Menus";
        owned.mEnabled = true;
        FalconPlugin *handle = PluginManager::toHandle(owned);
        std::vector<uint32_t> closes;
        const FalconWindowDescriptor chest{0, FalconBlockPos{2, 102, 2}, -1, 27, &windowClosed, &closes};
        const uint32_t window = api.playerOpenWindow(handle, fixture.playerHandle(), &chest);
        require(window != 0, "the window did not open");
        require(api.playerOpenWindow(handle, fixture.playerHandle(), &chest) == 0, "a second window opened over the first");
        const FalconWindowDescriptor empty{0, FalconBlockPos{2, 102, 2}, -1, 0, &windowClosed, &closes};
        require(api.playerOpenWindow(handle, fixture.playerHandle(), &empty) == 0, "a window without slots opened");

        FalconItem *diamonds = api.itemCreate("minecraft:diamond", 5);
        require(api.playerSetWindowItem(fixture.playerHandle(), window, 3, diamonds) == 1, "the slot was not set");
        require(api.playerSetWindowItem(fixture.playerHandle(), window, 27, diamonds) == 0, "a slot past the size was set");
        api.itemDestroy(diamonds);
        FalconItem *read = api.playerWindowItem(fixture.playerHandle(), window, 3);
        require(read != nullptr && std::string(api.itemIdentifier(read)) == "minecraft:diamond" && api.itemCount(read) == 5,
                "the slot did not read back");
        api.itemDestroy(read);
        require(fixture.player->getInventoryManager().getContainer() != nullptr, "the window is not the open container");

        api.playerCloseWindow(fixture.playerHandle(), window + 1);
        require(closes.empty(), "another window id closed the window");
        api.playerCloseWindow(fixture.playerHandle(), window);
        require(closes == std::vector<uint32_t>{window}, "the close handler did not run once with the window");
        require(api.playerWindowItem(fixture.playerHandle(), window, 3) == nullptr, "a closed window still read");
        // the client confirms the close before another window opens
        fixture.player->getInventoryManager().onClientRemoveWindow(window);
        const uint32_t again = api.playerOpenWindow(handle, fixture.playerHandle(), &chest);
        require(again != 0, "a window did not open once the first closed");
        api.playerCloseWindow(fixture.playerHandle(), again);
        fixture.player->getInventoryManager().onClientRemoveWindow(again);
        require(closes.size() == 2, "the second window's handler did not run");

        const uint32_t chestId = api.serverBlockNetworkId("minecraft:chest", "{\"minecraft:cardinal_direction\":\"north\"}");
        require(api.levelSetBlock(fixture.levelHandle(), FalconBlockPos{3, 101, 3}, "minecraft:chest",
                                  "{\"minecraft:cardinal_direction\":\"north\"}") == 1,
                "the chest was not placed");
        require(chestId != 0 && chestId == (uint32_t) fixture.level->getBlockState(3, 101, 3).getHash(),
                "the chest's network id is not the one Falcon sends it by");
        require(api.serverBlockNetworkId("minecraft:not_a_block", "{}") == 0, "an unknown block has a network id");
        require(api.serverBlockNetworkId("chest", "{\"minecraft:cardinal_direction\":\"north\"}") == chestId,
                "the namespace changed the id");
    }

    void uniqueId(const FalconServerApi &api, Fixture &fixture) {
        fixture.player->setUniqueId(-4242);
        require(api.playerUniqueId(fixture.playerHandle()) == -4242, "the player's unique id was not the one they keep");
        require(api.playerUniqueId(fixture.playerHandle()) != (int64_t) api.entityRuntimeId(api.playerEntity(fixture.playerHandle())),
                "the unique id was the runtime id");
    }

    void simulatedPlayers(const FalconServerApi &api, Fixture &fixture) {
        FalconLevel *level = fixture.levelHandle();
        const FalconVec3 at{4.5, 100.0, 4.5};
        require(!api.playerIsSimulated(fixture.playerHandle()), "a client is called simulated");
        require(api.playerRemoveSimulated(fixture.playerHandle()) == 0, "a client was removed as a simulated player");

        FalconPlayer *bot = api.serverSpawnSimulatedPlayer("Bot_1", level, at, 90.0f, FALCON_GAME_MODE_CREATIVE);
        require(bot != nullptr, "the simulated player was refused");
        require(api.playerIsSimulated(bot) == 1, "the simulated player is not called simulated");
        require(std::string(api.playerName(bot)) == "Bot_1", "the simulated player lost its name");
        require(api.playerGameMode(bot) == FALCON_GAME_MODE_CREATIVE, "the simulated player lost its game mode");
        require(api.findPlayer("Bot_1") == bot, "the simulated player is not online");
        // a classic skin with geometry: a client drops the connection over a list entry with an empty one
        uint32_t skinWidth = 0, skinHeight = 0, skinLength = 0;
        api.playerSkinImage(bot, &skinWidth, &skinHeight, &skinLength);
        require(skinWidth == 64 && skinHeight == 64 && skinLength == 64 * 64 * 4, "the simulated player has no skin");
        const SerializedSkin &skin = fixture.owner.getPlayerByName("Bot_1")->getSkin();
        require(!skin.mSkinResourcePatch.empty() && skin.mGeometryData == "{}" && !skin.mSkinId.empty(),
                "the simulated player's skin has no geometry");

        // a name already online, whatever its case, an empty or control name, a place outside the world
        require(api.serverSpawnSimulatedPlayer("bot_1", level, at, 0.0f, FALCON_GAME_MODE_SURVIVAL) == nullptr,
                "a second player took a name already online");
        require(api.serverSpawnSimulatedPlayer("tester", level, at, 0.0f, FALCON_GAME_MODE_SURVIVAL) == nullptr,
                "a simulated player took a client's name");
        require(api.serverSpawnSimulatedPlayer("", level, at, 0.0f, FALCON_GAME_MODE_SURVIVAL) == nullptr,
                "an empty name was taken");
        require(api.serverSpawnSimulatedPlayer("a\nb", level, at, 0.0f, FALCON_GAME_MODE_SURVIVAL) == nullptr,
                "a name with a line break was taken");
        require(api.serverSpawnSimulatedPlayer(std::string(33, 'x').c_str(), level, at, 0.0f,
                                               FALCON_GAME_MODE_SURVIVAL) == nullptr,
                "a name longer than 32 was taken");
        require(api.serverSpawnSimulatedPlayer("Bot_2", level, FalconVec3{0.0, 100000.0, 0.0}, 0.0f,
                                               FALCON_GAME_MODE_SURVIVAL) == nullptr,
                "a place above the world was taken");
        require(api.serverSpawnSimulatedPlayer("Bot_2", level, at, 0.0f, 3u) == nullptr,
                "an unknown game mode was taken");
        require(api.serverSpawnSimulatedPlayer("Bot_2", nullptr, at, 0.0f, FALCON_GAME_MODE_SURVIVAL) == nullptr,
                "a missing level was taken");

        // no cap over time: a tick takes a bounded number of spawns, the next tick takes more
        std::vector<FalconPlayer *> crowd;
        for (int index = 0; index < 250; index++) {
            const std::string name = "Crowd_" + std::to_string(index);
            FalconPlayer *extra = api.serverSpawnSimulatedPlayer(name.c_str(), level, at, 0.0f,
                                                                FALCON_GAME_MODE_SURVIVAL);
            if (extra == nullptr) {
                ServerTickProbe::reserveSpawns(fixture.owner);
                extra = api.serverSpawnSimulatedPlayer(name.c_str(), level, at, 0.0f, FALCON_GAME_MODE_SURVIVAL);
            }
            require(extra != nullptr, "a simulated player past 200 was refused");
            crowd.push_back(extra);
        }
        for (FalconPlayer *extra: crowd)
            api.playerRemoveSimulated(extra);
        require(api.playerRemoveSimulated(bot) == 1, "the simulated player was not removed");
        // it leaves at the end of the tick, as a disconnect
        require(api.findPlayer("Bot_1") == bot, "the simulated player left before the end of the tick");
        ServerTickProbe::removeSimulated(fixture.owner);
        require(api.findPlayer("Bot_1") == nullptr, "the simulated player is still online");
    }

    void pluginResourcePacks(const FalconServerApi &api, Fixture &fixture) {
        constexpr char uuid[] = "a3d60853-adde-4a23-a8a9-5e50aa433a77";
        ZipWriter writer;
        writer.addFile("manifest.json", R"({"header":{"uuid":"a3d60853-adde-4a23-a8a9-5e50aa433a77","version":[1,2,3],"name":"Fixture"}})");
        const std::string archive = writer.finish();
        const FalconResourcePackDescriptor good{archive.data(), (uint32_t) archive.size(), "fixture-key"};
        LoadedPlugin plugin;
        plugin.mDescription.mName = "resource-fixture";
        auto *handle = PluginManager::toHandle(plugin);
        require(api.resourcePacksReplace(handle, &good, 1, 1) == 1, "valid plugin resource pack was rejected");
        auto &packs = fixture.owner.getResourcePacks();
        const auto held = packs.getOffer(false);
        const auto *pack = held->findById(uuid);
        require(held->mRequired && pack != nullptr && pack->mVersion == "1.2.3" && pack->mData == archive
                && pack->mContentKey == "fixture-key", "plugin resource pack was not published to the offer");

        const FalconResourcePackDescriptor invalid{"bad", 3, nullptr};
        require(api.resourcePacksReplace(handle, &invalid, 1, 0) == 0, "invalid pack was accepted");
        require(packs.getOffer(false) == held, "a rejected replacement changed the active offer");
        require(api.resourcePacksReplace(handle, nullptr, 0, 0) == 1, "plugin pack was not removed");
        require(packs.getOffer(false)->findById(uuid) == nullptr && !packs.getOffer(false)->mRequired,
                "removed plugin pack stayed in new offers");
        require(held->findById(uuid) == pack && held->mRequired, "a replacement changed an in-flight offer");
    }

    void playerCompatibility(const FalconServerApi &api, Fixture &fixture) {
        require(api.playerSetNameTag(fixture.playerHandle(), "New tag") == 1, "the name tag was refused");
        require(fixture.player->getName() == "Tester", "the name tag changed the login identity");
        const EntityDataMap metadata = ServerTickProbe::playerMetadata(fixture.owner, *fixture.player);
        const auto tag = std::find_if(metadata.mEntries.begin(), metadata.mEntries.end(), [](const auto &entry) {
            return entry.mId == 4 && entry.mFormat == EntityDataFormat::String;
        });
        require(tag != metadata.mEntries.end() && tag->mStringValue == "New tag", "the tag was not in spawn metadata");

        LoadedPlugin plugin;
        plugin.mEnabled = true;
        std::vector<uint32_t> packets;
        const auto send = api.subscribe(
                PluginManager::toHandle(plugin), FALCON_EVENT_DATA_PACKET_SEND, FALCON_PRIORITY_NORMAL, 0,
                [](FalconEvent *event, void *data) {
                    const uint32_t id = PluginServerApi::get().eventPacketId(event);
                    if (id == (uint32_t) MinecraftPacketIds::ModalFormRequest
                        || id == (uint32_t) MinecraftPacketIds::ClientboundCloseForm)
                        static_cast<std::vector<uint32_t> *>(data)->push_back(id);
                }, &packets);
        std::vector<uint32_t> cancelled;
        const auto response = api.subscribe(
                PluginManager::toHandle(plugin), FALCON_EVENT_PLAYER_FORM_RESPONSE, FALCON_PRIORITY_NORMAL, 0,
                [](FalconEvent *event, void *data) {
                    const auto &api = PluginServerApi::get();
                    if (api.eventState(event))
                        static_cast<std::vector<uint32_t> *>(data)->push_back(api.eventFormId(event));
                }, &cancelled);
        const auto first = api.playerSendForm(fixture.playerHandle(), "{}");
        const int close = api.playerCloseForms(fixture.playerHandle());
        const auto second = api.playerSendForm(fixture.playerHandle(), "{}");
        ServerTickProbe::flushPostedTasks(fixture.owner);
        api.unsubscribe(send);
        api.unsubscribe(response);
        require(first != 0 && second != 0 && close == 1, "a queued form operation was refused");
        require(packets == std::vector<uint32_t>{(uint32_t) MinecraftPacketIds::ClientboundCloseForm,
                                                  (uint32_t) MinecraftPacketIds::ModalFormRequest},
                "a form discarded before close reached the client");
        require(cancelled == std::vector<uint32_t>{first}, "the discarded form did not receive cancellation");
    }

    void permissionGroups(const FalconServerApi &api, Fixture &fixture) {
        auto &permissions = fixture.owner.getPluginManager().getPermissions();
        require(permissions.updateGroups({"create", "member"}), "could not create member group");
        require(permissions.updateGroups({"create", "moderator"}), "could not create moderator group");
        require(permissions.updateGroups({"default", "member"}), "could not set the default group");
        require(permissions.updateGroups({"set", "member", "test.read", "true"}), "could not set a default node");
        require(permissions.updateGroups({"inherit", "moderator", "member"}), "could not inherit the member group");
        require(!permissions.updateGroups({"inherit", "member", "moderator"}), "an inheritance cycle was accepted");
        require(permissions.updateGroups({"set", "moderator", "falcon.command.weather", "true"}),
                "could not set a command node");
        require(permissions.updateGroups({"set", "moderator", "falcon.command.permissiongroup", "true"}),
                "could not record a console command node");
        require(permissions.updateGroups({"assign", "12345", "moderator"}), "could not assign a player");
        require(api.playerHasPermission(fixture.playerHandle(), "test.read"), "default group was ignored");
        require(api.playerPermissionOverride(fixture.playerHandle(), "test.read") == 1, "no explicit group grant");
        require(api.playerPermissionOverride(fixture.playerHandle(), "test.unknown") == 0, "a missing node was explicit");

        // a group grants game commands, never server management such as kick
        const auto *weather = fixture.owner.getCommands().getCommand("weather");
        const auto *admin = fixture.owner.getCommands().getCommand("permissiongroup");
        require(weather != nullptr && admin != nullptr, "the test commands were not registered");
        PlayerCommandOrigin sender(fixture.owner, *fixture.player, CommandOriginData{});
        require(!sender.canExecuteCommand(*weather), "an unassigned player received a group command");
        fixture.player->setXuid("12345");
        fixture.player->setName("Renamed");
        PluginPermissions reloaded(fixture.owner.getEventBus());
        require(reloaded.commandPermission(*fixture.player, *weather) == true, "the XUID assignment was not saved");
        require(api.playerHasPermission(fixture.playerHandle(), "test.read"), "inherited plugin node was ignored");
        LoadedPlugin plugin;
        permissions.setPermission(plugin, *fixture.player, "test.read", false);
        require(!api.playerHasPermission(fixture.playerHandle(), "test.read"), "a plugin denial was ignored");
        require(api.playerPermissionOverride(fixture.playerHandle(), "test.read") == -1, "no explicit plugin denial");
        permissions.clearPlugin(plugin);
        require(api.playerHasPermission(fixture.playerHandle(), "test.read"), "the inherited node was not restored");
        require(permissions.registerPermission("test.legacy", FALCON_PERMISSION_DEFAULT_TRUE),
                "an existing plugin permission default could not be registered");
        require(api.playerHasPermission(fixture.playerHandle(), "test.legacy"), "the plugin default was ignored");
        require(sender.canExecuteCommand(*weather) && !sender.canExecuteCommand(*admin),
                "the group did not grant exactly the permitted command level");
        ExecuteCommandOrigin chained(sender, nullptr, Vector3f(), Vector3f(), fixture.level);
        require(chained.canExecuteCommand(*weather) && !chained.canExecuteCommand(*admin),
                "execute did not preserve the source command permissions");
        require(permissions.updateGroups({"set", "member", "falcon.command.weather", "false"}),
                "could not set an inherited denial");
        fixture.player->setOp(true);
        require(api.playerPermissionOverride(fixture.playerHandle(), "test.unknown") == 0, "operator fallback was explicit");
        require(api.playerPermissionOverride(fixture.playerHandle(), "falcon.command.weather") == -1, "group denial was lost");
        require(!sender.canExecuteCommand(*weather),
                "an inherited denial did not take precedence over operator access");
        fixture.player->setOp(false);
        fixture.player->setXuid("");
        fixture.player->setName("Tester");
    }

    struct DeathProbe {
        uint32_t drops = 0;
        std::string firstDrop;
        int keptByRule = -1;
    };

    void deathHandler(FalconEvent *event, void *userData) {
        const FalconServerApi &api = PluginServerApi::get();
        auto &probe = *static_cast<DeathProbe *>(userData);
        probe.drops = api.eventDropCount(event);
        FalconItem *first = api.eventDropAt(event, 0);
        probe.firstDrop = first == nullptr ? "" : api.itemIdentifier(first);
        probe.keptByRule = api.eventKeepInventory(event);
        api.eventSetKeepInventory(event, 1);
    }

    // a plugin reads what an item or a block type is, and what an event drops, and changes it
    void dropsAndTypes(const FalconServerApi &api, Fixture &fixture) {
        FalconItemInfo sword{};
        require(api.serverItemInfo("minecraft:diamond_sword", &sword) == 1, "the sword has no info");
        require(sword.maxStackSize == 1 && sword.maxDurability == 1561 && sword.toolType == 1 && sword.toolTier == 6
                        && sword.attackDamage == 7 && std::string(sword.name) == "Diamond Sword",
                "the sword's info is wrong");
        FalconItemInfo apple{};
        require(api.serverItemInfo("apple", &apple) == 1 && apple.nutrition == 4, "the apple is not food");
        FalconBlockInfo stone{};
        require(api.serverBlockInfo("minecraft:stone", &stone) == 1 && stone.hardness == 1.5f && stone.toolType == 3
                        && stone.solid == 1,
                "the stone's info is wrong");
        require(api.serverItemInfo("minecraft:not_an_item", &apple) == 0
                        && api.serverBlockInfo("minecraft:not_a_block", &stone) == 0,
                "an unknown type has info");

        require(api.levelSetBlock(fixture.levelHandle(), FalconBlockPos{5, 101, 5}, "minecraft:chest", "{}") == 1,
                "the chest was not placed");
        require(api.levelContainerSize(fixture.levelHandle(), FalconBlockPos{5, 101, 5}) == 27,
                "a chest a plugin placed holds nothing");

        std::vector<ItemStack> drops;
        int32_t experience = 3;
        float horizontal = 0.4f;
        float vertical = 0.4f;
        PluginEvent synthetic;
        synthetic.mDrops = &drops;
        synthetic.mExperience = &experience;
        synthetic.mKnockbackHorizontal = &horizontal;
        synthetic.mKnockbackVertical = &vertical;
        FalconEvent *event = reinterpret_cast<FalconEvent *>(&synthetic);
        FalconItem *diamonds = api.itemCreate("minecraft:diamond", 2);
        FalconItem *set[] = {diamonds, nullptr};
        api.eventSetDrops(event, set, 2);
        api.itemDestroy(diamonds);
        require(api.eventDropCount(event) == 1 && api.itemCount(api.eventDropAt(event, 0)) == 2, "the drops were not set");
        api.eventSetExperience(event, -5);
        require(api.eventExperience(event) == 0, "the experience went below zero");
        float readHorizontal = 0.0f, readVertical = 0.0f;
        api.eventSetKnockback(event, 1.0f, 0.2f);
        require(api.eventKnockback(event, &readHorizontal, &readVertical) == 1 && readHorizontal == 1.0f
                        && readVertical == 0.2f,
                "the knockback was not set");
        PluginEvent none;
        require(api.eventKnockback(reinterpret_cast<FalconEvent *>(&none), &readHorizontal, &readVertical) == 0
                        && api.eventDropCount(reinterpret_cast<FalconEvent *>(&none)) == 0,
                "an event without them carries drops or a push");

        LoadedPlugin listener;
        listener.mDescription.mName = "Keeper";
        listener.mEnabled = true;
        FalconPlugin *handle = PluginManager::toHandle(listener);
        DeathProbe probe;
        const uint64_t id = api.subscribe(handle, FALCON_EVENT_PLAYER_DEATH, FALCON_PRIORITY_NORMAL, 0, &deathHandler,
                                          &probe);
        fixture.player->getInventory().clear();
        FalconItem *emeralds = api.itemCreate("minecraft:emerald", 7);
        api.playerSetInventoryItem(fixture.playerHandle(), 4, emeralds);
        api.itemDestroy(emeralds);
        // an earlier check killed the player and turned keepInventory on: they live again, and the rule drops
        require(api.levelSetGameRule(fixture.levelHandle(), "keepinventory", "false") == 1, "keepinventory stayed on");
        fixture.player->setDead(false);
        fixture.player->setHealth(20.0f);
        fixture.owner.killPlayer(*fixture.player, "death.attack.generic", {"Tester"});
        api.unsubscribe(id);
        require(probe.keptByRule == 0, "the event did not follow the keepInventory rule");
        require(probe.drops == 1 && probe.firstDrop == "minecraft:emerald", "the death did not show what would drop");
        FalconItem *kept = api.playerInventoryItem(fixture.playerHandle(), 4);
        require(kept != nullptr && std::string(api.itemIdentifier(kept)) == "minecraft:emerald" && api.itemCount(kept) == 7,
                "the kept inventory was not given back");
        api.itemDestroy(kept);
    }

    void countTeleports(FalconEvent *event, void *userData) {
        (void) event;
        ++*static_cast<int *>(userData);
    }

    // an entity its plugin draws: Falcon sends nothing for it and leaves it where it is put, hits still find it
    void pluginDrawnEntities(const FalconServerApi &api, Fixture &fixture) {
        LoadedPlugin painter;
        painter.mDescription.mName = "Painter";
        painter.mEnabled = true;
        FalconPlugin *handle = PluginManager::toHandle(painter);
        FalconCustomEntityDescriptor descriptor{};
        descriptor.identifier = "painter:ghost";
        descriptor.width = 0.6f;
        descriptor.height = 1.8f;
        require(api.registerCustomEntity(handle, &descriptor) == 1, "the custom entity was refused");

        FalconEntity *ghost = api.levelSpawnEntity(fixture.levelHandle(), "painter:ghost", FalconVec3{4.5, 110.0, 4.5});
        require(ghost != nullptr, "the custom entity did not spawn");
        auto *actor = dynamic_cast<PluginActor *>(reinterpret_cast<Actor *>(ghost));
        require(actor != nullptr, "the custom entity is not a plugin actor");
        fixture.player->getSentChunks().insert(ChunkStreamHandler::packChunk(0, 0));
        LevelChunk *chunk = fixture.level->peekChunkPtr(0, 0);
        require(chunk != nullptr, "the fixture's chunk is gone");
        chunk->setPopulated(true);
        const bool seenBefore = fixture.owner.canPlayerSeeActor(*fixture.player, *actor);

        require(api.entitySetPluginDrawn(api.playerEntity(fixture.playerHandle()), 1) == 0,
                "a player was handed to a plugin to draw");
        require(api.entitySetPluginDrawn(ghost, 1) == 1 && api.entityIsPluginDrawn(ghost) == 1,
                "the custom entity is not drawn by its plugin");
        require(seenBefore, "the custom entity was not shown before its plugin drew it");
        require(!fixture.owner.canPlayerSeeActor(*fixture.player, *actor), "Falcon still shows an entity its plugin draws");

        for (int tick = 0; tick < 5; ++tick)
            actor->tick(fixture.owner);
        require(actor->getPosition().y == 110.0f, "an entity its plugin draws fell on its own");

        int teleports = 0;
        const uint64_t id = api.subscribe(handle, FALCON_EVENT_ENTITY_TELEPORT, FALCON_PRIORITY_NORMAL, 0,
                                          &countTeleports, &teleports);
        api.entityTeleport(ghost, nullptr, FalconVec3{8.5, 110.0, 8.5});
        api.unsubscribe(id);
        require(teleports == 0 && actor->getPosition().x == 8.5f, "moving a drawn entity was taken for a teleport");

        require(api.entitySetSize(ghost, 3.0f, 2.0f) == 1, "the size was refused");
        require(api.entitySetSize(ghost, 0.0f, 2.0f) == 0 && api.entitySetSize(ghost, std::nanf(""), 1.0f) == 0,
                "a broken size was accepted");
        require(actor->getSize().mWidth == 3.0f && actor->getSize().mHeight == 2.0f, "the size did not apply");
        // only a box three wide reaches across to x 10
        FalconEntity *found[4] = {};
        bool reached = false;
        const uint32_t count = api.levelEntitiesInBox(fixture.levelHandle(), FalconVec3{9.9, 110.0, 8.0},
                                                      FalconVec3{10.5, 111.0, 9.0}, found, 4);
        for (uint32_t index = 0; index < count && index < 4; ++index)
            reached = reached || found[index] == ghost;
        require(reached, "the entity's new box is not found");

        PluginActor reloaded(9001, "painter:ghost");
        reloaded.loadNbt(actor->saveNbt());
        require(reloaded.isDrawnByPlugin() && reloaded.getSize().mWidth == 3.0f,
                "a saved entity forgot it is drawn by its plugin");
        require(api.entitySetPluginDrawn(ghost, 0) == 1 && api.entityIsPluginDrawn(ghost) == 0,
                "Falcon did not take the drawing back");

        require(api.levelSetBlock(fixture.levelHandle(), FalconBlockPos{6, 101, 6}, "minecraft:stone", "{}") == 1,
                "the stone was not placed");
        FalconBox boxes[8] = {};
        const uint32_t boxCount = api.levelCollisionBoxes(fixture.levelHandle(), FalconVec3{5.5, 100.5, 5.5},
                                                          FalconVec3{7.5, 102.5, 7.5}, boxes, 8);
        bool stone = false;
        for (uint32_t index = 0; index < boxCount && index < 8; ++index)
            stone = stone
                    || (boxes[index].min.x == 6.0 && boxes[index].min.y == 101.0 && boxes[index].min.z == 6.0
                        && boxes[index].max.x == 7.0 && boxes[index].max.y == 102.0 && boxes[index].max.z == 7.0);
        require(stone, "the stone's box is not among " + std::to_string(boxCount));
        require(api.levelCollisionBoxes(fixture.levelHandle(), FalconVec3{5.5, 100.5, 5.5}, FalconVec3{7.5, 102.5, 7.5},
                                        nullptr, 0)
                        == boxCount,
                "counting the boxes gave another number");

        require(api.playerHasChunk(fixture.playerHandle(), 0, 0) == 1, "the sent chunk is not held");
        require(api.playerHasChunk(fixture.playerHandle(), 5, 5) == 0, "an unsent chunk is held");

        FalconItem *sword = api.itemCreate("minecraft:diamond_sword", 1);
        FalconItem *stoneItem = api.itemCreate("minecraft:stone", 1);
        int32_t swordBlock = -1;
        int32_t stoneBlock = 0;
        const int32_t swordId = api.itemNetworkId(sword, &swordBlock);
        const int32_t stoneId = api.itemNetworkId(stoneItem, &stoneBlock);
        require(swordId != 0 && swordBlock == 0, "the sword has no network id, or a block");
        require(stoneId != 0 && stoneId != swordId && stoneBlock != 0, "the stone item has no block network id");
        require(api.itemNetworkId(nullptr, nullptr) == 0, "no item has a network id");
        api.itemDestroy(sword);
        api.itemDestroy(stoneItem);
        api.entityRemove(ghost);
    }

    void run() {
        Fixture fixture;
        const FalconServerApi &api = PluginServerApi::get();
        require(api.size == sizeof(FalconServerApi), "the table size is not the header's");
        customItems(api, fixture);
        wasmCustomItems();
        attributes(api, fixture);
        effects(api, fixture);
        soundsAndParticles(api, fixture);
        scoreboard(api, fixture);
        containers(api, fixture);
        persistentData(api, fixture);
        commands(api, fixture);
        joinAndLeaveMessages(api, fixture);
        recipes(api, fixture);
        world(api, fixture);
        entitiesAndPlayers(api, fixture);
        administration(api, fixture);
        playerAndEntityEvents(api, fixture);
        worldEvents(api, fixture);
        simulatedPlayers(api, fixture);
        uniqueId(api, fixture);
        pluginWindows(api, fixture);
        permissionGroups(api, fixture);
        playerCompatibility(api, fixture);
        pluginResourcePacks(api, fixture);
        dropsAndTypes(api, fixture);
        pluginDrawnEntities(api, fixture);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-plugin-api-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
        std::cout << "plugin api checks passed\n";
    } catch (const std::exception &error) {
        std::clog << "Plugin api regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
