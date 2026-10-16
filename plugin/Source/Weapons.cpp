/* SeriousHL - Half-Life's weapons on Sam's guns. See Weapons.h. */
#include "StdH.h"
#include "Weapons.h"
#include "Hev.h"
#include "HlMove.h"
#include "Movement.h"
#include "Player.h"
#include "Physics.h"
#include "SBridge.h"

#include <Engine/Templates/Stock_CSoundData.h>
#include <Extras/XGizmo/Objects/StructPtr.h>
#include <stdio.h>
#include <map>
#include <string>

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
static const ULONG PW_IRONBALLS = (402UL << 8) | 52;
static const ULONG PW_SNIPERBULLETS = (402UL << 8) | 54; // (TSE)
static const ULONG PW_SNIPING = (402UL << 8) | 236;      // m_bSniping (TSE): looking through the scope
// Projectile.es (class 501), the same in both
static const ULONG PJ_DAMAGE = (501UL << 8) | 14, PJ_RANGEDAMAGE = (501UL << 8) | 15;
static const ULONG PJ_HOTSPOT = (501UL << 8) | 16, PJ_FALLOFF = (501UL << 8) | 17;
// Sam's debris (Flags.h ECF_DEBRIS): only brushes stop it, nothing goes off on touching it
static const ULONG ECF_SATCHEL = ((1UL << 0) << ECB_TEST) | ((1UL << 5) << ECB_IS);
// sound components of PlayerWeapons.es (their numbers there)
enum { SND_KNIFE_HIGH = 24, SND_KNIFE_LONG = 25, SND_COLT_FIRE = 37, SND_COLT_RELOAD = 38, SND_SHOTGUN_FIRE = 46,
  SND_DOUBLESHOTGUN_FIRE = 60, SND_DOUBLESHOTGUN_RELOAD = 61, SND_TOMMYGUN_FIRE = 74, SND_ROCKET_FIRE = 97,
  SND_GRENADE_FIRE = 106, SND_LASER_FIRE = 146, SND_MINIGUN_ROTATE = 86, SND_MINIGUN_SPINUP = 87,
  SND_MINIGUN_SPINDOWN = 88, SND_SNIPER_FIRE = 113 /* TSE */ };
static const INDEX FLARE_ADD = 2;
// Player.es buttons
static const ULONG PLACT_FIRE = 1UL << 0, PLACT_RELOAD = 1UL << 1, PLACT_USE = 1UL << 5, PLACT_COMPUTER = 1UL << 6;
// Sam's weapons (WeaponType)
#if SE1_GAME == SS_TFE
enum { W_KNIFE = 1, W_COLT = 2, W_DOUBLECOLT = 3, W_SINGLESHOTGUN = 4, W_DOUBLESHOTGUN = 5, W_TOMMYGUN = 6,
  W_MINIGUN = 7, W_ROCKETLAUNCHER = 8, W_GRENADELAUNCHER = 9, W_LASER = 14, W_IRONCANNON = 16, W_CHAINSAW = -100,
  W_SNIPER = -101 };
#else
enum { W_KNIFE = 1, W_COLT = 2, W_DOUBLECOLT = 3, W_SINGLESHOTGUN = 4, W_DOUBLESHOTGUN = 5, W_TOMMYGUN = 6,
  W_MINIGUN = 7, W_ROCKETLAUNCHER = 8, W_GRENADELAUNCHER = 9, W_CHAINSAW = 10, W_LASER = 12, W_SNIPER = 13,
  W_IRONCANNON = 14 };
#endif

