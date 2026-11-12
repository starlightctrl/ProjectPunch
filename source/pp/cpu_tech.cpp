#include "pp/cpu_tech.h"

#include "pp/actions.h"
#include "pp/global_settings.h"
#include "pp/main.h"
#include "pp/playerdata.h"

#include <ft/fighter.h>
#include <ft/ft_manager.h>
#include <mt/mt_prng.h>
#include <so/posture/so_posture_module_simple.h>
#include <so/status/so_status_module_impl.h>

/*
 * IC-Basic values the game's action scripts read to decide on techs and getups.
 *
 * Landing in hitstun or tumble techs when the frames since shield was pressed are under the tech
 * window, and the lockout from pressing shield too often is over. The tech is a roll if the stick
 * is held to the side. Lying down, the stick to the side is a getup roll and up is standing up.
 * Either roll then goes the way stick X points, where positive is the way the fighter is facing.
 */
#define IC_STICK_X 1011
#define IC_STICK_SIDE 1013
#define IC_STICK_Y 1018
#define IC_FRAMES_SINCE_SHIELD 21017
#define IC_TECH_LOCKOUT 21018

/* Answers for the tech check that always tech, or never do. */
#define TECH_FRAMES_SINCE_SHIELD 0.0f
#define MISS_FRAMES_SINCE_SHIELD 32767.0f
#define TECH_LOCKOUT_OVER 32767.0f

namespace PP {

const char* cpuTechOptions[] = { "Off", "Random", "Random (No Miss)", "Away", "In", "Neutral", "Miss" };

// Don't reorder, the random picks depend on these being in order.
enum TechChoice { TECH_NEUTRAL, TECH_AWAY, TECH_IN, TECH_MISS };
enum GetupChoice { GETUP_STAND, GETUP_AWAY, GETUP_IN, GETUP_ATTACK };

static bool isFalling(int action) {
    return action == ACTION_HITSTUN || action == ACTION_DAMAGEFALL;
}

static bool isLyingDown(int action) {
    return action == ACTION_HITGROUNDTUMBLE || action == ACTION_DOWNWAIT;
}

/* 1 if the closest other fighter is to the right of x, -1 if to the left. */
static float directionToOpponent(const PlayerData& player, float x) {
    float closest = -1;
    float direction = 1;
    for (int i = 0; i < g_ftManager->getEntryCount(); i++) {
        int entryId = g_ftManager->getEntryIdFromIndex(i);
        if (entryId == player.entryId) { continue; }

        float dx = g_ftManager->getFighter(entryId, 0)->m_moduleAccesser->getPostureModule()->getPos().m_x - x;
        float dist = dx < 0 ? -dx : dx;
        if (closest < 0 || dist < closest) {
            closest = dist;
            direction = dx < 0 ? -1 : 1;
        }
    }
    return direction;
}

static int pickTech(int option) {
    switch (option) {
        case CPU_TECH_RANDOM: return randi(4); // neutral, away, in or miss
        case CPU_TECH_RANDOM_NO_MISS: return randi(3); // neutral, away or in
        case CPU_TECH_AWAY: return TECH_AWAY;
        case CPU_TECH_IN: return TECH_IN;
        case CPU_TECH_NEUTRAL: return TECH_NEUTRAL;
        default: return TECH_MISS;
    }
}

/* Picks what the CPU does on its next landing or getup. techValueHook then gives the game the answers for it. */
void processCpuTech(PlayerData& player) {
    CpuTechState& state = player.cpuTechState;
    state.modules = NULL;
    state.checkingTech = false;
    state.rollStick = 0;

    int option = player.settings().cpuTechOption;
    if (!player.isCpu || option == CPU_TECH_OFF) { return; }

    soModuleAccesser& modules = *g_ftManager->getFighter(player.entryId, 0)->m_moduleAccesser;
    u16 action = player.current->action;
    u16 prevAction = player.prev->action;

    if (isFalling(action)) {
        // Hitstun -> DamageFall is the same fall.
        if (!isFalling(prevAction)) {
            state.tech = pickTech(option);
        }
    } else if (isLyingDown(action)) {
        if (!isLyingDown(prevAction)) {
            state.getup = randi(4);
        }

        // The getup attack is a button press, which the game doesn't ask techValueHook about.
        if (action == ACTION_DOWNWAIT && state.getup == GETUP_ATTACK) {
            modules.getStatusModule()->changeStatus(ACTION_DOWNATTACK, &modules);
        }
    } else {
        return;
    }

    state.modules = &modules;
    state.lyingDown = isLyingDown(action);
    soPostureModule* posture = modules.getPostureModule();
    state.opponentDirection = directionToOpponent(player, posture->getPos().m_x) * posture->getLr();
}

float (*techValueOriginal)(soModuleAccesser* modules, u32 index, u32 arg);

/*
 * Replaces soValueAccesser::getValueFloat, which the game's action scripts read their values
 * through. For a CPU that's falling or lying down, answer the ones that decide on techs and getups
 * for the CPU Tech option instead, like UnclePunch's lab sets timer_LR and the stick.
 */
float techValueHook(soModuleAccesser* modules, u32 index, u32 arg) {
    float value = techValueOriginal(modules, index, arg);
    if (index != IC_STICK_X && index != IC_STICK_SIDE && index != IC_STICK_Y
        && index != IC_FRAMES_SINCE_SHIELD && index != IC_TECH_LOCKOUT) { return value; }
    if (allPlayerData == NULL || !battleSceneInitialized) { return value; }

    CpuTechState* state = NULL;
    for (int i = 0; i < g_ftManager->getEntryCount(); i++) {
        if (allPlayerData[i].cpuTechState.modules == modules) { state = &allPlayerData[i].cpuTechState; }
    }
    if (state == NULL) { return value; }

    bool rollIn;
    bool rollAway;
    switch (index) {
        case IC_FRAMES_SINCE_SHIELD:
            if (state->lyingDown) { return value; }
            state->checkingTech = state->tech != TECH_MISS;
            return state->checkingTech ? TECH_FRAMES_SINCE_SHIELD : MISS_FRAMES_SINCE_SHIELD;

        case IC_TECH_LOCKOUT:
            return state->lyingDown ? value : TECH_LOCKOUT_OVER;

        case IC_STICK_SIDE:
            if (state->lyingDown) {
                rollIn = state->getup == GETUP_IN;
                rollAway = state->getup == GETUP_AWAY;
            } else {
                // The stick is read for other things during a fall too, so only answer for the tech.
                if (!state->checkingTech) { return value; }
                state->checkingTech = false;
                rollIn = state->tech == TECH_IN;
                rollAway = state->tech == TECH_AWAY;
            }
            if (rollIn) { state->rollStick = state->opponentDirection; }
            if (rollAway) { state->rollStick = -state->opponentDirection; }
            return rollIn || rollAway ? 1.0f : 0.0f;

        case IC_STICK_X:
            // Asked right after a roll starts, for which way it goes.
            return state->rollStick != 0 ? state->rollStick : value;

        default:
            if (!state->lyingDown) { return value; }
            return state->getup == GETUP_STAND ? 1.0f : 0.0f;
    }
}

} // namespace PP
