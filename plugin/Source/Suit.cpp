/* SeriousHL - the HEV suit's voice. See Suit.h. */
#include "StdH.h"
#include "Suit.h"
#include "Player.h"
#include "Sounds.h"
#include "SBridge.h"

#include <Engine/Sound/SoundData.h>

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <string>
#include <vector>

namespace suit {

INDEX hl_bSuitVoice = TRUE;
FLOAT hl_fSuitVolume = 0.25f;

// --- sentences.txt, parsed as VOX does
struct Word {
  std::string strFile;  // "fvox\\boop.wav"
  FLOAT fPitch, fVolume, fEnd; // 1 = as recorded; fEnd: the part of it played
};
typedef std::vector<Word> Sentence;
static std::map<std::string, Sentence> _mapSentences;
static BOOL _bLoaded = FALSE;

static void ReadParams(const char *strParams, FLOAT &fPitch, FLOAT &fVolume, FLOAT &fEnd) {
  for (const char *pch = strParams; *pch != 0; pch++) {
    const char c = *pch;
    if (c != 'p' && c != 'v' && c != 'e') continue;
    const int i = atoi(pch + 1);
    if (c == 'p' && i > 0) fPitch = i / 100.0f;
    if (c == 'v' && i > 0) fVolume = i / 100.0f;
    if (c == 'e' && i > 0) fEnd = i / 100.0f;
  }
};

static void Parse(const char *strLine) {
  char strName[64];
  const char *pch = strLine;
  while (*pch == ' ' || *pch == '\t') pch++;
  INDEX n = 0;
  while (*pch != 0 && *pch != ' ' && *pch != '\t' && n < 63) strName[n++] = *pch++;
  strName[n] = 0;
  if (strncmp(strName, "HEV_", 4) != 0) return;
  Sentence sen;
  std::string strDir = "fvox";
  FLOAT fPitch = 1.0f, fVolume = 1.0f; // (for the words after a lone "(p..)")
  BOOL bWordJust = FALSE;              // (the last thing read was a word: "(..)" right after it is its own)
  while (*pch != 0) {
    const char c = *pch;
    if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
      bWordJust = FALSE;
      pch++;
    } else if (c == '(') {
      const char *pchEnd = strchr(pch, ')');
      std::string strParams(pch + 1, pchEnd != NULL ? pchEnd : pch + strlen(pch));
      pch = pchEnd != NULL ? pchEnd + 1 : pch + strlen(pch);
      if (bWordJust && !sen.empty()) {
        Word &w = sen.back();
        ReadParams(strParams.c_str(), w.fPitch, w.fVolume, w.fEnd);
      } else {
        FLOAT fDummy = 1.0f;
        ReadParams(strParams.c_str(), fPitch, fVolume, fDummy);
      }
    } else if (c == ',' || c == '.') {
      Word w = { strDir + "\\" + (c == ',' ? "_comma" : "_period") + ".wav", fPitch, fVolume, 1.0f };
      sen.push_back(w);
      bWordJust = FALSE;
      pch++;
    } else {
      std::string strWord;
      while (*pch != 0 && *pch != ' ' && *pch != '\t' && *pch != ',' && *pch != '.' && *pch != '(' && *pch != '\r' && *pch != '\n') strWord += *pch++;
      const size_t iSlash = strWord.find('/');
      if (iSlash != std::string::npos) {
        strDir = strWord.substr(0, iSlash);
        strWord = strWord.substr(iSlash + 1);
      }
      if (!strWord.empty()) {
        Word w = { strDir + "\\" + strWord + ".wav", fPitch, fVolume, 1.0f };
        sen.push_back(w);
        bWordJust = TRUE;
      }
    }
  }
  _mapSentences[strName] = sen;
};

