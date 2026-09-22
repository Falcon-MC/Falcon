#include "Actor/Actor.h"

#include "Item/EnchantmentData.h"
#include "Item/ItemEnchantments.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>

namespace {
    const char *const UNDEAD_ACTORS[] = {
            "minecraft:bogged", "minecraft:drowned", "minecraft:husk", "minecraft:parched",
            "minecraft:phantom", "minecraft:skeleton", "minecraft:stray", "minecraft:wither",
            "minecraft:wither_skeleton", "minecraft:zombie", "minecraft:zombie_horse",
            "minecraft:zombie_pigman", "minecraft:zombie_villager", "minecraft:zombie_villager_v2"
    };

    const char *const ARTHROPOD_ACTORS[] = {
            "minecraft:spider", "minecraft:cave_spider", "minecraft:silverfish", "minecraft:endermite"
    };

    const char *const FIRE_IMMUNE_ACTORS[] = {
            "minecraft:blaze", "minecraft:ender_dragon", "minecraft:ghast", "minecraft:magma_cube",
            "minecraft:strider", "minecraft:wither", "minecraft:wither_skeleton", "minecraft:zoglin",
            "minecraft:zombie_pigman"
    };

    template<size_t N>
    bool containsIdentifier(const char *const (&identifiers)[N], const char *identifier) {
        for (const char *candidate: identifiers) {
            if (std::strcmp(candidate, identifier) == 0)
                return true;
        }

        return false;
    }

    const char *ATTRIBUTE_HEALTH = "minecraft:health";
    const char *ATTRIBUTE_HUNGER = "minecraft:player.hunger";
    const char *ATTRIBUTE_SATURATION = "minecraft:player.saturation";
    const char *ATTRIBUTE_EXHAUSTION = "minecraft:player.exhaustion";

    const int DIFFICULTY_PEACEFUL = 0;
    const int DIFFICULTY_EASY = 1;
    const int DIFFICULTY_NORMAL = 2;
    const int DIFFICULTY_HARD = 3;
}

const float Actor::EXHAUSTION_PER_UNIT = 4.0f;
const float Actor::FALL_DAMAGE_THRESHOLD = 3.0f;

Actor::Actor(uint64_t runtimeId) : mRuntimeId(runtimeId), mEffects(*this) {}

void Actor::setOnFire(bool onFire) {
    if (onFire)
        setFireTicks(std::max(mFireTicks, 160));
    else
        extinguish();
}

void Actor::setFireTicks(int fireTicks) {
    mFireTicks = std::max(0, std::min(fireTicks, 32767));
    mFlags.set(ActorFlag::OnFire, mFireTicks > 0);
}

void Actor::extinguish() {
    mFireTicks = 0;
    mFlags.set(ActorFlag::OnFire, false);
}

bool Actor::tickFire() {
    if (mFireTicks <= 0) {
        mFlags.set(ActorFlag::OnFire, false);
        return false;
    }

    --mFireTicks;
    if (mFireTicks <= 0) {
        mFlags.set(ActorFlag::OnFire, false);
        return false;
    }

    mFlags.set(ActorFlag::OnFire, true);
    return mFireTicks % 20 == 0;
}

void Actor::teleport(const Vector3f &position) {
    mPosition = position;
    mMotion = Vector3f(0.0f, 0.0f, 0.0f);
    mOnGround = true;
    resetFallDistance();
}

void Actor::setXpAndProgress(int level, float progress) {
    mExperience.setXpAndProgress(level, progress);
    syncExperience();
}

void Actor::addXp(int amount) {
    mExperience.addXp(amount);
    syncExperience();
}

void Actor::addXpLevels(int amount) {
    mExperience.addXpLevels(amount);
    syncExperience();
}

float Actor::getHealth() const {
    return mAttributes.get(ATTRIBUTE_HEALTH);
}

void Actor::setHealth(float health) {
    mAttributes.setClamped(ATTRIBUTE_HEALTH, health);
}

