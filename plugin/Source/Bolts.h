/* SeriousHL - the crossbow's bolts in flight (crossbow.cpp CCrossbowBolt): fired without the scope a bolt flies
 * (BOLT_AIR_VELOCITY 2000 u/s, MOVETYPE_FLY: straight), from 2 units under the eyes; what it meets takes 50
 * (sk_plr_xbow_bolt_monster) with xbow_hitbod1/2, a wall keeps it 10 s with xbow_hit1. Through the scope it's Sam's
 * sniper bullet at once, as single player's FireSniperBolt. Each tick its way is a ray (CCastRay over the whole world:
 * it's far from any entity whose sectors it could start in); drawn as Half-Life's models/crossbow_bolt.mdl
 * (ViewModel.h DrawWorld). The usual build only. */
#ifndef SERIOUSHL_BOLTS_H
#define SERIOUSHL_BOLTS_H

#include "StdH.h"
#include "ViewModel.h"
#include <vector>

namespace bolts {

// A bolt off from here along this way (unit), the player's.
void Fire(CEntity *penPlayer, const FLOAT3D &vFrom, const FLOAT3D &vDir);
// Every tick: on along its way, what it meets.
void Tick(CEntity *penPlayer);
// What to draw in the world this frame.
void Collect(std::vector<viewmodel::Placed> &aPlaced);
// A level, the game over: none.
void Release(void);
CTString Status(void);

}; // namespace

#endif
