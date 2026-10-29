#include "World/Actor/Player/ExperienceManager.h"

#include <algorithm>
#include <cmath>
#include <limits>

const char *ExperienceManager::ATTRIBUTE_LEVEL = "minecraft:player.level";
const char *ExperienceManager::ATTRIBUTE_PROGRESS = "minecraft:player.experience";

int ExperienceManager::getXpToReachLevel(int level) {
    if (level <= 0)
        return 0;

    const double value = (double) level;

    if (level <= 16)
        return (int) (value * value + value * 6.0);

    if (level <= 31)
        return (int) (value * value * 2.5 - 40.5 * value + 360.0);

    return (int) (value * value * 4.5 - 162.5 * value + 2220.0);
}

int ExperienceManager::getXpToCompleteLevel(int level) {
    if (level <= 15)
        return 2 * level + 7;

    if (level <= 30)
        return 5 * level - 38;

    return 9 * level - 158;
}

float ExperienceManager::getLevelFromXp(int xp) {
    if (xp <= 0)
        return 0.0f;

    double a;
    double b;
    double c;

    if (xp <= getXpToReachLevel(16)) {
        a = 1.0;
        b = 6.0;
        c = 0.0;
    } else if (xp <= getXpToReachLevel(31)) {
        a = 2.5;
        b = -40.5;
        c = 360.0;
    } else {
        a = 4.5;
        b = -162.5;
        c = 2220.0;
    }

    const double discriminant = b * b - 4.0 * a * (c - (double) xp);
    if (discriminant < 0.0)
        return 0.0;

    const double root = (-b + std::sqrt(discriminant)) / (2.0 * a);
    if (root < 0.0)
        return 0.0;

    return (float) root;
}

void ExperienceManager::setXpLevel(int level) {
    setXpAndProgress(level, mXpProgress);
}

void ExperienceManager::addXpLevels(int amount) {
    setXpLevel(mXpLevel + amount);
}

void ExperienceManager::subtractXpLevels(int amount) {
    addXpLevels(-amount);
}

void ExperienceManager::setXpProgress(float progress) {
    setXpAndProgress(mXpLevel, progress);
}

void ExperienceManager::setXpAndProgress(int level, float progress) {
    mXpLevel = std::max(0, std::min(level, MAX_LEVEL));
    mXpProgress = std::max(0.0f, std::min(progress, 1.0f));
}

int ExperienceManager::getRemainderXp() const {
    return (int) ((float) getXpToCompleteLevel(mXpLevel) * mXpProgress);
}

int ExperienceManager::getCurrentTotalXp() const {
    return getXpToReachLevel(mXpLevel) + getRemainderXp();
}

void ExperienceManager::setCurrentTotalXp(int amount) {
    const int total = std::max(0, amount);
    const float newLevel = getLevelFromXp(total);
    const int level = (int) newLevel;

    setXpAndProgress(level, newLevel - (float) level);
}

void ExperienceManager::addXp(int amount) {
    const int capped = std::min(amount, std::numeric_limits<int>::max() - mLifetimeTotalXp);
    const int oldTotal = getCurrentTotalXp();

    setCurrentTotalXp(oldTotal + capped);

    if (capped > 0)
        mLifetimeTotalXp += capped;
}

void ExperienceManager::subtractXp(int amount) {
    addXp(-amount);
}

void ExperienceManager::setLifetimeTotalXp(int amount) {
    mLifetimeTotalXp = std::max(0, amount);
}

int ExperienceManager::getXpDropAmount() const {
    return std::min(100, 7 * mXpLevel);
}

void ExperienceManager::reset() {
    mXpLevel = 0;
    mXpProgress = 0.0f;
    mLifetimeTotalXp = 0;
}

void ExperienceManager::applyTo(ActorAttributes &attributes) const {
    attributes.set(ATTRIBUTE_LEVEL, (float) mXpLevel);
    attributes.set(ATTRIBUTE_PROGRESS, mXpProgress);
}
