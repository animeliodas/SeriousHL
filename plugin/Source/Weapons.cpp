/* SeriousHL - Half-Life's weapons on Sam's guns. See Weapons.h. */
#include "StdH.h"
#include "Weapons.h"
#include "HlMove.h"
#include "Movement.h"
#include "Player.h"
#include "SBridge.h"

#include <Extras/XGizmo/Objects/StructPtr.h>
#include <stdio.h>

namespace weapons {

INDEX hl_bWeapons = TRUE;
INDEX hl_iAltFireKey = VK_RBUTTON;

// --- Player.es (class 401) and PlayerWeapons.es (class 402), the same in TFE and TSE for these
static const ULONG PROP_WEAPONS = (401UL << 8) | 16;   // m_penWeapons
static const ULONG PROP_SOUND0 = (401UL << 8) | 60;    // m_soWeapon0
static const ULONG PROP_SOUND1 = (401UL << 8) | 61;    // m_soWeapon1
static const ULONG PW_CURRENT = (402UL << 8) | 4;      // m_iCurrentWeapon
static const ULONG PW_WANTED = (402UL << 8) | 5;       // m_iWantedWeapon
static const ULONG PW_MODEL = (402UL << 8) | 25;       // m_moWeapon
static const ULONG PW_BULLETS = (402UL << 8) | 40;
static const ULONG PW_SHELLS = (402UL << 8) | 42;
static const ULONG PW_ROCKETS = (402UL << 8) | 44;
static const ULONG PW_GRENADES = (402UL << 8) | 46;
static const ULONG PW_ELECTRICITY = (402UL << 8) | 50;
// sound components of PlayerWeapons.es (their numbers there)
enum { SND_KNIFE_HIGH = 24, SND_KNIFE_LONG = 25, SND_COLT_FIRE = 37, SND_COLT_RELOAD = 38, SND_SHOTGUN_FIRE = 46,
  SND_DOUBLESHOTGUN_FIRE = 60, SND_DOUBLESHOTGUN_RELOAD = 61, SND_TOMMYGUN_FIRE = 74, SND_ROCKET_FIRE = 97,
  SND_GRENADE_FIRE = 106, SND_LASER_FIRE = 146 };
static const INDEX FLARE_ADD = 2;
// Player.es buttons
static const ULONG PLACT_FIRE = 1UL << 0, PLACT_RELOAD = 1UL << 1, PLACT_USE = 1UL << 5, PLACT_COMPUTER = 1UL << 6;
// Sam's weapons (WeaponType)
#if SE1_GAME == SS_TFE
enum { W_KNIFE = 1, W_COLT = 2, W_DOUBLECOLT = 3, W_SINGLESHOTGUN = 4, W_DOUBLESHOTGUN = 5, W_TOMMYGUN = 6,
  W_ROCKETLAUNCHER = 8, W_GRENADELAUNCHER = 9, W_LASER = 14, W_CHAINSAW = -100 };
#else
enum { W_KNIFE = 1, W_COLT = 2, W_DOUBLECOLT = 3, W_SINGLESHOTGUN = 4, W_DOUBLESHOTGUN = 5, W_TOMMYGUN = 6,
  W_ROCKETLAUNCHER = 8, W_GRENADELAUNCHER = 9, W_CHAINSAW = 10, W_LASER = 12 };
#endif

// --- Sam's fire functions (CPlayerWeapons, Entities library)
typedef void (CEntity::*FFireMachineBullet)(FLOAT fX, FLOAT fY, FLOAT fRange, FLOAT fDamage, FLOAT fJitter, FLOAT fBulletSize);
typedef void (CEntity::*FFireBullets)(FLOAT fX, FLOAT fY, FLOAT fRange, FLOAT fDamage, INDEX iBullets, FLOAT *afPositions, FLOAT fStretch, FLOAT fJitter);
typedef BOOL (CEntity::*FCutWithKnife)(FLOAT fX, FLOAT fY, FLOAT fRange, FLOAT fWide, FLOAT fThickness, FLOAT fDamage);
typedef void (CEntity::*FVoid)(void);
typedef void (CEntity::*FIndex)(INDEX i);
typedef void (CEntity::*FFloat)(FLOAT f);
typedef void (CEntity::*FFlare)(INDEX iFlare, INDEX iState);
static FFireMachineBullet _pFireMachineBullet = NULL;
static FFireBullets _pFireBullets = NULL;
static FCutWithKnife _pCutWithKnife = NULL;
static FVoid _pFireRocket = NULL, _pDoRecoil = NULL;
static FIndex _pFireGrenade = NULL;
static FFloat _pSpawnRangeSound = NULL;
static FFlare _pSetFlare = NULL;
static BOOL _bReady = FALSE;
static CSymbolPtr _symFX, _symFY;

// --- Half-Life's guns
enum Gun { G_NONE, G_CROWBAR, G_PYTHON, G_GLOCK, G_SHOTGUN, G_MP5, G_RPG, G_GRENADE, G_GAUSS, G_COUNT };
static const char *_astrGun[G_COUNT] = { "-", "crowbar", ".357", "glock", "shotgun", "mp5", "rpg", "grenade", "gauss" };
static const INDEX _aiClip[G_COUNT] = { 0, 0, 6, 17, 8, 50, 1, 0, 0 };
static const FLOAT _afReload[G_COUNT] = { 0, 0, 2.0f, 1.5f, 0.5f, 1.5f, 2.0f, 0, 0 }; // (shotgun: a shell)

// skill.cfg sk_plr_*, in Half-Life's units; the fire functions take metres
static const FLOAT DMG_CROWBAR = 10.0f, DMG_357 = 40.0f, DMG_9MM = 8.0f, DMG_BUCKSHOT = 5.0f, DMG_MP5 = 5.0f;
static const FLOAT DMG_GAUSS = 20.0f, DMG_GAUSS_FULL = 200.0f;
// VECTOR_CONE_*DEGREES (sin of the half angle)
static const FLOAT CONE_1 = 0.00873f, CONE_3 = 0.02618f, CONE_10 = 0.08716f;

struct GunState {
  INDEX iClip;
  TIME tmNext;      // next attack
  TIME tmReloaded;  // reload done at (0: not reloading)
};
static GunState _aGuns[G_COUNT];
static INDEX _iLastSam = -1;
static BOOL _bOldFire = FALSE, _bOldAlt = FALSE;
static BOOL _bCharging = FALSE;   // gauss
static TIME _tmChargeStart = 0, _tmChargeDrain = 0;
static BOOL _bPinPulled = FALSE;  // hand grenade
static ULONG _ctShots = 0;
static FLOAT _fLastDamage = 0;
static const char *_strLast = "";

// tests
INDEX hl_iTestAlt = 0; // the second fire held for this many actions

static FLOAT Unit(void) { return hl::_settings.fUnit; };

// a time has come (the ticks' times are sums of floats: t + 0.1 may come out a hair short of two ticks later)
static BOOL Due(TIME tmNow, TIME tm) { return tmNow + 0.01f >= tm; };

static Gun GunFor(INDEX iSam) {
  switch (iSam) {
    case W_KNIFE: case W_CHAINSAW: return G_CROWBAR;
    case W_COLT: return G_PYTHON;
    case W_DOUBLECOLT: return G_GLOCK;
    case W_SINGLESHOTGUN: case W_DOUBLESHOTGUN: return G_SHOTGUN;
    case W_TOMMYGUN: return G_MP5;
    case W_ROCKETLAUNCHER: return G_RPG;
    case W_GRENADELAUNCHER: return G_GRENADE;
    case W_LASER: return G_GAUSS;
  }
  return G_NONE;
};

// the reserve a gun reloads from: Sam's ammo (NULL: endless, or the gun has no magazine)
static INDEX *Reserve(CEntity *pw, Gun g) {
  switch (g) {
    case G_GLOCK: case G_MP5: return (INDEX *)player::Prop(pw, PW_BULLETS);
    case G_SHOTGUN: return (INDEX *)player::Prop(pw, PW_SHELLS);
    case G_RPG: return (INDEX *)player::Prop(pw, PW_ROCKETS);
    case G_GRENADE: return (INDEX *)player::Prop(pw, PW_GRENADES);
    case G_GAUSS: return (INDEX *)player::Prop(pw, PW_ELECTRICITY);
  }
  return NULL;
};

static FLOAT FireX(INDEX iSam) { return _symFX.Exists() ? ((FLOAT *)_symFX.GetValue())[iSam] : 0.0f; };
static FLOAT FireY(INDEX iSam) { return _symFY.Exists() ? ((FLOAT *)_symFY.GetValue())[iSam] : 0.0f; };

static void Sound(CEntity *pen, CEntity *pw, INDEX iSound, BOOL bSecond) {
  CSoundObject *pso = (CSoundObject *)player::Prop(pen, bSecond ? PROP_SOUND1 : PROP_SOUND0);
  // (a component's id is its class's with its number: (402 << 8) | n)
  if (pso != NULL) pw->PlaySound(*pso, (402L << 8) | iSound, SOF_3D | SOF_VOLUMETRIC);
};

// Sam's fire animation of the gun in hand, by name (the first of the names the model has)
static void Anim(CEntity *pw, const char *strNames) {
  CModelObject *pmo = (CModelObject *)player::Prop(pw, PW_MODEL);
  if (pmo == NULL || pmo->GetData() == NULL) return;
  char strList[128];
  strncpy(strList, strNames, sizeof(strList) - 1);
  strList[sizeof(strList) - 1] = 0;
  for (char *strName = strtok(strList, ","); strName != NULL; strName = strtok(NULL, ",")) {
    for (INDEX i = 0; i < pmo->GetAnimsCt(); i++) {
      CAnimInfo ai;
      pmo->GetAnimInfo(i, ai);
      if (stricmp(ai.ai_AnimName, strName) == 0) {
        pmo->PlayAnim(i, 0);
        return;
      }
    }
  }
};

static void Flash(CEntity *pw) {
  if (_pSetFlare != NULL) (pw->*_pSetFlare)(0, FLARE_ADD);
  if (_pDoRecoil != NULL) (pw->*_pDoRecoil)();
};

static void Noise(CEntity *pw, FLOAT fRange) {
  if (_pSpawnRangeSound != NULL) (pw->*_pSpawnRangeSound)(fRange);
};

// Half-Life's spread: per pellet two random numbers in the cone (the sum of two uniforms, as FireBulletsPlayer)
static FLOAT Spread(CEntity *pw) { return (pw->FRnd() - 0.5f) + (pw->FRnd() - 0.5f); };

static void Bullet(CEntity *pw, INDEX iSam, FLOAT fRangeUnits, FLOAT fDamage, FLOAT fCone) {
  (pw->*_pFireMachineBullet)(FireX(iSam), FireY(iSam), fRangeUnits * Unit(), fDamage, fCone, 0.2f);
  _ctShots++;
  _fLastDamage = fDamage;
};

static void Pellets(CEntity *pw, INDEX iSam, INDEX ctPellets, FLOAT fDamage, FLOAT fCone) {
  FLOAT afPositions[2 * 12];
  for (INDEX i = 0; i < ctPellets; i++) {
    afPositions[i * 2 + 0] = Spread(pw) * fCone;
    afPositions[i * 2 + 1] = Spread(pw) * fCone;
  }
  (pw->*_pFireBullets)(FireX(iSam), FireY(iSam), 2048.0f * Unit(), fDamage, ctPellets, afPositions, 1.0f, 0.0f);
  _ctShots += ctPellets;
  _fLastDamage = fDamage * ctPellets;
};

// the view's forward direction (Sam's eyes)
static FLOAT3D Forward(CEntity *pen) {
  const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)pen, FALSE);
  FLOAT3D vDir;
  AnglesToDirectionVector(plView.pl_OrientationAngle, vDir);
  return vDir;
};

