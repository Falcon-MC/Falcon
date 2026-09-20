#pragma once

#include "Actor/ServerActor.h"

class SizedActor : public ServerActor {
public:
    SizedActor(uint64_t runtimeId, const std::string &identifier, const ActorSize &size, bool projectile)
            : ServerActor(runtimeId, identifier), mSize(size) {
        setProjectile(projectile);
    }

    ActorSize getSize() const override { return mSize; }

private:
    ActorSize mSize;
};
