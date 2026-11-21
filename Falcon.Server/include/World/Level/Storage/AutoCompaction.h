#pragma once

class Level;

// a compaction keeps its database open until it returns: untrack() and requestStop() do not wait for it
class AutoCompaction {
public:
    static void start(int intervalSeconds);

    // no new compaction starts; one running finishes on its own thread
    static void requestStop();

    // requestStop(), then waits for the thread
    static void stop();

    static void track(Level &level);

    static void untrack(Level &level);
};
