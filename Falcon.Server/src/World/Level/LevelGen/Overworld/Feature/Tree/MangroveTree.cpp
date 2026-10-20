#include "World/Level/LevelGen/Overworld/Feature/Tree/MangroveTree.h"

#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/LevelGen/Feature/BlockManager.h"
#include "World/Level/LevelGen/Feature/IFeature.h"
#include "World/Level/LevelGen/Feature/Tree/BeeNestGenerator.h"
#include "World/Level/LevelGen/Random/XoroshiroRandom.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace {

    const int32_t ROOT_WIDTH_LIMIT = 8;
    const int32_t ROOT_LENGTH_LIMIT = 15;
    const float ROOT_RANDOM_SKEW_CHANCE = 0.2f;
    const float BRANCH_PER_LOG_PROBABILITY = 0.5f;
    const int32_t LEAF_RADIUS = 3;
    const int32_t LEAF_HEIGHT = 2;
    const int32_t LEAF_PLACEMENT_ATTEMPTS = 70;
    const float VINE_PROBABILITY = 0.125f;
    const float PROPAGULE_PROBABILITY = 0.14f;

    class JavaRandom {
    public:
        explicit JavaRandom(int64_t seed) {
            mSeed = (seed ^ 0x5DEECE66DLL) & ((1LL << 48) - 1);
        }

        int32_t nextInt(int32_t bound) {
            int32_t r = _next(31);
            const int32_t m = bound - 1;
            if ((bound & m) == 0) {
                r = (int32_t) (((int64_t) bound * (int64_t) r) >> 31);
            } else {
                for (int32_t u = r; u - (r = u % bound) + m < 0; u = _next(31)) {
                }
            }

            return r;
        }

    private:
        int32_t _next(int32_t bits) {
            mSeed = (mSeed * 0x5DEECE66DLL + 0xBLL) & ((1LL << 48) - 1);
            return (int32_t) ((uint64_t) mSeed >> (48 - bits));
        }

        int64_t mSeed;
    };

    bool canPlaceLogInto(const std::string &identifier) {
        return identifier == "minecraft:air"
               || identifier == "minecraft:water"
               || identifier == "minecraft:flowing_water"
               || identifier == "minecraft:mangrove_leaves"
               || identifier == "minecraft:vine"
               || identifier == "minecraft:mangrove_propagule";
    }

    bool canPlaceLeafInto(const std::string &identifier) {
        return identifier == "minecraft:air"
               || identifier == "minecraft:water"
               || identifier == "minecraft:flowing_water"
               || identifier == "minecraft:vine"
               || identifier == "minecraft:mangrove_propagule";
    }

    bool canPlaceRoot(const std::string &identifier) {
        return identifier == "minecraft:air"
               || identifier == "minecraft:water"
               || identifier == "minecraft:flowing_water"
               || identifier == "minecraft:mud"
               || identifier == "minecraft:mangrove_roots"
               || identifier == "minecraft:muddy_mangrove_roots"
               || identifier == "minecraft:mangrove_leaves"
               || identifier == "minecraft:mangrove_propagule"
               || identifier == "minecraft:vine";
    }

    int32_t getVineMeta(TreeBlockFace attachedTo) {
        switch (attachedTo) {
            case TreeBlockFace::SOUTH:
                return 1;
            case TreeBlockFace::WEST:
                return 2;
            case TreeBlockFace::NORTH:
                return 4;
            case TreeBlockFace::EAST:
                return 8;
            default:
                return 0;
        }
    }

    TreeBlockFace randomHorizontal(IRandom &random) {
        return TreeBlockFaces::HORIZONTALS[random.nextBoundedInt(3)];
    }

}

MangroveTree::MangroveTree() {
    XoroshiroRandom random((int64_t) std::chrono::steady_clock::now().time_since_epoch().count());
    mTall = random.nextFloat() > 0.15f;
}

MangroveTree::MangroveTree(bool tall) : mTall(tall) {
}

