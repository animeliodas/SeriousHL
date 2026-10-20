/* SeriousHL - Half-Life's tripmines. See Tripmine.h. */
#include "StdH.h"
#include "Tripmine.h"
#include "Grenades.h"
#include "HlMove.h"
#include "Physics.h"
#include "Player.h"
#include "Sounds.h"
#include "ViewModel.h"
#include "SBridge.h"

#include <Engine/Brushes/Brush.h>
#include <stdlib.h>
#include <vector>

namespace tripmine {

static const FLOAT REACH = 128.0f;        // the weapon's trace from the eyes
static const FLOAT OFF_WALL = 8.0f;       // the mine's origin off the wall
static const FLOAT BEAM = 2048.0f;        // m_vecEnd
static const FLOAT POWERUP = 2.5f;
static const FLOAT DAMAGE = 150.0f;       // sk_plr_tripmine
static const FLOAT THINK = 0.1f;

struct Mine {
  CEntityPointer penGrenade; // (held: its sounds' owner stays while they're on)
  FLOAT3D vPos, vAhead, vLeft, vUp;
  TIME tmPlaced, tmPowerUp, tmThink;
  BOOL bPowered, bBlowing;
  FLOAT fBeam;               // how far the beam reaches, metres
  ULONG ulWall;              // what it's on, and where that was
  CPlacement3D plWall;
  CSoundObject *psoVoice, *psoBody;
};
static std::vector<Mine> _aMines;
static ULONG _ctPlaced = 0, _ctPowered = 0, _ctBroken = 0, _ctChained = 0, _ctCasts = 0;
static FLOAT _fLastBeam = 0.0f, _fLastCastMs = 0.0f;
static CTString _strLastBreak = "-";

static FLOAT Unit(void) { return hl::_settings.fUnit; };

static void Play(CSoundObject *pso, CEntity *penOwner, const char *strSound, FLOAT fVolume, FLOAT fPitch) {
  CSoundData *psd = sounds::Pick(strSound, (ULONG)rand());
  if (pso == NULL || psd == NULL) return;
  pso->SetOwner(penOwner);
  pso->Set3DParameters(40.0f, 4.0f, fVolume, fPitch);
  pso->Play(psd, SOF_3D | SOF_VOLUMETRIC);
};

static void Quiet(Mine &m) {
  CSoundObject *apso[2] = { m.psoVoice, m.psoBody };
  for (INDEX i = 0; i < 2; i++) {
    if (apso[i] == NULL) continue;
    apso[i]->Stop();
    apso[i]->SetOwner(NULL);
    delete apso[i];
  }
  m.psoVoice = m.psoBody = NULL;
};

BOOL Aim(CEntity *pen, FLOAT3D &vHit, FLOAT3D &vNormal, CEntity *&penWall) {
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL || pen == NULL) return FALSE;
  const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)pen, FALSE);
  FLOAT3D vDir;
  AnglesToDirectionVector(plView.pl_OrientationAngle, vDir);
  CCastRay cr(pen, plView.pl_PositionVector, plView.pl_PositionVector + vDir * (REACH * Unit()));
  cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
  cr.Cast(pwo);
  if (cr.cr_penHit == NULL || cr.cr_pbpoBrushPolygon == NULL || cr.cr_pbpoBrushPolygon->bpo_pbplPlane == NULL) return FALSE;
  vNormal = FLOAT3D(cr.cr_pbpoBrushPolygon->bpo_pbplPlane->bpl_plAbsolute);
  if (vNormal % vDir > 0.0f) vNormal = -vNormal;
  vHit = cr.cr_vHit;
  penWall = cr.cr_penHit;
  return TRUE;
};

