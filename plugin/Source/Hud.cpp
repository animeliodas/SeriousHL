/* SeriousHL - Half-Life's HUD. See Hud.h. */
#include "StdH.h"
#include "Hud.h"
#include "Gl.h"
#include "Player.h"
#include "Weapons.h"
#include "SBridge.h"

#include <Extras/XGizmo/Objects/StructPtr.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>

namespace hud {

INDEX hl_bHud = TRUE;
FLOAT hl_fHudScale = 1.0f;

using namespace gl;

// --- sprites (Half-Life's IDSP version 2): the first frame, padded into a power-of-two texture
struct Sprite {
  BOOL bBroken;
  INDEX iW, iH, iW2, iH2;
  std::vector<ULONG> aulPixels;
  GLuint uTexture;
  HGLRC hContext;
};
static std::map<std::string, Sprite *> _mapSprites;

static BOOL ReadFile(const CTString &strFile, std::vector<UBYTE> &aub) {
  try {
    CTFileStream strm;
    strm.Open_t(CTFileName(strFile));
    const SLONG slSize = strm.GetStreamSize();
    aub.resize(slSize > 0 ? slSize : 0);
    if (slSize > 0) strm.Read_t(&aub[0], slSize);
    strm.Close();
    return slSize > 0;
  } catch (char *) {
    return FALSE;
  }
};

static INDEX PowerOfTwo(INDEX i) {
  INDEX p = 1;
  while (p < i && p < 4096) p <<= 1;
  return p;
};

static Sprite *LoadSprite(const CTString &strName) {
  std::map<std::string, Sprite *>::iterator it = _mapSprites.find(strName.str_String);
  if (it != _mapSprites.end()) return it->second;
  Sprite *ps = new Sprite;
  ps->bBroken = TRUE;
  ps->uTexture = 0;
  ps->hContext = NULL;
  _mapSprites[strName.str_String] = ps;
  CTString strFile = CTString("Models\\SeriousHL\\sprites\\") + strName + ".spr";
  for (char *pch = strFile.str_String; *pch != 0; pch++) if (*pch == '/') *pch = '\\';
  std::vector<UBYTE> aub;
  if (!ReadFile(strFile, aub) || aub.size() < 46 || memcmp(&aub[0], "IDSP", 4) != 0 || *(const int *)&aub[4] != 2) return ps;
  const int iFormat = *(const int *)&aub[12];
  const int ctColors = *(const short *)&aub[40];
  const int iFrame = 42 + ctColors * 3;
  if (ctColors <= 0 || ctColors > 256 || (int)aub.size() < iFrame + 20) return ps;
  const UBYTE *pubPalette = &aub[42];
  const int w = *(const int *)&aub[iFrame + 12], h = *(const int *)&aub[iFrame + 16];
  if (w <= 0 || h <= 0 || (int)aub.size() < iFrame + 20 + w * h) return ps;
  const UBYTE *pubPixels = &aub[iFrame + 20];
  ps->iW = w;
  ps->iH = h;
  ps->iW2 = PowerOfTwo(w);
  ps->iH2 = PowerOfTwo(h);
  ps->aulPixels.assign(ps->iW2 * ps->iH2, 0);
  for (int y = 0; y < h; y++) {
    for (int x = 0; x < w; x++) {
      const UBYTE i = pubPixels[y * w + x];
      const UBYTE *rgb = pubPalette + Min((int)i, ctColors - 1) * 3;
      ULONG r = rgb[0], g = rgb[1], b = rgb[2];
      if (iFormat == 3 && i == 255) {
        // SPR_ALPHTEST: the last colour is clear - black, so adding it adds nothing
        r = g = b = 0;
      } else if (iFormat == 2) {
        // SPR_INDEXALPHA: the last colour, as strong as the index
        const UBYTE *rgbLast = pubPalette + (ctColors - 1) * 3;
        r = rgbLast[0] * i / 255;
        g = rgbLast[1] * i / 255;
        b = rgbLast[2] * i / 255;
      }
      ps->aulPixels[y * ps->iW2 + x] = r | (g << 8) | (b << 16) | (255UL << 24);
    }
  }
  ps->bBroken = FALSE;
  return ps;
};

static BOOL Bind(Sprite *ps) {
  if (ps == NULL || ps->bBroken) return FALSE;
  const HGLRC hNow = xwglGetCurrentContext();
  if (ps->uTexture == 0 || ps->hContext != hNow) {
    ps->hContext = hNow;
    xglGenTextures(1, &ps->uTexture);
    xglBindTexture(GL_TEXTURE_2D, ps->uTexture);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    xglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, ps->iW2, ps->iH2, 0, GL_RGBA, GL_UNSIGNED_BYTE, &ps->aulPixels[0]);
    return TRUE;
  }
  xglBindTexture(GL_TEXTURE_2D, ps->uTexture);
  return TRUE;
};