bool MangroveTree::generate(BlockManager &manager, IRandom &random, int32_t x, int32_t y, int32_t z) {
    const MangroveProperties properties = mTall
                                          ? MangroveProperties{4, 1, 9, 1, 6, 0, 1, 3, 7}
                                          : MangroveProperties{2, 1, 4, 1, 4, 0, 1, 1, 3};

    const int32_t trunkOffsetY = IFeature::randomRange(random, properties.mRootOffsetMin, properties.mRootOffsetMax);
    const Coordinate trunkOrigin{x, y + trunkOffsetY, z};
    const int32_t treeHeight = properties.mBaseHeight + random.nextInt(properties.mHeightRandA + 1)
                               + random.nextInt(properties.mHeightRandB + 1);

    if (!_placeRoots(manager, random, Coordinate{x, y, z}, trunkOrigin))
        return false;

    const std::vector<Coordinate> foliageAttachments = _placeTrunk(manager, random, treeHeight, trunkOrigin,
                                                                   properties);
    for (const Coordinate &attachment: foliageAttachments)
        _createRandomSpreadFoliage(manager, random, attachment);

    _placeLeafVines(manager, random, foliageAttachments);
    _placePropagules(manager, random, foliageAttachments);
    _placeBeeNest(manager, random, foliageAttachments);
    return true;
}

std::vector<MangroveTree::Coordinate> MangroveTree::_placeTrunk(BlockManager &manager, IRandom &random,
                                                                int32_t treeHeight, const Coordinate &origin,
                                                                const MangroveProperties &properties) {
    std::vector<Coordinate> attachments;

    for (int32_t heightPos = 0; heightPos < treeHeight; heightPos++) {
        const int32_t currentHeight = origin.mY + heightPos;
        const Coordinate logPos{origin.mX, currentHeight, origin.mZ};

        if (_placeLog(manager, logPos) && heightPos < treeHeight - 1
            && random.nextFloat() < BRANCH_PER_LOG_PROBABILITY) {
            const TreeBlockFace branchDir = randomHorizontal(random);
            const int32_t branchLen = IFeature::randomRange(random, properties.mExtraBranchLengthMin,
                                                            properties.mExtraBranchLengthMax);
            const int32_t branchPos = std::max(0, branchLen
                                                  - IFeature::randomRange(random, properties.mExtraBranchLengthMin,
                                                                          properties.mExtraBranchLengthMax) - 1);
            const int32_t branchSteps = IFeature::randomRange(random, properties.mExtraBranchStepsMin,
                                                              properties.mExtraBranchStepsMax);
            _placeBranch(manager, treeHeight, attachments, logPos, currentHeight, branchDir, branchPos, branchSteps);
        }

        if (heightPos == treeHeight - 1)
            attachments.push_back(Coordinate{origin.mX, currentHeight + 1, origin.mZ});
    }

    return attachments;
}

void MangroveTree::_placeBranch(BlockManager &manager, int32_t treeHeight, std::vector<Coordinate> &attachments,
                                const Coordinate &logPos, int32_t currentHeight, TreeBlockFace branchDir,
                                int32_t branchPos, int32_t branchSteps) {
    int32_t heightAlongBranch = currentHeight + branchPos;
    int32_t logX = logPos.mX;
    int32_t logZ = logPos.mZ;
    int32_t branchPlacementIndex = branchPos;

    while (branchPlacementIndex < treeHeight && branchSteps > 0) {
        if (branchPlacementIndex >= 1) {
            const int32_t placementHeight = currentHeight + branchPlacementIndex;
            logX += TreeBlockFaces::getXOffset(branchDir);
            logZ += TreeBlockFaces::getZOffset(branchDir);
            const Coordinate branchLogPos{logX, placementHeight, logZ};
            heightAlongBranch = placementHeight;
            if (_placeLog(manager, branchLogPos))
                heightAlongBranch = placementHeight + 1;

            attachments.push_back(branchLogPos);
        }

        branchPlacementIndex++;
        branchSteps--;
    }

    if (heightAlongBranch - currentHeight > 1) {
        const Coordinate foliagePos{logX, heightAlongBranch, logZ};
        attachments.push_back(foliagePos);
        attachments.push_back(Coordinate{foliagePos.mX, foliagePos.mY - 2, foliagePos.mZ});
    }
}

