#include "World/Level/LevelGen/Overworld/Feature/Decoration/SwampSeagrassFeature.h"

const char *SwampSeagrassFeature::name() const {
    return SeagrassFeature::name();
}

int32_t SwampSeagrassFeature::getBase() const {
    return 48;
}

float SwampSeagrassFeature::getTallSeagrassProbability() const {
    return 0.6f;
}
