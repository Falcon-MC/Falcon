#include "Scripting/Binding/ScriptApi.h"

#include "Network/ServerNetworkHandler.h"

#include <climits>
#include <cmath>
#include <stdexcept>
#include <string>

using namespace ScriptValues;

namespace {
    Scoreboard &scoreboardOf(ScriptApi &api) {
        return api.host().getScoreboard();
    }

    JSValue conversionError(JSContext *ctx) {
        return JS_ThrowTypeError(ctx, "Native type conversion failed.");
    }

    // DisplaySlotId values are the wire slot names, capitalised
    bool readSlot(JSContext *ctx, JSValueConst value, std::string &slot) {
        if (!JS_IsString(value))
            return false;
        const std::string name = toString(ctx, value);
        if (name == "Sidebar")
            slot = Scoreboard::SLOT_SIDEBAR;
        else if (name == "List")
            slot = Scoreboard::SLOT_LIST;
        else if (name == "BelowName")
            slot = Scoreboard::SLOT_BELOW_NAME;
        else
            return false;
        return true;
    }

    const char *identityTypeName(ScoreboardIdentityType type) {
        switch (type) {
            case ScoreboardIdentityType::Player:
                return "Player";
            case ScoreboardIdentityType::Entity:
                return "Entity";
            default:
                return "FakePlayer";
        }
    }

    JSValue makeObjective(ScriptApi &api, const ScoreboardObjective &objective) {
        ScriptObjectiveRef *ref = new ScriptObjectiveRef();
        ref->mName = objective.mName;
        ref->mSerial = objective.mSerial;
        return api.makeInstance("ScoreboardObjective", ref);
    }

    JSValue makeIdentity(ScriptApi &api, const ScoreboardIdentity &identity) {
        ScriptIdentityRef *ref = new ScriptIdentityRef();
        ref->mIdentity = identity;
        return api.makeInstance("ScoreboardIdentity", ref);
    }

    // null once removed, or once another objective took the name
    ScoreboardObjective *objectiveOf(ScriptApi &api, JSValueConst value) {
        const ScriptObjectiveRef *ref = api.native<ScriptObjectiveRef>(value);
        if (ref == nullptr)
            return nullptr;
        ScoreboardObjective *objective = scoreboardOf(api).getObjective(ref->mName);
        return objective != nullptr && objective->mSerial == ref->mSerial ? objective : nullptr;
    }

    JSValue invalidObjective(ScriptApi &api, JSValueConst value) {
        const ScriptObjectiveRef *ref = api.native<ScriptObjectiveRef>(value);
        return api.throwError("Error",
                              "Failed to find objective '" + (ref == nullptr ? std::string() : ref->mName) + "'.");
    }

    // Entity, ScoreboardIdentity or a fake player's name; 0 when it has no identity and create is false
    bool participantOf(ScriptApi &api, JSValueConst value, bool create, int64_t &id) {
        Scoreboard &scoreboard = scoreboardOf(api);
        id = 0;
        if (JS_IsString(value)) {
            const std::string name = toString(api.context(), value);
            if (name.empty())
                return false;
            if (const ScoreboardIdentity *fake = scoreboard.findFakePlayer(name))
                id = fake->mId;
            else if (create)
                id = scoreboard.createFakePlayer(name).mId;
            return true;
        }
        if (const ScriptIdentityRef *ref = api.native<ScriptIdentityRef>(value)) {
            const ScoreboardIdentity &identity = ref->mIdentity;
            if (scoreboard.findIdentity(identity.mId) != nullptr)
                id = identity.mId;
            else if (create && identity.mType == ScoreboardIdentityType::FakePlayer)
                id = scoreboard.createFakePlayer(identity.mFakeName).mId;
            else if (create)
                id = scoreboard.createActor(identity.mActorId, identity.mType == ScoreboardIdentityType::Player).mId;
            return true;
        }
        if (api.native<ScriptEntityRef>(value) == nullptr)
            return false;
        // the game keys an Entity participant by its unique id: a removed entity keeps its scores
        const ScriptEntityRef *ref = api.native<ScriptEntityRef>(value);
        int64_t uniqueId = 0;
        if (const Actor *actor = api.resolveEntity(*ref)) {
            uniqueId = actor->getUniqueId();
        } else {
            try {
                uniqueId = std::stoll(ref->mEntityId);
            } catch (const std::exception &) {
                return false;
            }
        }
        if (const ScoreboardIdentity *identity = scoreboard.findActor(uniqueId))
            id = identity->mId;
        else if (create)
            id = scoreboard.createActor(uniqueId, ref->mKind == ScriptEntityRef::Kind::Player).mId;
        return true;
    }


