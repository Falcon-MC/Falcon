#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/Math/Vector3i.h"

class Level;

struct FireContact {
    bool fire = false;
    float damage = 0.0f;
    Vector3i fireBlock;
};

class FireBlocksFetch {
public:
    static FireContact at(Level &level, const Vector3f &feet);
};
