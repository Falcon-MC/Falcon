#include "World/Scores/Scoreboard.h"

#include <algorithm>

const int32_t *ScoreboardObjective::find(int64_t identity) const {
    for (const auto &score: mScores) {
        if (score.first == identity)
            return &score.second;
    }
    return nullptr;
}

bool Scoreboard::isDisplaySlot(const std::string &slot) {
    return slot == SLOT_SIDEBAR || slot == SLOT_LIST || slot == SLOT_BELOW_NAME;
}

ScoreboardObjective *Scoreboard::addObjective(const std::string &name, const std::string &displayName,
                                              const std::string &criteria) {
    if (getObjective(name) != nullptr)
        return nullptr;
    std::unique_ptr<ScoreboardObjective> objective = std::make_unique<ScoreboardObjective>();
    objective->mName = name;
    objective->mDisplayName = displayName.empty() ? name : displayName;
    objective->mCriteria = criteria;
    objective->mSerial = mNextSerial++;
    mObjectives.push_back(std::move(objective));
    return mObjectives.back().get();
}

ScoreboardObjective *Scoreboard::getObjective(const std::string &name) {
    for (const std::unique_ptr<ScoreboardObjective> &objective: mObjectives) {
        if (objective->mName == name)
            return objective.get();
    }
    return nullptr;
}

bool Scoreboard::removeObjective(const std::string &name) {
    const auto it = std::find_if(
            mObjectives.begin(), mObjectives.end(),
            [&name](const std::unique_ptr<ScoreboardObjective> &objective) { return objective->mName == name; });
    if (it == mObjectives.end())
        return false;

    std::vector<int64_t> participants;
    for (const auto &score: (*it)->mScores)
        participants.push_back(score.first);
    mObjectives.erase(it);
    mDisplays.erase(std::remove_if(mDisplays.begin(), mDisplays.end(),
                                   [&name](const ScoreboardDisplay &display) { return display.mObjective == name; }),
                    mDisplays.end());
    if (mListener.mObjectiveRemoved)
        mListener.mObjectiveRemoved(name);
    for (int64_t identity: participants)
        _dropIfUnused(identity);
    return true;
}

const ScoreboardIdentity *Scoreboard::findIdentity(int64_t id) const {
    for (const ScoreboardIdentity &identity: mIdentities) {
        if (identity.mId == id)
            return &identity;
    }
    return nullptr;
}

const ScoreboardIdentity *Scoreboard::findActor(int64_t actorId) const {
    for (const ScoreboardIdentity &identity: mIdentities) {
        if (identity.mType != ScoreboardIdentityType::FakePlayer && identity.mActorId == actorId)
            return &identity;
    }
    return nullptr;
}

const ScoreboardIdentity *Scoreboard::findFakePlayer(const std::string &name) const {
    for (const ScoreboardIdentity &identity: mIdentities) {
        if (identity.mType == ScoreboardIdentityType::FakePlayer && identity.mFakeName == name)
            return &identity;
    }
    return nullptr;
}

const ScoreboardIdentity &Scoreboard::createActor(int64_t actorId, bool player) {
    if (const ScoreboardIdentity *existing = findActor(actorId))
        return *existing;
    ScoreboardIdentity identity;
    identity.mId = ++mLastUniqueId;
    identity.mType = player ? ScoreboardIdentityType::Player : ScoreboardIdentityType::Entity;
    identity.mActorId = actorId;
    mIdentities.push_back(identity);
    if (mListener.mIdentityChanged)
        mListener.mIdentityChanged(mIdentities.back(), true);
    return mIdentities.back();
}

const ScoreboardIdentity &Scoreboard::createFakePlayer(const std::string &name) {
    if (const ScoreboardIdentity *existing = findFakePlayer(name))
        return *existing;
    ScoreboardIdentity identity;
    identity.mId = ++mLastUniqueId;
    identity.mType = ScoreboardIdentityType::FakePlayer;
    identity.mFakeName = name;
    mIdentities.push_back(identity);
    if (mListener.mIdentityChanged)
        mListener.mIdentityChanged(mIdentities.back(), true);
    return mIdentities.back();
}

void Scoreboard::setScore(ScoreboardObjective &objective, int64_t identity, int32_t score) {
    const ScoreboardIdentity *participant = findIdentity(identity);
    if (participant == nullptr)
        return;
    auto it = std::find_if(objective.mScores.begin(), objective.mScores.end(),
                           [identity](const std::pair<int64_t, int32_t> &entry) { return entry.first == identity; });
    if (it == objective.mScores.end())
        objective.mScores.emplace_back(identity, score);
    else
        it->second = score;
    if (mListener.mScoreChanged)
        mListener.mScoreChanged(objective, *participant, score);
}

