#pragma once

#include "World/Actor/AI/Goal/BaseMoveToBlockGoal.h"

// minecraft:behavior.move_to_liquid: a mob off its liquid walks to the nearest block topped by it
class MoveToLiquidGoal final : public BaseMoveToBlockGoal {
public:
    enum class Material { Water, Lava, Any };

    MoveToLiquidGoal(Settings settings, Material material);

    bool canUse(ServerNetworkHandler &owner, MobActor &mob) override;

    bool canContinueToUse(ServerNetworkHandler &owner, MobActor &mob) override;

protected:
    bool isValidTarget(ServerNetworkHandler &owner, MobActor &mob, const Vector3i &position) override;

private:
    bool _isMaterial(Level &level, int32_t x, int32_t y, int32_t z) const;

    bool _standsInMaterial(ServerNetworkHandler &owner, const MobActor &mob) const;

    Material mMaterial;
};
