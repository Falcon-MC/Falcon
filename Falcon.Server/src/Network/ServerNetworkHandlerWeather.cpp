#include "Network/ServerNetworkHandler.h"

#include "World/Actor/ServerActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"
#include "Falcon/Plugin/PluginEvent.h"
#include "Falcon/Plugin/PluginManager.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Scripting/ScriptEngine.h"

#include <cmath>
#include <random>
#include <string>

namespace {
    const int32_t WEATHER_MIN_DURATION = 12000;
    const int32_t WEATHER_CLEAR_SPAN = 168000;
    const int32_t RAIN_SPAN = 12000;
    const int32_t THUNDER_MIN_DURATION = 3600;
    const int32_t THUNDER_SPAN = 12000;
    const int32_t LIGHTNING_CHANCE = 100000;
    const int32_t LIGHTNING_DAMAGE = 5;
    const float LIGHTNING_FIRE_TICKS = 8 * 20;

    std::mt19937 &weatherRandom() {
        static std::mt19937 generator{std::random_device{}()};
        return generator;
    }

    int32_t randomBetween(int32_t minimum, int32_t span) {
        std::uniform_int_distribution<int32_t> distribution(0, span - 1);
        return distribution(weatherRandom()) + minimum;
    }

    bool allowsWeatherChange(Level &level, FalconEventType type, bool state) {
        PluginManager *plugins = PluginManager::findWithSubscribers(type);
        if (plugins == nullptr)
            return true;

        PluginEvent event;
        event.mType = type;
        event.mCancellable = true;
        event.mLevel = &level;
        event.mState = state;
        plugins->dispatch(event);
        return !event.mCancelled;
    }

    const char *weatherName(bool raining, bool thundering) {
        if (!raining)
            return "Clear";
        return thundering ? "Thunder" : "Rain";
    }

    // false when a script cancelled; otherwise raining, thundering and duration hold what scripts chose
    bool scriptsAllowWeather(ScriptEngine &scripts, const Level &level, bool &raining, bool &thundering,
                             int32_t &duration) {
        const std::string previous = weatherName(level.isRaining(), level.isThundering());
        std::string next = weatherName(raining, thundering);
        if (next == previous)
            return true;

        if (scripts.beforeWeatherChange(previous, next, duration))
            return false;
        raining = next != "Clear";
        thundering = next == "Thunder";
        return true;
    }

    void notifyWeather(ScriptEngine &scripts, const Level &level, const std::string &previous) {
        const std::string current = weatherName(level.isRaining(), level.isThundering());
        if (current != previous)
            scripts.onWeatherChange("minecraft:overworld", previous, current);
    }
}

void ServerNetworkHandler::sendWeatherTo(ServerPlayer &player) {
    Level &level = getWorldFor(player).getOverworld();

    LevelEventPacket rain;
    rain.mEventId = level.isRaining() ? LevelEventPacket::StartRain : LevelEventPacket::StopRain;
    rain.mPosition = Vector3f(0.0f, 0.0f, 0.0f);
    rain.mData = level.isRaining() ? level.getRainTime() : 0;
    mNetworkHandler->send(player.getNetworkIdentifier(), rain, mCodecContext);

    LevelEventPacket thunder;
    thunder.mEventId = level.isThundering() ? LevelEventPacket::StartThunder : LevelEventPacket::StopThunder;
    thunder.mPosition = Vector3f(0.0f, 0.0f, 0.0f);
    thunder.mData = level.isThundering() ? level.getThunderTime() : 0;
    mNetworkHandler->send(player.getNetworkIdentifier(), thunder, mCodecContext);
}

void ServerNetworkHandler::broadcastWeather(World &world) {
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned() && entry.second.getWorldId() == world.getId())
            sendWeatherTo(entry.second);
    }
}

void ServerNetworkHandler::setRaining(World &world, bool raining) {
    Level &level = world.getOverworld();
    if (raining != level.isRaining() && !allowsWeatherChange(level, FALCON_EVENT_WEATHER_CHANGE, raining))
        raining = level.isRaining();

    const std::string previous = weatherName(level.isRaining(), level.isThundering());
    bool thundering = level.isThundering();
    int32_t duration = raining ? randomBetween(WEATHER_MIN_DURATION, RAIN_SPAN)
                               : randomBetween(WEATHER_MIN_DURATION, WEATHER_CLEAR_SPAN);
    if (!scriptsAllowWeather(mScriptEngine, level, raining, thundering, duration)) {
        raining = level.isRaining();
        thundering = level.isThundering();
        duration = raining ? randomBetween(WEATHER_MIN_DURATION, RAIN_SPAN)
                           : randomBetween(WEATHER_MIN_DURATION, WEATHER_CLEAR_SPAN);
    }

    level.setRainingState(raining);
    level.setRainTime(duration);
    if (thundering != level.isThundering()) {
        level.setThunderingState(thundering);
        level.setThunderTime(duration);
    }

    notifyWeather(mScriptEngine, level, previous);
    broadcastWeather(world);
}

