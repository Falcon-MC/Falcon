#pragma once

#include "Core/Math/Vector3i.h"
#include "Level/Particle/Particle.h"

#include <vector>

class BlockExplodeParticle : public Particle {
public:
    BlockExplodeParticle(const Vector3f &position, double radius, const std::vector<Vector3i> &blocks);

    std::unique_ptr<Packet> encode() const override;

private:
    double mRadius;
    std::vector<Vector3i> mBlocks;
};
