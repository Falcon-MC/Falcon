#pragma once

class ServerActor;
class ServerNetworkHandler;

// minecraft:burns_in_daylight for every actor that declares it: sub_14CE80670
class SunlightBurn {
public:
    static void tick(ServerNetworkHandler &owner, ServerActor &actor);
};