bool MangroveTree::_placeRoots(BlockManager &manager, IRandom &random, const Coordinate &origin,
                               const Coordinate &trunkOrigin) {
    std::vector<Coordinate> rootPositions;
    Coordinate columnPos = origin;

    while (columnPos.mY < trunkOrigin.mY) {
        if (!canPlaceRoot(manager.getBlockAt(columnPos.mX, columnPos.mY, columnPos.mZ).mName))
            return false;

        columnPos.mY++;
    }

    rootPositions.push_back(Coordinate{trunkOrigin.mX, trunkOrigin.mY - 1, trunkOrigin.mZ});

    for (const TreeBlockFace direction: TreeBlockFaces::HORIZONTAL_PLANE) {
        const Coordinate pos{
                trunkOrigin.mX + TreeBlockFaces::getXOffset(direction),
                trunkOrigin.mY + TreeBlockFaces::getYOffset(direction),
                trunkOrigin.mZ + TreeBlockFaces::getZOffset(direction)
        };

        std::vector<Coordinate> positionsInDirection;
        if (!_simulateRoots(manager, random, pos, direction, trunkOrigin, positionsInDirection, 0))
            return false;

        rootPositions.insert(rootPositions.end(), positionsInDirection.begin(), positionsInDirection.end());
        rootPositions.push_back(pos);
    }

    for (const Coordinate &rootPos: rootPositions)
        _placeRoot(manager, random, rootPos);

    return true;
}

bool MangroveTree::_simulateRoots(BlockManager &manager, IRandom &random, const Coordinate &rootPos,
                                  TreeBlockFace dir, const Coordinate &rootOrigin,
                                  std::vector<Coordinate> &rootPositions, int32_t layer) {
    if (layer == ROOT_LENGTH_LIMIT || (int32_t) rootPositions.size() > ROOT_LENGTH_LIMIT)
        return false;

    const std::vector<Coordinate> candidates = _potentialRootPositions(rootPos, dir, random, rootOrigin);
    for (const Coordinate &pos: candidates) {
        if (canPlaceRoot(manager.getBlockAt(pos.mX, pos.mY, pos.mZ).mName)) {
            rootPositions.push_back(pos);
            if (!_simulateRoots(manager, random, pos, dir, rootOrigin, rootPositions, layer + 1))
                return false;
        }
    }

    return true;
}

std::vector<MangroveTree::Coordinate> MangroveTree::_potentialRootPositions(const Coordinate &pos,
                                                                           TreeBlockFace previousDir,
                                                                           IRandom &random,
                                                                           const Coordinate &rootOrigin) {
    const Coordinate below{pos.mX, pos.mY - 1, pos.mZ};
    const Coordinate nextTo{
            pos.mX + TreeBlockFaces::getXOffset(previousDir),
            pos.mY + TreeBlockFaces::getYOffset(previousDir),
            pos.mZ + TreeBlockFaces::getZOffset(previousDir)
    };
    const Coordinate nextToDown{nextTo.mX, nextTo.mY - 1, nextTo.mZ};
    const int32_t width = std::abs(pos.mX - rootOrigin.mX) + std::abs(pos.mY - rootOrigin.mY)
                          + std::abs(pos.mZ - rootOrigin.mZ);

    if (width > ROOT_WIDTH_LIMIT - 3 && width <= ROOT_WIDTH_LIMIT) {
        if (random.nextFloat() < ROOT_RANDOM_SKEW_CHANCE)
            return std::vector<Coordinate>{below, nextToDown};

        return std::vector<Coordinate>{below};
    }

    if (width > ROOT_WIDTH_LIMIT)
        return std::vector<Coordinate>{below};

    if (random.nextFloat() < ROOT_RANDOM_SKEW_CHANCE)
        return std::vector<Coordinate>{below};

    return random.nextBoolean() ? std::vector<Coordinate>{nextTo} : std::vector<Coordinate>{below};
}

void MangroveTree::_placeRoot(BlockManager &manager, IRandom &random, const Coordinate &pos) {
    const std::string previous = manager.getBlockAt(pos.mX, pos.mY, pos.mZ).mName;
    const BlockState state = (previous == "minecraft:mud" || previous == "minecraft:muddy_mangrove_roots")
                             ? withPillarAxis(VanillaBlocks::MUDDY_MANGROVE_ROOTS().toBlockState(), "y")
                             : VanillaBlocks::MANGROVE_ROOTS().toBlockState();

    manager.setBlockStateAt(pos.mX, pos.mY, pos.mZ, state);

    if (random.nextFloat() < 0.5f) {
        if (manager.getBlockAt(pos.mX, pos.mY + 1, pos.mZ).mName == "minecraft:air")
            manager.setBlockStateAt(pos.mX, pos.mY + 1, pos.mZ, VanillaBlocks::MOSS_CARPET().toBlockState());
    }
}

