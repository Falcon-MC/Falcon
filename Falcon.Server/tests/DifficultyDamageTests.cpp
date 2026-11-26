#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/AI/Goal/MobAttack.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/DamageCause.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Difficulty damage", "", 1};
        Level *mLevel = nullptr;

        Arena(const std::filesystem::path &directory, const char *difficulty) {
            const std::filesystem::path path = directory / (std::string(difficulty) + ".properties");
            std::ofstream(path) << "difficulty=" << difficulty << "\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -1; x <= 1; x++) {
                for (int32_t z = -1; z <= 1; z++) {
                    // the opened world has generated the chunk its random seed spawns in, and an existing chunk wins
                    // the insert
                    mLevel->dropChunk(x, z);
                    mLevel->insertChunk(LevelChunk(x, z)).setPopulated(true);
                }
            }
        }

        MobActor &spawn(const char *identifier, const Vector3f &position) {
            MobActor *mob = dynamic_cast<MobActor *>(mOwner.spawnActor(*mLevel, identifier, position));
            require(mob != nullptr, std::string(identifier) + " did not spawn");
            return *mob;
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(1.5f, 100.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }

        float hitBy(ServerPlayer &player, Actor &attacker, float damage, bool projectile) {
            player.setHealth(20.0f);
            player.setNoDamageTicks(0);
            ActorDamageSource source = ActorDamageSource::attack("death.attack.mob", player.getName(), attacker,
                                                                 attacker.getName(), attacker.getPosition());
            if (projectile)
                source.asProjectile();
            mOwner.hurt(player, damage, source);
            return 20.0f - player.getHealth();
        }
    };

    void formula() {
        require(DamageCause::scaleForDifficulty(3.0f, Difficulty::Peaceful) == 0.0f, "peaceful kept the damage");
        require(DamageCause::scaleForDifficulty(3.0f, Difficulty::Easy) == 2.5f, "easy 3 is not 2.5");
        require(DamageCause::scaleForDifficulty(6.0f, Difficulty::Easy) == 4.0f, "easy 6 is not 4");
        require(DamageCause::scaleForDifficulty(2.0f, Difficulty::Easy) == 2.0f, "easy 2 changed");
        require(DamageCause::scaleForDifficulty(1.0f, Difficulty::Easy) == 1.0f, "easy 1 raised");
        require(DamageCause::scaleForDifficulty(3.0f, Difficulty::Normal) == 3.0f, "normal changed");
        require(DamageCause::scaleForDifficulty(3.0f, Difficulty::Hard) == 4.5f, "hard 3 is not 4.5");
    }

    void zombieHits(const std::filesystem::path &directory) {
        const struct {
            Difficulty mDifficulty;
            float mDealt;
        } CASES[] = {{Difficulty::Easy, 2.5f},
                     {Difficulty::Normal, 3.0f},
                     {Difficulty::Hard, 4.5f},
                     {Difficulty::Peaceful, 0.0f}};

        // one zombie through every difficulty: a change applies to the next hit
        Arena arena(directory, "normal");
        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(0.5f, 100.0f, 0.5f));
        ServerPlayer &player = arena.player(9001);
        for (const auto &entry: CASES) {
            arena.mOwner.changeDifficulty(entry.mDifficulty);
            const std::string name = std::to_string((int) entry.mDifficulty);
            player.setHealth(20.0f);
            player.setNoDamageTicks(0);
            MobAttack::hit(arena.mOwner, zombie, player, zombie.getAttackDamage());
            require(20.0f - player.getHealth() == entry.mDealt,
                    "zombie hit on difficulty " + name + " dealt " + std::to_string(20.0f - player.getHealth()));
            // a hit scaled to nothing still lands: it starts the hurt window like any 0 hit
            require(player.getNoDamageTicks() > 0, "zombie hit on difficulty " + name + " did not land");

            // the scaling sits in the player's hurt: peaceful zeroes a hit handed straight to it too
            require(arena.hitBy(player, zombie, 3.0f, false) == entry.mDealt,
                    "mob source on difficulty " + name + " not scaled");
        }
    }

    void unscaledSources(const std::filesystem::path &directory) {
        Arena arena(directory, "hard");
        MobActor &skeleton = arena.spawn("minecraft:skeleton", Vector3f(0.5f, 100.0f, 0.5f));
        MobActor &evoker = arena.spawn("minecraft:evocation_illager", Vector3f(0.5f, 100.0f, 1.5f));
        ServerPlayer &player = arena.player(9001);
        ServerPlayer &other = arena.player(9002);

        require(arena.hitBy(player, skeleton, 3.0f, true) == 3.0f, "a mob's projectile was scaled");
        require(arena.hitBy(player, evoker, 6.0f, false) == 6.0f, "evoker fangs were scaled");
        require(arena.hitBy(player, other, 3.0f, false) == 3.0f, "a player's hit was scaled");

        // the game scales in the player's hurt only: a mob hitting a mob keeps its damage
        MobActor &zombie = arena.spawn("minecraft:zombie", Vector3f(-0.5f, 100.0f, 0.5f));
        MobActor &villager = arena.spawn("minecraft:villager_v2", Vector3f(-1.5f, 100.0f, 0.5f));
        const float before = villager.getHealth();
        MobAttack::hit(arena.mOwner, zombie, villager, zombie.getAttackDamage());
        require(before - villager.getHealth() == 3.0f, "zombie hit on a villager was scaled");

        // peaceful included: only a player's hurt is scaled
        arena.mOwner.changeDifficulty(Difficulty::Peaceful);
        villager.setNoDamageTicks(0);
        const float healthy = villager.getHealth();
        MobAttack::hit(arena.mOwner, zombie, villager, zombie.getAttackDamage());
        require(healthy - villager.getHealth() == 3.0f, "zombie hit on a villager was zeroed on peaceful");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-difficulty-damage-"
               + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        formula();
        zombieHits(directory);
        unscaledSources(directory);
        std::cout << "difficulty damage checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
