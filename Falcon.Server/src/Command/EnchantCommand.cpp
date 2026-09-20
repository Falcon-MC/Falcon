#include "Command/EnchantCommand.h"

#include "Actor/ServerPlayer.h"
#include "Item/ItemEnchantments.h"
#include "Network/Handler/ServerNetworkHandler.h"

#include <cstdlib>

EnchantCommand::EnchantCommand(ServerNetworkHandler &handler)
        : Command("enchant", "commands.enchant.description",
                  "/enchant <player> <enchantment> [level]"),
          mHandler(handler) {}

bool EnchantCommand::_parseLevel(const std::string &value, int32_t &out) {
    if (value.empty()) {
        return false;
    }

    for (char character: value) {
        if (character < '0' || character > '9') {
            return false;
        }
    }

    out = (int32_t) strtol(value.c_str(), nullptr, 10);
    return true;
}

bool EnchantCommand::execute(CommandOrigin &sender, const std::vector<std::string> &arguments) {
    if (arguments.size() < 2) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    const std::vector<ServerPlayer *> targets = mHandler.resolveTargets(sender, arguments[0]);
    if (targets.empty()) {
        sender.sendTranslation("commands.generic.noTargetMatch", {});
        return false;
    }

    const EnchantmentData *enchantment = EnchantmentTable::find(arguments[1]);
    if (enchantment == nullptr) {
        sender.sendTranslation("commands.enchant.notFound", {arguments[1]});
        return false;
    }

    int32_t level = 1;
    if (arguments.size() > 2 && !_parseLevel(arguments[2], level)) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    if (level < 1 || level > enchantment->mMaxLevel) {
        sender.sendTranslation("commands.generic.usage", {getUsage()});
        return false;
    }

    bool enchanted = false;

    for (ServerPlayer *target: targets) {
        PlayerInventory &inventory = target->getInventory();
        ItemStack item = inventory.getItemInHand();

        if (item.isAir() || item.mCount <= 0) {
            sender.sendTranslation("commands.enchant.noItem", {target->getName()});
            continue;
        }

        if (item.mDefinition->getIdentifier() == "minecraft:book") {
            std::shared_ptr<ItemDefinition> enchantedBook =
                    mHandler.getItemDefinitions().getDefinition("minecraft:enchanted_book");

            if (enchantedBook != nullptr) {
                item.mDefinition = enchantedBook;
            }
        }

        const EnchantmentData *conflict = nullptr;
        const EnchantmentApplyResult result = ItemEnchantments::apply(item, *enchantment, level, &conflict);

        if (result == EnchantmentApplyResult::IncompatibleItem) {
            sender.sendTranslation("commands.enchant.noItem", {target->getName()});
            continue;
        }

        if (result == EnchantmentApplyResult::IncompatibleEnchantment) {
            sender.sendTranslation("commands.enchant.notFound", {enchantment->mDisplayName});
            continue;
        }

        if (result != EnchantmentApplyResult::Success) {
            sender.sendTranslation("commands.generic.usage", {getUsage()});
            continue;
        }

        inventory.setItemInHand(item);
        target->getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory,
                                               inventory.getSelectedSlot());

        enchanted = true;
        sender.sendTranslation("commands.enchant.success", {target->getName()});

        if (target != sender.asPlayer())
            target->sendTranslation("commands.enchant.success", {target->getName()});
    }

    return enchanted;
}

std::vector<CommandOverloadData> EnchantCommand::getOverloads() const {
    CommandParamData enchantmentParameter;
    enchantmentParameter.mName = "enchantmentName";
    enchantmentParameter.mHasEnumData = true;
    enchantmentParameter.mEnumData.mName = "Enchant";

    const EnchantmentData *entries = EnchantmentTable::getAll();
    for (size_t index = 0; index < EnchantmentTable::getCount(); index++)
        enchantmentParameter.mEnumData.mValues.push_back(entries[index].mName);

    CommandParamData levelParameter;
    levelParameter.mName = "level";
    levelParameter.mOptional = true;
    levelParameter.mHasType = true;
    levelParameter.mType = CommandParamType::Int;

    CommandOverloadData overload;
    overload.mParameters.push_back(makePlayerParameter("player"));
    overload.mParameters.push_back(enchantmentParameter);
    overload.mParameters.push_back(levelParameter);

    return {overload};
}
