/* SeriousHL - Half-Life's player sounds (pm_shared.c PM_UpdateStepSound, PM_PlayStepSound, PM_CheckFalling, PM_Jump;
 * player.cpp's pain): steps by the surface under him (Sam's sand, grass and snow: dirt; shallow water: slosh, wading
 * deeper; else concrete) at Half-Life's pace (walking 400 ms, running 300, ducked 100 more and at a third of the
 * volume), a step at full volume on a jump, the swim sound once a second jumping in water, a landing from 350 u/s (from
 * 580 with pl_fallpain3), pain. Sam's own (his feet, body and mouth) are quiet meanwhile. The usual build only: the
 * rules' every tick (HlMove's tick hook) and Half-Life's sounds from the player's own Half-Life (Sounds.h). */
#ifndef SERIOUSHL_PLAYERSOUNDS_H
#define SERIOUSHL_PLAYERSOUNDS_H

#include "StdH.h"
#include "Rules.h"

namespace playersounds {

extern INDEX hl_bPlayerSounds; // 1: Half-Life's player sounds; 0: Sam's

void Init(void);
// Every tick the rules ran (HlMove's tick hook).
void Tick(CEntity *penPlayer, const rules::Input &in, const rules::Output &out, const rules::Memory &mem);
// A level, the game over: the sounds stopped and let go of the player.
void Release(void);
CTString Status(void);

}; // namespace

#endif
