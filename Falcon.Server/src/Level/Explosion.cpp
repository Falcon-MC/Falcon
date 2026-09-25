#include "Level/Explosion.h"

#include "Actor/ActorClassRegistry.h"
#include "Actor/ItemActor.h"
#include "Actor/ServerActor.h"
#include "Actor/ServerPlayer.h"
#include "Block/BlockData.h"
#include "Block/BlockShape.h"
#include "Block/Blocks/LiquidView.h"
#include "Block/Blocks/TntBlock.h"
#include "Level/Level.h"
#include "Level/LevelChunk.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Level/Particle/BlockExplodeParticle.h"
#include "Level/Particle/ExplodeParticle.h"
#include "Protocol/Packets/LevelSoundEventPacket.h"
#include "Protocol/Types/ItemStack.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
    const int RAYS = 16;
    const double STEP_LEN = 0.3;
    const double DEFAULT_FIRE_CHANCE = 1.0 / 3.0;
    const char *DEATH_KEY = "death.attack.explosion";
    const char *NETHER_STAR = "minecraft:nether_star";

    std::mt19937 &explosionRandom() {
        static std::mt19937 random(std::random_device{}());
        return random;
    }

    int32_t nextInt(int32_t min, int32_t max) {
        return std::uniform_int_distribution<int32_t>(min, max)(explosionRandom());
    }

    double nextDouble() {
        return std::uniform_real_distribution<double>(0.0, 1.0)(explosionRandom());
    }

    float resistanceOf(const BlockState &state) {
        const BlockData *data = BlockDataTable::find(state.mName.c_str());
        return data == nullptr ? 0.0f : data->mResistance;
    }

    bool isAir(const BlockState &state) {
        return state.mName == "minecraft:air";
    }

    AxisAlignedBB boundingBoxOf(const Vector3f &position, const ActorSize &size) {
        const float halfWidth = size.mWidth * 0.5f;
        return AxisAlignedBB(position.x - halfWidth, position.y, position.z - halfWidth,
                             position.x + halfWidth, position.y + size.mHeight, position.z + halfWidth);
    }

    AxisAlignedBB boundingBoxOf(const Vector3f &position, const std::string &identifier) {
        return boundingBoxOf(position, ActorClassRegistry::getSize(identifier));
    }

    Vector3f normalized(const Vector3f &vector) {
        const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
        if (length <= 0.0f)
            return Vector3f();

        return Vector3f(vector.x / length, vector.y / length, vector.z / length);
    }

    double distanceBetween(const Vector3f &left, const Vector3f &right) {
        const double x = (double) left.x - (double) right.x;
        const double y = (double) left.y - (double) right.y;
        const double z = (double) left.z - (double) right.z;
        return std::sqrt(x * x + y * y + z * z);
    }
}

Explosion::Explosion(ServerNetworkHandler &owner, Level &level, const Vector3f &center, double size,
                     const ServerActor *sourceActor, bool fromTnt)
        : mOwner(owner), mLevel(level), mSource(center), mSize(std::max(size, 0.0)), mSourceActor(sourceActor),
          mFromTnt(fromTnt) {
}

void Explosion::setIncendiary(bool incendiary) {
    if (!incendiary)
        mFireChance = 0.0;
    else if (mFireChance <= 0.0)
        mFireChance = DEFAULT_FIRE_CHANCE;
}

int64_t Explosion::_key(const Vector3i &position) {
    return (((int64_t) position.x & 0x3FFFFFF) << 38) | (((int64_t) position.z & 0x3FFFFFF) << 12)
           | ((int64_t) (position.y + 2048) & 0xFFF);
}

bool Explosion::explode() {
    if (explodeA())
        return explodeB();

    return false;
}

