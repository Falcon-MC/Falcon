#pragma once

#include "Server/Commands/Command.h"
#include "Core/Math/Vector3i.h"

class Level;
class ServerNetworkHandler;

class LocateCommand : public Command {
public:
    static const int32_t SEARCH_SPIRAL = 0;
    static const int32_t SEARCH_X_AXIS = 1;
    static const int32_t DEFAULT_BIOME_RADIUS = 1000;

    explicit LocateCommand(ServerNetworkHandler &handler);

    bool execute(CommandOrigin &sender, const std::vector<std::string> &arguments) override;

    CommandPermission getRequiredPermission() const override;

    std::vector<CommandOverloadData> getOverloads() const override;

private:
    bool _locateBiome(CommandOrigin &sender, const std::vector<std::string> &arguments);

    static bool _findBiomeSpiral(Level &level, int32_t centerX, int32_t centerZ, int32_t biomeId,
                                 int32_t maxRadius, Vector3i &found);

    static bool _findBiomeXAxis(Level &level, int32_t centerX, int32_t centerZ, int32_t biomeId,
                                int32_t maxRadius, Vector3i &found);

    ServerNetworkHandler &mHandler;
};
