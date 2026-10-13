/* SeriousHL - Half-Life's player movement for Serious Sam's player: PM_PlayerMove of the Half-Life SDK's pm_shared.c
 * (ground friction and acceleration, air acceleration with its 30 u/s cap - air strafing -, the jump before friction -
 * bunny hopping -, the optional bunny hop cap of Steam's Half-Life, edge friction, gravity, fall damage) on top of the
 * SeriousBridge core's movement takeover (core/sam/Movement.h), which keeps Sam's own movement out of it.
 *
 * Half-Life works in units (an inch); here 1 unit = hl_fUnit metres, by default so that Sam's eyes (1.9 m) are where
 * a Half-Life player's are (64 units): 320 u/s run = 9.5 m/s, the 45-unit jump = 1.34 m. */
#ifndef SERIOUSHL_HLMOVE_H
#define SERIOUSHL_HLMOVE_H

#include "StdH.h"

namespace hl {

// console variables (Main.cpp declares them; names as Half-Life's cvars where there's one)
extern INDEX hl_bEnable;        // 1: Half-Life movement; 0: Sam's own
extern INDEX hl_bAutoJump;      // 1: holding jump hops on every landing (Bunnymod XT's bxt_autojump); 0: a jump per press
extern INDEX hl_bBhopCap;       // 1: Steam Half-Life's cap (over 1.7x max speed a jump cuts the speed to 65% of that); 0: none
extern INDEX hl_bFallDamage;    // 1: Half-Life's fall damage (from 580 u/s, deadly at 1024) instead of Sam's impacts
extern INDEX hl_iFps;           // Half-Life's frame rate: its movement runs in that many steps a second (100)
extern FLOAT hl_fUnit;          // metres in a Half-Life unit
extern FLOAT hl_fMaxSpeed;      // sv_maxspeed 320
extern FLOAT hl_fAccelerate;    // sv_accelerate 10
extern FLOAT hl_fAirAccelerate; // sv_airaccelerate 10
extern FLOAT hl_fFriction;      // sv_friction 4
extern FLOAT hl_fEdgeFriction;  // edgefriction 2
extern FLOAT hl_fStopSpeed;     // sv_stopspeed 100
extern FLOAT hl_fGravity;       // sv_gravity 800 (in a level of Sam's usual gravity; scaled by the level's)
extern FLOAT hl_fJumpHeight;    // 45 units
extern FLOAT hl_fMaxVelocity;   // sv_maxvelocity 2000
extern FLOAT hl_fForwardSpeed;  // cl_forwardspeed 400
extern FLOAT hl_fSideSpeed;     // cl_sidespeed 400

// Plugin startup: the local player moves by Half-Life's rules from now on (the core runs them every tick, right before
// the engine moves him). FALSE if the engine couldn't be hooked.
BOOL Init(void);
// Back to Sam's own movement.
void Release(void);
CTString Status(void);

// Tests: the next ticks into %TEMP%\SeriousHL\trace.csv (one line a tick).
void TraceTicks(INDEX ctTicks);

}; // namespace

#endif
