#include "Network/ServerNetworkHandler.h"
#include "World/Actor/ActorDamageSource.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/World.h"

#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
    void require(bool condition, const std::string &message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    struct Arena {
        ServerNetworkHandler mOwner{"Ghast", "", 1};
        Level &mLevel = mOwner.getWorlds().getDefault().getLevel(DimensionType::Nether);

        Arena() {
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++)
                    mLevel.insertChunk(LevelChunk(x, z)).setPopulated(true);
            }
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(mLevel);
            player.setPosition(Vector3f(0.5f, 64.0f, 0.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            return player;
        }

        ServerActor &spawn(const char *identifier, const Vector3f &position) {
            ServerActor *actor = mOwner.spawnActor(mLevel, identifier, position);
            require(actor != nullptr, identifier);
            if (MobActor *mob = dynamic_cast<MobActor *>(actor))
                mob->startDefinition(mOwner);
            return *actor;
        }

        // the items the kill drops, by identifier
        std::vector<std::string> killDrops(ServerActor &victim, const ActorDamageSource &source) {
            ServerNetworkHandler::DropCapture capture;
            ServerNetworkHandler::DropCapture *previous = mOwner.setDropCapture(&capture);
            const bool hurt = victim.hurt(mOwner, 1000.0f, source);
            mOwner.setDropCapture(previous);
            require(hurt && victim.isDead(), std::string(victim.getIdentifier()) + " did not die");

            std::vector<std::string> names;
            for (const ItemStack &item: capture.mItems) {
                if (item.mDefinition != nullptr)
                    names.push_back(item.mDefinition->getIdentifier());
            }
            return names;
        }
    };

    bool hasDisc(const std::vector<std::string> &names) {
        for (const std::string &name: names) {
            if (name.find("music_disc") != std::string::npos)
                return true;
        }
        return false;
    }

    bool hasTears(const std::vector<std::string> &names) {
        for (const std::string &name: names) {
            if (name == "minecraft:music_disc_tears")
                return true;
        }
        return false;
    }

    ActorDamageSource projectileHit(Actor &victim, Actor *owner, Actor &projectile) {
        ActorDamageSource source = ActorDamageSource::environment("death.attack.fireball", victim.getName());
        source.mAttacker = owner;
        return source.asProjectile().withDamager(&projectile);
    }

    // the tears disc wants a player kill through a fireball: the ghast's own fireball sent back
    void tearsFromReflectedFireball() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        ServerActor &ghast = arena.spawn("minecraft:ghast", Vector3f(0.5f, 80.0f, 10.5f));
        ServerActor &fireball = arena.spawn("minecraft:fireball", Vector3f(0.5f, 80.0f, 8.5f));
        fireball.setOwnerUniqueId(player.getUniqueId());
        require(hasTears(arena.killDrops(ghast, projectileHit(ghast, &player, fireball))),
                "a ghast killed by a player's fireball did not drop the tears disc");
    }

    void noTearsFromAnArrow() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        ServerActor &ghast = arena.spawn("minecraft:ghast", Vector3f(0.5f, 80.0f, 10.5f));
        ServerActor &arrow = arena.spawn("minecraft:arrow", Vector3f(0.5f, 80.0f, 8.5f));
        require(!hasTears(arena.killDrops(ghast, projectileHit(ghast, &player, arrow))),
                "a ghast shot with an arrow dropped the tears disc");
    }

    void noTearsFromABlow() {
        Arena arena;
        ServerPlayer &player = arena.player(1);
        ServerActor &ghast = arena.spawn("minecraft:ghast", Vector3f(0.5f, 64.0f, 2.5f));
        const ActorDamageSource blow = ActorDamageSource::attack("death.attack.player", ghast.getName(), player, player.getName(), player.getPosition());
        require(!hasTears(arena.killDrops(ghast, blow)), "a ghast killed by a blow dropped the tears disc");
    }

    // a fireball nobody sent back is the ghast's own: no player kill
    void noTearsWithoutPlayer() {
        Arena arena;
        ServerActor &shooter = arena.spawn("minecraft:ghast", Vector3f(10.5f, 80.0f, 10.5f));
        ServerActor &ghast = arena.spawn("minecraft:ghast", Vector3f(0.5f, 80.0f, 10.5f));
        ServerActor &fireball = arena.spawn("minecraft:fireball", Vector3f(0.5f, 80.0f, 8.5f));
        require(!hasTears(arena.killDrops(ghast, projectileHit(ghast, &shooter, fireball))),
                "a ghast killed by another ghast's fireball dropped the tears disc");
    }

    // killed_by_entity reads the owner of the fatal hit: a skeleton's arrow
    void creeperDiscFromSkeleton() {
        Arena arena;
        ServerActor &skeleton = arena.spawn("minecraft:skeleton", Vector3f(0.5f, 64.0f, 10.5f));
        ServerActor &creeper = arena.spawn("minecraft:creeper", Vector3f(0.5f, 64.0f, 2.5f));
        ServerActor &arrow = arena.spawn("minecraft:arrow", Vector3f(0.5f, 64.0f, 3.5f));
        require(hasDisc(arena.killDrops(creeper, projectileHit(creeper, &skeleton, arrow))),
                "a creeper shot by a skeleton did not drop a music disc");

        ServerPlayer &player = arena.player(1);
        ServerActor &other = arena.spawn("minecraft:creeper", Vector3f(4.5f, 64.0f, 2.5f));
        const ActorDamageSource blow = ActorDamageSource::attack("death.attack.player", other.getName(), player, player.getName(), player.getPosition());
        require(!hasDisc(arena.killDrops(other, blow)), "a creeper killed by a player dropped a music disc");
    }
}

int main() {
    try {
        EntityDefinitions::initialize();
        tearsFromReflectedFireball();
        noTearsFromAnArrow();
        noTearsFromABlow();
        noTearsWithoutPlayer();
        creeperDiscFromSkeleton();
    } catch (const std::exception &error) {
        std::cout << "FAILED: " << error.what() << std::endl;
        return 1;
    }

    std::cout << "ghast tests passed" << std::endl;
    return 0;
}
