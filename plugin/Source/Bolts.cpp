/* SeriousHL - the crossbow's bolts in flight. See Bolts.h. */
#include "StdH.h"
#include "Bolts.h"
#include "HlMove.h"
#include "Player.h"
#include "Sounds.h"
#include "SBridge.h"

#include <stdlib.h>

namespace bolts {

static const FLOAT SPEED = 2000.0f;     // BOLT_AIR_VELOCITY
static const FLOAT DAMAGE = 50.0f;      // sk_plr_xbow_bolt_monster
static const FLOAT STUCK = 10.0f;       // in a wall this long
static const FLOAT FLIGHT = 8.0f;       // (none meets anything this long: gone)

struct Bolt {
  FLOAT3D vPos, vPrev, vDir, vLeft, vUp;
  TIME tmFired, tmGone;
  BOOL bStuck;
};
static std::vector<Bolt> _aBolts;
static ULONG _ctFired = 0, _ctHitLiving = 0, _ctHitWall = 0, _ctCasts = 0;
static FLOAT _fLastCastMs = 0.0f, _fLastDamage = 0.0f;
static CTString _strLastHit = "-";
static CSoundObject *_apso[4];
static INDEX _iSound = 0;

static FLOAT Unit(void) { return hl::_settings.fUnit; };

// a hit's sound from the player, as loud as it'd be from where it is (Half-Life's ATTN_NORM: gone at ~1000 units)
static void Sound(CEntity *penPlayer, const char *strList, const FLOAT3D &vAt) {
  CSoundData *psd = sounds::Pick(strList, (ULONG)rand());
  if (psd == NULL || penPlayer == NULL) return;
  _iSound = (_iSound + 1) % 4;
  if (_apso[_iSound] == NULL) _apso[_iSound] = new CSoundObject;
  CSoundObject *pso = _apso[_iSound];
  const FLOAT fDistance = (vAt - penPlayer->GetPlacement().pl_PositionVector).Length();
  const FLOAT fVolume = Clamp(1.0f - fDistance / (1000.0f * Unit()), 0.0f, 1.0f);
  if (fVolume <= 0.0f) return;
  pso->SetOwner(penPlayer);
  pso->Set3DParameters(50.0f, 10.0f, fVolume, 1.0f);
  pso->Play(psd, SOF_3D | SOF_VOLUMETRIC);
};

void Fire(CEntity *penPlayer, const FLOAT3D &vFrom, const FLOAT3D &vDir) {
  if (penPlayer == NULL) return;
  Bolt b;
  b.vPos = b.vPrev = vFrom;
  b.vDir = vDir;
  const FLOAT3D vWorldUp = -((CMovableEntity *)penPlayer)->en_vGravityDir;
  b.vUp = vWorldUp - vDir * (vWorldUp % vDir);
  if (b.vUp.Length() < 0.001f) b.vUp = FLOAT3D(1, 0, 0);
  b.vUp /= b.vUp.Length();
  b.vLeft = b.vUp * b.vDir;
  b.tmFired = _pTimer->CurrentTick();
  b.tmGone = b.tmFired + FLIGHT;
  b.bStuck = FALSE;
  _aBolts.push_back(b);
  _ctFired++;
};

// what stops it (Half-Life's traces go through triggers, items, the dead): brushes, the living
static BOOL Blocks(CEntity *pen) {
  if (pen->IsPredictor()) return FALSE;
  if (pen->en_RenderType == CEntity::RT_BRUSH) return TRUE;
  return (pen->GetFlags() & ENF_ALIVE) != 0;
};

void Tick(CEntity *penPlayer) {
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL) return;
  const TIME tmNow = _pTimer->CurrentTick();
  for (size_t i = 0; i < _aBolts.size();) {
    Bolt &b = _aBolts[i];
    if (tmNow >= b.tmGone) {
      _aBolts.erase(_aBolts.begin() + i);
      continue;
    }
    i++;
    b.vPrev = b.vPos;
    if (b.bStuck) continue;
    const FLOAT3D vNext = b.vPos + b.vDir * (SPEED * Unit() * _pTimer->TickQuantum);
    const CTimerValue tvStart = _pTimer->GetHighPrecisionTimer();
    CCastRay cr(NULL, b.vPos, vNext);
    cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
    cr.cr_penIgnore = penPlayer;
    cr.Cast(pwo);
    for (INDEX n = 0; n < 8 && cr.cr_penHit != NULL && !Blocks(cr.cr_penHit); n++) cr.ContinueCast(pwo);
    _ctCasts++;
    _fLastCastMs = (FLOAT)((_pTimer->GetHighPrecisionTimer() - tvStart).GetSeconds() * 1000.0);
    CEntity *penHit = cr.cr_penHit;
    if (penHit == NULL || penHit == penPlayer) {
      b.vPos = vNext;
      continue;
    }
    b.vPos = cr.cr_vHit;
    if (penHit->GetFlags() & ENF_ALIVE) {
      // BoltTouch: the damage, the body's sound, the bolt gone
      if (penPlayer != NULL) penPlayer->InflictDirectDamage(penHit, penPlayer, DMT_BULLET, DAMAGE, cr.cr_vHit, b.vDir);
      Sound(penPlayer, "weapons\\xbow_hitbod1.wav|weapons\\xbow_hitbod2.wav", cr.cr_vHit);
      _ctHitLiving++;
      _fLastDamage = DAMAGE;
      _strLastHit = penHit->GetClass()->ec_pdecDLLClass->dec_strName;
      b.tmGone = tmNow;
    } else {
      // a wall: it stays in it a while (a little in)
      Sound(penPlayer, "weapons\\xbow_hit1.wav", cr.cr_vHit);
      b.vPos = cr.cr_vHit - b.vDir * (2.0f * Unit());
      b.bStuck = TRUE;
      b.tmGone = tmNow + STUCK;
      _ctHitWall++;
      _strLastHit = "a wall";
    }
    CPrintF(SB_TAG " bolt hit %s at %.2f %.2f %.2f, %.2f s after it was fired\n", _strLastHit.str_String,
      cr.cr_vHit(1), cr.cr_vHit(2), cr.cr_vHit(3), tmNow - b.tmFired);
  }
};

