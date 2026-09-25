#include "Actor/AI/Goal/EatBlockGoal.h"

#include "Actor/ActorFlags.h"
#include "Actor/Mob/MobActor.h"
#include "Block/BlockPaletteRegistry.h"
#include "Level/Level.h"
#include "Network/Handler/BlockActionHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cmath>
#include <cstdlib>
#include <initializer_list>
#include <random>

namespace {
    const char *const BABY_QUERIES[] = {"query.is_baby", "q.is_baby"};

    std::mt19937 &eatRandom() {
        static std::mt19937 generator(std::random_device{}());
        return generator;
    }

    std::string trim(const std::string &text) {
        const size_t first = text.find_first_not_of(" \t");
        if (first == std::string::npos)
            return std::string();
        const size_t last = text.find_last_not_of(" \t");
        return text.substr(first, last - first + 1);
    }

    BlockState defaultState(const std::string &name) {
        const Tag *states = BlockPaletteRegistry::getInstance().getDefaultStates(name);
        return states == nullptr ? BlockState(name) : BlockState(name, *states);
    }
}

EatBlockGoal::EatBlockGoal(std::vector<std::pair<std::string, std::string>> pairs, std::string chance,
                           int32_t eatTicks, std::string event)
        : mPairs(std::move(pairs)), mChance(std::move(chance)), mEatTicks(eatTicks), mEvent(std::move(event)) {
    setRequiredControlFlags((uint8_t) GoalControlFlag::Move | (uint8_t) GoalControlFlag::Look
                            | (uint8_t) GoalControlFlag::Jump);
}

float EatBlockGoal::_chance(const MobActor &mob) const {
    const size_t question = mChance.find('?');
    if (question == std::string::npos)
        return std::strtof(mChance.c_str(), nullptr);

    const size_t colon = mChance.find(':', question);
    if (colon == std::string::npos)
        return 0.0f;

    const std::string condition = trim(mChance.substr(0, question));
    bool truth = false;
    for (const char *query: BABY_QUERIES) {
        if (condition == query)
            truth = mob.getFlags().get(ActorFlag::Baby);
    }

    const std::string chosen = truth ? mChance.substr(question + 1, colon - question - 1)
                                     : mChance.substr(colon + 1);
    return std::strtof(trim(chosen).c_str(), nullptr);
}

const std::pair<std::string, std::string> *EatBlockGoal::_findPair(Level &level, const MobActor &mob,
                                                                     Vector3i &position) const {
    const Vector3f feet = mob.getPosition();
    const int32_t x = (int32_t) std::floor(feet.x);
    const int32_t y = (int32_t) std::floor(feet.y);
    const int32_t z = (int32_t) std::floor(feet.z);

    for (const int32_t candidateY: {y, y - 1}) {
        const std::string name = level.getBlockState(x, candidateY, z).mName;
        for (const std::pair<std::string, std::string> &pair: mPairs) {
            if (pair.first == name) {
                position = Vector3i(x, candidateY, z);
                return &pair;
            }
        }
    }
    return nullptr;
}

bool EatBlockGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    if (std::uniform_real_distribution<float>(0.0f, 1.0f)(eatRandom()) >= _chance(mob))
        return false;

    Vector3i position;
    return _findPair(owner.getLevelFor(mob), mob, position) != nullptr;
}

bool EatBlockGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    return mTimer > 0;
}

void EatBlockGoal::start(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    mTimer = mEatTicks;
    mob.getNavigation().stop(mob);
}

void EatBlockGoal::stop(ServerNetworkHandler &owner, MobActor &mob) {
    (void) owner;
    (void) mob;
    mTimer = 0;
}

void EatBlockGoal::tick(ServerNetworkHandler &owner, MobActor &mob) {
    if (--mTimer > 0)
        return;

    Level &level = owner.getLevelFor(mob);
    Vector3i position;
    const std::pair<std::string, std::string> *pair = _findPair(level, mob, position);
    if (pair == nullptr)
        return;

    const BlockState replacement = defaultState(pair->second);
    level.setBlock(position, replacement, true);
    BlockActionHandler::broadcastBlockUpdate(owner, level, position, replacement);

    if (!mEvent.empty())
        mob.fireEvent(owner, mEvent);
}
