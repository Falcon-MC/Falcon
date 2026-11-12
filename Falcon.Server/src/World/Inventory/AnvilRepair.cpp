#include "World/Inventory/AnvilRepair.h"

#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemData.h"
#include "World/Item/ItemProperties.h"
#include "Scripting/Content/CustomContentRegistry.h"
#include "World/Level/Block/BlockIdentifier.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

const char *AnvilRepair::TAG_REPAIR_COST = "RepairCost";

namespace {
    const char *const DISPLAY_TAG = "display";
    const char *const NAME_TAG = "Name";
    const char *const FILTERED_NAME_TAG = "FilteredName";

    const char *const BOOK = "minecraft:book";
    const char *const ENCHANTED_BOOK = "minecraft:enchanted_book";

    // Tier::getTierItem (sub_144743010): these planks for the wooden tier, these three blocks for stone
    const char *const PLANKS[] = {"minecraft:oak_planks",      "minecraft:spruce_planks", "minecraft:birch_planks",
                                  "minecraft:jungle_planks",   "minecraft:acacia_planks", "minecraft:dark_oak_planks",
                                  "minecraft:mangrove_planks", "minecraft:cherry_planks", "minecraft:bamboo_planks",
                                  "minecraft:crimson_planks",  "minecraft:warped_planks", "minecraft:pale_oak_planks",
                                  "minecraft:poplar_planks"};

    const char *const STONE_TOOL_MATERIALS[] = {"minecraft:cobblestone", "minecraft:cobbled_deepslate",
                                                "minecraft:blackstone"};

    const std::string &identifierOf(const ItemStack &item) {
        static const std::string empty;
        return item.isAir() ? empty : item.mDefinition->getIdentifier();
    }

    using BlockIdentifier::endsWith;

    template <size_t N>
    bool isOneOf(const std::string &identifier, const char *const (&list)[N]) {
        for (const char *entry: list) {
            if (identifier == entry)
                return true;
        }
        return false;
    }

    const ItemData *dataOf(const ItemStack &item) {
        return item.isAir() ? nullptr : ItemDataTable::find(identifierOf(item));
    }

    const CustomItemDefinition *customOf(const ItemStack &item) {
        return item.isAir() ? nullptr : CustomContentRegistry::getInstance().findItem(identifierOf(item));
    }

    // behavior pack items take their maximum durability from minecraft:durability
    int32_t maxDamageOf(const ItemStack &item) {
        const ItemData *data = dataOf(item);
        if (data != nullptr)
            return data->mMaxDurability;

        const CustomItemDefinition *custom = customOf(item);
        return custom == nullptr ? 0 : custom->mMaxDurability;
    }

    // repair_amount Molang: numbers, + - * /, parentheses, math.floor/ceil/round/abs/min/max, and the durability
    // queries of the repaired item (query.*) and the material (context.other->query.*)
    class RepairAmountParser {
    public:
        RepairAmountParser(const std::string &text, const ItemStack &item, const ItemStack &other)
                : mText(text), mItem(item), mOther(other) {}

        bool parse(double &out) {
            out = _sum();
            _skip();
            return mValid && mPosition == mText.size();
        }

    private:
        const std::string &mText;
        const ItemStack &mItem;
        const ItemStack &mOther;
        size_t mPosition = 0;
        bool mValid = true;

        void _skip() {
            while (mPosition < mText.size() && std::isspace((unsigned char) mText[mPosition]))
                ++mPosition;
        }

        bool _accept(char c) {
            _skip();
            if (mPosition < mText.size() && mText[mPosition] == c) {
                ++mPosition;
                return true;
            }
            return false;
        }

        double _sum() {
            double value = _product();
            while (mValid) {
                if (_accept('+'))
                    value += _product();
                else if (_accept('-'))
                    value -= _product();
                else
                    break;
            }
            return value;
        }

        double _product() {
            double value = _unary();
            while (mValid) {
                if (_accept('*')) {
                    value *= _unary();
                } else if (_accept('/')) {
                    const double divisor = _unary();
                    value = divisor == 0.0 ? 0.0 : value / divisor;
                } else {
                    break;
                }
            }
            return value;
        }

        double _unary() {
            if (_accept('-'))
                return -_unary();
            if (_accept('+'))
                return _unary();
            return _primary();
        }

