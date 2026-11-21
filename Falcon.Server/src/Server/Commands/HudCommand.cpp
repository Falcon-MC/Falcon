#include "Server/Commands/HudCommand.h"

#include "Network/ServerNetworkHandler.h"
#include "World/Actor/Player/ServerPlayer.h"

#include <algorithm>

namespace {
    // the game's HudElement enum after "all"; an element's index is its HUD element id
    const std::vector<std::string> &hudElements() {
        static const std::vector<std::string> ELEMENTS = {
                "paperdoll", "armor",     "tooltips",    "touch_controls", "crosshair",      "hotbar",   "health",
                "progress_bar", "hunger", "air_bubbles", "horse_health",   "status_effects", "item_text"};
        return ELEMENTS;
    }
}

HudCommand::HudCommand(ServerNetworkHandler &handler)
        : Command("hud", "commands.hud.description", "/hud <target> <hide|reset> [hud_element]"), mHandler(handler) {}

std::vector<CommandOverloadData> HudCommand::getOverloads() const {
    std::vector<std::string> elements = {"all"};
    elements.insert(elements.end(), hudElements().begin(), hudElements().end());

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("target"));
    overload.mParameters.push_back(makeEnumParameter("visible", "HudVisibility", {"hide", "reset"}));
    overload.mParameters.push_back(makeEnumParameter("hud_element", "HudElement", elements, true));
    return {overload};
}

bool HudCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    const bool shaped = (arguments.size() == 2 || arguments.size() == 3)
                        && (arguments[1] == "hide" || arguments[1] == "reset");
    const std::vector<std::string> &elements = hudElements();
    const auto element = arguments.size() == 3 ? std::find(elements.begin(), elements.end(), arguments[2])
                                               : elements.end();
    const bool all = arguments.size() < 3 || arguments[2] == "all";
    if (!shaped || (!all && element == elements.end())) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    const bool hidden = arguments[1] == "hide";
    std::vector<int32_t> only;
    if (!all)
        only.push_back((int32_t) (element - elements.begin()));
    for (ServerPlayer *target: targets)
        target->setHudVisibility(hidden, all ? nullptr : &only);
    sender.sendTranslation("commands.hud.success", {});
    return true;
}
