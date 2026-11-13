#include "Network/ServerNetworkHandler.h"

#include "Core/Debug/BedrockLog.h"
#include "Protocol/Packets/RemoveObjectivePacket.h"
#include "Protocol/Packets/SetDisplayObjectivePacket.h"
#include "Protocol/Packets/SetScorePacket.h"
#include "Protocol/Packets/SetScoreboardIdentityPacket.h"

#include <stdexcept>

namespace {
    SetDisplayObjectivePacket displayPacket(const std::string &slot, const ScoreboardObjective *objective,
                                            ObjectiveSortOrder order) {
        SetDisplayObjectivePacket packet;
        packet.mDisplaySlot = slot;
        // sub_1405DBDE0: a cleared slot sends empty strings and sort order 0
        packet.mObjectiveId = objective == nullptr ? std::string() : objective->mName;
        packet.mDisplayName = objective == nullptr ? std::string() : objective->mDisplayName;
        packet.mCriteria = objective == nullptr ? std::string() : objective->mCriteria;
        packet.mSortOrder = objective == nullptr ? 0 : (int32_t) order;
        return packet;
    }

    ScoreInfoEntry scoreEntry(const ScoreboardObjective &objective, const ScoreboardIdentity &identity, int32_t score,
                              const std::string &name) {
        ScoreInfoEntry info;
        info.mScoreboardId = identity.mId;
        info.mObjectiveId = objective.mName;
        info.mScore = score;
        if (identity.mType == ScoreboardIdentityType::Player) {
            info.mType = ScorerType::Player;
            info.mActorId = identity.mActorId;
        } else if (identity.mType == ScoreboardIdentityType::Entity) {
            info.mType = ScorerType::Entity;
            info.mActorId = identity.mActorId;
        } else {
            info.mType = ScorerType::Fake;
            info.mName = name;
        }
        return info;
    }

    // every score of the objective, in one packet
    SetScorePacket scoresPacket(ServerNetworkHandler &owner, const ScoreboardObjective &objective) {
        SetScorePacket packet;
        packet.mAction = SetScorePacket::Action::Change;
        for (const auto &score: objective.mScores) {
            const ScoreboardIdentity *identity = owner.getScoreboard().findIdentity(score.first);
            if (identity != nullptr)
                packet.mInfos.push_back(scoreEntry(objective, *identity, score.second, identity->mFakeName));
        }
        return packet;
    }
}

void ServerNetworkHandler::_bindScoreboard() {
    const auto broadcast = [this](const Packet &packet) {
        if (mNetworkHandler == nullptr)
            return;
        for (auto &entry: mPlayers) {
            if (entry.second.isSpawned())
                mNetworkHandler->send(entry.first, packet, mCodecContext);
        }
    };

    Scoreboard::Listener listener;
    // clients only track the objectives shown in a slot
    listener.mScoreChanged = [this, broadcast](const ScoreboardObjective &objective, const ScoreboardIdentity &identity,
                                               int32_t score) {
        if (!mScoreboard.isDisplayed(objective.mName))
            return;
        SetScorePacket packet;
        packet.mAction = SetScorePacket::Action::Change;
        packet.mInfos.push_back(scoreEntry(objective, identity, score, identity.mFakeName));
        broadcast(packet);
    };
    listener.mScoreRemoved = [this, broadcast](const ScoreboardObjective &objective,
                                               const ScoreboardIdentity &identity) {
        if (!mScoreboard.isDisplayed(objective.mName))
            return;
        ScoreInfoEntry info;
        info.mScoreboardId = identity.mId;
        info.mObjectiveId = objective.mName;
        info.mType = ScorerType::Invalid;
        SetScorePacket packet;
        packet.mAction = SetScorePacket::Action::Remove;
        packet.mInfos.push_back(info);
        broadcast(packet);
    };
    listener.mObjectiveRemoved = [broadcast](const std::string &objective) {
        RemoveObjectivePacket packet;
        packet.mObjectiveId = objective;
        broadcast(packet);
    };
    listener.mDisplayChanged = [this, broadcast](const std::string &slot, const ScoreboardObjective *objective,
                                                 ObjectiveSortOrder order) {
        broadcast(displayPacket(slot, objective, order));
        if (objective != nullptr && !objective->mScores.empty())
            broadcast(scoresPacket(*this, *objective));
    };
    listener.mIdentityChanged = [broadcast](const ScoreboardIdentity &identity, bool added) {
        if (identity.mType != ScoreboardIdentityType::Player)
            return;
        SetScoreboardIdentityPacket packet;
        packet.mAction = added ? SetScoreboardIdentityPacket::Action::Add : SetScoreboardIdentityPacket::Action::Remove;
        ScoreboardIdentityEntry entry;
        entry.mScoreboardId = identity.mId;
        entry.mPlayerId = identity.mActorId;
        packet.mEntries.push_back(entry);
        broadcast(packet);
    };
    mScoreboard.setListener(std::move(listener));
}

