#include "Falcon/Plugin/PluginServerApi.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
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

    void run() {
        Fixture fixture;
        const FalconServerApi &api = PluginServerApi::get();
        require(api.size == sizeof(FalconServerApi), "the table size is not the header's");
        attributes(api, fixture);
        effects(api, fixture);
        soundsAndParticles(api, fixture);
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