static void Load(void) {
  if (_bLoaded) return;
  _bLoaded = TRUE;
  try {
    CTFileStream strm;
    strm.Open_t(CTFileName(CTString("Sounds\\SeriousHL\\sentences.txt")));
    const SLONG slSize = strm.GetStreamSize();
    std::vector<char> ach(slSize + 1, 0);
    if (slSize > 0) strm.Read_t(&ach[0], slSize);
    strm.Close();
    for (char *strLine = strtok(&ach[0], "\n"); strLine != NULL; strLine = strtok(NULL, "\n")) Parse(strLine);
  } catch (char *) {
  }
};

// --- saying: the queue (SetSuitUpdate) and the words in turn
static std::vector<std::string> _astrQueue;
static std::map<std::string, TIME> _mapNoRepeat;
static TIME _tmNextSentence = 0;
static const Sentence *_psenNow = NULL;
static INDEX _iWord = 0;
static CTimerValue _tvWordEnd;
static BOOL _bWordOn = FALSE;
static CSoundObject *_pso = NULL;
static ULONG _ctSaid = 0, _ctWords = 0;
static FLOAT _fPitch = 1.0f; // the sentence's (EMIT_SOUND_SUIT: every other one 98-104)
static FLOAT _fLastArmor = -1.0f;
static std::string _strLast;
static ULONG _ulOwner = 0;

void Say(const char *strSentence, FLOAT tmNoRepeat) {
  if (!hl_bSuitVoice || strSentence == NULL) return;
  const TIME tmNow = _pTimer->CurrentTick();
  std::map<std::string, TIME>::iterator it = _mapNoRepeat.find(strSentence);
  if (it != _mapNoRepeat.end() && tmNow < it->second) return;
  _mapNoRepeat[strSentence] = tmNow + tmNoRepeat;
  if (_astrQueue.size() >= 4) return; // (CSUITPLAYLIST)
  _astrQueue.push_back(strSentence);
  if (_tmNextSentence <= tmNow) _tmNextSentence = tmNow + 0.1f; // (SUITFIRSTUPDATETIME)
};

static CEntity *Owner(void) { return _ulOwner != 0 ? player::Local() : NULL; };

static void StartWord(void) {
  _bWordOn = FALSE;
  while (_psenNow != NULL && _iWord < (INDEX)_psenNow->size()) {
    const Word &w = (*_psenNow)[_iWord++];
    CSoundData *psd = sounds::Data(w.strFile.c_str());
    if (psd == NULL) continue;
    if (_pso == NULL) _pso = new CSoundObject;
    _pso->SetOwner(Owner());
    const FLOAT fPitch = w.fPitch * _fPitch;
    _pso->Set3DParameters(30.0f, 3.0f, Clamp(w.fVolume * hl_fSuitVolume, 0.0f, 1.0f), fPitch);
    _pso->Play(psd, SOF_3D | SOF_VOLUMETRIC);
    _ctWords++;
    const double dLength = psd->GetSecondsLength() * w.fEnd / Max(fPitch, 0.1f);
    _tvWordEnd = _pTimer->GetHighPrecisionTimer() + CTimerValue(dLength);
    _bWordOn = TRUE;
    return;
  }
  _psenNow = NULL;
};

void Frame(void) {
  if (!_bWordOn) return;
  if (_pTimer->GetHighPrecisionTimer() < _tvWordEnd) return;
  // (a part of a word: cut there)
  if (_pso != NULL && _pso->IsPlaying()) _pso->Stop();
  StartWord();
};

void Tick(CEntity *pen) {
  if (pen == NULL) return;
  _ulOwner = pen->en_ulID;
  Load();
  // CheckSuitUpdate: the next sentence once the last one's time is up
  const TIME tmNow = _pTimer->CurrentTick();
  if (!_bWordOn && !_astrQueue.empty() && tmNow >= _tmNextSentence) {
    const std::string strName = _astrQueue.front();
    _astrQueue.erase(_astrQueue.begin());
    std::map<std::string, Sentence>::iterator it = _mapSentences.find(strName);
    if (it != _mapSentences.end()) {
      _psenNow = &it->second;
      _iWord = 0;
      _fPitch = (rand() & 1) ? (98 + rand() % 7) / 100.0f : 1.0f;
      StartWord();
      _ctSaid++;
      _strLast = strName;
    }
    _tmNextSentence = tmNow + 3.5f; // (SUITUPDATETIME)
  }
  // the power level when armour is picked up (CItemBattery): HEV_<n>P, n = (percent of 100 + rounding) / 5 - 1
  const FLOAT *pfArmor = (const FLOAT *)player::Prop(pen, player::PROP_ARMOR);
  if (pfArmor != NULL) {
    if (_fLastArmor >= 0.0f && *pfArmor > _fLastArmor + 0.5f) {
      INDEX iPct = (INDEX)(Min(*pfArmor, 100.0f) + 0.5f) / 5;
      if (iPct > 0) iPct--;
      char strName[16];
      sprintf(strName, "HEV_%dP", iPct);
      Say(strName, 30.0f);
    }
    _fLastArmor = *pfArmor;
  }
};

