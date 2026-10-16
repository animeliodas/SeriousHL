/* SeriousHL - the HEV suit: Half-Life's armour on Sam's player.
 *
 * Half-Life's CBasePlayer::TakeDamage: armour takes 80% of the damage (ARMOR_RATIO 0.2 goes to health), each point of
 * armour stopping two (ARMOR_BONUS 0.5); when it runs out the rest goes to health. It doesn't help against falls and
 * drowning. Sam's own: two thirds on armour, one third on health, a point for a point. Patched in CPlayer::ReceiveDamage
 * (the Entities library): the health part goes on to Sam's own code with no armour in the way, so everything else of
 * it (difficulty, god mode, the push, blood, death) stays Sam's. The difficulty scales the damage before the armour
 * splits it, as Sam's does; falls (the movement's own damage) are Half-Life's at every difficulty.
 */
#ifndef SERIOUSHL_HEV_H
#define SERIOUSHL_HEV_H

#include "StdH.h"

namespace hev {

extern INDEX hl_bHevArmor; // 1: Half-Life's armour; 0: Sam's

// Plugin startup: the patch.
BOOL Init(void);
// Damage to oneself that the armour takes, with no push (the gauss overcharging: Half-Life's DMG_SHOCK).
void SelfDamage(CEntity *pen, FLOAT fDamage);
CTString Status(void);

// Test hook: "<damage> <bullet|fall|shock> <health> <armour>" - health and armour set, the damage done, both told.
void TestDamage(CEntity *pen, const char *strArgs);

}; // namespace

#endif
