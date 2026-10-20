#pragma once

#include "Core/Math/Vector3f.h"

class Level;

struct LiquidContact {
    bool water = false;
    bool lava = false;
    bool bubble = false;
    bool dragDown = false;
    bool eyeSubmerged = false;
    /** Eyes in water for breathing: unlike eyeSubmerged, a bubble column does not count. */
    bool eyeInWater = false;
    /** Water at the feet, below its surface. */
    bool feetInWater = false;
    Vector3f flow;
};

class LiquidBlocksFetch {
public:
    static LiquidContact at(Level &level, const Vector3f &feet);
};
