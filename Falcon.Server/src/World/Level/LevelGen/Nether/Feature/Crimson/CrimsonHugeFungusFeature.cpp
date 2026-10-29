#include "World/Level/LevelGen/Nether/Feature/Crimson/CrimsonHugeFungusFeature.h"

#include "World/Level/LevelGen/Nether/NetherBiomeIds.h"
#include "World/Level/LevelGen/Nether/Tree/LegacyCrimsonTree.h"

int32_t CrimsonHugeFungusFeature::getBiomeId() const {
    return NetherBiomeIds::CRIMSON_FOREST;
}

const char *CrimsonHugeFungusFeature::getNyliumIdentifier() const {
    return "minecraft:crimson_nylium";
}

std::shared_ptr<LegacyNetherTree> CrimsonHugeFungusFeature::createTree(int32_t treeHeight) const {
    return std::make_shared<LegacyCrimsonTree>(treeHeight);
}

const char *CrimsonHugeFungusFeature::name() const {
    return "nether_crimson_fungi_tree";
}