static void Log(const char *strWhat, Gun g, INDEX iClip, INDEX iReserve) {
  _strLast = strWhat;
  CPrintF(SB_TAG " %s %s: clip %d, reserve %d\n", _astrGun[g], strWhat, iClip, iReserve);
};

// --- every action of the tracked player, before Player.es applies it
static void Filter(CEntity *pen, CPlayerAction &pa);

static BOOL AltDown(void) {
  if (hl_iTestAlt > 0) return TRUE;
  if (hl_iAltFireKey <= 0 || hl_iAltFireKey >= 256) return FALSE;
  HWND hwnd = GetForegroundWindow();
  DWORD dwPid = 0;
  GetWindowThreadProcessId(hwnd, &dwPid);
  if (dwPid != GetCurrentProcessId()) return FALSE;
  CGameAPI *pGame = GetGameAPI();
  if (pGame->IsMenuOn() || pGame->GetConState() != 0 || pGame->GetCompState() != 0) return FALSE;
  return (GetAsyncKeyState(hl_iAltFireKey) & 0x8000) != 0;
};

static void StartReload(CEntity *pen, CEntity *pw, Gun g, GunState &gs, TIME tmNow) {
  INDEX *piReserve = Reserve(pw, g);
  if (gs.tmReloaded > 0 || gs.iClip >= _aiClip[g]) return;
  if (piReserve != NULL && *piReserve <= 0) return;
  gs.tmReloaded = tmNow + _afReload[g];
  if (g == G_SHOTGUN) Sound(pen, pw, SND_DOUBLESHOTGUN_RELOAD, TRUE);
  else if (g == G_PYTHON || g == G_GLOCK) Sound(pen, pw, SND_COLT_RELOAD, TRUE);
  Log("reload", g, gs.iClip, piReserve != NULL ? *piReserve : -1);
};

