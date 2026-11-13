#include "Server/Commands/ScoreboardCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Actor/ServerActor.h"

#include <algorithm>
#include <cerrno>
#include <climits>
#include <cstdlib>
#include <random>

namespace {
    bool parseInt(const std::string &value, int32_t &out) {
        if (value.empty())
            return false;
        char *end = nullptr;
        errno = 0;
        const long long parsed = std::strtoll(value.c_str(), &end, 10);
        if (*end != '\0' || errno == ERANGE || parsed < INT32_MIN || parsed > INT32_MAX)
            return false;
        out = (int32_t) parsed;
        return true;
    }

    // "*" leaves the bound open
    bool parseBound(const std::string &value, int32_t open, int32_t &out) {
        if (value == "*") {
            out = open;
            return true;
        }
        return parseInt(value, out);
    }

    bool checkedAdd(int32_t left, int32_t right, int32_t &out) {
        const int64_t sum = (int64_t) left + right;
        if (sum < INT32_MIN || sum > INT32_MAX)
            return false;
        out = (int32_t) sum;
        return true;
    }

    // false on overflow, INT_MIN by -1 included; truncating division, a zero divisor keeps the target
    bool applyOperation(const std::string &operation, int32_t &target, int32_t &source) {
        int64_t result = target;
        if (operation == "+=")
            result = (int64_t) target + source;
        else if (operation == "-=")
            result = (int64_t) target - source;
        else if (operation == "*=")
            result = (int64_t) target * source;
        else if (operation == "/=")
            result = source == 0 ? target : (source == -1 ? -(int64_t) target : target / source);
        else if (operation == "%=")
            result = source == 0
                             ? target
                             : (source == -1 ? (target == INT32_MIN ? (int64_t) INT32_MAX + 1 : 0) : target % source);
        else if (operation == "=")
            result = source;
        else if (operation == "<")
            result = std::min(target, source);
        else if (operation == ">")
            result = std::max(target, source);
        else if (operation == "><") {
            std::swap(target, source);
            return true;
        }
        if (result < INT32_MIN || result > INT32_MAX)
            return false;
        target = (int32_t) result;
        return true;
    }

    bool isOperation(const std::string &operation) {
        for (const char *known: {"+=", "-=", "*=", "/=", "%=", "=", "<", ">", "><"}) {
            if (operation == known)
                return true;
        }
        return false;
    }
}

ScoreboardCommand::ScoreboardCommand(ServerNetworkHandler &handler)
        : Command("scoreboard", "commands.scoreboard.description",
                  "/scoreboard <objectives|players> <add|remove|list|setdisplay|set|reset|test|random|operation> ..."),
          mHandler(handler) {}

std::vector<CommandOverloadData> ScoreboardCommand::getOverloads() const {
    const auto category = [](const char *value) {
        return makeEnumParameter(
                "category", value[0] == 'o' ? "ScoreboardObjectivesCategory" : "ScoreboardPlayersCategory", {value});
    };
    const auto participant = [](const char *name, bool optional = false) {
        return makeTypedParameter(name, CommandParamType::WildcardTarget, optional);
    };
    const CommandParamData objective = makeTypedParameter("objective", CommandParamType::String);

    std::vector<CommandOverloadData> overloads;
    const auto add = [&overloads](std::vector<CommandParamData> parameters) {
        CommandOverloadData overload;
        overload.mParameters = std::move(parameters);
        overloads.push_back(std::move(overload));
    };

    add({category("objectives"), makeEnumParameter("action", "ScoreboardAddAction", {"add"}), objective,
         makeEnumParameter("criteria", "ScoreboardCriteria", {"dummy"}),
         makeTypedParameter("displayName", CommandParamType::String, true)});
    add({category("objectives"), makeEnumParameter("action", "ScoreboardRemoveAction", {"remove"}), objective});
    add({category("objectives"), makeEnumParameter("action", "ScoreboardListAction", {"list"})});
    add({category("objectives"), makeEnumParameter("action", "ScoreboardSetDisplayAction", {"setdisplay"}),
         makeEnumParameter("displaySlot", "ScoreboardDisplaySlotSortable", {"list", "sidebar"}),
         makeTypedParameter("objective", CommandParamType::String, true),
         makeEnumParameter("sortOrder", "ScoreboardSortOrder", {"ascending", "descending"}, true)});
    add({category("objectives"), makeEnumParameter("action", "ScoreboardSetDisplayAction", {"setdisplay"}),
         makeEnumParameter("displaySlot", "ScoreboardDisplaySlotNonSortable", {"belowname"}),
         makeTypedParameter("objective", CommandParamType::String, true)});

    add({category("players"), makeEnumParameter("action", "ScoreboardListAction", {"list"}),
         participant("playername", true)});
    add({category("players"), makeEnumParameter("action", "ScoreboardResetAction", {"reset"}), participant("player"),
         makeTypedParameter("objective", CommandParamType::String, true)});
    add({category("players"), makeEnumParameter("action", "ScoreboardTestAction", {"test"}), participant("player"),
         objective, makeTypedParameter("min", CommandParamType::WildcardInt),
         makeTypedParameter("max", CommandParamType::WildcardInt, true)});
    add({category("players"), makeEnumParameter("action", "ScoreboardRandomAction", {"random"}), participant("player"),
         objective, makeTypedParameter("min", CommandParamType::Int),
         makeTypedParameter("max", CommandParamType::Int)});
    add({category("players"), makeEnumParameter("action", "ScoreboardPlayersNumAction", {"set", "add", "remove"}),
         participant("player"), objective, makeTypedParameter("count", CommandParamType::Int)});
    add({category("players"), makeEnumParameter("action", "ScoreboardOperationAction", {"operation"}),
         participant("targetName"), makeTypedParameter("targetObjective", CommandParamType::String),
         makeTypedParameter("operation", CommandParamType::Operator), participant("selector"), objective});
    return overloads;
}

