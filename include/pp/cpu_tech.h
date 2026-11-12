#pragma once

#include "pp/common.h"

#define PP_CPU_TECH_OPT_COUNT 7

namespace PP {

enum CpuTechOption {
    CPU_TECH_OFF = 0,
    CPU_TECH_RANDOM = 1,
    CPU_TECH_RANDOM_NO_MISS = 2,
    CPU_TECH_AWAY = 3,
    CPU_TECH_IN = 4,
    CPU_TECH_NEUTRAL = 5,
    CPU_TECH_MISS = 6
};

extern const char* cpuTechOptions[];

} // namespace PP
