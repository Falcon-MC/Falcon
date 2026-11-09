#include "Core/Json/Json.h"
#include "World/Level/Block/BlockPlacementFilter.h"
#include "World/Level/Block/Components/PlacementOrientation.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"

#include <iostream>
#include <memory>
#include <stdexcept>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    BlockState block(const char *name) {
        const Block *value = VanillaBlocks::fromIdentifier(name);
        require(value != nullptr, "missing block registration");
        return value->toBlockState();
    }

    BlockPlacementFilter filterOf(const char *text) {
        const std::unique_ptr<json::Value> component = json::parse(text);
        require(component != nullptr, "filter json did not parse");
        return BlockPlacementFilter::parse(*component, "test:plant");
    }

    void placement() {
        using namespace PlacementOrientation;
        const BlockPlacementFilter filter = filterOf(R"json({"conditions": [{
            "allowed_faces": ["up"],
            "block_filter": ["grass_block", {"tags": "q.any_tag('sand')"},
                             {"name": "minecraft:oak_log", "states": {"pillar_axis": "y"}}]}]})json");
        Level level("placement-filter-test", 2);
        level.insertChunk(LevelChunk(0, 0));
        const Vector3i position(8, 20, 8);
        const Vector3i below(8, 19, 8);

        level.setBlock(below, block("minecraft:grass_block"), false);
        require(filter.allowsPlacement(level, position, FACE_UP), "grass below refused");
        require(!filter.allowsPlacement(level, position, FACE_NORTH), "a side face was allowed");
        require(filter.survives(level, position), "did not survive on grass");

        level.setBlock(below, block("minecraft:stone"), false);
        require(!filter.allowsPlacement(level, position, FACE_UP), "stone below allowed");
        require(!filter.survives(level, position), "survived on stone");

        level.setBlock(below, block("minecraft:sand"), false);
        require(filter.allowsPlacement(level, position, FACE_UP), "the sand tag did not match");

        BlockState log = block("minecraft:oak_log");
        log.mStates.putString("pillar_axis", "y");
        level.setBlock(below, log, false);
        require(filter.allowsPlacement(level, position, FACE_UP), "an upright log was refused");
        log.mStates.putString("pillar_axis", "x");
        level.setBlock(below, log, false);
        require(!filter.allowsPlacement(level, position, FACE_UP), "a lying log matched the upright state");

        level.setBlock(below, block("minecraft:air"), false);
        require(!filter.survives(level, position), "survived over air");
    }

    void unparsedTags() {
        using namespace PlacementOrientation;
        const BlockPlacementFilter filter = filterOf(R"json({"conditions": [{
            "allowed_faces": ["all"], "block_filter": [{"tags": "q.is_cold"}]}]})json");
        Level level("placement-filter-test", 2);
        level.insertChunk(LevelChunk(0, 0));
        level.setBlock(Vector3i(8, 19, 8), block("minecraft:stone"), false);
        require(!filter.allowsPlacement(level, Vector3i(8, 20, 8), FACE_UP),
                "a filter left empty by an unknown query held any block");
    }

    void noFilter() {
        using namespace PlacementOrientation;
        const BlockPlacementFilter filter = filterOf(R"json({"conditions": [{"allowed_faces": ["side"]}]})json");
        Level level("placement-filter-test", 2);
        level.insertChunk(LevelChunk(0, 0));
        level.setBlock(Vector3i(7, 20, 8), block("minecraft:stone"), false);
        require(filter.allowsPlacement(level, Vector3i(8, 20, 8), FACE_EAST), "any block on a side was refused");
        require(!filter.allowsPlacement(level, Vector3i(8, 20, 8), FACE_UP), "the top face was allowed");
        require(filter.survives(level, Vector3i(8, 20, 8)), "did not survive against its side block");
    }
}

int main() {
    try {
        placement();
        unparsedTags();
        noFilter();
    } catch (const std::exception &error) {
        std::cerr << "placement filter test failed: " << error.what() << std::endl;
        return 1;
    }
    std::cout << "placement filter tests passed" << std::endl;
    return 0;
}
