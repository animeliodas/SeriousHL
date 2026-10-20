/* SeriousHL - a crash's report, for the player to send: SeriousHL_crash.txt next to the game's log. What crashed (the
 * exception, where - the module and its offset, the thread), the registers, the return addresses into modules found on
 * the thread's stack, and what the plugin was doing then (its last phase: a tick, a frame, a world loading...).
 *
 * Two ways in, neither handling anything (the game crashes as it would): the unhandled exception filter (chained to the
 * one before), and a vectored handler for the one case nothing would report well - code run at an address in no module
 * (a call through a freed object's table). Access violations the engine catches itself happen inside modules. */
#ifndef SERIOUSHL_CRASHLOG_H
#define SERIOUSHL_CRASHLOG_H

#include "StdH.h"

namespace crashlog {

void Init(const char *strVersion);
void Shutdown(void);
// What the plugin is in now ("tick", "world load"; NULL: out of it). A string that lives on.
void Phase(const char *strPhase);

}; // namespace

#endif