// --- Sam's fire functions (CPlayerWeapons, Entities library)
typedef void (CEntity::*FFireMachineBullet)(FLOAT fX, FLOAT fY, FLOAT fRange, FLOAT fDamage, FLOAT fJitter, FLOAT fBulletSize);
typedef void (CEntity::*FFireBullets)(FLOAT fX, FLOAT fY, FLOAT fRange, FLOAT fDamage, INDEX iBullets, FLOAT *afPositions, FLOAT fStretch, FLOAT fJitter);
typedef BOOL (CEntity::*FCutWithKnife)(FLOAT fX, FLOAT fY, FLOAT fRange, FLOAT fWide, FLOAT fThickness, FLOAT fDamage);
typedef void (CEntity::*FVoid)(void);
typedef void (CEntity::*FIndex)(INDEX i);
typedef void (CEntity::*FFloat)(FLOAT f);
typedef void (CEntity::*FFlare)(INDEX iFlare, INDEX iState);
typedef void (CEntity::*FFireSniperBullet)(FLOAT fX, FLOAT fY, FLOAT fRange, FLOAT fDamage, FLOAT fImprecision);
static FFireMachineBullet _pFireMachineBullet = NULL;
static FFireBullets _pFireBullets = NULL;
static FCutWithKnife _pCutWithKnife = NULL;
static FVoid _pFireRocket = NULL, _pDoRecoil = NULL;
static FIndex _pFireGrenade = NULL;
static FFloat _pSpawnRangeSound = NULL;
static FFlare _pSetFlare = NULL;
static FFireSniperBullet _pFireSniperBullet = NULL;
static BOOL _bReady = FALSE;
static CSymbolPtr _symFX, _symFY;

// --- Half-Life's guns
enum Gun { G_NONE, G_CROWBAR, G_PYTHON, G_GLOCK, G_SHOTGUN, G_MP5, G_RPG, G_GRENADE, G_GAUSS, G_EGON, G_SATCHEL,
  G_CROSSBOW, G_COUNT };
static const char *_astrGun[G_COUNT] = { "-", "crowbar", ".357", "glock", "shotgun", "mp5", "rpg", "grenade", "gauss",
  "egon", "satchel", "crossbow" };
static const INDEX _aiClip[G_COUNT] = { 0, 0, 6, 17, 8, 50, 1, 0, 0, 0, 0, 5 };
static const FLOAT _afReload[G_COUNT] = { 0, 0, 2.0f, 1.5f, 0.5f, 1.5f, 2.0f, 0, 0, 0, 0, 4.5f }; // (shotgun: a shell)

// skill.cfg sk_plr_*, in Half-Life's units; the fire functions take metres
static const FLOAT DMG_CROWBAR = 10.0f, DMG_357 = 40.0f, DMG_9MM = 8.0f, DMG_BUCKSHOT = 5.0f, DMG_MP5 = 5.0f;
static const FLOAT DMG_GAUSS = 20.0f, DMG_GAUSS_FULL = 200.0f;
static const FLOAT DMG_EGON = 14.0f;      // sk_plr_egon_wide: single player's beam, every 0.1 s
static const FLOAT DMG_SATCHEL = 150.0f;  // over 2.5 x 150 units
static const FLOAT DMG_XBOW = 50.0f;      // sk_plr_xbow_bolt_monster
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
static BOOL _bEgonOn = FALSE;     // egon
static TIME _tmEgonPulse = 0, _tmEgonRun = 0;
static ULONG _aulSatchels[16];    // satchels out (Sam's grenades, their ids)
static INDEX _ctSatchels = 0;
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
    case W_MINIGUN: return G_EGON;
    case W_IRONCANNON: return G_SATCHEL;
    case W_SNIPER: return G_CROSSBOW;
  }
  return G_NONE;
};

// the reserve a gun reloads from: Sam's ammo (NULL: endless, or the gun has no magazine)
static INDEX *Reserve(CEntity *pw, Gun g) {
  switch (g) {
    case G_GLOCK: case G_MP5: case G_EGON: return (INDEX *)player::Prop(pw, PW_BULLETS);
    case G_SATCHEL: return (INDEX *)player::Prop(pw, PW_IRONBALLS);
    case G_CROSSBOW: return (INDEX *)player::Prop(pw, PW_SNIPERBULLETS);
    case G_SHOTGUN: return (INDEX *)player::Prop(pw, PW_SHELLS);
    case G_RPG: return (INDEX *)player::Prop(pw, PW_ROCKETS);
    case G_GRENADE: return (INDEX *)player::Prop(pw, PW_GRENADES);
    case G_GAUSS: return (INDEX *)player::Prop(pw, PW_ELECTRICITY);
  }
  return NULL;
};

