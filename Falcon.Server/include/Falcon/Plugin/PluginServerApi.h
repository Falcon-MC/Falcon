#pragma once

#include <falcon/falcon_api.h>

class PluginServerApi {
public:
    static const FalconServerApi &get();

    static void fillCore(FalconServerApi &api);

    static void fillEvents(FalconServerApi &api);

    static void fillEntities(FalconServerApi &api);

    static void fillPlayers(FalconServerApi &api);

    static void fillWorld(FalconServerApi &api);

    static void fillItems(FalconServerApi &api);

    static void fillPermissions(FalconServerApi &api);

    static void fillPackets(FalconServerApi &api);

    static void fillContent(FalconServerApi &api);

    static void fillServices(FalconServerApi &api);

    static void fillMaps(FalconServerApi &api);

    static void fillEffects(FalconServerApi &api);

    static void fillScoreboard(FalconServerApi &api);

    static void fillContainers(FalconServerApi &api);

    static void fillData(FalconServerApi &api);

    static void fillCommands(FalconServerApi &api);

    static void fillWorldQueries(FalconServerApi &api);

    static void fillActors(FalconServerApi &api);

    static void fillAdmin(FalconServerApi &api);
};