bool ScoreboardCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() >= 2 && arguments[0] == "objectives")
        return _objectives(sender, arguments);
    if (arguments.size() >= 2 && arguments[0] == "players")
        return _players(sender, arguments);
    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}

std::vector<int64_t> ScoreboardCommand::participants(ServerNetworkHandler &handler, CommandOrigin &sender,
                                                     const std::string &argument, bool create) {
    Scoreboard &scoreboard = handler.getScoreboard();
    std::vector<int64_t> ids;
    if (argument == "*") {
        for (const ScoreboardIdentity &identity: scoreboard.getIdentities())
            ids.push_back(identity.mId);
        return ids;
    }

    const auto addActor = [&](const Actor &actor) {
        const ScoreboardIdentity *identity = handler.findScoreboardIdentity(actor);
        if (identity != nullptr)
            ids.push_back(identity->mId);
        else if (create)
            ids.push_back(handler.createScoreboardIdentity(actor));
    };

    if (!argument.empty() && argument[0] == '@') {
        for (ServerPlayer *player: handler.resolveTargets(sender, argument))
            addActor(*player);
        for (ServerActor *actor: handler.resolveActorTargets(sender, argument))
            addActor(*actor);
        return ids;
    }

    // an online player's exact name is that player; any other name is a fake player
    for (auto &entry: handler.getPlayers()) {
        if (entry.second.isSpawned() && entry.second.getName() == argument) {
            addActor(entry.second);
            return ids;
        }
    }
    if (const ScoreboardIdentity *fake = scoreboard.findFakePlayer(argument))
        ids.push_back(fake->mId);
    else if (create && !argument.empty())
        ids.push_back(scoreboard.createFakePlayer(argument).mId);
    return ids;
}

