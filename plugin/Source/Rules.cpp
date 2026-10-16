/* SeriousHL - Half-Life's movement rules as one pure step. See Rules.h. */
#include "StdH.h"
#include "Rules.h"

namespace rules {

const Settings HALF_LIFE = {
  TRUE, FALSE, TRUE, 100,
  1.9f / 64.0f, 320.0f, 10.0f, 10.0f, 4.0f, 2.0f, 100.0f, 800.0f, 45.0f, 2000.0f, 400.0f, 400.0f,
};

// Sam's levels' usual gravity (WorldBase.es "Normal": 30 m/s²): Half-Life's gravity goes with the level's against it
static const FLOAT SAM_GRAVITY = 30.0f;
// pm_shared.c, player.cpp
static const FLOAT AIR_WISHSPEED_CAP = 30.0f;          // PM_AirAccelerate
static const FLOAT BUNNYJUMP_MAX_SPEED_FACTOR = 1.7f;  // PM_PreventMegaBunnyJumping
static const FLOAT PLAYER_DUCKING_MULTIPLIER = 0.333f; // PM_Duck
static const FLOAT TIME_TO_DUCK = 0.4f;                // ...on the ground; in the air at once
static const FLOAT DUCK_LIFT = 18.0f;                  // VEC_DUCK_HULL_MIN - VEC_HULL_MIN: the hull's half height change
static const FLOAT JUMP_GRAVITY = 800.0f;              // PM_Jump: sqrt(2 * 800 * 45), whatever sv_gravity is
static const FLOAT ON_GROUND_MAX_RISE = 180.0f;        // PM_CategorizePosition: faster up is in the air
static const FLOAT WALKABLE_NORMAL = 0.7f;             // ...steeper is in the air (surfing)
static const FLOAT PLAYER_MAX_SAFE_FALL_SPEED = 580.0f;
static const FLOAT PLAYER_FATAL_FALL_SPEED = 1024.0f;
static const FLOAT DAMAGE_FOR_FALL_SPEED = 100.0f / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED);

static void CheckVelocity(const Settings &s, FLOAT3D &v) {
  for (INDEX i = 1; i <= 3; i++) v(i) = Clamp(v(i), -s.fMaxVelocity, s.fMaxVelocity);
};

static void Friction(const Settings &s, FLOAT3D &v, FLOAT fFriction, FLOAT tmFrame) {
  const FLOAT fSpeed = v.Length();
  if (fSpeed < 0.1f) return;
  const FLOAT fControl = fSpeed < s.fStopSpeed ? s.fStopSpeed : fSpeed;
  const FLOAT fNewSpeed = Max(fSpeed - fControl * fFriction * tmFrame, 0.0f);
  v *= fNewSpeed / fSpeed;
};

static void Accelerate(FLOAT3D &v, const FLOAT3D &vWishDir, FLOAT fWishSpeed, FLOAT fAccel, FLOAT fSurface, FLOAT tmFrame) {
  const FLOAT fAddSpeed = fWishSpeed - (v % vWishDir);
  if (fAddSpeed <= 0.0f) return;
  const FLOAT fAccelSpeed = Min(fAccel * tmFrame * fWishSpeed * fSurface, fAddSpeed);
  v += vWishDir * fAccelSpeed;
};

static void AirAccelerate(FLOAT3D &v, const FLOAT3D &vWishDir, FLOAT fWishSpeed, FLOAT fAccel, FLOAT tmFrame) {
  // the cap that makes air strafing: what's added along the wish is up to 30 u/s, but it's added at the full rate
  const FLOAT fAddSpeed = Min(fWishSpeed, AIR_WISHSPEED_CAP) - (v % vWishDir);
  if (fAddSpeed <= 0.0f) return;
  const FLOAT fAccelSpeed = Min(fAccel * fWishSpeed * tmFrame, fAddSpeed);
  v += vWishDir * fAccelSpeed;
};

static void PreventMegaBunnyJumping(FLOAT3D &v, FLOAT fMaxSpeed) {
  const FLOAT fMaxScaled = BUNNYJUMP_MAX_SPEED_FACTOR * fMaxSpeed;
  if (fMaxScaled <= 0.0f) return;
  const FLOAT fSpeed = v.Length();
  if (fSpeed <= fMaxScaled) return;
  v *= (fMaxScaled / fSpeed) * 0.65f;
};

// The keys relative to the view as it was at fAt of the tick (0 its start, 1 now), absolute: as Player.es turns its
// keys by the view (relative to the body), and the engine the body's to absolute.
static FLOAT3D Wish(const Input &in, FLOAT fForward, FLOAT fSide, FLOAT fAt) {
  FLOATmatrix3D mView;
  MakeRotationMatrixFast(mView, ANGLE3D(in.fView - in.fTurn * (1.0f - fAt), 0.0f, 0.0f));
  const FLOAT3D vRelative = FLOAT3D(fSide, 0.0f, -fForward) * mView;
  return vRelative * in.mBody;
};