    bool readScore(JSContext *ctx, JSValueConst value, int32_t &score) {
        double number = 0.0;
        if (!JS_IsNumber(value) || JS_ToFloat64(ctx, &number, value) != 0 || !std::isfinite(number))
            return false;
        if (number < (double) INT32_MIN || number > (double) INT32_MAX)
            return false;
        score = (int32_t) std::trunc(number);
        return true;
    }

    JSValue boardAddObjective(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]) || (argc >= 2 && !JS_IsUndefined(argv[1]) && !JS_IsString(argv[1])))
            return conversionError(ctx);
        const std::string id = toString(ctx, argv[0]);
        const std::string displayName = argc >= 2 && JS_IsString(argv[1]) ? toString(ctx, argv[1]) : std::string();
        ScoreboardObjective *objective = scoreboardOf(api).addObjective(id, displayName);
        if (objective == nullptr)
            return api.throwError("Error", "Failed to add objective '" + id + "' as it is already being tracked.");
        return makeObjective(api, *objective);
    }

    JSValue boardGetObjective(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        if (argc < 1 || !JS_IsString(argv[0]))
            return conversionError(ctx);
        const ScoreboardObjective *objective = scoreboardOf(api).getObjective(toString(ctx, argv[0]));
        return objective == nullptr ? JS_UNDEFINED : makeObjective(api, *objective);
    }

    JSValue boardGetObjectives(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const std::unique_ptr<ScoreboardObjective> &objective: scoreboardOf(api).getObjectives())
            JS_SetPropertyUint32(ctx, list, index++, makeObjective(api, *objective));
        return list;
    }

    JSValue boardGetParticipants(JSContext *ctx, JSValueConst, int, JSValueConst *) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const ScoreboardIdentity &identity: scoreboardOf(api).getIdentities())
            JS_SetPropertyUint32(ctx, list, index++, makeIdentity(api, identity));
        return list;
    }

    JSValue boardRemoveObjective(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        if (argc < 1)
            return conversionError(ctx);
        if (JS_IsString(argv[0]))
            return JS_NewBool(ctx, scoreboardOf(api).removeObjective(toString(ctx, argv[0])));
        if (api.native<ScriptObjectiveRef>(argv[0]) == nullptr)
            return conversionError(ctx);
        const ScoreboardObjective *objective = objectiveOf(api, argv[0]);
        if (objective == nullptr)
            return api.throwError("Error", "Failed to find the objective specified.");
        const std::string name = objective->mName;
        return JS_NewBool(ctx, scoreboardOf(api).removeObjective(name));
    }

    JSValue previousObjective(ScriptApi &api, const std::string &name) {
        const ScoreboardObjective *previous = name.empty() ? nullptr : scoreboardOf(api).getObjective(name);
        return previous == nullptr ? JS_UNDEFINED : makeObjective(api, *previous);
    }

    JSValue boardSetDisplay(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        std::string slot;
        if (argc < 2 || !readSlot(ctx, argv[0], slot) || !JS_IsObject(argv[1]))
            return conversionError(ctx);
        JSValue objectiveValue = JS_GetPropertyStr(ctx, argv[1], "objective");
        JSValue orderValue = JS_GetPropertyStr(ctx, argv[1], "sortOrder");
        const ScoreboardObjective *objective = objectiveOf(api, objectiveValue);
        const bool isObjective = api.native<ScriptObjectiveRef>(objectiveValue) != nullptr;
        int32_t order = (int32_t) ObjectiveSortOrder::Descending;
        const bool orderOk = JS_IsUndefined(orderValue)
                             || (JS_IsNumber(orderValue) && JS_ToInt32(ctx, &order, orderValue) == 0
                                 && (order == 0 || order == 1));
        JS_FreeValue(ctx, objectiveValue);
        JS_FreeValue(ctx, orderValue);
        if (!isObjective || !orderOk)
            return conversionError(ctx);
        if (objective == nullptr)
            return api.throwError("Error", "Failed to set invalid objective at DisplaySlot.");
        // descending unless ascending is given
        const std::string previous = scoreboardOf(api).setDisplay(slot, *objective, (ObjectiveSortOrder) order);
        return previous == objective->mName ? makeObjective(api, *objective) : previousObjective(api, previous);
    }

    JSValue boardClearDisplay(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        std::string slot;
        if (argc < 1 || !readSlot(ctx, argv[0], slot))
            return conversionError(ctx);
        return previousObjective(api, scoreboardOf(api).clearDisplay(slot));
    }

    JSValue boardGetDisplay(JSContext *ctx, JSValueConst, int argc, JSValueConst *argv) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        std::string slot;
        if (argc < 1 || !readSlot(ctx, argv[0], slot))
            return conversionError(ctx);
        const ScoreboardDisplay *display = scoreboardOf(api).getDisplay(slot);
        const ScoreboardObjective *objective = display == nullptr ? nullptr
                                                                  : scoreboardOf(api).getObjective(display->mObjective);
        if (objective == nullptr)
            return JS_UNDEFINED;
        JSValue options = JS_NewObject(ctx);
        JS_SetPropertyStr(ctx, options, "objective", makeObjective(api, *objective));
        JS_SetPropertyStr(ctx, options, "sortOrder", JS_NewInt32(ctx, (int32_t) display->mSortOrder));
        return options;
    }

    // magic: 0 id, 1 displayName
    JSValue objectiveGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const ScoreboardObjective *objective = objectiveOf(api, thisVal);
        if (objective == nullptr)
            return invalidObjective(api, thisVal);
        return string(ctx, magic == 0 ? objective->mName : objective->mDisplayName);
    }

    JSValue objectiveIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        return JS_NewBool(ctx, objectiveOf(*ScriptApi::fromContext(ctx), thisVal) != nullptr);
    }

    // magic: 0 getScore, 1 hasParticipant, 2 removeParticipant
    JSValue objectiveRead(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        ScoreboardObjective *objective = objectiveOf(api, thisVal);
        if (objective == nullptr)
            return invalidObjective(api, thisVal);
        int64_t id = 0;
        if (argc < 1 || !participantOf(api, argv[0], false, id))
            return conversionError(ctx);
        const int32_t *score = id == 0 ? nullptr : objective->find(id);
        if (magic == 0)
            return score == nullptr ? JS_UNDEFINED : JS_NewInt32(ctx, *score);
        if (magic == 1)
            return JS_NewBool(ctx, score != nullptr);
        return JS_NewBool(ctx, score != nullptr && scoreboardOf(api).resetScore(*objective, id));
    }

    // magic: 0 setScore, 1 addScore
    JSValue objectiveWrite(JSContext *ctx, JSValueConst thisVal, int argc, JSValueConst *argv, int magic) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        ScoreboardObjective *objective = objectiveOf(api, thisVal);
        if (objective == nullptr)
            return invalidObjective(api, thisVal);
        int32_t value = 0;
        if (argc < 2 || !readScore(ctx, argv[1], value))
            return conversionError(ctx);
        int64_t id = 0;
        if (!participantOf(api, argv[0], false, id))
            return conversionError(ctx);

        const int32_t *current = id == 0 ? nullptr : objective->find(id);
        const int64_t next = magic == 0 ? value : (int64_t) (current == nullptr ? 0 : *current) + value;
        // string
        if (next < INT32_MIN || next > INT32_MAX) {
            const ScoreboardIdentity *identity = scoreboardOf(api).findIdentity(id);
            // the game names the participant
            const std::string name = identity == nullptr ? std::string("<invalid participant>")
                                                         : api.host().getScoreboardName(*identity, true);
            return api.throwError("Error", "Encountered an integer overflow when trying to modify objective score for "
                                           "participant '"
                                                   + name + "'.");
        }
        if (id == 0)
            participantOf(api, argv[0], true, id);
        scoreboardOf(api).setScore(*objective, id, (int32_t) next);
        return magic == 0 ? JS_UNDEFINED : JS_NewInt32(ctx, (int32_t) next);
    }

    // magic: 0 getParticipants, 1 getScores
    JSValue objectiveList(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const ScoreboardObjective *objective = objectiveOf(api, thisVal);
        if (objective == nullptr)
            return invalidObjective(api, thisVal);
        JSValue list = JS_NewArray(ctx);
        uint32_t index = 0;
        for (const auto &score: objective->mScores) {
            const ScoreboardIdentity *identity = scoreboardOf(api).findIdentity(score.first);
            if (identity == nullptr)
                continue;
            if (magic == 0) {
                JS_SetPropertyUint32(ctx, list, index++, makeIdentity(api, *identity));
                continue;
            }
            ScriptIdentityRef *ref = new ScriptIdentityRef();
            ref->mIdentity = *identity;
            ref->mScore = score.second;
            JS_SetPropertyUint32(ctx, list, index++, api.makeInstance("ScoreboardScoreInfo", ref));
        }
        return list;
    }

    const ScriptIdentityRef *identityRefOf(JSContext *ctx, JSValueConst value) {
        return ScriptApi::fromContext(ctx)->native<ScriptIdentityRef>(value);
    }

    // magic: 0 displayName, 1 id, 2 type
    JSValue identityGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const ScriptIdentityRef *ref = identityRefOf(ctx, thisVal);
        if (ref == nullptr)
            return conversionError(ctx);
        const ScoreboardIdentity &identity = ref->mIdentity;
        if (magic == 0)
            return string(ctx, api.host().getScoreboardName(identity, true));
        if (magic == 1)
            return JS_NewInt64(ctx, identity.mId);
        return JS_NewString(ctx, identityTypeName(identity.mType));
    }

    JSValue identityIsValid(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        const ScriptIdentityRef *ref = identityRefOf(ctx, thisVal);
        return JS_NewBool(ctx, ref != nullptr
                                       && scoreboardOf(*ScriptApi::fromContext(ctx)).findIdentity(ref->mIdentity.mId)
                                                  != nullptr);
    }

    JSValue identityGetEntity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const ScriptIdentityRef *ref = identityRefOf(ctx, thisVal);
        if (ref == nullptr)
            return conversionError(ctx);
        Actor *actor = api.host().findScoreboardActor(ref->mIdentity);
        return actor == nullptr ? JS_UNDEFINED : api.makeEntity(*actor);
    }

    // magic: 0 participant, 1 score
    JSValue scoreInfoGet(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *, int magic) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const ScriptIdentityRef *ref = identityRefOf(ctx, thisVal);
        if (ref == nullptr)
            return conversionError(ctx);
        return magic == 0 ? makeIdentity(api, ref->mIdentity) : JS_NewInt32(ctx, ref->mScore);
    }

    JSValue entityGetScoreboardIdentity(JSContext *ctx, JSValueConst thisVal, int, JSValueConst *) {
        ScriptApi &api = *ScriptApi::fromContext(ctx);
        const Actor *actor = api.resolveEntity(thisVal);
        const ScoreboardIdentity *identity = actor == nullptr ? nullptr : api.host().findScoreboardIdentity(*actor);
        return identity == nullptr ? JS_UNDEFINED : makeIdentity(api, *identity);
    }
}

