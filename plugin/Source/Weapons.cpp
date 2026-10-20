/* SeriousHL - Half-Life's weapons on Sam's guns. See Weapons.h. */
#include "StdH.h"
#include "Weapons.h"
#include "Hev.h"
#include "ViewModel.h"
#include "Sounds.h"
#include "Grenades.h"
#include "Tripmine.h"
#include "HlMove.h"
#include "Movement.h"
#include "Player.h"
#include "Physics.h"
#include "SBridge.h"

#include <Extras/XGizmo/Objects/StructPtr.h>
#include <stdio.h>
#include <map>
#include <string>

namespace weapons {

INDEX hl_bWeapons = TRUE;
INDEX hl_iAltFireKey = VK_RBUTTON;
INDEX hl_bSlots = TRUE;
INDEX hl_bFastSwitch = FALSE;

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
static const ULONG PLACT_WEAPON_NEXT = 1UL << 2, PLACT_WEAPON_PREV = 1UL << 3, PLACT_WEAPON_FLIP = 1UL << 4;
#if SE1_GAME == SS_TFE
static const INDEX PLACT_SELECT_SHIFT = 9;
#else
static const INDEX PLACT_SELECT_SHIFT = 14;
#endif
static const ULONG PW_AVAILABLE = (402UL << 8) | 11; // m_iAvailableWeapons: a bit a weapon (1 << (type - 1))
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
  G_CROSSBOW, G_TRIPMINE, G_COUNT };
static const char *_astrGun[G_COUNT] = { "-", "crowbar", ".357", "glock", "shotgun", "mp5", "rpg", "grenade", "gauss",
  "egon", "satchel", "crossbow", "tripmine" };
static const INDEX _aiClip[G_COUNT] = { 0, 0, 6, 17, 8, 50, 1, 0, 0, 0, 0, 5, 0 };
static const FLOAT _afReload[G_COUNT] = { 0, 0, 2.0f, 1.5f, 0.5f, 1.5f, 2.0f, 0, 0, 0, 0, 4.5f, 0 }; // (shotgun: a shell)
// the cannon carries two of Half-Life's: the satchels and the tripmines, from its balls - the one taken last
static Gun _gCannon = G_SATCHEL;

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
static Gun _gLastGun = G_NONE;
static BOOL _bOldFire = FALSE, _bOldAlt = FALSE;
static BOOL _bCharging = FALSE;   // gauss
static TIME _tmChargeStart = 0, _tmChargeDrain = 0;
static BOOL _bPinPulled = FALSE;  // hand grenade
static TIME _tmPinPulled = 0, _tmRedraw = 0;
static BOOL _bEgonOn = FALSE;     // egon
static TIME _tmEgonPulse = 0, _tmEgonRun = 0;

// --- Half-Life's view models (ViewModel.h): the file, and the SDK's weapon animation numbers (draw, idle, holster)
struct ViewGun { const char *strModel; INDEX iDraw, iIdle, iHolster; };
static const ViewGun _aView[G_COUNT] = {
  { NULL, 0, 0, -1 },
  { "v_crowbar.mdl", 1, 0, 2 },     // CROWBAR_DRAW, _IDLE, _HOLSTER
  { "v_357.mdl", 5, 0, 4 },         // PYTHON_DRAW, _IDLE1, _HOLSTER
  { "v_9mmhandgun.mdl", 7, 0, 8 },  // GLOCK_DRAW, _IDLE1, _HOLSTER
  { "v_shotgun.mdl", 6, 0, 7 },     // SHOTGUN_DRAW, _IDLE, _HOLSTER
  { "v_9mmar.mdl", 4, 1, -1 },      // MP5_DEPLOY, _IDLE1
  { "v_rpg.mdl", 5, 0, 4 },         // RPG_DRAW1, _IDLE, _HOLSTER1
  { "v_grenade.mdl", 7, 0, 6 },     // HANDGRENADE_DRAW, _IDLE, _HOLSTER
  { "v_gauss.mdl", 8, 0, 7 },       // GAUSS_DRAW, _IDLE, _HOLSTER
  { "v_egon.mdl", 9, 0, 10 },       // EGON_DRAW, _IDLE1, _HOLSTER
  { "v_satchel.mdl", 2, 0, -1 },    // SATCHEL_DRAW, _IDLE1 (the radio's once some are out)
  { "v_crossbow.mdl", 8, 0, 10 },   // CROSSBOW_DRAW1, _IDLE1, _HOLSTER1
  { "v_tripmine.mdl", 6, 0, 5 },    // TRIPMINE_DRAW, _IDLE1, _HOLSTER
};
static Gun _gView = G_NONE;
static BOOL _bHolstered = FALSE, _bRadio = FALSE;
static INDEX _iSwing = 0;

static void ViewTake(Gun g) {
  if (g == _gView && !_bHolstered) return;
  _gView = g;
  _bHolstered = FALSE;
  _bRadio = FALSE;
  viewmodel::SetModel(NULL, 0, 0);
  if (g != G_NONE && _aView[g].strModel != NULL) viewmodel::SetModel(_aView[g].strModel, _aView[g].iDraw, _aView[g].iIdle);
};

