#include "World/Actor/AI/Goal/MoveToLiquidGoal.h"

#include "World/Actor/Mob/MobActor.h"
#include "World/Level/Block/LiquidView.h"
#include "World/Level/Level.h"
#include "Network/ServerNetworkHandler.h"

#include <cmath>

namespace {
    const char *const AIR = "minecraft:air";
}

MoveToLiquidGoal::MoveToLiquidGoal(Settings settings, Material material)
        : BaseMoveToBlockGoal(settings), mMaterial(material) {}

bool MoveToLiquidGoal::_isMaterial(Level &level, int32_t x, int32_t y, int32_t z) const {
    const BlockState *state = level.peekBlockPtr(x, y, z);
    if (state == nullptr)
        return false;

    const LiquidView liquid(*state);
    return mMaterial == Material::Lava ? liquid.isLava() : liquid.isWater();
}

bool MoveToLiquidGoal::_standsInMaterial(ServerNetworkHandler &owner, const MobActor &mob) const {
    const Vector3f position = mob.getPosition();
    return _isMaterial(owner.getLevelFor(mob), (int32_t) std::floor(position.x), (int32_t) std::floor(position.y),
                       (int32_t) std::floor(position.z));
}

// the game never runs the goal for the any material, nor while the block at the feet is the liquid already
bool MoveToLiquidGoal::canUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mMaterial != Material::Any && !_standsInMaterial(owner, mob) && BaseMoveToBlockGoal::canUse(owner, mob);
}

bool MoveToLiquidGoal::canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) {
    return mMaterial != Material::Any && !_standsInMaterial(owner, mob)
           && BaseMoveToBlockGoal::canContinueToUse(owner, mob);
}

// over the mob's whole footprint: the liquid on top of the block and air above it
bool MoveToLiquidGoal::isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) {
    Level &level = owner.getLevelFor(mob);
    const int32_t footprint = (int32_t) std::floor(mob.getSize().mWidth);
    for (int32_t dx = 0; dx <= footprint; ++dx) {
        for (int32_t dz = 0; dz <= footprint; ++dz) {
            if (!_isMaterial(level, position.x + dx, position.y + 1, position.z + dz))
                return false;
            const BlockState *above = level.peekBlockPtr(position.x + dx, position.y + 2, position.z + dz);
            if (above == nullptr || above->mName != AIR)
                return false;
        }
    }
    return true;
}
