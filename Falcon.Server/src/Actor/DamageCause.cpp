#include "Actor/DamageCause.h"

#include <algorithm>

namespace {
    struct CauseEntry {
        const char *mName;
        const char *mDeathMessageKey;
    };

    const char *const ALL_CAUSES = "all";

    const CauseEntry CAUSES[] = {
            {"anvil", "death.attack.anvil"},
            {"drowning", "death.attack.drown"},
            {"entity_attack", "death.attack.mob"},
            {"entity_attack", "death.attack.player"},
            {"entity_explosion", "death.attack.explosion"},
            {"block_explosion", "death.attack.explosion"},
            {"fall", "death.fell.accident.generic"},
            {"fire", "death.attack.inFire"},
            {"fire_tick", "death.attack.onFire"},
            {"freezing", "death.attack.freeze"},
            {"lava", "death.attack.lava"},
            {"lightning", "death.attack.lightningBolt"},
            {"magic", "death.attack.magic"},
            {"magma", "death.attack.hotFloor"},
            {"projectile", "death.attack.arrow"},
            {"stalagmite", "death.attack.stalagmite"},
            {"starve", "death.attack.starve"},
            {"suffocation", "death.attack.inWall"},
            {"thorns", "death.attack.thorns"},
            {"void", "death.attack.outOfWorld"},
            {"override", "death.attack.generic"}
    };
}

const char *DamageCause::findDeathMessageKey(const std::string &cause) {
    for (const CauseEntry &entry: CAUSES) {
        if (cause == entry.mName)
            return entry.mDeathMessageKey;
    }

    return nullptr;
}

std::vector<std::string> DamageCause::getNames() {
    std::vector<std::string> names;
    for (const CauseEntry &entry: CAUSES) {
        if (std::find(names.begin(), names.end(), entry.mName) == names.end())
            names.push_back(entry.mName);
    }
    return names;
}

bool DamageCause::matches(const std::string &cause, const std::string &deathMessageKey) {
    if (cause.empty() || cause == ALL_CAUSES)
        return true;

    for (const CauseEntry &entry: CAUSES) {
        if (cause == entry.mName && deathMessageKey == entry.mDeathMessageKey)
            return true;
    }

    return false;
}