static void ViewHolster(void) {
  if (_bHolstered || _gView == G_NONE) return;
  _bHolstered = TRUE;
  const INDEX iHolster = _aView[_gView].iHolster;
  if (iHolster >= 0) {
    viewmodel::SetIdle(iHolster);
    viewmodel::Play(iHolster);
  }
};

static void View(INDEX iSeq) { viewmodel::Play(iSeq); };

// the view model's own sound events (the Glock's, the .357's, the MP5's reloads): on the second channel
static void Sound(CEntity *pen, CEntity *pw, const char *strHL, INDEX iSound, BOOL bSecond, ULONG ulFlags);
static void ModelSound(const char *strSound) {
  CEntity *pen = player::Local();
  CEntity *pw = pen != NULL ? player::PropEntity(pen, PROP_WEAPONS) : NULL;
  if (pw != NULL) Sound(pen, pw, strSound, -1, TRUE, 0);
};
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
    case W_IRONCANNON: return _gCannon;
    case W_SNIPER: return G_CROSSBOW;
  }
  return G_NONE;
};

// the reserve a gun reloads from: Sam's ammo (NULL: endless, or the gun has no magazine)
static INDEX *Reserve(CEntity *pw, Gun g) {
  switch (g) {
    case G_GLOCK: case G_MP5: case G_EGON: return (INDEX *)player::Prop(pw, PW_BULLETS);
    case G_SATCHEL: case G_TRIPMINE: return (INDEX *)player::Prop(pw, PW_IRONBALLS);
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

// Half-Life's own sound when the installer took it from the player's Half-Life (Sounds.h; a list "a.wav|b.wav" picks one
// at random, as Half-Life does), else Sam's component.
static ULONG _ctHLSounds = 0, _ctSamSounds = 0;

void FreeSounds(void) { sounds::FreeAll(); };

static void Sound(CEntity *pen, CEntity *pw, const char *strHL, INDEX iSound, BOOL bSecond, ULONG ulFlags = 0) {
  CSoundObject *pso = (CSoundObject *)player::Prop(pen, bSecond ? PROP_SOUND1 : PROP_SOUND0);
  if (pso == NULL) return;
  // Sam's guns set these before each of their sounds - and leave them muted ("mute fire": the tommygun, the minigun)
  // or low (the laser's pitch): the object's own range, volume and pitch every time (Sam's fire: 25 m, 1.5)
  pso->Set3DParameters(25.0f, 1.0f, bSecond ? 1.0f : 1.5f, 1.0f);
  CSoundData *psd = sounds::Pick(strHL, pw->IRnd());
  if (psd != NULL) {
    pso->Play(psd, SOF_3D | SOF_VOLUMETRIC | ulFlags);
    _ctHLSounds++;
    return;
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
  if (_gView == G_EGON) {
    viewmodel::SetIdle(0);
    View(4); // EGON_ALTFIREOFF
  }
  Log("off", G_EGON, 0, -1);
};

// --- Sam's grenade made (CPlayerWeapons::FireGrenade) and handed to Grenades.h as Half-Life's: placed where Half-Life's
// starts, its velocity and fuse
static CEntity *NewGrenade(CEntity *pw, INDEX iPower, const FLOAT3D &vFrom) {
  CWorld *pwo = IWorld::GetWorld();
  if (_pFireGrenade == NULL || pwo == NULL) return NULL;
  const ULONG ulFirst = pwo->wo_ulNextEntityID;
  (pw->*_pFireGrenade)(iPower);
  CEntity *penG = NULL;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    if (iten->en_ulID >= ulFirst && strcmp(iten->GetClass()->ec_pdecDLLClass->dec_strName, "Projectile") == 0) {
      penG = &*iten;
      break;
    }
  }
  if (penG != NULL) penG->SetPlacement(CPlacement3D(vFrom, penG->GetPlacement().pl_OrientationAngle));
  return penG;
};

static FLOAT3D Eyes(CEntity *pen) { return IWorld::GetViewpoint((CPlayerEntity *)pen, FALSE).pl_PositionVector; };

static const char *_astrHudFile[G_COUNT] = { NULL, "weapon_crowbar", "weapon_357", "weapon_9mmhandgun", "weapon_shotgun",
  "weapon_9mmar", "weapon_rpg", "weapon_handgrenade", "weapon_gauss", "weapon_egon", "weapon_satchel", "weapon_crossbow",
  "weapon_tripmine" };

// --- Half-Life's weapon menu (hud ammo.cpp: buckets 1-5, positions in them), on Sam's guns: the slot keys open it,
// press again for the next in the slot, fire takes the one picked (or at once with hl_bFastSwitch); the wheel goes
// through all of them, the flip key back to the last one. Sam is then asked for the gun's host by his own slot keys,
// a key again a tick later if he took the other of the pair. Keys 6-9 stay Sam's.
struct HLSlot { Gun g; INDEX iSlot, iPos; };
static const HLSlot _aSlots[] = {
  { G_CROWBAR, 0, 0 }, { G_GLOCK, 1, 0 }, { G_PYTHON, 1, 1 }, { G_MP5, 2, 0 }, { G_SHOTGUN, 2, 1 }, { G_CROSSBOW, 2, 2 },
  { G_RPG, 3, 0 }, { G_GAUSS, 3, 1 }, { G_EGON, 3, 2 }, { G_GRENADE, 4, 0 }, { G_SATCHEL, 4, 1 }, { G_TRIPMINE, 4, 2 },
};
static const INDEX CT_SLOTS = sizeof(_aSlots) / sizeof(_aSlots[0]);
// the hosts (the first he has), and Sam's slot key for each of Sam's weapons
static INDEX Host(CEntity *pw, Gun g) {
  INDEX aiHosts[2] = { -1, -1 };
  switch (g) {
    case G_CROWBAR: aiHosts[0] = W_KNIFE; aiHosts[1] = W_CHAINSAW; break;
    case G_PYTHON: aiHosts[0] = W_COLT; break;
    case G_GLOCK: aiHosts[0] = W_DOUBLECOLT; break;
    case G_SHOTGUN: aiHosts[0] = W_DOUBLESHOTGUN; aiHosts[1] = W_SINGLESHOTGUN; break;
    case G_MP5: aiHosts[0] = W_TOMMYGUN; break;
    case G_RPG: aiHosts[0] = W_ROCKETLAUNCHER; break;
    case G_GRENADE: aiHosts[0] = W_GRENADELAUNCHER; break;
    case G_GAUSS: aiHosts[0] = W_LASER; break;
    case G_EGON: aiHosts[0] = W_MINIGUN; break;
    case G_SATCHEL: case G_TRIPMINE: aiHosts[0] = W_IRONCANNON; break;
    case G_CROSSBOW: aiHosts[0] = W_SNIPER; break;
  }
  const INDEX iHave = player::PropIndex(pw, PW_AVAILABLE, 0);
  for (INDEX i = 0; i < 2; i++) {
    if (aiHosts[i] > 0 && (iHave & (1 << (aiHosts[i] - 1)))) return aiHosts[i];
  }
  return -1;
};

static INDEX SamKey(INDEX iSam) {
#if SE1_GAME == SS_TFE
  switch (iSam) {
    case W_KNIFE: return 1; case W_COLT: case W_DOUBLECOLT: return 2; case W_SINGLESHOTGUN: case W_DOUBLESHOTGUN: return 3;
    case W_TOMMYGUN: case W_MINIGUN: return 4; case W_ROCKETLAUNCHER: return 5; case W_GRENADELAUNCHER: return 6;
    case W_LASER: return 8; case W_IRONCANNON: return 9;
  }
#else
  switch (iSam) {
    case W_KNIFE: case W_CHAINSAW: return 1; case W_COLT: case W_DOUBLECOLT: return 2;
    case W_SINGLESHOTGUN: case W_DOUBLESHOTGUN: return 3; case W_TOMMYGUN: case W_MINIGUN: return 4;
    case W_ROCKETLAUNCHER: case W_GRENADELAUNCHER: return 5; case W_SNIPER: return 6; case W_LASER: return 7;
    case W_IRONCANNON: return 8;
  }
#endif
  return 0;
};

static BOOL HasAmmo(CEntity *pw, Gun g) {
  const INDEX *piReserve = Reserve(pw, g);
  return piReserve == NULL || *piReserve > 0 || (_aiClip[g] > 0 && _aGuns[g].iClip > 0);
};

static BOOL _bMenu = FALSE;
static INDEX _iMenu = -1;      // the menu's pick: an index into _aSlots
static TIME _tmMenu = 0;
static INDEX _iOldKey = 0;
static ULONG _ulOldMenuButtons = 0;
static INDEX _iTargetSam = -1, _ctTries = 0;
static BOOL _bAskedLastTick = FALSE;
static Gun _gLast = G_NONE, _gHolding = G_NONE;
static ULONG _ctPicks = 0;
static BOOL _bHoldFire = FALSE;

static void MenuSound(CEntity *pen, CEntity *pw, const char *strSound) {
  Sound(pen, pw, strSound, -1, TRUE);
};

// the next one he has (from index i, in that direction; only in one slot if iSlot >= 0)
static INDEX NextOwned(CEntity *pw, INDEX i, INDEX iDir, INDEX iSlot) {
  for (INDEX n = 1; n <= CT_SLOTS; n++) {
    const INDEX j = ((i + iDir * n) % CT_SLOTS + CT_SLOTS) % CT_SLOTS;
    if (iSlot >= 0 && _aSlots[j].iSlot != iSlot) continue;
    if (Host(pw, _aSlots[j].g) > 0) return j;
  }
  return -1;
};

static INDEX IndexOf(Gun g) {
  for (INDEX i = 0; i < CT_SLOTS; i++) if (_aSlots[i].g == g) return i;
  return -1;
};

static void Take(CEntity *pen, CEntity *pw, INDEX iSlotIndex) {
  _bMenu = FALSE;
  if (iSlotIndex < 0) return;
  const Gun g = _aSlots[iSlotIndex].g;
  const INDEX iSam = Host(pw, g);
  if (iSam <= 0) return;
  if (!HasAmmo(pw, g)) {
    MenuSound(pen, pw, "common\\wpn_denyselect.wav");
    return;
  }
  MenuSound(pen, pw, "common\\wpn_select.wav");
  if (_gHolding != g) _gLast = _gHolding;
  if (g == G_SATCHEL || g == G_TRIPMINE) _gCannon = g;
  _iTargetSam = iSam;
  _ctTries = 0;
  _ctPicks++;
};

// TRUE: fire went to the menu (taking the gun)
static BOOL Select(CEntity *pen, CEntity *pw, CPlayerAction &pa, Gun gInHand) {
  _gHolding = gInHand;
  const ULONG ulKeyMask = 0x1FUL << PLACT_SELECT_SHIFT;
  const INDEX iKey = (INDEX)((pa.pa_ulButtons & ulKeyMask) >> PLACT_SELECT_SHIFT);
  if (!hl_bSlots) {
    // (Sam's own keys: the cannon's again with the cannon in hand swaps the satchels and the tripmines)
    if (iKey != 0 && iKey != _iOldKey && iKey == SamKey(W_IRONCANNON) && player::PropIndex(pw, PW_CURRENT, 0) == W_IRONCANNON) {
      _gCannon = _gCannon == G_SATCHEL ? G_TRIPMINE : G_SATCHEL;
      MenuSound(pen, pw, "common\\wpn_select.wav");
    }
    _iOldKey = iKey;
    _bMenu = FALSE;
    _iTargetSam = -1;
    return FALSE;
  }
  const ULONG ulMenuButtons = pa.pa_ulButtons & (PLACT_WEAPON_NEXT | PLACT_WEAPON_PREV | PLACT_WEAPON_FLIP | PLACT_FIRE);
  const ULONG ulPressed = ulMenuButtons & ~_ulOldMenuButtons;
  _ulOldMenuButtons = ulMenuButtons;
  const BOOL bKeyPressed = iKey != 0 && iKey != _iOldKey;
  _iOldKey = iKey;
  // Sam's own selection out of it (the keys 6-9 stay his)
  if (iKey <= 5) pa.pa_ulButtons &= ~ulKeyMask;
  pa.pa_ulButtons &= ~(PLACT_WEAPON_NEXT | PLACT_WEAPON_PREV | PLACT_WEAPON_FLIP);
  const TIME tmNow = _pTimer->CurrentTick();

  if (bKeyPressed && iKey <= 5) {
    const INDEX iSlot = iKey - 1;
    const INDEX iFrom = (_bMenu && _iMenu >= 0 && _aSlots[_iMenu].iSlot == iSlot) ? _iMenu : -1;
    // (the first in the slot, or the next after the one picked)
    INDEX iPick = -1;
    if (iFrom >= 0) {
      iPick = NextOwned(pw, iFrom, 1, iSlot);
    } else {
      for (INDEX i = 0; i < CT_SLOTS && iPick < 0; i++) if (_aSlots[i].iSlot == iSlot && Host(pw, _aSlots[i].g) > 0) iPick = i;
    }
    if (iPick < 0) {
      MenuSound(pen, pw, "common\\wpn_denyselect.wav");
      _bMenu = FALSE;
    } else if (hl_bFastSwitch && !_bMenu && NextOwned(pw, iPick, 1, iSlot) == iPick) {
      // (fast: at once - only when it's the slot's only one, as Half-Life's SelectSlot)
      Take(pen, pw, iPick);
    } else {
      MenuSound(pen, pw, _bMenu ? "common\\wpn_moveselect.wav" : "common\\wpn_hudon.wav");
      _bMenu = TRUE;
      _iMenu = iPick;
      _tmMenu = tmNow;
    }
  } else if (bKeyPressed) {
    _bMenu = FALSE;
  }
  if (ulPressed & (PLACT_WEAPON_NEXT | PLACT_WEAPON_PREV)) {
    const INDEX iDir = (ulPressed & PLACT_WEAPON_NEXT) ? 1 : -1;
    const INDEX iFrom = _bMenu && _iMenu >= 0 ? _iMenu : IndexOf(gInHand);
    const INDEX iPick = NextOwned(pw, iFrom >= 0 ? iFrom : (iDir > 0 ? CT_SLOTS - 1 : 0), iDir, -1);
    if (iPick >= 0) {
      if (hl_bFastSwitch) {
        Take(pen, pw, iPick);
      } else {
        MenuSound(pen, pw, "common\\wpn_moveselect.wav");
        _bMenu = TRUE;
        _iMenu = iPick;
        _tmMenu = tmNow;
      }
    }
  }
  if (ulPressed & PLACT_WEAPON_FLIP) Take(pen, pw, IndexOf(_gLast));

  BOOL bFireTaken = FALSE;
  if (_bMenu && (ulPressed & PLACT_FIRE)) {
    Take(pen, pw, _iMenu);
    _bHoldFire = TRUE;
  }
  // (fire that took a gun from the menu shoots nothing till it's let go and Sam has the gun in hand)
  if (_bHoldFire) {
    const BOOL bSwitching = _iTargetSam > 0 || player::PropIndex(pw, PW_CURRENT, 0) != player::PropIndex(pw, PW_WANTED, 0);
    if ((pa.pa_ulButtons & PLACT_FIRE) || bSwitching) {
      pa.pa_ulButtons &= ~PLACT_FIRE;
      bFireTaken = TRUE;
    } else {
      _bHoldFire = FALSE;
    }
  }

  // Sam asked for the host: his slot key, again a tick later if he took the other of the pair
  if (_iTargetSam > 0) {
    const INDEX iWanted = player::PropIndex(pw, PW_WANTED, 0);
    if (iWanted == _iTargetSam || _ctTries >= 6) {
      _iTargetSam = -1;
    } else if (_bAskedLastTick) {
      _bAskedLastTick = FALSE;
    } else {
      const INDEX iSamKey = SamKey(_iTargetSam);
      if (iSamKey > 0) pa.pa_ulButtons = (pa.pa_ulButtons & ~ulKeyMask) | ((ULONG)iSamKey << PLACT_SELECT_SHIFT);
      _bAskedLastTick = TRUE;
      _ctTries++;
    }
  }
  return bFireTaken;
};

void GetMenu(CEntity *pen, Menu &menu) {
  memset(&menu, 0, sizeof(menu));
  CEntity *pw = pen != NULL ? player::PropEntity(pen, PROP_WEAPONS) : NULL;
  if (pw == NULL || !hl_bWeapons || !hl_bSlots) return;
  menu.bOpen = _bMenu;
  menu.iSlot = _bMenu && _iMenu >= 0 ? _aSlots[_iMenu].iSlot : -1;
  for (INDEX i = 0; i < CT_SLOTS && menu.ctGuns < 16; i++) {
    if (Host(pw, _aSlots[i].g) <= 0) continue;
    MenuGun &mg = menu.aGuns[menu.ctGuns++];
    mg.strFile = _astrHudFile[_aSlots[i].g];
    mg.iSlot = _aSlots[i].iSlot;
    mg.iPos = _aSlots[i].iPos;
    mg.bAmmo = HasAmmo(pw, _aSlots[i].g);
    mg.bSelected = _bMenu && i == _iMenu;
  }
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
  // (the Glock's, the .357's and the MP5's come with their view models' reloads)
  const BOOL bModelSounds = viewmodel::Ready() && (g == G_PYTHON || g == G_GLOCK || g == G_MP5);
  if (bModelSounds) {
  } else if (g == G_SHOTGUN) Sound(pen, pw, "weapons\\reload1.wav|weapons\\reload3.wav", SND_DOUBLESHOTGUN_RELOAD, TRUE);
  else if (g == G_PYTHON) Sound(pen, pw, "weapons\\357_reload1.wav", SND_COLT_RELOAD, TRUE);
  else if (g == G_GLOCK) Sound(pen, pw, "items\\9mmclip1.wav", SND_COLT_RELOAD, TRUE);
  else if (g == G_MP5) Sound(pen, pw, "items\\clipinsert1.wav", -1, TRUE);
  else if (g == G_CROSSBOW) Sound(pen, pw, "weapons\\xbow_reload1.wav", -1, TRUE);
  // PYTHON_RELOAD, GLOCK_RELOAD / _RELOAD_NOT_EMPTY, SHOTGUN_RELOAD (a shell), MP5_RELOAD, RPG_RELOAD, CROSSBOW_RELOAD
  switch (g) {
    case G_PYTHON: View(3); break;
    case G_GLOCK: View(gs.iClip == 0 ? 5 : 6); break;
    case G_SHOTGUN: View(3); break;
    case G_MP5: View(3); break;
    case G_RPG: View(2); break;
    case G_CROSSBOW: View(7); break;
  }
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
    ViewTake(G_NONE);
    return;
  }
  // a cinematic (a level's intro or end) or an auto action: nothing of the guns' - the fire is Sam's (it skips one)
  if (player::PropEntity(pen, player::PROP_CAMERA) != NULL || player::PropEntity(pen, player::PROP_ACTION_MARKER) != NULL) {
    EgonOff(pen, pw);
    _bCharging = FALSE;
    _bPinPulled = FALSE;
    _bMenu = FALSE;
    return;
  }
  const INDEX iSam = player::PropIndex(pw, PW_CURRENT, 0);
  const INDEX iWanted = player::PropIndex(pw, PW_WANTED, 0);
  const Gun g = GunFor(iSam);
  // Half-Life's weapon menu first (fire may go to it)
  if (Select(pen, pw, pa, g)) pa.pa_ulButtons &= ~PLACT_FIRE;
  const BOOL bAlt = AltDown();
  if (hl_iTestAlt > 0) hl_iTestAlt--;
  if (g == G_NONE) {
    _iLastSam = iSam;
    _bCharging = FALSE;
    EgonOff(pen, pw);
    ViewTake(G_NONE);
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
    ViewHolster();
    return;
  }
  ViewTake(g);
  if (iSam != _iLastSam || g != _gLastGun) {
    _iLastSam = iSam;
    _gLastGun = g;
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
      // CROWBAR_ATTACK1HIT, _ATTACK2HIT, _ATTACK3HIT; the misses: _ATTACK1MISS, _ATTACK2MISS, _ATTACK3MISS
      static const INDEX aiHit[3] = { 3, 6, 8 }, aiMiss[3] = { 4, 5, 7 };
      View(bHit ? aiHit[_iSwing % 3] : aiMiss[_iSwing % 3]);
      _iSwing++;
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
      View(2); // PYTHON_FIRE1
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
      View(gs.iClip == 0 ? 4 : 3); // GLOCK_SHOOT_EMPTY, GLOCK_SHOOT
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
      View(2); // SHOTGUN_FIRE2
      Flash(pw);
      Noise(pw, 60.0f);
      Log("double", g, gs.iClip, iReserve);
    } else if ((bFire || bAlt) && Due(tmNow, gs.tmNext) && gs.iClip >= 1) {
      Pellets(pw, iSam, 6, DMG_BUCKSHOT, CONE_10);
      gs.iClip -= 1;
      gs.tmNext = tmNow + 0.75f;
      Sound(pen, pw, "weapons\\sbarrel1.wav", SND_SHOTGUN_FIRE, FALSE);
      Anim(pw, "Fire1,Fire,Fire1Fast");
      View(1); // SHOTGUN_FIRE
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
        const FLOAT3D vAhead = Forward(pen);
        CEntity *penG = NewGrenade(pw, 1, Eyes(pen) + vAhead * (16.0f * Unit()));
        if (penG != NULL) grenades::Track(penG, grenades::K_CONTACT, vAhead * (800.0f * Unit()), -1.0f);
        (*piGrenades)--;
        gs.tmNext = tmNow + 1.0f;
        Sound(pen, pw, "weapons\\glauncher.wav|weapons\\glauncher2.wav", SND_GRENADE_FIRE, FALSE);
        View(2); // MP5_LAUNCH
        _ctShots++;
        Log("grenade", g, gs.iClip, *piGrenades);
      }
    } else if (bFire && Due(tmNow, gs.tmNext) && gs.iClip > 0) {
      Bullet(pw, iSam, 8192.0f, DMG_MP5, CONE_3);
      gs.iClip--;
      gs.tmNext = tmNow + 0.1f;
      Sound(pen, pw, "weapons\\hks1.wav|weapons\\hks2.wav|weapons\\hks3.wav", SND_TOMMYGUN_FIRE, FALSE);
      Anim(pw, "Fire");
      View(5 + pw->IRnd() % 3); // MP5_FIRE1..3
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
      View(3); // RPG_FIRE2
      Noise(pw, 60.0f);
      _ctShots++;
      Log("fire", g, gs.iClip, iReserve);
    }
    break;
  case G_GRENADE:
    // handgrenade.cpp: the pin comes out on the press; let go (and half a second on), it's thrown - the higher the view,
    // the faster ((90 - pitch) x 4, at most 500 u/s; the throw 10 degrees over the view), with his own speed; its fuse
    // is 3 s from the pin, so holding it cooks it
    if (_tmRedraw > 0 && Due(tmNow, _tmRedraw)) {
      _tmRedraw = 0;
      if (iReserve > 0) View(7); // HANDGRENADE_DRAW
    }
    if (bFire && !_bPinPulled && Due(tmNow, gs.tmNext) && iReserve > 0) {
      _bPinPulled = TRUE;
      _tmPinPulled = tmNow;
      View(2); // HANDGRENADE_PINPULL
    } else if (!bFire && _bPinPulled && Due(tmNow, _tmPinPulled + 0.5f)) {
      const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)pen, FALSE);
      FLOAT fPitch = -plView.pl_OrientationAngle(2); // (Half-Life's: down positive)
      fPitch = fPitch < 0.0f ? -10.0f + fPitch * (80.0f / 90.0f) : -10.0f + fPitch * (100.0f / 90.0f);
      const FLOAT fSpeed = Min((90.0f - fPitch) * 4.0f, 500.0f);
      FLOAT3D vThrow;
      AnglesToDirectionVector(ANGLE3D(plView.pl_OrientationAngle(1), -fPitch, 0), vThrow);
      CEntity *penG = NewGrenade(pw, 0, plView.pl_PositionVector + vThrow * (16.0f * Unit()));
      if (penG != NULL) {
        const FLOAT3D vVelocity = vThrow * (fSpeed * Unit()) + ((CMovableEntity *)pen)->en_vCurrentTranslationAbsolute;
        grenades::Track(penG, grenades::K_HAND, vVelocity, Max(_tmPinPulled + 3.0f - tmNow, 0.0f));
      }
      _bPinPulled = FALSE;
      if (piReserve != NULL) (*piReserve)--;
      gs.tmNext = tmNow + 0.5f;
      _tmRedraw = tmNow + 0.5f;
      Anim(pw, "Fire");
      View(fSpeed < 500.0f ? 3 : 4); // HANDGRENADE_THROW1, _THROW2
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
        viewmodel::SetIdle(0);
        View(0);
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
        viewmodel::SetIdle(0);
        View(5); // GAUSS_FIRE
        Flash(pw);
        Noise(pw, 60.0f);
        CPrintF(SB_TAG " gauss charged %.2f s: %.0f damage, pushed back %.0f u/s\n", tmCharged, fDamage, vPush.Length() / Unit());
        _strLast = "charged";
      }
    } else if (bAlt && Due(tmNow, gs.tmNext) && iReserve > 0) {
      _bCharging = TRUE;
      _tmChargeStart = tmNow;
      viewmodel::SetIdle(4); // GAUSS_SPIN, after GAUSS_SPINUP
      View(3);
      _tmChargeDrain = tmNow;
    } else if (bFire && Due(tmNow, gs.tmNext) && iReserve >= 2) {
      Bullet(pw, iSam, 8192.0f, DMG_GAUSS, 0.0f);
      if (piReserve != NULL) *piReserve -= 2;
      gs.tmNext = tmNow + 0.2f;
      Sound(pen, pw, "weapons\\gauss2.wav", SND_LASER_FIRE, FALSE);
      Anim(pw, "Fire");
      View(6); // GAUSS_FIRE2
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
        viewmodel::SetIdle(3); // EGON_ALTFIRECYCLE, after EGON_ALTFIREON
        View(2);
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
    const INDEX ctOut = grenades::Satchels();
    // the radio in hand while some are out (SATCHEL_RADIO_DRAW), the satchel again when none are (SATCHEL_DRAW)
    if ((ctOut > 0) != _bRadio && Due(tmNow, gs.tmNext)) {
      _bRadio = ctOut > 0;
      viewmodel::SetModel(_bRadio ? "v_satchel_radio.mdl" : "v_satchel.mdl", 2, 0);
    }
    if (bFire && ctOut > 0 && Due(tmNow, gs.tmNext)) {
      grenades::Detonate(pen);
      View(3); // SATCHEL_RADIO_FIRE
      gs.tmNext = tmNow + 0.5f;
      _ctShots++;
      _fLastDamage = DMG_SATCHEL;
      Log("detonate", g, ctOut, iReserve);
    } else if ((bFire || bAlt) && Due(tmNow, gs.tmNext) && iReserve > 0) {
      CMovableEntity *pmen = (CMovableEntity *)pen;
      CEntity *penS = NewGrenade(pw, 0, pen->GetPlacement().pl_PositionVector - pmen->en_vGravityDir * 1.0f);
      if (penS != NULL) {
        grenades::Track(penS, grenades::K_SATCHEL, Forward(pen) * (274.0f * Unit()) + pmen->en_vCurrentTranslationAbsolute, -1.0f);
        if (piReserve != NULL) (*piReserve)--;
        gs.tmNext = tmNow + 1.0f;
        Anim(pw, "Fire");
        if (!_bRadio) View(3); // SATCHEL_DROP
        Log("throw", g, grenades::Satchels(), piReserve != NULL ? *piReserve : -1);
      }
    }
    } break;
  case G_TRIPMINE:
    // tripmine.cpp: on the wall ahead (128 units at most), facing out; nothing there, nothing placed; 0.3 s apart
    if (bFire && Due(tmNow, gs.tmNext) && iReserve > 0) {
      FLOAT3D vHit, vNormal;
      CEntity *penWall = NULL;
      if (tripmine::Aim(pen, vHit, vNormal, penWall)) {
        CEntity *penM = NewGrenade(pw, 0, vHit + vNormal * (8.0f * Unit()));
        if (penM != NULL) {
          grenades::Track(penM, grenades::K_TRIPMINE, FLOAT3D(0, 0, 0), -1.0f);
          tripmine::Add(penM, pen, vNormal, penWall);
          if (piReserve != NULL) (*piReserve)--;
          Anim(pw, "Fire");
          View(6); // TRIPMINE_DRAW (the next one)
          _ctShots++;
          _fLastDamage = 150.0f;
          Log("placed", g, tripmine::Count(), piReserve != NULL ? *piReserve : -1);
        }
      }
      gs.tmNext = tmNow + 0.3f;
    }
    break;
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
      View(gs.iClip == 0 ? 6 : 4); // CROSSBOW_FIRE3 (the last), CROSSBOW_FIRE1
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
  viewmodel::SetSoundCallback(&ModelSound);
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
  _gLastGun = G_NONE;
  _bCharging = FALSE;
  _bPinPulled = FALSE;
  _tmRedraw = 0;
  _bOldFire = _bOldAlt = FALSE;
  _bEgonOn = FALSE;
  _gView = G_NONE;
  _bHolstered = _bRadio = FALSE;
  viewmodel::SetModel(NULL, 0, 0);
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
  } else if (g == G_SATCHEL && grenades::Satchels() > 0) {
    _snprintf(str, sizeof(str), "%s %d out", _astrGun[g], grenades::Satchels());
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
  CTString strMenu;
  strMenu.PrintF("; menu %s (pick %s), asking Sam for %d (%d tries), last %s, %u picks", _bMenu ? "open" : "closed",
    _iMenu >= 0 && _iMenu < CT_SLOTS ? _astrGun[_aSlots[_iMenu].g] : "-", _iTargetSam, _ctTries, _astrGun[_gLast], _ctPicks);
  str.PrintF("weapons %s; in hand Sam's %d = %s, clip %d, reserve %d%s%s; %d satchels out; %u shots (last %.0f damage, %s); sounds: %u Half-Life's, %u Sam's",
    !_bReady ? "unavailable" : hl_bWeapons ? "Half-Life's" : "Sam's",
    iSam, _astrGun[g], _aGuns[g].iClip, piReserve != NULL ? *piReserve : -1, _bCharging ? ", charging" : "",
    _bEgonOn ? ", beam on" : "", grenades::Satchels(), _ctShots,
    _fLastDamage, _strLast, _ctHLSounds, _ctSamSounds);
  str += strMenu;
  return str;
};

