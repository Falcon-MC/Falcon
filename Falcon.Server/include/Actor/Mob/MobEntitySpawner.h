#pragma once

#include "Core/Json/Json.h"
#include "Core/NBT/Tag.h"

#include <cstdint>
#include <vector>

class MobActor;
class ServerNetworkHandler;

class MobEntitySpawner {
public:
    void tick(ServerNetworkHandler &owner, MobActor &mob);

    void saveNbt(Tag &data) const;

    void loadNbt(const Tag &data);

private:
    void _start(const json::Value *component);

    static void _spawn(ServerNetworkHandler &owner, MobActor &mob, const json::Value &entry);

    const json::Value *mComponent = nullptr;
    std::vector<int32_t> mTicks;
    std::vector<int32_t> mSavedTicks;
    int64_t mLastTick = -1;
};