// --- hud.txt and the guns' weapon_*.txt: "name resolution sprite x y w h", the resolution's own
struct Rect { CTString strSprite; INDEX x, y, w, h; };
static INDEX _iRes = 0;
static std::map<std::string, Rect> _mapHud;
static std::map<std::string, std::map<std::string, Rect> > _mapGuns;
static BOOL _bLoaded = FALSE, _bTried = FALSE;

static void ReadList(const CTString &strFile, INDEX iRes, std::map<std::string, Rect> &map) {
  std::vector<UBYTE> aub;
  if (!ReadFile(strFile, aub)) return;
  aub.push_back(0);
  for (char *strLine = strtok((char *)&aub[0], "\r\n"); strLine != NULL; strLine = strtok(NULL, "\r\n")) {
    char *pchComment = strstr(strLine, "//");
    if (pchComment != NULL) *pchComment = 0;
    char strName[64], strSprite[128];
    int iLineRes, x, y, w, h;
    if (sscanf(strLine, "%63s %d %127s %d %d %d %d", strName, &iLineRes, strSprite, &x, &y, &w, &h) != 7 || iLineRes != iRes) continue;
    Rect r;
    r.strSprite = strSprite;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    map[strName] = r;
  }
};

static void LoadLists(CDrawPort *pdp) {
  // Half-Life 25th anniversary's choice by the screen
  const PIX w = pdp->GetWidth(), h = pdp->GetHeight();
  const INDEX iRes = (w > 2560 && h > 1600) ? 2560 : (w >= 1280 && h > 720) ? 1280 : 640;
  if (_bTried && iRes == _iRes) return;
  _bTried = TRUE;
  _iRes = iRes;
  _mapHud.clear();
  _mapGuns.clear();
  ReadList("Models\\SeriousHL\\sprites\\hud.txt", iRes, _mapHud);
  // (an older Half-Life: 640 only)
  if (_mapHud.find("number_0") == _mapHud.end() && iRes != 640) {
    _iRes = 640;
    ReadList("Models\\SeriousHL\\sprites\\hud.txt", 640, _mapHud);
  }
  _bLoaded = _mapHud.find("number_0") != _mapHud.end();
};

static const std::map<std::string, Rect> &Gun(const char *strFile) {
  std::map<std::string, std::map<std::string, Rect> >::iterator it = _mapGuns.find(strFile);
  if (it != _mapGuns.end()) return it->second;
  std::map<std::string, Rect> &map = _mapGuns[strFile];
  ReadList(CTString("Models\\SeriousHL\\sprites\\") + strFile + ".txt", _iRes, map);
  return map;
};

static const Rect *Find(const std::map<std::string, Rect> &map, const char *strName) {
  std::map<std::string, Rect>::const_iterator it = map.find(strName);
  return it != map.end() ? &it->second : NULL;
};

// --- drawing (the Half-Life client's SPR_DrawAdditive and FillRGBA), screen pixels
static FLOAT _fScale = 1.0f;