float Actor::getMaxHealth() const {
    return mAttributes.getMaximum(ATTRIBUTE_HEALTH);
}

void Actor::setMaxHealth(float maxHealth) {
    mAttributes.setBaseMaximum(ATTRIBUTE_HEALTH, maxHealth);
}

std::string Actor::getName() const {
    const std::string identifier = getIdentifier();
    const size_t separator = identifier.find(':');
    return "%entity." + (separator == std::string::npos ? identifier : identifier.substr(separator + 1)) + ".name";
}

bool Actor::isAlive() const {
    return getHealth() > 0.0f;
}

void Actor::tickCombat(int tickDiff) {
    if (tickDiff <= 0)
        return;

    mNoDamageTicks = std::max(0, mNoDamageTicks - tickDiff);
    mAttackTime = std::max(0, mAttackTime - tickDiff);
    if (mNoDamageTicks == 0)
        mLastDamageAmount = 0.0f;
}

void Actor::knockBack(float x, float z, float force, float verticalLimit) {
    const float length = std::sqrt(x * x + z * z);
    if (length <= 0.0f || force <= 0.0f)
        return;

    const float resistance = std::clamp(getAttributes().get("minecraft:knockback_resistance"), 0.0f, 1.0f);
    const float base = force * (1.0f - resistance);
    if (base <= 0.0f)
        return;

    const float inverse = 1.0f / length;
    Vector3f motion = getMotion();
    motion.x = motion.x * 0.5f + x * inverse * base;
    motion.y = std::min(motion.y * 0.5f + base, verticalLimit);
    motion.z = motion.z * 0.5f + z * inverse * base;
    setMotion(motion);
}

float Actor::getFood() const {
    return mAttributes.get(ATTRIBUTE_HUNGER);
}

float Actor::getMaxFood() const {
    return mAttributes.getMaximum(ATTRIBUTE_HUNGER);
}

void Actor::setFood(float food) {
    const float old = mAttributes.get(ATTRIBUTE_HUNGER);
    mAttributes.setClamped(ATTRIBUTE_HUNGER, food);

    const float updated = mAttributes.get(ATTRIBUTE_HUNGER);
    const float bounds[3] = {17.0f, 6.0f, 0.0f};

    for (float bound: bounds) {
        if ((old > bound) != (updated > bound)) {
            mFoodTickTimer = 0;
            break;
        }
    }
}

void Actor::addFood(float amount) {
    setFood(mAttributes.get(ATTRIBUTE_HUNGER) + amount);
}

bool Actor::isHungry() const {
    return getFood() < getMaxFood();
}

bool Actor::canEat() const {
    return isHungry();
}

float Actor::getSaturation() const {
    return mAttributes.get(ATTRIBUTE_SATURATION);
}

void Actor::setSaturation(float saturation) {
    mAttributes.setClamped(ATTRIBUTE_SATURATION, saturation);
}

void Actor::addSaturation(float amount) {
    setSaturation(mAttributes.get(ATTRIBUTE_SATURATION) + amount);
}

float Actor::getExhaustion() const {
    return mAttributes.get(ATTRIBUTE_EXHAUSTION);
}

void Actor::setExhaustion(float exhaustion) {
    mAttributes.setClamped(ATTRIBUTE_EXHAUSTION, exhaustion);
}

bool Actor::hasTag(const std::string &tag) const {
    return std::find(mTags.begin(), mTags.end(), tag) != mTags.end();
}

bool Actor::addTag(const std::string &tag) {
    if (tag.empty() || hasTag(tag))
        return false;

    mTags.push_back(tag);
    return true;
}

bool Actor::removeTag(const std::string &tag) {
    const auto found = std::find(mTags.begin(), mTags.end(), tag);
    if (found == mTags.end())
        return false;

    mTags.erase(found);
    return true;
}

void Actor::saveTags(Tag &data) const {
    Tag tags = Tag::ofList(Tag::Type::String);
    for (const std::string &tag: mTags)
        tags.addToList(Tag::ofString(tag));
    data.put("Tags", tags);
}

