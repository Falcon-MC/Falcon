#pragma once

#include "World/Actor/Spawn/SpawnRules.h"
#include "Core/Math/Vector3i.h"

#include <string>
#include <vector>

class Level;

// the mobs a structure spawns inside its pieces in place of the biome's ones, for one population
namespace StructureSpawnOverrides {
    // the override of the structure piece holding the block, nullptr when none replaces that population
    const std::vector<SpawnRule> *find(Level &level, const Vector3i &position, const std::string &population);

    // the rules a structure spawns from, nullptr for a structure without an override of that population
    const std::vector<SpawnRule> *forStructure(const std::string &structure, const std::string &population);
}
