/* SeriousHL - Half-Life's thrown things on Sam's grenades (Projectile, class 501, made by CPlayerWeapons::FireGrenade):
 * the hand grenade (ggrenade.cpp ShootTimed: thrown as handgrenade.cpp throws it, its fuse 3 s from the pin, 100 over
 * 250 units), the satchel (satchel.cpp: 274 u/s, set off by the radio, 150 over 375) and the MP5's contact grenade
 * (100 over 250), a tripmine (Tripmine.h: 150 over 375, still). Each is turned into Half-Life's when it's made - its damage, its speed, no going off on a monster
 * (debris collision: brushes only; the contact grenade keeps Sam's) - and tracked every tick: half of Half-Life's gravity
 * (pev->gravity 0.5 - the level's taken back to it), Half-Life's bounce sounds instead of Sam's, and when it's gone,
 * Half-Life's explosion sound instead of the one of Sam's explosion effect (BasicEffect, class 601) left where it was. */
#ifndef SERIOUSHL_GRENADES_H
#define SERIOUSHL_GRENADES_H

#include "StdH.h"

namespace grenades {

enum Kind { K_HAND, K_SATCHEL, K_CONTACT, K_TRIPMINE };
// Told of every blast of these (where, how far its damage goes: metres) - the tripmines go off in another's.
typedef void (*BlastHook)(const FLOAT3D &vAt, FLOAT fRadius);
extern BlastHook _pBlastHook;

// A projectile Sam just made, from now Half-Life's: its velocity (m/s), its fuse (seconds; < 0: none / on contact).
void Track(CEntity *penProjectile, Kind kind, const FLOAT3D &vVelocity, FLOAT tmFuse);
// The satchels out set off (within 4096 units of the player, as the radio's reach). The count.
INDEX Detonate(CEntity *penPlayer);
INDEX Satchels(void);
// Every game tick.
void Tick(void);
// A level, the game over: nothing tracked.
void Release(void);
CTString Status(void);

}; // namespace

#endif
