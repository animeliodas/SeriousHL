/* SeriousHL - Half-Life's movement in Serious Sam Classic (TFE 1.05 / TSE 1.07), Classics Patch plugin.
 *
 * Sam's player runs, jumps, ducks, strafes in the air and bunny hops by Half-Life's rules (Rules.cpp, HlMove.cpp);
 * Sam's own movement is out of the way (the SeriousBridge core's movement takeover, core/sam/Movement.h). Everything
 * else is Sam's: weapons, monsters, levels, swimming, cutscenes. Part of SeriousBridge: mechanics of other games in
 * Serious Sam.
 *
 * Two builds: the usual one (the ruleset in the console as hl_*, test hooks), and the speedrun one (SERIOUSHL_RUN:
 * Half-Life's ruleset fixed, no hl_* to change, no test hooks, a mark on the screen - Run.h).
 */
#include "StdH.h"
#include "HlMove.h"
#include "Run.h"
#include "Weapons.h"
#include "Movement.h"
#include "Player.h"
#include "Physics.h"
#include "DevHook.h"
#include "Speedrun.h"

#include <stdio.h>

#define SERIOUSHL_VERSION "0.4.0"

CLASSICSPATCH_DEFINE_PLUGIN(k_EPluginFlagGame, MakeVersion(0, 4, 0),
  "SeriousBridge", "SeriousHL", "Half-Life's movement and weapons for Serious Sam's player.");

// ------------------------------------------------------------------
// Console
// ------------------------------------------------------------------
static void HL_Status(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  CPrintF("[SeriousHL] %s\n", speedrun::Status().str_String);
#ifdef SERIOUSHL_RUN
  CPrintF("[SeriousHL] %s; %s\n", run::Mark(), hl::Status().str_String);
#else
  CPrintF("[SeriousHL] %s\n", hl::Status().str_String);
  CPrintF("[SeriousHL] %s\n", weapons::Status(player::Local()).str_String);
#endif
};

// Tests: where the player is (and an intro or Netricsa in the way); Netricsa closed.
static void HL_Where(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  devhook::Where();
};

static void HL_CloseNetricsa(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  devhook::CloseNetricsa();
};

#ifndef SERIOUSHL_RUN
// ETrigger (Engine's base events: code 7), to trigger a level's exit in tests
class CTriggerEventLocal : public CEntityEvent {
public:
  CEntityPointer penCaused;
  CTriggerEventLocal(void) : CEntityEvent(0x00000007) {};
  CEntityEvent *MakeCopy(void) { return new CTriggerEventLocal(*this); };
};

// --- test hooks (tools/test_hl.py): keys held for the next ticks, the view turned every tick, fire pressed (intros),
// keys by the tick, the player moved, ticks traced to a file. Not in the speedrun build.
static INDEX hl_iTestTicks = 0;
static FLOAT hl_fTestForward = 0.0f;
static FLOAT hl_fTestSide = 0.0f;
static INDEX hl_bTestJump = FALSE;
static INDEX hl_bTestDuck = FALSE;
static FLOAT hl_fTestTurn = 0.0f;
static INDEX hl_iTestFire = 0;
static INDEX hl_iTestSelect = 0;
static FLOAT hl_fTestPitch = 0.0f;   // the view's pitch held there (degrees, down negative) while not 0 // a weapon to select (Player.es WeaponType: 8 rocket launcher)

static const ULONG PLACT_FIRE = 1UL << 0; // Player.es

// keys by the tick, "j2,d20,-5,fj10": f b l r (forward, back, left, right), j jump, d duck, - nothing; the number is how
// many ticks they're held
struct TestKeys { FLOAT fForward, fSide; BOOL bJump, bDuck; INDEX ctActions; };
static TestKeys _atkScript[32];
static INDEX _ctScript = 0, _iScript = 0;
static BOOL _bFireSent = FALSE, _bScriptSent = FALSE;

static void HL_TestKeys(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strScript = *NEXT_ARG(CTString *);
  _ctScript = 0;
  _iScript = 0;
  const char *pch = strScript.str_String;
  while (*pch != 0 && _ctScript < 32) {
    TestKeys tk = { 0.0f, 0.0f, FALSE, FALSE, 0 };
    for (; *pch != 0 && !(*pch >= '0' && *pch <= '9'); pch++) {
      if (*pch == 'f') tk.fForward += 1.0f;
      if (*pch == 'b') tk.fForward -= 1.0f;
      if (*pch == 'r') tk.fSide += 1.0f;
      if (*pch == 'l') tk.fSide -= 1.0f;
      if (*pch == 'j') tk.bJump = TRUE;
      if (*pch == 'd') tk.bDuck = TRUE;
    }
    while (*pch >= '0' && *pch <= '9') tk.ctActions = tk.ctActions * 10 + (*pch++ - '0');
    if (*pch == ',') pch++;
    if (tk.ctActions > 0) _atkScript[_ctScript++] = tk;
  }
  CPrintF("[SeriousHL] test keys: %d segments\n", _ctScript);
};