void Actor::loadTags(const Tag &data) {
    mTags.clear();

    const Tag *tags = data.get("Tags");
    if (tags == nullptr || tags->getType() != Tag::Type::List)
        return;

    for (const Tag &tag: tags->getList()) {
        if (tag.getType() == Tag::Type::String)
            addTag(tag.asString());
    }
}

int64_t Actor::getVisibleEffectsData() const {
    std::map<int32_t, bool> visible;
    for (const auto &entry: mEffects.getAll()) {
        const MobEffectInstance &effect = entry.second;
        if (effect.mParticles)
            visible[(int32_t) effect.mId] = effect.mAmbient;
    }

    int64_t packed = 0;
    int packedCount = 0;
    for (const auto &entry: visible) {
        packed = (packed << 7) | ((int64_t) (entry.first & 0x3f) << 1) | (entry.second ? 1 : 0);
        if (++packedCount >= 8)
            break;
    }

    return packed;
}

bool Actor::refreshVisibleEffects() {
    const int64_t packed = getVisibleEffectsData();
    if (packed == mSentVisibleEffects)
        return false;

    mSentVisibleEffects = packed;
    return true;
}

bool Actor::isUndead() const {
    return containsIdentifier(UNDEAD_ACTORS, getIdentifier());
}

bool Actor::isArthropod() const {
    return containsIdentifier(ARTHROPOD_ACTORS, getIdentifier());
}

bool Actor::isFireImmune() const {
    return containsIdentifier(FIRE_IMMUNE_ACTORS, getIdentifier());
}

float Actor::getMeleeEnchantmentBonus(const ItemStack &weapon) const {
    float bonus = 1.25f * (float) ItemEnchantments::getLevel(weapon, EnchantmentIds::SHARPNESS);

    if (isUndead())
        bonus += 2.5f * (float) ItemEnchantments::getLevel(weapon, EnchantmentIds::SMITE);

    if (isArthropod())
        bonus += 2.5f * (float) ItemEnchantments::getLevel(weapon, EnchantmentIds::BANE_OF_ARTHROPODS);

    return bonus;
}

void Actor::onMeleeEnchantmentHit(const ItemStack &weapon) {
    const int32_t baneOfArthropods = ItemEnchantments::getLevel(weapon, EnchantmentIds::BANE_OF_ARTHROPODS);
    if (baneOfArthropods <= 0 || !isArthropod())
        return;

    MobEffectInstance slowness;
    slowness.mId = MobEffectId::Slowness;
    slowness.mDuration = 20 + std::rand() % (10 * baneOfArthropods);
    slowness.mAmplifier = 3;
    addEffect(slowness);
}

void Actor::exhaust(float amount) {
    if (!mHungerEnabled || mDifficulty == DIFFICULTY_PEACEFUL || hasEffect(MobEffectId::Saturation))
        return;

    float exhaustion = getExhaustion() + amount;

    while (exhaustion >= EXHAUSTION_PER_UNIT) {
        exhaustion -= EXHAUSTION_PER_UNIT;

        const float saturation = getSaturation();
        if (saturation > 0.0f) {
            setSaturation(std::max(0.0f, saturation - 1.0f));
            continue;
        }

        const float food = getFood();
        if (food > 0.0f)
            setFood(std::max(0.0f, food - 1.0f));
    }

    setExhaustion(exhaustion);
}

void Actor::setFoodTickTimer(int foodTickTimer) {
    mFoodTickTimer = std::max(0, foodTickTimer);
}

void Actor::consumeFood(int nutrition, float saturation) {
    addFood((float) nutrition);
    addSaturation(saturation);
}

void Actor::resetHungerAndExperience() {
    mAttributes.setClamped(ATTRIBUTE_HUNGER, getMaxFood());
    mAttributes.setClamped(ATTRIBUTE_SATURATION, mAttributes.getMaximum(ATTRIBUTE_SATURATION));
    mAttributes.setClamped(ATTRIBUTE_EXHAUSTION, 0.0f);
    mFoodTickTimer = 0;

    mExperience.reset();
    syncExperience();
}

