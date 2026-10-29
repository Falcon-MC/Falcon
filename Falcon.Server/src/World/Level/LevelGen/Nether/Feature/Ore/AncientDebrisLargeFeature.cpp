#include "World/Level/LevelGen/Nether/Feature/Ore/AncientDebrisLargeFeature.h"

int32_t AncientDebrisLargeFeature::getClusterCount() const {
    return 2;
}

int32_t AncientDebrisLargeFeature::getClusterSize() const {
    return 3;
}

int32_t AncientDebrisLargeFeature::getMaxHeight() const {
    return 23;
}

const char *AncientDebrisLargeFeature::name() const {
    return "nether_ancientdebris_large";
}