bool Explosion::explodeA() {
    if (mSourceActor != nullptr) {
        const Vector3i floor((int32_t) std::floor(mSource.x), (int32_t) std::floor(mSource.y),
                             (int32_t) std::floor(mSource.z));
        const BlockState layer0 = mLevel.getBlockState(floor.x, floor.y, floor.z);
        const BlockState layer1 = mLevel.getBlockStateAtLayer(floor.x, floor.y, floor.z, 1);
        if (LiquidView(layer0).isWater() || LiquidView(layer1).isWater())
            return true;
    }

    if (mSize < 0.1)
        return false;

    const bool incendiary = mFireChance > 0.0;
    const int lastRay = RAYS - 1;

    for (int i = 0; i < RAYS; ++i) {
        for (int j = 0; j < RAYS; ++j) {
            for (int k = 0; k < RAYS; ++k) {
                if (i != 0 && i != lastRay && j != 0 && j != lastRay && k != 0 && k != lastRay)
                    continue;

                double vectorX = (double) i / (double) lastRay * 2.0 - 1.0;
                double vectorY = (double) j / (double) lastRay * 2.0 - 1.0;
                double vectorZ = (double) k / (double) lastRay * 2.0 - 1.0;
                const double length = std::sqrt(vectorX * vectorX + vectorY * vectorY + vectorZ * vectorZ);
                vectorX = vectorX / length * STEP_LEN;
                vectorY = vectorY / length * STEP_LEN;
                vectorZ = vectorZ / length * STEP_LEN;

                double pointerX = mSource.x;
                double pointerY = mSource.y;
                double pointerZ = mSource.z;

                for (double blastForce = mSize * nextInt(700, 1300) / 1000.0; blastForce > 0.0;
                     blastForce -= STEP_LEN * 0.75) {
                    const Vector3i block((int32_t) std::floor(pointerX), (int32_t) std::floor(pointerY),
                                         (int32_t) std::floor(pointerZ));
                    if (block.y < LevelChunk::MIN_Y || block.y > LevelChunk::MAX_Y)
                        break;

                    const BlockState layer0 = mLevel.getBlockState(block.x, block.y, block.z);
                    const float layer0Resistance = resistanceOf(layer0);

                    if (!isAir(layer0) && layer0Resistance != -1.0f) {
                        const BlockState layer1 = mLevel.getBlockStateAtLayer(block.x, block.y, block.z, 1);
                        const double resistance = std::max(layer0Resistance, resistanceOf(layer1));
                        blastForce -= (resistance / 5.0 + 0.3) * STEP_LEN;

                        if (blastForce > 0.0 && mAffectedKeys.insert(_key(block)).second) {
                            mAffectedBlocks.push_back(block);
                            if (incendiary && nextDouble() <= mFireChance)
                                mFireIgnitions.push_back(block);
                        }
                    }

                    pointerX += vectorX;
                    pointerY += vectorY;
                    pointerZ += vectorZ;
                }
            }
        }
    }

    return true;
}

bool Explosion::explodeB() {
    _damageEntities();
    _destroyBlocks();
    _ignite();
    _playEffects();
    return true;
}

float Explosion::_calculateEntityDamage(double doubleRadius, double impact) {
    return (float) ((impact * impact + impact) / 2.0 * 7.0 * doubleRadius + 1.0);
}

float Explosion::_scaleDamageForDifficulty(float damage) const {
    switch (mOwner.getProperties().getDifficulty()) {
        case Difficulty::Peaceful:
            return 0.0f;
        case Difficulty::Easy:
            return std::min(damage / 2.0f + 1.0f, damage);
        case Difficulty::Hard:
            return damage * 1.5f;
        default:
            return damage;
    }
}