bool ScriptApi::scoresMatch(const Actor &actor, JSValueConst options) {
    JSContext *ctx = mContext;
    JSValue list = JS_GetPropertyStr(ctx, options, "scoreOptions");
    uint32_t length = 0;
    if (JS_IsArray(list)) {
        JSValue lengthValue = JS_GetPropertyStr(ctx, list, "length");
        JS_ToUint32(ctx, &length, lengthValue);
        JS_FreeValue(ctx, lengthValue);
    }

    Scoreboard &scoreboard = mHost.getScoreboard();
    const ScoreboardIdentity *identity = mHost.findScoreboardIdentity(actor);
    bool matches = true;
    for (uint32_t index = 0; index < length && matches; ++index) {
        JSValue entry = JS_GetPropertyUint32(ctx, list, index);
        JSValue objectiveValue = JS_GetPropertyStr(ctx, entry, "objective");
        JSValue minValue = JS_GetPropertyStr(ctx, entry, "minScore");
        JSValue maxValue = JS_GetPropertyStr(ctx, entry, "maxScore");
        JSValue excludeValue = JS_GetPropertyStr(ctx, entry, "exclude");
        double low = (double) INT32_MIN, high = (double) INT32_MAX;
        if (JS_IsNumber(minValue))
            JS_ToFloat64(ctx, &low, minValue);
        if (JS_IsNumber(maxValue))
            JS_ToFloat64(ctx, &high, maxValue);
        const bool exclude = JS_ToBool(ctx, excludeValue) == 1;
        const ScoreboardObjective *objective = JS_IsString(objectiveValue)
                                                       ? scoreboard.getObjective(toString(ctx, objectiveValue))
                                                       : nullptr;
        const int32_t *score = objective == nullptr || identity == nullptr ? nullptr : objective->find(identity->mId);
        // no score fails the filter, excluded or not
        matches = score != nullptr && ((*score >= low && *score <= high) != exclude);
        JS_FreeValue(ctx, entry);
        JS_FreeValue(ctx, objectiveValue);
        JS_FreeValue(ctx, minValue);
        JS_FreeValue(ctx, maxValue);
        JS_FreeValue(ctx, excludeValue);
    }
    JS_FreeValue(ctx, list);
    return matches;
}

