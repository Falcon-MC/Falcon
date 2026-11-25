#include "Protocol/Types/CommandParam.h"

#include <iostream>

namespace {
    bool expect(CommandParamType type, int32_t id, const char *name) {
        if ((int32_t) type == id)
            return true;
        std::cout << name << " is " << (int32_t) type << ", the client reads " << id << '\n';
        return false;
    }
}

int main() {
    // ids from the client's own parameter table: an old one shows the argument under the wrong type
    bool ok = expect(CommandParamType::IntegerRange, 23, "IntegerRange");
    ok = expect(CommandParamType::Message, 68, "Message") && ok;
    ok = expect(CommandParamType::Command, 90, "Command") && ok;
    if (ok)
        std::cout << "command param id checks passed\n";
    return ok ? 0 : 1;
}
