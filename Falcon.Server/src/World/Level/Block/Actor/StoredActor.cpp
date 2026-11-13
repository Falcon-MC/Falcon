#include "World/Level/Block/Actor/StoredActor.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Mob/MobActor.h"

ServerActor *StoredActor::restore(ServerNetworkHandler &owner, Level &level, const std::string &identifier,
                                  const Tag &saved, const Vector3f &position,
                                  const std::function<void(ServerActor &)> &configure) {
    ServerActor *actor = owner.spawnActor(level, identifier, position,
                                          [&owner, &saved, &configure](ServerActor &spawned) {
                                              spawned.loadNbt(saved);
                                              if (MobActor *mob = dynamic_cast<MobActor *>(&spawned))
                                                  mob->getEquipment().loadNbt(saved, owner.getCodecContext());
                                              if (configure)
                                                  configure(spawned);
                                          });
    if (MobActor *mob = dynamic_cast<MobActor *>(actor))
        mob->setHealth(saved.getFloat("Health", mob->getHealth()));
    return actor;
}
