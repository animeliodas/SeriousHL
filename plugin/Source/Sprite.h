/* SeriousHL - Half-Life's sprites (IDSP version 2: a header, the palette - a short count of colours -, the frames),
 * read from Models\SeriousHL\sprites (the player's own Half-Life's, copied by the installer) and made GL textures, each
 * frame padded into a power-of-two one. Drawn added onto the picture: SPR_ALPHTEST's last colour is clear (black),
 * SPR_INDEXALPHA's colour is the last one as strong as the index. */
#ifndef SERIOUSHL_SPRITE_H
#define SERIOUSHL_SPRITE_H

#include "StdH.h"
#include "Gl.h"
#include <vector>

namespace sprite {

struct Frame {
  INDEX iW, iH, iW2, iH2;  // the frame's size, its texture's
  INDEX iOriginX, iOriginY; // where its top left is from its middle (Half-Life's: x right, y up)
  std::vector<ULONG> aulPixels;
  GLuint uTexture;
  HGLRC hContext;
};

struct Sprite {
  BOOL bBroken;
  INDEX iFormat; // SPR_NORMAL 0, SPR_ADDITIVE 1, SPR_INDEXALPHA 2, SPR_ALPHTEST 3
  std::vector<Frame> aFrames;
};

// "640hud7", "1280/hud_cross", "muzzleflash1" (kept once loaded; broken if not there)
Sprite *Load(const CTString &strName);
// The frame's texture bound (made in the current GL context if it isn't yet). FALSE: no such frame.
BOOL Bind(Sprite *ps, INDEX iFrame);
void FreeAll(void);
INDEX Count(void);

}; // namespace

#endif
