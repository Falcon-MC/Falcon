#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/BlockPaletteRegistry.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Sculk/SculkSpreader.h"
#include "World/Level/Sculk/SculkWorld.h"

#include <array>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <map>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <tuple>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // one block everywhere, with the blocks set over it
    class FilledWorld : public SculkWorld {
    public:
        explicit FilledWorld(const BlockState &fill) : mFill(fill) {}

        const BlockState &getBlock(const Vector3i &position) override {
            const auto found = mBlocks.find(std::make_tuple(position.x, position.y, position.z));
            return found != mBlocks.end() ? found->second : mFill;
        }

        void setBlock(const Vector3i &position, const BlockState &state) override {
            mBlocks[std::make_tuple(position.x, position.y, position.z)] = state;
        }

        bool isWaterSource(const Vector3i &) override {
            return false;
        }

    private:
        BlockState mFill;
        std::map<std::tuple<int32_t, int32_t, int32_t>, BlockState> mBlocks;
    };

    // the game's shuffle: each slot from the second on swaps with one of the slots up to it
    template <typename T, size_t N>
    std::array<T, N> shuffled(std::array<T, N> values, std::mt19937 &random) {
        for (size_t index = 1; index < N; ++index) {
            const size_t swap = (size_t) (random() % (uint32_t) (index + 1));
            if (swap != index)
                std::swap(values[index], values[swap]);
        }
        return values;
    }

    const std::array<Vector3i, 18> NON_CORNER_NEIGHBOURS = {
            Vector3i(-1, -1, 0), Vector3i(0, -1, -1), Vector3i(0, -1, 0), Vector3i(0, -1, 1), Vector3i(1, -1, 0),
            Vector3i(-1, 0, -1), Vector3i(-1, 0, 0),  Vector3i(-1, 0, 1), Vector3i(0, 0, -1), Vector3i(0, 0, 1),
            Vector3i(1, 0, -1),  Vector3i(1, 0, 0),   Vector3i(1, 0, 1),  Vector3i(-1, 1, 0), Vector3i(0, 1, -1),
            Vector3i(0, 1, 0),   Vector3i(0, 1, 1),   Vector3i(1, 1, 0)};

    bool sideBySide(const Vector3i &offset) {
        return std::abs(offset.x) + std::abs(offset.y) + std::abs(offset.z) == 1;
    }

    // inside solid sculk only the 6 sides are open; the cursor goes to the last of them in the shuffled order,
    // not the first
    void cursorTakesTheLastSculk() {
        const uint32_t seed = 7;
        // one draw keeps the charge, then the 17 of the shuffle
        std::mt19937 replay(seed);
        replay();
        const std::array<Vector3i, 18> order = shuffled(NON_CORNER_NEIGHBOURS, replay);
        Vector3i first;
        Vector3i last;
        bool found = false;
        for (const Vector3i &offset: order) {
            if (!sideBySide(offset))
                continue;
            if (!found)
                first = offset;
            last = offset;
            found = true;
        }

        FilledWorld world(VanillaBlocks::SCULK().toBlockState());
        SculkSpreader spreader({false, 10, 4, 1000000, 5});
        spreader.addCursors(Vector3i(0, 0, 0), 10);
        std::mt19937 random(seed);
        spreader.updateCursors(world, Vector3i(0, 0, 0), random, false);

        const Vector3i moved = spreader.getCursors().at(0).mPosition;
        require(moved.x == last.x && moved.y == last.y && moved.z == last.z,
                "the cursor went to " + std::to_string(moved.x) + " " + std::to_string(moved.y) + " "
                        + std::to_string(moved.z) + " instead of the last open side");
        require(!(first.x == last.x && first.y == last.y && first.z == last.z), "the seed tells nothing apart");
    }

    // a vein on all its faces in stone turns the block of the first face of its shuffle into sculk; the game
    // shuffles from up, down, north, south, west, east
    void veinShufflesItsFacesFromUp() {
        const std::array<Vector3i, 6> steps = {Vector3i(0, -1, 0), Vector3i(0, 1, 0),  Vector3i(0, 0, -1),
                                               Vector3i(0, 0, 1),  Vector3i(-1, 0, 0), Vector3i(1, 0, 0)};
        // the first seed where both starting orders pick another face
        uint32_t seed = 1;
        int32_t face = 0;
        for (;; seed++) {
            std::mt19937 game(seed);
            std::mt19937 plain(seed);
            face = shuffled(std::array<int32_t, 6>{1, 0, 2, 3, 4, 5}, game)[0];
            if (face != shuffled(std::array<int32_t, 6>{0, 1, 2, 3, 4, 5}, plain)[0])
                break;
        }

        FilledWorld world(VanillaBlocks::STONE().toBlockState());
        const BlockState vein = VanillaBlocks::SCULK_VEIN().toBlockState();
        Tag states = vein.mStates;
        states.putInt("multi_face_direction_bits", 63);
        world.setBlock(Vector3i(0, 0, 0), BlockState(vein.mName, states));

        SculkSpreader spreader;
        spreader.addCursors(Vector3i(0, 0, 0), 10);
        std::mt19937 random(seed);
        spreader.updateCursors(world, Vector3i(0, 0, 0), random, true);

        for (int32_t other = 0; other < 6; other++) {
            const bool sculk = world.getBlock(steps[(size_t) other]).mName == BlockNames::SCULK;
            require(sculk == (other == face),
                    "face " + std::to_string(other) + (sculk ? " turned to sculk" : " did not turn to sculk"));
        }
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));
    BlockPaletteRegistry::getInstance().initialize();
    try {
        cursorTakesTheLastSculk();
        veinShufflesItsFacesFromUp();
    } catch (const std::exception &exception) {
        std::fprintf(stderr, "FAILED: %s\n", exception.what());
        return 1;
    }
    return 0;
}
