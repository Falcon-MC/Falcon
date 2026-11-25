#pragma once

#include "Server/Commands/Command.h"

#include <cstdint>
#include <functional>
#include <vector>

class Level;
class ServerNetworkHandler;

// scatters the targets over a square around a centre, at least a distance apart, each on safe ground
class SpreadPlayersCommand : public Command {
public:
    // the game gives up after this many rounds of pushing apart and checking the ground
    static constexpr int32_t MAX_ROUNDS = 10000;

    struct Point {
        float mX;
        float mZ;
    };

    // a number in [0, 1); the game's own random unless a test sets one
    using Random = std::function<float()>;

    explicit SpreadPlayersCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    std::vector<CommandOverloadData> getOverloads() const override;

    void setRandom(Random random) {
        mRandom = std::move(random);
    }

    // false with no safe ground under it; the column's chunk is loaded or generated first
    static bool isSafe(Level &level, int32_t x, int32_t z, int32_t top);

    // where a target lands: on the block the column's search stops above, plus one
    static float landingY(Level &level, int32_t x, int32_t z, int32_t top);

private:
    ServerNetworkHandler &mHandler;
    Random mRandom;
};