static FLOAT FireX(INDEX iSam) { return _symFX.Exists() ? ((FLOAT *)_symFX.GetValue())[iSam] : 0.0f; };
static FLOAT FireY(INDEX iSam) { return _symFY.Exists() ? ((FLOAT *)_symFY.GetValue())[iSam] : 0.0f; };

// Half-Life's own sound when the installer took it from the player's Half-Life (Sounds\SeriousHL\<"weapons\x.wav">;
// a list "a.wav|b.wav" picks one at random, as Half-Life does), else Sam's component. The sound's data is kept here:
// CEntity::PlaySound(file) lets go of it right after starting it, and the stock frees it - a sound that never comes out.
static std::map<std::string, CSoundData *> _mapSounds;
static ULONG _ctHLSounds = 0, _ctSamSounds = 0;

static CSoundData *SoundData(const CTFileName &fnm) {
  std::map<std::string, CSoundData *>::iterator it = _mapSounds.find(fnm.str_String);
  if (it != _mapSounds.end()) return it->second;
  CSoundData *psd = NULL;
  if (FileExists(fnm)) {
    try {
      psd = _pSoundStock->Obtain_t(fnm);
    } catch (char *strError) {
      CPrintF(SB_TAG " %s\n", strError);
    }
  }
  _mapSounds[fnm.str_String] = psd;
  return psd;
};

void FreeSounds(void) {
  for (std::map<std::string, CSoundData *>::iterator it = _mapSounds.begin(); it != _mapSounds.end(); it++) {
    if (it->second != NULL) _pSoundStock->Release(it->second);
  }
  _mapSounds.clear();
};

