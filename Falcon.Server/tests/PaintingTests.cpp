#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/AddPaintingPacket.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorClassRegistry.h"
#include "World/Actor/Misc/PaintingActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Item/PaintingItem.h"
#include "World/Level/Block/BlockSupport.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <vector>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void run() {
        ServerNetworkHandler owner("Paintings", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0)).setPopulated(true);
        const NetworkIdentifier id;
        auto &player = owner.getPlayers().try_emplace(id, id, 8001).first->second;
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.getInventoryManager().attach(&player, &owner);
        player.getInventory().setItemInHand(owner.createItemStack("minecraft:painting", 3));

        const auto *item = VanillaItems::fromIdentifier(PaintingActor::IDENTIFIER);
        require(VanillaItems::PAINTING().getIdentifier() == PaintingActor::IDENTIFIER, "the painting item constant has the wrong identifier");
        require(dynamic_cast<const PaintingItem *>(item) != nullptr, "the painting item factory was bypassed");
        const Vector3i support(6, 80, 6);
        const auto use = [&](const Vector3i &position, int32_t face) {
            return item->onUseOnBlock(owner, player, player.getInventory().getItemInHand(), position, face, Vector3f());
        };
        level.setBlockState(6, 80, 6, BlockState("minecraft:stone"));
        require(!use(support, 1) && player.getInventory().getItemInHand().mCount == 3,
                "a horizontal surface accepted a painting");

        require(BlockSupport::isSolid(level.getBlockState(6, 80, 6)), "the support was not a solid block");
        require(level.getCollisionBoxes(AxisAlignedBB(6.03125f, 80.03125f, 7.0f, 6.96875f, 80.96875f, 7.0625f)).empty(),
                "the area ahead of the support was obstructed");
        auto torchStates = Tag::ofCompound();
        torchStates.putString("torch_facing_direction", "north");
        level.setBlockState(6, 80, 7, BlockState("minecraft:torch", torchStates));
        require(!use(support, 3), "a painting covered a torch attached to the same wall");
        level.setBlockState(6, 80, 7, BlockState("minecraft:air"));
        player.setGameType((int32_t) GameType::Adventure);
        require(!use(support, 3), "adventure placement ignored the item's restrictions");
        player.setGameType((int32_t) GameType::Survival);
        require(use(support, 3), "a one-block wall did not accept a painting");
        require(player.getInventory().getItemInHand().mCount == 2, "placement did not consume exactly one item");
        auto *small = dynamic_cast<PaintingActor *>(owner.getActors().begin()->second.get());
        require(small != nullptr && small->getSize().mWidth == 1.0f && small->getSize().mHeight == 1.0f,
                "the one-block wall selected an oversized motive");
        require(small->getPosition() == Vector3f(6.5f, 80.5f, 7.03125f), "the painting is off its wall");
        require(!use(support, 3) && player.getInventory().getItemInHand().mCount == 2,
                "overlapping placement succeeded or consumed an item");

        for (int32_t x = 9; x <= 12; ++x) {
            for (int32_t y = 79; y <= 82; ++y)
                level.setBlockState(x, y, 6, BlockState("minecraft:stone"));
        }
        require(use(Vector3i(10, 80, 6), 3), "the larger wall did not accept a painting");
        PaintingActor *large = nullptr;
        for (auto &[actorId, actor]: owner.getActors()) {
            if (actor.get() != small)
                large = dynamic_cast<PaintingActor *>(actor.get());
        }
        require(large != nullptr && large->getSize().mWidth == 4.0f && large->getSize().mHeight == 4.0f,
                "placement did not choose a maximum-area motive");
        require(large->getPosition() == Vector3f(11.0f, 81.0f, 7.03125f),
                "the even-sized painting was not centered on the correct seam");

        const Tag stored = large->saveNbt();
        require(stored.contains("Motif", Tag::Type::String) && stored.getByte("Direction") == 0
                        && stored.getByte("Dir") == 2,
                "the painting save did not use Bedrock's tags");
        auto reloaded = ActorClassRegistry::create(9001, PaintingActor::IDENTIFIER);
        reloaded->loadNbt(stored);
        require(reloaded->getPosition() == large->getPosition() && reloaded->getSize().mWidth == 4.0f,
                "save and reload lost the geometry");

        Tag legacy = stored;
        legacy.remove("Direction");
        legacy.remove("Motif");
        legacy.putByte("Dir", 0);
        legacy.putString("Motive", "prairie_ride");
        reloaded->loadNbt(legacy);
        require(reloaded->saveNbt().getString("Motif") == "prairie_ride"
                        && reloaded->saveNbt().getByte("Direction") == 2,
                "the legacy direction and motive did not load");

        std::vector<AddPaintingPacket> appearance;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &data) {
            ReadOnlyBinaryStream header(data);
            unsigned char sender = 0, receiver = 0;
            if (Packet::peekId(header, sender, receiver) == MinecraftPacketIds::AddPainting) {
                ReadOnlyBinaryStream body(data);
                AddPaintingPacket packet;
                packet.readHeader(body);
                packet.read(body, owner.getCodecContext());
                appearance.push_back(packet);
            }
            return false;
        });
        owner._sendActorSpawn(player, *large);
        owner.getNetworkHandler().setOutboundFilter(nullptr);
        require(appearance.size() == 1 && appearance[0].mRuntimeActorId == large->getRuntimeId()
                        && appearance[0].mPosition == large->getPosition() && appearance[0].mDirection == 0
                        && appearance[0].mMotive == stored.getString("Motif"),
                "the painting spawn did not use its dedicated packet");

        large->tick(owner);
        level.setBlockState(9, 79, 6, BlockState("minecraft:air"));
        for (int32_t tick = 0; tick < 121; ++tick)
            large->tick(owner);
        require(large->isExpired() && !large->shouldSave() && owner.getItemEntities().size() == 1,
                "breaking the support did not drop exactly one painting");

        player.setGameType((int32_t) GameType::Creative);
        for (int32_t face: {2, 4, 5})
            require(use(support, face), "a different wall face did not accept a painting");
        require(player.getInventory().getItemInHand().mCount == 1, "creative placement consumed a painting");
        require(small->onHurt(owner, 1.0f, &player) && small->isExpired() && owner.getItemEntities().size() == 1,
                "creative damage dropped a painting");
    }
}

int main() {
    const auto previous = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-paintings-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Painting regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(previous);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
