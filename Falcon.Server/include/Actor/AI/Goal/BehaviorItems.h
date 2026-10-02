#pragma once

#include "Core/Json/Json.h"

#include <string>
#include <vector>

class ItemStack;
class MobActor;
class ServerNetworkHandler;
class ServerPlayer;

class BehaviorItems {
public:
    explicit BehaviorItems(const json::Value *items);

    bool contains(const ItemStack &item) const;

    ServerPlayer *findNearestHolder(ServerNetworkHandler &owner, const MobActor &mob, float range) const;

    bool isEmpty() const {
        return mItems.empty();
    }

private:
    std::vector<std::string> mItems;
};
