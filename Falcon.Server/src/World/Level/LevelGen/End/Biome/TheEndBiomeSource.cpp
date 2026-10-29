#include "World/Level/LevelGen/End/Biome/TheEndBiomeSource.h"

#include "World/Level/LevelGen/End/TheEndGeneratorConstants.h"

TheEndBiomeSource::TheEndBiomeSource() {
}

int32_t TheEndBiomeSource::pickBiomeId(int32_t, int32_t, int32_t) const {
    return TheEndGeneratorConstants::BIOME_ID;
}
