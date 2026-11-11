#include "Network/Handler/BookHandler.h"

#include "Network/ServerNetworkHandler.h"
#include "Protocol/Packets/BookEditPacket.h"

#include <algorithm>
#include <utility>
#include <vector>

namespace {
    // the page limit of every edit (sub_1447CE350, sub_1447CE930)
    const int32_t MAX_PAGES = 50;

    Tag page(const std::string &text, const std::string &photo) {
        Tag entry = Tag::ofCompound();
        entry.putString("text", text);
        entry.putString("photoname", photo);
        return entry;
    }

    std::vector<Tag> pagesOf(const ItemStack &book) {
        const Tag *pages = book.mTag.isCompound() ? book.mTag.get("pages") : nullptr;
        if (pages == nullptr || !pages->isList())
            return {};
        return pages->getList();
    }

    // an index past the end pads with empty pages first, as the game does
    void padTo(std::vector<Tag> &pages, int32_t count) {
        while ((int32_t) pages.size() < count)
            pages.push_back(page("", ""));
    }
}

void BookHandler::handleEdit(ServerNetworkHandler &owner, ServerPlayer &player, const BookEditPacket &packet) {
    const int32_t slot = packet.mInventorySlot;
    if (slot < 0 || slot >= PlayerInventory::CONTAINER_SIZE)
        return;
    const ItemStack &held = player.getInventory().getItem(slot);
    if (held.isAir() || held.mDefinition == nullptr || held.mDefinition->getIdentifier() != "minecraft:writable_book")
        return;

    std::vector<Tag> pages = pagesOf(held);
    const int32_t first = packet.mPageNumber;
    const int32_t second = packet.mSecondaryPageNumber;
    switch (packet.mAction) {
        case BookEditPacket::Action::ReplacePage:
            if (first < 0 || first >= MAX_PAGES)
                return;
            padTo(pages, first + 1);
            pages[(size_t) first] = page(packet.mText, packet.mPhotoName);
            break;
        case BookEditPacket::Action::AddPage:
            if (first < 0 || first >= MAX_PAGES || (int32_t) pages.size() >= MAX_PAGES)
                return;
            padTo(pages, first);
            pages.insert(pages.begin() + first, page(packet.mText, packet.mPhotoName));
            break;
        case BookEditPacket::Action::DeletePage:
            if (first < 0 || first >= (int32_t) pages.size())
                return;
            pages.erase(pages.begin() + first);
            break;
        case BookEditPacket::Action::SwapPages:
            if (first < 0 || second < 0 || first >= MAX_PAGES || second >= MAX_PAGES)
                return;
            padTo(pages, std::max(first, second) + 1);
            std::swap(pages[(size_t) first], pages[(size_t) second]);
            break;
        case BookEditPacket::Action::SignBook:
            break;
        default:
            return;
    }

    ItemStack edited = held;
    if (packet.mAction == BookEditPacket::Action::SignBook) {
        edited = owner.createItemStack("minecraft:written_book", held.mCount);
        if (edited.isAir())
            return;
        // sub_1447CD9D0: the signed fields next to the pages
        edited.mTag = held.mTag.isCompound() ? held.mTag : Tag::ofCompound();
        edited.mTag.putString("author", packet.mAuthor);
        edited.mTag.putString("xuid", player.getXuid());
        edited.mTag.putString("title", packet.mTitle);
        edited.mTag.putInt("generation", 0);
    } else if (!edited.mTag.isCompound()) {
        edited.mTag = Tag::ofCompound();
    }
    edited.mTag.put("pages", Tag::ofList(Tag::Type::Compound, std::move(pages)));

    player.getInventory().setItem(slot, std::move(edited));
    player.getInventoryManager().syncSlot(InventoryManager::InventoryId::Inventory, slot);
}