        std::string _name() {
            _skip();
            std::string name;
            while (mPosition < mText.size()) {
                const char c = mText[mPosition];
                if (std::isalnum((unsigned char) c) || c == '_' || c == '.') {
                    name += (char) std::tolower((unsigned char) c);
                    ++mPosition;
                } else if (c == '-' && mPosition + 1 < mText.size() && mText[mPosition + 1] == '>') {
                    name += "->";
                    mPosition += 2;
                } else {
                    break;
                }
            }
            return name;
        }

        static std::string _expand(const std::string &name) {
            std::string expanded = name;
            if (expanded.rfind("c.", 0) == 0)
                expanded = "context." + expanded.substr(2);

            const size_t arrow = expanded.find("->");
            const size_t queryAt = arrow == std::string::npos ? 0 : arrow + 2;
            if (expanded.compare(queryAt, 2, "q.") == 0)
                expanded = expanded.substr(0, queryAt) + "query." + expanded.substr(queryAt + 2);
            return expanded;
        }

        static double _durability(const ItemStack &item, const std::string &query) {
            const int32_t maxDamage = maxDamageOf(item);
            if (query == "query.max_durability")
                return maxDamage;
            if (query == "query.remaining_durability")
                return std::max(0, maxDamage - item.mDamage);
            return 0.0;
        }

        double _primary() {
            if (_accept('(')) {
                const double value = _sum();
                if (!_accept(')'))
                    mValid = false;
                return value;
            }

            _skip();
            if (mPosition < mText.size()
                && (std::isdigit((unsigned char) mText[mPosition]) || mText[mPosition] == '.')) {
                char *end = nullptr;
                const double value = std::strtod(mText.c_str() + mPosition, &end);
                mPosition = (size_t) (end - mText.c_str());
                if (mPosition < mText.size() && (mText[mPosition] == 'f' || mText[mPosition] == 'F'))
                    ++mPosition;
                return value;
            }

            const std::string name = _expand(_name());
            if (name.empty()) {
                mValid = false;
                return 0.0;
            }

            if (name.rfind("math.", 0) == 0)
                return _math(name);

            const std::string otherPrefix = "context.other->";
            if (name.rfind(otherPrefix, 0) == 0)
                return _durability(mOther, name.substr(otherPrefix.size()));
            return _durability(mItem, name);
        }

        double _math(const std::string &name) {
            std::vector<double> arguments;
            if (_accept('(')) {
                if (!_accept(')')) {
                    do {
                        arguments.push_back(_sum());
                    } while (mValid && _accept(','));
                    if (!_accept(')'))
                        mValid = false;
                }
            }

            const double first = arguments.empty() ? 0.0 : arguments[0];
            const double second = arguments.size() < 2 ? 0.0 : arguments[1];
            if (name == "math.floor")
                return std::floor(first);
            if (name == "math.ceil")
                return std::ceil(first);
            if (name == "math.round")
                return std::round(first);
            if (name == "math.abs")
                return std::fabs(first);
            if (name == "math.min")
                return std::min(first, second);
            if (name == "math.max")
                return std::max(first, second);
            mValid = false;
            return 0.0;
        }
    };

    // RepairableItemComponent (sub_148F1BBF0): the first repair_items entry listing the material gives the amount,
    // evaluated with the material as context.other; -1 without an entry
    int32_t customRepairAmount(const ItemStack &item, const ItemStack &material) {
        const CustomItemDefinition *custom = customOf(item);
        if (custom == nullptr || material.isAir())
            return -1;

        const std::string &materialId = identifierOf(material);
        for (const CustomRepairEntry &entry: custom->mRepairItems) {
            if (std::find(entry.mItems.begin(), entry.mItems.end(), materialId) == entry.mItems.end())
                continue;

            double amount = 0.0;
            RepairAmountParser parser(entry.mAmount, item, material);
            if (!parser.parse(amount))
                return -1;
            return std::max(0, (int32_t) amount);
        }
        return -1;
    }

    // ItemStackBase::isDamageableItem (sub_141ACF450): damageable and not Unbreakable
    bool isDamageable(const ItemStack &item) {
        return maxDamageOf(item) > 0 && !ItemProperties::isUnbreakable(item);
    }

    bool hasEnchantList(const ItemStack &item) {
        if (!item.mTag.isCompound())
            return false;

        const Tag *list = item.mTag.get(ItemEnchantments::TAG_ENCHANTMENTS);
        return list != nullptr && list->getType() == Tag::Type::List;
    }

    // sub_141AD0BD0: a book only when it is an enchanted book carrying enchantments
    bool isEnchantedBook(const ItemStack &item) {
        return identifierOf(item) == ENCHANTED_BOOK && hasEnchantList(item);
    }