static void FinishReload(CEntity *pw, Gun g, GunState &gs, TIME tmNow) {
  if (gs.tmReloaded <= 0 || !Due(tmNow, gs.tmReloaded)) return;
  gs.tmReloaded = 0;
  INDEX *piReserve = Reserve(pw, g);
  // the shotgun takes a shell at a time
  const INDEX ctWant = g == G_SHOTGUN ? 1 : _aiClip[g] - gs.iClip;
  const INDEX ctTake = piReserve != NULL ? Min(ctWant, *piReserve) : ctWant;
  gs.iClip += ctTake;
  if (piReserve != NULL) *piReserve -= ctTake;
  gs.tmNext = Max(gs.tmNext, tmNow);
};

static void Filter(CEntity *pen, CPlayerAction &pa) {
  if (!_bReady || !hl_bWeapons || !(pen->GetFlags() & ENF_ALIVE)) return;
  CEntity *pw = player::PropEntity(pen, PROP_WEAPONS);
  if (pw == NULL) return;
  const INDEX iSam = player::PropIndex(pw, PW_CURRENT, 0);
  const INDEX iWanted = player::PropIndex(pw, PW_WANTED, 0);
  const Gun g = GunFor(iSam);
  const BOOL bAlt = AltDown();
  if (hl_iTestAlt > 0) hl_iTestAlt--;
  if (g == G_NONE) {
    _iLastSam = iSam;
    _bCharging = FALSE;
    return;
  }
  // Sam's own firing and reloading are out; the right mouse button doesn't open Netricsa
  const BOOL bFire = (pa.pa_ulButtons & PLACT_FIRE) != 0;
  const BOOL bReloadKey = (pa.pa_ulButtons & PLACT_RELOAD) != 0;
  pa.pa_ulButtons &= ~(PLACT_FIRE | PLACT_RELOAD);
  if (bAlt) pa.pa_ulButtons &= ~(PLACT_USE | PLACT_COMPUTER);

  const TIME tmNow = _pTimer->CurrentTick();
  GunState &gs = _aGuns[g];
  // switching: the new gun is drawn first
  if (iSam != iWanted) {
    _bCharging = FALSE;
    _bPinPulled = FALSE;
    return;
  }
  if (iSam != _iLastSam) {
    _iLastSam = iSam;
    gs.tmNext = Max(gs.tmNext, tmNow + 0.5f);
    _bCharging = FALSE;
    _bPinPulled = FALSE;
  }
  FinishReload(pw, g, gs, tmNow);
  INDEX *piReserve = Reserve(pw, g);
  const INDEX iReserve = piReserve != NULL ? *piReserve : 999;

  // reloading: by the key, or an empty magazine with something to put in it (the shotgun: shell by shell, until fired)
  if (_aiClip[g] > 0) {
    if ((bReloadKey || gs.iClip == 0 || (g == G_SHOTGUN && gs.tmReloaded == 0 && gs.iClip < _aiClip[g] && !bFire && !bAlt && Due(tmNow, gs.tmNext)))
      && !(g == G_SHOTGUN && (bFire || bAlt) && gs.iClip > 0)) {
      StartReload(pen, pw, g, gs, tmNow);
    }
    if (g == G_SHOTGUN && (bFire || bAlt) && gs.iClip > 0) gs.tmReloaded = 0; // firing stops the shell loading
    if (gs.tmReloaded > 0) return;
  }

  switch (g) {
  case G_CROWBAR:
    if (bFire && Due(tmNow, gs.tmNext)) {
      // 32 units from the eyes
      const BOOL bHit = (pw->*_pCutWithKnife)(0.0f, 0.0f, 32.0f * Unit() + 0.5f, 0.3f, 0.3f, DMG_CROWBAR);
      gs.tmNext = tmNow + (bHit ? 0.25f : 0.5f);
      Sound(pen, pw, bHit ? SND_KNIFE_HIGH : SND_KNIFE_LONG, FALSE);
      Anim(pw, "Attack1,Attack01,Attack");
      _ctShots++;
      _fLastDamage = bHit ? DMG_CROWBAR : 0.0f;
      Log(bHit ? "hit" : "swing", g, 0, -1);
    }
    break;
  case G_PYTHON:
    if (bFire && Due(tmNow, gs.tmNext) && gs.iClip > 0) {
      Bullet(pw, iSam, 8192.0f, DMG_357, CONE_1);
      gs.iClip--;
      gs.tmNext = tmNow + 0.75f;
      Sound(pen, pw, SND_COLT_FIRE, FALSE);
      Anim(pw, "Fire1,Fire");
      Flash(pw);
      Noise(pw, 50.0f);
      Log("fire", g, gs.iClip, iReserve);
    }
    break;
  case G_GLOCK: {
    // primary: aimed, 0.3 s; second: fast and loose, 0.2 s
    const BOOL bTry = bFire || bAlt;
    if (bTry && Due(tmNow, gs.tmNext) && gs.iClip > 0) {
      Bullet(pw, iSam, 8192.0f, DMG_9MM, bFire ? 0.01f : 0.1f);
      gs.iClip--;
      gs.tmNext = tmNow + (bFire ? 0.3f : 0.2f);
      Sound(pen, pw, SND_COLT_FIRE, FALSE);
      Anim(pw, "FireRight,Fire1,Fire");
      Flash(pw);
      Noise(pw, 40.0f);
      Log("fire", g, gs.iClip, iReserve);
    }
    } break;
  case G_SHOTGUN:
    if (bAlt && Due(tmNow, gs.tmNext) && gs.iClip >= 2) {
      Pellets(pw, iSam, 12, DMG_BUCKSHOT, CONE_10);
      gs.iClip -= 2;
      gs.tmNext = tmNow + 1.5f;
      Sound(pen, pw, SND_DOUBLESHOTGUN_FIRE, FALSE);
      Anim(pw, "Fire1,Fire,Fire1Fast");
      Flash(pw);
      Noise(pw, 60.0f);
      Log("double", g, gs.iClip, iReserve);
    } else if ((bFire || bAlt) && Due(tmNow, gs.tmNext) && gs.iClip >= 1) {
      Pellets(pw, iSam, 6, DMG_BUCKSHOT, CONE_10);
      gs.iClip -= 1;
      gs.tmNext = tmNow + 0.75f;
      Sound(pen, pw, SND_SHOTGUN_FIRE, FALSE);
      Anim(pw, "Fire1,Fire,Fire1Fast");
      Flash(pw);
      Noise(pw, 60.0f);
      Log("fire", g, gs.iClip, iReserve);
    }
    break;
  case G_MP5:
    if (bAlt && !_bOldAlt && Due(tmNow, gs.tmNext)) {
      // the M203: a grenade (Sam's), from the grenades
      INDEX *piGrenades = (INDEX *)player::Prop(pw, PW_GRENADES);
      if (piGrenades != NULL && *piGrenades > 0 && _pFireGrenade != NULL) {
        (pw->*_pFireGrenade)(1);
        (*piGrenades)--;
        gs.tmNext = tmNow + 1.0f;
        Sound(pen, pw, SND_GRENADE_FIRE, FALSE);
        _ctShots++;
        Log("grenade", g, gs.iClip, *piGrenades);
      }
    } else if (bFire && Due(tmNow, gs.tmNext) && gs.iClip > 0) {
      Bullet(pw, iSam, 8192.0f, DMG_MP5, CONE_3);
      gs.iClip--;
      gs.tmNext = tmNow + 0.1f;
      Sound(pen, pw, SND_TOMMYGUN_FIRE, FALSE);
      Anim(pw, "Fire");
      Flash(pw);
      Noise(pw, 50.0f);
      if ((gs.iClip % 10) == 0) Log("fire", g, gs.iClip, iReserve);
    }
    break;
  case G_RPG:
    if (bFire && Due(tmNow, gs.tmNext) && gs.iClip > 0 && _pFireRocket != NULL) {
      (pw->*_pFireRocket)();
      gs.iClip--;
      gs.tmNext = tmNow + 1.5f;
      Sound(pen, pw, SND_ROCKET_FIRE, FALSE);
      Anim(pw, "Fire");
      Noise(pw, 60.0f);
      _ctShots++;
      Log("fire", g, gs.iClip, iReserve);
    }
    break;
  case G_GRENADE:
    // the pin comes out on the press, it flies on the release
    if (bFire && !_bPinPulled && Due(tmNow, gs.tmNext) && iReserve > 0) {
      _bPinPulled = TRUE;
    } else if (!bFire && _bPinPulled && _pFireGrenade != NULL) {
      _bPinPulled = FALSE;
      (pw->*_pFireGrenade)(0);
      if (piReserve != NULL) (*piReserve)--;
      gs.tmNext = tmNow + 0.5f;
      Sound(pen, pw, SND_GRENADE_FIRE, FALSE);
      Anim(pw, "Fire");
      _ctShots++;
      Log("throw", g, 0, piReserve != NULL ? *piReserve : -1);
    }
    break;
  case G_GAUSS: {
    if (_bCharging) {
      const FLOAT tmCharged = tmNow - _tmChargeStart;
      // a cell every 0.3 s until full (4 s); held 10 s it discharges into you
      if (tmCharged < 4.0f && Due(tmNow, _tmChargeDrain) && piReserve != NULL && *piReserve > 0) {
        (*piReserve)--;
        _tmChargeDrain = tmNow + 0.3f;
      }
      if (tmCharged >= 10.0f) {
        _bCharging = FALSE;
        pen->InflictDirectDamage(pen, pen, DMT_IMPACT, 50.0f, pen->GetPlacement().pl_PositionVector, FLOAT3D(0, -1, 0));
        gs.tmNext = tmNow + 1.0f;
        Log("overcharge", g, 0, iReserve);
      } else if (!bAlt) {
        // the charged shot, and its push back - along the ground only, as in single player
        _bCharging = FALSE;
        const FLOAT fDamage = DMG_GAUSS_FULL * Min(tmCharged / 4.0f, 1.0f);
        Bullet(pw, iSam, 8192.0f, fDamage, 0.0f);
        CMovableEntity *pmen = (CMovableEntity *)pen;
        const FLOAT3D vUp = -pmen->en_vGravityDir;
        FLOAT3D vPush = Forward(pen) * (fDamage * 5.0f * Unit());
        vPush -= vUp * (vPush % vUp);
        pmen->en_vCurrentTranslationAbsolute -= vPush;
        gs.tmNext = tmNow + 1.0f;
        Sound(pen, pw, SND_LASER_FIRE, FALSE);
        Anim(pw, "Fire");
        Flash(pw);
        Noise(pw, 60.0f);
        CPrintF(SB_TAG " gauss charged %.2f s: %.0f damage, pushed back %.0f u/s\n", tmCharged, fDamage, vPush.Length() / Unit());
        _strLast = "charged";
      }
    } else if (bAlt && Due(tmNow, gs.tmNext) && iReserve > 0) {
      _bCharging = TRUE;
      _tmChargeStart = tmNow;
      _tmChargeDrain = tmNow;
    } else if (bFire && Due(tmNow, gs.tmNext) && iReserve >= 2) {
      Bullet(pw, iSam, 8192.0f, DMG_GAUSS, 0.0f);
      if (piReserve != NULL) *piReserve -= 2;
      gs.tmNext = tmNow + 0.2f;
      Sound(pen, pw, SND_LASER_FIRE, FALSE);
      Anim(pw, "Fire");
      Flash(pw);
      Noise(pw, 50.0f);
      Log("fire", g, 0, *piReserve);
    }
    } break;
  }
  _bOldFire = bFire;
  _bOldAlt = bAlt;
};

