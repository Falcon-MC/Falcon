#pragma once

#include "Block/Blocks/ReplaceableBlock.h"

class VineBlock : public ReplaceableBlock {
public:
    explicit VineBlock(const Block &block) : ReplaceableBlock(block) {
    }

    static bool matches(const std::string &identifier);

    void onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                      const BlockState &state) const override;

private:
    static bool canSpread(Level &level, const Vector3i &position);

    static void putVine(Level &level, const Vector3i &source, const Vector3i &position, int32_t bits);

    static void putVineOnHorizontalFace(Level &level, const Vector3i &source, const Vector3i &position,
                                        int32_t bits);
};