const char *HudGun(CEntity *pen, INDEX &iClip, INDEX &iAmmo, INDEX &iAmmo2) {
  iClip = iAmmo = iAmmo2 = -1;
  if (!_bReady || !hl_bWeapons || pen == NULL) return NULL;
  CEntity *pw = player::PropEntity(pen, PROP_WEAPONS);
  if (pw == NULL) return NULL;
  const Gun g = GunFor(player::PropIndex(pw, PW_CURRENT, 0));
  if (g == G_NONE) return NULL;
  if (_aiClip[g] > 0) iClip = _aGuns[g].iClip;
  const INDEX *piReserve = Reserve(pw, g);
  iAmmo = piReserve != NULL ? *piReserve : -1;
  if (g == G_MP5) {
    const INDEX *piGrenades = (const INDEX *)player::Prop(pw, PW_GRENADES);
    iAmmo2 = piGrenades != NULL ? *piGrenades : -1;
  }
  return _astrHudFile[g];
};

// the egon's (ev_hldm.cpp EV_EgonFire, the wide beam single player fires: 50 50 125 at brightness 0.7; 3.5 and 5 units
// wide, the second its noise) from the gun's attachment to where the view meets something (2048 units), the flare there
// (EGON_FLARE_SPRITE: sprites/XSpark1, scale 1)
void CollectWorld(CEntity *pen, std::vector<viewmodel::Beam> &aBeams, std::vector<viewmodel::Glow> &aGlows) {
  CWorld *pwo = IWorld::GetWorld();
  if (!_bReady || !hl_bWeapons || pen == NULL || pwo == NULL || !_bEgonOn || _gView != G_EGON) return;
  const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)pen, TRUE);
  FLOAT3D vDir;
  AnglesToDirectionVector(plView.pl_OrientationAngle, vDir);
  const FLOAT fReach = 2048.0f * Unit();
  CCastRay cr(pen, plView.pl_PositionVector, plView.pl_PositionVector + vDir * fReach);
  cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
  pwo->CastRay(cr);
  const FLOAT3D vEnd = cr.cr_penHit != NULL ? cr.cr_vHit : plView.pl_PositionVector + vDir * fReach;
  FLOAT3D vFrom;
  if (viewmodel::Attachment(0, vFrom)) {
    vFrom *= Unit();
  } else {
    vFrom = FLOAT3D(0.1f, -0.2f, -0.5f);
  }
  viewmodel::Beam be;
  be.vFrom = vFrom;
  be.vTo = vEnd;
  be.bFromEye = TRUE;
  be.strSprite = "xbeam1";
  be.fR = 50.0f / 255.0f * 0.7f;
  be.fG = 50.0f / 255.0f * 0.7f;
  be.fB = 125.0f / 255.0f * 0.7f;
  be.fWidth = 3.5f * Unit();
  aBeams.push_back(be);
  be.fWidth = 5.0f * Unit();
  aBeams.push_back(be);
  if (cr.cr_penHit != NULL) {
    viewmodel::Glow gw;
    gw.vPos = vEnd - vDir * (4.0f * Unit());
    gw.fSize = Unit();
    gw.strSprite = "xspark1";
    gw.iFrame = (INDEX)(_pTimer->GetLerpedCurrentTick() * 10.0f);
    gw.fR = gw.fG = gw.fB = 1.0f;
    aGlows.push_back(gw);
  }
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
