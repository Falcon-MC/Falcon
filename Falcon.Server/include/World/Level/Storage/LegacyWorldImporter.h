#pragma once

#include <string>

// worlds saved before 0.9: a 256 by 256 block area in chunks.dat, blocks as numeric ids and meta values.
// The first time such a world is opened its terrain is written to the level database, as the game does
// when it upgrades one; chunks.dat is left as it was
class LegacyWorldImporter {
public:
    // true when the world was one and is now in the database
    static bool importIfNeeded(const std::string &worldsDirectory, const std::string &levelName);

    // a world that still has to be imported: terrain in chunks.dat and no database yet
    static bool isLegacyWorld(const std::string &worldsDirectory, const std::string &levelName);
};
