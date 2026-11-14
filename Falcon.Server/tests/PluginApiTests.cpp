#include "Falcon/Plugin/PluginServerApi.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
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
        require(api.entityAddEffect(body, FALCON_EFFECT_SPEED, 100, 0, 1) == 0, "a weaker speed replaced a stronger one");
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
        fixture.level->setBlock(signAt, VanillaBlocks::fromIdentifier("minecraft:standing_sign")->toBlockState(), false);
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

    void run() {
        Fixture fixture;
        const FalconServerApi &api = PluginServerApi::get();
        require(api.size == sizeof(FalconServerApi), "the table size is not the header's");
        attributes(api, fixture);
        effects(api, fixture);
        soundsAndParticles(api, fixture);
        scoreboard(api, fixture);
        containers(api, fixture);
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