static void HL_Move(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strArgs = *NEXT_ARG(CTString *);
  FLOAT x = 0, y = 0, z = 0;
  CEntity *pen = player::Local();
  if (pen == NULL || sscanf(strArgs.str_String, "%f %f %f", &x, &y, &z) != 3) return;
  CPlacement3D pl = pen->GetPlacement();
  pl.pl_PositionVector += FLOAT3D(x, y, z);
  const FLOAT3D vStill(0, 0, 0);
  physics::PlaceNow(pen, pl, &vStill);
  CPrintF("[SeriousHL] player at (%.2f, %.2f, %.2f)\n", pl.pl_PositionVector(1), pl.pl_PositionVector(2), pl.pl_PositionVector(3));
};

// a Sam monster on the floor ahead as a target: "Boneman" (6 m), "Boneman 1.2"
static void HL_SpawnTarget(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strArgs = *NEXT_ARG(CTString *);
  char strName[64] = "";
  FLOAT fDistance = 6.0f;
  if (sscanf(strArgs.str_String, "%63s %f", strName, &fDistance) < 1) return;
  CEntity *pen = player::Local();
  if (pen == NULL || !_pNetwork->IsServer()) return;
  FLOATmatrix3D m;
  MakeRotationMatrixFast(m, ANGLE3D(pen->GetPlacement().pl_OrientationAngle(1), 0, 0));
  const FLOAT3D vAhead = FLOAT3D(0, 0, -1) * m;
  CPlacement3D pl(pen->GetPlacement().pl_PositionVector + vAhead * fDistance + FLOAT3D(0, 0.5f, 0),
    ANGLE3D(pen->GetPlacement().pl_OrientationAngle(1) + 180.0f, 0, 0));
  try {
    CEntity *penTarget = IWorld::GetWorld()->CreateEntity_t(pl, CTString("Classes\\") + strName + ".ecl");
    penTarget->Initialize();
    CPrintF("[SeriousHL] target %s (%u)\n", strName, penTarget->en_ulID);
  } catch (char *strError) {
    CPrintF("[SeriousHL] target: %s\n", strError);
  }
};

// what's under the crosshair, and its health
static void HL_Probe(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  CEntity *pen = player::Local();
  if (pen == NULL) return;
  const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)pen, TRUE);
  FLOAT3D vDir;
  AnglesToDirectionVector(plView.pl_OrientationAngle, vDir);
  CCastRay cr(pen, plView.pl_PositionVector, plView.pl_PositionVector + vDir * 300.0f);
  cr.cr_ttHitModels = CCastRay::TT_COLLISIONBOX;
  IWorld::GetWorld()->CastRay(cr);
  CEntity *penHit = cr.cr_penHit;
  if (penHit == NULL) {
    CPrintF("[SeriousHL] probe: nothing\n");
  } else {
    const FLOAT fHealth = (penHit->GetFlags() & ENF_ALIVE) ? ((CLiveEntity *)penHit)->en_fHealth : -1.0f;
    CPrintF("[SeriousHL] probe: %s (%u) at %.2f m, health %.2f\n", penHit->GetClass()->ec_pdecDLLClass->dec_strName,
      penHit->en_ulID, cr.cr_fHitDistance, fHealth);
  }
};

// look at an entity (by id: its middle) and tell its health
static void HL_Aim(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strId = *NEXT_ARG(CTString *);
  CEntity *pen = player::Local();
  CEntity *penTarget = physics::FindEntity((ULONG)atol(strId.str_String));
  if (pen == NULL || penTarget == NULL) {
    CPrintF("[SeriousHL] aim: no %s\n", strId.str_String);
    return;
  }
  FLOATaabbox3D box;
  penTarget->GetBoundingBox(box);
  const CPlacement3D plView = IWorld::GetViewpoint((CPlayerEntity *)pen, TRUE);
  FLOAT3D vTo = box.Center() - plView.pl_PositionVector;
  const FLOAT fDistance = vTo.Length();
  ANGLE3D aView;
  DirectionVectorToAngles(vTo.SafeNormalize(), aView);
  CPlayerEntity *ppl = (CPlayerEntity *)pen;
  ppl->en_plViewpoint.pl_OrientationAngle(1) = NormalizeAngle(aView(1) - pen->GetPlacement().pl_OrientationAngle(1));
  ppl->en_plViewpoint.pl_OrientationAngle(2) = aView(2);
  const FLOAT fHealth = (penTarget->GetFlags() & ENF_ALIVE) ? ((CLiveEntity *)penTarget)->en_fHealth : -1.0f;
  CPrintF("[SeriousHL] aim: %s (%u) at %.2f m, health %.2f\n", penTarget->GetClass()->ec_pdecDLLClass->dec_strName,
    penTarget->en_ulID, fDistance, fHealth);
};

