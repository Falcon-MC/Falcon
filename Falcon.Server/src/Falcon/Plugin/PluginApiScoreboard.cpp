#include "Falcon/Plugin/PluginServerApi.h"

#include "Falcon/Plugin/PluginApiHelpers.h"
#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Actor.h"
#include "World/Scores/Scoreboard.h"

#include <string>

using namespace PluginApiHelpers;

namespace {
    Scoreboard &scoreboard() {
        return owner().getScoreboard();
    }

    ScoreboardObjective *objectiveNamed(const char *name) {
        return name == nullptr ? nullptr : scoreboard().getObjective(name);
    }

    const char *slotName(FalconDisplaySlot slot) {
        switch (slot) {
            case FALCON_DISPLAY_SLOT_SIDEBAR:
                return Scoreboard::SLOT_SIDEBAR;
            case FALCON_DISPLAY_SLOT_LIST:
                return Scoreboard::SLOT_LIST;
            case FALCON_DISPLAY_SLOT_BELOW_NAME:
                return Scoreboard::SLOT_BELOW_NAME;
            default:
                return nullptr;
        }
    }

    // the game keys an entity participant by its unique id: a removed entity keeps its scores
    int64_t entityIdentity(FalconEntity *target, bool create) {
        const Actor *value = entity(target);
        if (const ScoreboardIdentity *identity = scoreboard().findActor(value->getUniqueId()))
            return identity->mId;
        return create ? scoreboard().createActor(value->getUniqueId(), value->isPlayer()).mId : 0;
    }

    int64_t fakeIdentity(const char *name, bool create) {
        if (name == nullptr || *name == '\0')
            return 0;
        if (const ScoreboardIdentity *identity = scoreboard().findFakePlayer(name))
            return identity->mId;
        return create ? scoreboard().createFakePlayer(name).mId : 0;
    }

    int readScore(const char *objective, int64_t identity, int32_t *score) {
        ScoreboardObjective *found = objectiveNamed(objective);
        const int32_t *value = found == nullptr || identity == 0 ? nullptr : found->find(identity);
        if (value == nullptr)
            return 0;
        if (score != nullptr)
            *score = *value;
        return 1;
    }

    int writeScore(const char *objective, int64_t identity, int32_t score) {
        ScoreboardObjective *found = objectiveNamed(objective);
        if (found == nullptr || identity == 0)
            return 0;
        scoreboard().setScore(*found, identity, score);
        return 1;
    }

    int resetScore(const char *objective, int64_t identity) {
        ScoreboardObjective *found = objectiveNamed(objective);
        return found != nullptr && identity != 0 && scoreboard().resetScore(*found, identity) ? 1 : 0;
    }

    int scoreboardAddObjective(const char *name, const char *displayName) {
        if (name == nullptr || *name == '\0')
            return 0;
        return scoreboard().addObjective(name, displayName == nullptr ? std::string() : std::string(displayName))
                               != nullptr
                       ? 1
                       : 0;
    }

    int scoreboardRemoveObjective(const char *name) {
        return name != nullptr && scoreboard().removeObjective(name) ? 1 : 0;
    }

    uint32_t scoreboardObjectiveCount() {
        return (uint32_t) scoreboard().getObjectives().size();
    }

    const char *scoreboardObjectiveName(uint32_t index) {
        const auto &objectives = scoreboard().getObjectives();
        return index < objectives.size() ? hold(objectives[index]->mName) : nullptr;
    }

    const char *scoreboardObjectiveDisplayName(const char *objective) {
        const ScoreboardObjective *found = objectiveNamed(objective);
        return found == nullptr ? nullptr : hold(found->mDisplayName);
    }

    int scoreboardEntityScore(const char *objective, FalconEntity *target, int32_t *score) {
        return readScore(objective, entityIdentity(target, false), score);
    }

    int scoreboardSetEntityScore(const char *objective, FalconEntity *target, int32_t score) {
        if (objectiveNamed(objective) == nullptr)
            return 0;
        return writeScore(objective, entityIdentity(target, true), score);
    }

