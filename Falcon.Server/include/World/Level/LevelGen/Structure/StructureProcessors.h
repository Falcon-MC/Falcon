#pragma once

#include "World/Level/Block/BlockState.h"

#include <cstdint>
#include <random>
#include <string>
#include <vector>

// rule lists of the jigsaw pieces, built in code; every rule runs on what the earlier ones left
class StructureProcessors {
public:
    enum class Test : uint8_t {
        AlwaysTrue,
        BlockMatch,
        RandomBlockMatch,
    };

    struct Rule {
        Test mInput;
        const char *mInputBlock;
        float mProbability;
        // null: any world block
        const char *mLocationBlock;
        // default states, plus mOutputState when set
        const char *mOutput;
        const char *mOutputState;
        int32_t mOutputValue;
    };

    static const std::vector<Rule> &get(const std::string &name);

    static BlockState apply(const std::vector<const Rule *> &rules, const BlockState &block, const BlockState &world,
                            std::mt19937 &random);
};
