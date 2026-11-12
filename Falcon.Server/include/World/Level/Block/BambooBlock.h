#pragma once

#include "World/Level/Block/Block.h"

class BambooBlock : public Block {
public:
    explicit BambooBlock(const Block &block) : Block(block) {}

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

    bool hasRandomTick() const override {
        return true;
    }

    bool getCollisionShapeAt(const BlockState &state, const Vector3i &position, const Actor *actor,
                             AxisAlignedBB &shape) const override;

private:
    static int32_t countStalkBelow(Level &level, const Vector3i &position);

    static void grow(Level &level, const Vector3i &position, const BlockState &state, int32_t height);
};
