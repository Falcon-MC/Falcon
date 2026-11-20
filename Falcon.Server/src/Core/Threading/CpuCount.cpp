#include "Core/Threading/CpuCount.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <thread>

#ifdef __linux__
#include <sched.h>
#endif

namespace {
    unsigned roundedUp(int64_t quota, int64_t period) {
        if (quota <= 0 || period <= 0)
            return 0;
        return (unsigned) std::max<int64_t>(1, (quota + period - 1) / period);
    }

#ifdef __linux__
    bool readLine(const std::string &path, std::string &line) {
        std::ifstream file(path);
        return file && std::getline(file, line);
    }

    int64_t readNumber(const std::string &path) {
        std::string line;
        if (!readLine(path, line))
            return 0;
        try {
            return std::stoll(line);
        } catch (...) {
            return 0;
        }
    }

    // the tightest quota on the way from the process's cgroup up to the root: a parent limits its children
    unsigned quotaCores() {
        std::ifstream groups("/proc/self/cgroup");
        std::string entry;
        unsigned cores = 0;
        const auto keep = [&cores](unsigned limit) {
            if (limit != 0)
                cores = cores == 0 ? limit : std::min(cores, limit);
        };
        while (std::getline(groups, entry)) {
            // "0::/path" on cgroup v2, "4:cpu,cpuacct:/path" on v1
            const size_t first = entry.find(':');
            const size_t second = first == std::string::npos ? std::string::npos : entry.find(':', first + 1);
            if (second == std::string::npos)
                continue;
            const std::string controllers = entry.substr(first + 1, second - first - 1);
            std::string path = entry.substr(second + 1);
            if (controllers.empty()) {
                for (;;) {
                    std::string line;
                    if (readLine("/sys/fs/cgroup" + (path == "/" ? std::string() : path) + "/cpu.max", line))
                        keep(CpuCount::fromCpuMax(line));
                    if (path.empty() || path == "/")
                        break;
                    path = path.substr(0, path.find_last_of('/'));
                }
                continue;
            }
            if (("," + controllers + ",").find(",cpu,") == std::string::npos)
                continue;
            // a container sees its own group at the mount point, the host the path below it
            for (const std::string mount: {"/sys/fs/cgroup/cpu,cpuacct", "/sys/fs/cgroup/cpu"}) {
                for (const std::string &group: {path == "/" ? std::string() : path, std::string()}) {
                    const int64_t quota = readNumber(mount + group + "/cpu.cfs_quota_us");
                    const int64_t period = readNumber(mount + group + "/cpu.cfs_period_us");
                    keep(CpuCount::fromCfsQuota(quota, period));
                }
            }
        }
        return cores;
    }
#endif

    unsigned detect() {
        unsigned cores = std::thread::hardware_concurrency();
#ifdef __linux__
        cpu_set_t set;
        CPU_ZERO(&set);
        if (sched_getaffinity(0, sizeof(set), &set) == 0 && CPU_COUNT(&set) > 0)
            cores = cores == 0 ? (unsigned) CPU_COUNT(&set) : std::min(cores, (unsigned) CPU_COUNT(&set));
        const unsigned quota = quotaCores();
        if (quota != 0)
            cores = cores == 0 ? quota : std::min(cores, quota);
#endif
        return std::max(cores, 1u);
    }
}

unsigned CpuCount::usable() {
    static const unsigned cores = detect();
    return cores;
}

unsigned CpuCount::fromCpuMax(const std::string &line) {
    std::istringstream fields(line);
    std::string quota;
    int64_t period = 0;
    if (!(fields >> quota >> period) || quota == "max")
        return 0;
    try {
        return roundedUp(std::stoll(quota), period);
    } catch (...) {
        return 0;
    }
}

unsigned CpuCount::fromCfsQuota(int64_t quota, int64_t period) {
    return roundedUp(quota, period);
}