void ScriptApi::_bindScoreboard() {
    if (mSurface.has("Scoreboard")) {
        JSValue board = defineClass("Scoreboard");
        method(board, "Scoreboard", "addObjective", boardAddObjective, 2);
        method(board, "Scoreboard", "clearObjectiveAtDisplaySlot", boardClearDisplay, 1);
        method(board, "Scoreboard", "getObjective", boardGetObjective, 1);
        method(board, "Scoreboard", "getObjectiveAtDisplaySlot", boardGetDisplay, 1);
        method(board, "Scoreboard", "getObjectives", boardGetObjectives, 0);
        method(board, "Scoreboard", "getParticipants", boardGetParticipants, 0);
        method(board, "Scoreboard", "removeObjective", boardRemoveObjective, 1);
        method(board, "Scoreboard", "setObjectiveAtDisplaySlot", boardSetDisplay, 2);
        mScoreboard = makeInstance("Scoreboard", new ScriptNative());
    }

    if (mSurface.has("ScoreboardObjective")) {
        JSValue objective = defineClass("ScoreboardObjective");
        propertyMagic(objective, "ScoreboardObjective", "id", objectiveGet, 0);
        propertyMagic(objective, "ScoreboardObjective", "displayName", objectiveGet, 1);
        // a method up to 1.x
        if (isStable1())
            method(objective, "ScoreboardObjective", "isValid", objectiveIsValid, 0);
        else
            property(objective, "ScoreboardObjective", "isValid", objectiveIsValid);
        methodMagic(objective, "ScoreboardObjective", "getScore", objectiveRead, 1, 0);
        methodMagic(objective, "ScoreboardObjective", "hasParticipant", objectiveRead, 1, 1);
        methodMagic(objective, "ScoreboardObjective", "removeParticipant", objectiveRead, 1, 2);
        methodMagic(objective, "ScoreboardObjective", "setScore", objectiveWrite, 2, 0);
        methodMagic(objective, "ScoreboardObjective", "addScore", objectiveWrite, 2, 1);
        methodMagic(objective, "ScoreboardObjective", "getParticipants", objectiveList, 0, 0);
        methodMagic(objective, "ScoreboardObjective", "getScores", objectiveList, 0, 1);
    }

    if (mSurface.has("ScoreboardIdentity")) {
        JSValue identity = defineClass("ScoreboardIdentity");
        propertyMagic(identity, "ScoreboardIdentity", "displayName", identityGet, 0);
        propertyMagic(identity, "ScoreboardIdentity", "id", identityGet, 1);
        propertyMagic(identity, "ScoreboardIdentity", "type", identityGet, 2);
        if (isStable1())
            method(identity, "ScoreboardIdentity", "isValid", identityIsValid, 0);
        else
            property(identity, "ScoreboardIdentity", "isValid", identityIsValid);
        method(identity, "ScoreboardIdentity", "getEntity", identityGetEntity, 0);
    }

    if (mSurface.has("ScoreboardScoreInfo")) {
        JSValue info = defineClass("ScoreboardScoreInfo");
        propertyMagic(info, "ScoreboardScoreInfo", "participant", scoreInfoGet, 0);
        propertyMagic(info, "ScoreboardScoreInfo", "score", scoreInfoGet, 1);
    }

    JSValue entity = prototypeOf("Entity");
    if (!JS_IsUndefined(entity))
        property(entity, "Entity", "scoreboardIdentity", entityGetScoreboardIdentity);
}
