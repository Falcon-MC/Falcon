#pragma once

#include "Core/NBT/Tag.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

// the IdentityType byte of the saved entries
enum class ScoreboardIdentityType : uint8_t { Invalid = 0, Player = 1, Entity = 2, FakePlayer = 3 };

enum class ObjectiveSortOrder : uint8_t { Ascending = 0, Descending = 1 };

struct ScoreboardIdentity {
    int64_t mId = 0;
    ScoreboardIdentityType mType = ScoreboardIdentityType::Invalid;
    // the actor unique id of a player or an entity
    int64_t mActorId = 0;
    std::string mFakeName;
};

struct ScoreboardObjective {
    std::string mName;
    std::string mDisplayName;
    std::string mCriteria;
    // tells a removed objective from a new one of the same name
    uint64_t mSerial = 0;
    // identity id and score, in the order the participants joined
    std::vector<std::pair<int64_t, int32_t>> mScores;

    const int32_t *find(int64_t identity) const;
};

struct ScoreboardDisplay {
    std::string mSlot;
    std::string mObjective;
    ObjectiveSortOrder mSortOrder = ObjectiveSortOrder::Descending;
};

// objectives, participants and display slots of the server; one per server, saved in the default world
class Scoreboard {
public:
    static constexpr const char *STORAGE_KEY = "scoreboard";
    static constexpr const char *CRITERIA_DUMMY = "dummy";
    static constexpr const char *SLOT_SIDEBAR = "sidebar";
    static constexpr const char *SLOT_LIST = "list";
    static constexpr const char *SLOT_BELOW_NAME = "belowname";

    // the network side; every callback is optional
    struct Listener {
        std::function<void(const ScoreboardObjective &, const ScoreboardIdentity &, int32_t)> mScoreChanged;
        std::function<void(const ScoreboardObjective &, const ScoreboardIdentity &)> mScoreRemoved;
        std::function<void(const std::string &objective)> mObjectiveRemoved;
        // objective null: the slot was cleared
        std::function<void(const std::string &slot, const ScoreboardObjective *, ObjectiveSortOrder)> mDisplayChanged;
        std::function<void(const ScoreboardIdentity &, bool added)> mIdentityChanged;
    };

    void setListener(Listener listener) {
        mListener = std::move(listener);
    }

    static bool isDisplaySlot(const std::string &slot);

    // null when the name is taken
    ScoreboardObjective *addObjective(const std::string &name, const std::string &displayName,
                                      const std::string &criteria = CRITERIA_DUMMY);

    ScoreboardObjective *getObjective(const std::string &name);

    const std::vector<std::unique_ptr<ScoreboardObjective>> &getObjectives() const {
        return mObjectives;
    }

    // also clears the display slots showing it
    bool removeObjective(const std::string &name);

    const ScoreboardIdentity *findIdentity(int64_t id) const;

    const ScoreboardIdentity *findActor(int64_t actorId) const;

    const ScoreboardIdentity *findFakePlayer(const std::string &name) const;

    // a player, an entity or a fake player; reuses the identity it already has
    const ScoreboardIdentity &createActor(int64_t actorId, bool player);

    const ScoreboardIdentity &createFakePlayer(const std::string &name);

    const std::vector<ScoreboardIdentity> &getIdentities() const {
        return mIdentities;
    }

    void setScore(ScoreboardObjective &objective, int64_t identity, int32_t score);

    // false when the identity has no score in the objective; drops an identity left without scores
    bool resetScore(ScoreboardObjective &objective, int64_t identity);

    // every objective; false when the identity had no score
    bool resetScores(int64_t identity);

    bool hasScores(int64_t identity) const;

    // returns the objective the slot showed before, empty when none
    std::string setDisplay(const std::string &slot, const ScoreboardObjective &objective, ObjectiveSortOrder order);

    std::string clearDisplay(const std::string &slot);

    const ScoreboardDisplay *getDisplay(const std::string &slot) const;

    const std::vector<ScoreboardDisplay> &getDisplays() const {
        return mDisplays;
    }

    bool isDisplayed(const std::string &objective) const;

    Tag save() const;

    void load(const Tag &saved);

    void clear();

private:
    void _dropIfUnused(int64_t identity);

    std::vector<std::unique_ptr<ScoreboardObjective>> mObjectives;
    std::vector<ScoreboardIdentity> mIdentities;
    std::vector<ScoreboardDisplay> mDisplays;
    int64_t mLastUniqueId = 0;
    uint64_t mNextSerial = 1;
    Listener mListener;
};