template<class Type> static BOOL Find(Type &pfn, const char *strSymbol) {
  StructPtr ptr(ClassicsCore_GetEntitiesSymbol(strSymbol));
  if (ptr.iAddress == NULL) return FALSE;
  pfn = ptr(Type());
  return TRUE;
};

BOOL Init(void) {
  _bReady = Find(_pFireMachineBullet, "?FireMachineBullet@CPlayerWeapons@@QAEXMMMMMM@Z")
         && Find(_pFireBullets, "?FireBullets@CPlayerWeapons@@QAEXMMMMJPAMMM@Z")
         && Find(_pCutWithKnife, "?CutWithKnife@CPlayerWeapons@@QAEHMMMMMM@Z");
  Find(_pFireRocket, "?FireRocket@CPlayerWeapons@@QAEXXZ");
  Find(_pFireGrenade, "?FireGrenade@CPlayerWeapons@@QAEXJ@Z");
  Find(_pDoRecoil, "?DoRecoil@CPlayerWeapons@@QAEXXZ");
  Find(_pSpawnRangeSound, "?SpawnRangeSound@CPlayerWeapons@@QAEXM@Z");
  Find(_pSetFlare, "?SetFlare@CPlayerWeapons@@QAEXJJ@Z");
  _symFX.Find("wpn_fFX");
  _symFY.Find("wpn_fFY");
  Reset();
  movement::SetActionFilter(&Filter);
  if (!_bReady) CPrintF(SB_TAG " Sam's weapon functions aren't in the Entities library: Sam's weapons stay\n");
  return _bReady;
};