static void DrawAdditive(const Rect *pr, FLOAT x, FLOAT y, INDEX r, INDEX g, INDEX b) {
  if (pr == NULL) return;
  Sprite *ps = LoadSprite(pr->strSprite);
  if (!Bind(ps)) return;
  xglEnable(GL_TEXTURE_2D);
  xglBlendFunc(GL_ONE, GL_ONE);
  xglColor4f(r / 255.0f, g / 255.0f, b / 255.0f, 1.0f);
  const FLOAT u0 = (FLOAT)pr->x / ps->iW2, v0 = (FLOAT)pr->y / ps->iH2;
  const FLOAT u1 = (FLOAT)(pr->x + pr->w) / ps->iW2, v1 = (FLOAT)(pr->y + pr->h) / ps->iH2;
  const FLOAT x1 = x + pr->w * _fScale, y1 = y + pr->h * _fScale;
  xglBegin(GL_QUADS);
  xglTexCoord2f(u0, v0); xglVertex2f(x, y);
  xglTexCoord2f(u1, v0); xglVertex2f(x1, y);
  xglTexCoord2f(u1, v1); xglVertex2f(x1, y1);
  xglTexCoord2f(u0, v1); xglVertex2f(x, y1);
  xglEnd();
};

static void FillRGBA(FLOAT x, FLOAT y, FLOAT w, FLOAT h, INDEX r, INDEX g, INDEX b, INDEX a) {
  xglDisable(GL_TEXTURE_2D);
  xglBlendFunc(GL_SRC_ALPHA, GL_ONE);
  xglColor4f(r / 255.0f, g / 255.0f, b / 255.0f, a / 255.0f);
  xglBegin(GL_QUADS);
  xglVertex2f(x, y);
  xglVertex2f(x + w, y);
  xglVertex2f(x + w, y + h);
  xglVertex2f(x, y + h);
  xglEnd();
};

static const ULONG DHN_DRAWZERO = 1, DHN_2DIGITS = 2, DHN_3DIGITS = 4;

static FLOAT DrawHudNumber(FLOAT x, FLOAT y, ULONG ulFlags, INDEX iNumber, INDEX r, INDEX g, INDEX b) {
  const Rect *pr0 = Find(_mapHud, "number_0");
  const FLOAT fWidth = pr0->w * _fScale;
  char strDigit[16];
  if (iNumber > 0) {
    if (iNumber >= 100) {
      sprintf(strDigit, "number_%d", (iNumber / 100) % 10);
      DrawAdditive(Find(_mapHud, strDigit), x, y, r, g, b);
      x += fWidth;
    } else if (ulFlags & DHN_3DIGITS) {
      x += fWidth;
    }
    if (iNumber >= 10) {
      sprintf(strDigit, "number_%d", (iNumber % 100) / 10);
      DrawAdditive(Find(_mapHud, strDigit), x, y, r, g, b);
      x += fWidth;
    } else if (ulFlags & (DHN_3DIGITS | DHN_2DIGITS)) {
      x += fWidth;
    }
    sprintf(strDigit, "number_%d", iNumber % 10);
    DrawAdditive(Find(_mapHud, strDigit), x, y, r, g, b);
    x += fWidth;
  } else if (ulFlags & DHN_DRAWZERO) {
    if (ulFlags & DHN_3DIGITS) x += fWidth;
    if (ulFlags & (DHN_3DIGITS | DHN_2DIGITS)) x += fWidth;
    DrawAdditive(pr0, x, y, r, g, b);
    x += fWidth;
  }
  return x;
};

static void ScaleColors(INDEX &r, INDEX &g, INDEX &b, INDEX a) {
  r = r * a / 255;
  g = g * a / 255;
  b = b * a / 255;
};

// --- the pieces, with their fades (MIN_ALPHA 100; a changed number bright, fading back)
static const INDEX MIN_ALPHA = 100;
static INDEX _iHealth = -1, _iArmor = -1, _iClip = -2, _iAmmo = -2;
static FLOAT _fHealthFade = 0, _fArmorFade = 0, _fAmmoFade = 0;
static CTimerValue _tvLast;
static BOOL _bLastTime = FALSE;
static ULONG _ctDrawn = 0;

