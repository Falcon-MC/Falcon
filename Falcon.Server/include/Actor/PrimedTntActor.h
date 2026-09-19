#pragma once

#include "Actor/ServerActor.h"

#include <cstdint>

class PrimedTntActor : public ServerActor {
public:
    static const char *IDENTIFIER;

    static const float GRAVITY;
    static const float DRAG;
    static const double EXPLOSION_Y_OFFSET;
    static const double EXPLOSION_SIZE;
    static const int32_t DEFAULT_FUSE = 80;

    PrimedTntActor(uint64_t runtimeId, int32_t fuse);

    void tick(ServerNetworkHandler &owner) override;

    bool isExpired() const override {
        return mExpired;
    }

    bool shouldSave() const override {
        return !mExpired;
    }

    void fillSpawnMetadata(EntityDataMap &metadata) const override;

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    void _explode(ServerNetworkHandler &owner);

    int32_t mFuse;
    bool mExpired = false;
};