bool Actor::tickHunger(int tickDiff, int difficulty, bool naturalRegeneration) {
    mDifficulty = difficulty;

    if (!isAlive() || !mHungerEnabled)
        return false;

    const float previousFood = getFood();
    const float previousSaturation = getSaturation();
    const float previousExhaustion = getExhaustion();
    const float previousHealth = getHealth();

    float food = previousFood;
    const float maxHealth = getMaxHealth();

    mFoodTickTimer += tickDiff;
    if (mFoodTickTimer >= FOOD_TICK_PERIOD)
        mFoodTickTimer = 0;

    if (difficulty == DIFFICULTY_PEACEFUL && mFoodTickTimer % 10 == 0) {
        if (food < getMaxFood()) {
            addFood(1.0f);
            food = getFood();
        }

        if (mFoodTickTimer % 20 == 0 && naturalRegeneration && getHealth() < maxHealth)
            mAttributes.setClamped(ATTRIBUTE_HEALTH, getHealth() + 1.0f);
    }

    if (mFoodTickTimer == 0) {
        if (food >= 18.0f && naturalRegeneration) {
            if (getHealth() < maxHealth) {
                mAttributes.setClamped(ATTRIBUTE_HEALTH, getHealth() + 1.0f);
                exhaust(6.0f);
            }
        } else if (food <= 0.0f) {
            const float health = getHealth();
            const bool starve = (difficulty == DIFFICULTY_EASY && health > 10.0f)
                                || (difficulty == DIFFICULTY_NORMAL && health > 1.0f)
                                || difficulty == DIFFICULTY_HARD;

            if (starve)
                mPendingStarveDamage = true;
        }
    }

    if (!canSprint())
        mFlags.set(ActorFlag::Sprinting, false);

    return getFood() != previousFood
           || getSaturation() != previousSaturation
           || getExhaustion() != previousExhaustion
           || getHealth() != previousHealth;
}

void Actor::kill() {
    mIsDead = true;
    mEffects.clear();
    extinguish();
    mAttributes.set(ATTRIBUTE_HEALTH, 0.0f);
    setMotion(Vector3f(0.0f, 0.0f, 0.0f));
    resetFallDistance();
    mNoDamageTicks = 0;
    mAttackTime = 0;
    mLastDamageAmount = 0.0f;
}

float Actor::reduceHealth(float amount) {
    if (amount <= 0.0f || !isAlive())
        return getHealth();

    if (const MobEffectInstance *resistance = getEffect(MobEffectId::Resistance))
        amount *= std::max(0.0f, 1.0f - 0.2f * (float) resistance->level());

    const float absorption = mAttributes.get("minecraft:absorption");
    if (absorption > 0.0f) {
        const float absorbed = std::min(absorption, amount);
        mAttributes.setClamped("minecraft:absorption", absorption - absorbed);
        amount -= absorbed;
    }

    const float newHealth = getHealth() - amount;
    mAttributes.setClamped(ATTRIBUTE_HEALTH, newHealth);
    return getHealth();
}

float Actor::heal(float amount) {
    if (amount <= 0.0f || !isAlive())
        return getHealth();
    mAttributes.setClamped(ATTRIBUTE_HEALTH, getHealth() + amount);
    return getHealth();
}

float Actor::computeFallDamage() const {
    if (hasEffect(MobEffectId::SlowFalling))
        return 0.0f;

    const float distance = getFallDistance();
    const MobEffectInstance *jumpBoost = getEffect(MobEffectId::JumpBoost);
    const float jumpBoostLevel = jumpBoost == nullptr ? 0.0f : (float) jumpBoost->level();
    const float damage = std::ceil(distance - FALL_DAMAGE_THRESHOLD - jumpBoostLevel);
    if (damage < 1.0f)
        return 0.0f;

    return damage;
}
