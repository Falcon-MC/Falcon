#pragma once

#include "World/Level/Block/PlantBlock.h"

class CactusBlock : public PlantBlock {
public:
    using PlantBlock::PlantBlock;

    static bool matches(const std::string &identifier);

    PathHazard getPathHazard() const override {
        return PathHazard::Damaging;
    }

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }

    bool getCollisionShape(const BlockState &state, const Actor *actor, AxisAlignedBB &shape) const override;

    void onActorInside(ServerNetworkHandler &owner, Actor &actor, const Vector3i &position,
                       const BlockState &state) const override;
};