static void DrawHealth(PIX pixW, PIX pixH, INDEX iHealth, FLOAT tmDelta) {
  if (iHealth != _iHealth) {
    _fHealthFade = 100.0f;
    _iHealth = iHealth;
  }
  INDEX a = MIN_ALPHA;
  if (_fHealthFade > 0) {
    _fHealthFade -= tmDelta * 20.0f;
    if (_fHealthFade <= 0) _fHealthFade = 0;
    a = MIN_ALPHA + (INDEX)(_fHealthFade / 100.0f * 128.0f);
  }
  if (iHealth <= 15) a = 255;
  INDEX r = 255, g = 160, b = 0;
  if (iHealth <= 25) {
    r = 250;
    g = 0;
    b = 0;
  }
  ScaleColors(r, g, b, a);
  const Rect *prCross = Find(_mapHud, "cross"), *pr0 = Find(_mapHud, "number_0");
  const FLOAT fFont = pr0->h * _fScale, fWidth = pr0->w * _fScale;
  const FLOAT fCross = prCross != NULL ? prCross->w * _fScale : fWidth;
  const FLOAT y = pixH - fFont - fFont / 2;
  DrawAdditive(prCross, fCross / 2, y, r, g, b);
  FLOAT x = fCross + fWidth / 2;
  x = DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, iHealth, r, g, b);
  x += fWidth / 2;
  FillRGBA(x, y, fWidth / 10, fFont, 255, 160, 0, a);
};

static void DrawBattery(PIX pixW, PIX pixH, INDEX iArmor, FLOAT tmDelta) {
  if (iArmor != _iArmor) {
    _fArmorFade = 100.0f;
    _iArmor = iArmor;
  }
  INDEX a = MIN_ALPHA;
  if (_fArmorFade > 0) {
    _fArmorFade -= tmDelta * 20.0f;
    if (_fArmorFade <= 0) _fArmorFade = 0;
    a = MIN_ALPHA + (INDEX)(_fArmorFade / 100.0f * 128.0f);
  }
  INDEX r = 255, g = 160, b = 0;
  ScaleColors(r, g, b, a);
  const Rect *prEmpty = Find(_mapHud, "suit_empty"), *prFull = Find(_mapHud, "suit_full"), *pr0 = Find(_mapHud, "number_0");
  if (prEmpty == NULL || prFull == NULL) return;
  const FLOAT fFont = pr0->h * _fScale;
  const FLOAT y = pixH - fFont - fFont / 2;
  FLOAT x = pixW / 5.0f;
  const FLOAT fOffset = prEmpty->h * _fScale / 6;
  DrawAdditive(prEmpty, x, y - fOffset, r, g, b);
  // the full suit's picture over it, cut from the top by what's missing of 100
  Rect rcFull = *prFull;
  const INDEX iCut = (INDEX)(prFull->h * (100 - Min(100L, iArmor)) * 0.01f);
  rcFull.y += iCut;
  rcFull.h -= iCut;
  if (rcFull.h > 0) DrawAdditive(&rcFull, x, y - fOffset + iCut * _fScale, r, g, b);
  x += prEmpty->w * _fScale;
  DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, iArmor, r, g, b);
};

