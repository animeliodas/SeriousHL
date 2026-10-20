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
#include "Hev.h"
#include "ViewModel.h"
#include "Hud.h"
#include "PlayerSounds.h"
#include "HlMove.h"
#include "Rules.h"
#include "Run.h"
#include "Weapons.h"
#include "Movement.h"
#include "Player.h"
#include "Physics.h"
#include "DevHook.h"

#include <stdio.h>

#define SERIOUSHL_VERSION "0.5.0"

CLASSICSPATCH_DEFINE_PLUGIN(k_EPluginFlagGame, MakeVersion(0, 5, 0),
  "SeriousBridge", "SeriousHL", "Half-Life's movement and weapons for Serious Sam's player.");

// ------------------------------------------------------------------
// Console
// ------------------------------------------------------------------
static void HL_Status(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
#ifdef SERIOUSHL_RUN
  CPrintF("[SeriousHL] %s; %s\n", run::Mark(), hl::Status().str_String);
#else
  CPrintF("[SeriousHL] %s\n", hl::Status().str_String);
  CPrintF("[SeriousHL] %s\n", weapons::Status(player::Local()).str_String);
  CPrintF("[SeriousHL] %s\n", hev::Status().str_String);
  CPrintF("[SeriousHL] %s\n", viewmodel::Status().str_String);
  CPrintF("[SeriousHL] %s\n", hud::Status().str_String);
  CPrintF("[SeriousHL] %s\n", playersounds::Status().str_String);
  CEntity *pen = player::Local();
  const FLOAT *pfArmor = pen != NULL ? (FLOAT *)player::Prop(pen, player::PROP_ARMOR) : NULL;
  if (pen != NULL) CPrintF("[SeriousHL] health %.1f, armour %.1f\n", ((CLiveEntity *)pen)->en_fHealth, pfArmor != NULL ? *pfArmor : 0.0f);
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

// --- water: the level's biggest swimmable sector (levels here have gravity down -y)
static CBrushSector *BiggestWater(FLOATaabbox3D &boxWater) {
  CWorld *pwo = IWorld::GetWorld();
  FLOAT fBest = 0.0f;
  CBrushSector *pbscBest = NULL;
  FOREACHINDYNAMICCONTAINER(pwo->wo_cenEntities, CEntity, iten) {
    if (iten->GetRenderType() != CEntity::RT_BRUSH || iten->en_pbrBrush == NULL) continue;
    CBrushMip *pbm = iten->en_pbrBrush->GetFirstMip();
    if (pbm == NULL) continue;
    FOREACHINDYNAMICARRAY(pbm->bm_abscSectors, CBrushSector, itbsc) {
      const INDEX iContent = itbsc->GetContentType();
      if (iContent == 0 || iContent >= pwo->wo_actContentTypes.Count()) continue;
      if (!(pwo->wo_actContentTypes[iContent].ct_ulFlags & CTF_SWIMABLE)) continue;
      const FLOAT3D vSize = itbsc->bsc_boxBoundingBox.Size();
      const FLOAT fVolume = vSize(1) * vSize(2) * vSize(3);
      if (fVolume > fBest) {
        fBest = fVolume;
        boxWater = itbsc->bsc_boxBoundingBox;
        pbscBest = &*itbsc;
      }
    }
  }
  return pbscBest;
};

static BOOL InSector(CBrushSector *pbsc, const FLOAT3D &v) {
  const DOUBLE3D vD = FLOATtoDOUBLE(v);
  const DOUBLEbspnode3D *pbn = pbsc->bsc_bspBSPTree.bt_pbnRoot;
  while (pbn != NULL && pbn->bn_bnlLocation == BNL_BRANCH) pbn = pbn->PointDistance(vD) >= 0.0 ? pbn->bn_pbnFront : pbn->bn_pbnBack;
  return pbn != NULL && pbn->bn_bnlLocation == BNL_INSIDE;
};

// how far a ray goes before it hits a brush (the whole world tested: no entity to start from)
static FLOAT Clear(const FLOAT3D &vFrom, const FLOAT3D &vDir, FLOAT fMax, CBrushPolygon **ppbpo = NULL) {
  CCastRay cr(NULL, vFrom, vFrom + vDir * fMax);
  cr.cr_ttHitModels = CCastRay::TT_NONE;
  cr.cr_bHitPortals = cr.cr_bHitTranslucentPortals = FALSE;
  cr.cr_bPhysical = TRUE;
  IWorld::GetWorld()->CastRay(cr);
  if (ppbpo != NULL) *ppbpo = cr.cr_pbpoBrushPolygon;
  return cr.cr_penHit != NULL ? cr.cr_fHitDistance : fMax;
};

static void Place(CEntity *pen, const FLOAT3D &vFeet, ANGLE aHeading) {
  const CPlacement3D pl(vFeet, ANGLE3D(aHeading, 0, 0));
  ((CPlayerEntity *)pen)->en_plViewpoint.pl_OrientationAngle = ANGLE3D(0, 0, 0);
  const FLOAT3D vStill(0, 0, 0);
  physics::PlaceNow(pen, pl, &vStill);
};

// in the biggest water, the feet this deep under its top ("2.5"; "-8": over it), at its most open spot (room around,
// under and over), facing its longest way
static void HL_GoWater(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strArgs = *NEXT_ARG(CTString *);
  CEntity *pen = player::Local();
  FLOATaabbox3D box;
  FLOAT fDepth = 2.5f;
  sscanf(strArgs.str_String, "%f", &fDepth);
  CBrushSector *pbsc = pen != NULL ? BiggestWater(box) : NULL;
  if (pbsc == NULL) {
    CPrintF("[SeriousHL] water: none\n");
    return;
  }
  const FLOAT fTop = box.Max()(2);
  const FLOAT3D vUp(0, 1, 0);
  FLOAT fBest = -1.0f, fBestHeading = 0.0f;
  FLOAT3D vBest(0, 0, 0);
  for (INDEX ix = 1; ix < 7; ix++) {
    for (INDEX iz = 1; iz < 7; iz++) {
      const FLOAT3D vFeet(Lerp(box.Min()(1), box.Max()(1), ix / 7.0f), fTop - fDepth, Lerp(box.Min()(3), box.Max()(3), iz / 7.0f));
      // in the water there (deep under it: from the surface down)
      const FLOAT3D vInWater = fDepth > 0.0f ? vFeet + vUp * 0.05f : FLOAT3D(vFeet(1), fTop - 0.3f, vFeet(3));
      if (!InSector(pbsc, vInWater)) continue;
      const FLOAT fDown = Min(Clear(vInWater, -vUp, 6.0f), 6.0f);
      const FLOAT fUp = fDepth < 0.0f ? Clear(vInWater, vUp, -fDepth + 3.0f) : 3.0f;
      if (fDepth < 0.0f && fUp < -fDepth + 2.5f) continue;
      FLOAT fRound = 1e6f, fLongest = 0.0f, fHeading = 0.0f;
      for (INDEX i = 0; i < 8; i++) {
        const FLOAT a = i * 45.0f;
        const FLOAT3D vDir(-Sin(a), 0, -Cos(a));
        const FLOAT fWay = Clear(vInWater + vUp * 1.0f, vDir, 16.0f);
        fRound = Min(fRound, fWay);
        if (fWay > fLongest) {
          fLongest = fWay;
          fHeading = a;
        }
      }
      const FLOAT fScore = Min(fDown, 4.0f) + Min(fRound, 4.0f) + fLongest * 0.2f;
      if (fScore > fBest) {
        fBest = fScore;
        vBest = vFeet;
        fBestHeading = fHeading;
      }
    }
  }
  if (fBest < 0.0f) {
    CPrintF("[SeriousHL] water: no spot\n");
    return;
  }
  Place(pen, vBest, fBestHeading);
  CPrintF("[SeriousHL] water: box %.1f %.1f %.1f - %.1f %.1f %.1f, feet at %.2f %.2f %.2f, heading %.0f, in water %d\n",
    box.Min()(1), box.Min()(2), box.Min()(3), box.Max()(1), box.Max()(2), box.Max()(3), vBest(1), vBest(2), vBest(3),
    fBestHeading, InSector(pbsc, vBest + vUp * 0.05f));
};

// waist deep at the biggest water's wall that has room over it (Half-Life's water jump), facing it; the wall looked
// for this deep under the surface ("0.1")
static INDEX _bEdgeDebug = FALSE;
static void HL_WaterEdge(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  const CTString &strArgs = *NEXT_ARG(CTString *);
  FLOAT fUnder = 0.1f;
  sscanf(strArgs.str_String, "%f", &fUnder);
  CEntity *pen = player::Local();
  FLOATaabbox3D box;
  if (pen == NULL || BiggestWater(box) == NULL) {
    CPrintF("[SeriousHL] water edge: no water\n");
    return;
  }
  const FLOAT fUnit = hl::_settings.fUnit;
  const FLOAT fTop = box.Max()(2);
  // waist deep: the wall's trace (8 units over his box's middle) just under the surface - the water between the middle
  // and the eyes 28 units over it
  const CCollisionInfo *pci = pen->en_pciCollisionInfo;
  const FLOAT fBoxMiddle = pci != NULL ? (pci->ci_fMinHeight + pci->ci_fMaxHeight) * 0.5f : 0.9f;
  const FLOAT fFeetY = fTop - fUnder - rules::WATERJUMP_AT * fUnit - fBoxMiddle;
  if (_bEdgeDebug) CPrintF("  water edge: top %.2f, feet %.2f, box %.2f..%.2f, centre %.2f %.2f %.2f\n", fTop, fFeetY,
    pci != NULL ? pci->ci_fMinHeight : 0.0f, pci != NULL ? pci->ci_fMaxHeight : 0.0f, box.Center()(1), box.Center()(2), box.Center()(3));
  const FLOAT3D vC = box.Center();
  // (rays go through the sectors from those of the entity they're cast for: he goes to the water first)
  Place(pen, FLOAT3D(vC(1), fFeetY, vC(3)), pen->GetPlacement().pl_OrientationAngle(1));
  FLOAT fBest = 1e6f;
  FLOAT3D vBestFeet(0, 0, 0), vBestDir(0, 0, 0);
  for (INDEX i = 0; i < 32; i++) {
    const FLOAT a = i * 360.0f / 32;
    const FLOAT3D vDir(-Sin(a), 0, -Cos(a));
    // the wall at the waist (8 over the middle), from the middle of the water
    const FLOAT fMiddle = fFeetY + fBoxMiddle;
    const FLOAT3D vFrom(vC(1), fMiddle + rules::WATERJUMP_AT * fUnit, vC(3));
    const FLOAT fTopAt = fMiddle + rules::WATERJUMP_TOP * fUnit;
    CCastRay cr(pen, vFrom, vFrom + vDir * 200.0f);
    cr.cr_ttHitModels = CCastRay::TT_NONE;
    cr.cr_bHitPortals = cr.cr_bHitTranslucentPortals = FALSE;
    cr.cr_bPhysical = TRUE;
    IWorld::GetWorld()->CastRay(cr);
    if (cr.cr_penHit == NULL || cr.cr_pbpoBrushPolygon == NULL) { if (_bEdgeDebug) CPrintF("  %d: no wall\n", i); continue; }
    const FLOAT3D vN = FLOAT3D(cr.cr_pbpoBrushPolygon->bpo_pbplPlane->bpl_plAbsolute);
    if (Abs(vN(2)) >= 0.1f || (vN % vDir) > -0.7f) { if (_bEdgeDebug) CPrintF("  %d: wall at %.2f not upright (%.2f %.2f %.2f)\n", i, cr.cr_fHitDistance, vN(1), vN(2), vN(3)); continue; }
    const FLOAT fWall = cr.cr_fHitDistance;
    // nothing at the hull's top, a good way past the wall
    const FLOAT3D vFromTop(vC(1), fTopAt, vC(3));
    CCastRay crTop(pen, vFromTop, vFromTop + vDir * (fWall + 1.5f));
    crTop.cr_ttHitModels = CCastRay::TT_NONE;
    crTop.cr_bHitPortals = crTop.cr_bHitTranslucentPortals = FALSE;
    crTop.cr_bPhysical = TRUE;
    IWorld::GetWorld()->CastRay(crTop);
    if (crTop.cr_penHit != NULL) { if (_bEdgeDebug) CPrintF("  %d: wall at %.2f, the top blocked at %.2f\n", i, fWall, crTop.cr_fHitDistance); continue; }
    // (and room to stand up over the ledge: a hull's height more over that)
    const FLOAT3D vFromOver(vC(1), fTopAt + 72.0f * fUnit, vC(3));
    if (Clear(vFromOver, vDir, fWall + 1.5f) < fWall + 1.5f) { if (_bEdgeDebug) CPrintF("  %d: wall at %.2f, no room over the ledge\n", i, fWall); continue; }
    // a spot 0.6 m from the wall, its floor under the feet
    const FLOAT3D vFeet = FLOAT3D(vC(1), fFeetY, vC(3)) + vDir * (fWall - 0.6f);
    CCastRay crDown(pen, vFeet + FLOAT3D(0, 0.1f, 0), vFeet - FLOAT3D(0, 1.0f, 0));
    crDown.cr_ttHitModels = CCastRay::TT_NONE;
    crDown.cr_bHitPortals = crDown.cr_bHitTranslucentPortals = FALSE;
    crDown.cr_bPhysical = TRUE;
    IWorld::GetWorld()->CastRay(crDown);
    if (crDown.cr_penHit != NULL || !movement::PointInWater(pen, vFeet)) { if (_bEdgeDebug) CPrintF("  %d: wall at %.2f, floor %d water %d\n", i, fWall, crDown.cr_penHit != NULL, movement::PointInWater(pen, vFeet)); continue; }
    if (fWall < fBest) {
      fBest = fWall;
      vBestFeet = vFeet;
      vBestDir = vDir;
    }
  }
  if (fBest >= 1e6f) {
    CPrintF("[SeriousHL] water edge: no wall with room over it\n");
    return;
  }
  ANGLE3D aDir;
  DirectionVectorToAngles(vBestDir, aDir);
  Place(pen, vBestFeet, aDir(1));
  CPrintF("[SeriousHL] water edge: feet at %.2f %.2f %.2f, the surface %.2f m over them, the wall %.2f m ahead\n",
    vBestFeet(1), vBestFeet(2), vBestFeet(3), fTop - vBestFeet(2), 0.6f);
};

// the water around him: the engine's own view (contents, immersion over its collision box) and the points tested
static void HL_WaterProbe(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  CEntity *pen = player::Local();
  if (pen == NULL) return;
  CMovableEntity *pmen = (CMovableEntity *)pen;
  CWorld *pwo = pen->GetWorld();
  const FLOAT3D vFeet = pen->GetPlacement().pl_PositionVector;
  CPrintF("[SeriousHL] water probe: feet y %.3f; engine: up %d (flags %x), down %d (flags %x), immersion %.3f, box %.3f..%.3f; above feet %.3f m\n",
    vFeet(2), pmen->en_iUpContent, pwo->wo_actContentTypes[pmen->en_iUpContent].ct_ulFlags, pmen->en_iDnContent,
    pwo->wo_actContentTypes[pmen->en_iDnContent].ct_ulFlags, pmen->en_fImmersionFactor,
    pen->en_pciCollisionInfo != NULL ? pen->en_pciCollisionInfo->ci_fMinHeight : -1.0f,
    pen->en_pciCollisionInfo != NULL ? pen->en_pciCollisionInfo->ci_fMaxHeight : -1.0f, movement::WaterAbove(pen, 3.0f));
  CTString str;
  for (INDEX i = 0; i <= 12; i++) str += movement::PointInWater(pen, vFeet + FLOAT3D(0, i * 0.25f, 0)) ? "W" : ".";
  CPrintF("[SeriousHL] water probe: 0..3 m by 0.25: %s\n", str.str_String);
  {FOREACHSRCOFDST(pen->en_rdSectors, CBrushSector, bsc_rsEntities, pbsc)
    const INDEX iContent = pbsc->GetContentType();
    const FLOATaabbox3D &box = pbsc->bsc_boxBoundingBox;
    const CContentType &ct = pwo->wo_actContentTypes[iContent];
    CPrintF("[SeriousHL]   sector '%s' content %d '%s' (flags %x, density %.0f, swim damage %.1f every %.1f s, drowning %.1f), first mip %d, y %.2f..%.2f\n",
      pbsc->bsc_strName.str_String, iContent, ct.ct_strName.str_String, ct.ct_ulFlags, ct.ct_fDensity, ct.ct_fSwimDamageAmount,
      ct.ct_tmSwimDamageFrequency, ct.ct_fDrowningDamageAmount, pbsc->bsc_pbmBrushMip->IsFirstMip(), box.Min()(2), box.Max()(2));
  ENDFOR;}
};

static void HL_DumpAnims(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  weapons::DumpAnims(player::Local());
};

static void HL_TestDamage(SHELL_FUNC_ARGS) {
  BEGIN_SHELL_FUNC;
  hev::TestDamage(player::Local(), NEXT_ARG(CTString *)->str_String);
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

static void DeclareUsual(void) {
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Move", "CTString", &HL_Move);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Trace", "INDEX", &HL_Trace);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_TestKeys", "CTString", &HL_TestKeys);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_DumpAnims", "void", &HL_DumpAnims);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_SpawnTarget", "CTString", &HL_SpawnTarget);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Probe", "void", &HL_Probe);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_Aim", "CTString", &HL_Aim);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_TestDamage", "CTString", &HL_TestDamage);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_GoWater", "CTString", &HL_GoWater);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_WaterEdge", "CTString", &HL_WaterEdge);
  _pShell->DeclareSymbol("user INDEX hl_bEdgeDebug;", &_bEdgeDebug);
  ClassicsPlugins()->RegisterMethod(TRUE, "void", "hl_WaterProbe", "void", &HL_WaterProbe);
  _pShell->DeclareSymbol("persistent user INDEX hl_bWeapons;", &weapons::hl_bWeapons);
  _pShell->DeclareSymbol("persistent user INDEX hl_iAltFireKey;", &weapons::hl_iAltFireKey);
  _pShell->DeclareSymbol("persistent user INDEX hl_bHevArmor;", &hev::hl_bHevArmor);
  _pShell->DeclareSymbol("persistent user INDEX hl_bViewModels;", &viewmodel::hl_bViewModels);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fViewModelFov;", &viewmodel::hl_fViewModelFov);
  _pShell->DeclareSymbol("user FLOAT hl_fMuzzleFlashTime;", &viewmodel::hl_fMuzzleFlashTime);
  _pShell->DeclareSymbol("persistent user INDEX hl_bHud;", &hud::hl_bHud);
  _pShell->DeclareSymbol("persistent user INDEX hl_bPlayerSounds;", &playersounds::hl_bPlayerSounds);
  _pShell->DeclareSymbol("persistent user FLOAT hl_fHudScale;", &hud::hl_fHudScale);
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
  if (!GetGameAPI()->IsGameOn()) return;
  CEntity *pen = player::Local();