static void HL_DumpAnims(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  weapons::DumpAnims(player::Local());
};

static void HL_Trace(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  hl::TraceTicks(NEXT_ARG(INDEX));
};

// (the hooks count ticks: the client sends an action every frame, more often than the game ticks)
static void TestTick(CEntity *pen) {
  if (hl_fTestPitch != 0.0f) ((CPlayerEntity *)pen)->en_plViewpoint.pl_OrientationAngle(2) = hl_fTestPitch;
  // (a tick counts once an action with the keys has gone: a command can come between them)
  if (hl_iTestFire > 0 && _bFireSent) hl_iTestFire--;
  if (_iScript < _ctScript && _bScriptSent && --_atkScript[_iScript].ctActions <= 0) _iScript++;
  _bFireSent = _bScriptSent = FALSE;
  if (hl_iTestTicks <= 0) return;
  hl_iTestTicks--;
  if (hl_fTestTurn != 0.0f) ((CPlayerEntity *)pen)->en_plViewpoint.pl_OrientationAngle(1) += hl_fTestTurn;
};

// the keys into an action (jump and duck together as the core puts them: duck, and the jump's own bit)
static void TestTranslation(CPlayerAction &pa, FLOAT fForward, FLOAT fSide, BOOL bJump, BOOL bDuck) {
  pa.pa_vTranslation = FLOAT3D(fSide * 10.0f, bDuck ? -11.0f : (bJump ? 11.0f : 0.0f), -fForward * 10.0f);
  if (bJump && bDuck) pa.pa_ulButtons |= movement::ACTION_JUMP_WITH_DUCK;
};

static void TestAction(CPlayerAction &pa, BOOL bFresh) {
  if (hl_iTestTicks > 0) TestTranslation(pa, hl_fTestForward, hl_fTestSide, hl_bTestJump, hl_bTestDuck);
  if (_iScript < _ctScript) {
    TestKeys &tk = _atkScript[_iScript];
    TestTranslation(pa, tk.fForward, tk.fSide, tk.bJump, tk.bDuck);
    _bScriptSent = TRUE;
  }
  if (hl_iTestFire > 0) {
    pa.pa_ulButtons |= PLACT_FIRE;
    _bFireSent = TRUE;
  }
  if (bFresh && hl_iTestSelect > 0) {
    // PLACT_SELECT_WEAPON_SHIFT: 9 in TFE's Player.es, 14 in TSE's
#if SE1_GAME == SS_TFE
    const INDEX iShift = 9;
#else
    const INDEX iShift = 14;
#endif
    pa.pa_ulButtons = (pa.pa_ulButtons & ~(0x1FUL << iShift)) | ((ULONG)(hl_iTestSelect & 0x1F) << iShift);
    hl_iTestSelect = 0;
  }
};

// the level's way out (its first WorldLink) triggered, as if the player reached it
static void HL_TestLevelExit(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  CWorld *pwo = IWorld::GetWorld();
  CEntity *penPlayer = player::Local();
  if (pwo == NULL || penPlayer == NULL) return;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    CEntity *pen = &*iten;
    const char *strClass = pen->GetClass()->ec_pdecDLLClass->dec_strName;
    if ((pen->GetFlags() & ENF_DELETED) || (stricmp(strClass, "WorldLink") != 0 && stricmp(strClass, "World link") != 0)) continue;
    // (the level's own way out: a link to another world)
    CTString *pstrWorld = (CTString *)player::Prop(pen, (214UL << 8) | 2); // WorldLink.es m_strWorld
    if (pstrWorld != NULL && pstrWorld->Length() == 0) continue;
    CTriggerEventLocal ee;
    ee.penCaused = penPlayer;
    pen->SendEvent(ee);
    CPrintF("[SeriousHL] level exit %u triggered\n", pen->en_ulID);
    return;
  }
  CPrintF("[SeriousHL] level exit: none\n");
};

