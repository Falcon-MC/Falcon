#pragma once

#include "World/Level/Particle/Particle.h"

// one puff of a sonic boom's line
class SonicExplosionParticle : public Particle {
public:
    explicit SonicExplosionParticle(const Vector3f &position) : Particle(position) {}

    std::unique_ptr<Packet> encode() const override;
};
