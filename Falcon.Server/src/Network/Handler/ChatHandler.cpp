#include "Network/Handler/ChatHandler.h"

#include "Core/Debug/BedrockLog.h"
#include "Core/Text/StringUtil.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "Network/Handler/NetworkHandler.h"
#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/TextPacket.h"

void ChatHandler::broadcastChat(ServerNetworkHandler &owner, ServerPlayer &player, const std::string &message) {
    PlayerChatBeforeEvent before(player, message);
    owner.getEventBus().before().mChatSend.emit(before);
    if (before.isCancelled())
        return;

    const std::string &finalMessage = before.mMessage;

    LOG_INFO(LogAreaID::Server, "<%s> %s", player.getName().c_str(), StringUtil::printable(finalMessage, 512).c_str());

    const ServerPlayer::ChatStyle &style = player.getChatStyle();
    TextPacket chat;
    chat.mType = TextPacket::Type::Chat;
    chat.mSourceName = style.mNamePrefix + player.getName() + style.mNameSuffix;
    chat.mMessage = style.mMessagePrefix + finalMessage;

    for (auto &entry: owner.getPlayers()) {
        if (entry.second.isSpawned())
            owner.getNetworkHandler().send(entry.second.getNetworkIdentifier(), chat, owner.getCodecContext());
    }

    PlayerChatAfterEvent after(player, finalMessage);
    owner.getEventBus().after().mChatSend.emit(after);
}