static void Sound(CEntity *pen, CEntity *pw, const char *strHL, INDEX iSound, BOOL bSecond, ULONG ulFlags = 0) {
  CSoundObject *pso = (CSoundObject *)player::Prop(pen, bSecond ? PROP_SOUND1 : PROP_SOUND0);
  if (pso == NULL) return;
  // Sam's guns set these before each of their sounds - and leave them muted ("mute fire": the tommygun, the minigun)
  // or low (the laser's pitch): the object's own range, volume and pitch every time (Sam's fire: 25 m, 1.5)
  pso->Set3DParameters(25.0f, 1.0f, bSecond ? 1.0f : 1.5f, 1.0f);
  if (strHL != NULL) {
    // one of the list
    char strList[160];
    strncpy(strList, strHL, sizeof(strList) - 1);
    strList[sizeof(strList) - 1] = 0;
    const char *astr[8];
    INDEX ct = 0;
    for (char *str = strtok(strList, "|"); str != NULL && ct < 8; str = strtok(NULL, "|")) astr[ct++] = str;
    if (ct > 0) {
      const CTFileName fnm = CTString("Sounds\\SeriousHL\\") + astr[pw->IRnd() % ct];
      CSoundData *psd = SoundData(fnm);
      if (psd != NULL) {
        pso->Play(psd, SOF_3D | SOF_VOLUMETRIC | ulFlags);
        _ctHLSounds++;
        return;
      }
    }
  }
  // (a component's id is its class's with its number: (402 << 8) | n)
  if (iSound >= 0) {
    pw->PlaySound(*pso, (402L << 8) | iSound, SOF_3D | SOF_VOLUMETRIC | ulFlags);
    _ctSamSounds++;
  }
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

// --- the egon's beam off (its loop stopped)
static void EgonOff(CEntity *pen, CEntity *pw) {
  if (!_bEgonOn) return;
  _bEgonOn = FALSE;
  Sound(pen, pw, "weapons\\egon_off1.wav", SND_MINIGUN_SPINDOWN, TRUE);
  Log("off", G_EGON, 0, -1);
};

// --- satchels: Sam's grenades with no fuse, set off by the player
static CEntity *Satchel(ULONG ulID) {
  CEntity *penS = physics::FindEntity(ulID);
  return penS != NULL && strcmp(penS->GetClass()->ec_pdecDLLClass->dec_strName, "Projectile") == 0 ? penS : NULL;
};

static INDEX Satchels(void) {
  INDEX ct = 0;
  for (INDEX i = 0; i < _ctSatchels; i++) {
    if (Satchel(_aulSatchels[i]) != NULL) _aulSatchels[ct++] = _aulSatchels[i];
  }
  return _ctSatchels = ct;
};

static BOOL ThrowSatchel(CEntity *pen, CEntity *pw) {
  CWorld *pwo = IWorld::GetWorld();
  if (_pFireGrenade == NULL || pwo == NULL || _ctSatchels >= (INDEX)ARRAYCOUNT(_aulSatchels)) return FALSE;
  const ULONG ulFirst = pwo->wo_ulNextEntityID;
  (pw->*_pFireGrenade)(0);
  CEntity *penS = NULL;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    if (iten->en_ulID >= ulFirst && strcmp(iten->GetClass()->ec_pdecDLLClass->dec_strName, "Projectile") == 0) {
      penS = &*iten;
      break;
    }
  }
  if (penS == NULL) return FALSE;
  // thrown at 274 u/s plus the player's own speed; it goes off on nothing it meets, comes to rest and waits
  CMovableEntity *pmS = (CMovableEntity *)penS;
  pmS->en_vCurrentTranslationAbsolute = Forward(pen) * (274.0f * Unit()) + ((CMovableEntity *)pen)->en_vCurrentTranslationAbsolute;
  pmS->en_vDesiredTranslationRelative = FLOAT3D(0, 0, 0);
  penS->SetCollisionFlags(ECF_SATCHEL);
  // RadiusDamage(150, 2.5 x 150 units): from the middle down to nothing at the edge
  FLOAT *pf;
  if ((pf = (FLOAT *)player::Prop(penS, PJ_DAMAGE)) != NULL) *pf = 0.0f;
  if ((pf = (FLOAT *)player::Prop(penS, PJ_RANGEDAMAGE)) != NULL) *pf = DMG_SATCHEL;
  if ((pf = (FLOAT *)player::Prop(penS, PJ_HOTSPOT)) != NULL) *pf = 0.1f;
  if ((pf = (FLOAT *)player::Prop(penS, PJ_FALLOFF)) != NULL) *pf = 2.5f * DMG_SATCHEL * Unit();
  // (its fly time is the timer of the state it waits in)
  ((CRationalEntity *)penS)->SetTimerAfter(3600.0f);
  _aulSatchels[_ctSatchels++] = penS->en_ulID;
  return TRUE;
};

