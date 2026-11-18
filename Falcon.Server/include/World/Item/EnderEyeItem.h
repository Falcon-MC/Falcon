#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"
#include "World/Item/Item.h"

class ChunkGenerator;

// thrown at the nearest stronghold; filling a frame is the frame's own interaction
class EnderEyeItem : public Item {
public:
    explicit EnderEyeItem(const Item &base);

    static bool matches(const std::string &identifier);

    // the block the eye flies to from a player standing at the position; false when no stronghold is found
    static bool findTarget(const ChunkGenerator &generator, const Vector3f &position, Vector3i &out);

    bool onUse(ServerNetworkHandler &owner, ServerPlayer &player, const ItemStack &item) const override;
};
