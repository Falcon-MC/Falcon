#include "World/Level/LevelGen/Overworld/Feature/Decoration/SeaAnemoneFeature.h"

const char *SeaAnemoneFeature::name() const {
    return "minecraft:sea_anemone_feature";
}

int32_t SeaAnemoneFeature::getBase() const {
    return 1;
}

int32_t SeaAnemoneFeature::getRandom() const {
    return 0;
}

bool SeaAnemoneFeature::placeFeature(LevelChunk &chunk, IRandom &random, int32_t x, int32_t y, int32_t z,
                                     const BlockState &coralState) {
    return placeCoralBlock(chunk, random, x, y, z, coralState);
}