static void DrawAmmo(PIX pixW, PIX pixH, const std::map<std::string, Rect> &gun, INDEX iClip, INDEX iAmmo, INDEX iAmmo2, FLOAT tmDelta) {
  if (iClip != _iClip || iAmmo != _iAmmo) {
    _fAmmoFade = 200.0f;
    _iClip = iClip;
    _iAmmo = iAmmo;
  }
  const INDEX a = Max(MIN_ALPHA, (INDEX)_fAmmoFade);
  if (_fAmmoFade > 0) _fAmmoFade = Max(_fAmmoFade - tmDelta * 20.0f, 0.0f);
  INDEX r = 255, g = 160, b = 0;
  ScaleColors(r, g, b, a);
  const Rect *pr0 = Find(_mapHud, "number_0");
  const FLOAT fFont = pr0->h * _fScale, fWidth = pr0->w * _fScale;
  FLOAT y = pixH - fFont - fFont / 2;
  const Rect *prAmmo = Find(gun, "ammo");
  if (prAmmo != NULL && (iClip >= 0 || iAmmo >= 0)) {
    const FLOAT fIcon = prAmmo->w * _fScale;
    FLOAT x;
    if (iClip >= 0) {
      // the magazine, a bar, the reserve
      x = pixW - 8 * fWidth - fIcon;
      x = DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, iClip, r, g, b);
      x += fWidth / 2;
      FillRGBA(x, y, fWidth / 10, fFont, r, g, b, a);
      x += fWidth / 10 + fWidth / 2;
      x = iAmmo >= 0 ? DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, iAmmo, r, g, b) : x + 3 * fWidth;
    } else {
      x = pixW - 4 * fWidth - fIcon;
      x = DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, iAmmo, r, g, b);
    }
    DrawAdditive(prAmmo, x, y - prAmmo->h * _fScale / 8, r, g, b);
  }
  const Rect *prAmmo2 = Find(gun, "ammo2");
  if (prAmmo2 != NULL && iAmmo2 > 0) {
    y -= fFont + fFont / 4;
    FLOAT x = pixW - 4 * fWidth - prAmmo2->w * _fScale;
    x = DrawHudNumber(x, y, DHN_3DIGITS | DHN_DRAWZERO, iAmmo2, r, g, b);
    DrawAdditive(prAmmo2, x, y - prAmmo2->h * _fScale / 8, r, g, b);
  }
};

// --- Sam's status line off while this is on; his crosshair (CPlayerWeapons::RenderCrosshair) skipped
static CSymbolPtr _symShowInfo;
static BOOL _bSamHidden = FALSE;

static void HideSam(BOOL bHide) {
  if (bHide == _bSamHidden) return;
  if (!_symShowInfo.Exists()) _symShowInfo.Find("hud_bShowInfo");
  if (!_symShowInfo.Exists()) return;
  _symShowInfo.GetIndex() = bHide ? 0 : 1;
  _bSamHidden = bHide;
};

typedef void (CEntity::*CRenderCrosshair)(CProjection3D &, CDrawPort *, CPlacement3D &);
static CRenderCrosshair _pRenderCrosshair = NULL;
static BOOL _bHooked = FALSE;

class CPlayerWeaponsHook : public CEntity {
public:
  void P_RenderCrosshair(CProjection3D &pr, CDrawPort *pdp, CPlacement3D &plView) {
    if (_bSamHidden) return;
    (this->*_pRenderCrosshair)(pr, pdp, plView);
  };
};

BOOL Init(void) {
  StructPtr ptr(ClassicsCore_GetEntitiesSymbol("?RenderCrosshair@CPlayerWeapons@@QAEXAAVCProjection3D@@PAVCDrawPort@@AAVCPlacement3D@@@Z"));
  if (ptr.iAddress == NULL) return FALSE;
  _pRenderCrosshair = ptr(CRenderCrosshair());
  HFuncPatch hPatch = CreatePatch(_pRenderCrosshair, &CPlayerWeaponsHook::P_RenderCrosshair, "SeriousHL: CPlayerWeapons::RenderCrosshair()");
  _bHooked = hPatch != NULL && hPatch->IsPatched();
  return _bHooked;
};

