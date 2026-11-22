#pragma once

#include "Core/NBT/Tag.h"
#include "World/Level/Particle/Particle.h"

#include <cstdint>

// a level event the client reads its own fields from
class GenericLevelEventParticle : public Particle {
public:
    GenericLevelEventParticle(int32_t eventId, const Vector3f &position, Tag data)
            : Particle(position), mEventId(eventId), mData(std::move(data)) {}

    std::unique_ptr<Packet> encode() const override;

private:
    int32_t mEventId;
    Tag mData;
};