bool Scoreboard::resetScore(ScoreboardObjective &objective, int64_t identity) {
    auto it = std::find_if(objective.mScores.begin(), objective.mScores.end(),
                           [identity](const std::pair<int64_t, int32_t> &entry) { return entry.first == identity; });
    if (it == objective.mScores.end())
        return false;
    objective.mScores.erase(it);
    if (mListener.mScoreRemoved) {
        if (const ScoreboardIdentity *participant = findIdentity(identity))
            mListener.mScoreRemoved(objective, *participant);
    }
    _dropIfUnused(identity);
    return true;
}

bool Scoreboard::resetScores(int64_t identity) {
    bool removed = false;
    for (const std::unique_ptr<ScoreboardObjective> &objective: mObjectives)
        removed = resetScore(*objective, identity) || removed;
    return removed;
}

bool Scoreboard::hasScores(int64_t identity) const {
    for (const std::unique_ptr<ScoreboardObjective> &objective: mObjectives) {
        if (objective->find(identity) != nullptr)
            return true;
    }
    return false;
}

void Scoreboard::_dropIfUnused(int64_t identity) {
    if (hasScores(identity))
        return;
    const auto it = std::find_if(mIdentities.begin(), mIdentities.end(),
                                 [identity](const ScoreboardIdentity &entry) { return entry.mId == identity; });
    if (it == mIdentities.end())
        return;
    const ScoreboardIdentity removed = *it;
    mIdentities.erase(it);
    if (mListener.mIdentityChanged)
        mListener.mIdentityChanged(removed, false);
}

std::string Scoreboard::setDisplay(const std::string &slot, const ScoreboardObjective &objective,
                                   ObjectiveSortOrder order) {
    std::string previous;
    auto it = std::find_if(mDisplays.begin(), mDisplays.end(),
                           [&slot](const ScoreboardDisplay &display) { return display.mSlot == slot; });
    if (it == mDisplays.end()) {
        mDisplays.push_back(ScoreboardDisplay{slot, objective.mName, order});
    } else {
        previous = it->mObjective;
        it->mObjective = objective.mName;
        it->mSortOrder = order;
    }
    if (mListener.mDisplayChanged)
        mListener.mDisplayChanged(slot, &objective, order);
    return previous;
}

std::string Scoreboard::clearDisplay(const std::string &slot) {
    auto it = std::find_if(mDisplays.begin(), mDisplays.end(),
                           [&slot](const ScoreboardDisplay &display) { return display.mSlot == slot; });
    if (it == mDisplays.end())
        return std::string();
    const std::string previous = it->mObjective;
    mDisplays.erase(it);
    if (mListener.mDisplayChanged)
        mListener.mDisplayChanged(slot, nullptr, ObjectiveSortOrder::Descending);
    return previous;
}

const ScoreboardDisplay *Scoreboard::getDisplay(const std::string &slot) const {
    for (const ScoreboardDisplay &display: mDisplays) {
        if (display.mSlot == slot)
            return &display;
    }
    return nullptr;
}

bool Scoreboard::isDisplayed(const std::string &objective) const {
    for (const ScoreboardDisplay &display: mDisplays) {
        if (display.mObjective == objective)
            return true;
    }
    return false;
}

// the layout sub_1405D50C0 loads: under the "scoreboard" key itself, IdentityType and SortOrder as bytes
Tag Scoreboard::save() const {
    Tag objectives = Tag::ofList(Tag::Type::Compound);
    for (const std::unique_ptr<ScoreboardObjective> &objective: mObjectives) {
        Tag scores = Tag::ofList(Tag::Type::Compound);
        for (const auto &score: objective->mScores) {
            Tag entry = Tag::ofCompound();
            entry.putInt("Score", score.second);
            entry.putLong("ScoreboardId", score.first);
            scores.addToList(std::move(entry));
        }
        Tag saved = Tag::ofCompound();
        saved.putString("Name", objective->mName);
        saved.putString("DisplayName", objective->mDisplayName);
        saved.putString("Criteria", objective->mCriteria);
        saved.put("Scores", std::move(scores));
        objectives.addToList(std::move(saved));
    }

    Tag entries = Tag::ofList(Tag::Type::Compound);
    for (const ScoreboardIdentity &identity: mIdentities) {
        Tag entry = Tag::ofCompound();
        entry.putByte("IdentityType", (int8_t) identity.mType);
        entry.putLong("ScoreboardId", identity.mId);
        if (identity.mType == ScoreboardIdentityType::Player)
            entry.putLong("PlayerId", identity.mActorId);
        else if (identity.mType == ScoreboardIdentityType::Entity)
            entry.putLong("EntityID", identity.mActorId);
        else
            entry.putString("FakePlayerName", identity.mFakeName);
        entries.addToList(std::move(entry));
    }

    Tag displays = Tag::ofList(Tag::Type::Compound);
    for (const ScoreboardDisplay &display: mDisplays) {
        Tag entry = Tag::ofCompound();
        entry.putString("Name", display.mSlot);
        entry.putString("ObjectiveName", display.mObjective);
        entry.putByte("SortOrder", (int8_t) display.mSortOrder);
        displays.addToList(std::move(entry));
    }

    Tag root = Tag::ofCompound();
    root.put("Objectives", std::move(objectives));
    root.put("Entries", std::move(entries));
    root.put("DisplayObjectives", std::move(displays));
    root.putLong("LastUniqueID", mLastUniqueId);
    return root;
}

