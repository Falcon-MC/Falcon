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
};