// the game's end, as Player.es marks it (m_bEndOfGame)
static void HL_TestGameEnd(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  INDEX *pb = (INDEX *)player::Prop(player::Local(), (401UL << 8) | 97);
  if (pb != NULL) *pb = TRUE;
};

// the game paused / going on, as the Pause key does
static void HL_TestPause(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  _pNetwork->TogglePause();
};

static void DeclareUsual(void) {
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_TestPause", "void", &HL_TestPause);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_TestLevelExit", "void", &HL_TestLevelExit);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_TestGameEnd", "void", &HL_TestGameEnd);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Move", "CTString", &HL_Move);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Trace", "INDEX", &HL_Trace);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_TestKeys", "CTString", &HL_TestKeys);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_DumpAnims", "void", &HL_DumpAnims);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_SpawnTarget", "CTString", &HL_SpawnTarget);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Probe", "void", &HL_Probe);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Aim", "CTString", &HL_Aim);
  _pShell->DeclareSymbol("persistent user INDEX hl_bWeapons;", &weapons::hl_bWeapons);
  _pShell->DeclareSymbol("persistent user INDEX hl_iAltFireKey;", &weapons::hl_iAltFireKey);
  _pShell->DeclareSymbol("user INDEX hl_iTestAlt;", &weapons::hl_iTestAlt);

  rules::Settings &s = hl::_settings;
  _pShell->DeclareSymbol("persistent user INDEX hl_bEnable;", &hl::hl_bEnable);
  _pShell->DeclareSymbol("persistent user INDEX hl_bAutoJump;", &s.bAutoJump);
  _pShell->DeclareSymbol("persistent user INDEX hl_bBhopCap;", &s.bBhopCap);
  _pShell->DeclareSymbol("persistent user INDEX hl_bFallDamage;", &s.bFallDamage);
  _pShell->DeclareSymbol("persistent user INDEX hl_iFps;", &s.iFps);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fUnit;", &s.fUnit);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fMaxSpeed;", &s.fMaxSpeed);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fAccelerate;", &s.fAccelerate);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fAirAccelerate;", &s.fAirAccelerate);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fFriction;", &s.fFriction);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fEdgeFriction;", &s.fEdgeFriction);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fStopSpeed;", &s.fStopSpeed);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fGravity;", &s.fGravity);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fJumpHeight;", &s.fJumpHeight);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fMaxVelocity;", &s.fMaxVelocity);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fForwardSpeed;", &s.fForwardSpeed);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fSideSpeed;", &s.fSideSpeed);

  _pShell->DeclareSymbol("user INDEX hl_iTestTicks;", &hl_iTestTicks);
  _pShell->DeclareSymbol("user FLOAT hl_fTestForward;", &hl_fTestForward);
  _pShell->DeclareSymbol("user FLOAT hl_fTestSide;", &hl_fTestSide);
  _pShell->DeclareSymbol("user INDEX hl_bTestJump;", &hl_bTestJump);
  _pShell->DeclareSymbol("user INDEX hl_bTestDuck;", &hl_bTestDuck);
  _pShell->DeclareSymbol("user FLOAT hl_fTestTurn;", &hl_fTestTurn);
  _pShell->DeclareSymbol("user INDEX hl_iTestFire;", &hl_iTestFire);
  _pShell->DeclareSymbol("user INDEX hl_iTestSelect;", &hl_iTestSelect);
  _pShell->DeclareSymbol("user FLOAT hl_fTestPitch;", &hl_fTestPitch);
};
#endif // !SERIOUSHL_RUN

// ------------------------------------------------------------------
// The speedometer: the player's speed along the ground, in Half-Life's units, under the crosshair
// ------------------------------------------------------------------
static INDEX sr_bShowSpeed = TRUE;

static void DrawSpeed(CDrawPort *pdp, CEntity *pen) {
  if (!sr_bShowSpeed || pen == NULL || !(pen->en_ulPhysicsFlags & EPF_MOVABLE)) return;
  const CMovableEntity *pmen = (CMovableEntity *)pen;
  const FLOAT3D v = pmen->en_vCurrentTranslationAbsolute;
  const FLOAT3D vUp = -pmen->en_vGravityDir;
  const FLOAT fSpeed = (v - vUp * (v % vUp)).Length() / hl::_settings.fUnit;
  const FLOAT fScale = pdp->GetHeight() / 720.0f;
  pdp->SetFont(_pfdConsoleFont);
  pdp->SetTextAspect(1.0f);
  pdp->SetTextScaling(fScale * 1.5f);
  CTString str;
  str.PrintF("%d", (INDEX)floorf(fSpeed + 0.5f));
  const PIX pixX = pdp->GetWidth() / 2;
  const PIX pixY = pdp->GetHeight() / 2 + (PIX)(60 * fScale);
  pdp->PutTextC(str, pixX + 1, pixY + 1, 0x000000A0);
  pdp->PutTextC(str, pixX, pixY, fSpeed > hl::_settings.fMaxSpeed * 1.05f ? 0x80FF80E0 : 0xFFFFFFC0);
};

