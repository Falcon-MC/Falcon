#pragma once

#include <cstdint>
#include <string>

namespace CpuCount {
    // the cores this process may run on, at least 1: the affinity mask and a container's cpu quota count,
    // unlike std::thread::hardware_concurrency, which counts every core of the host
    unsigned usable();

    // the cores a cgroup v2 cpu.max line allows ("150000 100000"), rounded up; 0 for no limit ("max 100000")
    unsigned fromCpuMax(const std::string &line);

    // the same from a cgroup v1 cfs_quota_us and cfs_period_us; 0 for no limit (a quota of -1)
    unsigned fromCfsQuota(int64_t quota, int64_t period);
}
