#pragma once

#include <string>

class ServerActor;

enum class ActorCategory {
    Other,
    Passive,
    Neutral,
    Hostile
};

namespace ActorCategories {

    ActorCategory of(const std::string &identifier);

    bool isHostile(const std::string &identifier);

    bool isNeutral(const std::string &identifier);

    bool isPassive(const std::string &identifier);

    bool isPreventingSleep(const ServerActor &actor);

}
