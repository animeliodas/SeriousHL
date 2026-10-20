/* SeriousHL - Half-Life's sounds. See Sounds.h. */
#include "StdH.h"
#include "Sounds.h"
#include "SBridge.h"

#include <Engine/Templates/Stock_CSoundData.h>
#include <map>
#include <string>

namespace sounds {

static std::map<std::string, CSoundData *> _mapSounds;

CSoundData *Data(const char *strSound) {
  const CTFileName fnm = CTString("Sounds\\SeriousHL\\") + strSound;
  std::map<std::string, CSoundData *>::iterator it = _mapSounds.find(fnm.str_String);
  if (it != _mapSounds.end()) return it->second;
  CSoundData *psd = NULL;
  if (FileExists(fnm)) {
    try {
      psd = _pSoundStock->Obtain_t(fnm);
    } catch (char *strError) {
      CPrintF(SB_TAG " %s\n", strError);
    }
  }
  _mapSounds[fnm.str_String] = psd;
  return psd;
};

CSoundData *Pick(const char *strList, ULONG ulRandom) {
  if (strList == NULL) return NULL;
  char strCopy[256];
  strncpy(strCopy, strList, sizeof(strCopy) - 1);
  strCopy[sizeof(strCopy) - 1] = 0;
  const char *astr[8];
  INDEX ct = 0;
  for (char *str = strtok(strCopy, "|"); str != NULL && ct < 8; str = strtok(NULL, "|")) astr[ct++] = str;
  return ct > 0 ? Data(astr[ulRandom % ct]) : NULL;
};

void FreeAll(void) {
  for (std::map<std::string, CSoundData *>::iterator it = _mapSounds.begin(); it != _mapSounds.end(); it++) {
    if (it->second != NULL) _pSoundStock->Release(it->second);
  }
  _mapSounds.clear();
};

}; // namespace