void Draw(CDrawPort *pdp, CEntity *pen) {
  if (pdp == NULL || pen == NULL || !hl_bHud || _pGfx->GetCurrentAPI() != GAT_OGL || !gl::Load()) {
    HideSam(FALSE);
    return;
  }
  LoadLists(pdp);
  if (!_bLoaded) {
    HideSam(FALSE);
    return;
  }
  HideSam(TRUE);
  const CTimerValue tvNow = _pTimer->GetHighPrecisionTimer();
  const FLOAT tmDelta = _bLastTime ? Clamp((FLOAT)(tvNow - _tvLast).GetSeconds(), 0.0f, 0.25f) : 0.0f;
  _tvLast = tvNow;
  _bLastTime = TRUE;
  _fScale = Max(hl_fHudScale, 0.25f);

  // what's shown: Half-Life's numbers (health truncated, but never 0 while alive), the gun's
  const FLOAT fHealth = ((CLiveEntity *)pen)->en_fHealth;
  INDEX iHealth = fHealth > 0.0f && fHealth <= 1.0f ? 1 : Max((INDEX)fHealth, 0L);
  const FLOAT *pfArmor = (const FLOAT *)player::Prop(pen, player::PROP_ARMOR);
  const INDEX iArmor = pfArmor != NULL ? Max((INDEX)*pfArmor, 0L) : 0;
  INDEX iClip = -1, iAmmo = -1, iAmmo2 = -1;
  const char *strGun = weapons::HudGun(pen, iClip, iAmmo, iAmmo2);

  const PIX pixW = pdp->GetWidth(), pixH = pdp->GetHeight();
  xglPushAttrib(GL_ALL_ATTRIB_BITS);
  xglPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
  xglMatrixMode(GL_PROJECTION);
  xglPushMatrix();
  xglLoadIdentity();
  xglOrtho(0, pixW, pixH, 0, -1, 1);
  xglMatrixMode(GL_TEXTURE);
  xglPushMatrix();
  xglLoadIdentity();
  xglMatrixMode(GL_MODELVIEW);
  xglPushMatrix();
  xglLoadIdentity();
  xglViewport(pdp->dp_MinI, pdp->dp_Raster->ra_Height - (pdp->dp_MinJ + pixH), pixW, pixH);
  xglDisable(GL_DEPTH_TEST);
  xglDisable(GL_CULL_FACE);
  xglDisable(GL_ALPHA_TEST);
  xglDisable(GL_FOG);
  xglDisable(GL_LIGHTING);
  xglDisable(GL_SCISSOR_TEST);
  xglDepthMask(GL_FALSE);
  xglEnable(GL_BLEND);
  xglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  DrawHealth(pixW, pixH, iHealth, tmDelta);
  DrawBattery(pixW, pixH, iArmor, tmDelta);
  if (strGun != NULL) {
    const std::map<std::string, Rect> &gun = Gun(strGun);
    DrawAmmo(pixW, pixH, gun, iClip, iAmmo, iAmmo2, tmDelta);
    // the crosshair in the middle
    const Rect *prCross = Find(gun, "crosshair");
    if (prCross != NULL && (pen->GetFlags() & ENF_ALIVE)) {
      DrawAdditive(prCross, pixW / 2 - prCross->w * _fScale / 2, pixH / 2 - prCross->h * _fScale / 2, 255, 255, 255);
    }
  }

  xglMatrixMode(GL_PROJECTION);
  xglPopMatrix();
  xglMatrixMode(GL_TEXTURE);
  xglPopMatrix();
  xglMatrixMode(GL_MODELVIEW);
  xglPopMatrix();
  xglPopClientAttrib();
  xglPopAttrib();
  _ctDrawn++;
};

void Shutdown(void) {
  HideSam(FALSE);
  for (std::map<std::string, Sprite *>::iterator it = _mapSprites.begin(); it != _mapSprites.end(); it++) delete it->second;
  _mapSprites.clear();
};

BOOL Active(void) { return _bSamHidden; };

CTString Status(void) {
  CTString str;
  INDEX ctSprites = 0;
  for (std::map<std::string, Sprite *>::iterator it = _mapSprites.begin(); it != _mapSprites.end(); it++) {
    if (!it->second->bBroken) ctSprites++;
  }
  str.PrintF("HUD %s; resolution %d, %d sprites; Sam's status line %s; %u frames drawn",
    !hl_bHud ? "Sam's" : _bLoaded ? "Half-Life's" : "Sam's (no Half-Life sprites)", _iRes, ctSprites,
    _bSamHidden ? "off" : "on", _ctDrawn);
  return str;
};

}; // namespace