#ifdef SERIOUSHL_RUN
  run::Draw(pdp);
#else
  hud::Draw(pdp, pen);
  if (!hud::Active()) weapons::Draw(pdp, pen);
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
#ifndef SERIOUSHL_RUN
  weapons::Reset();
  playersounds::Release();
#endif
};

static void HL_OnGameStop(void) {
  hl::Release();
#ifndef SERIOUSHL_RUN
  weapons::Reset();
  playersounds::Release();
#endif
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
  // (the timer, LiveSplit and the speedometer: SeriousSpeedrun, a plugin of its own)
  const BOOL bHooked = hl::Init();
#ifndef SERIOUSHL_RUN
  // (the speedrun build is the movement only, as its category is)
  if (bHooked) {
    weapons::Init();
    hev::Init();
    viewmodel::Init();
    hud::Init();
    playersounds::Init();
    hl::_pTickHook = &playersounds::Tick;
  }
#endif
  CPrintF("^cff8800[SeriousHL]^r %s loaded: Half-Life movement %s (autojump %d, bhop cap %d)\n", SERIOUSHL_VERSION,
    !bHooked ? "unavailable" : hl::hl_bEnable ? "on" : "off", hl::_settings.bAutoJump, hl::_settings.bBhopCap);
};

CLASSICSPATCH_PLUGIN_SHUTDOWN(HIniConfig props) {
  devhook::Shutdown();
  hl::Release();
#ifndef SERIOUSHL_RUN
  weapons::FreeSounds();
  viewmodel::Shutdown();
  hud::Shutdown();
#endif
};
