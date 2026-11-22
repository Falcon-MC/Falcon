#pragma once

#include "Core/Math/Vector3i.h"
#include "World/Level/Particle/Particle.h"

// the rings rising from a shrieking shrieker
class ShriekParticle : public Particle {
public:
    explicit ShriekParticle(const Vector3i &origin);

    std::unique_ptr<Packet> encode() const override;

private:
    Vector3i mOrigin;
};