void Scoreboard::load(const Tag &saved) {
    clear();
    if (!saved.isCompound())
        return;
    mLastUniqueId = saved.getLong("LastUniqueID");

    if (const Tag *entries = saved.get("Entries")) {
        if (entries->isList()) {
            for (const Tag &entry: entries->getList()) {
                if (!entry.isCompound())
                    continue;
                ScoreboardIdentity identity;
                identity.mType = (ScoreboardIdentityType) entry.getByte("IdentityType");
                identity.mId = entry.getLong("ScoreboardId");
                if (identity.mType == ScoreboardIdentityType::Player)
                    identity.mActorId = entry.getLong("PlayerId");
                else if (identity.mType == ScoreboardIdentityType::Entity)
                    identity.mActorId = entry.getLong("EntityID");
                else if (identity.mType == ScoreboardIdentityType::FakePlayer)
                    identity.mFakeName = entry.getString("FakePlayerName");
                else
                    continue;
                if (identity.mId == 0 || findIdentity(identity.mId) != nullptr)
                    continue;
                mLastUniqueId = std::max(mLastUniqueId, identity.mId);
                mIdentities.push_back(identity);
            }
        }
    }

    if (const Tag *objectives = saved.get("Objectives")) {
        if (objectives->isList()) {
            for (const Tag &entry: objectives->getList()) {
                if (!entry.isCompound())
                    continue;
                ScoreboardObjective *objective = addObjective(entry.getString("Name"), entry.getString("DisplayName"),
                                                              entry.getString("Criteria", CRITERIA_DUMMY));
                if (objective == nullptr || objective->mName.empty())
                    continue;
                const Tag *scores = entry.get("Scores");
                if (scores == nullptr || !scores->isList())
                    continue;
                for (const Tag &score: scores->getList()) {
                    const int64_t identity = score.isCompound() ? score.getLong("ScoreboardId") : 0;
                    if (findIdentity(identity) != nullptr && objective->find(identity) == nullptr)
                        objective->mScores.emplace_back(identity, score.getInt("Score"));
                }
            }
        }
    }
    mObjectives.erase(std::remove_if(mObjectives.begin(), mObjectives.end(),
                                     [](const std::unique_ptr<ScoreboardObjective> &objective) {
                                         return objective->mName.empty();
                                     }),
                      mObjectives.end());

    if (const Tag *displays = saved.get("DisplayObjectives")) {
        if (displays->isList()) {
            for (const Tag &entry: displays->getList()) {
                if (!entry.isCompound())
                    continue;
                const std::string slot = entry.getString("Name");
                const std::string objective = entry.getString("ObjectiveName");
                if (!isDisplaySlot(slot) || getObjective(objective) == nullptr || getDisplay(slot) != nullptr)
                    continue;
                const ObjectiveSortOrder order = entry.getByte("SortOrder") == 0 ? ObjectiveSortOrder::Ascending
                                                                                 : ObjectiveSortOrder::Descending;
                mDisplays.push_back(ScoreboardDisplay{slot, objective, order});
            }
        }
    }

    // identities without any score are not kept
    mIdentities.erase(std::remove_if(mIdentities.begin(), mIdentities.end(),
                                     [this](const ScoreboardIdentity &identity) { return !hasScores(identity.mId); }),
                      mIdentities.end());
}

void Scoreboard::clear() {
    mObjectives.clear();
    mIdentities.clear();
    mDisplays.clear();
    mLastUniqueId = 0;
}
