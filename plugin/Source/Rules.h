/* SeriousHL - Half-Life's movement rules as one pure step: a tick's input to its output, nothing else read or written.
 *
 * PM_PlayerMove of the Half-Life SDK's pm_shared.c (PM_CategorizePosition, PM_Duck, PM_Jump, PM_PreventMegaBunnyJumping,
 * PM_Friction, PM_Accelerate, PM_AirAccelerate, PM_AddCorrectGravity / PM_FixupGravityVelocity, PM_CheckVelocity) and
 * player.cpp's fall damage, in Half-Life units, at Half-Life's frame rate inside each of Sam's ticks. The game side
 * (HlMove.cpp) fills the input from Sam's player and applies the output; a run journal records both, and the same
 * build checks a journal by running the step again (Journal.h). The structures are plain 4-byte fields: they go to
 * journals as they are.
 */
#ifndef SERIOUSHL_RULES_H
#define SERIOUSHL_RULES_H

#include "StdH.h"

namespace rules {

// The ruleset (Half-Life's cvars where there's one).
struct Settings {
  INDEX bAutoJump;       // holding jump hops on every landing (Bunnymod XT's bxt_autojump); 0: a jump per press
  INDEX bBhopCap;        // Steam Half-Life's cap: over 1.7x max speed a jump cuts the speed to 65% of that
  INDEX bFallDamage;     // Half-Life's fall damage (from 580 u/s, deadly at 1024)
  INDEX iFps;            // Half-Life's frame rate: steps a second
  FLOAT fUnit;           // metres in a Half-Life unit
  FLOAT fMaxSpeed;       // sv_maxspeed 320
  FLOAT fAccelerate;     // sv_accelerate 10
  FLOAT fAirAccelerate;  // sv_airaccelerate 10
  FLOAT fFriction;       // sv_friction 4
  FLOAT fEdgeFriction;   // edgefriction 2
  FLOAT fStopSpeed;      // sv_stopspeed 100
  FLOAT fGravity;        // sv_gravity 800 (in a level of Sam's usual gravity; scaled by the level's)
  FLOAT fJumpHeight;     // 45 units
  FLOAT fMaxVelocity;    // sv_maxvelocity 2000
  FLOAT fForwardSpeed;   // cl_forwardspeed 400
  FLOAT fSideSpeed;      // cl_sidespeed 400
};
// Half-Life's own values; 1 unit = Sam's eyes (1.9 m) over a Half-Life player's (64 units)
extern const Settings HALF_LIFE;

// One tick: what Sam's world says (core/sam/Movement.h State) and the game side adds.
struct Input {
  FLOAT tmTick;
  FLOAT3D vVelocity;     // m/s
  FLOAT3D vUp;
  FLOAT fGravity;        // the level's, m/s²
  INDEX bGround;
  FLOAT3D vNormal;
  FLOAT fFriction;       // the surface's
  FLOAT fLanding;        // m/s down when he landed since the last tick
  INDEX bSamDucked;      // Sam's crouching box is on
  FLOAT fForward, fSide; // keys, -1..1
  INDEX bJump, bDuck;
  FLOAT fTurn;           // degrees the view turned since the last tick
  FLOAT fView;           // the view's heading relative to the body (after the tick's turn)
  FLOATmatrix3D mBody;   // the body's rotation
  INDEX bEdge;           // no ground ahead (the edge friction's test, done by the game side)
  FLOAT fFast;           // speed factor (Serious Speed: 2)
};

// What the rules keep from tick to tick (pmove->oldbuttons, the duck's state).
struct Memory {
  INDEX bOldJump, bOldDuck;
  INDEX bDucked;         // FL_DUCKING
  INDEX bInDuck;         // ducking on the ground, not done yet
  FLOAT fDuckLeft;
};

struct Output {
  FLOAT3D vMove;         // m/s, the average over the tick (with the duck's lift)
  FLOAT3D vEnd;          // m/s at its end
  INDEX bJumped, bLifted, bHoldCrouch, bGround, bDucktap;
  FLOAT fFallDamage;     // to inflict now
  FLOAT fFall;           // u/s he landed with (0: didn't)
  FLOAT fSpeed;          // u/s, horizontal, at the end
};

void Step(const Settings &s, const Input &in, Memory &mem, Output &out);

}; // namespace

#endif