// ------------------------------------------------------------------
// Events
// ------------------------------------------------------------------
static void HL_OnStep(void) {
  CEntity *pen = GetGameAPI()->IsGameOn() ? player::Local() : NULL;
  if (pen == NULL) {
    movement::Track(NULL);
    return;
  }
#ifndef SERIOUSHL_RUN
  TestTick(pen);
#endif
  movement::Track(pen);
  speedrun::Step(pen);
};

static void HL_OnFrame(CDrawPort *pdp) {
  devhook::Poll();
  speedrun::Frame();
};

static void HL_OnPostDraw(CDrawPort *pdp) {
  if (!GetGameAPI()->IsGameOn()) return;
  CEntity *pen = player::Local();
#ifdef SERIOUSHL_RUN
  run::Draw(pdp);
#else
  weapons::Draw(pdp, pen);
#endif
  DrawSpeed(pdp, pen);
  speedrun::Draw(pdp);
};

static void HL_OnPlayerAction(INDEX iClient, INDEX iPlayer, CPlayerAction &pa, INDEX iResent) {
  const BOOL bFresh = iResent < 0;
#ifndef SERIOUSHL_RUN
  TestAction(pa, bFresh);
#endif
  movement::OnAction(pa, bFresh);
};

static void HL_OnWorldLoad(CWorld *pwo, const CTFileName &fnmWorld) {
  hl::Release(); // a new player entity: the next tick takes him from scratch
#ifndef SERIOUSHL_RUN
  weapons::Reset();
#endif
};

static void HL_OnGameStop(void) {
  hl::Release();
#ifndef SERIOUSHL_RUN
  weapons::Reset();
#endif
};

static void HL_OnGameStart(void) {
  speedrun::OnGameStart();
};

static void HL_OnChangeLevel(void) {
  speedrun::OnChangeLevel();
};

static void HL_OnGameLoad(const CTFileName &fnmSave) {
  speedrun::OnGameLoad();
};

CLASSICSPATCH_PLUGIN_STARTUP(HIniConfig props, PluginEvents_t &events) {
  events.m_processing->OnStep = &HL_OnStep;
  events.m_processing->OnFrame = &HL_OnFrame;
  events.m_rendering->OnPostDraw = &HL_OnPostDraw;
  events.m_packet->OnPlayerAction = &HL_OnPlayerAction;
  events.m_world->OnWorldLoad = &HL_OnWorldLoad;
  events.m_game->OnGameStop = &HL_OnGameStop;
  events.m_game->OnGameStart = &HL_OnGameStart;
  events.m_game->OnChangeLevel = &HL_OnChangeLevel;
  events.m_game->OnGameLoad = &HL_OnGameLoad;

  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Status", "void", &HL_Status);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Where", "void", &HL_Where);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_CloseNetricsa", "void", &HL_CloseNetricsa);
#ifdef SERIOUSHL_RUN
  hl::_settings = rules::HALF_LIFE;
  run::Init(SERIOUSHL_VERSION);
#else
  DeclareUsual();
#endif

  devhook::Init("SERIOUSBRIDGE_DEV");
  // the speedrun tools (both builds; they change nothing in the game): timer, LiveSplit, speedometer
  speedrun::Init();
  _pShell->DeclareSymbol("persistent user INDEX sr_bShowSpeed;", &sr_bShowSpeed);
  const BOOL bHooked = hl::Init();
#ifndef SERIOUSHL_RUN
  // (the speedrun build is the movement only, as its category is)
  if (bHooked) weapons::Init();
#endif
  CPrintF("^cff8800[SeriousHL]^r %s loaded: Half-Life movement %s (autojump %d, bhop cap %d)\n", SERIOUSHL_VERSION,
    !bHooked ? "unavailable" : hl::hl_bEnable ? "on" : "off", hl::_settings.bAutoJump, hl::_settings.bBhopCap);
};

CLASSICSPATCH_PLUGIN_SHUTDOWN(HIniConfig props) {
  speedrun::Shutdown();
  devhook::Shutdown();
  hl::Release();
};
