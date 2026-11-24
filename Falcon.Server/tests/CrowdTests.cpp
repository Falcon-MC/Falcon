#include "Core/Debug/BedrockLog.h"
#include "Core/Debug/ContentLogEndPoint.h"
#include "Core/Math/Vector3f.h"
#include "Network/ServerNetworkHandler.h"
#include "Network/SharedPacket.h"
#include "Protocol/Types/SerializedSkin.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Level.h"

#include <functional>
#include <memory>
#include <set>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    std::set<std::string> namesNear(ServerNetworkHandler &owner, const Vector3f &position, float radius) {
        std::set<std::string> names;
        owner.forEachPlayerNear(position, radius, [&](ServerPlayer &player) { names.insert(player.getName()); });
        return names;
    }

    void playerIndex() {
        ServerNetworkHandler owner("Crowd", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);

        ServerPlayer &near = *owner.spawnSimulatedPlayer("near", level, Vector3f(8.5f, 100.0f, 8.5f), 0.0f, 1);
        ServerPlayer &beside = *owner.spawnSimulatedPlayer("beside", level, Vector3f(40.5f, 100.0f, 8.5f), 0.0f, 1);
        ServerPlayer &far = *owner.spawnSimulatedPlayer("far", level, Vector3f(2000.5f, 100.0f, 2000.5f), 0.0f, 1);

        const std::set<std::string> around = namesNear(owner, near.getPosition(), 48.0f);
        require(around.count("near") == 1 && around.count("beside") == 1, "players within the radius are visited");
        require(around.count("far") == 0, "a player 2000 blocks away is not visited");
        require(namesNear(owner, far.getPosition(), 16.0f) == std::set<std::string>{"far"},
                "a lone player is the only one visited around itself");

        require(owner.getPlayerByRuntimeId(beside.getRuntimeId()) == &beside, "a player is found by its runtime id");
        require(owner.getPlayerByRuntimeId(0xffffffffULL) == nullptr, "an unknown runtime id finds nobody");

        // joined after the index was built: still found, by id and by place
        ServerPlayer &late = *owner.spawnSimulatedPlayer("late", level, Vector3f(9.5f, 100.0f, 9.5f), 0.0f, 1);
        require(owner.getPlayerByRuntimeId(late.getRuntimeId()) == &late, "a player who just joined is found by id");

        const uint64_t besideId = beside.getRuntimeId();
        owner.erasePlayer(beside.getNetworkIdentifier());
        require(owner.getPlayerByRuntimeId(besideId) == nullptr, "a player who left is no longer found by id");
        require(namesNear(owner, near.getPosition(), 48.0f).count("beside") == 0,
                "a player who left is no longer visited");
    }

    void listEntryCache() {
        ServerNetworkHandler owner("Crowd list", "", 1);
        Level &level = owner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
        ServerPlayer &player = *owner.spawnSimulatedPlayer("listed", level, Vector3f(0.5f, 100.0f, 0.5f), 0.0f, 1);

        std::shared_ptr<const SharedPacket> &slot = player.getListEntryPacket(1);
        slot = std::make_shared<const SharedPacket>(std::string("entry"));
        require(player.getListEntryPacket(1) != nullptr, "the entry is kept for its protocol version");
        require(player.getListEntryPacket(2) == nullptr, "another protocol version has its own entry");

        // the entry carries the skin: a new skin makes it stale
        player.setSkin(SerializedSkin());
        require(player.getListEntryPacket(1) == nullptr, "a skin change drops the cached entries");
    }
}

int main() {
    BedrockLog::addEndPoint(std::make_shared<ContentLogEndPoint>(false));

    int failures = 0;
    const auto check = [&failures](const char *name, const std::function<void()> &run) {
        try {
            run();
            LOG_INFO(LogAreaID::Server, "PASS: %s", name);
        } catch (const std::exception &error) {
            ++failures;
            LOG_ERROR(LogAreaID::Server, "FAIL: %s: %s", name, error.what());
        }
    };

    check("players are indexed by place and by id", playerIndex);
    check("a list entry is cached until the skin changes", listEntryCache);

    BedrockLog::flush();
    return failures == 0 ? 0 : 1;
}
