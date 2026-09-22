#include "Actor/Mob/Hostile/AbstractSlimeActor.h"

#include "Actor/ServerPlayer.h"
#include "Level/Level.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/StartGameTypes.h"

#include <random>

namespace {
    constexpr int32_t ACTOR_DATA_VARIANT = 2;
    constexpr int32_t ACTOR_DATA_WIDTH = 53;
    constexpr int32_t ACTOR_DATA_HEIGHT = 54;
    constexpr const char *TAG_VARIANT = "Variant";
    constexpr int32_t ATTACK_COOLDOWN_TICKS = 10;
    constexpr float ATTACK_RANGE = 0.15f;
    constexpr float PLAYER_HALF_WIDTH = 0.3f;
    constexpr float PLAYER_HEIGHT = 1.8f;
    constexpr int MIN_SPLIT_COUNT = 2;
    constexpr int MAX_SPLIT_COUNT = 4;

    std::mt19937 &slimeRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    bool isValidSize(int variant) {
        return variant == AbstractSlimeActor::SMALL_SIZE || variant == AbstractSlimeActor::MEDIUM_SIZE
               || variant == AbstractSlimeActor::LARGE_SIZE;
    }

    void pushEntry(EntityDataMap &metadata, int32_t id, int32_t value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Int;
        entry.mIntValue = value;
        metadata.mEntries.push_back(entry);
    }

    void pushEntry(EntityDataMap &metadata, int32_t id, float value) {
        EntityDataEntry entry;
        entry.mId = id;
        entry.mFormat = EntityDataFormat::Float;
        entry.mFloatValue = value;
        metadata.mEntries.push_back(entry);
    }
}

void AbstractSlimeActor::setSizeVariant(int variant) {
    mSizeVariant = isValidSize(variant) ? variant : LARGE_SIZE;
}

ActorSize AbstractSlimeActor::getSize() const {
    const float extent = SIZE_SCALE * (float) mSizeVariant;
    return ActorSize{extent, extent};
}

float AbstractSlimeActor::getDefaultMaxHealth() const {
    if (mSizeVariant == LARGE_SIZE)
        return 16.0f;
    if (mSizeVariant == MEDIUM_SIZE)
        return 4.0f;
    return 1.0f;
}

void AbstractSlimeActor::finalizeSpawn() {
    static const int sizes[] = {SMALL_SIZE, MEDIUM_SIZE, LARGE_SIZE};
    mSizeVariant = sizes[std::uniform_int_distribution<int>(0, 2)(slimeRandom())];
}

void AbstractSlimeActor::kill(ServerNetworkHandler &owner, ServerPlayer *source, int32_t lootingLevel) {
    if (!isDead() && mSizeVariant > SMALL_SIZE)
        _split(owner, owner.getLevelFor(*this));

    HostileActor::kill(owner, source, lootingLevel);
}

void AbstractSlimeActor::_split(ServerNetworkHandler &owner, Level &level) {
    const int childSize = mSizeVariant / 2;
    const int count = std::uniform_int_distribution<int>(MIN_SPLIT_COUNT, MAX_SPLIT_COUNT)(slimeRandom());
    const float offset = (float) mSizeVariant / 4.0f;
    const Vector3f position = getPosition();

    for (int index = 0; index < count; ++index) {
        const float dx = ((float) (index % 2) - 0.5f) * offset;
        const float dz = ((float) (index / 2) - 0.5f) * offset;
        owner.spawnActor(level, getTypeId(),Vector3f(position.x + dx, position.y + 0.5f, position.z + dz),
                         [childSize](ServerActor &actor) {
                             static_cast<AbstractSlimeActor &>(actor).setSizeVariant(childSize);
                         });
    }
}

void AbstractSlimeActor::tick(ServerNetworkHandler &owner) {
    HostileActor::tick(owner);

    if (mAttackCooldown > 0)
        --mAttackCooldown;

    if (isAlive() && mAttackCooldown <= 0 && getContactDamage() > 0.0f)
        _attackTouchingPlayers(owner);
}

void AbstractSlimeActor::_attackTouchingPlayers(ServerNetworkHandler &owner) {
    const Vector3f position = getPosition();
    const float halfWidth = getSize().mWidth / 2.0f + ATTACK_RANGE;
    const float height = getSize().mHeight + ATTACK_RANGE;

    for (auto &entry: owner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || player.getDimension() != getDimension())
            continue;

        const int32_t gameType = player.getGameType();
        if (gameType == (int32_t) GameType::Creative || gameType == (int32_t) GameType::Spectator)
            continue;

        const Vector3f target = player.getPosition();
        if (target.x + PLAYER_HALF_WIDTH < position.x - halfWidth || target.x - PLAYER_HALF_WIDTH > position.x + halfWidth
            || target.z + PLAYER_HALF_WIDTH < position.z - halfWidth || target.z - PLAYER_HALF_WIDTH > position.z + halfWidth
            || target.y + PLAYER_HEIGHT < position.y - ATTACK_RANGE || target.y > position.y + height)
            continue;

        owner.applyDamage(player, getContactDamage(), "death.attack.mob",
                          {player.getName(), getName()});
        mAttackCooldown = ATTACK_COOLDOWN_TICKS;
    }
}

void AbstractSlimeActor::fillSpawnMetadata(EntityDataMap &metadata) const {
    HostileActor::fillSpawnMetadata(metadata);

    const ActorSize size = getSize();
    pushEntry(metadata, ACTOR_DATA_VARIANT, (int32_t) mSizeVariant);
    pushEntry(metadata, ACTOR_DATA_WIDTH, size.mWidth);
    pushEntry(metadata, ACTOR_DATA_HEIGHT, size.mHeight);
}

Tag AbstractSlimeActor::saveNbt() const {
    Tag data = HostileActor::saveNbt();
    data.putInt(TAG_VARIANT, mSizeVariant);
    return data;
}

void AbstractSlimeActor::loadNbt(const Tag &data) {
    HostileActor::loadNbt(data);
    setSizeVariant(data.getInt(TAG_VARIANT, LARGE_SIZE));
}