static void Detonate(void) {
  for (INDEX i = 0; i < _ctSatchels; i++) {
    CEntity *penS = Satchel(_aulSatchels[i]);
    if (penS != NULL) ((CRationalEntity *)penS)->SetTimerAfter(0.05f);
  }
  _ctSatchels = 0;
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
  if (g == G_SHOTGUN) Sound(pen, pw, "weapons\\reload1.wav|weapons\\reload3.wav", SND_DOUBLESHOTGUN_RELOAD, TRUE);
  else if (g == G_PYTHON) Sound(pen, pw, "weapons\\357_reload1.wav", SND_COLT_RELOAD, TRUE);
  else if (g == G_GLOCK) Sound(pen, pw, "items\\9mmclip1.wav", SND_COLT_RELOAD, TRUE);
  else if (g == G_MP5) Sound(pen, pw, "items\\clipinsert1.wav", -1, TRUE);
  else if (g == G_CROSSBOW) Sound(pen, pw, "weapons\\xbow_reload1.wav", -1, TRUE);
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
  if (!_bReady) return;
  CEntity *pw = player::PropEntity(pen, PROP_WEAPONS);
  if (pw == NULL) return;
  if (!hl_bWeapons || !(pen->GetFlags() & ENF_ALIVE)) {
    EgonOff(pen, pw);
    return;
  }
  const INDEX iSam = player::PropIndex(pw, PW_CURRENT, 0);
  const INDEX iWanted = player::PropIndex(pw, PW_WANTED, 0);
  const Gun g = GunFor(iSam);
  const BOOL bAlt = AltDown();
  if (hl_iTestAlt > 0) hl_iTestAlt--;
  if (g == G_NONE) {
    _iLastSam = iSam;
    _bCharging = FALSE;
    EgonOff(pen, pw);
    return;
  }
  // Sam's own firing and reloading are out; the right mouse button doesn't open Netricsa
  const BOOL bFire = (pa.pa_ulButtons & PLACT_FIRE) != 0;
  const BOOL bReloadKey = (pa.pa_ulButtons & PLACT_RELOAD) != 0;
  pa.pa_ulButtons &= ~(PLACT_FIRE | PLACT_RELOAD);
  // (the crossbow's second fire is Sam's scope: the button stays)
  if (bAlt && g != G_CROSSBOW) pa.pa_ulButtons &= ~(PLACT_USE | PLACT_COMPUTER);

  const TIME tmNow = _pTimer->CurrentTick();
  GunState &gs = _aGuns[g];
  // switching: the new gun is drawn first
  if (iSam != iWanted) {
    _bCharging = FALSE;
    _bPinPulled = FALSE;
    EgonOff(pen, pw);
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
      Sound(pen, pw, bHit ? "weapons\\cbar_hitbod1.wav|weapons\\cbar_hitbod2.wav|weapons\\cbar_hitbod3.wav" : "weapons\\cbar_miss1.wav",
        bHit ? SND_KNIFE_HIGH : SND_KNIFE_LONG, FALSE);
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
      Sound(pen, pw, "weapons\\357_shot1.wav|weapons\\357_shot2.wav", SND_COLT_FIRE, FALSE);
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
      Sound(pen, pw, "weapons\\pl_gun3.wav", SND_COLT_FIRE, FALSE);
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
      Sound(pen, pw, "weapons\\dbarrel1.wav", SND_DOUBLESHOTGUN_FIRE, FALSE);
      Anim(pw, "Fire1,Fire,Fire1Fast");
      Flash(pw);
      Noise(pw, 60.0f);
      Log("double", g, gs.iClip, iReserve);
    } else if ((bFire || bAlt) && Due(tmNow, gs.tmNext) && gs.iClip >= 1) {
      Pellets(pw, iSam, 6, DMG_BUCKSHOT, CONE_10);
      gs.iClip -= 1;
      gs.tmNext = tmNow + 0.75f;
      Sound(pen, pw, "weapons\\sbarrel1.wav", SND_SHOTGUN_FIRE, FALSE);
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
        Sound(pen, pw, "weapons\\glauncher.wav|weapons\\glauncher2.wav", SND_GRENADE_FIRE, FALSE);
        _ctShots++;
        Log("grenade", g, gs.iClip, *piGrenades);
      }
    } else if (bFire && Due(tmNow, gs.tmNext) && gs.iClip > 0) {
      Bullet(pw, iSam, 8192.0f, DMG_MP5, CONE_3);
      gs.iClip--;
      gs.tmNext = tmNow + 0.1f;
      Sound(pen, pw, "weapons\\hks1.wav|weapons\\hks2.wav|weapons\\hks3.wav", SND_TOMMYGUN_FIRE, FALSE);
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
      Sound(pen, pw, "weapons\\rocketfire1.wav", SND_ROCKET_FIRE, FALSE);
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
      Sound(pen, pw, NULL, SND_GRENADE_FIRE, FALSE);
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
        hev::SelfDamage(pen, 50.0f);
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
        Sound(pen, pw, "weapons\\gauss2.wav", SND_LASER_FIRE, FALSE);
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
      Sound(pen, pw, "weapons\\gauss2.wav", SND_LASER_FIRE, FALSE);
      Anim(pw, "Fire");
      Flash(pw);
      Noise(pw, 50.0f);
      Log("fire", g, 0, *piReserve);
    }
    } break;
  case G_EGON:
    // single player's wide beam: 14 damage and a cell every 0.1 s (the first after 0.1 s), the run loop after 2 s
    if (bFire && iReserve > 0) {
      if (!_bEgonOn) {
        _bEgonOn = TRUE;
        _tmEgonPulse = tmNow + 0.1f;
        _tmEgonRun = tmNow + 2.0f;
        Sound(pen, pw, "weapons\\egon_windup2.wav", SND_MINIGUN_SPINUP, TRUE);
        Log("on", g, 0, iReserve);
      }
      if (Due(tmNow, _tmEgonPulse)) {
        Bullet(pw, iSam, 2048.0f, DMG_EGON, 0.0f);
        if (piReserve != NULL) (*piReserve)--;
        _tmEgonPulse = tmNow + 0.1f;
        Noise(pw, 50.0f);
      }
      if (_tmEgonRun > 0 && Due(tmNow, _tmEgonRun)) {
        Sound(pen, pw, "weapons\\egon_run3.wav", SND_MINIGUN_ROTATE, TRUE, SOF_LOOP);
        _tmEgonRun = 0;
      }
    } else {
      EgonOff(pen, pw);
    }
    break;
  case G_SATCHEL: {
    // the first fire throws one, or sets off those out; the second throws another
    const INDEX ctOut = Satchels();
    if (bFire && ctOut > 0 && Due(tmNow, gs.tmNext)) {
      Detonate();
      gs.tmNext = tmNow + 0.5f;
      _ctShots++;
      _fLastDamage = DMG_SATCHEL;
      Log("detonate", g, ctOut, iReserve);
    } else if ((bFire || bAlt) && Due(tmNow, gs.tmNext) && iReserve > 0 && ThrowSatchel(pen, pw)) {
      if (piReserve != NULL) (*piReserve)--;
      gs.tmNext = tmNow + 1.0f;
      Anim(pw, "Fire");
      Log("throw", g, _ctSatchels, piReserve != NULL ? *piReserve : -1);
    }
    } break;
  case G_CROSSBOW:
    // a bolt (Sam's sniper bullet), 0.75 s apart, 5 in the clip; through the scope from the middle of the view
    if (bFire && Due(tmNow, gs.tmNext) && gs.iClip > 0) {
      const BOOL bScope = player::PropIndex(pw, PW_SNIPING, 0) != 0;
      if (_pFireSniperBullet != NULL) {
        (pw->*_pFireSniperBullet)(bScope ? 0.0f : FireX(iSam), bScope ? 0.0f : FireY(iSam), 8192.0f * Unit(), DMG_XBOW, 0.0f);
        _ctShots++;
        _fLastDamage = DMG_XBOW;
      } else {
        Bullet(pw, iSam, 8192.0f, DMG_XBOW, 0.0f);
      }
      gs.iClip--;
      gs.tmNext = tmNow + 0.75f;
      Sound(pen, pw, "weapons\\xbow_fire1.wav", SND_SNIPER_FIRE, FALSE);
      Anim(pw, "Fire");
      Noise(pw, 30.0f);
      Log("fire", g, gs.iClip, iReserve);
    }
    break;
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
  Find(_pFireSniperBullet, "?FireSniperBullet@CPlayerWeapons@@QAEXMMMMM@Z"); // (TSE)
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
  _bEgonOn = FALSE;
  _ctSatchels = 0;
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
  } else if (g == G_SATCHEL && _ctSatchels > 0) {
    _snprintf(str, sizeof(str), "%s %d out", _astrGun[g], _ctSatchels);
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
  const INDEX *piReserve = pw != NULL ? Reserve(pw, g) : NULL;
  str.PrintF("weapons %s; in hand Sam's %d = %s, clip %d, reserve %d%s%s; %d satchels out; %u shots (last %.0f damage, %s); sounds: %u Half-Life's, %u Sam's",
    !_bReady ? "unavailable" : hl_bWeapons ? "Half-Life's" : "Sam's",
    iSam, _astrGun[g], _aGuns[g].iClip, piReserve != NULL ? *piReserve : -1, _bCharging ? ", charging" : "",
    _bEgonOn ? ", beam on" : "", _ctSatchels, _ctShots,
    _fLastDamage, _strLast, _ctHLSounds, _ctSamSounds);
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
