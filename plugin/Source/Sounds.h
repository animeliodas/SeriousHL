/* SeriousHL - Half-Life's sounds: the player's own Half-Life's (copied by the installer into Sounds\SeriousHL, never
 * shipped), their data kept here while the plugin runs - CEntity::PlaySound(file) lets go of a sound's data right after
 * starting it, and the stock frees it: a sound that never comes out. */
#ifndef SERIOUSHL_SOUNDS_H
#define SERIOUSHL_SOUNDS_H

#include "StdH.h"

namespace sounds {

// "weapons\\cbar_miss1.wav" (under Sounds\SeriousHL); NULL if it isn't there.
CSoundData *Data(const char *strSound);
// One of a list "a.wav|b.wav", picked by a random number (the game's own: penRandom->IRnd()).
CSoundData *Pick(const char *strList, ULONG ulRandom);
// Plugin shutdown: all let go.
void FreeAll(void);

}; // namespace

#endif