void Add(CEntity *penGrenade, CEntity *penPlayer, const FLOAT3D &vNormal, CEntity *penWall) {
  if (penGrenade == NULL) return;
  CMovableEntity *pmen = (CMovableEntity *)penGrenade;
  // still: no gravity, no speed, no spin; unseen: no model to speak of, no trail
  pmen->SetPhysicsFlags(pmen->GetPhysicsFlags() & ~(EPF_TRANSLATEDBYGRAVITY | EPF_ORIENTEDBYGRAVITY));
  pmen->en_vCurrentTranslationAbsolute = FLOAT3D(0, 0, 0);
  pmen->en_vDesiredTranslationRelative = FLOAT3D(0, 0, 0);
  pmen->en_aDesiredRotationRelative = ANGLE3D(0, 0, 0);
  pmen->en_aCurrentRotationAbsolute = ANGLE3D(0, 0, 0);
  CModelObject *pmo = penGrenade->GetModelObject();
  if (pmo != NULL) {
    pmo->StretchModel(FLOAT3D(0.001f, 0.001f, 0.001f));
    penGrenade->ModelChangeNotify();
  }
  penGrenade->SetFlags(penGrenade->GetFlags() & ~ENF_HASPARTICLES);

  Mine m;
  m.penGrenade = penGrenade;
  m.vPos = penGrenade->GetPlacement().pl_PositionVector;
  // its axes: ahead out of the wall, up as near the world's up as it can be (on a floor or a ceiling: the way he looked)
  m.vAhead = vNormal;
  const FLOAT3D vWorldUp = -((CMovableEntity *)penPlayer)->en_vGravityDir;
  FLOAT3D vRef = vWorldUp;
  if (Abs(vNormal % vWorldUp) > 0.99f) {
    const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)penPlayer, FALSE);
    AnglesToDirectionVector(plView.pl_OrientationAngle, vRef);
    if (vNormal % vWorldUp > 0.0f) vRef = -vRef;
  }
  m.vUp = vRef - vNormal * (vRef % vNormal);
  m.vUp /= Max(m.vUp.Length(), 0.0001f);
  m.vLeft = m.vUp * m.vAhead;
  const TIME tmNow = _pTimer->CurrentTick();
  m.tmPlaced = tmNow;
  m.tmPowerUp = tmNow + POWERUP;
  m.tmThink = tmNow + 0.2f;
  m.bPowered = m.bBlowing = FALSE;
  m.fBeam = 0.0f;
  m.ulWall = penWall != NULL ? penWall->en_ulID : 0;
  m.plWall = penWall != NULL ? penWall->GetPlacement() : CPlacement3D(FLOAT3D(0, 0, 0), ANGLE3D(0, 0, 0));
  m.psoVoice = new CSoundObject;
  m.psoBody = new CSoundObject;
  Play(m.psoVoice, penGrenade, "weapons\\mine_deploy.wav", 1.0f, 1.0f);
  Play(m.psoBody, penGrenade, "weapons\\mine_charge.wav", 0.2f, 1.0f);
  _aMines.push_back(m);
  _ctPlaced++;
  CPrintF(SB_TAG " tripmine placed at %.2f %.2f %.2f facing %.2f %.2f %.2f (%.2f s)\n", m.vPos(1), m.vPos(2), m.vPos(3),
    vNormal(1), vNormal(2), vNormal(3), tmNow);
};

// what stops the beam (Half-Life's traces go through triggers, items, the dead): brushes, the living, projectiles
static BOOL Blocks(CEntity *pen) {
  if (pen->IsPredictor()) return FALSE;
  if (pen->en_RenderType == CEntity::RT_BRUSH) return TRUE;
  if (pen->GetFlags() & ENF_ALIVE) return TRUE;
  return strcmp(pen->GetClass()->ec_pdecDLLClass->dec_strName, "Projectile") == 0;
};

// how far the beam goes from the mine, metres
static FLOAT Cast(Mine &m, CEntity **ppenHit = NULL) {
  CWorld *pwo = IWorld::GetWorld();
  const FLOAT fReach = BEAM * Unit();
  if (pwo == NULL) return fReach;
  const CTimerValue tvStart = _pTimer->GetHighPrecisionTimer();
  CCastRay cr(m.penGrenade, m.vPos, m.vPos + m.vAhead * fReach);
  cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
  cr.Cast(pwo);
  for (INDEX i = 0; i < 8 && cr.cr_penHit != NULL && !Blocks(cr.cr_penHit); i++) cr.ContinueCast(pwo);
  _ctCasts++;
  _fLastCastMs = (FLOAT)((_pTimer->GetHighPrecisionTimer() - tvStart).GetSeconds() * 1000.0);
  if (ppenHit != NULL) *ppenHit = cr.cr_penHit;
  return cr.cr_penHit != NULL ? (cr.cr_vHit - m.vPos).Length() : fReach;
};