void ServerNetworkHandler::setThundering(World &world, bool thundering) {
    Level &level = world.getOverworld();
    if (thundering != level.isThundering() && !allowsWeatherChange(level, FALCON_EVENT_THUNDER_CHANGE, thundering))
        thundering = level.isThundering();

    if (thundering && !level.isRaining())
        setRaining(world, true);
    if (thundering && !level.isRaining())
        thundering = false;

    const std::string previous = weatherName(level.isRaining(), level.isThundering());
    bool raining = level.isRaining();
    int32_t duration = thundering ? randomBetween(THUNDER_MIN_DURATION, THUNDER_SPAN)
                                  : randomBetween(WEATHER_MIN_DURATION, WEATHER_CLEAR_SPAN);
    if (!scriptsAllowWeather(mScriptEngine, level, raining, thundering, duration)) {
        raining = level.isRaining();
        thundering = level.isThundering();
        duration = thundering ? randomBetween(THUNDER_MIN_DURATION, THUNDER_SPAN)
                              : randomBetween(WEATHER_MIN_DURATION, WEATHER_CLEAR_SPAN);
    }

    if (raining != level.isRaining()) {
        level.setRainingState(raining);
        level.setRainTime(duration);
    }
    level.setThunderingState(thundering);
    level.setThunderTime(duration);

    notifyWeather(mScriptEngine, level, previous);
    broadcastWeather(world);
}

void ServerNetworkHandler::strikeLightning(Level &level, const Vector3f &position) {
    ServerActor *bolt = spawnActor(level, "minecraft:lightning_bolt", position);
    if (bolt != nullptr)
        bolt->setProjectile(true);

    playLevelSound(level, LevelSoundEvent::THUNDER, position);
    playLevelSound(level, LevelSoundEvent::EXPLODE, position);

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || !player.isIn(level))
            continue;

        const Vector3f playerPosition = player.getPosition();
        const float dx = playerPosition.x - position.x;
        const float dy = playerPosition.y - position.y;
        const float dz = playerPosition.z - position.z;
        if (dx * dx + dy * dy + dz * dz > 9.0f)
            continue;

        hurt(player, (float) LIGHTNING_DAMAGE,
             ActorDamageSource::environment("death.attack.lightningBolt", player.getName())
                     .withoutArmor()
                     .withoutCooldown());
        player.setFireTicks((int) LIGHTNING_FIRE_TICKS);
        player.setOnFire(true);
    }

    for (auto &entry: mActors) {
        ServerActor &actor = *entry.second;
        if (!actor.isAlive() || actor.isProjectile() || !actor.isIn(level))
            continue;

        const Vector3f actorPosition = actor.getPosition();
        const float dx = actorPosition.x - position.x;
        const float dy = actorPosition.y - position.y;
        const float dz = actorPosition.z - position.z;
        if (dx * dx + dy * dy + dz * dz > 9.0f)
            continue;

        actor.onStruckByLightning(*this);
        damageActor(actor, (float) LIGHTNING_DAMAGE,
                    ActorDamageSource::environment("death.attack.lightningBolt", actor.getName())
                            .withoutArmor()
                            .withDamager(bolt));
        actor.setFireTicks((int) LIGHTNING_FIRE_TICKS);
        actor.setOnFire(true);
    }
}

void ServerNetworkHandler::tickWeather() {
    for (World *world: mWorlds.getWorlds())
        _tickWeather(*world);
}

void ServerNetworkHandler::_tickWeather(World &world) {
    Level &level = world.getOverworld();
    if (level.getGameRules().getBool("doweathercycle")) {
        level.setRainTime(level.getRainTime() - 1);
        if (level.getRainTime() <= 0)
            setRaining(world, !level.isRaining());

        level.setThunderTime(level.getThunderTime() - 1);
        if (level.getThunderTime() <= 0)
            setThundering(world, !level.isThundering());
    }

    if (!level.isThundering())
        return;

    std::uniform_int_distribution<int32_t> strike(0, LIGHTNING_CHANCE - 1);
    std::uniform_int_distribution<int32_t> offset(0, 15);

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || !player.isIn(level))
            continue;

        if (strike(weatherRandom()) != 0)
            continue;

        const Vector3f playerPosition = player.getPosition();
        const int32_t chunkX = ((int32_t) std::floor(playerPosition.x) >> 4) << 4;
        const int32_t chunkZ = ((int32_t) std::floor(playerPosition.z) >> 4) << 4;
        const int32_t targetX = chunkX + offset(weatherRandom());
        const int32_t targetZ = chunkZ + offset(weatherRandom());

        if (!level.canRainAt(targetX, targetZ))
            continue;

        int32_t surfaceY = (int32_t) std::floor(playerPosition.y) + 16;
        while (surfaceY > 0 && !level.isSolidAt(targetX, surfaceY, targetZ))
            --surfaceY;

        strikeLightning(level, Vector3f((float) targetX + 0.5f, (float) (surfaceY + 1), (float) targetZ + 0.5f));
    }
}
