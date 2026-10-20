#include "World/Level/LevelGen/Overworld/Feature/Decoration/WarmOceanSeagrassFeature.h"

const char *WarmOceanSeagrassFeature::name() const {
    return SeagrassFeature::name();
}

int32_t WarmOceanSeagrassFeature::getBase() const {
    return 45;
}
