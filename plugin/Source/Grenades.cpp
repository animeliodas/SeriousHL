/* SeriousHL - Half-Life's thrown things on Sam's grenades. See Grenades.h. */
#include "StdH.h"
#include "Grenades.h"
#include "HlMove.h"
#include "Physics.h"
#include "Player.h"
#include "Sounds.h"
#include "SBridge.h"

#include <stdlib.h>

namespace grenades {

BlastHook _pBlastHook = NULL;

// Projectile.es (class 501)
static const ULONG PJ_DAMAGE = (501UL << 8) | 14, PJ_RANGEDAMAGE = (501UL << 8) | 15;
static const ULONG PJ_HOTSPOT = (501UL << 8) | 16, PJ_FALLOFF = (501UL << 8) | 17, PJ_SOUND = (501UL << 8) | 30;
// BasicEffects.es (class 601): its sound
static const ULONG BE_SOUND = (601UL << 8) | 30;
// Sam's debris (Flags.h ECF_DEBRIS): only brushes stop it, nothing sets it off
static const ULONG ECF_THROWN = ((1UL << 0) << ECB_TEST) | ((1UL << 5) << ECB_IS);

struct Thrown {
  ULONG ulID;
  Kind kind;
  FLOAT3D vLast;
  BOOL bBounced;
  FLOAT fRadius; // how far its damage goes, metres
};
static const INDEX MAX_THROWN = 32;
static Thrown _aThrown[MAX_THROWN];
static INDEX _ctThrown = 0;
// sound objects taken in turn (a sound goes on after its grenade is gone: its owner held, so Sam's mixer - its own
// thread - never asks a freed entity where it is)
static CSoundObject *_apsoBounce[8], *_apsoBoom[8];
static CEntityPointer _apenBounce[8], _apenBoom[8];
static INDEX _iBounce = 0, _iBoom = 0;

static CSoundObject *Next(CSoundObject **apso, CEntityPointer *apen, INDEX &i, CEntity *penOwner) {
  i = (i + 1) % 8;
  if (apso[i] == NULL) apso[i] = new CSoundObject;
  apso[i]->Stop();
  apso[i]->SetOwner(NULL);
  apen[i] = penOwner;
  return apso[i];
};
static ULONG _ctBounces = 0, _ctExplosions = 0, _ctMade = 0;

static FLOAT Unit(void) { return hl::_settings.fUnit; };

static CEntity *Find(ULONG ulID) {
  CEntity *pen = physics::FindEntity(ulID);
  return pen != NULL && strcmp(pen->GetClass()->ec_pdecDLLClass->dec_strName, "Projectile") == 0 ? pen : NULL;
};

static void Play(CSoundObject *pso, CEntity *penOwner, const char *strList, FLOAT fVolume, FLOAT fRange) {
  CSoundData *psd = sounds::Pick(strList, (ULONG)rand());
  if (pso == NULL || psd == NULL) return;
  pso->SetOwner(penOwner);
  pso->Set3DParameters(fRange, fRange / 10.0f, fVolume, 1.0f);
  pso->Play(psd, SOF_3D | SOF_VOLUMETRIC);
};

void Track(CEntity *pen, Kind kind, const FLOAT3D &vVelocity, FLOAT tmFuse) {
  if (pen == NULL) return;
  CMovableEntity *pmen = (CMovableEntity *)pen;
  pmen->en_vCurrentTranslationAbsolute = vVelocity;
  FLOAT *pf;
  // RadiusDamage(dmg, dmg x 2.5): from the middle down to nothing at the edge; no blow of its own on what it hits
  const FLOAT fDamage = (kind == K_SATCHEL || kind == K_TRIPMINE) ? 150.0f : 100.0f;
  if ((pf = (FLOAT *)player::Prop(pen, PJ_DAMAGE)) != NULL) *pf = 0.0f;
  if ((pf = (FLOAT *)player::Prop(pen, PJ_RANGEDAMAGE)) != NULL) *pf = fDamage;
  if ((pf = (FLOAT *)player::Prop(pen, PJ_HOTSPOT)) != NULL) *pf = 0.1f;
  if ((pf = (FLOAT *)player::Prop(pen, PJ_FALLOFF)) != NULL) *pf = 2.5f * fDamage * Unit();
  if (kind != K_CONTACT) {
    // bounces off, slides and stops instead of going off when stuck, passes monsters (as Half-Life's: no blow on them)
    pmen->en_vDesiredTranslationRelative = FLOAT3D(0, 0, 0);
    pen->SetCollisionFlags(ECF_THROWN);
    // (the fuse is the timer of the state it waits in: none for a satchel)
    ((CRationalEntity *)pen)->SetTimerAfter(tmFuse >= 0.0f ? Max(tmFuse, 0.05f) : 3600.0f);
  }
  _ctMade++;
  CPrintF(SB_TAG " thrown (%s) at %.2f %.2f %.2f (%.2f s), %.1f m/s, fuse %.2f s\n", kind == K_HAND ? "hand grenade" : kind == K_SATCHEL ? "satchel" : kind == K_TRIPMINE ? "tripmine" : "contact",
    pen->GetPlacement().pl_PositionVector(1), pen->GetPlacement().pl_PositionVector(2), pen->GetPlacement().pl_PositionVector(3),
    _pTimer->CurrentTick(), vVelocity.Length(), tmFuse);
  if (_ctThrown >= MAX_THROWN) return;
  Thrown &t = _aThrown[_ctThrown++];
  t.ulID = pen->en_ulID;
  t.kind = kind;
  t.vLast = pen->GetPlacement().pl_PositionVector;
  t.bBounced = FALSE;
  t.fRadius = 2.5f * fDamage * Unit();
};

INDEX Detonate(CEntity *penPlayer) {
  INDEX ct = 0;
  const FLOAT fReach = 4096.0f * Unit();
  for (INDEX i = 0; i < _ctThrown; i++) {
    if (_aThrown[i].kind != K_SATCHEL) continue;
    CEntity *pen = Find(_aThrown[i].ulID);
    if (pen == NULL) continue;
    if (penPlayer != NULL && (pen->GetPlacement().pl_PositionVector - penPlayer->GetPlacement().pl_PositionVector).Length() > fReach) continue;
    ((CRationalEntity *)pen)->SetTimerAfter(0.05f);
    ct++;
  }
  return ct;
};

INDEX Satchels(void) {
  INDEX ct = 0;
  for (INDEX i = 0; i < _ctThrown; i++) {
    if (_aThrown[i].kind == K_SATCHEL && Find(_aThrown[i].ulID) != NULL) ct++;
  }
  return ct;
};

// Sam's explosion effect where it went off: its sound quiet, Half-Life's from it
static void Exploded(Thrown &t) {
  CWorld *pwo = IWorld::GetWorld();
  CEntity *penEffect = NULL;
  if (pwo != NULL) {
    FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
      if (iten->en_ulID <= t.ulID || (iten->GetFlags() & ENF_DELETED)) continue;
      if (iten->GetClass()->ec_pdecDLLClass->dec_iID != 601) continue;
      if ((iten->GetPlacement().pl_PositionVector - t.vLast).Length() > 4.0f) continue;
      CSoundObject *pso = (CSoundObject *)player::Prop(&*iten, BE_SOUND);
      if (pso != NULL && pso->IsPlaying()) pso->Stop();
      if (penEffect == NULL) penEffect = &*iten;
    }
  }
  if (penEffect != NULL) Play(Next(_apsoBoom, _apenBoom, _iBoom, penEffect), penEffect, "weapons\\explode3.wav|weapons\\explode4.wav|weapons\\explode5.wav", 1.0f, 80.0f);
  _ctExplosions++;
  CPrintF(SB_TAG " thrown went off at %.2f %.2f %.2f (%.2f s)%s\n", t.vLast(1), t.vLast(2), t.vLast(3), _pTimer->CurrentTick(),
    penEffect != NULL ? ", Half-Life's sound" : "");
  if (_pBlastHook != NULL) _pBlastHook(t.vLast, t.fRadius);
};

