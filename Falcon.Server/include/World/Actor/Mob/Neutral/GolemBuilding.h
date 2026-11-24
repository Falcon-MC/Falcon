#pragma once

#include "Core/Math/Vector3i.h"

#include <cstdint>

class Level;
class ServerNetworkHandler;

// a carved pumpkin or a jack o'lantern placed on top of snow or iron blocks turns them into a golem
namespace GolemBuilding {
    enum class Kind { None, Snow, Iron };

    struct Match {
        Kind mKind = Kind::None;
        // the pattern's first cell, its rows running along up and its columns along forwards x up
        Vector3i mOrigin;
        int32_t mForwards = 0;
        int32_t mUp = 0;
    };

    // the snow golem with the placed block as its head, else the iron golem anywhere around it
    Match find(Level &level, const Vector3i &pumpkin);

    // spawns the golem the pattern makes and clears the blocks; false when nothing matched
    bool tryBuild(ServerNetworkHandler &owner, Level &level, const Vector3i &pumpkin);
}