void Reset(void) {
  for (INDEX i = 0; i < G_COUNT; i++) {
    _aGuns[i].iClip = _aiClip[i];
    _aGuns[i].tmNext = 0;
    _aGuns[i].tmReloaded = 0;
  }
  _iLastSam = -1;
  _bCharging = FALSE;
  _bPinPulled = FALSE;
  _bOldFire = _bOldAlt = FALSE;
};

void Draw(CDrawPort *pdp, CEntity *pen) {
  if (!_bReady || !hl_bWeapons || pen == NULL || pdp == NULL) return;
  CEntity *pw = player::PropEntity(pen, PROP_WEAPONS);
  if (pw == NULL) return;
  const Gun g = GunFor(player::PropIndex(pw, PW_CURRENT, 0));
  if (g == G_NONE) return;
  char str[64] = "";
  const GunState &gs = _aGuns[g];
  if (g == G_GAUSS && _bCharging) {
    _snprintf(str, sizeof(str), "%s %3.0f%%", _astrGun[g], Min((_pTimer->CurrentTick() - _tmChargeStart) / 4.0f, 1.0f) * 100.0f);
  } else if (_aiClip[g] > 0) {
    _snprintf(str, sizeof(str), "%s %d%s", _astrGun[g], gs.iClip, gs.tmReloaded > 0 ? " reloading" : "");
  } else {
    _snprintf(str, sizeof(str), "%s", _astrGun[g]);
  }
  // above Sam's ammo, bottom right
  const FLOAT fScale = pdp->GetHeight() / 720.0f;
  pdp->SetFont(_pfdConsoleFont);
  pdp->SetTextAspect(1.0f);
  pdp->SetTextScaling(fScale * 1.4f);
  const PIX pixX = pdp->GetWidth() / 2 + (PIX)(20 * fScale);
  const PIX pixY = pdp->GetHeight() - (PIX)(82 * fScale);
  pdp->PutText(str, pixX + 1, pixY + 1, 0x000000A0);
  pdp->PutText(str, pixX, pixY, 0xFFE070E0);
};

