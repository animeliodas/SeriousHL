/* SeriousHL - Half-Life's sprites. See Sprite.h. */
#include "StdH.h"
#include "Sprite.h"

#include <map>
#include <string>

namespace sprite {

using namespace gl;

static std::map<std::string, Sprite *> _mapSprites;

static INDEX PowerOfTwo(INDEX i) {
  INDEX p = 1;
  while (p < i && p < 4096) p <<= 1;
  return p;
};

Sprite *Load(const CTString &strName) {
  std::map<std::string, Sprite *>::iterator it = _mapSprites.find(strName.str_String);
  if (it != _mapSprites.end()) return it->second;
  Sprite *ps = new Sprite;
  ps->bBroken = TRUE;
  ps->iFormat = 1;
  _mapSprites[strName.str_String] = ps;
  CTString strFile = CTString("Models\\SeriousHL\\sprites\\") + strName + ".spr";
  for (char *pch = strFile.str_String; *pch != 0; pch++) if (*pch == '/') *pch = '\\';
  std::vector<UBYTE> aub;
  try {
    CTFileStream strm;
    strm.Open_t(CTFileName(strFile));
    const SLONG slSize = strm.GetStreamSize();
    if (slSize > 0) {
      aub.resize(slSize);
      strm.Read_t(&aub[0], slSize);
    }
    strm.Close();
  } catch (char *) {
    return ps;
  }
  const INDEX ctSize = (INDEX)aub.size();
  if (ctSize < 46 || memcmp(&aub[0], "IDSP", 4) != 0 || *(const int *)&aub[4] != 2) return ps;
  ps->iFormat = *(const int *)&aub[12];
  const INDEX ctFrames = *(const int *)&aub[28];
  const INDEX ctColors = *(const short *)&aub[40];
  if (ctColors <= 0 || ctColors > 256 || ctFrames <= 0 || ctFrames > 256) return ps;
  const UBYTE *pubPalette = &aub[42];
  INDEX iAt = 42 + ctColors * 3;
  for (INDEX f = 0; f < ctFrames; f++) {
    // (a single frame; a frame group isn't read)
    if (iAt + 20 > ctSize || *(const int *)&aub[iAt] != 0) break;
    Frame fr;
    fr.iOriginX = *(const int *)&aub[iAt + 4];
    fr.iOriginY = *(const int *)&aub[iAt + 8];
    fr.iW = *(const int *)&aub[iAt + 12];
    fr.iH = *(const int *)&aub[iAt + 16];
    iAt += 20;
    if (fr.iW <= 0 || fr.iH <= 0 || iAt + fr.iW * fr.iH > ctSize) break;
    const UBYTE *pubPixels = &aub[iAt];
    iAt += fr.iW * fr.iH;
    fr.iW2 = PowerOfTwo(fr.iW);
    fr.iH2 = PowerOfTwo(fr.iH);
    fr.uTexture = 0;
    fr.hContext = NULL;
    fr.aulPixels.assign(fr.iW2 * fr.iH2, 0);
    for (INDEX y = 0; y < fr.iH; y++) {
      for (INDEX x = 0; x < fr.iW; x++) {
        const UBYTE i = pubPixels[y * fr.iW + x];
        const UBYTE *rgb = pubPalette + Min((INDEX)i, ctColors - 1) * 3;
        ULONG r = rgb[0], g = rgb[1], b = rgb[2];
        if (ps->iFormat == 3 && i == 255) {
          r = g = b = 0;
        } else if (ps->iFormat == 2) {
          const UBYTE *rgbLast = pubPalette + (ctColors - 1) * 3;
          r = rgbLast[0] * i / 255;
          g = rgbLast[1] * i / 255;
          b = rgbLast[2] * i / 255;
        }
        fr.aulPixels[y * fr.iW2 + x] = r | (g << 8) | (b << 16) | (255UL << 24);
      }
    }
    ps->aFrames.push_back(fr);
  }
  ps->bBroken = ps->aFrames.empty();
  return ps;
};

BOOL Bind(Sprite *ps, INDEX iFrame) {
  if (ps == NULL || ps->bBroken || iFrame < 0 || iFrame >= (INDEX)ps->aFrames.size()) return FALSE;
  Frame &fr = ps->aFrames[iFrame];
  const HGLRC hNow = xwglGetCurrentContext();
  if (fr.uTexture == 0 || fr.hContext != hNow) {
    // (a new context: the old texture went with the old one)
    fr.hContext = hNow;
    xglGenTextures(1, &fr.uTexture);
    xglBindTexture(GL_TEXTURE_2D, fr.uTexture);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    xglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, fr.iW2, fr.iH2, 0, GL_RGBA, GL_UNSIGNED_BYTE, &fr.aulPixels[0]);
    return TRUE;
  }
  xglBindTexture(GL_TEXTURE_2D, fr.uTexture);
  return TRUE;
};

void FreeAll(void) {
  for (std::map<std::string, Sprite *>::iterator it = _mapSprites.begin(); it != _mapSprites.end(); it++) delete it->second;
  _mapSprites.clear();
};

INDEX Count(void) {
  INDEX ct = 0;
  for (std::map<std::string, Sprite *>::iterator it = _mapSprites.begin(); it != _mapSprites.end(); it++) {
    if (!it->second->bBroken) ct++;
  }
  return ct;
};

}; // namespace
