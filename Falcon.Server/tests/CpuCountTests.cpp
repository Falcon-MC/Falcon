#include "Core/Threading/CpuCount.h"

#include <iostream>
#include <stdexcept>
#include <thread>

namespace {
    void require(bool condition, const char *message) {
        if (!condition)
            throw std::runtime_error(message);
    }
}

int main() {
    try {
        require(CpuCount::fromCpuMax("max 100000") == 0, "an unlimited cpu.max counted as a limit");
        require(CpuCount::fromCpuMax("100000 100000\n") == 1, "one core of quota was not one core");
        // a share of a core still runs one thread: rounded up
        require(CpuCount::fromCpuMax("150000 100000") == 2, "a core and a half was not rounded up");
        require(CpuCount::fromCpuMax("50000 100000") == 1, "half a core counted as none");
        require(CpuCount::fromCpuMax("") == 0 && CpuCount::fromCpuMax("x y") == 0, "a broken cpu.max was read");
        require(CpuCount::fromCfsQuota(-1, 100000) == 0, "an unlimited cfs quota counted as a limit");
        require(CpuCount::fromCfsQuota(400000, 100000) == 4, "four cores of cfs quota were misread");
        require(CpuCount::fromCfsQuota(400000, 0) == 0, "a zero cfs period was read");

        const unsigned usable = CpuCount::usable();
        const unsigned hardware = std::thread::hardware_concurrency();
        require(usable >= 1 && (hardware == 0 || usable <= hardware), "the usable cores exceed the host's");
        std::cout << "cpu count checks passed\n";
        return 0;
    } catch (const std::exception &error) {
        std::cout << error.what() << '\n';
        return 1;
    }
}
