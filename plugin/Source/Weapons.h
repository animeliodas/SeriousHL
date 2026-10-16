/* SeriousHL - Half-Life's weapons on Sam's guns.
 *
 * Sam's guns stay what you see and hear (models, animations, sounds, bullet holes, blood, rockets, grenades); what
 * they do is Half-Life's: damage, spread, rate of fire, magazines and reloading, the second fire on the right mouse
 * button. Sam's own firing and reloading are taken out of every action (the core's action filter, Movement.h) while a
 * Half-Life gun is in hand; Sam's weapon switching stays.
 *
 *   knife (TSE chainsaw) -> crowbar      colt -> .357 Python          double colt -> Glock 9mm
 *   shotguns -> shotgun (6 / 12 pellets) tommygun -> MP5 (+ grenade)   rocket launcher -> RPG
 *   grenade launcher -> hand grenades     laser -> gauss (charged shot pushes you back)
 *   minigun -> egon (the beam)            cannon -> satchel charges     TSE sniper rifle -> crossbow (Sam's scope)
 * TSE's flamer stays Sam's.
 *
 * Ammo: what Sam picks up is the reserve (bullets for the Glock, MP5 and egon, shells, rockets, grenades, electricity
 * for the gauss, cannon balls for satchels, sniper bullets for bolts; the Python has Sam's colt's endless ammo); the
 * magazines are Half-Life's. Numbers: the Half-Life SDK's skill defaults (sk_plr_*) and its weapons' fire rates.
 * Sounds: Half-Life's own when the installer took them from the player's Half-Life, else Sam's.
 */
#ifndef SERIOUSHL_WEAPONS_H
#define SERIOUSHL_WEAPONS_H

#include "StdH.h"

namespace weapons {

extern INDEX hl_bWeapons;     // 1: Half-Life's weapons on Sam's guns; 0: Sam's
extern INDEX hl_iAltFireKey;  // the second fire's key (Windows virtual-key code; right mouse button)
extern INDEX hl_iTestAlt;     // tests: the second fire held for this many actions

// Plugin startup: find Sam's weapon functions and install the action filter. FALSE: Sam's weapons stay.
BOOL Init(void);
// A level, a new game: magazines full, nothing charging.
void Reset(void);
// After the game is drawn: the magazine next to Sam's ammo.
void Draw(CDrawPort *pdp, CEntity *penPlayer);
CTString Status(CEntity *penPlayer);
// Dev: the current gun's model's animations into the console.
void DumpAnims(CEntity *penPlayer);
// Plugin shutdown: Half-Life's sounds let go.
void FreeSounds(void);

}; // namespace

#endif
