#include "Server/Commands/RecipeCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/RecipeUnlocking.h"

namespace {
    const char *const EVERY_RECIPE = "*";
}

RecipeCommand::RecipeCommand(ServerNetworkHandler &handler)
        : Command("recipe", "commands.recipes.description", "/recipe <give|take> <player> <recipe|*>"),
          mHandler(handler) {}

std::vector<CommandOverloadData> RecipeCommand::getOverloads() const {
    CommandOverloadData change;
    change.mParameters = {makeEnumParameter("action", "RecipeAction", {"give", "take"}),
                          makePlayerParameter("player"), makeTypedParameter("recipe", CommandParamType::String)};
    return {change};
}

bool RecipeCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 3 || (arguments[0] != "give" && arguments[0] != "take")) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[1]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    const bool giving = arguments[0] == "give";
    const std::string &recipe = arguments[2];
    if (recipe == EVERY_RECIPE) {
        for (ServerPlayer *target: targets) {
            if (giving)
                RecipeUnlockSystem::giveAll(mHandler, *target);
            else
                RecipeUnlockSystem::takeAll(mHandler, *target);
            const char *message =
                    giving ? "commands.recipes.unlockedAllRecipes" : "commands.recipes.removedAllRecipes";
            sender.sendTranslation(message, {target->getName()});
        }
        return true;
    }

    const RecipeUnlockIndex &index = mHandler.getRecipeUnlockIndex();
    if (!index.contains(recipe)) {
        sender.sendTranslation("commands.recipes.notFound", {recipe});
        return false;
    }
    if (!index.isUnlockable(recipe)) {
        sender.sendTranslation("commands.recipes.notUnlockable", {recipe});
        return false;
    }

    bool changed = false;
    for (ServerPlayer *target: targets) {
        if (giving && RecipeUnlockSystem::give(mHandler, *target, recipe)) {
            sender.sendTranslation("commands.recipes.unlockedRecipe", {target->getName(), recipe});
            changed = true;
        } else if (!giving && RecipeUnlockSystem::take(mHandler, *target, recipe)) {
            sender.sendTranslation("commands.recipes.removedRecipe", {target->getName(), recipe});
            changed = true;
        } else {
            const char *message =
                    giving ? "commands.recipes.playerHasRecipe" : "commands.recipes.playerDoesNotHaveRecipe";
            sender.sendTranslation(message, {target->getName(), recipe});
        }
    }
    return changed;
}