bool ScoreboardCommand::_objectives(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Scoreboard &scoreboard = mHandler.getScoreboard();
    const std::string &action = arguments[1];

    if (action == "add") {
        if (arguments.size() < 3) {
            sender.sendTranslation("commands.scoreboard.objectives.add.needName", {});
            return false;
        }
        const std::string &name = arguments[2];
        const std::string criteria = arguments.size() >= 4 ? arguments[3] : std::string();
        if (criteria != Scoreboard::CRITERIA_DUMMY) {
            sender.sendTranslation("commands.scoreboard.objectives.add.wrongType", {criteria});
            return false;
        }
        const std::string displayName = arguments.size() >= 5 ? joinArguments(arguments, 4) : std::string();
        if (scoreboard.addObjective(name, displayName, criteria) == nullptr) {
            sender.sendTranslation("commands.scoreboard.objectives.add.alreadyExists", {name});
            return false;
        }
        sender.sendTranslation("commands.scoreboard.objectives.add.success", {name});
        return true;
    }

    if (action == "remove") {
        if (arguments.size() < 3) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        if (!scoreboard.removeObjective(arguments[2])) {
            sender.sendTranslation("commands.scoreboard.objectiveNotFound", {arguments[2]});
            return false;
        }
        sender.sendTranslation("commands.scoreboard.objectives.remove.success", {arguments[2]});
        return true;
    }

    if (action == "list") {
        const auto &objectives = scoreboard.getObjectives();
        if (objectives.empty()) {
            sender.sendTranslation("commands.scoreboard.objectives.list.empty", {});
            return false;
        }
        sender.sendTranslation("commands.scoreboard.objectives.list.count", {std::to_string(objectives.size())});
        for (const std::unique_ptr<ScoreboardObjective> &objective: objectives)
            sender.sendTranslation("commands.scoreboard.objectives.list.entry",
                                   {objective->mName, objective->mDisplayName, objective->mCriteria});
        return true;
    }

    if (action == "setdisplay") {
        if (arguments.size() < 3) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            return false;
        }
        const std::string &slot = arguments[2];
        if (!Scoreboard::isDisplaySlot(slot)) {
            sender.sendTranslation("commands.scoreboard.objectives.setdisplay.invalidSlot", {slot});
            return false;
        }
        if (arguments.size() < 4) {
            scoreboard.clearDisplay(slot);
            sender.sendTranslation("commands.scoreboard.objectives.setdisplay.successCleared", {slot});
            return true;
        }
        const ScoreboardObjective *objective = scoreboard.getObjective(arguments[3]);
        if (objective == nullptr) {
            sender.sendTranslation("commands.scoreboard.objectiveNotFound", {arguments[3]});
            return false;
        }
        // the command starts descending
        ObjectiveSortOrder order = ObjectiveSortOrder::Descending;
        if (arguments.size() >= 5 && slot != Scoreboard::SLOT_BELOW_NAME) {
            if (arguments[4] == "ascending")
                order = ObjectiveSortOrder::Ascending;
            else if (arguments[4] != "descending") {
                sender.sendTranslation("commands.generic.parameter.invalid", {arguments[4]});
                return false;
            }
        }
        scoreboard.setDisplay(slot, *objective, order);
        sender.sendTranslation("commands.scoreboard.objectives.setdisplay.successSet", {slot, objective->mName});
        return true;
    }

    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}

bool ScoreboardCommand::_players(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const std::string &action = arguments[1];
    if (action == "list")
        return _list(sender, arguments);
    if (action == "set" || action == "add" || action == "remove")
        return _change(sender, arguments);
    if (action == "reset")
        return _reset(sender, arguments);
    if (action == "test")
        return _test(sender, arguments);
    if (action == "random")
        return _random(sender, arguments);
    if (action == "operation")
        return _operation(sender, arguments);
    sender.sendTranslation("commands.generic.usage", {getUsage()});
    return false;
}

bool ScoreboardCommand::_list(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Scoreboard &scoreboard = mHandler.getScoreboard();
    if (arguments.size() < 3) {
        const std::vector<ScoreboardIdentity> &identities = scoreboard.getIdentities();
        if (identities.empty()) {
            sender.sendTranslation("commands.scoreboard.players.list.empty", {});
            return false;
        }
        std::string names;
        for (const ScoreboardIdentity &identity: identities)
            names += (names.empty() ? "" : ", ") + mHandler.getScoreboardName(identity);
        sender.sendTranslation("commands.scoreboard.players.list.count", {std::to_string(identities.size())});
        sender.sendMessage(names);
        return true;
    }

    const std::vector<int64_t> ids = participants(mHandler, sender, arguments[2], false);
    if (ids.empty()) {
        sender.sendTranslation("commands.scoreboard.players.list.player.empty", {arguments[2]});
        return false;
    }
    for (int64_t id: ids) {
        const ScoreboardIdentity *identity = scoreboard.findIdentity(id);
        if (identity == nullptr)
            continue;
        const std::string name = mHandler.getScoreboardName(*identity);
        size_t count = 0;
        for (const std::unique_ptr<ScoreboardObjective> &objective: scoreboard.getObjectives())
            count += objective->find(id) != nullptr ? 1 : 0;
        sender.sendTranslation("commands.scoreboard.players.list.player.count", {std::to_string(count), name});
        for (const std::unique_ptr<ScoreboardObjective> &objective: scoreboard.getObjectives()) {
            if (const int32_t *score = objective->find(id))
                sender.sendTranslation("commands.scoreboard.players.list.player.entry",
                                       {std::to_string(*score), objective->mDisplayName, objective->mName});
        }
    }
    return true;
}

