#include "Network/Handler/LodestoneHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/PositionTrackingDB.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    TrackingRecord recordAt(int32_t id, const Vector3i &position, int32_t dimension) {
        TrackingRecord record;
        record.mId = id;
        record.mPosition = position;
        record.mDimension = dimension;
        return record;
    }

    ItemStack compassFor(ServerNetworkHandler &owner, int32_t trackingId) {
        ItemStack compass = owner.createItemStack("minecraft:lodestone_compass", 1);
        require(!compass.isAir(), "no lodestone compass item");
        compass.mTag = Tag::ofCompound();
        compass.mTag.putInt("trackingHandle", trackingId);
        return compass;
    }

    void run() {
        ServerNetworkHandler owner("Lodestone tracking", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);

        const NetworkIdentifier id("9701", 9701);
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 9701).first->second;
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setPosition(Vector3f(8.5f, 100.0f, 8.5f));
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.getChunkStreamState().mRadius = 4;

        const int32_t overworld = level.getDimensionId();
        const TrackingRecord far = recordAt(7, Vector3i(20000, 64, -20000), overworld);
        require(!LodestoneHandler::mayKnow(owner, player, far), "a far lodestone was known by its id alone");

        player.getInventory().setItem(5, compassFor(owner, 6));
        require(!LodestoneHandler::mayKnow(owner, player, far), "a compass of another lodestone revealed this one");
        player.getInventory().setItem(6, compassFor(owner, 7));
        require(LodestoneHandler::mayKnow(owner, player, far), "the compass linked to the lodestone was refused");
        player.getInventory().setItem(6, ItemStack::air());

        const TrackingRecord near = recordAt(8, Vector3i(8 + 16 * 4, 64, 8 - 16 * 4), overworld);
        require(LodestoneHandler::mayKnow(owner, player, near), "a lodestone in the loaded chunks was hidden");
        const TrackingRecord edge = recordAt(9, Vector3i(8 + 16 * 5, 64, 8), overworld);
        require(!LodestoneHandler::mayKnow(owner, player, edge), "a lodestone past the view was known");
        const TrackingRecord elsewhere = recordAt(10, Vector3i(8, 64, 8), overworld + 1);
        require(!LodestoneHandler::mayKnow(owner, player, elsewhere), "a lodestone of another dimension was known");
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-lodestone-tracking-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        run();
        std::cout << "lodestone tracking checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