void Collect(std::vector<viewmodel::Placed> &aPlaced) {
  if (_aBolts.empty()) return;
  // (between the last two ticks, as Sam's own things are drawn)
  const FLOAT fLerp = Clamp(1.0f + (FLOAT)(_pTimer->GetLerpedCurrentTick() - _pTimer->CurrentTick()) / _pTimer->TickQuantum, 0.0f, 1.0f);
  for (size_t i = 0; i < _aBolts.size(); i++) {
    const Bolt &b = _aBolts[i];
    if (b.tmGone <= _pTimer->CurrentTick()) continue;
    viewmodel::Placed pl;
    pl.strModel = "crossbow_bolt.mdl";
    pl.iSeq = 0;
    pl.fFrame = 0.0f;
    pl.iBody = 0;
    pl.vPos = b.vPrev + (b.vPos - b.vPrev) * fLerp;
    pl.vAhead = b.vDir;
    pl.vLeft = b.vLeft;
    pl.vUp = b.vUp;
    pl.fUnit = Unit();
    pl.fLight = 0.9f;
    aPlaced.push_back(pl);
  }
};

void Release(void) {
  _aBolts.clear();
  for (INDEX i = 0; i < 4; i++) {
    if (_apso[i] == NULL) continue;
    _apso[i]->Stop();
    _apso[i]->SetOwner(NULL);
  }
};

CTString Status(void) {
  CTString str;
  str.PrintF("bolts: %d out, %u fired, %u hit the living (last %s, %.0f damage), %u walls; %u casts (last %.3f ms)",
    (INDEX)_aBolts.size(), _ctFired, _ctHitLiving, _strLastHit.str_String, _fLastDamage, _ctHitWall, _ctCasts, _fLastCastMs);
  return str;
};

}; // namespace
