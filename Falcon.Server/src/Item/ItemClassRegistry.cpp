#include "Item/ItemClassRegistry.h"

#include "Plugin/PluginRegistrationScope.h"

#include <algorithm>
#include <vector>

namespace {
    struct Entry {
        int mPriority;
        int mOrder;
        ItemClassRegistry::Matcher mMatcher;
        ItemClassRegistry::Factory mFactory;
        const void *mOwner;
        bool mActive;
    };

    std::vector<Entry> &entries() {
        static std::vector<Entry> registered;
        return registered;
    }

    bool gSorted = false;
    int gNextOrder = 0;

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

    const Entry *findEntry(const std::string &identifier) {
        sortEntries();

        for (const Entry &entry: entries()) {
            if (entry.mActive && entry.mMatcher(identifier))
                return &entry;
        }

        return nullptr;
    }
}

ItemClassRegistry::Registration::Registration(int priority, Matcher matcher, Factory factory) {
    entries().push_back({priority, gNextOrder++, matcher, factory, PluginRegistrationScope::getOwner(),
                         PluginRegistrationScope::isActive()});
    gSorted = false;
}

std::unique_ptr<Item> ItemClassRegistry::create(const Item &item) {
    const Entry *entry = findEntry(item.getIdentifier());
    if (entry != nullptr)
        return entry->mFactory(item);

    return std::make_unique<Item>(item);
}

const void *ItemClassRegistry::findOwner(const std::string &identifier) {
    const Entry *entry = findEntry(identifier);
    return entry == nullptr ? nullptr : entry->mOwner;
}

void ItemClassRegistry::activate(const void *owner) {
    for (Entry &entry: entries()) {
        if (entry.mOwner == owner)
            entry.mActive = true;
    }
}

void ItemClassRegistry::remove(const void *owner) {
    if (owner == nullptr)
        return;

    std::vector<Entry> &registered = entries();
    registered.erase(std::remove_if(registered.begin(), registered.end(), [owner](const Entry &entry) {
        return entry.mOwner == owner;
    }), registered.end());
}
