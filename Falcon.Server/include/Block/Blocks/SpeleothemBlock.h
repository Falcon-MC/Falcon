#pragma once

#include "Block/Block.h"

class SpeleothemBlock : public Block {
public:
    explicit SpeleothemBlock(const Block &block) : Block(block) {
    }

    void onNeighbourChanged(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                            const BlockState &state) const override;

protected:
    static bool isHanging(const BlockState &state);

    bool pointsTowards(const BlockState &state, bool hanging) const;

    bool findTip(Level &level, const Vector3i &root, bool hanging, Vector3i &tip) const;

    bool canTipGrow(Level &level, const Vector3i &tip, bool hanging) const;

    void grow(Level &level, const Vector3i &tip, bool hanging) const;

    void refreshThickness(Level &level, const Vector3i &position, bool hanging) const;
};