void MangroveTree::_createRandomSpreadFoliage(BlockManager &manager, IRandom &random, const Coordinate &origin) {
    for (int32_t i = 0; i < LEAF_PLACEMENT_ATTEMPTS; i++) {
        const int32_t leafX = origin.mX + random.nextInt(LEAF_RADIUS) - random.nextInt(LEAF_RADIUS);
        const int32_t leafY = origin.mY + random.nextInt(LEAF_HEIGHT) - random.nextInt(LEAF_HEIGHT);
        const int32_t leafZ = origin.mZ + random.nextInt(LEAF_RADIUS) - random.nextInt(LEAF_RADIUS);
        _placeLeaf(manager, Coordinate{leafX, leafY, leafZ});
    }
}

void MangroveTree::_placeLeafVines(BlockManager &manager, IRandom &random,
                                   const std::vector<Coordinate> &foliageAttachments) {
    const std::vector<Coordinate> leaves = _collectPlacedLeaves(manager, foliageAttachments);

    for (const Coordinate &leafPos: leaves) {
        _maybePlaceVine(manager, random, Coordinate{leafPos.mX - 1, leafPos.mY, leafPos.mZ}, TreeBlockFace::EAST);
        _maybePlaceVine(manager, random, Coordinate{leafPos.mX + 1, leafPos.mY, leafPos.mZ}, TreeBlockFace::WEST);
        _maybePlaceVine(manager, random, Coordinate{leafPos.mX, leafPos.mY, leafPos.mZ - 1}, TreeBlockFace::SOUTH);
        _maybePlaceVine(manager, random, Coordinate{leafPos.mX, leafPos.mY, leafPos.mZ + 1}, TreeBlockFace::NORTH);
    }
}

void MangroveTree::_maybePlaceVine(BlockManager &manager, IRandom &random, const Coordinate &pos,
                                   TreeBlockFace attachedTo) {
    if (random.nextFloat() < VINE_PROBABILITY && manager.getBlockAt(pos.mX, pos.mY, pos.mZ).mName == "minecraft:air")
        _addHangingVine(manager, pos, attachedTo);
}

void MangroveTree::_addHangingVine(BlockManager &manager, const Coordinate &pos, TreeBlockFace attachedTo) {
    _placeVine(manager, pos, attachedTo);
    Coordinate vinePos{pos.mX, pos.mY - 1, pos.mZ};
    int32_t maxLength = 4;

    while (manager.getBlockAt(vinePos.mX, vinePos.mY, vinePos.mZ).mName == "minecraft:air" && maxLength > 0) {
        _placeVine(manager, vinePos, attachedTo);
        vinePos.mY--;
        maxLength--;
    }
}

void MangroveTree::_placePropagules(BlockManager &manager, IRandom &random,
                                    const std::vector<Coordinate> &foliageAttachments) {
    std::vector<Coordinate> blacklist;
    std::vector<Coordinate> leaves = _collectPlacedLeaves(manager, foliageAttachments);

    JavaRandom shuffleRandom(random.nextLong());
    for (size_t i = leaves.size(); i > 1; i--) {
        const int32_t j = shuffleRandom.nextInt((int32_t) i);
        std::swap(leaves[i - 1], leaves[(size_t) j]);
    }

    for (const Coordinate &leafPos: leaves) {
        const Coordinate placementPos{leafPos.mX, leafPos.mY - 1, leafPos.mZ};
        const bool blacklisted = std::find(blacklist.begin(), blacklist.end(), placementPos) != blacklist.end();

        if (!blacklisted && random.nextFloat() < PROPAGULE_PROBABILITY
            && _hasRequiredEmptyBlocks(manager, leafPos, TreeBlockFace::DOWN, 2)) {
            for (int32_t bx = placementPos.mX - 1; bx <= placementPos.mX + 1; bx++) {
                for (int32_t bz = placementPos.mZ - 1; bz <= placementPos.mZ + 1; bz++)
                    blacklist.push_back(Coordinate{bx, placementPos.mY, bz});
            }

            BlockState propagule = VanillaBlocks::MANGROVE_PROPAGULE().toBlockState();
            propagule.mStates.putByte("hanging", 1);
            propagule.mStates.putInt("propagule_stage", IFeature::randomRange(random, 0, 4));
            manager.setBlockStateAt(placementPos.mX, placementPos.mY, placementPos.mZ,
                                    BlockState(propagule.mName, propagule.mStates));
        }
    }
}

