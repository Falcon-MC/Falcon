#pragma once

#include "World/Actor/Mob/MobActor.h"

#include <cstdint>
#include <string>

class PluginActor : public MobActor {
public:
    PluginActor(uint64_t runtimeId, const std::string &identifier);

    ActorCategory getCategory() const override;

    ActorSize getSize() const override;

    float getDefaultMaxHealth() const override;

    void tick(ServerNetworkHandler &owner) override;

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player) override;

    bool isDrawnByPlugin() const override {
        return mPluginDrawn;
    }

    // its plugin shows it with packets of its own: Falcon sends nothing about it and leaves its movement alone
    void setDrawnByPlugin(bool pluginDrawn) {
        mPluginDrawn = pluginDrawn;
    }

    // the box hits aim at, in place of the type's; 0 or less keeps the type's
    void setSize(float width, float height) {
        mWidth = width;
        mHeight = height;
    }

    Tag saveNbt() const override;

    void loadNbt(const Tag &data) override;

private:
    bool mPluginDrawn = false;
    float mWidth = 0.0f;
    float mHeight = 0.0f;
};
