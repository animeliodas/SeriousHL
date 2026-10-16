/* SeriousHL - the speedrun build's mark. See Run.h. */
#include "StdH.h"
#include "Run.h"

#include <bcrypt.h>
#include <stdio.h>
#pragma comment(lib, "bcrypt.lib")

namespace run {

static char _strMark[96] = "SeriousHL";
static char _strHash[72] = "";

// SHA-256 of a file as 64 hex digits, "" if it can't be read.
static void HashFile(const char *strFile, char *strHex) {
  strHex[0] = 0;
  FILE *f = fopen(strFile, "rb");
  if (f == NULL) return;
  BCRYPT_ALG_HANDLE hAlg = NULL;
  BCRYPT_HASH_HANDLE hHash = NULL;
  UCHAR aubHash[32];
  BOOL bOK = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA256_ALGORITHM, NULL, 0) == 0
          && BCryptCreateHash(hAlg, &hHash, NULL, 0, NULL, 0, 0) == 0;
  UCHAR aubBuffer[65536];
  size_t ct;
  while (bOK && (ct = fread(aubBuffer, 1, sizeof(aubBuffer), f)) > 0) {
    bOK = BCryptHashData(hHash, aubBuffer, (ULONG)ct, 0) == 0;
  }
  bOK = bOK && BCryptFinishHash(hHash, aubHash, sizeof(aubHash), 0) == 0;
  if (hHash != NULL) BCryptDestroyHash(hHash);
  if (hAlg != NULL) BCryptCloseAlgorithmProvider(hAlg, 0);
  fclose(f);
  if (!bOK) return;
  for (INDEX i = 0; i < 32; i++) sprintf(strHex + i * 2, "%02x", aubHash[i]);
};

void Init(const char *strVersion) {
  // this DLL's file
  HMODULE hModule = NULL;
  char strFile[MAX_PATH] = "";
  if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)&Init, &hModule)) {
    GetModuleFileNameA(hModule, strFile, MAX_PATH);
  }
  HashFile(strFile, _strHash);
  char strShort[9] = "????????";
  if (_strHash[0] != 0) memcpy(strShort, _strHash, 8);
  _snprintf(_strMark, sizeof(_strMark), "SeriousHL %s RUN %s", strVersion, strShort);
  CPrintF("[SeriousHL] speedrun build %s: Half-Life's ruleset, fixed; SeriousHL.dll SHA-256 %s\n", strVersion, _strHash[0] != 0 ? _strHash : "(unread)");
};

const char *Mark(void) { return _strMark; };

void Draw(CDrawPort *pdp) {
  if (pdp == NULL) return;
  // small, bottom left, above the health; the same size on any screen
  const FLOAT fScale = pdp->GetHeight() / 720.0f;
  pdp->SetFont(_pfdConsoleFont);
  pdp->SetTextAspect(1.0f);
  pdp->SetTextScaling(fScale);
  const PIX pixX = (PIX)(8 * fScale);
  const PIX pixY = pdp->GetHeight() - (PIX)(82 * fScale);
  pdp->PutText(_strMark, pixX + 1, pixY + 1, 0x000000A0);
  pdp->PutText(_strMark, pixX, pixY, 0xFFC040D0);
};

}; // namespace