void Explosion::_damageEntities() {
    const double explosionSize = mSize * 2.0;

    for (auto &entry: mOwner.getPlayers()) {
        ServerPlayer &player = entry.second;
        if (!player.isSpawned() || player.isDead() || &mOwner.getLevelFor(player) != &mLevel)
            continue;

        const double distance = distanceBetween(player.getPosition(), mSource) / explosionSize;
        if (distance > 1.0)
            continue;

        const Vector3f position = player.getPosition();
        const Vector3f motion = normalized(Vector3f(position.x - mSource.x, position.y - mSource.y,
                                                    position.z - mSource.z));
        const float density = getBlockDensity(mLevel, mSource, boundingBoxOf(position, "minecraft:player"));
        const double impact = (1.0 - distance) * density;
        const float damage = _scaleDamageForDifficulty(_calculateEntityDamage(explosionSize, impact));

        mOwner.hurt(player, damage, DamageSource::environment(DEATH_KEY, player.getName()).fromOrigin(mSource));

        const Vector3f current = player.getMotion();
        player.setMotion(Vector3f(current.x + motion.x * (float) impact, current.y + motion.y * (float) impact,
                                  current.z + motion.z * (float) impact));
        mOwner.sendActorMotion(player);
    }

    const DimensionType dimension = mLevel.getDimensionType();

    std::vector<ServerActor *> actors;
    for (auto &entry: mOwner.getActors()) {
        ServerActor *actor = entry.second.get();
        if (actor != nullptr && actor != mSourceActor && actor->isAlive() && actor->getDimension() == dimension)
            actors.push_back(actor);
    }

    for (ServerActor *actor: actors) {
        const double distance = distanceBetween(actor->getPosition(), mSource) / explosionSize;
        if (distance > 1.0)
            continue;

        const Vector3f position = actor->getPosition();
        const Vector3f motion = normalized(Vector3f(position.x - mSource.x, position.y - mSource.y,
                                                    position.z - mSource.z));
        const float density = getBlockDensity(mLevel, mSource, boundingBoxOf(position, actor->getSize()));
        const double impact = (1.0 - distance) * density;

        mOwner.damageActor(*actor, _calculateEntityDamage(explosionSize, impact), nullptr);

        const Vector3f current = actor->getMotion();
        actor->setMotion(Vector3f(current.x + motion.x * (float) impact, current.y + motion.y * (float) impact,
                                  current.z + motion.z * (float) impact));
    }

    for (const std::unique_ptr<ItemActor> &item: mOwner.getItemEntities()) {
        if (item->isRemoved() || item->getDimension() != dimension || _isInsideWater(item->getPosition()))
            continue;

        const ItemStack &stack = item->getItem();
        if (stack.mDefinition != nullptr && stack.mDefinition->getIdentifier() == NETHER_STAR)
            continue;

        const Vector3f position = item->getPosition();
        const double distance = distanceBetween(position, mSource) / explosionSize;
        if (distance > 1.0)
            continue;

        const float density = getBlockDensity(mLevel, mSource, boundingBoxOf(position, "minecraft:item"));
        const double impact = (1.0 - distance) * density;
        if (item->reduceHealth(_calculateEntityDamage(explosionSize, impact)) <= 0.0f)
            item->setRemoved(true);
    }
}

bool Explosion::_isInsideWater(const Vector3f &position) const {
    const int32_t x = (int32_t) std::floor(position.x);
    const int32_t y = (int32_t) std::floor(position.y);
    const int32_t z = (int32_t) std::floor(position.z);
    if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
        return false;

    return LiquidView(mLevel.getBlockState(x, y, z)).isWater()
           || LiquidView(mLevel.getBlockStateAtLayer(x, y, z, 1)).isWater();
}

void Explosion::_destroyBlocks() {
    const double yield = mFromTnt && !mLevel.getGameRules().getBool("tntexplosiondropdecay")
                         ? 100.0 : (1.0 / mSize) * 100.0;
    const ItemStack noTool;

    for (const Vector3i &position: mAffectedBlocks) {
        const BlockState state = mLevel.getBlockState(position.x, position.y, position.z);
        if (isAir(state))
            continue;

        if (nextInt(0, 7) == 0)
            mSmokePositions.push_back(position);

        if (TntBlock::matches(state.mName)) {
            TntBlock::prime(mOwner, mLevel, position, nextInt(10, 30));
            continue;
        }

        if (!isAir(mLevel.getBlockStateAtLayer(position.x, position.y, position.z, 1)))
            mLevel.setBlockStateAtLayer(position.x, position.y, position.z, 1, BlockState());

        BlockActionHandler::destroyBlock(mOwner, mLevel, position, state, nextDouble() * 100.0 < yield, noTool);
    }
}

