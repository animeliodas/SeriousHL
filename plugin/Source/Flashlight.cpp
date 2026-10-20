/* SeriousHL - Half-Life's flashlight. See Flashlight.h. */
#include "StdH.h"
#include "Flashlight.h"
#include "Player.h"
#include "Sounds.h"
#include "HlMove.h"
#include "SBridge.h"

namespace flashlight {

INDEX hl_bFlashlight = TRUE;
INDEX hl_iFlashlightKey = 'F';
INDEX hl_bTestFlashlight = FALSE;

// Light.es (class 200)
static const ULONG LIGHT_FALLOFF = (200UL << 8) | 1, LIGHT_COLOR = (200UL << 8) | 2, LIGHT_HOTSPOT = (200UL << 8) | 4;
static const ULONG LIGHT_NAME = (200UL << 8) | 11, LIGHT_DYNAMIC = (200UL << 8) | 26;
static const char *LIGHT_TAG = "SeriousHL flashlight";

static const FLOAT DRAIN_TIME = 1.2f, CHARGE_TIME = 0.2f; // seconds a percent
static BOOL _bOn = FALSE, _bKeyWas = FALSE, _bSearched = FALSE;
static INDEX _iBattery = 100;
static TIME _tmBattery = 0;
static ULONG _ulLight = 0;
static ULONG _ctSwitches = 0;
static FLOAT3D _vLastSpot(0, 0, 0);
static CSoundObject *_pso = NULL;

static CEntity *Light(void) {
  if (_ulLight == 0) return NULL;
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL) return NULL;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    if (iten->en_ulID == _ulLight && !(iten->GetFlags() & ENF_DELETED)) return &*iten;
  }
  return NULL;
};

static void RemoveLight(void) {
  CEntity *pen = Light();
  if (pen != NULL) pen->Destroy();
  _ulLight = 0;
};

// (one left in a saved game: found by its name)
static void RemoveStray(void) {
  CWorld *pwo = IWorld::GetWorld();
  if (pwo == NULL) return;
  CDynamicContainer<CEntity> cen;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    if (iten->en_ulID == _ulLight || (iten->GetFlags() & ENF_DELETED)) continue;
    if (iten->GetClass()->ec_pdecDLLClass->dec_iID != 200) continue;
    const CTString *pstr = (const CTString *)player::Prop(&*iten, LIGHT_NAME);
    if (pstr != NULL && *pstr == LIGHT_TAG) cen.Add(&*iten);
  }
  FOREACHINDYNAMICCONTAINER(cen, CEntity, itenStray) itenStray->Destroy();
};

static BOOL KeyPressed(void) {
  if (hl_bTestFlashlight) {
    hl_bTestFlashlight = FALSE;
    return TRUE;
  }
  BOOL bDown = FALSE;
  if (hl_iFlashlightKey > 0 && hl_iFlashlightKey < 256) {
    DWORD dwPid = 0;
    GetWindowThreadProcessId(GetForegroundWindow(), &dwPid);
    CGameAPI *pGame = GetGameAPI();
    if (dwPid == GetCurrentProcessId() && !pGame->IsMenuOn() && pGame->GetConState() == 0 && pGame->GetCompState() == 0) {
      bDown = (GetAsyncKeyState(hl_iFlashlightKey) & 0x8000) != 0;
    }
  }
  const BOOL bPressed = bDown && !_bKeyWas;
  _bKeyWas = bDown;
  return bPressed;
};

static void Click(CEntity *pen) {
  CSoundData *psd = sounds::Data("items\\flashlight1.wav");
  if (psd == NULL) return;
  if (_pso == NULL) _pso = new CSoundObject;
  _pso->SetOwner(pen);
  _pso->Set3DParameters(30.0f, 3.0f, 1.0f, 1.0f);
  _pso->Play(psd, SOF_3D | SOF_VOLUMETRIC);
};