    bool acceptsEveryEnchantment(const std::string &identifier) {
        return identifier == BOOK || identifier == ENCHANTED_BOOK;
    }

    // the anvil keeps maps for their own recipes: they never repair or merge here
    bool isMap(const ItemStack &item) {
        const std::string &identifier = identifierOf(item);
        return identifier == "minecraft:filled_map" || identifier == "minecraft:empty_map";
    }

    // sub_148EEB790: chemistry glow stick and sparkler never merge with a second copy
    bool isUnmergeable(const ItemStack &item) {
        const std::string &identifier = identifierOf(item);
        return identifier == "minecraft:glow_stick" || identifier == "minecraft:sparkler";
    }

    bool isSpear(const ItemStack &item) {
        return endsWith(identifierOf(item), "_spear");
    }

    bool matchesToolTier(int32_t tier, const std::string &material) {
        switch ((ToolTier) tier) {
            case ToolTier::Wooden:
                return isOneOf(material, PLANKS);
            case ToolTier::Gold:
                return material == "minecraft:gold_ingot";
            case ToolTier::Stone:
                return isOneOf(material, STONE_TOOL_MATERIALS);
            case ToolTier::Copper:
                return material == "minecraft:copper_ingot";
            case ToolTier::Iron:
                return material == "minecraft:iron_ingot";
            case ToolTier::Diamond:
                return material == "minecraft:diamond";
            case ToolTier::Netherite:
                return material == "minecraft:netherite_ingot";
            default:
                return false;
        }
    }

    // ArmorItem::getTierItem (sub_141A53300)
    bool matchesArmorTier(int32_t tier, const std::string &material) {
        switch ((ArmorTier) tier) {
            case ArmorTier::Leather:
                return material == "minecraft:leather";
            case ArmorTier::Iron:
            case ArmorTier::Chain:
                return material == "minecraft:iron_ingot";
            case ArmorTier::Copper:
                return material == "minecraft:copper_ingot";
            case ArmorTier::Gold:
                return material == "minecraft:gold_ingot";
            case ArmorTier::Diamond:
                return material == "minecraft:diamond";
            case ArmorTier::Netherite:
                return material == "minecraft:netherite_ingot";
            default:
                return false;
        }
    }

    // RepairableItemComponent::_repairItem (sub_148F1C560): each material item takes up to amount damage off;
    // returns the items used
    int32_t repairWith(ItemStack &result, int32_t materialCount, int32_t amount) {
        int32_t step = std::min(amount, result.mDamage);
        int32_t used = 0;
        while (step > 0 && used < materialCount) {
            result.mDamage -= step;
            step = std::min(amount, result.mDamage);
            ++used;
        }
        return used;
    }

    std::string currentName(const ItemStack &item) {
        if (AnvilRepair::hasCustomName(item))
            return item.mTag.get(DISPLAY_TAG)->getString(NAME_TAG, std::string());

        const ItemData *data = dataOf(item);
        return data == nullptr ? identifierOf(item) : std::string(data->mName);
    }

    Tag &displayOf(ItemStack &item) {
        if (!item.mTag.isCompound())
            item.mTag = Tag::ofCompound();

        Tag *display = item.mTag.get(DISPLAY_TAG);
        if (display == nullptr || !display->isCompound()) {
            item.mTag.put(DISPLAY_TAG, Tag::ofCompound());
            display = item.mTag.get(DISPLAY_TAG);
        }
        return *display;
    }

    // ItemStackBase::resetHoverName (sub_141AD96B0)
    void resetHoverName(ItemStack &item) {
        if (!item.mTag.isCompound())
            return;

        Tag *display = item.mTag.get(DISPLAY_TAG);
        if (display == nullptr || !display->isCompound())
            return;

        display->remove(NAME_TAG);
        display->remove(FILTERED_NAME_TAG);
        if (display->isEmpty())
            item.mTag.remove(DISPLAY_TAG);
    }

    int32_t enchantWeight(EnchantmentRarity rarity, bool bookModifier) {
        switch (rarity) {
            case EnchantmentRarity::Common:
                return 1;
            case EnchantmentRarity::Uncommon:
                return bookModifier ? 1 : 2;
            case EnchantmentRarity::Rare:
                return bookModifier ? 2 : 4;
            case EnchantmentRarity::Mythic:
                return bookModifier ? 4 : 8;
        }
        return 0;
    }
}

