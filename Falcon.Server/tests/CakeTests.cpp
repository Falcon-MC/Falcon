#include "Network/Handler/BlockActionHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventPacket.h"
#include "Protocol/Types/ItemDefinitionRegistry.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/ActorAttributes.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Misc/ItemActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Item/VanillaItems.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/CakeBlock.h"
#include "World/Level/Block/VanillaBlocks.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <cmath>
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

    // the spawn chunk keeps its generated terrain: the cases stay in the empty chunks east of it
    struct Arena {
        ServerNetworkHandler mOwner{"Cakes", "", 1};
        std::filesystem::path mDirectory;
        Level *mLevel = nullptr;
        ServerPlayer *mPlayer = nullptr;

        // the difficulty is set once: reloading the properties drops the chunks placed below
        Arena(const std::filesystem::path &directory, const char *difficulty) : mDirectory(directory) {
            const std::filesystem::path path = mDirectory / (std::string(difficulty) + ".properties");
            std::ofstream(path) << "difficulty=" << difficulty << "\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            mLevel->setOwner(&mOwner);
            for (int32_t x = 1; x <= 3; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                    mLevel->getBlockActors().loadChunk(x, z, {}, mOwner.getCodecContext());
                }
            }
            const NetworkIdentifier id("cake", 9001);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, 9001).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(20.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.setGameType((int32_t) GameType::Survival);
            // the player sees these chunks, so what they are sent can be read
            for (int32_t x = 1; x <= 3; x++) {
                for (int32_t z = -1; z <= 1; z++)
                    player.getSentChunks().insert(((int64_t) x << 32) | (uint32_t) z);
            }
            mPlayer = &player;
        }

        BlockState block(const char *identifier) {
            const Block *value = VanillaBlocks::fromIdentifier(identifier);
            require(value != nullptr, std::string("no block ") + identifier);
            return value->toBlockState();
        }

        // a block on stone
        Vector3i place(int32_t x, const BlockState &state) {
            const Vector3i at(x, 100, 0);
            mLevel->setBlock(Vector3i(x, 99, 0), block("minecraft:stone"), false);
            mLevel->setBlock(at, state, false);
            return at;
        }

        BlockState at(const Vector3i &position) {
            return mLevel->getBlockState(position.x, position.y, position.z);
        }

        void hold(const char *identifier, int32_t count = 1) {
            mPlayer->getInventory().setItemInHand(identifier == nullptr ? ItemStack::air()
                                                                        : mOwner.createItemStack(identifier, count));
        }

        bool use(const Vector3i &position) {
            const BlockState state = at(position);
            return VanillaBlocks::fromState(state)->onInteract(mOwner, *mPlayer, position, state);
        }

        int32_t bites(const Vector3i &position) {
            return at(position).mStates.getInt("bite_counter", 0);
        }

        bool lit(const Vector3i &position) {
            return at(position).mStates.getByte("lit", 0) != 0;
        }

        // the smoke the server sends while it runs
        template<typename Action>
        std::vector<Vector3f> smokeOf(Action action) {
            std::vector<Vector3f> smoke;
            mOwner.getNetworkHandler().setOutboundFilter([&](const NetworkIdentifier &, std::string &data) {
                ReadOnlyBinaryStream header(data);
                unsigned char sender = 0, receiver = 0;
                if (Packet::peekId(header, sender, receiver) == MinecraftPacketIds::LevelEvent) {
                    ReadOnlyBinaryStream body(data);
                    LevelEventPacket packet;
                    packet.readHeader(body);
                    packet.read(body, mOwner.getCodecContext());
                    if (packet.mEventId == (0x4000 | 5))
                        smoke.push_back(packet.mPosition);
                }
                return false;
            });
            action();
            mOwner.getNetworkHandler().setOutboundFilter(nullptr);
            return smoke;
        }

        bool useItemOn(const Vector3i &position, int32_t face) {
            const ItemStack held = mPlayer->getInventory().getItemInHand();
            const Item *item = VanillaItems::fromIdentifier(held.mDefinition->getIdentifier());
            require(item != nullptr, "no item for what the player holds");
            return item->onUseOnBlock(mOwner, *mPlayer, held, position, face, Vector3f());
        }

        bool hitBy(const Vector3i &position, ServerActor &projectile) {
            const BlockState state = at(position);
            return VanillaBlocks::fromState(state)->onProjectileHit(mOwner, *mLevel, position, state, projectile);
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
    };

    // a hungry player eats a slice: two food, one bite; the seventh slice takes the cake away
    void eating(Arena &arena) {
        const Vector3i cake = arena.place(20, arena.block("minecraft:cake"));
        arena.mPlayer->setFood(10.0f);
        arena.hold(nullptr);
        arena.use(cake);
        require(arena.mPlayer->getFood() == 12.0f, "a slice did not give two food");
        require(arena.bites(cake) == 1, "a slice was not taken off the cake");

        for (int32_t slice = 2; slice <= 7; ++slice) {
            arena.mPlayer->setFood(0.0f);
            arena.use(cake);
        }
        require(arena.at(cake).mName == "minecraft:air", "the last slice did not take the cake away");
    }

    // a full player eats nothing, unless in creative or on peaceful
    void appetite(Arena &arena) {
        const Vector3i cake = arena.place(22, arena.block("minecraft:cake"));
        arena.mPlayer->setFood(20.0f);
        arena.hold(nullptr);
        arena.use(cake);
        require(arena.bites(cake) == 0, "a full player ate cake");

        arena.mPlayer->setGameType((int32_t) GameType::Creative);
        arena.use(cake);
        require(arena.bites(cake) == 1, "a creative player could not eat cake");
        arena.mPlayer->setGameType((int32_t) GameType::Survival);
    }

    void peaceful(const std::filesystem::path &directory) {
        Arena arena(directory, "peaceful");
        const Vector3i cake = arena.place(20, arena.block("minecraft:cake"));
        arena.mPlayer->setFood(20.0f);
        arena.hold(nullptr);
        arena.use(cake);
        require(arena.bites(cake) == 1, "a full player on peaceful could not eat cake");
    }

    // a candle tops a whole cake, of its own colour, used up; a bitten cake is eaten instead
    void candleOnCake(Arena &arena) {
        const Vector3i cake = arena.place(24, arena.block("minecraft:cake"));
        arena.mPlayer->setFood(10.0f);
        arena.hold("minecraft:red_candle", 2);
        arena.use(cake);
        require(arena.at(cake).mName == "minecraft:red_candle_cake", "a red candle did not make a red candle cake, "
                                                                           + arena.at(cake).mName);
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 1, "topping the cake did not use the candle");
        require(!arena.lit(cake), "a new candle cake is lit");

        const Vector3i bitten = arena.place(26, arena.block("minecraft:cake"));
        BlockState state = arena.at(bitten);
        state.mStates.putInt("bite_counter", 1);
        arena.mLevel->setBlock(bitten, state, false);
        arena.hold("minecraft:candle", 1);
        arena.use(bitten);
        require(arena.at(bitten).mName == "minecraft:cake", "a candle went on a bitten cake");
        require(arena.bites(bitten) == 2, "a candle in hand on a bitten cake did not eat a slice");
    }

    // flint and steel lights a candle cake once and wears; an empty hand puts it out
    void lighting(Arena &arena) {
        const Vector3i cake = arena.place(28, arena.block("minecraft:candle_cake"));
        arena.hold("minecraft:flint_and_steel", 1);
        arena.use(cake);
        require(arena.lit(cake), "flint and steel did not light the candle cake");
        require(arena.mPlayer->getInventory().getItemInHand().mDamage == 1, "lighting did not wear the flint");
        arena.use(cake);
        require(arena.mPlayer->getInventory().getItemInHand().mDamage == 1, "relighting a lit cake wore the flint");

        arena.hold(nullptr);
        arena.use(cake);
        require(!arena.lit(cake), "an empty hand did not put the candle cake out");
        require(arena.at(cake).mName == "minecraft:candle_cake", "putting it out ate the cake");
    }

    // anything else, from a hungry player, drops the candle and eats a slice of the cake left
    void eatingCandleCake(Arena &arena) {
        const Vector3i cake = arena.place(30, arena.block("minecraft:blue_candle_cake"));
        arena.mOwner.getItemEntities().clear();
        arena.mPlayer->setFood(10.0f);
        arena.hold("minecraft:stick", 1);
        arena.use(cake);
        require(arena.at(cake).mName == "minecraft:cake", "eating a candle cake left no cake");
        require(arena.bites(cake) == 1, "eating a candle cake took no slice");
        require(arena.dropped("minecraft:blue_candle") == 1, "eating a candle cake did not drop the candle");
    }

    // broken, a candle cake gives its candle; without a block under it, a cake goes and drops nothing
    void breaking(Arena &arena) {
        const Vector3i cake = arena.place(32, arena.block("minecraft:green_candle_cake"));
        arena.mOwner.getItemEntities().clear();
        BlockActionHandler::destroyBlock(arena.mOwner, *arena.mLevel, cake, arena.at(cake), true, ItemStack::air(),
                                         arena.mPlayer);
        require(arena.dropped("minecraft:green_candle") == 1, "a broken candle cake did not drop its candle");
        require(arena.dropped("minecraft:green_candle_cake") == 0, "a broken candle cake dropped itself");

        const Vector3i floating = arena.place(34, arena.block("minecraft:cake"));
        arena.mOwner.getItemEntities().clear();
        arena.mLevel->setBlock(Vector3i(34, 99, 0), BlockState("minecraft:air"), true);
        require(arena.at(floating).mName == "minecraft:air", "a cake with nothing under it stayed");
        require(arena.mOwner.getItemEntities().empty(), "a cake losing its floor dropped something");

        // the floor is the material's flag, not any block: glass holds a cake, water does not
        const Vector3i onGlass = arena.place(40, arena.block("minecraft:cake"));
        arena.mLevel->setBlock(Vector3i(40, 99, 0), arena.block("minecraft:glass"), true);
        require(arena.at(onGlass).mName == "minecraft:cake", "a cake fell off glass");
        arena.mLevel->setBlock(Vector3i(40, 99, 0), arena.block("minecraft:water"), true);
        require(arena.at(onGlass).mName == "minecraft:air", "a cake stood on water");
    }

    // candles light, wear the lighter, go out by hand, and never light under water
    void candles(Arena &arena) {
        const Vector3i candle = arena.place(36, arena.block("minecraft:candle"));
        arena.hold("minecraft:flint_and_steel", 1);
        arena.use(candle);
        require(arena.lit(candle), "flint and steel did not light the candle");
        arena.hold(nullptr);
        arena.use(candle);
        require(!arena.lit(candle), "an empty hand did not put the candle out");

        ItemStack sword = arena.mOwner.createItemStack("minecraft:diamond_sword", 1);
        ItemEnchantments::write(sword, {EnchantmentInstance{EnchantmentIds::FIRE_ASPECT, 1}});
        arena.mPlayer->getInventory().setItemInHand(sword);
        arena.use(candle);
        require(arena.lit(candle), "a Fire Aspect sword did not light the candle");

        const Vector3i wet = arena.place(38, arena.block("minecraft:candle"));
        arena.mLevel->setBlockStateAtLayer(wet.x, wet.y, wet.z, 1, arena.block("minecraft:water"));
        arena.hold("minecraft:flint_and_steel", 1);
        arena.use(wet);
        require(!arena.lit(wet), "a candle under water was lit");
    }

    bool near(const Vector3f &value, float x, float y, float z) {
        return std::abs(value.x - x) < 0.001f && std::abs(value.y - y) < 0.001f && std::abs(value.z - z) < 0.001f;
    }

    // a candle cake gives a whole cake's signal
    void comparator(Arena &arena) {
        const Vector3i cake = arena.place(42, arena.block("minecraft:candle_cake"));
        const BlockState state = arena.at(cake);
        require(VanillaBlocks::fromState(state)->getComparatorSignal(*arena.mLevel, cake, state) == 14,
                "a candle cake did not give 14");
    }

    // putting out sends one puff of smoke over each flame, and none for a candle never lit
    void smoke(Arena &arena) {
        const Vector3i candle = arena.place(44, arena.block("minecraft:candle"));
        BlockState three = arena.at(candle);
        three.mStates.putInt("candles", 2);
        three.mStates.putByte("lit", 1);
        arena.mLevel->setBlock(candle, three, false);
        arena.hold(nullptr);
        const std::vector<Vector3f> puffs = arena.smokeOf([&] { arena.use(candle); });
        require(puffs.size() == 3, "three candles did not puff three times, " + std::to_string(puffs.size()));
        require(near(puffs[0], 44.5625f, 100.5f, 0.4375f) && near(puffs[1], 44.375f, 100.4375f, 0.5f)
                        && near(puffs[2], 44.5f, 100.3125f, 0.625f),
                "the smoke of three candles was not over their flames");

        const Vector3i single = arena.place(46, arena.block("minecraft:candle"));
        BlockState one = arena.at(single);
        one.mStates.putByte("lit", 1);
        arena.mLevel->setBlock(single, one, false);
        const std::vector<Vector3f> lone = arena.smokeOf([&] { arena.use(single); });
        require(lone.size() == 1 && near(lone[0], 46.5f, 100.5f, 0.5f), "one candle did not puff over its flame");
        require(arena.smokeOf([&] { arena.use(single); }).empty(), "a candle never lit puffed smoke");

        const Vector3i cake = arena.place(48, arena.block("minecraft:candle_cake"));
        BlockState lit = arena.at(cake);
        lit.mStates.putByte("lit", 1);
        arena.mLevel->setBlock(cake, lit, false);
        const std::vector<Vector3f> cakePuff = arena.smokeOf([&] { arena.use(cake); });
        require(cakePuff.size() == 1 && near(cakePuff[0], 48.5f, 100.975f, 0.5f),
                "a candle cake did not puff over its candle");
    }

    // a wind burst puts out a lit candle or candle cake with its smoke, and leaves an unlit one be
    void wind(Arena &arena) {
        const Vector3i candle = arena.place(58, arena.block("minecraft:candle"));
        BlockState lit = arena.at(candle);
        lit.mStates.putByte("lit", 1);
        arena.mLevel->setBlock(candle, lit, false);
        bool reacted = false;
        const std::vector<Vector3f> puffs = arena.smokeOf([&] {
            reacted = VanillaBlocks::fromState(arena.at(candle))
                              ->onWindBurst(arena.mOwner, *arena.mLevel, candle, arena.at(candle));
        });
        require(reacted && !arena.lit(candle), "a wind burst did not put the candle out");
        require(puffs.size() == 1, "the wind put the candle out without its smoke");
        require(!VanillaBlocks::fromState(arena.at(candle))
                         ->onWindBurst(arena.mOwner, *arena.mLevel, candle, arena.at(candle)),
                "a wind burst reacted to an unlit candle");

        const Vector3i cake = arena.place(60, arena.block("minecraft:candle_cake"));
        BlockState litCake = arena.at(cake);
        litCake.mStates.putByte("lit", 1);
        arena.mLevel->setBlock(cake, litCake, false);
        VanillaBlocks::fromState(arena.at(cake))->onWindBurst(arena.mOwner, *arena.mLevel, cake, arena.at(cake));
        require(!arena.lit(cake) && arena.at(cake).mName == "minecraft:candle_cake",
                "a wind burst did not put the candle cake out");
    }

    // a fire charge lights a candle or a candle cake and is used up; a lit one keeps the charge and sets no fire
    void fireCharge(Arena &arena) {
        const Vector3i candle = arena.place(50, arena.block("minecraft:candle"));
        const Vector3i above(candle.x, candle.y + 1, candle.z);
        arena.hold("minecraft:fire_charge", 3);
        require(arena.useItemOn(candle, 1), "a fire charge did nothing on a candle");
        require(arena.lit(candle), "a fire charge did not light the candle");
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 2, "lighting a candle kept the fire charge");
        require(arena.at(above).mName == "minecraft:air", "a fire charge on a candle set a fire");

        require(!arena.useItemOn(candle, 1), "a fire charge did something on a lit candle");
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 2, "a lit candle took the fire charge");
        require(arena.at(above).mName == "minecraft:air", "a fire charge on a lit candle set a fire");

        const Vector3i cake = arena.place(52, arena.block("minecraft:candle_cake"));
        require(arena.useItemOn(cake, 1) && arena.lit(cake), "a fire charge did not light the candle cake");
        require(arena.mPlayer->getInventory().getItemInHand().mCount == 1, "lighting a candle cake kept the charge");
    }

    // a burning arrow lights what it hits and flies on; a cold one lights nothing
    void projectiles(Arena &arena) {
        const Vector3i candle = arena.place(54, arena.block("minecraft:candle"));
        ServerActor *arrow = arena.mOwner.spawnActor(*arena.mLevel, "minecraft:arrow", Vector3f(54.5f, 102.0f, 0.5f));
        require(arrow != nullptr, "no arrow spawned");
        require(!arena.hitBy(candle, *arrow) && !arena.lit(candle), "a cold arrow lit the candle");

        arrow->getProjectileData().mFlameTicks = 100;
        require(!arena.hitBy(candle, *arrow), "a burning arrow stopped on the candle");
        require(arena.lit(candle), "a Flame arrow did not light the candle");

        const Vector3i cake = arena.place(56, arena.block("minecraft:candle_cake"));
        arrow->getProjectileData().mFlameTicks = 0;
        arrow->setOnFire(true);
        require(!arena.hitBy(cake, *arrow), "a burning arrow stopped on the candle cake");
        require(arena.lit(cake), "an arrow on fire did not light the candle cake");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-cakes-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        {
            Arena arena(directory, "normal");
            eating(arena);
            appetite(arena);
            candleOnCake(arena);
            lighting(arena);
            eatingCandleCake(arena);
            breaking(arena);
            candles(arena);
            comparator(arena);
            smoke(arena);
            fireCharge(arena);
            projectiles(arena);
            wind(arena);
        }
        // a second arena only once the first has closed the world
        peaceful(directory);
    } catch (const std::exception &error) {
        std::cout << "Cake regression failed: " << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
