#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

/**
 * The block rule lists the jigsaw pieces of the game are placed with, built in code (sub_14CCD0800) rather than read
 * from the packs. A list is applied to every block of a piece as it is placed (sub_14775E430): each rule whose tests
 * pass replaces the block, and the next rules test the replaced block, so the whole list always runs.
 */
class StructureProcessors {
public:
    enum class Test : uint8_t {
        AlwaysTrue,
        // the block has this name, whatever its states
        BlockMatch,
        // the block has this name and the random passes its probability (sub_1469B2D30)
        RandomBlockMatch,
    };

    struct Rule {
        Test mInput;
        const char *mInputBlock;
        float mProbability;
        // the block already in the world at the position; null always passes
        const char *mLocationBlock;
        // the default states of the output, with one state set when mOutputState is not null
        const char *mOutput;
        const char *mOutputState;
        int32_t mOutputValue;
    };

    // the rules of a named list, empty when the game has no such list
    static const std::vector<Rule> &get(const std::string &name);

    // the block a piece places once the rules ran on the block of its template; world is what stands at the position
    static BlockState apply(const std::vector<const Rule *> &rules, const BlockState &block, const BlockState &world,
                            std::mt19937 &random);
};
