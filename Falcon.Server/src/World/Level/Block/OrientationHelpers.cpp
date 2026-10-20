#include "World/Level/Block/OrientationHelpers.h"

#include "World/Level/Block/Components/PlacementOrientation.h"

namespace OrientationHelpers {
    void setFacingDirection(Tag &states, int facing) {
        if (states.contains("facing_direction"))
            states.putInt("facing_direction", facing);

        if (states.contains("minecraft:facing_direction"))
            states.putString("minecraft:facing_direction", PlacementOrientation::faceName(facing));
    }
}
