#include "Network/Handler/MovementHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/PlayerAuthInputPacket.h"
#include "Protocol/Packets/SetActorDataPacket.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/Projectile/FishingHookActor.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

class ServerTickProbe {
public:
    static void advance(ServerNetworkHandler &owner, int64_t ticks) {
        owner.mCurrentTick += ticks;
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    void checkFishingInput(ServerNetworkHandler &owner, ServerPlayer &player) {
        player.getInventory().setItemInHand(owner.createItemStack("minecraft:fishing_rod", 1));
        player.setLastChunkPosition(0, 0);
        PlayerAuthInputPacket input;
        input.mPosition = Vector3f(0.5f, 101.62f, 0.5f);
        input.mInputData = {(int32_t) PlayerAuthInputData::StartUsingItem};
        input.mHasItemUseTransaction = true;
        input.mItemUseTransaction.mActionType = 1;
        MovementHandler::handlePlayerAuthInput(owner, player.getNetworkIdentifier(), player, input);
        owner.tickActors();
        require(owner.getActors().size() == 1, "one rod input must leave one hook in flight");
        auto *hook = dynamic_cast<FishingHookActor *>(owner.getActors().begin()->second.get());
        require(hook != nullptr && hook->isOwnedBy(player), "the hook lost its angler");

        ++input.mTick;
        input.mPlayerActions.emplace_back().mAction = PlayerActionType::StartUsingItem;
        MovementHandler::handlePlayerAuthInput(owner, player.getNetworkIdentifier(), player, input);
        owner.tickActors();
        require(owner.getActors().empty(), "one reel input must remove the hook without recasting");
    }

    // starting to eat ends a sprint in the same input: no flags sent for it may clear the use
    void checkSprintingEatInput(ServerNetworkHandler &owner, ServerPlayer &player) {
        player.getInventory().setItemInHand(owner.createItemStack("minecraft:golden_apple", 1));
        player.getFlags().set(ActorFlag::UsingItem, false);
        player.getFlags().set(ActorFlag::Sprinting, true);
        ServerTickProbe::advance(owner, 40);

        std::vector<bool> usingItemSent;
        owner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &bytes) {
            ReadOnlyBinaryStream header(bytes);
            unsigned char sender = 0, recipient = 0;
            if (Packet::peekId(header, sender, recipient) != MinecraftPacketIds::SetActorData)
                return false;
            ReadOnlyBinaryStream body(bytes);
            SetActorDataPacket data;
            data.readHeader(body);
            data.read(body, owner.getCodecContext());
            for (const EntityDataEntry &entry: data.mMetadata.mEntries) {
                if (entry.mId == ActorFlags::FLAGS_DATA_ID)
                    usingItemSent.push_back(((entry.mLongValue >> (int) ActorFlag::UsingItem) & 1) != 0);
            }
            return false;
        });
        PlayerAuthInputPacket input;
        input.mTick = 100;
        input.mPosition = Vector3f(0.5f, 101.62f, 0.5f);
        input.mInputData = {(int32_t) PlayerAuthInputData::StopSprinting,
                            (int32_t) PlayerAuthInputData::StartUsingItem};
        input.mHasItemUseTransaction = true;
        input.mItemUseTransaction.mActionType = 1;
        MovementHandler::handlePlayerAuthInput(owner, player.getNetworkIdentifier(), player, input);
        owner.getNetworkHandler().setOutboundFilter(nullptr);

        require(player.getFlags().get(ActorFlag::UsingItem), "the apple did not start being eaten while sprinting");
        require(!player.getFlags().get(ActorFlag::Sprinting), "the sprint survived StopSprinting");
        require(!usingItemSent.empty(), "the sprint stop sent no flags");
        for (const bool usingItem: usingItemSent)
            require(usingItem, "flags sent with the eating input cleared UsingItem on the client");
    }

    // a brush on a block is held like food: without the flag the client drops the brushing
    void checkBrushUse(ServerNetworkHandler &owner, ServerPlayer &player) {
        Level &level = owner.getLevelFor(player);
        const Vector3i position(1, 99, 0);
        level.setBlock(position, VanillaBlocks::fromIdentifier("minecraft:stone")->toBlockState(), false);
        player.getInventory().setItemInHand(owner.createItemStack("minecraft:brush", 1));
        player.getFlags().set(ActorFlag::UsingItem, false);
        const Item *brush = VanillaItems::fromIdentifier("minecraft:brush");
        require(brush != nullptr, "no brush item");
        require(brush->onUseOnBlock(owner, player, player.getInventory().getItemInHand(), position, 1,
                                    Vector3f(0.5f, 1.0f, 0.5f)),
                "the brush did not take the block");
        require(player.getFlags().get(ActorFlag::UsingItem), "brushing did not start an item use");
        require(player.getItemUseSlot() == player.getInventory().getSelectedSlot(), "the use lost its slot");
    }

    // a drawn bow switched for a golden apple must not hand the apple its elapsed time
    void run() {
        ServerNetworkHandler owner("Item use slot", "", 1);
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));
        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 3001).first->second;
        player.setName("Archer");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));

        PlayerInventory &inventory = player.getInventory();
        inventory.setItem(0, owner.createItemStack("minecraft:bow", 1));
        inventory.setItem(1, owner.createItemStack("minecraft:golden_apple", 2));
        inventory.setItem(9, owner.createItemStack("minecraft:arrow", 16));
        inventory.setSelectedSlot(0);

        owner._useHeldItem(player);
        require(player.getFlags().get(ActorFlag::UsingItem), "the bow did not start drawing");
        ServerTickProbe::advance(owner, 40);

        inventory.setSelectedSlot(1);
        owner._tickItemUse(player);
        require(!player.getFlags().get(ActorFlag::UsingItem), "the use survived the slot change");
        require(inventory.getItem(1).mCount == 2, "the apple was eaten with the bow's draw time");

        owner._completeItemUse(player, (int32_t) inventory.getItem(1).mDefinition->getRuntimeId());
        require(inventory.getItem(1).mCount == 2, "a completed use ate the apple without eating it");

        // a use that keeps its item still completes
        owner._useHeldItem(player);
        require(player.getFlags().get(ActorFlag::UsingItem), "the apple did not start being eaten");
        ServerTickProbe::advance(owner, 40);
        owner._tickItemUse(player);
        require(inventory.getItem(1).mCount == 1, "a full use of the apple did not eat it");
        checkFishingInput(owner, player);
        checkSprintingEatInput(owner, player);
        checkBrushUse(owner, player);
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-item-use-slot-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run();
    } catch (const std::exception &error) {
        std::clog << "Item use slot regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
