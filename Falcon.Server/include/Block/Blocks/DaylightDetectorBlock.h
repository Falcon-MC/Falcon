#pragma once

#include "Block/Block.h"

#include <string>

class Level;

class DaylightDetectorBlock : public Block {
public:
    explicit DaylightDetectorBlock(const Block &base);

    static bool matches(const std::string &identifier);

    bool onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                    const BlockState &state) const override;

    bool isSignalSource() const override;

    static int computeSignal(Level &level, const Vector3i &position, bool inverted);

    static int effectiveSkyLightAround(Level &level, const Vector3i &position);
};
