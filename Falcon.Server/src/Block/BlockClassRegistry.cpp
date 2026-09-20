#include "Block/BlockClassRegistry.h"

#include <algorithm>
#include <vector>

namespace {
    struct Entry {
        int mPriority;
        int mOrder;
        BlockClassRegistry::Matcher mMatcher;
        BlockClassRegistry::Factory mFactory;
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

BlockClassRegistry::Registration::Registration(int priority, Matcher matcher, Factory factory) {
    entries().push_back({priority, (int) entries().size(), matcher, factory});
    gSorted = false;
}

std::unique_ptr<Block> BlockClassRegistry::create(const Block &block) {
    sortEntries();

    const std::string &identifier = block.getIdentifier();

    for (const Entry &entry: entries()) {
        if (entry.mMatcher(identifier))
            return entry.mFactory(block);
    }

    return std::make_unique<Block>(block);
}