void Step(const Settings &s, const Input &in, Memory &mem, Output &out) {
  const FLOAT fUnit = Max(s.fUnit, 0.001f);
  const FLOAT tmTick = in.tmTick;
  const INDEX ctFrames = Clamp((INDEX)floorf(s.iFps * tmTick + 0.5f), (INDEX)1, (INDEX)50);
  const FLOAT tmFrame = tmTick / ctFrames;
  const FLOAT3D vUp = in.vUp;
  FLOAT3D v = in.vVelocity / fUnit;
  const FLOAT fGravity = s.fGravity * in.fGravity / SAM_GRAVITY;
  const FLOAT fMaxSpeed = s.fMaxSpeed * in.fFast;
  memset(&out, 0, sizeof(out));

  // PM_CategorizePosition
  BOOL bGround = in.bGround && (in.vNormal % vUp) >= WALKABLE_NORMAL && (v % vUp) <= ON_GROUND_MAX_RISE;

  // player.cpp: landing too fast hurts
  if (in.fLanding > 0.0f) {
    out.fFall = in.fLanding / fUnit;
    if (s.bFallDamage && out.fFall > PLAYER_MAX_SAFE_FALL_SPEED) {
      out.fFallDamage = (out.fFall - PLAYER_MAX_SAFE_FALL_SPEED) * DAMAGE_FOR_FALL_SPEED;
    }
  }

  // PM_Duck: ducked after TIME_TO_DUCK on the ground, at once in the air - where the hull shrinks about its centre, so
  // the feet come up 18 units (Sam's crouching box keeps its feet down: the move lifts him). Let go on the ground before
  // the duck is done and the standing hull pops up 18 units: the ducktap. Sam's own box says when there's room.
  FLOAT fFeetUp = 0.0f;
  const BOOL bDuckKey = in.bDuck;
  if (bDuckKey && !mem.bOldDuck && !mem.bDucked && !mem.bInDuck) {
    mem.bInDuck = TRUE;
    mem.fDuckLeft = TIME_TO_DUCK;
  }
  if (mem.bInDuck && bDuckKey) {
    mem.fDuckLeft -= tmTick;
    if (in.bSamDucked && (!bGround || mem.fDuckLeft <= 0.0f)) {
      if (!bGround) fFeetUp += DUCK_LIFT;
      mem.bDucked = TRUE;
      mem.bInDuck = FALSE;
    }
  }
  if (!bDuckKey && mem.bInDuck && !in.bSamDucked) {
    if (bGround) {
      fFeetUp += DUCK_LIFT;
      out.bLifted = TRUE;
      out.bDucktap = TRUE;
      bGround = FALSE;
    }
    mem.bInDuck = FALSE;
  }
  if (mem.bDucked && !in.bSamDucked) {
    // Sam's box stood up (the key's off, and there's room): in the air the feet go back down
    if (!bGround) fFeetUp -= DUCK_LIFT;
    mem.bDucked = FALSE;
  }
  mem.bOldDuck = bDuckKey;

  // PM_Jump - before friction: a jump on the tick he lands keeps all of his speed (bunny hopping)
  if (in.bJump && bGround && (s.bAutoJump || !mem.bOldJump)) {
    if (s.bBhopCap) PreventMegaBunnyJumping(v, fMaxSpeed);
    v = v - vUp * (v % vUp) + vUp * Sqrt(2.0f * JUMP_GRAVITY * s.fJumpHeight);
    bGround = FALSE;
    out.bJumped = TRUE;
  }
  mem.bOldJump = in.bJump;

  // the keys, as cl_forwardspeed / cl_sidespeed would give them; ducked, a third
  FLOAT fForward = in.fForward * s.fForwardSpeed * in.fFast;
  FLOAT fSide = in.fSide * s.fSideSpeed * in.fFast;
  if (mem.bDucked) {
    fForward *= PLAYER_DUCKING_MULTIPLIER;
    fSide *= PLAYER_DUCKING_MULTIPLIER;
  }

  // the surface's friction (pmove->friction: Sam's ice is slippery), twice at an edge
  const FLOAT fSurface = bGround ? in.fFriction : 1.0f;
  FLOAT fFriction = s.fFriction * fSurface;
  if (bGround && in.bEdge) fFriction *= s.fEdgeFriction;

  FLOAT3D vMoved(0, 0, 0);
  for (INDEX iFrame = 0; iFrame < ctFrames; iFrame++) {
    // the view turns through the tick as the mouse turned it
    const FLOAT3D vWishVel = Wish(in, fForward, fSide, (FLOAT)(iFrame + 1) / ctFrames);
    FLOAT fWishSpeed = vWishVel.Length();
    const FLOAT3D vWishDir = fWishSpeed > 0.001f ? vWishVel / fWishSpeed : FLOAT3D(0, 0, 0);
    if (fWishSpeed > fMaxSpeed) fWishSpeed = fMaxSpeed;

    if (bGround) {
      // PM_WalkMove
      v -= vUp * (v % vUp);
      Friction(s, v, fFriction, tmFrame);
      Accelerate(v, vWishDir, fWishSpeed, s.fAccelerate, fSurface, tmFrame);
      v -= vUp * (v % vUp);
      if (v.Length() < 1.0f) v = FLOAT3D(0, 0, 0);
      CheckVelocity(s, v);
      vMoved += v * tmFrame;
    } else {
      // PM_AirMove, half the gravity before the move and half after
      v -= vUp * (fGravity * tmFrame * 0.5f);
      AirAccelerate(v, vWishDir, fWishSpeed, s.fAirAccelerate, tmFrame);
      CheckVelocity(s, v);
      vMoved += v * tmFrame;
      v -= vUp * (fGravity * tmFrame * 0.5f);
      CheckVelocity(s, v);
    }
  }

  FLOAT3D vMove = vMoved * (fUnit / tmTick);
  if (bGround) {
    // along the floor: up a slope as fast as on the flat (Half-Life steps up ramps), down it without flying off
    const FLOAT fNormalUp = in.vNormal % vUp;
    if (fNormalUp > 0.01f) vMove -= vUp * ((in.vNormal % vMove) / fNormalUp);
  }
  // the duck's lift goes with the move (the engine stops it at a ceiling or the floor)
  vMove += vUp * (fFeetUp * fUnit / tmTick);

  out.vMove = vMove;
  out.vEnd = v * fUnit;
  out.bGround = bGround;
  out.bHoldCrouch = !bGround && bDuckKey;
  out.fSpeed = (v - vUp * (v % vUp)).Length();
};

}; // namespace
