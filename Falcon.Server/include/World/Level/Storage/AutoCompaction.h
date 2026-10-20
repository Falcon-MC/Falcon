#pragma once

class Level;

/**
 * Background thread that periodically compacts the database of every tracked world. A world must be
 * untracked before its storage closes: untrack waits for a compaction that is running on it.
 */
class AutoCompaction {
public:
    static void start(int intervalSeconds);

    static void stop();

    static void track(Level &level);

    static void untrack(Level &level);
};