    int scoreboardResetEntityScore(const char *objective, FalconEntity *target) {
        return resetScore(objective, entityIdentity(target, false));
    }

    int scoreboardFakePlayerScore(const char *objective, const char *name, int32_t *score) {
        return readScore(objective, fakeIdentity(name, false), score);
    }

    int scoreboardSetFakePlayerScore(const char *objective, const char *name, int32_t score) {
        if (objectiveNamed(objective) == nullptr)
            return 0;
        return writeScore(objective, fakeIdentity(name, true), score);
    }

    int scoreboardResetFakePlayerScore(const char *objective, const char *name) {
        return resetScore(objective, fakeIdentity(name, false));
    }

    uint32_t scoreboardScoreCount(const char *objective) {
        const ScoreboardObjective *found = objectiveNamed(objective);
        return found == nullptr ? 0 : (uint32_t) found->mScores.size();
    }

    const ScoreboardIdentity *scoreIdentity(const char *objective, uint32_t index, int32_t *score) {
        const ScoreboardObjective *found = objectiveNamed(objective);
        if (found == nullptr || index >= found->mScores.size())
            return nullptr;
        if (score != nullptr)
            *score = found->mScores[index].second;
        return scoreboard().findIdentity(found->mScores[index].first);
    }

    const char *scoreboardScoreAt(const char *objective, uint32_t index, int32_t *score) {
        const ScoreboardIdentity *identity = scoreIdentity(objective, index, score);
        return identity == nullptr ? nullptr : hold(owner().getScoreboardName(*identity, true));
    }

    FalconEntity *scoreboardScoreEntity(const char *objective, uint32_t index) {
        const ScoreboardIdentity *identity = scoreIdentity(objective, index, nullptr);
        return identity == nullptr ? nullptr : toHandle(owner().findScoreboardActor(*identity));
    }

    int scoreboardSetDisplay(FalconDisplaySlot slot, const char *objective, int ascending) {
        const char *name = slotName(slot);
        if (name == nullptr)
            return 0;
        if (objective == nullptr) {
            scoreboard().clearDisplay(name);
            return 1;
        }

        const ScoreboardObjective *found = objectiveNamed(objective);
        if (found == nullptr)
            return 0;
        scoreboard().setDisplay(name, *found,
                                ascending != 0 ? ObjectiveSortOrder::Ascending : ObjectiveSortOrder::Descending);
        return 1;
    }

    const char *scoreboardDisplayObjective(FalconDisplaySlot slot) {
        const char *name = slotName(slot);
        const ScoreboardDisplay *display = name == nullptr ? nullptr : scoreboard().getDisplay(name);
        return display == nullptr || display->mObjective.empty() ? nullptr : hold(display->mObjective);
    }
}

void PluginServerApi::fillScoreboard(FalconServerApi &api) {
    api.scoreboardAddObjective = &scoreboardAddObjective;
    api.scoreboardRemoveObjective = &scoreboardRemoveObjective;
    api.scoreboardObjectiveCount = &scoreboardObjectiveCount;
    api.scoreboardObjectiveName = &scoreboardObjectiveName;
    api.scoreboardObjectiveDisplayName = &scoreboardObjectiveDisplayName;
    api.scoreboardEntityScore = &scoreboardEntityScore;
    api.scoreboardSetEntityScore = &scoreboardSetEntityScore;
    api.scoreboardResetEntityScore = &scoreboardResetEntityScore;
    api.scoreboardFakePlayerScore = &scoreboardFakePlayerScore;
    api.scoreboardSetFakePlayerScore = &scoreboardSetFakePlayerScore;
    api.scoreboardResetFakePlayerScore = &scoreboardResetFakePlayerScore;
    api.scoreboardScoreCount = &scoreboardScoreCount;
    api.scoreboardScoreAt = &scoreboardScoreAt;
    api.scoreboardScoreEntity = &scoreboardScoreEntity;
    api.scoreboardSetDisplay = &scoreboardSetDisplay;
    api.scoreboardDisplayObjective = &scoreboardDisplayObjective;
}
