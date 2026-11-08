#pragma once

class Level;

// untrack a world before its storage closes: untrack waits for a running compaction on it
class AutoCompaction {
public:
    static void start(int intervalSeconds);

    static void stop();

    static void track(Level &level);

    static void untrack(Level &level);
};