// the wall it's on moved (a door, a lift)
static BOOL WallMoved(const Mine &m) {
  if (m.ulWall == 0) return FALSE;
  CEntity *penWall = physics::FindEntity(m.ulWall);
  if (penWall == NULL) return TRUE;
  const CPlacement3D &pl = penWall->GetPlacement();
  return (pl.pl_PositionVector - m.plWall.pl_PositionVector).Length() > 0.001f
      || (pl.pl_OrientationAngle - m.plWall.pl_OrientationAngle).Length() > 0.01f;
};

// Killed(): 0.1-0.3 s later, from 0.6 x (damage - 24) units out of the wall
static void Blow(Mine &m, const char *strWhy) {
  if (m.bBlowing) return;
  m.bBlowing = TRUE;
  if (m.psoBody != NULL) m.psoBody->Stop();
  CEntity *penG = m.penGrenade;
  if (penG == NULL || (penG->GetFlags() & ENF_DELETED)) return;
  const FLOAT3D vWall = m.vPos - m.vAhead * (OFF_WALL * Unit());
  penG->SetPlacement(CPlacement3D(vWall + m.vAhead * ((DAMAGE - 24.0f) * 0.6f * Unit()), penG->GetPlacement().pl_OrientationAngle));
  ((CRationalEntity *)penG)->SetTimerAfter(0.1f + (rand() % 201) / 1000.0f);
  _strLastBreak = strWhy;
  CPrintF(SB_TAG " tripmine set off (%s) at %.2f %.2f %.2f (%.2f s)\n", strWhy, m.vPos(1), m.vPos(2), m.vPos(3), _pTimer->CurrentTick());
};

// another blast near: Half-Life's mines have 1 health - any of the radius damage sets them off (seen from the blast)
static void Blast(const FLOAT3D &vAt, FLOAT fRadius) {
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL) return;
  for (size_t i = 0; i < _aMines.size(); i++) {
    Mine &m = _aMines[i];
    if (m.bBlowing || m.penGrenade == NULL) continue;
    if ((m.vPos - vAt).Length() >= fRadius) continue;
    CCastRay cr(m.penGrenade, m.vPos, vAt);
    cr.cr_ttHitModels = CCastRay::TT_NONE;
    cr.Cast(pwo);
    if (cr.cr_penHit != NULL && (cr.cr_vHit - vAt).Length() > 0.5f) continue;
    _ctChained++;
    Blow(m, "a blast");
  }
};

