/* SeriousHL - the HEV suit's voice (player.cpp SetSuitUpdate / CheckSuitUpdate; its sentences from the player's own
 * Half-Life's sound/sentences.txt, said word by word from sound/fvox as the engine's VOX does: a word's folder from the
 * first, (p v e) after a word for it - pitch, volume, how much of it - or alone for the words after, a comma or a full
 * stop the pauses _comma and _period). One sentence at a time, the next 3.5 s after (SUITUPDATETIME), each not again
 * before its own time. What it says (CBasePlayer::TakeDamage): fractures, lacerations, blood loss by the kind and size
 * of a hit; the automedic and morphine on the first big one; health critical, near death, seek medical attention;
 * the power level when armour is picked up (CItemBattery: HEV_<n>P). The usual build only. */
#ifndef SERIOUSHL_SUIT_H
#define SERIOUSHL_SUIT_H

#include "StdH.h"

namespace suit {

extern INDEX hl_bSuitVoice; // 1: the HEV suit speaks
extern FLOAT hl_fSuitVolume; // its volume (Half-Life's suitvolume: 0.25)

// Every frame (OnFrame): the words in turn.
void Frame(void);
// Every tick (OnStep), the local player: his armour picked up (the power level).
void Tick(CEntity *penPlayer);
// A hit the player took (the HEV hook): Sam's damage type, what of it went to health, health before and after.
void Damage(CEntity *penPlayer, INDEX iDamageType, FLOAT fDamage, FLOAT fHealthBefore, FLOAT fHealthAfter);
// Say a sentence ("HEV_DMG4") unless it was said less than tmNoRepeat ago.
void Say(const char *strSentence, FLOAT tmNoRepeat);
// A level, the game over: quiet, nothing waiting.
void Release(void);
CTString Status(void);

}; // namespace

#endif
