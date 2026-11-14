#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Types/ItemUseTransaction.h"
#include "Protocol/Types/StartGameTypes.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "Scripting/ScriptEngine.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"
#include "World/Level/WorldManager.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    ItemStackRequestSlotData slotOf(ContainerSlotType container, int32_t slot) {
        ItemStackRequestSlotData data;
        data.mContainer = container;
        data.mContainerName.mContainer = container;
        data.mSlot = slot;
        return data;
    }

    // moves the whole stack of inventory slot 0 into the off hand
    int32_t moveToOffhand(ServerNetworkHandler &owner, PlayerInventory &inventory) {
        ItemStackRequest request;
        request.mRequestId = -1;

        ItemStackRequestAction place;
        place.mType = ItemStackRequestActionType::Place;
        place.mCount = 1;
        place.mSource = slotOf(ContainerSlotType::Inventory, 0);
        place.mDestination = slotOf(ContainerSlotType::Offhand, PlayerInventory::OFFHAND_NETWORK_SLOT);
        request.mActions.push_back(place);

        const std::vector<CreativeItemData> creative;
        return ItemStackRequestHandler::execute(inventory, request, creative, owner.getRecipeOutputs(),
                                                owner.getRecipeSourceIndices(), false, false, nullptr, nullptr,
                                                &owner.getCodecContext(), nullptr, 0, false, nullptr, false, nullptr,
                                                nullptr, nullptr, nullptr, nullptr, nullptr)
                .mResult;
    }

    size_t countOf(ServerNetworkHandler &owner, const std::string &identifier) {
        size_t count = 0;
        for (const auto &entry: owner.getActors()) {
            if (entry.second != nullptr && identifier == entry.second->getIdentifier())
                ++count;
        }
        return count;
    }

    void run(const std::filesystem::path &directory) {
        ServerNetworkHandler owner("Pack item rules", "", 1);
        const auto properties = directory / "test.properties";
        std::ofstream(properties) << "spawn-protection=0\n";
        owner.setProperties(PropertiesSettings(properties.string()));

        owner.getScriptEngine().bindHost(owner);
        owner.getScriptEngine().startup();
        EntityDefinitions::initialize();
        Level &level = owner.getWorlds().getDefault().getOverworld();
        level.insertChunk(LevelChunk(0, 0));

        CustomContentRegistry &registry = CustomContentRegistry::getInstance();
        CustomItemDefinition plain;
        plain.mIdentifier = "test:plain";
        plain.mDisplayName = "Plain";
        require(registry.registerItem(plain, owner.getItemDefinitions()) != nullptr, "the plain item was refused");

        CustomItemDefinition club;
        club.mIdentifier = "test:club";
        club.mDisplayName = "Club";
        club.mAllowOffHand = true;
        club.mCanDestroyInCreative = false;
        require(registry.registerItem(club, owner.getItemDefinitions()) != nullptr, "the club item was refused");

        CustomItemDefinition crate;
        crate.mIdentifier = "test:crate";
        crate.mDisplayName = "Crate";
        crate.mHasEntityPlacer = true;
        crate.mPlacerEntity = "minecraft:pig";
        crate.mEntityPlacerUseOn = {"minecraft:stone"};
        require(registry.registerItem(crate, owner.getItemDefinitions()) != nullptr, "the crate item was refused");

        NetworkIdentifier id;
        ServerPlayer &player = owner.getPlayers().try_emplace(id, id, 4001).first->second;
        player.setName("Tester");
        player.getAttributes() = ActorAttributes::createPlayerDefaults();
        player.moveToLevel(level);
        player.setLoginState(ServerPlayer::LoginState::Spawned);
        player.setPosition(Vector3f(0.5f, 100.0f, 0.5f));
        player.getInventoryManager().attach(&player, &owner);

        // the off hand only takes a pack item that allows it
        {
            PlayerInventory refused;
            refused.setItem(0, owner.createItemStack("test:plain", 1));
            require(moveToOffhand(owner, refused) != ItemStackRequestHandler::RESULT_OK && refused.getOffhand().isAir(),
                    "a pack item without allow_off_hand went into the off hand");

            PlayerInventory allowed;
            allowed.setItem(0, owner.createItemStack("test:club", 1));
            require(moveToOffhand(owner, allowed) == ItemStackRequestHandler::RESULT_OK
                            && !allowed.getOffhand().isAir()
                            && allowed.getOffhand().mDefinition->getIdentifier() == "test:club",
                    "a pack item with allow_off_hand was kept out of the off hand");
        }

        // a creative player breaks a block at once, except while holding an item that refuses to
        {
            const Vector3i block(0, 101, 2);
            const BlockState stone("minecraft:stone");
            player.setGameType((int32_t) GameType::Creative);
            PlayerInventory &inventory = player.getInventory();
            inventory.setSelectedSlot(0);

            level.setBlockState(block.x, block.y, block.z, stone);
            inventory.setItem(0, owner.createItemStack("test:club", 1));
            BlockActionHandler::startBreakingBlock(owner, player, block, 1);
            BlockActionHandler::completeBreakingBlock(owner, player, block);
            require(level.getBlockState(block.x, block.y, block.z).mName == "minecraft:stone",
                    "an item with can_destroy_in_creative false broke a block in creative");
            BlockActionHandler::stopBreakingBlock(owner, player);

            inventory.setItem(0, owner.createItemStack("test:plain", 1));
            BlockActionHandler::startBreakingBlock(owner, player, block, 1);
            BlockActionHandler::completeBreakingBlock(owner, player, block);
            require(level.getBlockState(block.x, block.y, block.z).mName == "minecraft:air",
                    "a creative player holding an ordinary pack item could not break a block");
            BlockActionHandler::stopBreakingBlock(owner, player);
            player.setGameType((int32_t) GameType::Survival);
        }

        // an entity placer spawns its entity against the clicked face, only on the blocks it lists
        {
            const Vector3i stone(2, 100, 2);
            const Vector3i dirt(4, 100, 2);
            level.setBlockState(stone.x, stone.y, stone.z, BlockState("minecraft:stone"));
            level.setBlockState(dirt.x, dirt.y, dirt.z, BlockState("minecraft:dirt"));
            PlayerInventory &inventory = player.getInventory();
            inventory.setSelectedSlot(0);
            inventory.setItem(0, owner.createItemStack("test:crate", 2));
            player.setPosition(Vector3f(2.5f, 101.0f, 0.5f));

            ItemUseTransaction use;
            use.mBlockFace = 1;
            use.mHotbarSlot = 0;
            use.mItemInHand = inventory.getItemInHand();
            use.mPlayerPosition = player.getPosition();
            use.mClickPosition = Vector3f(0.5f, 1.0f, 0.5f);
            use.mClientInteractPrediction = ItemUsePredictedResult::Success;

            const size_t before = countOf(owner, "minecraft:pig");
            use.mBlockPosition = dirt;
            BlockActionHandler::placeBlock(owner, player, use);
            require(countOf(owner, "minecraft:pig") == before && inventory.getItem(0).mCount == 2,
                    "an entity placer worked on a block outside its use_on list");

            use.mBlockPosition = stone;
            BlockActionHandler::placeBlock(owner, player, use);
            require(countOf(owner, "minecraft:pig") == before + 1, "the entity placer spawned nothing on its block");
            require(inventory.getItem(0).mCount == 1, "the entity placer did not consume one item");
        }
    }
}

int main() {
    const auto originalRoot = ServerPaths::getRoot();
    const auto directory = std::filesystem::temp_directory_path()
                           / ("falcon-pack-item-rules-"
                              + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        run(directory);
    } catch (const std::exception &error) {
        std::clog << "Pack item rules regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
