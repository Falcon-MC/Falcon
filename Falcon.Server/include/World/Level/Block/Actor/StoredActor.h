#pragma once

#include "Core/Math/Vector3f.h"
#include "Core/NBT/Tag.h"

#include <functional>
#include <string>

class Level;
class ServerActor;
class ServerNetworkHandler;

// an actor kept inside a block as its save: bees in a hive, entities in a block entity storage
namespace StoredActor {
    // the saved actor comes back in the world: its data, equipment and health, then configure
    ServerActor *restore(ServerNetworkHandler &owner, Level &level, const std::string &identifier, const Tag &saved,
                         const Vector3f &position, const std::function<void(ServerActor &)> &configure = nullptr);
}
