#pragma once

#include "Block/BlockState.h"
#include "Core/Math/Vector3i.h"

class Level;

enum class BlockChangeCause {
    Grow,
    Spread,
    Form,
    Fade,
    Decay
};

class BlockChangeSystem {
public:
    static bool allows(Level &level, const Vector3i &position, const BlockState &state, BlockChangeCause cause,
                       const Vector3i *source = nullptr);

    static bool change(Level &level, const Vector3i &position, const BlockState &state, BlockChangeCause cause,
                       bool update);

    static bool spread(Level &level, const Vector3i &source, const Vector3i &position, const BlockState &state,
                       bool update);

    static bool allowsFlow(Level &level, const Vector3i &source, const Vector3i &position, const BlockState &liquid);
};
