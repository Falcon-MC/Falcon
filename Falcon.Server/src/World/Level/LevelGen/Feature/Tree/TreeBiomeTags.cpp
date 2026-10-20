#include "World/Level/LevelGen/Feature/Tree/TreeBiomeTags.h"

#include "World/Level/LevelGen/Biome/BiomeIds.h"

bool TreeBiomeTags::isBamboo(int32_t biomeId) {
    return biomeId == BiomeIds::BAMBOO_JUNGLE;
}

bool TreeBiomeTags::isBirch(int32_t biomeId) {
    return biomeId == BiomeIds::BIRCH_FOREST || biomeId == BiomeIds::BIRCH_FOREST_MUTATED;
}

bool TreeBiomeTags::isCherryGrove(int32_t biomeId) {
    return biomeId == BiomeIds::CHERRY_GROVE;
}

bool TreeBiomeTags::isEdge(int32_t biomeId) {
    return biomeId == BiomeIds::JUNGLE_EDGE;
}

bool TreeBiomeTags::isFlowerForest(int32_t biomeId) {
    return biomeId == BiomeIds::FLOWER_FOREST;
}

bool TreeBiomeTags::isForest(int32_t biomeId) {
    return biomeId == BiomeIds::FOREST || biomeId == BiomeIds::FLOWER_FOREST
           || biomeId == BiomeIds::BIRCH_FOREST || biomeId == BiomeIds::BIRCH_FOREST_MUTATED
           || biomeId == BiomeIds::ROOFED_FOREST;
}

bool TreeBiomeTags::isGrove(int32_t biomeId) {
    return biomeId == BiomeIds::GROVE;
}

bool TreeBiomeTags::isJungle(int32_t biomeId) {
    return biomeId == BiomeIds::JUNGLE || biomeId == BiomeIds::JUNGLE_EDGE
           || biomeId == BiomeIds::BAMBOO_JUNGLE;
}

bool TreeBiomeTags::isMangroveSwamp(int32_t biomeId) {
    return biomeId == BiomeIds::MANGROVE_SWAMP;
}

bool TreeBiomeTags::isMeadow(int32_t biomeId) {
    return biomeId == BiomeIds::MEADOW;
}

bool TreeBiomeTags::isMesa(int32_t biomeId) {
    return biomeId == BiomeIds::MESA || biomeId == BiomeIds::MESA_PLATEAU_STONE
           || biomeId == BiomeIds::MESA_BRYCE;
}

bool TreeBiomeTags::isMooshroomIsland(int32_t biomeId) {
    return biomeId == BiomeIds::MUSHROOM_ISLAND;
}

bool TreeBiomeTags::isPaleGarden(int32_t biomeId) {
    return biomeId == BiomeIds::PALE_GARDEN;
}

bool TreeBiomeTags::isPlains(int32_t biomeId) {
    return biomeId == BiomeIds::PLAINS || biomeId == BiomeIds::SUNFLOWER_PLAINS;
}

bool TreeBiomeTags::isRoofed(int32_t biomeId) {
    return biomeId == BiomeIds::ROOFED_FOREST;
}

bool TreeBiomeTags::isSavanna(int32_t biomeId) {
    return biomeId == BiomeIds::SAVANNA || biomeId == BiomeIds::SAVANNA_PLATEAU
           || biomeId == BiomeIds::SAVANNA_MUTATED;
}

bool TreeBiomeTags::isSwamp(int32_t biomeId) {
    return biomeId == BiomeIds::SWAMPLAND || biomeId == BiomeIds::MANGROVE_SWAMP;
}

bool TreeBiomeTags::isTaiga(int32_t biomeId) {
    return biomeId == BiomeIds::TAIGA || biomeId == BiomeIds::COLD_TAIGA
           || biomeId == BiomeIds::MEGA_TAIGA || biomeId == BiomeIds::REDWOOD_TAIGA_MUTATED;
}