void ServerNetworkHandler::loadScoreboard() {
    mScoreboardUnreadable = false;
    Level &overworld = mWorlds.getDefault().getOverworld();
    Tag saved;
    if (!overworld.loadSavedData(Scoreboard::STORAGE_KEY, saved, false)) {
        // unreadable is not absent: an empty save would replace the only copy
        if (overworld.hasSavedData(Scoreboard::STORAGE_KEY)) {
            mScoreboardUnreadable = true;
            LOG_WARN(LogAreaID::Server, "The stored scoreboard is unreadable: kept as is, not saved this run");
        }
        return;
    }
    try {
        mScoreboard.load(saved);
    } catch (const std::exception &exception) {
        mScoreboard.clear();
        mScoreboardUnreadable = true;
        LOG_WARN(LogAreaID::Server, "Malformed scoreboard data, kept as is and not saved this run: %s",
                 exception.what());
    }
}

void ServerNetworkHandler::saveScoreboard(bool checked) {
    Level &overworld = mWorlds.getDefault().getOverworld();
    if (!overworld.isStorageOpen() || mScoreboardUnreadable)
        return;
    if (!overworld.saveSavedData(Scoreboard::STORAGE_KEY, mScoreboard.save(), false) && checked)
        throw std::runtime_error("Could not save the scoreboard");
}

void ServerNetworkHandler::sendScoreboard(ServerPlayer &player) {
    if (mNetworkHandler == nullptr)
        return;

    SetScoreboardIdentityPacket identities;
    identities.mAction = SetScoreboardIdentityPacket::Action::Add;
    for (const ScoreboardIdentity &identity: mScoreboard.getIdentities()) {
        if (identity.mType != ScoreboardIdentityType::Player)
            continue;
        ScoreboardIdentityEntry entry;
        entry.mScoreboardId = identity.mId;
        entry.mPlayerId = identity.mActorId;
        identities.mEntries.push_back(entry);
    }
    if (!identities.mEntries.empty())
        mNetworkHandler->send(player.getNetworkIdentifier(), identities, mCodecContext);

    for (const ScoreboardDisplay &display: mScoreboard.getDisplays()) {
        const ScoreboardObjective *objective = mScoreboard.getObjective(display.mObjective);
        if (objective == nullptr)
            continue;
        mNetworkHandler->send(player.getNetworkIdentifier(),
                              displayPacket(display.mSlot, objective, display.mSortOrder), mCodecContext);
        if (!objective->mScores.empty())
            mNetworkHandler->send(player.getNetworkIdentifier(), scoresPacket(*this, *objective), mCodecContext);
    }
}

std::string ServerNetworkHandler::getScoreboardName(const ScoreboardIdentity &identity, bool script) {
    if (identity.mType == ScoreboardIdentityType::FakePlayer)
        return identity.mFakeName;
    // sub_14BE520A0, sub_14B9D7950: the loaded actor's name string, empty otherwise
    std::string name;
    if (Actor *actor = findScoreboardActor(identity)) {
        const ServerActor *entity = dynamic_cast<const ServerActor *>(actor);
        name = entity != nullptr ? entity->getNameTag() : actor->getName();
    }
    // the client translates a leading %: the game's own string, sub_14BDDC010
    if (name.empty() && !script)
        return "%commands.scoreboard.players.offlinePlayerName";
    return name;
}

Actor *ServerNetworkHandler::findScoreboardActor(const ScoreboardIdentity &identity) {
    if (identity.mType == ScoreboardIdentityType::Player) {
        for (auto &entry: mPlayers) {
            if (entry.second.getUniqueId() == identity.mActorId && entry.second.isSpawned())
                return &entry.second;
        }
        return nullptr;
    }
    if (identity.mType != ScoreboardIdentityType::Entity)
        return nullptr;
    if (ServerActor *actor = getActor(identity.mActorId))
        return actor->isDead() ? nullptr : actor;
    for (const std::unique_ptr<ItemActor> &item: mItemEntities) {
        if (item->getUniqueId() == identity.mActorId && !item->isRemoved())
            return item.get();
    }
    return nullptr;
}

const ScoreboardIdentity *ServerNetworkHandler::findScoreboardIdentity(const Actor &actor) const {
    return mScoreboard.findActor(actor.getUniqueId());
}

int64_t ServerNetworkHandler::createScoreboardIdentity(const Actor &actor) {
    return mScoreboard.createActor(actor.getUniqueId(), dynamic_cast<const ServerPlayer *>(&actor) != nullptr).mId;
}
