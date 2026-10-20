/* SeriousHL - Half-Life's HUD: health with its cross, the HEV suit's battery, the ammo (magazine | reserve, the second
 * ammo over it) and the gun's crosshair, as the Half-Life client draws them (health.cpp, battery.cpp, ammo.cpp,
 * hud.cpp DrawHudNumber: orange 255,160,0 added onto the picture, bright for a while when a number changes, red with
 * 25 health or less). Its pictures are the player's own Half-Life's sprites (sprites/*.spr with hud.txt and the guns'
 * weapon_*.txt; the installer copies them into Models\SeriousHL\sprites, never shipped) at Half-Life 25th anniversary's
 * resolutions (640, 1280, 2560 by the screen). Sam's own status line and crosshair are off while it's on. */
#ifndef SERIOUSHL_HUD_H
#define SERIOUSHL_HUD_H

#include "StdH.h"

namespace hud {

extern INDEX hl_bHud;     // 1: Half-Life's HUD; 0: Sam's
extern FLOAT hl_fHudScale; // on top of the resolution's sprites (1)

// Plugin startup: Sam's crosshair patched (off while Half-Life's HUD is on).
BOOL Init(void);
// After the game is drawn (OnPostDraw).
void Draw(CDrawPort *pdp, CEntity *penPlayer);
// Sam's status line back (the plugin going, or the HUD off).
void Shutdown(void);
CTString Status(void);
// Drawn now (Sam's status line off).
BOOL Active(void);

}; // namespace

#endif
