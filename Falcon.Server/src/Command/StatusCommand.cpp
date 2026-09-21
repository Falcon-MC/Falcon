#include "Command/StatusCommand.h"

#include "Actor/ServerPlayer.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <algorithm>
#include <cstdio>

namespace {
    const double TICKS_PER_SECOND = 20.0;

    std::string formatDecimal(double value) {
        char buffer[32];
        std::snprintf(buffer, sizeof(buffer), "%.2f", value);
        return buffer;
    }
}

StatusCommand::StatusCommand(ServerNetworkHandler &handler)
        : Command("status", "Shows the server performance", "/status"), mHandler(handler) {}

bool StatusCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    (void) arguments;

    const double millisecondsPerTick = mHandler.getMillisecondsPerTick();
    const double ticksPerSecond = millisecondsPerTick <= 0.0
                                  ? TICKS_PER_SECOND
                                  : std::min(TICKS_PER_SECOND, 1000.0 / millisecondsPerTick);

    int online = 0;
    for (const auto &entry: mHandler.getPlayers()) {
        if (entry.second.isSpawned())
            ++online;
    }

    sender.sendLocalized("falcon.commands.status.performance",
                         {formatDecimal(ticksPerSecond), formatDecimal(millisecondsPerTick),
                          formatDecimal(mHandler.getPeakMillisecondsPerTick())});
    sender.sendLocalized("falcon.commands.status.players",
                         {std::to_string(online), std::to_string(mHandler.getMaxPlayers())});
    return true;
}
