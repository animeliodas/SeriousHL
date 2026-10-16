/* SeriousHL - Half-Life's movement for Serious Sam's player: the game side of the rules. See HlMove.h.
 *
 * Every tick the core's takeover calls Rules() right before the engine moves the player: the rules' input is filled
 * from Sam's player (what the world did, the keys, the view, the ground and whether there's ground ahead), the step
 * runs (Rules.cpp), and its output goes back: the velocity, a jump or a lift off the ground, fall damage. */
#include "StdH.h"
#include "HlMove.h"
#include "Movement.h"
#include "Player.h"
#include "SBridge.h"

#include <Extras/XGizmo/Objects/StructPtr.h>
#include <stdio.h>

namespace hl {

rules::Settings _settings = rules::HALF_LIFE;
INDEX hl_bEnable = TRUE;

static const FLOAT EDGE_AHEAD = 16.0f, EDGE_DOWN = 34.0f; // PM_Friction's trace

static rules::Memory _mem;
static INDEX _ctTrace = 0;
static FILE *_fTrace = NULL;
static ULONG _ctTicks = 0, _ctJumps = 0, _ctDucktaps = 0, _ctEdges = 0;
static FLOAT _fSpeed = 0.0f, _fTopSpeed = 0.0f, _fLastFall = 0.0f;
static BOOL _bGround = FALSE;
static ULONG _ctPushes = 0;
static FLOAT _fLastPush = 0.0f;
static INDEX _iWater = 0;
static ULONG _ctWaterJumps = 0;
static CSymbolPtr _symViewHeightStand;

// Models/Player/SeriousSam/Player.h
static const INDEX PLAYER_BOX_STAND = 0;

// Sam swims in a box around his middle (1.5 m) with his camera 0.4 m over it, and switches to it and back as the water
// rises past half of the box he's in - the middle jumping by most of a metre each time. Half-Life's player keeps his
// hull and his eyes in water: so does Sam while the rules move him (if the standing box fits there), his camera at the
// standing height; Sam's swimming state, its animation and breathing stay.
static void KeepStanding(CEntity *pen, movement::State &st) {
  const INDEX iState = player::PropIndex(pen, player::PROP_STATE, player::PST_STAND);
  if (iState != player::PST_SWIM && iState != player::PST_DIVE) return;
  CMovableModelEntity *pmme = (CMovableModelEntity *)pen;
  if (pmme->en_iCollisionBox != PLAYER_BOX_STAND && !pmme->ChangeCollisionBoxIndexNow(PLAYER_BOX_STAND)) return;
  pmme->en_iWantedCollisionBox = PLAYER_BOX_STAND;
  if (!_symViewHeightStand.Exists()) _symViewHeightStand.Find("plr_fViewHeightStand");
  ((CPlayerEntity *)pen)->en_plViewpoint.pl_PositionVector(2) = _symViewHeightStand.Exists() ? _symViewHeightStand.GetFloat() : 1.9f;
  const CCollisionInfo *pci = pen->en_pciCollisionInfo;
  st.fWater = movement::WaterAbove(pen, 3.0f);
  if (pci != NULL) st.fHeight = pci->ci_fMaxHeight - pci->ci_fMinHeight;
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
  const FLOAT fReach = EDGE_AHEAD * 2.0f * _settings.fUnit;
  for (INDEX i = 0; i < 5; i++) {
    const FLOAT3D vFrom = vFeet + vAhead * (afAhead[i] * fReach) + vSide * (afSide[i] * fReach) + vUp * 0.1f;
    const FLOAT3D vTo = vFrom - vUp * (EDGE_DOWN * _settings.fUnit + 0.1f);
    CCastRay cr(pen, vFrom, vTo);
    cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
    cr.cr_bPhysical = TRUE;
    pen->GetWorld()->CastRay(cr);
    if (cr.cr_penHit != NULL) return FALSE;
  }
  return TRUE;
};

// PM_CheckWaterJump's traces: a point 8 units over the hull's middle and one at its top (36 over it), 24 units ahead
// along the view (flat) - a near vertical wall in front of the first, nothing in front of the second. The hull's
// middle: Sam's collision box's.
static BOOL WallAhead(CEntity *pen, const movement::State &st, FLOAT3D &vNormal) {
  const FLOAT fUnit = _settings.fUnit;
  const FLOAT3D vAhead = movement::Wish(pen, st, 1.0f, 0.0f, 1.0f);
  const CCollisionInfo *pci = pen->en_pciCollisionInfo;
  const FLOAT fMiddle = pci != NULL ? (pci->ci_fMinHeight + pci->ci_fMaxHeight) * 0.5f : 0.9f;
  const FLOAT3D vMiddle = pen->GetPlacement().pl_PositionVector + st.vUp * fMiddle;
  const FLOAT afUp[2] = { rules::WATERJUMP_AT, rules::WATERJUMP_TOP };
  for (INDEX i = 0; i < 2; i++) {
    const FLOAT3D vFrom = vMiddle + st.vUp * (afUp[i] * fUnit);
    CCastRay cr(pen, vFrom, vFrom + vAhead * (rules::WATERJUMP_AHEAD * fUnit));
    cr.cr_ttHitModels = CCastRay::TT_NONE;
    cr.cr_bHitPortals = FALSE;
    cr.cr_bHitTranslucentPortals = FALSE;
    cr.cr_bPhysical = TRUE;
    pen->GetWorld()->CastRay(cr);
    if (i == 0) {
      if (cr.cr_penHit == NULL || cr.cr_pbpoBrushPolygon == NULL) return FALSE;
      vNormal = FLOAT3D(cr.cr_pbpoBrushPolygon->bpo_pbplPlane->bpl_plAbsolute);
      if (Abs(vNormal % st.vUp) >= 0.1f) return FALSE;
    } else if (cr.cr_penHit != NULL) {
      return FALSE;
    }
  }
  return TRUE;
};

static void Trace(CEntity *pen, const FLOAT3D &vEnd, BOOL bGround, BOOL bJumped) {
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
    fprintf(_fTrace, "tick,x,y,z,vx,vy,vz,ground,jumped,hspeed_u,ducked,water\n");
  }
  const FLOAT3D vPos = pen->GetPlacement().pl_PositionVector;
  const FLOAT3D vUp = -((CMovableEntity *)pen)->en_vGravityDir;
  const FLOAT fH = (vEnd - vUp * (vEnd % vUp)).Length() / _settings.fUnit;
  fprintf(_fTrace, "%.2f,%.4f,%.4f,%.4f,%.4f,%.4f,%.4f,%d,%d,%.2f,%d,%d\n", _pTimer->CurrentTick(), vPos(1), vPos(2), vPos(3),
    vEnd(1), vEnd(2), vEnd(3), bGround ? 1 : 0, bJumped ? 1 : 0, fH, _mem.bDucked ? 1 : 0, _iWater);
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
static BOOL Rules(CEntity *pen, const movement::State &stWorld, movement::Result &res) {
  movement::State st = stWorld;
  if (!hl_bEnable) {
    // (traced too: tests compare with Sam's own movement)
    const CMovableEntity *pmen = (CMovableEntity *)pen;
    Trace(pen, pmen->en_vCurrentTranslationAbsolute, pmen->en_penReference.ep_pen != NULL, FALSE);
    return FALSE;
  }

  KeepStanding(pen, st);
  rules::Input in;
  in.tmTick = _pTimer->TickQuantum;
  in.vVelocity = st.vVelocity;
  in.vUp = st.vUp;
  in.fGravity = st.fGravity;
  in.bGround = st.bGround;
  in.vNormal = st.vNormal;
  in.fFriction = st.fFriction;
  in.fLanding = st.fLanding;
  in.bSamDucked = st.bDucked;
  in.fForward = st.keys.fForward;
  in.fSide = st.keys.fSide;
  in.bJump = st.keys.bJump;
  in.bDuck = st.keys.bDuck;
  in.fTurn = st.fTurn;
  in.fView = st.fView;
  in.mBody = pen->GetRotationMatrix();
  in.bEdge = st.bGround && _settings.fEdgeFriction != 1.0f && AtEdge(pen, st.vVelocity, st.vUp);
  // Serious Speed (TSE): twice as fast, as Sam's own with it
  in.fFast = player::PowerupLeft(pen, player::PU_SERIOUSSPEED) > 0.0f ? 2.0f : 1.0f;
  in.fPitch = st.fPitch;
  in.fWater = st.fWater;
  in.fHeight = st.fHeight;
  in.bWallAhead = FALSE;
  in.vWallNormal = FLOAT3D(0, 0, 0);
  if (st.fWater > 0.0f && st.fWater < movement::WATER_DEEP) in.bWallAhead = WallAhead(pen, st, in.vWallNormal);

  rules::Output out;
  rules::Step(_settings, in, _mem, out);

  if (out.fFallDamage > 0.0f) {
    pen->InflictDirectDamage(pen, pen, DMT_IMPACT, out.fFallDamage, pen->GetPlacement().pl_PositionVector, -st.vUp);
  }
  res.vMove = out.vMove;
  res.vEnd = out.vEnd;
  res.bJumped = out.bJumped;
  res.bLifted = out.bLifted;
  res.bHoldCrouch = out.bHoldCrouch;

  _ctTicks++;
  if (out.bJumped) _ctJumps++;
  if (out.bDucktap) _ctDucktaps++;
  if (in.bEdge && out.bGround) _ctEdges++;
  if (out.fFall > 0.0f) _fLastFall = out.fFall;
  if (out.bWaterJump) {
    _ctWaterJumps++;
    CPrintF(SB_TAG " water jump: the wall's normal %.2f %.2f %.2f, pushed at %.0f u/s\n", in.vWallNormal(1), in.vWallNormal(2),
      in.vWallNormal(3), _mem.vWaterJump.Length());
  }
  _iWater = out.iWaterLevel;
  _bGround = out.bGround;
  _fSpeed = out.fSpeed;
  _fTopSpeed = Max(_fTopSpeed, _fSpeed);
  Trace(pen, out.vEnd, out.bGround, out.bJumped);
  return TRUE;
};

// --- damage pushes (Half-Life's CBaseMonster::TakeDamage: velocity += away from the inflictor * DamageForce, which is
// damage * 5 u/s for a standing player's hull, twice for the ducked one's - half the size -, at most 1000): rocket and
// grenade jumps go Half-Life's way. Sam's own push (CPlayer::DamageImpact) is taken back out while the rules move him.
typedef void (CEntity::*CDamageImpact)(enum DamageType, FLOAT, const FLOAT3D &, const FLOAT3D &);
static CDamageImpact _pDamageImpact = NULL;

static FLOAT3D HalfLifePush(DamageType dmt, FLOAT fDamage, const FLOAT3D &vDirection, const FLOAT3D &vUp) {
  // (falls, crushing, drowning, abysses, spikes: Half-Life has no inflictor to push from)
  if (dmt == DMT_IMPACT || dmt == DMT_BRUSH || dmt == DMT_DROWNING || dmt == DMT_ABYSS || dmt == DMT_SPIKESTAB) {
    return FLOAT3D(0, 0, 0);
  }
  FLOAT3D vAway = vDirection;
  const FLOAT fLength = vAway.Length();
  vAway = fLength > 0.01f ? vAway / fLength : vUp;
  const FLOAT fForce = Min(fDamage * 5.0f * (_mem.bDucked ? 2.0f : 1.0f), 1000.0f);
  return vAway * (fForce * _settings.fUnit);
};

class CPlayerHook : public CEntity {
public:
  void P_DamageImpact(enum DamageType dmt, FLOAT fDamage, const FLOAT3D &vHit, const FLOAT3D &vDirection) {
    CMovableEntity *pmen = (CMovableEntity *)this;
    const FLOAT3D vBefore = pmen->en_vCurrentTranslationAbsolute;
    (this->*_pDamageImpact)(dmt, fDamage, vHit, vDirection);
    if (!hl_bEnable || this != movement::Tracked() || !movement::Active()) return;
    const FLOAT3D vPush = HalfLifePush(dmt, fDamage, vDirection, -pmen->en_vGravityDir);
    pmen->en_vCurrentTranslationAbsolute = vBefore + vPush;
    if (vPush.Length() > 0.0f) {
      _ctPushes++;
      _fLastPush = vPush.Length() / _settings.fUnit;
      CPrintF(SB_TAG " damage push: %.1f damage -> %.0f u/s\n", fDamage, _fLastPush);
    }
  };
};

BOOL Init(void) {
  memset(&_mem, 0, sizeof(_mem));
  StructPtr ptr(ClassicsCore_GetEntitiesSymbol("?DamageImpact@CPlayer@@QAEXW4DamageType@@MABV?$Vector@M$02@@1@Z"));
  if (ptr.iAddress != NULL) {
    _pDamageImpact = ptr(CDamageImpact());
    CreatePatch(_pDamageImpact, &CPlayerHook::P_DamageImpact, "SeriousHL: CPlayer::DamageImpact()");
  } else {
    CPrintF(SB_TAG " (CPlayer::DamageImpact isn't in the Entities library: damage pushes stay Sam's)\n");
  }
  // (in water too: Half-Life's swimming)
  movement::TakeWater(TRUE);
  return movement::Init(&Rules);
};

void Release(void) {
  movement::Release();
  memset(&_mem, 0, sizeof(_mem));
};

CTString Status(void) {
  CTString str;
  str.PrintF("%s; %s; speed %.0f u/s (top %.0f), %s, water level %d, %u jumps, %u water jumps, %u ducktaps, ducked %d, %u ticks at an edge, last fall %.0f u/s, %u damage pushes (last %.0f u/s); autojump %d, bhop cap %d, %d fps, 1 u = %.4f m",
    hl_bEnable ? "Half-Life movement" : "Sam's movement (hl_bEnable 0)", movement::Status().str_String, _fSpeed, _fTopSpeed,
    _bGround ? "on the ground" : "in the air", _iWater, _ctJumps, _ctWaterJumps, _ctDucktaps, _mem.bDucked, _ctEdges, _fLastFall,
    _ctPushes, _fLastPush,
    _settings.bAutoJump, _settings.bBhopCap, _settings.iFps, _settings.fUnit);
  return str;
};

}; // namespace
