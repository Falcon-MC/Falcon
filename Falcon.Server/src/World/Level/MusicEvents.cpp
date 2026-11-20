#include "World/Level/MusicEvents.h"

#include "Core/NBT/Tag.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/LevelEventGenericPacket.h"
#include "World/Actor/Player/ServerPlayer.h"

Tag MusicEvents::trackTag(const std::string &trackName, float volume, float fadeSeconds, bool loop) {
    Tag tag = Tag::ofCompound();
    tag.putString("trackName", trackName);
    tag.putFloat("volume", volume);
    tag.putFloat("fadeSeconds", fadeSeconds);
    tag.putByte("repeatMode", loop ? 1 : 0);
    return tag;
}

void MusicEvents::send(ServerNetworkHandler &owner, ServerPlayer *player, int32_t event, const Tag &data) {
    LevelEventGenericPacket packet;
    packet.mEventId = event;
    packet.mData = data;
    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned() && (player == nullptr || &entry.second == player))
            owner.sendPacketTo(entry.first, packet);
    }
}