// the spot the view's trace ends at (2048 units), a little back from it
static FLOAT3D Spot(CEntity *pen) {
  const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)pen, TRUE);
  FLOAT3D vDir;
  AnglesToDirectionVector(plView.pl_OrientationAngle, vDir);
  const FLOAT fReach = 2048.0f * hl::_settings.fUnit;
  CCastRay cr(pen, plView.pl_PositionVector, plView.pl_PositionVector + vDir * fReach);
  cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
  cr.cr_bHitTranslucentPortals = FALSE;
  IWorld::GetWorld()->CastRay(cr);
  const FLOAT fHit = cr.cr_penHit != NULL ? cr.cr_fHitDistance : fReach;
  return plView.pl_PositionVector + vDir * Max(fHit - 0.3f, 0.1f);
};

void Tick(CEntity *pen) {
  if (pen == NULL || IWorld::GetWorld() == NULL) return;
  if (!_bSearched) {
    RemoveStray();
    _bSearched = TRUE;
  }
  const TIME tmNow = _pTimer->CurrentTick();
  const BOOL bAlive = (pen->GetFlags() & ENF_ALIVE) != 0;
  if (KeyPressed() && hl_bFlashlight && bAlive) {
    _bOn = !_bOn && _iBattery > 0;
    _tmBattery = tmNow;
    _ctSwitches++;
    Click(pen);
  }
  if (!hl_bFlashlight || !bAlive) _bOn = FALSE;
  // the battery
  if (_bOn) {
    while (tmNow - _tmBattery >= DRAIN_TIME && _iBattery > 0) {
      _iBattery--;
      _tmBattery += DRAIN_TIME;
    }
    if (_iBattery <= 0) {
      _bOn = FALSE;
      Click(pen);
    }
  } else {
    while (tmNow - _tmBattery >= CHARGE_TIME && _iBattery < 100) {
      _iBattery++;
      _tmBattery += CHARGE_TIME;
    }
    if (_iBattery >= 100) _tmBattery = tmNow;
  }
  // the light
  if (!_bOn) {
    RemoveLight();
    return;
  }
  _vLastSpot = Spot(pen);
  const CPlacement3D plSpot(_vLastSpot, ANGLE3D(0, 0, 0));
  CEntity *penLight = Light();
  if (penLight != NULL) {
    penLight->SetPlacement(plSpot);
    return;
  }
  try {
    penLight = IWorld::GetWorld()->CreateEntity_t(plSpot, CTString("Classes\\Light.ecl"));
  } catch (char *strError) {
    CPrintF(SB_TAG " flashlight: %s\n", strError);
    _bOn = FALSE;
    return;
  }
  // white, about 80 Half-Life units round, dynamic
  COLOR *pcol = (COLOR *)player::Prop(penLight, LIGHT_COLOR);
  FLOAT *pfFallOff = (FLOAT *)player::Prop(penLight, LIGHT_FALLOFF);
  FLOAT *pfHotSpot = (FLOAT *)player::Prop(penLight, LIGHT_HOTSPOT);
  BOOL *pbDynamic = (BOOL *)player::Prop(penLight, LIGHT_DYNAMIC);
  CTString *pstrName = (CTString *)player::Prop(penLight, LIGHT_NAME);
  if (pcol != NULL) *pcol = 0xE0E0D0FF;
  if (pfFallOff != NULL) *pfFallOff = 80.0f * hl::_settings.fUnit * 1.5f;
  if (pfHotSpot != NULL) *pfHotSpot = 40.0f * hl::_settings.fUnit;
  if (pbDynamic != NULL) *pbDynamic = TRUE;
  if (pstrName != NULL) *pstrName = LIGHT_TAG;
  penLight->Initialize();
  _ulLight = penLight->en_ulID;
};

void Release(void) {
  // (the light entity goes with the world; a new one finds strays again)
  _ulLight = 0;
  _bOn = FALSE;
  _bSearched = FALSE;
  if (_pso != NULL) {
    _pso->Stop();
    _pso->SetOwner(NULL);
  }
};

BOOL On(void) { return _bOn; };
FLOAT Battery(void) { return _iBattery / 100.0f; };

CTString Status(void) {
  CTString str;
  str.PrintF("flashlight %s, battery %d%%, %u switches, light %u at %.1f %.1f %.1f", _bOn ? "on" : "off", _iBattery, _ctSwitches,
    _ulLight, _vLastSpot(1), _vLastSpot(2), _vLastSpot(3));
  return str;
};

}; // namespace