CTString Status(CEntity *pen) {
  CTString str;
  CEntity *pw = pen != NULL ? player::PropEntity(pen, PROP_WEAPONS) : NULL;
  const INDEX iSam = pw != NULL ? player::PropIndex(pw, PW_CURRENT, 0) : -1;
  const Gun g = GunFor(iSam);
  str.PrintF("weapons %s; in hand Sam's %d = %s, clip %d%s; %u shots (last %.0f damage, %s)", !_bReady ? "unavailable" : hl_bWeapons ? "Half-Life's" : "Sam's",
    iSam, _astrGun[g], _aGuns[g].iClip, _bCharging ? ", charging" : "", _ctShots, _fLastDamage, _strLast);
  return str;
};

void DumpAnims(CEntity *pen) {
  CEntity *pw = pen != NULL ? player::PropEntity(pen, PROP_WEAPONS) : NULL;
  CModelObject *pmo = pw != NULL ? (CModelObject *)player::Prop(pw, PW_MODEL) : NULL;
  if (pmo == NULL || pmo->GetData() == NULL) return;
  CTString strList;
  for (INDEX i = 0; i < pmo->GetAnimsCt(); i++) {
    CAnimInfo ai;
    pmo->GetAnimInfo(i, ai);
    strList += CTString(0, " %d:%s", i, ai.ai_AnimName);
  }
  CPrintF(SB_TAG " anims of Sam's weapon %d:%s\n", player::PropIndex(pw, PW_CURRENT, 0), strList.str_String);
};

}; // namespace
