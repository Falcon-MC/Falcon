#pragma once

#include "Core/Json/Json.h"

class Actor;
class MobActor;
class ServerNetworkHandler;

class EntityFilter {
public:
    static bool test(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                     const Actor *other = nullptr);

private:
    static bool _testAll(const json::Value &filters, ServerNetworkHandler &owner, const MobActor &self,
                         const Actor *other);

    static bool _testAny(const json::Value &filters, ServerNetworkHandler &owner, const MobActor &self,
                         const Actor *other);

    static bool _testSingle(const json::Value &filter, ServerNetworkHandler &owner, const MobActor &self,
                            const Actor *other);
};
