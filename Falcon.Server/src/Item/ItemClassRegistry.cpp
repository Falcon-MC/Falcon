#include "Item/ItemClassRegistry.h"

#include <algorithm>
#include <vector>

namespace {
    struct Entry {
        int mPriority;
        int mOrder;
        ItemClassRegistry::Matcher mMatcher;
        ItemClassRegistry::Factory mFactory;
    };

    std::vector<Entry> &entries() {
        static std::vector<Entry> registered;
        return registered;
    }

    bool gSorted = false;

    void sortEntries() {
        if (gSorted)
            return;

        std::sort(entries().begin(), entries().end(), [](const Entry &left, const Entry &right) {
            if (left.mPriority != right.mPriority)
                return left.mPriority < right.mPriority;
            return left.mOrder < right.mOrder;
        });

        gSorted = true;
    }
}

ItemClassRegistry::Registration::Registration(int priority, Matcher matcher, Factory factory) {
    entries().push_back({priority, (int) entries().size(), matcher, factory});
    gSorted = false;
}

std::unique_ptr<Item> ItemClassRegistry::create(const Item &item) {
    sortEntries();

    const std::string &identifier = item.getIdentifier();

    for (const Entry &entry: entries()) {
        if (entry.mMatcher(identifier))
            return entry.mFactory(item);
    }

    return std::make_unique<Item>(item);
}
