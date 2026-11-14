#include "Falcon/Plugin/LoadedPlugin.h"
#include "Falcon/Plugin/PluginCommand.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Falcon/Plugin/PluginServerApi.h"
#include "Network/ServerNetworkHandler.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Server/Commands/CommandMap.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/CraftingRecipeTable.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/ChestBlockActor.h"
#include "World/Level/Block/Actor/SignBlockActor.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

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

    void effects(const FalconServerApi &api, Fixture &fixture) {
        FalconEntity *body = api.playerEntity(fixture.playerHandle());
        require(api.entityAddEffect(body, FALCON_EFFECT_SPEED, 200, 1, 1) == 1, "speed was not added");
        require(api.entityAddEffect(body, FALCON_EFFECT_SPEED, 100, 0, 1) == 0,
                "a weaker speed replaced a stronger one");
        require(api.entityAddEffect(body, 31, 100, 0, 1) == 0, "an unknown effect id was accepted");
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

        require(api.serverExecuteCommand("probe fast 3") == 1 && probe.calls == 1 && !probe.player,
                "the console did not run the command");
        require(probe.arguments == std::vector<std::string>{"fast", "3"}, "the console arguments were lost");
        require(api.playerExecuteCommand(fixture.playerHandle(), "probe slow") == 1 && probe.calls == 2 && probe.player,
                "the player did not run the command");
        require(api.serverExecuteCommand("") == 0, "an empty line ran");
        fixture.owner.getCommands().unregisterCommand("probe");
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

    void run() {
        Fixture fixture;
        const FalconServerApi &api = PluginServerApi::get();
        require(api.size == sizeof(FalconServerApi), "the table size is not the header's");
        attributes(api, fixture);
        effects(api, fixture);
        soundsAndParticles(api, fixture);
        scoreboard(api, fixture);
        containers(api, fixture);
        persistentData(api, fixture);
        commands(api, fixture);
        recipes(api, fixture);
        world(api, fixture);
        entitiesAndPlayers(api, fixture);
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