void Tick(CEntity *penPlayer) {
  grenades::_pBlastHook = &Blast;
  const TIME tmNow = _pTimer->CurrentTick();
  for (size_t i = 0; i < _aMines.size();) {
    Mine &m = _aMines[i];
    CEntity *penG = m.penGrenade;
    if (penG == NULL || (penG->GetFlags() & ENF_DELETED)) {
      // (gone: it went off)
      Quiet(m);
      _aMines.erase(_aMines.begin() + i);
      continue;
    }
    i++;
    if (m.bBlowing || tmNow + 0.01f < m.tmThink) continue;
    m.tmThink = tmNow + THINK;
    if (!m.bPowered) {
      // PowerupThink: its wall moving takes it off (Half-Life drops it as a pickup: here back to the ammo)
      if (WallMoved(m)) {
        Quiet(m);
        penG->Destroy();
        CEntity *pw = penPlayer != NULL ? player::PropEntity(penPlayer, (401UL << 8) | 16) : NULL;
        INDEX *piBalls = pw != NULL ? (INDEX *)player::Prop(pw, (402UL << 8) | 52) : NULL;
        if (piBalls != NULL) (*piBalls)++;
        continue;
      }
      if (tmNow + 0.01f < m.tmPowerUp) continue;
      m.bPowered = TRUE;
      CEntity *penHit = NULL;
      m.fBeam = Cast(m, &penHit);
      _fLastBeam = m.fBeam;
      _ctPowered++;
      if (m.psoVoice != NULL) Play(m.psoVoice, penG, "weapons\\mine_activate.wav", 0.5f, 0.75f);
      CPrintF(SB_TAG " tripmine on: beam %.2f m (%.0f units) to %s (%u) (%.2f s)\n", m.fBeam, m.fBeam / Unit(),
        penHit != NULL ? penHit->GetClass()->ec_pdecDLLClass->dec_strName : "nothing", penHit != NULL ? penHit->en_ulID : 0, tmNow);
      continue;
    }
    // BeamBreakThink: the beam's length changed (by more than a thousandth of its whole reach), or the wall moved
    CEntity *penHit = NULL;
    const FLOAT fNow = Cast(m, &penHit);
    if (Abs(fNow - m.fBeam) > 0.001f * BEAM * Unit()) {
      _ctBroken++;
      CTString strWhy;
      strWhy.PrintF("beam %.2f -> %.2f m, %s", m.fBeam, fNow, penHit != NULL ? penHit->GetClass()->ec_pdecDLLClass->dec_strName : "nothing");
      Blow(m, strWhy.str_String);
    } else if (WallMoved(m)) {
      _ctBroken++;
      Blow(m, "its wall moved");
    }
  }
};

void Render(CAnyProjection3D &apr, CDrawPort *pdp) {
  if (_aMines.empty()) return;
  static std::vector<viewmodel::Placed> aPlaced;
  static std::vector<viewmodel::Beam> aBeams;
  aPlaced.clear();
  aBeams.clear();
  const FLOAT fNow = _pTimer->GetLerpedCurrentTick();
  for (size_t i = 0; i < _aMines.size(); i++) {
    const Mine &m = _aMines[i];
    viewmodel::Placed pl;
    pl.strModel = "v_tripmine.mdl";
    pl.iSeq = 7; // TRIPMINE_WORLD
    pl.fFrame = (fNow - m.tmPlaced) * 30.0f;
    pl.iBody = 3;
    pl.vPos = m.vPos;
    pl.vAhead = m.vAhead;
    pl.vLeft = m.vLeft;
    pl.vUp = m.vUp;
    pl.fUnit = Unit();
    pl.fLight = 0.9f;
    aPlaced.push_back(pl);
    if (!m.bPowered || m.bBlowing) continue;
    // MakeBeam: laserbeam, width 10 (tenths of a unit), 0 214 198 at brightness 64
    viewmodel::Beam be;
    be.vFrom = m.vPos;
    be.vTo = m.vPos + m.vAhead * m.fBeam;
    be.fWidth = 1.0f * Unit();
    be.fR = 0.0f;
    be.fG = (214.0f / 255.0f) * (64.0f / 255.0f);
    be.fB = (198.0f / 255.0f) * (64.0f / 255.0f);
    be.strSprite = "laserbeam";
    aBeams.push_back(be);
  }
  viewmodel::DrawWorld(apr, pdp, &aPlaced[0], (INDEX)aPlaced.size(), aBeams.empty() ? NULL : &aBeams[0], (INDEX)aBeams.size());
};

INDEX Count(void) { return (INDEX)_aMines.size(); };

void Release(void) {
  for (size_t i = 0; i < _aMines.size(); i++) Quiet(_aMines[i]);
  _aMines.clear();
};

CTString Status(void) {
  CTString str;
  INDEX ctOn = 0;
  for (size_t i = 0; i < _aMines.size(); i++) if (_aMines[i].bPowered && !_aMines[i].bBlowing) ctOn++;
  str.PrintF("tripmines: %d out (%d on), %u placed, %u powered, %u set off by their beams (last: %s), %u by blasts; last beam %.2f m; %u casts (last %.3f ms)",
    (INDEX)_aMines.size(), ctOn, _ctPlaced, _ctPowered, _ctBroken, _strLastBreak.str_String, _ctChained, _fLastBeam, _ctCasts, _fLastCastMs);
  return str;
};

}; // namespace
