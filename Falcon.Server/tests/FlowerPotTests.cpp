#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BlockPickRequestPacket.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/FlowerPotBlockActor.h"
#include "World/Level/Block/FlowerPotBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/BlockTickingQueue.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>

// the pick request handler is the server's own: reached the way the other tests reach its internals
class ServerTickProbe {
public:
    static void pick(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position) {
        BlockPickRequestPacket packet;
        packet.mBlockPosition = position;
        packet.mAddUserData = false;
        packet.mHotbarSlot = 0;
        owner.handle(player.getNetworkIdentifier(), packet);
    }
};

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    // the spawn chunk keeps its generated terrain: the cases stay in the empty chunks east of it
    struct Arena {
        ServerNetworkHandler mOwner{"Flower pots", "", 1};
        Level *mLevel = nullptr;
        ServerPlayer *mPlayer = nullptr;

        Arena() {
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            mLevel->setOwner(&mOwner);
            for (int32_t x = 1; x <= 3; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                    mLevel->getBlockActors().loadChunk(x, z, {}, mOwner.getCodecContext());
                }
            }
            const NetworkIdentifier id("pot", 9001);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, 9001).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(20.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            mPlayer = &player;
        }

        BlockState block(const char *identifier) {
            const Block *value = VanillaBlocks::fromIdentifier(identifier);
            require(value != nullptr, std::string("no block ") + identifier);
            return value->toBlockState();
        }

        // a pot on stone
        Vector3i pot(int32_t x) {
            const Vector3i at(x, 100, 0);
            mLevel->setBlock(Vector3i(x, 99, 0), block("minecraft:stone"), false);
            mLevel->setBlock(at, block("minecraft:flower_pot"), false);
            return at;
        }

        FlowerPotBlockActor *actor(const Vector3i &at) {
            return mLevel->getBlockActors().find<FlowerPotBlockActor>(at);
        }

        void hold(const char *identifier, int32_t count = 1) {
            ItemStack item = identifier == nullptr ? ItemStack::air() : mOwner.createItemStack(identifier, count);
            mPlayer->getInventory().setItemInHand(item);
        }

        bool use(const Vector3i &at) {
            const BlockState state = mLevel->getBlockState(at.x, at.y, at.z);
            return VanillaBlocks::fromState(state)->onInteract(mOwner, *mPlayer, at, state);
        }

        std::string plantAt(const Vector3i &at) {
            FlowerPotBlockActor *pot = actor(at);
            return pot == nullptr || !pot->hasPlant() ? std::string() : pot->getPlant().mName;
        }

        int32_t dropped(const char *identifier) {
            int32_t count = 0;
            for (const auto &item: mOwner.getItemEntities()) {
                const ItemStack &stack = item->getItem();
                if (!stack.isAir() && stack.mDefinition != nullptr && stack.mDefinition->getIdentifier() == identifier)
                    count += stack.mCount;
            }
            return count;
        }

        int32_t carried(const char *identifier) {
            int32_t count = 0;
            PlayerInventory &inventory = mPlayer->getInventory();
            for (int32_t slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot) {
                const ItemStack &stack = inventory.getItem(slot);
                if (!stack.isAir() && stack.mDefinition != nullptr && stack.mDefinition->getIdentifier() == identifier)
                    count += stack.mCount;
            }
            return count;
        }

        void clearDrops() {
            mOwner.getItemEntities().clear();
        }
    };

    // a pottable plant fills an empty pot, used up, and redraws it with the flipped update_bit
    void planting(Arena &arena) {
        const Vector3i at = arena.pot(20);
        const int8_t before = arena.mLevel->getBlockState(at.x, at.y, at.z).mStates.getByte("update_bit", 0);
        arena.mPlayer->setGameType((int32_t) GameType::Survival);
        arena.hold("minecraft:poppy", 3);
        require(arena.use(at), "using a pot was not handled");
        require(arena.plantAt(at) == "minecraft:poppy", "a poppy did not go into the pot");
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 2, "planting did not use the poppy up");
        require(arena.mLevel->getBlockState(at.x, at.y, at.z).mStates.getByte("update_bit", 0) != before,
                "the pot's update_bit did not flip");

        // a second pottable plant leaves a full pot as it is
        arena.hold("minecraft:dandelion", 1);
        arena.use(at);
        require(arena.plantAt(at) == "minecraft:poppy", "a dandelion replaced the poppy");
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 1, "a full pot took the dandelion");
    }

    // anything not pottable, or an empty hand, takes the plant back
    void takingOut(Arena &arena) {
        const Vector3i at = arena.pot(22);
        arena.hold("minecraft:cornflower", 1);
        arena.use(at);
        require(arena.plantAt(at) == "minecraft:cornflower", "the cornflower did not go in");

        arena.hold("minecraft:stick", 1);
        const int32_t before = arena.carried("minecraft:cornflower");
        arena.use(at);
        require(arena.plantAt(at).empty(), "a stick did not take the plant out");
        require(arena.carried("minecraft:cornflower") == before + 1, "the plant taken out is not in the inventory");

        arena.hold("minecraft:oak_sapling", 1);
        arena.use(at);
        arena.hold(nullptr);
        arena.use(at);
        require(arena.plantAt(at).empty(), "an empty hand did not take the sapling out");
        require(arena.carried("minecraft:oak_sapling") >= 1, "the sapling taken out is not in the inventory");

        // grass is no pottable plant: an empty pot stays empty
        arena.hold("minecraft:short_grass", 1);
        arena.use(at);
        require(arena.plantAt(at).empty(), "short grass went into a pot");
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 1, "short grass was used up");
    }

    // a creative player plants without using the plant up
    void creative(Arena &arena) {
        const Vector3i at = arena.pot(24);
        arena.mPlayer->setGameType((int32_t) GameType::Creative);
        arena.hold("minecraft:cactus", 1);
        arena.use(at);
        require(arena.plantAt(at) == "minecraft:cactus", "a cactus did not go in");
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 1, "a creative player used the cactus up");

        // breaking it in creative drops nothing
        arena.clearDrops();
        const BlockState state = arena.mLevel->getBlockState(at.x, at.y, at.z);
        VanillaBlocks::fromState(state)->onPlayerWillDestroy(arena.mOwner, *arena.mPlayer, *arena.mLevel, at, state);
        BlockActionHandler::destroyBlock(arena.mOwner, *arena.mLevel, at, state, false, ItemStack::air(),
                                         arena.mPlayer);
        require(arena.dropped("minecraft:cactus") == 0, "a pot broken in creative dropped its cactus");
        arena.mPlayer->setGameType((int32_t) GameType::Survival);
    }

    // broken any other way, the pot drops with its plant
    void breaking(Arena &arena) {
        const Vector3i at = arena.pot(26);
        arena.hold("minecraft:red_mushroom", 1);
        arena.use(at);
        arena.clearDrops();
        const BlockState state = arena.mLevel->getBlockState(at.x, at.y, at.z);
        BlockActionHandler::destroyBlock(arena.mOwner, *arena.mLevel, at, state, true, ItemStack::air(),
                                         arena.mPlayer);
        require(arena.dropped("minecraft:red_mushroom") == 1, "a broken pot did not drop its mushroom");
        require(arena.dropped("minecraft:flower_pot") == 1, "a broken pot did not drop itself");
        require(arena.actor(at) == nullptr, "the pot's block actor outlived it");

        // with nothing under it, it breaks and drops the same
        const Vector3i floating = arena.pot(28);
        arena.hold("minecraft:fern", 1);
        arena.use(floating);
        arena.clearDrops();
        arena.mLevel->setBlock(Vector3i(28, 99, 0), BlockState("minecraft:air"), true);
        require(arena.mLevel->getBlockState(floating.x, floating.y, floating.z).mName == "minecraft:air",
                "a pot with nothing under it stayed");
        require(arena.dropped("minecraft:fern") == 1, "a pot losing its support did not drop its fern");
    }

    // a potted eyeblossom opens at night and closes by day, not where there is no day
    void eyeblossom(Arena &arena) {
        const Vector3i at = arena.pot(30);
        arena.hold("minecraft:closed_eyeblossom", 1);
        arena.use(at);
        const BlockState state = arena.mLevel->getBlockState(at.x, at.y, at.z);
        const Block *pot = VanillaBlocks::fromState(state);

        arena.mLevel->setTime(12600);
        pot->onRandomTick(arena.mOwner, *arena.mLevel, at, state);
        require(arena.plantAt(at) == "minecraft:closed_eyeblossom", "an eyeblossom opened before 12601");
        arena.mLevel->setTime(12601);
        pot->onRandomTick(arena.mOwner, *arena.mLevel, at, state);
        require(arena.plantAt(at) == "minecraft:open_eyeblossom", "an eyeblossom did not open at 12601");
        arena.mLevel->setTime(24000 + 23400);
        pot->onRandomTick(arena.mOwner, *arena.mLevel, at, state);
        require(arena.plantAt(at) == "minecraft:open_eyeblossom", "an eyeblossom closed before 23401");
        arena.mLevel->setTime(24000 + 23401);
        pot->onRandomTick(arena.mOwner, *arena.mLevel, at, state);
        require(arena.plantAt(at) == "minecraft:closed_eyeblossom", "an eyeblossom did not close at 23401");

        Level &nether = arena.mOwner.getWorlds().getDefault().getLevel(DimensionType::Nether);
        require(FlowerPotBlock::eyeblossomFor(nether).empty(), "an eyeblossom keeps time in the Nether");
    }

    // picking a planted pot gives its plant; an empty one, the pot
    void picking(Arena &arena) {
        const Vector3i at = arena.pot(32);
        arena.mPlayer->setGameType((int32_t) GameType::Creative);
        arena.hold(nullptr);
        ServerTickProbe::pick(arena.mOwner, *arena.mPlayer, at);
        require(arena.carried("minecraft:flower_pot") >= 1, "picking an empty pot did not give a pot");

        arena.mPlayer->getInventory().setItem(0, ItemStack::air());
        arena.hold("minecraft:blue_orchid", 1);
        arena.use(at);
        arena.mPlayer->getInventory().setItem(0, ItemStack::air());
        const int32_t before = arena.carried("minecraft:blue_orchid");
        ServerTickProbe::pick(arena.mOwner, *arena.mPlayer, at);
        require(arena.carried("minecraft:blue_orchid") > before, "picking a planted pot did not give its plant");
        arena.mPlayer->setGameType((int32_t) GameType::Survival);
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-pots-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena;
        planting(arena);
        takingOut(arena);
        creative(arena);
        breaking(arena);
        eyeblossom(arena);
        picking(arena);
    } catch (const std::exception &error) {
        std::cout << "Flower pot regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