bool ScoreboardCommand::_change(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Scoreboard &scoreboard = mHandler.getScoreboard();
    const std::string &action = arguments[1];
    int32_t count = 0;
    if (arguments.size() < 5) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    if (!parseInt(arguments[4], count)) {
        sender.sendTranslation("commands.generic.num.invalid", {arguments[4]});
        return false;
    }
    ScoreboardObjective *objective = scoreboard.getObjective(arguments[3]);
    if (objective == nullptr) {
        sender.sendTranslation("commands.scoreboard.objectiveNotFound", {arguments[3]});
        return false;
    }
    const std::vector<int64_t> ids = participants(mHandler, sender, arguments[2], true);
    if (ids.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    size_t changed = 0;
    int32_t last = 0;
    std::string lastName;
    for (int64_t id: ids) {
        const int32_t *current = objective->find(id);
        int32_t value = count;
        // an add or remove past the int range is refused, the score kept
        if (action != "set"
            && !checkedAdd(current == nullptr ? 0 : *current, action == "add" ? count : -count, value)) {
            sender.sendTranslation("commands.scoreboard.players.operation.integerOverflow", {});
            continue;
        }
        scoreboard.setScore(*objective, id, value);
        if (const ScoreboardIdentity *identity = scoreboard.findIdentity(id))
            lastName = mHandler.getScoreboardName(*identity);
        last = value;
        ++changed;
    }
    if (changed == 0)
        return false;

    if (action == "set") {
        if (changed == 1)
            sender.sendTranslation("commands.scoreboard.players.set.success",
                                   {objective->mName, lastName, std::to_string(count)});
        else
            sender.sendTranslation("commands.scoreboard.players.set.multiple.success",
                                   {objective->mName, std::to_string(changed), std::to_string(count)});
        return true;
    }
    const std::string prefix = "commands.scoreboard.players." + action;
    if (changed == 1)
        sender.sendTranslation(prefix + ".success",
                               {std::to_string(count), objective->mName, lastName, std::to_string(last)});
    else
        sender.sendTranslation(prefix + ".multiple.success",
                               {std::to_string(count), objective->mName, std::to_string(changed)});
    return true;
}

bool ScoreboardCommand::_reset(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Scoreboard &scoreboard = mHandler.getScoreboard();
    if (arguments.size() < 3) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    ScoreboardObjective *objective = nullptr;
    if (arguments.size() >= 4) {
        objective = scoreboard.getObjective(arguments[3]);
        if (objective == nullptr) {
            sender.sendTranslation("commands.scoreboard.objectiveNotFound", {arguments[3]});
            return false;
        }
    }

    const std::vector<int64_t> ids = participants(mHandler, sender, arguments[2], false);
    if (ids.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }
    for (int64_t id: ids) {
        const ScoreboardIdentity *identity = scoreboard.findIdentity(id);
        if (identity == nullptr)
            continue;
        // the name first: an identity left without scores is dropped
        const std::string name = mHandler.getScoreboardName(*identity);
        if (objective != nullptr) {
            scoreboard.resetScore(*objective, id);
            sender.sendTranslation("commands.scoreboard.players.resetscore.success", {objective->mName, name});
        } else {
            scoreboard.resetScores(id);
            sender.sendTranslation("commands.scoreboard.players.reset.success", {name});
        }
    }
    return true;
}

bool ScoreboardCommand::_test(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Scoreboard &scoreboard = mHandler.getScoreboard();
    int32_t min = INT32_MIN, max = INT32_MAX;
    if (arguments.size() < 5) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    if (!parseBound(arguments[4], INT32_MIN, min)
        || (arguments.size() >= 6 && !parseBound(arguments[5], INT32_MAX, max))) {
        sender.sendTranslation("commands.generic.num.invalid", {arguments.size() >= 6 ? arguments[5] : arguments[4]});
        return false;
    }
    const ScoreboardObjective *objective = scoreboard.getObjective(arguments[3]);
    if (objective == nullptr) {
        sender.sendTranslation("commands.scoreboard.objectiveNotFound", {arguments[3]});
        return false;
    }

    const std::vector<int64_t> ids = participants(mHandler, sender, arguments[2], false);
    if (ids.empty()) {
        sender.sendTranslation("commands.scoreboard.players.score.notFound", {objective->mName, arguments[2]});
        return false;
    }
    bool passed = true;
    for (int64_t id: ids) {
        const ScoreboardIdentity *identity = scoreboard.findIdentity(id);
        const int32_t *score = objective->find(id);
        if (identity == nullptr || score == nullptr) {
            sender.sendTranslation(
                    "commands.scoreboard.players.score.notFound",
                    {objective->mName, identity == nullptr ? arguments[2] : mHandler.getScoreboardName(*identity)});
            passed = false;
            continue;
        }
        const bool inside = *score >= min && *score <= max;
        sender.sendTranslation(inside ? "commands.scoreboard.players.test.success"
                                      : "commands.scoreboard.players.test.failed",
                               {std::to_string(*score), std::to_string(min), std::to_string(max)});
        passed = passed && inside;
    }
    return passed;
}

bool ScoreboardCommand::_random(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Scoreboard &scoreboard = mHandler.getScoreboard();
    int32_t min = 0, max = 0;
    if (arguments.size() < 6) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    if (!parseInt(arguments[4], min) || !parseInt(arguments[5], max)) {
        sender.sendTranslation("commands.generic.num.invalid",
                               {parseInt(arguments[4], min) ? arguments[5] : arguments[4]});
        return false;
    }
    // the game refuses min >= max
    if (min >= max) {
        sender.sendTranslation("commands.scoreboard.players.random.invalidRange",
                               {std::to_string(min), std::to_string(max)});
        return false;
    }
    ScoreboardObjective *objective = scoreboard.getObjective(arguments[3]);
    if (objective == nullptr) {
        sender.sendTranslation("commands.scoreboard.objectiveNotFound", {arguments[3]});
        return false;
    }
    const std::vector<int64_t> ids = participants(mHandler, sender, arguments[2], true);
    if (ids.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    static std::mt19937 random{std::random_device{}()};
    // min + nextInt(max - min + 1); mt19937 output is the same everywhere, distributions are not
    const uint64_t range = (uint64_t) ((int64_t) max - min + 1);
    // the game has no random success line: set.success
    for (int64_t id: ids) {
        const int32_t value = (int32_t) ((int64_t) min + (int64_t) ((uint64_t) random() % range));
        scoreboard.setScore(*objective, id, value);
        if (const ScoreboardIdentity *identity = scoreboard.findIdentity(id))
            sender.sendTranslation("commands.scoreboard.players.set.success",
                                   {objective->mName, mHandler.getScoreboardName(*identity), std::to_string(value)});
    }
    return true;
}

bool ScoreboardCommand::_operation(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    Scoreboard &scoreboard = mHandler.getScoreboard();
    if (arguments.size() < 7) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }
    const std::string &operation = arguments[4];
    if (!isOperation(operation)) {
        sender.sendTranslation("commands.scoreboard.players.operation.invalidOperation", {operation});
        return false;
    }
    ScoreboardObjective *targetObjective = scoreboard.getObjective(arguments[3]);
    ScoreboardObjective *sourceObjective = scoreboard.getObjective(arguments[6]);
    if (targetObjective == nullptr || sourceObjective == nullptr) {
        sender.sendTranslation("commands.scoreboard.objectiveNotFound",
                               {targetObjective == nullptr ? arguments[3] : arguments[6]});
        return false;
    }

    if (arguments[2] == "*" && arguments[5] == "*") {
        sender.sendTranslation("commands.scoreboard.noMultiWildcard", {});
        return false;
    }
    const std::vector<int64_t> sources = participants(mHandler, sender, arguments[5], false);
    std::vector<int64_t> scored;
    for (int64_t id: sources) {
        if (sourceObjective->find(id) != nullptr)
            scored.push_back(id);
    }
    if (scored.empty()) {
        sender.sendTranslation("commands.scoreboard.players.operation.notFound",
                               {sourceObjective->mName, arguments[5]});
        return false;
    }
    const std::vector<int64_t> targets = participants(mHandler, sender, arguments[2], true);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    size_t updated = 0;
    for (int64_t target: targets) {
        // a target without a score starts from 0
        const int32_t *current = targetObjective->find(target);
        int32_t value = current == nullptr ? 0 : *current;
        bool overflow = false;
        for (int64_t source: scored) {
            int32_t other = *sourceObjective->find(source);
            if (!applyOperation(operation, value, other)) {
                overflow = true;
                break;
            }
            if (operation == "><")
                scoreboard.setScore(*sourceObjective, source, other);
        }
        if (overflow) {
            sender.sendTranslation("commands.scoreboard.players.operation.integerOverflow", {});
            continue;
        }
        scoreboard.setScore(*targetObjective, target, value);
        ++updated;
    }
    if (updated == 0)
        return false;
    sender.sendTranslation("commands.scoreboard.players.operation.success",
                           {targetObjective->mName, std::to_string(updated)});
    return true;
}
