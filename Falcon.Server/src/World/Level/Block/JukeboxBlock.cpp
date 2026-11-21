#include "World/Level/Block/JukeboxBlock.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/TextPacket.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Level/Block/Actor/BlockActorStore.h"
#include "World/Level/Block/Actor/JukeboxBlockActor.h"
#include "World/Level/Block/BlockClassRegistry.h"
#include "World/Level/Block/BlockNames.h"
#include "World/Level/Block/Systems/RedstoneSystem.h"
#include "World/Level/Level.h"

FALCON_REGISTER_BLOCK(JukeboxBlock, 100);

bool JukeboxBlock::matches(const std::string &identifier) {
    return identifier == BlockNames::JUKEBOX;
}

const char *JukeboxBlock::getBlockActorId() const {
    return JukeboxBlockActor::BLOCK_ACTOR_ID;
}

bool JukeboxBlock::onInteract(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                              const BlockState &state) const {
    (void) state;
    Level &level = owner.getLevelFor(player);
    JukeboxBlockActor &jukebox = level.getBlockActors().getOrCreate<JukeboxBlockActor>(position);
    if (!jukebox.getRecord().isAir()) {
        jukebox.eject(owner);
        return true;
    }

    const ItemStack held = player.getInventory().getItemInHand();
    const JukeboxBlockActor::Song *song = JukeboxBlockActor::findSong(held);
    if (song == nullptr)
        return false;

    jukebox.setRecord(held, true);
    jukebox.play(owner);
    player.consumeOneHeldItem();

    // the "now playing" banner goes to the player who put the record in
    TextPacket popup;
    popup.mType = TextPacket::Type::JukeboxPopup;
    popup.mNeedsTranslation = true;
    popup.mMessage = "record.nowPlaying";
    popup.mParameters = {std::string("%item.record_") + song->mDisc + ".desc"};
    owner.sendPacketTo(player.getNetworkIdentifier(), popup);
    return true;
}

void JukeboxBlock::onPlaced(ServerNetworkHandler &owner, ServerPlayer &player, const Vector3i &position,
                            const BlockState &state, const ItemStack &usedItem, int blockFace) const {
    (void) state;
    (void) usedItem;
    (void) blockFace;
    owner.getLevelFor(player).getBlockActors().getOrCreate<JukeboxBlockActor>(position);
}

int JukeboxBlock::getComparatorSignal(Level &level, const Vector3i &position, const BlockState &state) const {
    (void) state;
    const JukeboxBlockActor *jukebox = level.getBlockActors().find<JukeboxBlockActor>(position);
    return jukebox == nullptr ? 0 : jukebox->getComparatorSignal();
}

int JukeboxBlock::getEmittedSignal(Level &level, const Vector3i &position, const BlockState &state, int face,
                                   bool strong) const {
    (void) state;
    (void) face;
    if (strong)
        return 0;
    const JukeboxBlockActor *jukebox = level.getBlockActors().find<JukeboxBlockActor>(position);
    return jukebox != nullptr && jukebox->isPlaying() ? RedstoneSystem::MAX_SIGNAL : 0;
}
