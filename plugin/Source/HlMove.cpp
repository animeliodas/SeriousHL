/* SeriousHL - Half-Life's player movement for Serious Sam's player. See HlMove.h.
 *
 * The rules follow the Half-Life SDK's pm_shared.c (PM_PlayerMove: PM_CategorizePosition, PM_Jump,
 * PM_PreventMegaBunnyJumping, PM_Friction, PM_Accelerate, PM_AirAccelerate, PM_AddCorrectGravity /
 * PM_FixupGravityVelocity, PM_CheckVelocity) and player.cpp's fall damage, in Half-Life units, at Half-Life's frame
 * rate inside each of Sam's 20 Hz ticks. Sam's engine still moves him (collision, sliding, steps) by the average
 * velocity of those frames; the core's movement takeover keeps Sam's own movement and gravity out. */
#include "StdH.h"
#include "HlMove.h"
#include "Movement.h"
#include "Player.h"
#include "SBridge.h"

#include <stdio.h>

namespace hl {

INDEX hl_bEnable = TRUE;
INDEX hl_bAutoJump = TRUE;
INDEX hl_bBhopCap = FALSE;
INDEX hl_bFallDamage = TRUE;
INDEX hl_iFps = 100;
FLOAT hl_fUnit = 1.9f / 64.0f;
FLOAT hl_fMaxSpeed = 320.0f;
FLOAT hl_fAccelerate = 10.0f;
FLOAT hl_fAirAccelerate = 10.0f;
FLOAT hl_fFriction = 4.0f;
FLOAT hl_fEdgeFriction = 2.0f;
FLOAT hl_fStopSpeed = 100.0f;
FLOAT hl_fGravity = 800.0f;
FLOAT hl_fJumpHeight = 45.0f;
FLOAT hl_fMaxVelocity = 2000.0f;
FLOAT hl_fForwardSpeed = 400.0f;
FLOAT hl_fSideSpeed = 400.0f;

// Sam's levels' usual gravity (WorldBase.es "Normal": 30 m/s²): Half-Life's gravity goes with the level's against it
static const FLOAT SAM_GRAVITY = 30.0f;
// pm_shared.c, player.cpp
static const FLOAT AIR_WISHSPEED_CAP = 30.0f;          // PM_AirAccelerate
static const FLOAT BUNNYJUMP_MAX_SPEED_FACTOR = 1.7f;  // PM_PreventMegaBunnyJumping
static const FLOAT PLAYER_DUCKING_MULTIPLIER = 0.333f; // PM_Duck
static const FLOAT JUMP_GRAVITY = 800.0f;              // PM_Jump: sqrt(2 * 800 * 45), whatever sv_gravity is
static const FLOAT ON_GROUND_MAX_RISE = 180.0f;        // PM_CategorizePosition: faster up is in the air
static const FLOAT WALKABLE_NORMAL = 0.7f;             // ...steeper is in the air (surfing)
static const FLOAT PLAYER_MAX_SAFE_FALL_SPEED = 580.0f;
static const FLOAT PLAYER_FATAL_FALL_SPEED = 1024.0f;
static const FLOAT DAMAGE_FOR_FALL_SPEED = 100.0f / (PLAYER_FATAL_FALL_SPEED - PLAYER_MAX_SAFE_FALL_SPEED);
static const FLOAT EDGE_AHEAD = 16.0f, EDGE_DOWN = 34.0f; // PM_Friction's trace

static BOOL _bOldJump = FALSE; // pmove->oldbuttons & IN_JUMP
static INDEX _ctTrace = 0;
static FILE *_fTrace = NULL;
static ULONG _ctTicks = 0, _ctJumps = 0, _ctEdges = 0;
static FLOAT _fSpeed = 0.0f, _fTopSpeed = 0.0f, _fLastFall = 0.0f;
static BOOL _bGround = FALSE;

// --- pm_shared.c

static void CheckVelocity(FLOAT3D &v) {
  for (INDEX i = 1; i <= 3; i++) v(i) = Clamp(v(i), -hl_fMaxVelocity, hl_fMaxVelocity);
};

static void Friction(FLOAT3D &v, FLOAT fFriction, FLOAT tmFrame) {
  const FLOAT fSpeed = v.Length();
  if (fSpeed < 0.1f) return;
  const FLOAT fControl = fSpeed < hl_fStopSpeed ? hl_fStopSpeed : fSpeed;
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

// PM_Friction's edge test: Half-Life traces the player's box 16 units ahead, 34 down - no ground under it, twice the
// friction. Here rays from the feet: under them, 16 and 32 units ahead, and 16 to each side half way.
static BOOL AtEdge(CEntity *pen, const FLOAT3D &v, const FLOAT3D &vUp) {
  FLOAT3D vAhead = v - vUp * (v % vUp);
  const FLOAT fLength = vAhead.Length();
  if (fLength < 0.1f) return FALSE;
  vAhead /= fLength;
  const FLOAT3D vSide = vAhead * vUp;
  const FLOAT3D vFeet = pen->GetPlacement().pl_PositionVector;
  static const FLOAT afAhead[5] = { 0.0f, 0.5f, 1.0f, 0.5f, 0.5f };
  static const FLOAT afSide[5] = { 0.0f, 0.0f, 0.0f, -0.5f, 0.5f };
  const FLOAT fReach = EDGE_AHEAD * 2.0f * hl_fUnit;
  for (INDEX i = 0; i < 5; i++) {
    const FLOAT3D vFrom = vFeet + vAhead * (afAhead[i] * fReach) + vSide * (afSide[i] * fReach) + vUp * 0.1f;
    const FLOAT3D vTo = vFrom - vUp * (EDGE_DOWN * hl_fUnit + 0.1f);
    CCastRay cr(pen, vFrom, vTo);
    cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
    cr.cr_bPhysical = TRUE;
    pen->GetWorld()->CastRay(cr);
    if (cr.cr_penHit != NULL) return FALSE;
  }
  return TRUE;
};

static void Trace(CEntity *pen, const FLOAT3D &vMove, const FLOAT3D &vEnd, BOOL bGround, BOOL bJumped) {
  if (_ctTrace <= 0) return;
  if (_fTrace == NULL) {
    char strTemp[MAX_PATH];
    GetTempPathA(MAX_PATH, strTemp);
    CTString strDir = CTString(strTemp) + SB_TEMP_DIR;
    CreateDirectoryA(strDir.str_String, NULL);
    CTString strFile = strDir + "\\trace.csv";
    _fTrace = fopen(strFile.str_String, "w");
    if (_fTrace == NULL) {
      _ctTrace = 0;
      return;
    }
    fprintf(_fTrace, "tick,x,y,z,vx,vy,vz,ground,jumped,hspeed_u\n");
  }
  const FLOAT3D vPos = pen->GetPlacement().pl_PositionVector;
  const FLOAT3D vUp = -((CMovableEntity *)pen)->en_vGravityDir;
  const FLOAT fH = (vEnd - vUp * (vEnd % vUp)).Length() / hl_fUnit;
  fprintf(_fTrace, "%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%.2f\n", _pTimer->CurrentTick(), vPos(1), vPos(2), vPos(3),
    vEnd(1), vEnd(2), vEnd(3), bGround ? 1 : 0, bJumped ? 1 : 0, fH);
  if (--_ctTrace <= 0) {
    fclose(_fTrace);
    _fTrace = NULL;
    CPrintF(SB_TAG " trace written\n");
  }
};

void TraceTicks(INDEX ctTicks) {
  if (_fTrace != NULL) {
    fclose(_fTrace);
    _fTrace = NULL;
  }
  _ctTrace = ctTicks;
};

// The rules (movement::Rules): every tick, right before the engine moves him.
static BOOL Rules(CEntity *pen, const movement::State &st, movement::Result &res) {
  if (!hl_bEnable) {
    // (traced too: tests compare with Sam's own movement)
    const CMovableEntity *pmen = (CMovableEntity *)pen;
    Trace(pen, pmen->en_vCurrentTranslationAbsolute, pmen->en_vCurrentTranslationAbsolute, pmen->en_penReference.ep_pen != NULL, FALSE);
    return FALSE;
  }

  const FLOAT fUnit = Max(hl_fUnit, 0.001f);
  const FLOAT tmTick = _pTimer->TickQuantum;
  const INDEX ctFrames = Clamp((INDEX)floorf(hl_iFps * tmTick + 0.5f), (INDEX)1, (INDEX)50);
  const FLOAT tmFrame = tmTick / ctFrames;
  const FLOAT3D vUp = st.vUp;
  FLOAT3D v = st.vVelocity / fUnit;
  const FLOAT fGravity = hl_fGravity * st.fGravity / SAM_GRAVITY;
  // Serious Speed (TSE): twice as fast, as Sam's own with it
  const FLOAT fFast = player::PowerupLeft(pen, player::PU_SERIOUSSPEED) > 0.0f ? 2.0f : 1.0f;
  const FLOAT fMaxSpeed = hl_fMaxSpeed * fFast;

  // PM_CategorizePosition
  BOOL bGround = st.bGround && (st.vNormal % vUp) >= WALKABLE_NORMAL && (v % vUp) <= ON_GROUND_MAX_RISE;

  // player.cpp: landing too fast hurts
  if (st.fLanding > 0.0f) {
    _fLastFall = st.fLanding / fUnit;
    if (hl_bFallDamage && _fLastFall > PLAYER_MAX_SAFE_FALL_SPEED) {
      const FLOAT fDamage = (_fLastFall - PLAYER_MAX_SAFE_FALL_SPEED) * DAMAGE_FOR_FALL_SPEED;
      pen->InflictDirectDamage(pen, pen, DMT_IMPACT, fDamage, pen->GetPlacement().pl_PositionVector, -vUp);
    }
  }

  // PM_Jump - before friction: a jump on the tick he lands keeps all of his speed (bunny hopping)
  BOOL bJumped = FALSE;
  if (st.keys.bJump && bGround && (hl_bAutoJump || !_bOldJump)) {
    if (hl_bBhopCap) PreventMegaBunnyJumping(v, fMaxSpeed);
    v = v - vUp * (v % vUp) + vUp * Sqrt(2.0f * JUMP_GRAVITY * hl_fJumpHeight);
    bGround = FALSE;
    bJumped = TRUE;
    _ctJumps++;
  }
  _bOldJump = st.keys.bJump;

  // the keys, as cl_forwardspeed / cl_sidespeed would give them; ducked, a third
  FLOAT fForward = st.keys.fForward * hl_fForwardSpeed * fFast;
  FLOAT fSide = st.keys.fSide * hl_fSideSpeed * fFast;
  if (st.bDucked) {
    fForward *= PLAYER_DUCKING_MULTIPLIER;
    fSide *= PLAYER_DUCKING_MULTIPLIER;
  }

  // the surface's friction (pmove->friction: Sam's ice is slippery), twice at an edge
  const FLOAT fSurface = bGround ? st.fFriction : 1.0f;
  FLOAT fFriction = hl_fFriction * fSurface;
  if (bGround && hl_fEdgeFriction != 1.0f && AtEdge(pen, v, vUp)) {
    fFriction *= hl_fEdgeFriction;
    _ctEdges++;
  }

  FLOAT3D vMoved(0, 0, 0);
  for (INDEX iFrame = 0; iFrame < ctFrames; iFrame++) {
    // the view turns through the tick as the mouse turned it
    const FLOAT3D vWishVel = movement::Wish(pen, st, fForward, fSide, (FLOAT)(iFrame + 1) / ctFrames);
    FLOAT fWishSpeed = vWishVel.Length();
    const FLOAT3D vWishDir = fWishSpeed > 0.001f ? vWishVel / fWishSpeed : FLOAT3D(0, 0, 0);
    if (fWishSpeed > fMaxSpeed) fWishSpeed = fMaxSpeed;

    if (bGround) {
      // PM_WalkMove
      v -= vUp * (v % vUp);
      Friction(v, fFriction, tmFrame);
      Accelerate(v, vWishDir, fWishSpeed, hl_fAccelerate, fSurface, tmFrame);
      v -= vUp * (v % vUp);
      if (v.Length() < 1.0f) v = FLOAT3D(0, 0, 0);
      CheckVelocity(v);
      vMoved += v * tmFrame;
    } else {
      // PM_AirMove, half the gravity before the move and half after
      v -= vUp * (fGravity * tmFrame * 0.5f);
      AirAccelerate(v, vWishDir, fWishSpeed, hl_fAirAccelerate, tmFrame);
      CheckVelocity(v);
      vMoved += v * tmFrame;
      v -= vUp * (fGravity * tmFrame * 0.5f);
      CheckVelocity(v);
    }
  }

  FLOAT3D vMove = vMoved * (fUnit / tmTick);
  const FLOAT3D vEnd = v * fUnit;
  if (bGround) {
    // along the floor: up a slope as fast as on the flat (Half-Life steps up ramps), down it without flying off
    const FLOAT fNormalUp = st.vNormal % vUp;
    if (fNormalUp > 0.01f) vMove -= vUp * ((st.vNormal % vMove) / fNormalUp);
  }
  res.vMove = vMove;
  res.vEnd = vEnd;
  res.bJumped = bJumped;

  _ctTicks++;
  _bGround = bGround;
  _fSpeed = (v - vUp * (v % vUp)).Length();
  _fTopSpeed = Max(_fTopSpeed, _fSpeed);
  Trace(pen, vMove, vEnd, bGround, bJumped);
  return TRUE;
};

BOOL Init(void) {
  return movement::Init(&Rules);
};

void Release(void) {
  movement::Release();
  _bOldJump = FALSE;
};

CTString Status(void) {
  CTString str;
  str.PrintF("%s; %s; speed %.0f u/s (top %.0f), %s, %u jumps, %u ticks at an edge, last fall %.0f u/s; autojump %d, bhop cap %d, %d fps, 1 u = %.4f m",
    hl_bEnable ? "Half-Life movement" : "Sam's movement (hl_bEnable 0)", movement::Status().str_String, _fSpeed, _fTopSpeed,
    _bGround ? "on the ground" : "in the air", _ctJumps, _ctEdges, _fLastFall, hl_bAutoJump, hl_bBhopCap, hl_iFps, hl_fUnit);
  return str;
};

}; // namespace
