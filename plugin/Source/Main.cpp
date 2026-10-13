/* SeriousHL - Half-Life's movement in Serious Sam Classic (TFE 1.05 / TSE 1.07), Classics Patch plugin.
 *
 * Sam's player runs, jumps, strafes in the air and bunny hops by Half-Life's rules (HlMove.cpp); Sam's own movement is
 * out of the way (the SeriousBridge core's movement takeover, core/sam/Movement.h). Everything else is Sam's: weapons,
 * monsters, levels, swimming, cutscenes. Part of SeriousBridge: mechanics of other games in Serious Sam.
 */
#include "StdH.h"
#include "HlMove.h"
#include "Movement.h"
#include "Player.h"
#include "Physics.h"
#include "DevHook.h"

#include <stdio.h>

CLASSICSPATCH_DEFINE_PLUGIN(k_EPluginFlagGame, MakeVersion(0, 1, 0),
  "SeriousBridge", "SeriousHL", "Half-Life's movement for Serious Sam's player: air strafing and bunny hopping.");

// test hooks (tools/test_hl.py): keys held for the next ticks, the view turned every tick, fire pressed (intros)
static INDEX hl_iTestTicks = 0;
static FLOAT hl_fTestForward = 0.0f;
static FLOAT hl_fTestSide = 0.0f;
static INDEX hl_bTestJump = FALSE;
static INDEX hl_bTestDuck = FALSE;
static FLOAT hl_fTestTurn = 0.0f;
static INDEX hl_iTestFire = 0;

static const ULONG PLACT_FIRE = 1UL << 0; // Player.es

// ------------------------------------------------------------------
// Console
// ------------------------------------------------------------------
static void HL_Status(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  CPrintF("[SeriousHL] %s\n", hl::Status().str_String);
};

// Tests: where the player is (and an intro or Netricsa in the way); move him by "dx dy dz"; trace ticks to a file.
static void HL_Where(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  devhook::Where();
};

static void HL_CloseNetricsa(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  devhook::CloseNetricsa();
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

// ------------------------------------------------------------------
// Events
// ------------------------------------------------------------------
static void HL_OnStep(void) {
  CEntity *pen = GetGameAPI()->IsGameOn() ? player::Local() : NULL;
  if (pen == NULL) {
    movement::Track(NULL);
    return;
  }
  if (hl_iTestTicks > 0) {
    hl_iTestTicks--;
    if (hl_fTestTurn != 0.0f) ((CPlayerEntity *)pen)->en_plViewpoint.pl_OrientationAngle(1) += hl_fTestTurn;
  }
  movement::Track(pen);
};

static void HL_OnFrame(CDrawPort *pdp) {
  devhook::Poll();
};

static void HL_OnPlayerAction(INDEX iClient, INDEX iPlayer, CPlayerAction &pa, INDEX iResent) {
  const BOOL bFresh = iResent < 0;
  if (hl_iTestTicks > 0) {
    pa.pa_vTranslation = FLOAT3D(hl_fTestSide * 10.0f, hl_bTestJump ? 11.0f : (hl_bTestDuck ? -11.0f : 0.0f), -hl_fTestForward * 10.0f);
  }
  if (bFresh && hl_iTestFire > 0) {
    hl_iTestFire--;
    pa.pa_ulButtons |= PLACT_FIRE;
  }
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
  events.m_packet->OnPlayerAction = &HL_OnPlayerAction;
  events.m_world->OnWorldLoad = &HL_OnWorldLoad;
  events.m_game->OnGameStop = &HL_OnGameStop;

  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Status", "void", &HL_Status);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Where", "void", &HL_Where);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_CloseNetricsa", "void", &HL_CloseNetricsa);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Move", "CTString", &HL_Move);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Trace", "INDEX", &HL_Trace);

  _pShell->DeclareSymbol("persistent user INDEX hl_bEnable;", &hl::hl_bEnable);
  _pShell->DeclareSymbol("persistent user INDEX hl_bAutoJump;", &hl::hl_bAutoJump);
  _pShell->DeclareSymbol("persistent user INDEX hl_bBhopCap;", &hl::hl_bBhopCap);
  _pShell->DeclareSymbol("persistent user INDEX hl_bFallDamage;", &hl::hl_bFallDamage);
  _pShell->DeclareSymbol("persistent user INDEX hl_iFps;", &hl::hl_iFps);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fUnit;", &hl::hl_fUnit);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fMaxSpeed;", &hl::hl_fMaxSpeed);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fAccelerate;", &hl::hl_fAccelerate);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fAirAccelerate;", &hl::hl_fAirAccelerate);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fFriction;", &hl::hl_fFriction);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fEdgeFriction;", &hl::hl_fEdgeFriction);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fStopSpeed;", &hl::hl_fStopSpeed);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fGravity;", &hl::hl_fGravity);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fJumpHeight;", &hl::hl_fJumpHeight);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fMaxVelocity;", &hl::hl_fMaxVelocity);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fForwardSpeed;", &hl::hl_fForwardSpeed);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fSideSpeed;", &hl::hl_fSideSpeed);

  _pShell->DeclareSymbol("user INDEX hl_iTestTicks;", &hl_iTestTicks);
  _pShell->DeclareSymbol("user FLOAT hl_fTestForward;", &hl_fTestForward);
  _pShell->DeclareSymbol("user FLOAT hl_fTestSide;", &hl_fTestSide);
  _pShell->DeclareSymbol("user INDEX hl_bTestJump;", &hl_bTestJump);
  _pShell->DeclareSymbol("user INDEX hl_bTestDuck;", &hl_bTestDuck);
  _pShell->DeclareSymbol("user FLOAT hl_fTestTurn;", &hl_fTestTurn);
  _pShell->DeclareSymbol("user INDEX hl_iTestFire;", &hl_iTestFire);

  devhook::Init("SERIOUSBRIDGE_DEV");
  const BOOL bHooked = hl::Init();
  CPrintF("^cff8800[SeriousHL]^r loaded: Half-Life movement %s (hl_bEnable; hl_bAutoJump %d, hl_bBhopCap %d)\n",
    !bHooked ? "unavailable" : hl::hl_bEnable ? "on" : "off", hl::hl_bAutoJump, hl::hl_bBhopCap);
};

CLASSICSPATCH_PLUGIN_SHUTDOWN(HIniConfig props) {
  devhook::Shutdown();
  hl::Release();
};