bool MangroveTree::_hasRequiredEmptyBlocks(BlockManager &manager, const Coordinate &leafPos, TreeBlockFace direction,
                                           int32_t count) {
    for (int32_t i = 1; i <= count; i++) {
        const int32_t checkX = leafPos.mX + TreeBlockFaces::getXOffset(direction) * i;
        const int32_t checkY = leafPos.mY + TreeBlockFaces::getYOffset(direction) * i;
        const int32_t checkZ = leafPos.mZ + TreeBlockFaces::getZOffset(direction) * i;
        if (manager.getBlockAt(checkX, checkY, checkZ).mName != "minecraft:air")
            return false;
    }

    return true;
}

void MangroveTree::_placeBeeNest(BlockManager &manager, IRandom &random,
                                 const std::vector<Coordinate> &foliageAttachments) {
    if (!mWithBeeNest || foliageAttachments.empty())
        return;

    const Coordinate &attachment = foliageAttachments[(size_t) random.nextInt((int32_t) foliageAttachments.size())];
    const Coordinate nestPos{attachment.mX, attachment.mY - 1, attachment.mZ + 1};

    if (manager.getBlockAt(nestPos.mX, nestPos.mY, nestPos.mZ).mName != "minecraft:air"
        || manager.getBlockAt(nestPos.mX, nestPos.mY + 1, nestPos.mZ).mName == "minecraft:air")
        return;

    BeeNestGenerator::placeAt(manager, nestPos.mX, nestPos.mY, nestPos.mZ, mBeeCount);
}

std::vector<MangroveTree::Coordinate> MangroveTree::_collectPlacedLeaves(
        BlockManager &manager, const std::vector<Coordinate> &foliageAttachments) {
    std::vector<Coordinate> leaves;

    for (const Coordinate &origin: foliageAttachments) {
        for (int32_t x = origin.mX - LEAF_RADIUS + 1; x <= origin.mX + LEAF_RADIUS - 1; x++) {
            for (int32_t y = origin.mY - LEAF_HEIGHT + 1; y <= origin.mY + LEAF_HEIGHT - 1; y++) {
                for (int32_t z = origin.mZ - LEAF_RADIUS + 1; z <= origin.mZ + LEAF_RADIUS - 1; z++) {
                    if (manager.getBlockAt(x, y, z).mName != "minecraft:mangrove_leaves")
                        continue;

                    const Coordinate pos{x, y, z};
                    if (std::find(leaves.begin(), leaves.end(), pos) == leaves.end())
                        leaves.push_back(pos);
                }
            }
        }
    }

    return leaves;
}

bool MangroveTree::_placeLog(BlockManager &manager, const Coordinate &pos) {
    if (!canPlaceLogInto(manager.getBlockAt(pos.mX, pos.mY, pos.mZ).mName))
        return false;

    manager.setBlockStateAt(pos.mX, pos.mY, pos.mZ,
                            withPillarAxis(VanillaBlocks::MANGROVE_LOG().toBlockState(), "y"));
    return true;
}

void MangroveTree::_placeLeaf(BlockManager &manager, const Coordinate &pos) {
    if (canPlaceLeafInto(manager.getBlockAt(pos.mX, pos.mY, pos.mZ).mName))
        manager.setBlockStateAt(pos.mX, pos.mY, pos.mZ, VanillaBlocks::MANGROVE_LEAVES().toBlockState());
}

void MangroveTree::_placeVine(BlockManager &manager, const Coordinate &pos, TreeBlockFace attachedTo) {
    manager.setBlockStateAt(pos.mX, pos.mY, pos.mZ, getVineState(getVineMeta(attachedTo)));
}
