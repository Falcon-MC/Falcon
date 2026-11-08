#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Core/Math/Vector3f.h"
#include "Network/Handler/ChunkStreamHandler.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <utility>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the chunk offsets the game copies into each level (sub_1430054F0, 57 entries)
    const std::pair<int32_t, int32_t> GAME_OFFSETS[] = {
            {-1, -4}, {0, -4}, {1, -4},
            {-2, -3}, {-1, -3}, {0, -3}, {1, -3}, {2, -3},
            {-3, -2}, {-2, -2}, {-1, -2}, {0, -2}, {1, -2}, {2, -2}, {3, -2},
            {-4, -1}, {-3, -1}, {-2, -1}, {-1, -1}, {0, -1}, {1, -1}, {2, -1}, {3, -1}, {4, -1},
            {-4, 0}, {-3, 0}, {-2, 0}, {-1, 0}, {0, 0}, {1, 0}, {2, 0}, {3, 0}, {4, 0},
            {-4, 1}, {-3, 1}, {-2, 1}, {-1, 1}, {0, 1}, {1, 1}, {2, 1}, {3, 1}, {4, 1},
            {-3, 2}, {-2, 2}, {-1, 2}, {0, 2}, {1, 2}, {2, 2}, {3, 2},
            {-2, 3}, {-1, 3}, {0, 3}, {1, 3}, {2, 3},
            {-1, 4}, {0, 4}, {1, 4},
    };

    Vector3f inChunk(int32_t chunkX, int32_t chunkZ) {
        return Vector3f((float) chunkX * 16.0f + 8.0f, 64.0f, (float) chunkZ * 16.0f + 8.0f);
    }

    void gameOffsets() {
        const std::set<std::pair<int32_t, int32_t>> expected(std::begin(GAME_OFFSETS), std::end(GAME_OFFSETS));
        require(expected.size() == 57, "the game table holds 57 distinct chunks");

        for (const auto &centre: {std::make_pair(0, 0), std::make_pair(-3, 7), std::make_pair(1000, -1000)}) {
            for (int32_t dx = -8; dx <= 8; ++dx) {
                for (int32_t dz = -8; dz <= 8; ++dz) {
                    const bool seen = ChunkStreamHandler::isInActorRange(
                            inChunk(centre.first, centre.second),
                            inChunk(centre.first + dx, centre.second + dz));
                    require(seen == (expected.count(std::make_pair(dx, dz)) != 0),
                            "an actor is sent exactly in the chunks of the game table");
                }
            }
        }
    }

    void viewDistanceIgnored() {
        // a mob 6 chunks away stays hidden however far the chunks stream
        require(!ChunkStreamHandler::isInActorRange(inChunk(0, 0), inChunk(6, 0)),
                "six chunks out is past the actor range");
        require(!ChunkStreamHandler::isInActorRange(inChunk(0, 0), inChunk(3, 3)),
                "the corner at 3, 3 is past the actor range");
        require(ChunkStreamHandler::isInActorRange(inChunk(0, 0), inChunk(4, 1)),
                "4, 1 is inside the actor range");
    }

    void negativeCoordinates() {
        // chunks come from floored positions: -0.5 is chunk -1, not 0
        require(ChunkStreamHandler::isInActorRange(Vector3f(-0.5f, 64.0f, 0.0f), Vector3f(-64.5f, 64.0f, 0.0f)),
                "-0.5 and -64.5 are four chunks apart");
        require(!ChunkStreamHandler::isInActorRange(Vector3f(0.5f, 64.0f, 0.0f), Vector3f(-64.5f, 64.0f, 0.0f)),
                "0.5 and -64.5 are five chunks apart");
        require(ChunkStreamHandler::isInActorRange(Vector3f(15.9f, 64.0f, -0.1f), Vector3f(-48.0f, 64.0f, -16.0f)),
                "the actor chunk is taken from its floored position");
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));

    int failures = 0;
    const auto check = [&failures](const char *name, const std::function<void()> &run) {
        try {
            run();
            LOG_INFO(LogAreaID::Server, "PASS: %s", name);
        } catch (const std::exception &error) {
            ++failures;
            LOG_ERROR(LogAreaID::Server, "FAIL: %s: %s", name, error.what());
        }
    };

    check("actor range matches the game table", gameOffsets);
    check("actor range ignores the view distance", viewDistanceIgnored);
    check("actor range floors negative positions", negativeCoordinates);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
