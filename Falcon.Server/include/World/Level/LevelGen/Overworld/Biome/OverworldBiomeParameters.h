#pragma once

#include <array>
#include <cstdint>
#include <memory>
#include <vector>

// climate quantized like the game: (int64) (value * 10000.0) in double (sub_147186B10); a biome is the entry at the
// smallest squared distance to its parameter ranges (sub_147186B70)
class OverworldBiomeParameters {
public:
    static constexpr int32_t DIMENSIONS = 7;

    struct Parameter {
        int64_t mMin;
        int64_t mMax;
    };

    using Space = std::array<Parameter, DIMENSIONS>;

    // temperature, humidity, continentalness, erosion, depth, weirdness, then 0 for the offset
    using Target = std::array<int64_t, DIMENSIONS>;

    struct Node {
        Space mSpace;
        int32_t mBiome = -1;
        std::vector<std::unique_ptr<Node>> mChildren;

        bool isLeaf() const {
            return mChildren.empty();
        }
    };

    OverworldBiomeParameters();

    // nearest leaf, starting from the previous result of the same column like the game (sub_147188720)
    const Node *search(const Target &target, const Node *lastResult) const;

    static int64_t quantize(float value);

    static Target target(float temperature, float humidity, float continentalness, float erosion, float depth,
                         float weirdness);

private:
    void _addEntry(const Space &space, int32_t biome);

    std::vector<std::unique_ptr<Node>> mLeaves;
    std::unique_ptr<Node> mRoot;
};
