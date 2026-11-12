#pragma once

#include "pp/common.h"

#include <so/so_module_accesser.h>

#define PP_CPU_TECH_OPT_COUNT 7

namespace PP {

struct PlayerData;

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

/* Kept per player in PlayerData. */
class CpuTechState {
public:
    CpuTechState() {
        modules = NULL;
        techCheck = false;
        lyingDown = false;
        tech = 0;
        getup = 0;
        opponentDirection = 0;
        checkingTech = false;
        rollStick = 0;
    }

    // Set while the CPU is falling or lying down, for techValueHook to find it.
    soModuleAccesser* modules;
    bool techCheck;
    bool lyingDown;
    int tech;
    int getup;

    // 1 if the opponent is in front of the CPU, -1 if behind it.
    float opponentDirection;

    // Set by techValueHook between the game's checks.
    bool checkingTech;
    float rollStick;
};

void processCpuTech(PlayerData& player);

extern float (*techValueOriginal)(soModuleAccesser* modules, u32 index, u32 arg);
float techValueHook(soModuleAccesser* modules, u32 index, u32 arg);

} // namespace PP
