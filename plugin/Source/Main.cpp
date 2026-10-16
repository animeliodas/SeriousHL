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
#include "Movement.h"
#include "Player.h"
#include "Physics.h"
#include "DevHook.h"

#include <stdio.h>

#define SERIOUSHL_VERSION "0.2.0"

CLASSICSPATCH_DEFINE_PLUGIN(k_EPluginFlagGame, MakeVersion(0, 2, 0),
  "SeriousBridge", "SeriousHL", "Half-Life's movement for Serious Sam's player: air strafing, bunny hopping, ducking.");

// ------------------------------------------------------------------
// Console
// ------------------------------------------------------------------
static void HL_Status(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
#ifdef SERIOUSHL_RUN
  CPrintF("[SeriousHL] %s; %s\n", run::Mark(), hl::Status().str_String);
#else
  CPrintF("[SeriousHL] %s\n", hl::Status().str_String);
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
// many actions they're held
struct TestKeys { FLOAT fForward, fSide; BOOL bJump, bDuck; INDEX ctActions; };
static TestKeys _atkScript[32];
static INDEX _ctScript = 0, _iScript = 0;

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

static void HL_Trace(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  hl::TraceTicks(NEXT_ARG(INDEX));
};

static void TestTick(CEntity *pen) {
  if (hl_fTestPitch != 0.0f) ((CPlayerEntity *)pen)->en_plViewpoint.pl_OrientationAngle(2) = hl_fTestPitch;
  if (hl_iTestTicks <= 0) return;
  hl_iTestTicks--;
  if (hl_fTestTurn != 0.0f) ((CPlayerEntity *)pen)->en_plViewpoint.pl_OrientationAngle(1) += hl_fTestTurn;
};

static void TestAction(CPlayerAction &pa, BOOL bFresh) {
  if (hl_iTestTicks > 0) {
    pa.pa_vTranslation = FLOAT3D(hl_fTestSide * 10.0f, hl_bTestJump ? 11.0f : (hl_bTestDuck ? -11.0f : 0.0f), -hl_fTestForward * 10.0f);
  }
  if (_iScript < _ctScript) {
    TestKeys &tk = _atkScript[_iScript];
    pa.pa_vTranslation = FLOAT3D(tk.fSide * 10.0f, tk.bJump ? 11.0f : (tk.bDuck ? -11.0f : 0.0f), -tk.fForward * 10.0f);
    if (bFresh && --tk.ctActions <= 0) _iScript++;
  }
  if (bFresh && hl_iTestFire > 0) {
    hl_iTestFire--;
    pa.pa_ulButtons |= PLACT_FIRE;
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

static void DeclareUsual(void) {
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Move", "CTString", &HL_Move);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Trace", "INDEX", &HL_Trace);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_TestKeys", "CTString", &HL_TestKeys);

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
};

static void HL_OnFrame(CDrawPort *pdp) {
  devhook::Poll();
};

static void HL_OnPostDraw(CDrawPort *pdp) {
#ifdef SERIOUSHL_RUN
  if (GetGameAPI()->IsGameOn()) run::Draw(pdp);
#endif
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
};

static void HL_OnGameStop(void) {
  hl::Release();
};

CLASSICSPATCH_PLUGIN_STARTUP(HIniConfig props, PluginEvents_t &events) {
  events.m_processing->OnStep = &HL_OnStep;
  events.m_processing->OnFrame = &HL_OnFrame;
  events.m_rendering->OnPostDraw = &HL_OnPostDraw;
  events.m_packet->OnPlayerAction = &HL_OnPlayerAction;
  events.m_world->OnWorldLoad = &HL_OnWorldLoad;
  events.m_game->OnGameStop = &HL_OnGameStop;

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
  const BOOL bHooked = hl::Init();
  CPrintF("^cff8800[SeriousHL]^r %s loaded: Half-Life movement %s (autojump %d, bhop cap %d)\n", SERIOUSHL_VERSION,
    !bHooked ? "unavailable" : hl::hl_bEnable ? "on" : "off", hl::_settings.bAutoJump, hl::_settings.bBhopCap);
};

CLASSICSPATCH_PLUGIN_SHUTDOWN(HIniConfig props) {
  devhook::Shutdown();
  hl::Release();
};
