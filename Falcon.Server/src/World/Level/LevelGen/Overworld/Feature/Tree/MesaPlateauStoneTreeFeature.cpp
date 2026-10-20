#include "World/Level/LevelGen/Overworld/Feature/Tree/MesaPlateauStoneTreeFeature.h"

#include "World/Level/LevelGen/Overworld/Feature/Tree/MesaTreeFeature.h"

const char *MesaPlateauStoneTreeFeature::NAME = "minecraft:mesa_plateau_stone_surface_trees_feature";

const char *MesaPlateauStoneTreeFeature::name() const {
    return NAME;
}

std::vector<const char *> MesaPlateauStoneTreeFeature::getFeatures() const {
    return std::vector<const char *>{
            "minecraft:dead_bush_feature",
            "minecraft:mesa_foliage_feature",
            MesaTreeFeature::NAME
    };
}
