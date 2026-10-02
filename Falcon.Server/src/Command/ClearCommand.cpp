#include "Command/ClearCommand.h"

#include "Actor/ServerPlayer.h"
#include "Item/StringToItemParser.h"
#include "Network/Handler/InventoryHandler.h"
#include "Network/Handler/ServerNetworkHandler.h"
#include "Protocol/Types/ItemDefinition.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <functional>

namespace {
    const int32_t CLEAR_EVERYTHING = -1;
    const int32_t COUNT_ONLY = 0;

    bool parseInteger(const std::string &value, int32_t &out) {
        if (value.empty())
            return false;

        char *end = nullptr;
        errno = 0;
        const long parsed = std::strtol(value.c_str(), &end, 10);
        if (end == value.c_str() || *end != '\0' || errno == ERANGE)
            return false;

        out = (int32_t) parsed;
        return true;
    }

    bool matchesFilter(const ItemStack &stack, const std::string &identifier, int32_t data) {
        if (stack.isAir() || stack.mCount <= 0 || stack.mDefinition == nullptr)
            return false;

        if (stack.mDefinition->getIdentifier() != identifier)
            return false;

        return data < 0 || stack.mDamage == data;
    }
}

ClearCommand::ClearCommand(ServerNetworkHandler &handler)
        : Command("clear", "commands.clear.description", "/clear [player] [itemName] [data] [maxCount]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> ClearCommand::getOverloads() const {
    CommandParamData playerParameter = makePlayerParameter("player");
    playerParameter.mOptional = true;

    CommandParamData itemParameter;
    itemParameter.mName = "itemName";
    itemParameter.mOptional = true;
    itemParameter.mHasType = true;
    itemParameter.mType = CommandParamType::String;

    CommandParamData dataParameter;
    dataParameter.mName = "data";
    dataParameter.mOptional = true;
    dataParameter.mHasType = true;
    dataParameter.mType = CommandParamType::Int;

    CommandParamData maxCountParameter;
    maxCountParameter.mName = "maxCount";
    maxCountParameter.mOptional = true;
    maxCountParameter.mHasType = true;
    maxCountParameter.mType = CommandParamType::Int;

    CommandOverloadData overload;
    overload.mParameters = {playerParameter, itemParameter, dataParameter, maxCountParameter};

    return {overload};
}

int32_t ClearCommand::clearMatching(ServerPlayer &target, const std::string &identifier, int32_t data,
                                    int32_t maxCount) {
    PlayerInventory &inventory = target.getInventory();
    const bool countOnly = maxCount == COUNT_ONLY;
    const bool unlimited = maxCount == CLEAR_EVERYTHING;
    int32_t removed = 0;

    const auto take = [&](const ItemStack &stack, const std::function<void(ItemStack)> &store) {
        if (!identifier.empty() && !matchesFilter(stack, identifier, data))
            return;
        if (identifier.empty() && (stack.isAir() || stack.mCount <= 0))
            return;

        int32_t taken = stack.mCount;
        if (!countOnly && !unlimited)
            taken = std::min(taken, maxCount - removed);

        if (taken <= 0)
            return;

        removed += taken;

        if (countOnly)
            return;

        ItemStack remaining = stack;
        remaining.mCount -= taken;
        store(remaining.mCount <= 0 ? ItemStack::air() : std::move(remaining));
    };

    for (int slot = 0; slot < PlayerInventory::CONTAINER_SIZE; ++slot)
        take(inventory.getItem(slot), [&](ItemStack item) { inventory.setItem(slot, std::move(item)); });

    for (int slot = 0; slot < PlayerInventory::ARMOR_SIZE; ++slot)
        take(inventory.getArmor(slot), [&](ItemStack item) { inventory.setArmor(slot, std::move(item)); });

    take(inventory.getOffhand(), [&](ItemStack item) { inventory.setOffhand(std::move(item)); });

    if (removed > 0 && !countOnly)
        target.getInventoryManager().syncAll();

    return removed;
}

bool ClearCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    std::vector<ServerPlayer *> targets;

    if (arguments.empty()) {
        ServerPlayer *self = sender.asPlayer();
        if (self == nullptr) {
            sender.sendTranslation("commands.generic.targetNotPlayer", {});
            return false;
        }

        targets.push_back(self);
    } else {
        targets = mHandler.resolveTargets(sender, arguments[0]);
    }

    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    std::string identifier;
    if (arguments.size() > 1) {
        Item parsed;
        if (!StringToItemParser::getInstance().parse(arguments[1], parsed)) {
            sender.sendTranslation("commands.give.item.notFound", {arguments[1]});
            return false;
        }

        identifier = parsed.getIdentifier();
    }

    int32_t data = CLEAR_EVERYTHING;
    if (arguments.size() > 2 && !parseInteger(arguments[2], data)) {
        sender.sendTranslation("commands.generic.num.invalid", {arguments[2]});
        return false;
    }

    int32_t maxCount = CLEAR_EVERYTHING;
    if (arguments.size() > 3 && !parseInteger(arguments[3], maxCount)) {
        sender.sendTranslation("commands.generic.num.invalid", {arguments[3]});
        return false;
    }

    bool cleared = false;

    for (ServerPlayer *target: targets) {
        const int32_t removed = clearMatching(*target, identifier, data, maxCount);

        if (removed == 0) {
            sender.sendTranslation("commands.clear.failure.no.items", {target->getName()});
            continue;
        }

        cleared = true;
        InventoryHandler::sendArmorContent(mHandler, *target);
        InventoryHandler::sendOffhandContent(mHandler, *target);
        InventoryHandler::sendHeldItem(mHandler, *target);
        sender.sendTranslation(maxCount == COUNT_ONLY ? "commands.clear.testing" : "commands.clear.success",
                               {target->getName(), std::to_string(removed)});
    }

    return cleared;
}