int32_t AnvilRepair::getRepairCost(const ItemStack &item) {
    if (item.isAir() || !item.mTag.isCompound())
        return 0;

    return item.mTag.getInt(TAG_REPAIR_COST, 0);
}

bool AnvilRepair::hasCustomName(const ItemStack &item) {
    if (item.isAir() || !item.mTag.isCompound())
        return false;

    const Tag *display = item.mTag.get(DISPLAY_TAG);
    return display != nullptr && display->isCompound() && display->contains(NAME_TAG, Tag::Type::String);
}

int32_t AnvilRepair::getEnchantValue(const std::vector<EnchantmentInstance> &enchantments, bool bookModifier) {
    int32_t value = 0;
    for (const EnchantmentInstance &instance: enchantments) {
        const EnchantmentData *data = EnchantmentTable::findById(instance.mId);
        if (data != nullptr)
            value += instance.mLevel * enchantWeight(data->mRarity, bookModifier);
    }
    return value;
}

int32_t AnvilRepair::mergeEnchantments(const ItemStack &input, const ItemStack &material, ItemStack &result,
                                       bool bookModifier, const std::shared_ptr<ItemDefinition> &enchantedBook) {
    if (!hasEnchantList(material))
        return 0;

    // ItemEnchants::addEnchants (sub_14555AAA0) runs canEnchant (sub_14555A940) on each: skipped when the item
    // can't hold it, it conflicts with one on the item, or is at max level; equal levels go up by one, else the higher wins
    const std::string &identifier = identifierOf(result);
    std::vector<EnchantmentInstance> merged = ItemEnchantments::read(result);
    int32_t changed = 0;
    for (const EnchantmentInstance &incoming: ItemEnchantments::read(material)) {
        const EnchantmentData *data = EnchantmentTable::findById(incoming.mId);
        if (data == nullptr)
            continue;

        if (!acceptsEveryEnchantment(identifier) && !ItemEnchantments::canApply(identifier, *data))
            continue;

        bool handled = false;
        for (EnchantmentInstance &existing: merged) {
            if (existing.mId == incoming.mId) {
                if (existing.mLevel < data->mMaxLevel) {
                    existing.mLevel = existing.mLevel == incoming.mLevel ? existing.mLevel + 1
                                                                         : std::max(existing.mLevel, incoming.mLevel);
                    ++changed;
                }
                handled = true;
                break;
            }

            const EnchantmentData *other = EnchantmentTable::findById(existing.mId);
            if (other != nullptr && (other->mIncompatibleGroups & data->mIncompatibleGroups) != 0) {
                handled = true;
                break;
            }
        }

        if (!handled) {
            merged.push_back(incoming);
            ++changed;
        }
    }

    // EnchantUtils::applyEnchant (sub_1436AB5D0) turns a plain book into an enchanted book
    if (changed > 0) {
        if (identifier == BOOK && enchantedBook != nullptr)
            result.mDefinition = enchantedBook;
        ItemEnchantments::write(result, merged);
    }

    const int32_t before = getEnchantValue(ItemEnchantments::read(input), bookModifier);
    const int32_t after = getEnchantValue(ItemEnchantments::read(result), bookModifier);
    if (after > before && input.mCount >= 2)
        return TOO_EXPENSIVE_COST;

    return after - before;
}

bool AnvilRepair::isValidRepairItem(const ItemStack &item, const ItemStack &material) {
    const ItemData *data = dataOf(item);
    if (data == nullptr || material.isAir())
        return false;

    const std::string &identifier = identifierOf(item);
    const std::string &repairItem = identifierOf(material);

    // MaceItem (sub_141AE0880), wolf armor (sub_1447CC510), the shield's wooden tier (sub_14357EC30), elytra and
    // turtle shell materials of ArmorItem::getTierItem (sub_141A53300)
    if (identifier == "minecraft:mace")
        return repairItem == "minecraft:breeze_rod";
    if (identifier == "minecraft:wolf_armor")
        return repairItem == "minecraft:armadillo_scute";
    if (identifier == "minecraft:shield")
        return isOneOf(repairItem, PLANKS);
    if (identifier == "minecraft:elytra")
        return repairItem == "minecraft:phantom_membrane";
    if (identifier == "minecraft:turtle_helmet")
        return repairItem == "minecraft:turtle_scute";

    // ArmorItem (sub_141A53520), then tiered tools and weapons (sub_147B9A490, sub_1447CB770)
    if (data->mArmorSlot != ArmorSlot::None)
        return matchesArmorTier(data->mArmorTier, repairItem);

    return matchesToolTier(data->mToolTier, repairItem);
}

