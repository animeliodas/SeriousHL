/* SeriousHL - Half-Life's tripmines (tripmine.cpp: CTripmine, CTripmineGrenade).
 *
 * Put on a wall (a brush) at most 128 units ahead of the eyes, facing out of it, 8 units off; it powers up in 2.5 s
 * (mine_deploy, mine_charge), then its laser goes out 2048 units (mine_activate): anything that changes how far the
 * beam reaches - a monster, the player, a door - or the wall it's on moving sets it off 0.1-0.3 s later: 150 damage
 * over 2.5 x 150 units, from 0.6 x 126 units out of the wall (CGrenade::Explode).
 *
 * The mine is a Sam's grenade (the weapons' NewGrenade, kept by Grenades.h: the explosion, its damage and Half-Life's
 * sound) made still and unseen; Half-Life's v_tripmine.mdl (body 3, sequence "world") and the laser (sprites/laserbeam,
 * 0 214 198 at 64 of 255) are drawn over Sam's world (ViewModel.h DrawWorld). The beam is a ray from the grenade
 * (CCastRay: through the sectors around it) every 0.1 s, as Half-Life's think. The usual build only. */
#ifndef SERIOUSHL_TRIPMINE_H
#define SERIOUSHL_TRIPMINE_H

#include "StdH.h"
#include "ViewModel.h"
#include <vector>

namespace tripmine {

// Where a mine would go: the wall 128 units ahead of the eyes at most (a brush), its normal out of it, the wall's entity.
BOOL Aim(CEntity *penPlayer, FLOAT3D &vHit, FLOAT3D &vNormal, CEntity *&penWall);
// A grenade made a mine (put 8 units off the wall): still, unseen, its sounds; the player's to set off.
void Add(CEntity *penGrenade, CEntity *penPlayer, const FLOAT3D &vNormal, CEntity *penWall);
// Every tick: powering up, the beams, the walls.
void Tick(CEntity *penPlayer);
// What to draw in the world (ViewModel.h DrawWorld): the mines and their beams.
void Collect(std::vector<viewmodel::Placed> &aPlaced, std::vector<viewmodel::Beam> &aBeams);
INDEX Count(void);
// A level, the game over: none kept.
void Release(void);
CTString Status(void);

}; // namespace

#endif
