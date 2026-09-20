#include "Actor/Mob/MobActor.h"

#include <random>

namespace {
    std::mt19937 &experienceRandom() {
        static std::mt19937 generator(0x9E3779B9u);
        return generator;
    }
}

MobActor::MobActor(uint64_t runtimeId, const std::string &identifier) : ServerActor(runtimeId, identifier) {
}

float MobActor::resolveMaxHealth(Difficulty difficulty) const {
    (void) difficulty;
    return getDefaultMaxHealth();
}

void MobActor::applyDefaults(Difficulty difficulty) {
    const float health = resolveMaxHealth(difficulty);
    setMaxHealth(health);
    setHealth(health);
}

const std::vector<LootEntry> &MobActor::getLootEntries() const {
    static const std::vector<LootEntry> none;
    return none;
}

int MobActor::randomRange(int minimum, int maximum) {
    if (minimum >= maximum)
        return minimum;

    std::uniform_int_distribution<int> distribution(minimum, maximum);
    return distribution(experienceRandom());
}

std::vector<MobDrop> MobActor::rollDrops(bool onFire, int32_t lootingLevel) const {
    return MobLoot::roll(getLootEntries(), onFire, lootingLevel);
}
