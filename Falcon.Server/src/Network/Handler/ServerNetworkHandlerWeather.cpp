#include "Network/Handler/ServerNetworkHandler.h"

#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"

#include <cmath>
#include <random>

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
}

void ServerNetworkHandler::sendWeatherTo(ServerPlayer &player) {
    LevelEventPacket rain;
    rain.mEventId = mLevel.isRaining() ? LevelEventPacket::StartRain : LevelEventPacket::StopRain;
    rain.mPosition = Vector3f(0.0f, 0.0f, 0.0f);
    rain.mData = mLevel.isRaining() ? mLevel.getRainTime() : 0;
    mNetworkHandler->send(player.getNetworkIdentifier(), rain, mCodecContext);

    LevelEventPacket thunder;
    thunder.mEventId = mLevel.isThundering() ? LevelEventPacket::StartThunder : LevelEventPacket::StopThunder;
    thunder.mPosition = Vector3f(0.0f, 0.0f, 0.0f);
    thunder.mData = mLevel.isThundering() ? mLevel.getThunderTime() : 0;
    mNetworkHandler->send(player.getNetworkIdentifier(), thunder, mCodecContext);
}

void ServerNetworkHandler::broadcastWeather() {
    for (auto &entry: mPlayers) {
        if (entry.second.isSpawned())
            sendWeatherTo(entry.second);
    }
}

void ServerNetworkHandler::setRaining(bool raining) {
    mLevel.setRainingState(raining);

    if (raining)
        mLevel.setRainTime(randomBetween(WEATHER_MIN_DURATION, RAIN_SPAN));
    else
        mLevel.setRainTime(randomBetween(WEATHER_MIN_DURATION, WEATHER_CLEAR_SPAN));

    broadcastWeather();
}

void ServerNetworkHandler::setThundering(bool thundering) {
    if (thundering && !mLevel.isRaining())
        setRaining(true);

    mLevel.setThunderingState(thundering);

    if (thundering)
        mLevel.setThunderTime(randomBetween(THUNDER_MIN_DURATION, THUNDER_SPAN));
    else
        mLevel.setThunderTime(randomBetween(WEATHER_MIN_DURATION, WEATHER_CLEAR_SPAN));

    broadcastWeather();
}

void ServerNetworkHandler::strikeLightning(Level &level, const Vector3f &position) {
    ServerActor *bolt = spawnActor(level, "minecraft:lightning_bolt", position);
    if (bolt != nullptr)
        bolt->setProjectile(true);

    playLevelSound(level, LevelSoundEvent::THUNDER, position);
    playLevelSound(level, LevelSoundEvent::EXPLODE, position);

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != level.getDimensionType())
            continue;

        const Vector3f playerPosition = player.getPosition();
        const float dx = playerPosition.x - position.x;
        const float dy = playerPosition.y - position.y;
        const float dz = playerPosition.z - position.z;
        if (dx * dx + dy * dy + dz * dz > 9.0f)
            continue;

        applyDamage(player, (float) LIGHTNING_DAMAGE, "death.attack.lightningBolt", {player.getName()},
                    false, false);
        player.setFireTicks((int) LIGHTNING_FIRE_TICKS);
        player.setOnFire(true);
    }

    for (auto &entry: mActors) {
        ServerActor &actor = *entry.second;
        if (!actor.isAlive() || actor.isProjectile() || actor.getDimension() != level.getDimensionType())
            continue;

        const Vector3f actorPosition = actor.getPosition();
        const float dx = actorPosition.x - position.x;
        const float dy = actorPosition.y - position.y;
        const float dz = actorPosition.z - position.z;
        if (dx * dx + dy * dy + dz * dz > 9.0f)
            continue;

        damageActor(actor, (float) LIGHTNING_DAMAGE, nullptr);
        actor.setFireTicks((int) LIGHTNING_FIRE_TICKS);
        actor.setOnFire(true);
    }
}

void ServerNetworkHandler::tickWeather() {
    if (mLevel.getGameRules().getBool("doweathercycle")) {
        mLevel.setRainTime(mLevel.getRainTime() - 1);
        if (mLevel.getRainTime() <= 0)
            setRaining(!mLevel.isRaining());

        mLevel.setThunderTime(mLevel.getThunderTime() - 1);
        if (mLevel.getThunderTime() <= 0)
            setThundering(!mLevel.isThundering());
    }

    if (!mLevel.isThundering())
        return;

    std::uniform_int_distribution<int32_t> strike(0, LIGHTNING_CHANCE - 1);
    std::uniform_int_distribution<int32_t> offset(0, 15);

    for (auto &entry: mPlayers) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.getDimension() != DimensionType::Overworld)
            continue;

        if (strike(weatherRandom()) != 0)
            continue;

        const Vector3f playerPosition = player.getPosition();
        const int32_t chunkX = ((int32_t) std::floor(playerPosition.x) >> 4) << 4;
        const int32_t chunkZ = ((int32_t) std::floor(playerPosition.z) >> 4) << 4;
        const int32_t targetX = chunkX + offset(weatherRandom());
        const int32_t targetZ = chunkZ + offset(weatherRandom());

        if (!mLevel.canRainAt(targetX, targetZ))
            continue;

        int32_t surfaceY = (int32_t) std::floor(playerPosition.y) + 16;
        while (surfaceY > 0 && !mLevel.isSolidAt(targetX, surfaceY, targetZ))
            --surfaceY;

        strikeLightning(mLevel, Vector3f((float) targetX + 0.5f, (float) (surfaceY + 1), (float) targetZ + 0.5f));
    }
}
