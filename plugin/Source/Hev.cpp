/* SeriousHL - the HEV suit: Half-Life's armour on Sam's player. See Hev.h. */
#include "StdH.h"
#include "Hev.h"
#include "Movement.h"
#include "Player.h"
#include "SBridge.h"

#include <GameV/SessionProperties.h>
#include <Extras/XGizmo/Objects/StructPtr.h>
#include <stdio.h>

namespace hev {

INDEX hl_bHevArmor = TRUE;

static const FLOAT ARMOR_RATIO = 0.2f; // what of the damage still goes to health
static const FLOAT ARMOR_BONUS = 0.5f; // armour used per point of damage it stops

typedef void (CEntity::*CReceiveDamage)(CEntity *, enum DamageType, FLOAT, const FLOAT3D &, const FLOAT3D &);
static CReceiveDamage _pReceiveDamage = NULL;
static BOOL _bHooked = FALSE;
static BOOL _bArmorTakes = FALSE; // SelfDamage() under way
static ULONG _ctHits = 0;
static FLOAT _fLastHealth = 0, _fLastArmor = 0;

// Sam's difficulty on the player's damage (Player.es: not one's own when it'd make it bigger)
static FLOAT Strength(CEntity *pen, CEntity *penInflictor) {
  const CSessionProperties *psp = (const CSessionProperties *)_pNetwork->GetSessionProperties();
  const FLOAT fStrength = psp != NULL ? psp->sp_fDamageStrength : 1.0f;
  return (fStrength > 1.0f && penInflictor == pen) ? 1.0f : fStrength;
};

class CPlayerHook : public CEntity {
public:
  void P_ReceiveDamage(CEntity *penInflictor, enum DamageType dmt, FLOAT fDamage, const FLOAT3D &vHit, const FLOAT3D &vDirection) {
    FLOAT *pfArmor = (FLOAT *)player::Prop(this, player::PROP_ARMOR);
    const FLOAT fStrength = Strength(this, penInflictor);
    if (!hl_bHevArmor || pfArmor == NULL || this != movement::Tracked() || fDamage <= 0.0f || fStrength <= 0.0f) {
      (this->*_pReceiveDamage)(penInflictor, dmt, fDamage, vHit, vDirection);
      return;
    }
    const FLOAT fArmorBefore = *pfArmor;
    // a fall (the movement's own: one's own impact) - Half-Life's armour doesn't help, nor does the difficulty change it
    if (dmt == DMT_IMPACT && penInflictor == this && !_bArmorTakes) {
      *pfArmor = 0.0f;
      (this->*_pReceiveDamage)(penInflictor, dmt, fDamage / Min(fStrength, 1.0f), vHit, vDirection);
      *pfArmor = fArmorBefore;
      return;
    }
    // drowning: Sam's code doesn't use the armour either
    if (dmt == DMT_DROWNING || fArmorBefore <= 0.0f) {
      (this->*_pReceiveDamage)(penInflictor, dmt, fDamage, vHit, vDirection);
      return;
    }
    // Half-Life's split of the damage as the difficulty made it
    const FLOAT fScaled = fDamage * fStrength;
    FLOAT fHealth = fScaled * ARMOR_RATIO;
    FLOAT fArmorUsed = (fScaled - fHealth) * ARMOR_BONUS;
    if (fArmorUsed > fArmorBefore) {
      fArmorUsed = fArmorBefore;
      fHealth = fScaled - fArmorUsed / ARMOR_BONUS;
    }
    // Sam's code gets only the health part, with no armour in the way of it (and its push is of that part, as Half-Life's)
    const FLOAT fHealthBefore = ((CLiveEntity *)this)->en_fHealth;
    *pfArmor = 0.0f;
    (this->*_pReceiveDamage)(penInflictor, dmt, fHealth / fStrength, vHit, vDirection);
    // (Sam's code didn't take it - god mode, auto actions, one's own rocket on easy: no armour used)
    const BOOL bTaken = ((CLiveEntity *)this)->en_fHealth != fHealthBefore || fHealth <= 0.0f;
    *pfArmor = bTaken ? fArmorBefore - fArmorUsed : fArmorBefore;
    _ctHits++;
    _fLastHealth = fHealth;
    _fLastArmor = bTaken ? fArmorUsed : 0.0f;
  };
};

BOOL Init(void) {
  StructPtr ptr(ClassicsCore_GetEntitiesSymbol("?ReceiveDamage@CPlayer@@UAEXPAVCEntity@@W4DamageType@@MABV?$Vector@M$02@@2@Z"));
  if (ptr.iAddress == NULL) {
    CPrintF(SB_TAG " (CPlayer::ReceiveDamage isn't in the Entities library: Sam's armour stays)\n");
    return FALSE;
  }
  _pReceiveDamage = ptr(CReceiveDamage());
  HFuncPatch hPatch = CreatePatch(_pReceiveDamage, &CPlayerHook::P_ReceiveDamage, "SeriousHL: CPlayer::ReceiveDamage()");
  _bHooked = hPatch != NULL && hPatch->IsPatched();
  return _bHooked;
};

void SelfDamage(CEntity *pen, FLOAT fDamage) {
  _bArmorTakes = TRUE;
  pen->InflictDirectDamage(pen, pen, DMT_IMPACT, fDamage, pen->GetPlacement().pl_PositionVector, FLOAT3D(0, 0, 0));
  _bArmorTakes = FALSE;
};

CTString Status(void) {
  CTString str;
  str.PrintF("HEV armour %s; %u hits through it (last: %.1f to health, %.1f armour used)",
    !_bHooked ? "unavailable" : hl_bHevArmor ? "Half-Life's" : "Sam's", _ctHits, _fLastHealth, _fLastArmor);
  return str;
};

void TestDamage(CEntity *pen, const char *strArgs) {
  // "<damage> <bullet|fall|shock> <health> <armour>"
  FLOAT fDamage = 0, fHealth = 100, fArmor = 100;
  char strKind[16] = "bullet";
  FLOAT *pfArmor = pen != NULL ? (FLOAT *)player::Prop(pen, player::PROP_ARMOR) : NULL;
  if (pfArmor == NULL || sscanf(strArgs, "%f %15s %f %f", &fDamage, strKind, &fHealth, &fArmor) < 1) return;
  // someone else to do it: the world's first brush
  CEntity *penFrom = NULL;
  FOREACHINDYNAMICCONTAINER(IWorld::GetWorld()->wo_cenEntities, CEntity, iten) {
    if (iten->GetRenderType() == CEntity::RT_BRUSH) {
      penFrom = &*iten;
      break;
    }
  }
  CLiveEntity *plen = (CLiveEntity *)pen;
  plen->en_fHealth = fHealth;
  *pfArmor = fArmor;
  const FLOAT3D vHit = pen->GetPlacement().pl_PositionVector;
  if (strcmp(strKind, "fall") == 0) {
    pen->InflictDirectDamage(pen, pen, DMT_IMPACT, fDamage, vHit, FLOAT3D(0, -1, 0));
  } else if (strcmp(strKind, "shock") == 0) {
    SelfDamage(pen, fDamage);
  } else if (penFrom != NULL) {
    penFrom->InflictDirectDamage(pen, penFrom, DMT_BULLET, fDamage, vHit, FLOAT3D(0, 0, 0));
  }
  CPrintF(SB_TAG " test damage %.1f %s: health %.2f -> %.2f, armour %.2f -> %.2f, difficulty %.2f\n", fDamage, strKind,
    fHealth, plen->en_fHealth, fArmor, *pfArmor, Strength(pen, NULL));
};

}; // namespace
