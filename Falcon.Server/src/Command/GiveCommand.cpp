#include "Command/GiveCommand.h"

#include "Actor/ServerPlayer.h"
#include "Inventory/InventoryManager.h"
#include "Item/StringToItemParser.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cstdlib>

namespace {
    const int MAX_GIVE_COUNT = 32767;
    const int MAX_DATA_VALUE = 32767;
}

GiveCommand::GiveCommand(ServerNetworkHandler &handler)
        : Command("give", "commands.give.description", "/give <player> <item> [amount] [data]"),
          mHandler(handler) {}

std::vector<CommandOverloadData> GiveCommand::getOverloads() const {
    const CommandParamData playerParameter = makePlayerParameter("player");

    CommandParamData itemParameter;
    itemParameter.mName = "itemName";
    itemParameter.mHasEnumData = true;
    itemParameter.mEnumData.mName = "Item";
    itemParameter.mEnumData.mValues = StringToItemParser::getInstance().getKnownAliases();

    CommandParamData amountParameter;
    amountParameter.mName = "amount";
    amountParameter.mOptional = true;
    amountParameter.mHasType = true;
    amountParameter.mType = CommandParamType::Int;

    CommandParamData dataParameter;
    dataParameter.mName = "data";
    dataParameter.mOptional = true;
    dataParameter.mHasType = true;
    dataParameter.mType = CommandParamType::Int;

    CommandOverloadData overload;
    overload.mParameters.push_back(playerParameter);
    overload.mParameters.push_back(itemParameter);
    overload.mParameters.push_back(amountParameter);
    overload.mParameters.push_back(dataParameter);

    return {overload};
}

bool GiveCommand::_parseCount(const std::string &value, int &out) {
    if (value.empty())
        return false;

    for (char character: value) {
        if (character < '0' || character > '9')
            return false;
    }

    const long parsed = strtol(value.c_str(), nullptr, 10);
    if (parsed < 1 || parsed > MAX_GIVE_COUNT)
        return false;

    out = (int) parsed;
    return true;
}

bool GiveCommand::_parseData(const std::string &value, int &out) {
    if (value.empty())
        return false;

    for (char character: value) {
        if (character < '0' || character > '9')
            return false;
    }

    const long parsed = strtol(value.c_str(), nullptr, 10);
    if (parsed < 0 || parsed > MAX_DATA_VALUE)
        return false;

    out = (int) parsed;
    return true;
}

bool GiveCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.player.notFound", {});
        return false;
    }

    Item item;
    if (!StringToItemParser::getInstance().parse(arguments[1], item)) {
        sender.sendTranslation("commands.give.item.notFound", {arguments[1]});
        return false;
    }

    std::shared_ptr<ItemDefinition> definition =
            mHandler.getItemDefinitions().getDefinition(item.getIdentifier());

    if (definition == nullptr) {
        sender.sendTranslation("commands.give.item.notFound", {arguments[1]});
        return false;
    }

    int count = item.getMaxStackSize();
    if (arguments.size() > 2 && !_parseCount(arguments[2], count)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    int data = 0;
    if (arguments.size() > 3 && !_parseData(arguments[3], data)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    for (ServerPlayer *target: targets) {
        ItemStack stack;
        stack.mDefinition = definition;
        stack.mBlockDefinition = mHandler.getBlockDefinitions().getDefinition(item.getIdentifier());
        stack.mCount = count;
        stack.mDamage = data;

        std::vector<int> touchedSlots;
        const int remaining = target->getInventory().addItemPartial(stack, touchedSlots);
        const int given = count - remaining;

        if (given <= 0) {
            sender.sendTranslation("commands.give.success", {item.getName(), "0", target->getName()});
            continue;
        }

        for (const int slot: touchedSlots)
            target->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);

        sender.sendTranslation("commands.give.success", {item.getName(), std::to_string(given), target->getName()});
    }

    return true;
}