void Explosion::_ignite() {
    for (const Vector3i &position: mFireIgnitions) {
        const BlockState state = mLevel.getBlockState(position.x, position.y, position.z);
        const BlockData *below = BlockDataTable::find(
                mLevel.getBlockState(position.x, position.y - 1, position.z).mName.c_str());
        if (!isAir(state) || below == nullptr || !below->mSolid)
            continue;

        const BlockState fire("minecraft:fire");
        mLevel.setBlockState(position.x, position.y, position.z, fire);
        BlockActionHandler::broadcastBlockUpdate(mOwner, mLevel, position, fire);
    }
}

void Explosion::_playEffects() {
    mOwner.playLevelSound(mLevel, LevelSoundEvent::EXPLODE, mSource);

    mLevel.addParticle(ExplodeParticle(mSource, mSize));
    mLevel.addParticle(BlockExplodeParticle(mSource, mSize, mSmokePositions));
}

float Explosion::getBlockDensity(Level &level, const Vector3f &source, const AxisAlignedBB &boundingBox) {
    if (boundingBox.isVectorInside(source))
        return 1.0f;

    const double diffX = boundingBox.mMaxX - boundingBox.mMinX;
    const double diffY = boundingBox.mMaxY - boundingBox.mMinY;
    const double diffZ = boundingBox.mMaxZ - boundingBox.mMinZ;
    const double xInterval = 1.0 / (diffX * 2.0 + 1.0);
    const double yInterval = 1.0 / (diffY * 2.0 + 1.0);
    const double zInterval = 1.0 / (diffZ * 2.0 + 1.0);

    if (xInterval < 0.0 || yInterval < 0.0 || zInterval < 0.0)
        return 0.0f;

    const double xOffset = boundingBox.mMinX + (1.0 - std::floor(1.0 / xInterval) * xInterval) / 2.0;
    const double yOffset = boundingBox.mMinY;
    const double zOffset = boundingBox.mMinZ + (1.0 - std::floor(1.0 / zInterval) * zInterval) / 2.0;

    int visibleBlocks = 0;
    int totalBlocks = 0;

    for (float x = 0.0f; x <= 1.0f; x = (float) ((double) x + xInterval)) {
        const double fromX = std::fma((double) x, diffX, xOffset);
        for (float y = 0.0f; y <= 1.0f; y = (float) ((double) y + yInterval)) {
            const double fromY = std::fma((double) y, diffY, yOffset);
            for (float z = 0.0f; z <= 1.0f; z = (float) ((double) z + zInterval)) {
                totalBlocks++;
                const double fromZ = std::fma((double) z, diffZ, zOffset);

                if (!isRayCollidingWithBlocks(level, source.x, source.y, source.z, fromX, fromY, fromZ, 0.3))
                    visibleBlocks++;
            }
        }
    }

    return (float) visibleBlocks / (float) totalBlocks;
}

bool Explosion::isRayCollidingWithBlocks(Level &level, double srcX, double srcY, double srcZ, double dstX,
                                         double dstY, double dstZ, double stepSize) {
    const double directionX = dstX - srcX;
    const double directionY = dstY - srcY;
    const double directionZ = dstZ - srcZ;
    if (directionX == 0.0 && directionY == 0.0 && directionZ == 0.0)
        return false;

    const double length = std::sqrt(directionX * directionX + directionY * directionY + directionZ * directionZ);
    const double normalX = directionX / length;
    const double normalY = directionY / length;
    const double normalZ = directionZ / length;

    for (double t = 0.0; t < length; t += stepSize) {
        const double rayX = srcX + normalX * t;
        const double rayY = srcY + normalY * t;
        const double rayZ = srcZ + normalZ * t;
        const int32_t x = (int32_t) std::floor(rayX);
        const int32_t y = (int32_t) std::floor(rayY);
        const int32_t z = (int32_t) std::floor(rayZ);

        if (y < LevelChunk::MIN_Y || y > LevelChunk::MAX_Y)
            continue;

        const BlockState *state = level.peekBlockPtr(x, y, z);
        if (state != nullptr
            && BlockShape::isPositionInside(*state, x, y, z, (float) rayX, (float) rayY, (float) rayZ))
            return true;
    }

    return false;
}
