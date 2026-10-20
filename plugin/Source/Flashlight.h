/* SeriousHL - Half-Life's flashlight (player.cpp FlashlightTurnOn/Off, its battery; the engine's EF_DIMLIGHT: a light
 * where the view's trace ends, about 80 units round). F switches it (hl_iFlashlightKey), with items/flashlight1.wav;
 * the battery runs down a percent every 1.2 s while it's on (off at none) and charges one every 0.2 s while it's off.
 * The light is one of Sam's own Light entities (class 200) made dynamic, moved to the spot every tick; named, so one
 * left in a saved game is found and taken away. The HUD shows it top right (Hud.cpp, CHudFlashlight). */
#ifndef SERIOUSHL_FLASHLIGHT_H
#define SERIOUSHL_FLASHLIGHT_H

#include "StdH.h"

namespace flashlight {

extern INDEX hl_bFlashlight;    // 1: the flashlight on its key
extern INDEX hl_iFlashlightKey; // Windows virtual-key code ('F')
extern INDEX hl_bTestFlashlight; // tests: switch it (set to 1; it goes back to 0)

// Every tick (OnStep), the local player.
void Tick(CEntity *penPlayer);
// A level, the game over: off (the light went with the world).
void Release(void);
BOOL On(void);
FLOAT Battery(void); // 0..1
CTString Status(void);

}; // namespace

#endif