// Sam's damage types (Common/Common.h DamageType) as Half-Life's kinds (CBasePlayer::TakeDamage; fDamage the whole
// hit, armour's part too - m_lastDamageAmount; a blast, a burn, a shock, drowning: nothing said, as there)
void Damage(CEntity *pen, INDEX iDamageType, FLOAT fDamage, FLOAT fHealthBefore, FLOAT fHealthAfter) {
  if (!hl_bSuitVoice || fHealthAfter <= 0.0f || fDamage <= 0.0f) return;
  const BOOL bTrivial = fHealthAfter > 75.0f || fDamage < 5.0f;
  const BOOL bMajor = fDamage > 25.0f;
  const BOOL bCritical = fHealthAfter < 30.0f;
  const BOOL bTimeBased = iDamageType == DMT_ACID; // (DMG_ACID is of DMG_TIMEBASED)
  if (!bTrivial || bTimeBased) {
    switch (iDamageType) {
      case DMT_CLOSERANGE: Say(bMajor ? "HEV_DMG1" : "HEV_DMG0", 30.0f); break;   // DMG_SLASH: lacerations
      case DMT_IMPACT: case DMT_BRUSH: Say(bMajor ? "HEV_DMG5" : "HEV_DMG4", 30.0f); break; // DMG_FALL / DMG_CRUSH: fractures
      case DMT_BULLET: if (fDamage > 5.0f) Say("HEV_DMG6", 30.0f); break;          // blood loss
      case DMT_ACID: Say("HEV_DET1", 60.0f); break;                                // hazardous chemicals
    }
  }
  // the first big hit: the automedic, a morphine shot
  if (!bTrivial && bMajor && fHealthBefore >= 75.0f) {
    Say("HEV_MED1", 1800.0f);
    Say("HEV_HEAL7", 1800.0f);
  }
  // already hurt, now critical
  if (!bTrivial && bCritical && fHealthBefore < 75.0f) {
    if (fHealthAfter < 6.0f) Say("HEV_HLTH3", 600.0f);
    else if (fHealthAfter < 20.0f) Say("HEV_HLTH2", 600.0f);
    if ((rand() & 3) == 0 && fHealthBefore < 50.0f) Say("HEV_DMG7", 300.0f);
  }
  // time based: its effects go on
  if (bTimeBased && fHealthBefore < 75.0f) {
    if (fHealthBefore >= 50.0f) Say("HEV_HLTH1", 600.0f);
    else if ((rand() & 3) == 0) Say("HEV_DMG7", 300.0f);
  }
};

void Release(void) {
  _astrQueue.clear();
  _psenNow = NULL;
  _bWordOn = FALSE;
  _ulOwner = 0;
  _fLastArmor = -1.0f;
  if (_pso != NULL) {
    _pso->Stop();
    _pso->SetOwner(NULL);
  }
};

CTString Status(void) {
  CTString str;
  str.PrintF("HEV voice %s; %d sentences known, %u said (last %s, %u words), %d waiting", hl_bSuitVoice ? "on" : "off",
    (INDEX)_mapSentences.size(), _ctSaid, _strLast.empty() ? "-" : _strLast.c_str(), _ctWords, (INDEX)_astrQueue.size());
  return str;
};

}; // namespace
