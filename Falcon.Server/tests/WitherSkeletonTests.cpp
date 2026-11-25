#include "Network/ServerNetworkHandler.h"
#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    const int32_t TICKS_PER_SECOND = 20;

    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Wither skeletons", "", 1};
        Level &mLevel;

        explicit Arena(DimensionType dimension) : mLevel(mOwner.getWorlds().getDefault().getLevel(dimension)) {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
            for (int32_t x = -8; x <= 8; x++) {
                for (int32_t z = -8; z <= 8; z++)
                    mLevel.setBlockState(x, 63, z, BlockState("minecraft:stone"));
            }
        }

        ServerPlayer &player(uint64_t runtimeId, const Vector3f &position) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(position);
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &spawn(const char *identifier, const Vector3f &position, const char *event) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(mLevel, identifier, position,
                    [event](ServerActor &actor) { static_cast<MobActor &>(actor).setSpawnEvent(event); }));
            require(mob != nullptr, identifier);
            mob->startDefinition(mOwner);
            return *mob;
        }
    };

    MobActor &skeleton(Arena &arena, const Vector3f &position) {
        return arena.spawn("minecraft:wither_skeleton", position, "minecraft:entity_spawned");
    }

    void gear() {
        Arena arena(DimensionType::Nether);
        MobActor &mob = skeleton(arena, Vector3f(0.5f, 64.0f, 0.5f));
        const ItemStack &sword = mob.getEquipment().getSlot(MobEquipment::MAINHAND);
        require(!sword.isAir() && sword.mDefinition->getIdentifier() == "minecraft:stone_sword",
                "a wither skeleton spawned without its stone sword");
        require(mob.getComponent("minecraft:fire_immune") != nullptr,
                "a wither skeleton burns");
    }

    // effect_name wither for effect_duration 10 s, amplifier 0
    void witherOnHit() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 1.5f));
        MobActor &mob = skeleton(arena, Vector3f(0.5f, 64.0f, 0.5f));
        require(MobAttack::hit(arena.mOwner, mob, player, 4.0f), "the wither skeleton missed");
        const MobEffectInstance *wither = player.getEffect(MobEffectId::Wither);
        require(wither != nullptr, "the blow did not wither the player");
        require(wither->mDuration == 10 * TICKS_PER_SECOND && wither->mAmplifier == 0,
                "wither for " + std::to_string(wither->mDuration) + " ticks at " + std::to_string(wither->mAmplifier));
    }

    // a player kill rolls random_chance_with_looting 0.025 + 0.02 per looting level
    int32_t skulls(Arena &arena, const ServerPlayer *killer, int32_t looting, int32_t kills) {
        int32_t found = 0;
        for (int32_t kill = 0; kill < kills; kill++) {
            MobActor &mob = skeleton(arena, Vector3f(0.5f, 64.0f, 2.5f));
            ServerNetworkHandler::DropCapture capture;
            arena.mOwner.setDropCapture(&capture);
            mob.dropLoot(arena.mOwner, arena.mLevel, killer, looting);
            arena.mOwner.setDropCapture(nullptr);
            for (const ItemStack &item: capture.mItems) {
                if (item.mDefinition != nullptr && item.mDefinition->getIdentifier() == "minecraft:wither_skeleton_skull")
                    ++found;
            }
            arena.mOwner.removeActor(mob.getUniqueId());
        }
        return found;
    }

    void skullDrops() {
        Arena arena(DimensionType::Nether);
        ServerPlayer &player = arena.player(1, Vector3f(0.5f, 64.0f, 0.5f));
        const int32_t kills = 4000;
        require(skulls(arena, nullptr, 3, 400) == 0, "a skull dropped without a player kill");
        const int32_t plain = skulls(arena, &player, 0, kills);
        require(plain > kills * 0.012 && plain < kills * 0.04,
                "the skull rate without looting is not 2.5%: " + std::to_string(plain) + "/" + std::to_string(kills));
        const int32_t looted = skulls(arena, &player, 3, kills);
        require(looted > kills * 0.06 && looted < kills * 0.11,
                "the skull rate with looting III is not 8.5%: " + std::to_string(looted) + "/" + std::to_string(kills));
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        gear();
        witherOnHit();
        skullDrops();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "wither skeleton tests passed" << std::endl;
    return 0;
}
