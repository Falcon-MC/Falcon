#include "World/Level/LevelGen/Feature/IFeature.h"

#include "World/Level/Block/BlockState.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"

#include <cstring>

namespace {
    const char *DIRT_BLOCKS[] = {
            "minecraft:dirt",
            "minecraft:grass_block",
            "minecraft:podzol",
            "minecraft:coarse_dirt",
            "minecraft:mycelium",
            "minecraft:rooted_dirt",
            "minecraft:moss_block",
            "minecraft:pale_moss_block",
            "minecraft:mud",
            "minecraft:muddy_mangrove_roots",
            "minecraft:dirt_with_roots",
            "minecraft:farmland",
            "minecraft:grass_path"
    };
}

const char *IFeature::identifier() const {
    return name();
}

int32_t IFeature::javaStringHash(const char *text) {
    int32_t hash = 0;
    for (const char *cursor = text; *cursor != '\0'; cursor++)
        hash = (int32_t) ((uint32_t) hash * 31u + (uint32_t) (unsigned char) *cursor);

    return hash;
}

int32_t IFeature::randomRange(IRandom &random, int32_t start, int32_t end) {
    return start + (random.nextInt() % (end + 1 - start));
}

bool IFeature::isSupportDirt(const BlockState &state) {
    for (const char *candidate: DIRT_BLOCKS) {
        if (state.mName == candidate)
            return true;
    }

    return false;
}

bool IFeature::isSupportGrass(const BlockState &state) {
    return state.mName == "minecraft:grass_block";
}

void IFeature::queueObject(const BlockManager &object) {
    if (mRoot != nullptr)
        mRoot->merge(object);
}
