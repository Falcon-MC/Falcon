#pragma once

#include "World/Actor/Player/ServerPlayer.h"

#include <cstddef>
#include <string>
#include <vector>

class ServerNetworkHandler;
class Tag;

// the fog stack /fog and the scripting FogSettings share; entries bottom first
namespace FogStack {
    using Entries = std::vector<ServerPlayer::FogEntry>;

    const size_t LIMIT = 16;
    // the tag the game files an entry under when none is given
    const char *const UNTAGGED = "untagged";

    // lowercased, the id must read [a-z0-9._]+:[a-z0-9._]+
    bool isValidId(const std::string &id);

    // false at the limit
    bool push(Entries &stack, const std::string &id, const std::string &tag);

    // the topmost entry with tag, or the top with none; empty when nothing was popped
    std::string pop(Entries &stack, const std::string *tag);

    // every entry with tag, or all with none; false when nothing was removed
    bool remove(Entries &stack, const std::string *tag);

    // the ids the client renders
    void send(ServerNetworkHandler &owner, ServerPlayer &player);

    // "fogCommandStack": a flat list of strings, id then tag
    void save(const Entries &stack, Tag &data);

    void load(Entries &stack, const Tag &data);
}
