#include "Block/Blocks/FenceHelpers.h"

namespace FenceHelpers {
    AxisAlignedBB postShape() {
        return AxisAlignedBB(POST_MIN, 0.0f, POST_MIN, POST_MAX, POST_HEIGHT, POST_MAX);
    }
}
