#pragma once

#include <cstdint>
#include <vector>

struct BehaviorPack;
class ServerActor;
class ServerNetworkHandler;

class ServerAnimations {
public:
    static void loadPacks(const std::vector<BehaviorPack> &packs);
    static void tick(ServerNetworkHandler &owner, ServerActor &actor);
    static void forget(uint64_t runtimeId);
};
