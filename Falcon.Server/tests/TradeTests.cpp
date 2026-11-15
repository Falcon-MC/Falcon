#include "Core/Json/Json.h"
#include "Network/ServerNetworkHandler.h"
#include "Server/PropertiesSettings.h"
#include "Server/ServerPaths.h"
#include "World/Actor/Definition/EntityDefinitions.h"
#include "World/Actor/Definition/EntityEvents.h"
#include "World/Actor/Mob/MobActor.h"
#include "World/Actor/Player/ServerPlayer.h"
#include "World/Inventory/ItemStackRequestHandler.h"
#include "World/Inventory/PlayerInventory.h"
#include "World/Actor/Trade/MerchantOffers.h"
#include "Protocol/Packets/UpdateTradePacket.h"
#include "World/Actor/Trade/TradeTable.h"
#include "World/Actor/Trade/TradeTableRegistry.h"
#include "World/Item/EnchantmentData.h"
#include "World/Item/ItemEnchantments.h"
#include "World/Level/Chunk/LevelChunk.h"
#include "World/Level/Level.h"
#include "World/Level/LevelGen/Biome/BiomeIds.h"
#include "World/Level/Storage/Loot/LootFunction.h"
#include "World/Level/World.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <random>
#include <set>
#include <stdexcept>
#include <string>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }

    const char *ARMORER_TABLE = "trading/economy_trades/armorer_trades.json";

    const char *const SCHEDULE_GROUPS[] = {"basic_schedule",
                                           "child_schedule",
                                           "work_schedule",
                                           "farmer_schedule",
                                           "fisher_schedule",
                                           "librarian_schedule",
                                           "jobless_schedule",
                                           "baby"};

    struct Arena {
        ServerNetworkHandler mOwner{"Trade", "", 1};
        Level *mLevel = nullptr;

        explicit Arena(const std::filesystem::path &directory) {
            const std::filesystem::path path = directory / "normal.properties";
            std::ofstream(path) << "difficulty=normal\n";
            mOwner.setProperties(PropertiesSettings(path.string()));
            mLevel = &mOwner.getWorlds().getDefault().getLevel(DimensionType::Overworld);
            for (int32_t x = -2; x <= 2; x++) {
                for (int32_t z = -2; z <= 2; z++) {
                    LevelChunk &chunk = mLevel->insertChunk(LevelChunk(x, z));
                    chunk.setPopulated(true);
                    chunk.setBiome(BiomeIds::PLAINS);
                }
            }
        }

        ServerPlayer &player(uint64_t runtimeId) {
            const NetworkIdentifier id(std::to_string(runtimeId), runtimeId);
            ServerPlayer &player = mOwner.getPlayers().try_emplace(id, id, runtimeId).first->second;
            player.getAttributes() = ActorAttributes::createPlayerDefaults();
            player.moveToLevel(*mLevel);
            player.setPosition(Vector3f(0.5f, 100.0f, 1.5f));
            player.setLoginState(ServerPlayer::LoginState::Spawned);
            player.getInventoryManager().attach(&player, &mOwner);
            return player;
        }

        MobActor &villager(const char *profession) {
            MobActor *mob = dynamic_cast<MobActor *>(
                    mOwner.spawnActor(*mLevel, "minecraft:villager_v2", Vector3f(0.5f, 100.0f, 0.5f)));
            require(mob != nullptr, "the villager did not spawn");
            mob->startDefinition(mOwner);
            for (const char *group: SCHEDULE_GROUPS)
                mob->removeComponentGroup(group);
            mob->addComponentGroup("adult");
            mob->addComponentGroup(profession);
            mob->tick(mOwner);
            return *mob;
        }
    };

    int32_t expectedOfferCount(const TradeTable &table) {
        int32_t count = 0;
        for (const TradeTier &tier: table.getTiers()) {
            for (const TradeGroup &group: tier.mGroups)
                count += group.mNumToSelect <= 0 ? (int32_t) group.mTrades.size()
                                                 : std::min(group.mNumToSelect, (int32_t) group.mTrades.size());
        }
        return count;
    }

    void armorerRollsEveryTier(Arena &arena) {
        const TradeTable *table = TradeTableRegistry::getInstance().get(ARMORER_TABLE);
        require(table != nullptr && !table->getTiers().empty(), "the armorer table was not loaded");

        MobActor &armorer = arena.villager("armorer");
        require(MobEconomy::isTrader(armorer), "an armorer has no economy trade table");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr, "the armorer rolled no offers");
        require((int32_t) offers->mOffers.size() == expectedOfferCount(*table),
                "the armorer did not get one offer per selected trade");
        require(armorer.getEconomy().getMaxTier() == (int32_t) table->getTiers().size() - 1,
                "the top tier is not the last tier of the table");

        require(offers->mTierExpRequirements.size() == table->getTiers().size(), "a tier requirement is missing");
        for (size_t tier = 0; tier < table->getTiers().size(); ++tier)
            require(offers->mTierExpRequirements[tier] == table->getTiers()[tier].mTotalExpRequired,
                    "a tier requirement differs from the table");

        std::set<int32_t> netIds;
        int32_t lastTier = 0;
        for (const MerchantOffer &offer: offers->mOffers) {
            require(!offer.mBuyA.isAir() && !offer.mSell.isAir(), "an offer lost an item");
            require(offer.mBuyCountA == offer.mBuyA.mCount, "the first price is not the rolled count");
            require(offer.mUses == 0 && offer.mDemand == 0, "a new offer was already used");
            require(offer.mTier >= lastTier, "the offers are not in tier order");
            lastTier = offer.mTier;
            require(offer.mNetId != 0 && netIds.insert(offer.mNetId).second, "two offers share a net id");
        }

        // asking again keeps the same offers
        const int32_t firstId = offers->mOffers.front().mNetId;
        require(armorer.getEconomy().getOffers(arena.mOwner, armorer)->mOffers.front().mNetId == firstId,
                "the offers were rolled twice");
    }

    void wanderingTraderRollsItsTable(Arena &arena) {
        const TradeTable *table =
                TradeTableRegistry::getInstance().get("trading/economy_trades/wandering_trader_trades.json");
        require(table != nullptr, "the wandering trader table was not loaded");
        MobActor *trader = dynamic_cast<MobActor *>(
                arena.mOwner.spawnActor(*arena.mLevel, "minecraft:wandering_trader", Vector3f(3.5f, 100.0f, 3.5f)));
        require(trader != nullptr, "the wandering trader did not spawn");
        trader->startDefinition(arena.mOwner);
        MerchantOffers *offers = trader->getEconomy().getOffers(arena.mOwner, *trader);
        require(offers != nullptr && (int32_t) offers->mOffers.size() == expectedOfferCount(*table),
                "the wandering trader did not get one offer per selected trade");
    }

    void professionWithoutTableOffersNothing(Arena &arena) {
        MobActor &butcher = arena.villager("butcher");
        const json::Value *component = butcher.getComponent(MobEconomy::COMPONENT);
        require(component != nullptr, "a butcher has no economy trade table");
        const json::Value *path = component->get("table");
        if (path != nullptr && TradeTableRegistry::getInstance().get(path->string()) != nullptr)
            return;
        const MerchantOffers *offers = butcher.getEconomy().getOffers(arena.mOwner, butcher);
        require(offers == nullptr || offers->mOffers.empty(), "a profession without its table got offers");
    }

    void librarianPricesItsBooks(Arena &arena) {
        bool sawBook = false;
        for (int32_t attempt = 0; attempt < 8 && !sawBook; ++attempt) {
            MobActor &librarian = arena.villager("librarian");
            MerchantOffers *offers = librarian.getEconomy().getOffers(arena.mOwner, librarian);
            require(offers != nullptr && !offers->mOffers.empty(), "the librarian rolled no offers");
            for (const MerchantOffer &offer: offers->mOffers) {
                if (offer.mSell.mDefinition->getIdentifier() != "minecraft:enchanted_book")
                    continue;
                sawBook = true;
                const std::vector<EnchantmentInstance> enchantments = ItemEnchantments::read(offer.mSell);
                require(enchantments.size() == 1, "a traded book has more than one enchantment");
                require(enchantments.front().mId != EnchantmentIds::SOUL_SPEED
                                && enchantments.front().mId != EnchantmentIds::SWIFT_SNEAK
                                && enchantments.front().mId != EnchantmentIds::WIND_BURST,
                        "a librarian sold an enchantment the game keeps out of trades");
                require(offer.mBuyCountA >= 2 && offer.mBuyCountA <= 64, "the book price left the stack range");
            }
            librarian.despawn();
        }
        require(sawBook, "no librarian sold an enchanted book");
    }

    void offersSurviveASave(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr, "the armorer rolled no offers");
        offers->mOffers.front().mUses = 3;
        offers->mOffers.front().mDemand = 2;

        const Tag saved = armorer.saveNbt();
        const Tag *savedOffers = saved.get("Offers");
        require(savedOffers != nullptr && savedOffers->get("Recipes") != nullptr, "the offers were not saved");
        require(savedOffers->get("Recipes")->getList().front().get("netId") == nullptr,
                "the save kept a net id");
        require(saved.getString("TradeTablePath") == ARMORER_TABLE, "the table path was not saved");
        require(saved.contains("TradeTier", Tag::Type::Int) && saved.contains("Riches", Tag::Type::Int),
                "the trader state was not saved");

        MobActor &copy = arena.villager("armorer");
        copy.loadNbt(saved);
        const MerchantOffers *loaded = copy.getEconomy().getOffers(arena.mOwner, copy);
        require(loaded != nullptr && loaded->mOffers.size() == offers->mOffers.size(), "the offers came back short");
        for (size_t index = 0; index < offers->mOffers.size(); ++index) {
            const MerchantOffer &before = offers->mOffers[index];
            const MerchantOffer &after = loaded->mOffers[index];
            require(after.mBuyA.mDefinition->getIdentifier() == before.mBuyA.mDefinition->getIdentifier()
                            && after.mBuyA.mCount == before.mBuyA.mCount
                            && after.mSell.mDefinition->getIdentifier() == before.mSell.mDefinition->getIdentifier()
                            && after.mUses == before.mUses && after.mMaxUses == before.mMaxUses
                            && after.mDemand == before.mDemand && after.mTraderExp == before.mTraderExp
                            && after.mPriceMultiplierA == before.mPriceMultiplierA && after.mTier == before.mTier,
                    "an offer changed through the save");
            require(after.mNetId != 0, "a loaded offer has no net id");
        }
        require(loaded->mTierExpRequirements == offers->mTierExpRequirements, "the tier requirements changed");
    }

    void missingFieldsTakeTheGameDefaults(Arena &arena) {
        Tag recipe = Tag::ofCompound();
        Tag buyA = Tag::ofCompound();
        buyA.putString("Name", "minecraft:emerald");
        buyA.putByte("Count", 5);
        recipe.put("buyA", buyA);
        recipe.put("sell", buyA);
        Tag list = Tag::ofList(Tag::Type::Compound);
        list.addToList(recipe);
        Tag data = Tag::ofCompound();
        data.put("Recipes", list);

        const MerchantOffers offers = MerchantOffers::load(data, arena.mOwner.getCodecContext());
        require(offers.mOffers.size() == 1, "the recipe was not read");
        const MerchantOffer &offer = offers.mOffers.front();
        require(offer.mMaxUses == 7 && offer.mRewardExp && offer.mBuyCountA == 5 && offer.mBuyB.isAir(),
                "a missing field did not take the game default");
    }

    void demandRaisesThePrice() {
        MerchantOffer offer;
        offer.mBuyCountA = 10;
        offer.mPriceMultiplierA = 0.05f;
        offer.mUses = 12;
        offer.mMaxUses = 16;
        offer.resupply();
        require(offer.mDemand == 8 && offer.mUses == 0, "resupply did not turn the uses into demand");

        offer.mUses = 2;
        offer.resupply();
        require(offer.mDemand == 0, "demand fell under zero");
    }

    void priceFormula(Arena &arena) {
        MerchantOffer offer;
        offer.mBuyA = arena.mOwner.createItemStack("minecraft:emerald", 10);
        offer.mBuyCountA = 10;
        offer.mPriceMultiplierA = 0.05f;
        offer.mDemand = 8;
        offer.updatePrice(0, 0, false, 0);
        require(offer.mBuyA.mCount == 14, "demand did not raise the price");

        offer.updatePrice(0, 0, true, 0);
        require(offer.mBuyA.mCount == 11, "the hero of the village paid full price");

        offer.mDemand = 0;
        offer.mPriceMultiplierA = 0.2f;
        offer.updatePrice(-25, -20, false, 0);
        require(offer.mBuyA.mCount == 1, "a cured villager priced under one emerald");
    }

    void tiersFollowExperience() {
        MerchantOffers offers;
        offers.mTierExpRequirements = {0, 10, 70, 150, 250};
        require(offers.tierForExperience(0) == 0 && offers.tierForExperience(9) == 0, "a novice moved up early");
        require(offers.tierForExperience(10) == 1 && offers.tierForExperience(249) == 3, "a tier was skipped");
        require(offers.tierForExperience(1000) == 4, "experience went past the master tier");
    }

    void resupplyEventRestocks(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr, "the armorer rolled no offers");
        MerchantOffer &offer = offers->mOffers.front();
        offer.mUses = offer.mMaxUses;
        EntityEvents::fire(arena.mOwner, armorer, "minecraft:resupply_trades");
        require(offer.mUses == 0, "the resupply event left the offer used");
        require(offer.mDemand == std::max(0, offer.mMaxUses), "the full uses did not become demand");
    }

    ItemStackRequestSlotData slotOf(ContainerSlotType container, int32_t slot) {
        ItemStackRequestSlotData data;
        data.mContainer = container;
        data.mContainerName.mContainer = container;
        data.mSlot = slot;
        return data;
    }

    ItemStackRequest tradeRequest(int32_t netId, int32_t crafts, ContainerSlotType from, int32_t consumed,
                                  int32_t taken) {
        ItemStackRequest request;
        request.mRequestId = -1;

        ItemStackRequestAction craft;
        craft.mType = ItemStackRequestActionType::CraftRecipe;
        craft.mRecipeNetworkId = netId;
        craft.mNumberOfRequestedCrafts = crafts;
        request.mActions.push_back(craft);

        ItemStackRequestAction consume;
        consume.mType = ItemStackRequestActionType::Consume;
        consume.mCount = consumed;
        consume.mSource = slotOf(from, 4);
        request.mActions.push_back(consume);

        ItemStackRequestAction take;
        take.mType = ItemStackRequestActionType::Take;
        take.mCount = taken;
        take.mSource = slotOf(ContainerSlotType::CreatedOutput, 50);
        take.mDestination = slotOf(ContainerSlotType::Inventory, 0);
        request.mActions.push_back(take);
        return request;
    }

    int runTrade(Arena &arena, PlayerInventory &inventory, const ItemStackRequest &request,
                 const MerchantOffers *offers, TradeCraft &trade) {
        const std::vector<CreativeItemData> creative;
        return ItemStackRequestHandler::execute(inventory, request, creative, arena.mOwner.getRecipeOutputs(),
                                                arena.mOwner.getRecipeSourceIndices(), false, false, nullptr, nullptr,
                                                &arena.mOwner.getCodecContext(), nullptr, 0, false, nullptr, false,
                                                nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, offers, &trade)
                .mResult;
    }

    void tradeRequestsAreChecked(Arena &arena) {
        MerchantOffers offers;
        MerchantOffer offer;
        offer.mBuyA = arena.mOwner.createItemStack("minecraft:emerald", 3);
        offer.mBuyCountA = 3;
        offer.mSell = arena.mOwner.createItemStack("minecraft:bread", 2);
        offer.mMaxUses = 2;
        offer.mNetId = MerchantOffers::nextNetId();
        offers.mOffers.push_back(offer);
        const int32_t netId = offer.mNetId;

        PlayerInventory inventory;
        inventory.setTradeItem(0, arena.mOwner.createItemStack("minecraft:emerald", 10));
        TradeCraft trade;
        const int ok = ItemStackRequestHandler::RESULT_OK;

        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 3, 2), nullptr,
                         trade) != ok,
                "a trade went through with no trader");
        require(runTrade(arena, inventory, tradeRequest(netId + 1, 1, ContainerSlotType::Trade2Ingredient1, 3, 2),
                         &offers, trade) != ok,
                "an unknown offer was traded");
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 2, 2), &offers,
                         trade) != ok,
                "a trade paid less than the price");
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 4, 2), &offers,
                         trade) != ok,
                "a trade paid more than the price");
        require(runTrade(arena, inventory, tradeRequest(netId, 0, ContainerSlotType::Trade2Ingredient1, 0, 0), &offers,
                         trade) != ok,
                "a trade of nothing was granted");
        require(runTrade(arena, inventory, tradeRequest(netId, 2, ContainerSlotType::Trade2Ingredient1, 6, 2), &offers,
                         trade) != ok,
                "the output was not fully taken");
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Inventory, 3, 2), &offers,
                         trade) != ok,
                "a trade paid from outside the wanted slots");
        require(inventory.getTradeItem(0).mCount == 10 && inventory.getItem(0).isAir(),
                "a refused trade changed the inventory");

        require(runTrade(arena, inventory, tradeRequest(netId, 2, ContainerSlotType::Trade2Ingredient1, 6, 4), &offers,
                         trade) == ok,
                "a valid double trade was refused");
        require(trade.mOfferIndex == 0 && trade.mCrafts == 2, "the trade did not report its offer");
        require(inventory.getTradeItem(0).mCount == 4 && inventory.getItem(0).mCount == 4,
                "the trade moved the wrong counts");

        offers.mOffers.front().mUses = 2;
        TradeCraft spent;
        require(runTrade(arena, inventory, tradeRequest(netId, 1, ContainerSlotType::Trade2Ingredient1, 3, 2), &offers,
                         spent) != ok,
                "an offer out of stock was traded");
    }

    void tradingTeachesTheTrader(Arena &arena) {
        MobActor &armorer = arena.villager("armorer");
        MerchantOffers *offers = armorer.getEconomy().getOffers(arena.mOwner, armorer);
        require(offers != nullptr && !offers->mOffers.empty(), "the armorer rolled no offers");
        MerchantOffer &offer = offers->mOffers.front();
        const int32_t needed = offers->mTierExpRequirements.size() > 1 ? offers->mTierExpRequirements[1] : 0;
        require(needed > 0 && offer.mTraderExp > 0, "the first tier needs no experience");

        const int32_t trades = (needed + offer.mTraderExp - 1) / offer.mTraderExp;
        armorer.getEconomy().onTrade(arena.mOwner, armorer, 0, trades);
        require(offer.mUses == trades, "the trade did not count its uses");
        require(armorer.getEconomy().getExperience() == trades * offer.mTraderExp, "the trader gained no experience");
        require(armorer.getEconomy().getTier() == 1, "enough experience did not raise the tier");

        ServerPlayer &player = arena.player(7);
        require(armorer.onInteract(arena.mOwner, player), "interacting did not open the trade");
        require(player.getInventoryManager().getTradeActorId() == armorer.getUniqueId(),
                "the trade screen is not the armorer's");
        UpdateTradePacket packet;
        require(armorer.getEconomy().startTrading(arena.mOwner, armorer, player, packet), "the trade did not start");
        require(packet.mTradeTier == 1 && packet.mUsingEconomyTrade && packet.mNewTradingUi,
                "the trade packet lost the tier or the screen");
        const Tag *recipes = packet.mOffers.get("Recipes");
        require(recipes != nullptr && recipes->getList().size() == offers->mOffers.size()
                        && recipes->getList().front().contains("netId", Tag::Type::Int),
                "the packet offers lack their net ids");
        ServerPlayer &other = arena.player(8);
        require(!armorer.getEconomy().canTradeWith(arena.mOwner, armorer, other), "a second player traded at once");
        require(!armorer.onInteract(arena.mOwner, other), "a second player opened the busy trader");

        // closing gives the wanted items back and frees the trader
        player.getInventory().setTradeItem(0, arena.mOwner.createItemStack("minecraft:emerald", 5));
        player.getInventoryManager().onClientRemoveWindow(player.getInventoryManager().getCurrentWindowId());
        require(!player.getInventoryManager().isTradeOpen(), "the trade screen stayed open");
        require(player.getInventory().getTradeItem(0).isAir(), "the wanted slot kept its items");
        require(armorer.getEconomy().canTradeWith(arena.mOwner, armorer, other), "closing did not free the trader");
    }

    void tippedArrowsKeepThePotionOffset() {
        const std::unique_ptr<json::Value> definition =
                json::parse(R"({"function": "set_potion", "id": "nightvision"})");
        const std::unique_ptr<LootFunction> function = LootFunction::create(*definition);
        require(function != nullptr, "set_potion was not created");
        std::mt19937 random(1);
        const LootContext context(random);
        LootDrop arrow;
        arrow.mIdentifier = "minecraft:arrow";
        function->run(arrow, context);
        LootDrop potion;
        potion.mIdentifier = "minecraft:potion";
        function->run(potion, context);
        require(arrow.mData == potion.mData + 1, "a tipped arrow took the potion id itself");
    }
}

int main() {
    const std::filesystem::path originalRoot = ServerPaths::getRoot();
    const std::filesystem::path directory =
            std::filesystem::temp_directory_path()
            / ("falcon-trade-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    int result = 0;
    try {
        std::filesystem::create_directories(directory);
        ServerPaths::setRoot(directory);
        EntityDefinitions::initialize();
        Arena arena(directory);
        armorerRollsEveryTier(arena);
        librarianPricesItsBooks(arena);
        wanderingTraderRollsItsTable(arena);
        professionWithoutTableOffersNothing(arena);
        offersSurviveASave(arena);
        missingFieldsTakeTheGameDefaults(arena);
        demandRaisesThePrice();
        priceFormula(arena);
        tiersFollowExperience();
        resupplyEventRestocks(arena);
        tippedArrowsKeepThePotionOffset();
        tradeRequestsAreChecked(arena);
        tradingTeachesTheTrader(arena);
        std::cout << "trade checks passed\n";
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        result = 1;
    }
    ServerPaths::setRoot(originalRoot);
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
    return result;
}