void Tick(void) {
  INDEX ctKept = 0;
  for (INDEX i = 0; i < _ctThrown; i++) {
    Thrown t = _aThrown[i];
    CEntity *pen = Find(t.ulID);
    if (pen == NULL) {
      Exploded(t);
      continue;
    }
    CMovableEntity *pmen = (CMovableEntity *)pen;
    // Sam's bounce sound out, Half-Life's in
    CSoundObject *psoSam = (CSoundObject *)player::Prop(pen, PJ_SOUND);
    if (psoSam != NULL && psoSam->IsPlaying() && t.kind != K_CONTACT && t.kind != K_TRIPMINE) {
      psoSam->Stop();
      Play(Next(_apsoBounce, _apenBounce, _iBounce, pen), pen, t.kind == K_SATCHEL
        ? "weapons\\g_bounce1.wav|weapons\\g_bounce2.wav|weapons\\g_bounce3.wav|weapons\\g_bounce4.wav|weapons\\g_bounce5.wav"
        : "weapons\\grenade_hit1.wav|weapons\\grenade_hit2.wav|weapons\\grenade_hit3.wav", 0.8f, 30.0f);
      _ctBounces++;
      if (!t.bBounced) {
        t.bBounced = TRUE;
        const FLOAT3D v = pen->GetPlacement().pl_PositionVector;
        CPrintF(SB_TAG " thrown bounced first at %.2f %.2f %.2f (%.2f s)\n", v(1), v(2), v(3), _pTimer->CurrentTick());
      }
    }
    // half of Half-Life's gravity (400 u/s², as the level's against Sam's usual 30 m/s²): the rest taken back in the air
    if (pmen->en_penReference == NULL && pmen->en_fGravityA > 0.0f && t.kind != K_TRIPMINE) {
      const FLOAT fHalfLife = 400.0f * Unit() * pmen->en_fGravityA / 30.0f;
      pmen->en_vCurrentTranslationAbsolute -= pmen->en_vGravityDir * ((pmen->en_fGravityA - fHalfLife) * _pTimer->TickQuantum);
    }
    t.vLast = pen->GetPlacement().pl_PositionVector;
    _aThrown[ctKept++] = t;
  }
  _ctThrown = ctKept;
};

void Release(void) {
  for (INDEX i = 0; i < 8; i++) {
    CSoundObject *apso[2] = { _apsoBounce[i], _apsoBoom[i] };
    for (INDEX j = 0; j < 2; j++) {
      if (apso[j] == NULL) continue;
      apso[j]->Stop();
      apso[j]->SetOwner(NULL);
    }
    _apenBounce[i] = NULL;
    _apenBoom[i] = NULL;
  }
  _ctThrown = 0;
};

CTString Status(void) {
  CTString str;
  str.PrintF("thrown: %d out (%d satchels), %u made, %u bounces, %u explosions", _ctThrown, Satchels(), _ctMade, _ctBounces, _ctExplosions);
  return str;
};

}; // namespace
