/* SeriousHL - Half-Life's movement for Serious Sam's player: the rules (Rules.h) on the SeriousBridge core's movement
 * takeover (core/sam/Movement.h), which keeps Sam's own movement out of it.
 *
 * Half-Life works in units (an inch); here 1 unit = the ruleset's fUnit metres, by default so that Sam's eyes (1.9 m)
 * are where a Half-Life player's are (64 units): 320 u/s run = 9.5 m/s, the 45-unit jump = 1.34 m. */
#ifndef SERIOUSHL_HLMOVE_H
#define SERIOUSHL_HLMOVE_H

#include "StdH.h"
#include "Rules.h"

namespace hl {

// The ruleset in use: the console's hl_* (Main.cpp) in the usual build; Half-Life's, fixed, in the speedrun build.
extern rules::Settings _settings;
extern INDEX hl_bEnable; // 1: Half-Life movement; 0: Sam's own (always 1 in the speedrun build)

// Plugin startup: the local player moves by Half-Life's rules from now on (the core runs them every tick, right before
// the engine moves him). FALSE if the engine couldn't be hooked.
BOOL Init(void);
// Back to Sam's own movement; the rules start afresh.
void Release(void);
CTString Status(void);

// Every tick the rules ran: what went in and came out (the usual build's player sounds; none in the speedrun build).
typedef void (*TickHook)(CEntity *penPlayer, const rules::Input &in, const rules::Output &out, const rules::Memory &mem);
extern TickHook _pTickHook;

// The rules' latest: horizontal speed (units a second) and whether he's on the ground (the view model's bob).
FLOAT Speed(void);
BOOL OnGround(void);

// Tests: the next ticks into %TEMP%\SeriousHL\trace.csv (one line a tick).
void TraceTicks(INDEX ctTicks);

}; // namespace

#endif
