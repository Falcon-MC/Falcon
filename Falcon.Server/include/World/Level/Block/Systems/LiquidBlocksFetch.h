#pragma once

#include "Core/Math/Vector3f.h"

class Level;

struct LiquidContact {
    bool water = false;
    bool lava = false;
    bool bubble = false;
    bool dragDown = false;
    bool eyeSubmerged = false;
    // for breathing: unlike eyeSubmerged, a bubble column does not count
    bool eyeInWater = false;
    // below the water surface
    bool feetInWater = false;
    bool swimProbeInWater = false;
    Vector3f flow;
};

class LiquidBlocksFetch {
public:
    static LiquidContact at(Level &level, const Vector3f &feet, bool horizontalPose = false, float bodyHeight = 1.8f);
};