AnvilResult AnvilRepair::createResult(const ItemStack &input, const ItemStack &material, const std::string *newName,
                                      bool creative, const std::shared_ptr<ItemDefinition> &enchantedBook) {
    AnvilResult outcome;
    if (input.isAir())
        return outcome;

    ItemStack result = input;
    const bool mapInput = isMap(input);
    const bool book = isEnchantedBook(material);
    const int32_t penalty = getRepairCost(input) + getRepairCost(material);
    int32_t cost = 0;

    if (!material.isAir()) {
        if (mapInput)
            return outcome;

        bool repaired = false;
        const bool sameItem = identifierOf(material) == identifierOf(input);
        int32_t amount = -1;
        if (isSpear(input)) {
            // spears repair through minecraft:repairable (sub_148F1BBF0): a second spear restores its remaining durability,
            // a tier material a quarter of max durability per item
            if (sameItem)
                amount = maxDamageOf(material) - material.mDamage;
            else if (dataOf(input) != nullptr && matchesToolTier(dataOf(input)->mToolTier, identifierOf(material)))
                amount = (int32_t) ((float) maxDamageOf(input) * 0.25f);
        } else {
            // behavior pack items repair through the same component, with the amount their pack gives
            amount = customRepairAmount(input, material);
        }

        if (amount >= 0) {
            repaired = true;
            const int32_t used = repairWith(result, material.mCount, amount);
            outcome.mMaterialCost = used;
            cost = sameItem ? used * 2 : used;
            if (sameItem) {
                const int32_t bonus = std::min(7 * maxDamageOf(result) / 100, result.mDamage);
                if (result.mDamage > bonus)
                    result.mDamage -= bonus;
                outcome.mMaterialCost = input.mCount;
            }

            const int32_t enchantCost = mergeEnchantments(input, material, result, book, enchantedBook);
            if (enchantCost > 0)
                cost += enchantCost;
        }

        if (!repaired) {
            if (isDamageable(result) && isValidRepairItem(input, material)) {
                const int32_t quarter = maxDamageOf(result) / 4;
                if (std::min(quarter, result.mDamage) <= 0)
                    return outcome;

                cost = repairWith(result, material.mCount, quarter);
                outcome.mMaterialCost = cost;
            } else {
                if (!book
                    && (identifierOf(input) != identifierOf(material) || isUnmergeable(material)
                        || !isDamageable(result)))
                    return outcome;

                int32_t durabilityCost = 0;
                if (!book && isDamageable(result)) {
                    const int32_t maxDamage = maxDamageOf(result);
                    const int32_t remaining = (maxDamageOf(input) - input.mDamage)
                                              + (maxDamageOf(material) - material.mDamage);
                    const int32_t damage = std::max(0, maxDamage - remaining - 12 * maxDamage / 100);
                    if (damage < result.mDamage) {
                        result.mDamage = damage;
                        durabilityCost = 2;
                    }
                }

                outcome.mMaterialCost = result.mCount;
                cost = durabilityCost + mergeEnchantments(input, material, result, book, enchantedBook);
            }
        }

        if (cost <= 0)
            return AnvilResult();
    }

    int32_t renameCost = 0;
    if (newName != nullptr) {
        if (newName->empty()) {
            if (hasCustomName(input)) {
                resetHoverName(result);
                renameCost = 1;
            }
        } else if (currentName(input) != *newName) {
            displayOf(result).putString(NAME_TAG, *newName);
            renameCost = 1;
        }
    }

    cost += renameCost;
    if (cost <= 0)
        return AnvilResult();

    int32_t total = cost + penalty;
    if (renameCost > 0 && renameCost == cost && total >= TOO_EXPENSIVE_COST)
        total = TOO_EXPENSIVE_COST - 1;

    if (total >= TOO_EXPENSIVE_COST && !creative)
        return AnvilResult();

    int32_t repairCost = std::max(getRepairCost(result), getRepairCost(material));
    if (!mapInput && (renameCost == 0 || cost != renameCost))
        repairCost = repairCost * 2 + 1;

    if (!result.mTag.isCompound())
        result.mTag = Tag::ofCompound();
    result.mTag.putInt(TAG_REPAIR_COST, repairCost);

    outcome.mResult = std::move(result);
    outcome.mCost = total;
    return outcome;
}
