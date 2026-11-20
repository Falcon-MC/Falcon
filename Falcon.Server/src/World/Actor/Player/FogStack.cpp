#include "World/Actor/Player/FogStack.h"

#include "Core/NBT/Tag.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerFogPacket.h"

#include <algorithm>
#include <regex>

namespace {
    const char *const TAG_FOG_STACK = "fogCommandStack";
}

bool FogStack::isValidId(const std::string &id) {
    static const std::regex PATTERN("[a-z0-9._]+:[a-z0-9._]+");
    std::string lowered = id;
    for (char &c: lowered) {
        if (c >= 'A' && c <= 'Z')
            c = (char) (c + 32);
    }
    return std::regex_match(lowered, PATTERN);
}

bool FogStack::push(Entries &stack, const std::string &id, const std::string &tag) {
    if (stack.size() >= LIMIT)
        return false;
    stack.push_back({id, tag});
    return true;
}

std::string FogStack::pop(Entries &stack, const std::string *tag) {
    for (auto it = stack.rbegin(); it != stack.rend(); ++it) {
        if (tag != nullptr && it->mTag != *tag)
            continue;
        std::string id = it->mId;
        stack.erase(std::next(it).base());
        return id;
    }
    return std::string();
}

bool FogStack::remove(Entries &stack, const std::string *tag) {
    const size_t before = stack.size();
    if (tag == nullptr)
        stack.clear();
    else
        stack.erase(std::remove_if(stack.begin(), stack.end(),
                                   [&](const ServerPlayer::FogEntry &entry) { return entry.mTag == *tag; }),
                    stack.end());
    return stack.size() != before;
}

void FogStack::send(ServerNetworkHandler &owner, ServerPlayer &player) {
    PlayerFogPacket packet;
    for (const ServerPlayer::FogEntry &entry: player.getFogStack())
        packet.mFogStack.push_back(entry.mId);
    owner.sendPacketTo(player.getNetworkIdentifier(), packet);
}

void FogStack::save(const Entries &stack, Tag &data) {
    Tag list = Tag::ofList(Tag::Type::String);
    for (const ServerPlayer::FogEntry &entry: stack) {
        // scripts may push what the loader stops at: written, it would take every later entry with it
        if (entry.mId.empty() || entry.mTag.empty() || !isValidId(entry.mId))
            continue;
        list.addToList(Tag::ofString(entry.mId));
        list.addToList(Tag::ofString(entry.mTag));
    }
    data.put(TAG_FOG_STACK, std::move(list));
}

void FogStack::load(Entries &stack, const Tag &data) {
    stack.clear();
    const Tag *list = data.get(TAG_FOG_STACK);
    if (list == nullptr || list->getType() != Tag::Type::List)
        return;

    const std::vector<Tag> &values = list->getList();
    // an odd count is dropped whole, as the game does
    if (values.size() % 2 != 0)
        return;
    const size_t pairs = std::min(values.size() / 2, LIMIT);
    for (size_t i = 0; i < pairs; ++i) {
        const Tag &id = values[2 * i];
        const Tag &tag = values[2 * i + 1];
        if (id.getType() != Tag::Type::String || tag.getType() != Tag::Type::String)
            return;
        // the game stops at the first empty or invalid entry
        if (id.asString().empty() || tag.asString().empty() || !isValidId(id.asString()))
            return;
        stack.push_back({id.asString(), tag.asString()});
    }
}
