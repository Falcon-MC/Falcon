#include "World/Level/Block/SweetBerryBushBlock.h"

#include "World/Actor/Actor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/PlantGrowthHelpers.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(SweetBerryBushBlock, 318);

using namespace PlantGrowthHelpers;

namespace {
    const char *GROWTH = "growth";
    const int32_t MAX_BERRY_GROWTH = 3;
    const int32_t BERRY_GROWTH_CHANCE = 5;
    const int32_t BERRY_MIN_LIGHT = 9;
    const Vector3f STUCK_MULTIPLIER(0.8f, 0.75f, 0.8f);
}

bool SweetBerryBushBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:sweet_berry_bush";
}

void SweetBerryBushBlock::onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                                        const BlockState &state) const {
    (void) owner;
    (void) position;
    (void) state;
    actor.makeStuckInBlock(STUCK_MULTIPLIER);
}

void SweetBerryBushBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                       const BlockState &state) const {
    (void) owner;

    const int32_t growth = state.mStates.getInt(GROWTH, 0);
    if (growth >= MAX_BERRY_GROWTH || RandomTickSystem::nextInt(BERRY_GROWTH_CHANCE) != 0)
        return;

    if (RandomTickSystem::getFullLight(level, above(position)) < BERRY_MIN_LIGHT)
        return;

    BlockChangeSystem::change(level, position, DecorationSupport::withState(state, GROWTH, growth + 1),
                              BlockChangeCause::Grow, true);
}

bool SweetBerryBushBlock::getStuckMultiplier(const Actor &actor, Vector3f &multiplier) const {
    (void) actor;
    multiplier = STUCK_MULTIPLIER;
    return true;
}
