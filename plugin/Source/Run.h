/* SeriousHL - the speedrun build (SERIOUSHL_RUN): Half-Life's ruleset fixed, nothing of it in the console, and a mark in
 * the corner of the screen so a run's video shows which build it was: "SeriousHL <version> RUN <first 8 hex digits of
 * the DLL's SHA-256>" (the full hash is in the log; a release lists it). */
#ifndef SERIOUSHL_RUN_H
#define SERIOUSHL_RUN_H

#include "StdH.h"

namespace run {

// Plugin startup: hash this DLL's file.
void Init(const char *strVersion);
// After the game is drawn (OnPostDraw): the mark.
void Draw(CDrawPort *pdp);
// "SeriousHL 0.2.0 RUN 1a2b3c4d"
const char *Mark(void);

}; // namespace

#endif
