#include "World/Level/Block/ChorusFlowerBlock.h"

#include <algorithm>

#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/PlantGrowthHelpers.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Block/Systems/BlockChangeSystem.h"
#include "World/Level/Block/Systems/RandomTickSystem.h"
#include "World/Level/LevelGen/Overworld/Feature/Decoration/DecorationSupport.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(ChorusFlowerBlock, 321);

using namespace PlantGrowthHelpers;

namespace {
    const int32_t MAX_CHORUS_AGE = 5;
    const int32_t CHORUS_BRANCH_AGE = 4;
    const int32_t NO_FACE = -1;
}

bool ChorusFlowerBlock::matches(const std::string &identifier) {
    return identifier == "minecraft:chorus_flower";
}

void ChorusFlowerBlock::onRandomTick(ServerNetworkHandler &owner, Level &level, const Vector3i &position,
                                     const BlockState &state) const {
    (void) owner;

    const Vector3i top = above(position);
    if (!isInRange(level, top) || !isAirAt(level, top))
        return;

    const int32_t age = state.mStates.getInt(AGE, 0);
    if (age >= MAX_CHORUS_AGE)
        return;

    bool growUp = false;
    bool grounded = false;
    const BlockState under = stateAt(level, below(position));

    if (under.mName == "minecraft:end_stone") {
        growUp = true;
    } else if (under.mName == "minecraft:chorus_plant") {
        int32_t height = 1;
        for (int32_t step = 0; step < 4; ++step) {
            const BlockState stem = stateAt(level, Vector3i(position.x, position.y - height - 1, position.z));
            if (stem.mName == "minecraft:chorus_plant") {
                ++height;
                continue;
            }
            if (stem.mName == "minecraft:end_stone")
                grounded = true;
            break;
        }

        if (height < 2 || height <= RandomTickSystem::nextInt(grounded ? 5 : 4))
            growUp = true;
    } else if (DecorationSupport::isAir(under)) {
        growUp = true;
    }

    const auto flowerOfAge = [](int32_t age) {
        return DecorationSupport::withState(VanillaBlocks::CHORUS_FLOWER().toBlockState(), AGE, age);
    };
    const BlockState stem = VanillaBlocks::CHORUS_PLANT().toBlockState();
    const Vector3i twoAbove = above(top);
    if (growUp && isInRange(level, twoAbove) && isAirAt(level, twoAbove) && allNeighboursEmpty(level, top, NO_FACE)) {
        const BlockState flower = flowerOfAge(age);
        if (!BlockChangeSystem::allows(level, top, flower, BlockChangeCause::Grow))
            return;
        BlockChangeSystem::StructureBlocks blocks{{top, flower}, {position, stem}};
        if (BlockChangeSystem::prepareStructure(level, position, blocks))
            BlockChangeSystem::applyStructure(level, blocks, true);
        return;
    }

    if (age < CHORUS_BRANCH_AGE) {
        int32_t attempts = RandomTickSystem::nextInt(4);
        if (grounded)
            ++attempts;

        const BlockState flower = flowerOfAge(age + 1);
        BlockChangeSystem::StructureBlocks blocks;
        for (int32_t attempt = 0; attempt < attempts; ++attempt) {
            const int32_t face = HORIZONTAL_FACES[RandomTickSystem::nextInt(4)];
            const Vector3i branch = side(position, face);
            if (isAirAt(level, branch) && isAirAt(level, below(branch))
                && allNeighboursEmpty(level, branch, opposite(face))
                && std::none_of(blocks.begin(), blocks.end(), [&branch](const auto &entry) { return entry.first == branch; })
                && BlockChangeSystem::allows(level, branch, flower, BlockChangeCause::Grow)) {
                blocks.emplace_back(branch, flower);
            }
        }

        if (!blocks.empty()) {
            blocks.emplace_back(position, stem);
            if (BlockChangeSystem::prepareStructure(level, position, blocks))
                BlockChangeSystem::applyStructure(level, blocks, true);
            return;
        }
    }

    level.setBlock(position, DecorationSupport::withState(state, AGE, MAX_CHORUS_AGE), true);
}

bool ChorusFlowerBlock::allNeighboursEmpty(Level &level, const Vector3i &position, int32_t exceptFace) {
    for (int32_t face: HORIZONTAL_FACES) {
        if (face != exceptFace && !isAirAt(level, side(position, face)))
            return false;
    }
    return true;
}
