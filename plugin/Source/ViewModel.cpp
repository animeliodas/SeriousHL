/* SeriousHL - Half-Life's view models drawn instead of Sam's. See ViewModel.h. */
#include "StdH.h"
#include "ViewModel.h"
#include "Player.h"
#include "HlMove.h"
#include "Gl.h"
#include "Sprite.h"
#include "SBridge.h"

#include <Extras/XGizmo/Objects/StructPtr.h>
#include <math.h>
#include <map>
#include <string>
#include <vector>


namespace viewmodel {


INDEX hl_bViewModels = TRUE;
FLOAT hl_fViewModelFov = 90.0f;
FLOAT hl_fMuzzleFlashTime = 0.04f;

// --- the Half-Life SDK's studio.h (version 10)
#pragma pack(push, 4)
struct StudioHeader {
  int id, version;
  char name[64];
  int length;
  float eyeposition[3], min[3], max[3], bbmin[3], bbmax[3];
  int flags;
  int numbones, boneindex;
  int numbonecontrollers, bonecontrollerindex;
  int numhitboxes, hitboxindex;
  int numseq, seqindex;
  int numseqgroups, seqgroupindex;
  int numtextures, textureindex, texturedataindex;
  int numskinref, numskinfamilies, skinindex;
  int numbodyparts, bodypartindex;
  int numattachments, attachmentindex;
  int soundtable, soundindex, soundgroups, soundgroupindex;
  int numtransitions, transitionindex;
};
struct StudioBone { char name[32]; int parent; int flags; int bonecontroller[6]; float value[6]; float scale[6]; };
struct StudioSeq {
  char label[32];
  float fps;
  int flags, activity, actweight, numevents, eventindex, numframes, numpivots, pivotindex, motiontype, motionbone;
  float linearmovement[3];
  int automoveposindex, automoveangleindex;
  float bbmin[3], bbmax[3];
  int numblends, animindex, blendtype[2];
  float blendstart[2], blendend[2];
  int blendparent, seqgroup, entrynode, exitnode, nodeflags, nextseq;
};
struct StudioSeqGroup { char label[32]; char name[64]; int unused1, unused2; };
struct StudioAnim { unsigned short offset[6]; };
union StudioAnimValue { struct { UBYTE valid, total; } num; short value; };
struct StudioBodyPart { char name[64]; int nummodels, base, modelindex; };
struct StudioModel {
  char name[64];
  int type;
  float boundingradius;
  int nummesh, meshindex, numverts, vertinfoindex, vertindex, numnorms, norminfoindex, normindex, numgroups, groupindex;
};
struct StudioMesh { int numtris, triindex, skinref, numnorms, normindex; };
struct StudioTexture { char name[64]; int flags, width, height, index; };
struct StudioEvent { int frame, event, type; char options[64]; };
struct StudioAttachment { char name[32]; int type, bone; float org[3]; float vectors[3][3]; };
#pragma pack(pop)

static const int STUDIO_LOOPING = 0x0001;
static const int NF_CHROME = 0x0002, NF_FULLBRIGHT = 0x0004, NF_ADDITIVE = 0x0020, NF_MASKED = 0x0040;
static const INDEX MAX_BONES = 128, MAX_VERTS = 4096;

// --- a loaded model
struct Model {
  std::vector<UBYTE> aubData;
  std::vector<GLuint> aulTextures; // (in the GL context below)
  HGLRC hContext;
  BOOL bBroken;
  const StudioHeader *Header(void) const { return (const StudioHeader *)&aubData[0]; };
  template<class Type> const Type *At(int iOffset) const { return (const Type *)(&aubData[0] + iOffset); };
};
static std::map<std::string, Model *> _mapModels;

static Model *Load(const char *strName) {
  std::map<std::string, Model *>::iterator it = _mapModels.find(strName);
  if (it != _mapModels.end()) return it->second;
  Model *pm = new Model;
  pm->hContext = NULL;
  pm->bBroken = TRUE;
  _mapModels[strName] = pm;
  try {
    CTFileStream strm;
    strm.Open_t(CTFileName(CTString("Models\\SeriousHL\\") + strName));
    const SLONG slSize = strm.GetStreamSize();
    if (slSize > (SLONG)sizeof(StudioHeader)) {
      pm->aubData.resize(slSize);
      strm.Read_t(&pm->aubData[0], slSize);
    }
    strm.Close();
  } catch (char *strError) {
    CPrintF(SB_TAG " view model %s: %s\n", strName, strError);
    return pm;
  }
  if (pm->aubData.empty()) return pm;
  const StudioHeader *ph = pm->Header();
  const int ctSize = (int)pm->aubData.size();
  if (memcmp(&ph->id, "IDST", 4) != 0 || ph->version != 10 || ph->numbones <= 0 || ph->numbones > MAX_BONES
   || ph->boneindex + ph->numbones * (int)sizeof(StudioBone) > ctSize || ph->seqindex + ph->numseq * (int)sizeof(StudioSeq) > ctSize
   || ph->numtextures <= 0 || ph->textureindex + ph->numtextures * (int)sizeof(StudioTexture) > ctSize
   || ph->bodypartindex + ph->numbodyparts * (int)sizeof(StudioBodyPart) > ctSize) {
    CPrintF(SB_TAG " view model %s: not a Half-Life studio model (version 10, its textures in it)\n", strName);
    return pm;
  }
  pm->bBroken = FALSE;
  return pm;
};

// --- the animation (StudioCalcBoneQuaternion, StudioCalcBonePosition, StudioCalcRotations)
typedef float Vec3[3];
typedef float Quat[4];
typedef float Mat34[3][4];

static void AngleQuaternion(const Vec3 a, Quat q) {
  const float sy = sinf(a[2] * 0.5f), cy = cosf(a[2] * 0.5f);
  const float sp = sinf(a[1] * 0.5f), cp = cosf(a[1] * 0.5f);
  const float sr = sinf(a[0] * 0.5f), cr = cosf(a[0] * 0.5f);
  q[0] = sr * cp * cy - cr * sp * sy;
  q[1] = cr * sp * cy + sr * cp * sy;
  q[2] = cr * cp * sy - sr * sp * cy;
  q[3] = cr * cp * cy + sr * sp * sy;
};

static void QuaternionSlerp(const Quat p, Quat q, float t, Quat qt) {
  // (the shorter way round)
  float a = 0, b = 0;
  for (int i = 0; i < 4; i++) {
    a += (p[i] - q[i]) * (p[i] - q[i]);
    b += (p[i] + q[i]) * (p[i] + q[i]);
  }
  if (a > b) for (int i = 0; i < 4; i++) q[i] = -q[i];
  const float cosom = p[0] * q[0] + p[1] * q[1] + p[2] * q[2] + p[3] * q[3];
  float sclp, sclq;
  if ((1.0f + cosom) > 0.000001f) {
    if ((1.0f - cosom) > 0.000001f) {
      const float omega = acosf(cosom), sinom = sinf(omega);
      sclp = sinf((1.0f - t) * omega) / sinom;
      sclq = sinf(t * omega) / sinom;
    } else {
      sclp = 1.0f - t;
      sclq = t;
    }
    for (int i = 0; i < 4; i++) qt[i] = sclp * p[i] + sclq * q[i];
  } else {
    qt[0] = -q[1]; qt[1] = q[0]; qt[2] = -q[3]; qt[3] = q[2];
    sclp = sinf((1.0f - t) * 0.5f * 3.14159265f);
    sclq = sinf(t * 0.5f * 3.14159265f);
    for (int i = 0; i < 3; i++) qt[i] = sclp * p[i] + sclq * qt[i];
  }
};

static void QuaternionMatrix(const Quat q, Mat34 m) {
  m[0][0] = 1.0f - 2.0f * q[1] * q[1] - 2.0f * q[2] * q[2];
  m[1][0] = 2.0f * q[0] * q[1] + 2.0f * q[3] * q[2];
  m[2][0] = 2.0f * q[0] * q[2] - 2.0f * q[3] * q[1];
  m[0][1] = 2.0f * q[0] * q[1] - 2.0f * q[3] * q[2];
  m[1][1] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[2] * q[2];
  m[2][1] = 2.0f * q[1] * q[2] + 2.0f * q[3] * q[0];
  m[0][2] = 2.0f * q[0] * q[2] + 2.0f * q[3] * q[1];
  m[1][2] = 2.0f * q[1] * q[2] - 2.0f * q[3] * q[0];
  m[2][2] = 1.0f - 2.0f * q[0] * q[0] - 2.0f * q[1] * q[1];
};

static void ConcatTransforms(const Mat34 a, const Mat34 b, Mat34 out) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 4; j++) {
      out[i][j] = a[i][0] * b[0][j] + a[i][1] * b[1][j] + a[i][2] * b[2][j] + (j == 3 ? a[i][3] : 0.0f);
    }
  }
};

// one channel's two values around the frame (the runs: valid values stored, total frames covered)
static void AnimValues(const StudioAnimValue *pv, int k, float &v1, float &v2) {
  if (pv->num.total < pv->num.valid) k = 0;
  while (pv->num.total <= k) {
    k -= pv->num.total;
    pv += pv->num.valid + 1;
    if (pv->num.total < pv->num.valid) k = 0;
  }
  if (pv->num.valid > k) {
    v1 = pv[k + 1].value;
    if (pv->num.valid > k + 1) v2 = pv[k + 2].value;
    else v2 = pv->num.total > k + 1 ? v1 : pv[pv->num.valid + 2].value;
  } else {
    v1 = pv[pv->num.valid].value;
    v2 = pv->num.total > k + 1 ? v1 : pv[pv->num.valid + 2].value;
  }
};

static void BoneTransforms(const Model &m, int iSeq, float fFrame, Mat34 *amBones) {
  const StudioHeader *ph = m.Header();
  const StudioSeq &seq = m.At<StudioSeq>(ph->seqindex)[iSeq];
  const StudioBone *pbones = m.At<StudioBone>(ph->boneindex);
  // (sequence group 0: the animation is in the model's own file)
  const StudioSeqGroup &group = m.At<StudioSeqGroup>(ph->seqgroupindex)[0];
  const StudioAnim *panims = m.At<StudioAnim>(group.unused2 + seq.animindex);
  const int iFrame = (int)fFrame;
  const float s = fFrame - iFrame;
  for (int b = 0; b < ph->numbones; b++) {
    const StudioBone &bone = pbones[b];
    const StudioAnim &anim = panims[b];
    Vec3 pos, a1, a2;
    for (int j = 0; j < 3; j++) {
      pos[j] = bone.value[j];
      if (anim.offset[j] != 0) {
        float v1, v2;
        AnimValues((const StudioAnimValue *)((const UBYTE *)&anim + anim.offset[j]), iFrame, v1, v2);
        pos[j] += (v1 * (1.0f - s) + v2 * s) * bone.scale[j];
      }
      a1[j] = a2[j] = bone.value[j + 3];
      if (anim.offset[j + 3] != 0) {
        float v1, v2;
        AnimValues((const StudioAnimValue *)((const UBYTE *)&anim + anim.offset[j + 3]), iFrame, v1, v2);
        a1[j] = bone.value[j + 3] + v1 * bone.scale[j + 3];
        a2[j] = bone.value[j + 3] + v2 * bone.scale[j + 3];
      }
    }
    Quat q1, q2, q;
    AngleQuaternion(a1, q1);
    if (a1[0] != a2[0] || a1[1] != a2[1] || a1[2] != a2[2]) {
      AngleQuaternion(a2, q2);
      QuaternionSlerp(q1, q2, s, q);
    } else {
      memcpy(q, q1, sizeof(q));
    }
    Mat34 mLocal;
    QuaternionMatrix(q, mLocal);
    mLocal[0][3] = pos[0];
    mLocal[1][3] = pos[1];
    mLocal[2][3] = pos[2];
    if (bone.parent < 0 || bone.parent >= b) {
      memcpy(amBones[b], mLocal, sizeof(Mat34));
    } else {
      ConcatTransforms(amBones[bone.parent], mLocal, amBones[b]);
    }
  }
};

// --- textures: 8 bits with the palette after them, made RGBA at power-of-two sizes (index 255 clear when masked)
static INDEX PowerOfTwo(INDEX i) {
  INDEX p = 1;
  while (p < i && p < 1024) p <<= 1;
  return p;
};

static void MakeTextures(Model &m) {
  const HGLRC hNow = gl::xwglGetCurrentContext();
  if (m.hContext == hNow && !m.aulTextures.empty()) return;
  // (a new context - the old textures went with the old one)
  m.hContext = hNow;
  const StudioHeader *ph = m.Header();
  m.aulTextures.assign(ph->numtextures, 0);
  const StudioTexture *ptex = m.At<StudioTexture>(ph->textureindex);
  for (int t = 0; t < ph->numtextures; t++) {
    const StudioTexture &tex = ptex[t];
    if (tex.width <= 0 || tex.height <= 0 || tex.index + tex.width * tex.height + 768 > (int)m.aubData.size()) continue;
    const UBYTE *pubPixels = &m.aubData[tex.index];
    const UBYTE *pubPalette = pubPixels + tex.width * tex.height;
    const INDEX w2 = PowerOfTwo(tex.width), h2 = PowerOfTwo(tex.height);
    std::vector<ULONG> aul(w2 * h2);
    for (INDEX y = 0; y < h2; y++) {
      for (INDEX x = 0; x < w2; x++) {
        const UBYTE i = pubPixels[(y * tex.height / h2) * tex.width + (x * tex.width / w2)];
        const UBYTE *rgb = pubPalette + i * 3;
        const ULONG a = ((tex.flags & NF_MASKED) && i == 255) ? 0 : 255;
        aul[y * w2 + x] = rgb[0] | (rgb[1] << 8) | (rgb[2] << 16) | (a << 24);
      }
    }
    GLuint id = 0;
    gl::xglGenTextures(1, &id);
    gl::xglBindTexture(GL_TEXTURE_2D, id);
    gl::xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    gl::xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    gl::xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    gl::xglTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    gl::xglTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w2, h2, 0, GL_RGBA, GL_UNSIGNED_BYTE, &aul[0]);
    m.aulTextures[t] = id;
  }
};

// --- what's in hand
static CTString _strModel;
static Model *_pmModel = NULL;
static INDEX _iSeq = 0, _iIdle = 0, _iBody = 0;
static TIME _tmStart = 0;
static ULONG _ctFrames = 0, _ctTris = 0;
static BOOL _bDrawn = FALSE;

static TIME Now(void) { return _pTimer->CurrentTick(); };

void SetModel(const char *strModel, INDEX iDraw, INDEX iIdle) {
  if (strModel == NULL) {
    _strModel = "";
    _pmModel = NULL;
    return;
  }
  if (_strModel == strModel && _pmModel != NULL) return;
  _strModel = strModel;
  _pmModel = Load(strModel);
  _iBody = 0;
  _iIdle = iIdle;
  _iSeq = iDraw;
  _tmStart = Now();
};

// --- the sequences' events (the client's StudioEvent): 5001, 5011, 5021, 5031 a muzzle flash at attachment 0..3
// (R_MuzzleFlash: the options' last digit picks sprites/muzzleflash1..3, the tens are the scale in tenths), 5004 a sound
static SoundCallback _pSound = NULL;
static INDEX _iEventSeq = -1;
static TIME _tmEventStart = -1;
static float _fEventFrame = -1.0f;
struct Flash { INDEX iAttachment, iSprite, iFrame; float fScale; CTimerValue tvUntil; };
static Flash _aFlashes[4];
static INDEX _ctFlashes = 0;
static ULONG _ctEvents = 0, _ctFlashesDrawn = 0;
static float _afLastFlash[3] = { 0, 0, 0 };
static INDEX _iLastFlashWhy = 0;

void SetSoundCallback(SoundCallback pCallback) { _pSound = pCallback; };

static void Events(const Model &m, float fFrame) {
  const StudioHeader *ph = m.Header();
  if (_iSeq != _iEventSeq || _tmStart != _tmEventStart) {
    _iEventSeq = _iSeq;
    _tmEventStart = _tmStart;
    _fEventFrame = -1.0f;
  }
  if (fFrame < _fEventFrame) _fEventFrame = -1.0f; // (a loop went round)
  const StudioSeq &seq = m.At<StudioSeq>(ph->seqindex)[_iSeq];
  if (seq.numevents <= 0 || seq.eventindex + seq.numevents * (int)sizeof(StudioEvent) > (int)m.aubData.size()) {
    _fEventFrame = fFrame;
    return;
  }
  const StudioEvent *pev = m.At<StudioEvent>(seq.eventindex);
  for (int e = 0; e < seq.numevents; e++) {
    const StudioEvent &ev = pev[e];
    if (!(ev.frame > _fEventFrame && ev.frame <= fFrame)) continue;
    _ctEvents++;
    if (ev.event == 5001 || ev.event == 5011 || ev.event == 5021 || ev.event == 5031) {
      if (_ctFlashes >= 4) continue;
      const int iType = atoi(ev.options);
      Flash &fl = _aFlashes[_ctFlashes++];
      fl.iAttachment = (ev.event - 5001) / 10;
      fl.iSprite = (iType % 10) % 3;
      fl.fScale = (iType / 10) * 0.1f;
      if (fl.fScale == 0.0f) fl.fScale = 0.5f;
      fl.iFrame = rand();
      // (Half-Life's lasts till the next frame: here a few)
      fl.tvUntil = _pTimer->GetHighPrecisionTimer() + CTimerValue((double)hl_fMuzzleFlashTime);
    } else if (ev.event == 5004 && _pSound != NULL) {
      char strSound[64];
      strncpy(strSound, ev.options, 63);
      strSound[63] = 0;
      for (char *pch = strSound; *pch != 0; pch++) if (*pch == '/') *pch = '\\';
      _pSound(strSound);
    }
  }
  _fEventFrame = fFrame;
};

void Play(INDEX iSequence) {
  _iSeq = iSequence;
  _tmStart = Now();
};

void SetIdle(INDEX iIdle) { _iIdle = iIdle; };
void SetBody(INDEX iBody) { _iBody = iBody; };
BOOL Ready(void) { return _pmModel != NULL && !_pmModel->bBroken && hl_bViewModels; };

// the frame of the sequence now; a sequence done goes on to the idle one
static float Frame(const Model &m, TIME tmNow) {
  const StudioHeader *ph = m.Header();
  if (_iSeq < 0 || _iSeq >= ph->numseq) _iSeq = 0;
  const StudioSeq &seq = m.At<StudioSeq>(ph->seqindex)[_iSeq];
  if (seq.numframes <= 1) return 0.0f;
  float f = (float)(tmNow - _tmStart) * seq.fps;
  if (f < 0.0f) f = 0.0f;
  if (f >= seq.numframes - 1) {
    if (seq.flags & STUDIO_LOOPING) {
      f = fmodf(f, (float)(seq.numframes - 1));
    } else if (_iSeq != _iIdle && _iIdle >= 0 && _iIdle < ph->numseq) {
      _tmStart += (seq.numframes - 1) / Max(seq.fps, 0.001f);
      _iSeq = _iIdle;
      return Frame(m, tmNow);
    } else {
      f = seq.numframes - 1.001f;
    }
  }
  return f;
};

// --- the bob (view.cpp V_CalcBob: cl_bob 0.01, cl_bobcycle 0.8, cl_bobup 0.5): up and down with the running speed, the
// gun pushed ahead and tilted with it (V_CalcNormalRefdef); held while in the air
static double _tmBob = 0.0;
static float _fBob = 0.0f;
static CTimerValue _tvLast;
static BOOL _bLast = FALSE;

static float Bob(void) {
  const CTimerValue tvNow = _pTimer->GetHighPrecisionTimer();
  const double tmFrame = _bLast ? Clamp((tvNow - _tvLast).GetSeconds(), 0.0, 0.1) : 0.0;
  _tvLast = tvNow;
  _bLast = TRUE;
  if (!hl::OnGround() || tmFrame <= 0.0) return _fBob;
  _tmBob += tmFrame;
  float fCycle = (float)(_tmBob - (int)(_tmBob / 0.8) * 0.8) / 0.8f;
  fCycle = fCycle < 0.5f ? 3.14159265f * fCycle / 0.5f : 3.14159265f + 3.14159265f * (fCycle - 0.5f) / 0.5f;
  const float fBob = hl::Speed() * 0.01f;
  _fBob = Clamp(fBob * 0.3f + fBob * 0.7f * sinf(fCycle), -7.0f, 4.0f);
  return _fBob;
};

// mathlib.c AngleMatrix: (yaw x pitch) x roll, degrees
static void AngleMatrix(float fPitch, float fYaw, float fRoll, Mat34 m) {
  const float d = 3.14159265f / 180.0f;
  const float sy = sinf(fYaw * d), cy = cosf(fYaw * d), sp = sinf(fPitch * d), cp = cosf(fPitch * d);
  const float sr = sinf(fRoll * d), cr = cosf(fRoll * d);
  m[0][0] = cp * cy; m[1][0] = cp * sy; m[2][0] = -sp;
  m[0][1] = sr * sp * cy - cr * sy; m[1][1] = sr * sp * sy + cr * cy; m[2][1] = sr * cp;
  m[0][2] = cr * sp * cy + sr * sy; m[1][2] = cr * sp * sy - sr * cy; m[2][2] = cr * cp;
  m[0][3] = m[1][3] = m[2][3] = 0.0f;
};

// --- drawing: Half-Life model space (x ahead, y left, z up) into the eye's (x right, y up, z back)
static inline void ToEye(const float *v, float *e) {
  e[0] = -v[1];
  e[1] = v[2];
  e[2] = -v[0];
};

static void Draw(Model &m, CDrawPort *pdp, const FLOAT3D &vLightEye, FLOAT fAmbient, FLOAT fDiffuse) {
  const StudioHeader *ph = m.Header();
  static Mat34 amBones[MAX_BONES];
  const float fFrame = Frame(m, _pTimer->GetLerpedCurrentTick());
  BoneTransforms(m, _iSeq, fFrame, amBones);
  Events(m, fFrame);
  // the bob: the whole gun turned (yaw -bob/2, roll -bob, pitch -bob*0.3) and moved (ahead bob*0.4, up bob)
  const float fBob = Bob();
  if (fBob != 0.0f) {
    Mat34 mBob, mOut;
    AngleMatrix(-fBob * 0.3f, -fBob * 0.5f, -fBob, mBob);
    mBob[0][3] = fBob * 0.4f;
    mBob[2][3] = fBob;
    for (int b = 0; b < ph->numbones; b++) {
      ConcatTransforms(mBob, amBones[b], mOut);
      memcpy(amBones[b], mOut, sizeof(Mat34));
    }
  }
  MakeTextures(m);

  // the projection: Half-Life's field of view (horizontal at 4:3, kept vertical for wider screens), units as they are
  const float fFovY = 2.0f * atanf(tanf(hl_fViewModelFov * 0.5f * 3.14159265f / 180.0f) * 0.75f);
  const float fNear = 1.0f, fFar = 1024.0f;
  const float fAspect = (float)pdp->dp_Width / Max((float)pdp->dp_Height, 1.0f);
  const float fTop = fNear * tanf(fFovY * 0.5f);
  gl::xglMatrixMode(GL_PROJECTION);
  gl::xglPushMatrix();
  gl::xglLoadIdentity();
  gl::xglFrustum(-fTop * fAspect, fTop * fAspect, -fTop, fTop, fNear, fFar);
  gl::xglMatrixMode(GL_TEXTURE);
  gl::xglPushMatrix();
  gl::xglLoadIdentity();
  gl::xglMatrixMode(GL_MODELVIEW);
  gl::xglPushMatrix();
  gl::xglLoadIdentity();
  gl::xglViewport(pdp->dp_MinI, pdp->dp_Raster->ra_Height - (pdp->dp_MinJ + pdp->dp_Height), pdp->dp_Width, pdp->dp_Height);
  gl::xglDisable(GL_FOG);
  gl::xglDisable(GL_LIGHTING);
  gl::xglDisable(GL_CULL_FACE);
  gl::xglDisable(GL_STENCIL_TEST);
  gl::xglDisable(GL_SCISSOR_TEST);
  gl::xglDisable(GL_CLIP_PLANE0);
  gl::xglDisable(GL_TEXTURE_GEN_S);
  gl::xglDisable(GL_TEXTURE_GEN_T);
  gl::xglEnable(GL_DEPTH_TEST);
  gl::xglDepthFunc(GL_LEQUAL);
  gl::xglDepthRange(0.0, 0.1);
  gl::xglColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  gl::xglPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
  gl::xglShadeModel(GL_SMOOTH);
  gl::xglEnable(GL_TEXTURE_2D);
  gl::xglTexEnvi(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

  const short *pskins = m.At<short>(ph->skinindex);
  const StudioTexture *ptex = m.At<StudioTexture>(ph->textureindex);
  const StudioBodyPart *pparts = m.At<StudioBodyPart>(ph->bodypartindex);
  static float afVerts[MAX_VERTS][3], afLight[MAX_VERTS], afNormEye[MAX_VERTS][3];
  ULONG ctTris = 0;
  // two passes: solid (and masked) first, additive over them
  for (INDEX iPass = 0; iPass < 2; iPass++) {
    if (iPass == 0) {
      gl::xglDisable(GL_BLEND);
      gl::xglEnable(GL_ALPHA_TEST);
      gl::xglAlphaFunc(GL_GREATER, 0.5f);
      gl::xglDepthMask(GL_TRUE);
    } else {
      gl::xglDisable(GL_ALPHA_TEST);
      gl::xglEnable(GL_BLEND);
      gl::xglBlendFunc(GL_ONE, GL_ONE);
      gl::xglDepthMask(GL_FALSE);
    }
    for (int bp = 0; bp < ph->numbodyparts; bp++) {
      const StudioBodyPart &part = pparts[bp];
      if (part.nummodels <= 0) continue;
      const int iSub = (_iBody / Max(part.base, 1)) % part.nummodels;
      const StudioModel &sm = m.At<StudioModel>(part.modelindex)[iSub];
      if (sm.numverts <= 0 || sm.numverts > MAX_VERTS || sm.numnorms > MAX_VERTS) continue;
      const UBYTE *pvertbone = m.At<UBYTE>(sm.vertinfoindex);
      const UBYTE *pnormbone = m.At<UBYTE>(sm.norminfoindex);
      const Vec3 *pverts = m.At<Vec3>(sm.vertindex);
      const Vec3 *pnorms = m.At<Vec3>(sm.normindex);
      for (int v = 0; v < sm.numverts; v++) {
        const Mat34 &mb = amBones[pvertbone[v] < ph->numbones ? pvertbone[v] : 0];
        const float *p = pverts[v];
        float w[3];
        for (int i = 0; i < 3; i++) w[i] = p[0] * mb[i][0] + p[1] * mb[i][1] + p[2] * mb[i][2] + mb[i][3];
        ToEye(w, afVerts[v]);
      }
      for (int n = 0; n < sm.numnorms; n++) {
        const Mat34 &mb = amBones[pnormbone[n] < ph->numbones ? pnormbone[n] : 0];
        const float *p = pnorms[n];
        float w[3], e[3];
        for (int i = 0; i < 3; i++) w[i] = p[0] * mb[i][0] + p[1] * mb[i][1] + p[2] * mb[i][2];
        ToEye(w, e);
        memcpy(afNormEye[n], e, sizeof(e));
        // (Sam's light: its direction is where it shines to)
        const float fDot = -(e[0] * vLightEye(1) + e[1] * vLightEye(2) + e[2] * vLightEye(3));
        afLight[n] = Min(fAmbient + fDiffuse * Max(fDot, 0.0f), 1.0f);
      }
      const StudioMesh *pmeshes = m.At<StudioMesh>(sm.meshindex);
      for (int me = 0; me < sm.nummesh; me++) {
        const StudioMesh &mesh = pmeshes[me];
        const int iTex = pskins[mesh.skinref];
        if (iTex < 0 || iTex >= ph->numtextures) continue;
        const StudioTexture &tex = ptex[iTex];
        const BOOL bAdditive = (tex.flags & NF_ADDITIVE) != 0;
        if ((iPass == 1) != bAdditive) continue;
        gl::xglBindTexture(GL_TEXTURE_2D, m.aulTextures[iTex]);
        const float fS = 1.0f / Max(tex.width, 1), fT = 1.0f / Max(tex.height, 1);
        const BOOL bChrome = (tex.flags & NF_CHROME) != 0, bBright = (tex.flags & NF_FULLBRIGHT) != 0 || bAdditive;
        const short *pcmd = m.At<short>(mesh.triindex);
        for (int ct = *(pcmd++); ct != 0; ct = *(pcmd++)) {
          if (ct < 0) {
            gl::xglBegin(GL_TRIANGLE_FAN);
            ct = -ct;
          } else {
            gl::xglBegin(GL_TRIANGLE_STRIP);
          }
          ctTris += ct - 2;
          for (; ct > 0; ct--, pcmd += 4) {
            const int iv = pcmd[0] < sm.numverts ? pcmd[0] : 0, in = pcmd[1] < sm.numnorms ? pcmd[1] : 0;
            if (bChrome) {
              gl::xglTexCoord2f(0.5f + 0.5f * afNormEye[in][0], 0.5f - 0.5f * afNormEye[in][1]);
            } else {
              gl::xglTexCoord2f(pcmd[2] * fS, pcmd[3] * fT);
            }
            const float l = bBright ? 1.0f : afLight[in];
            gl::xglColor4f(l, l, l, 1.0f);
            gl::xglVertex3fv(afVerts[iv]);
          }
          gl::xglEnd();
        }
      }
    }
  }
  // the muzzle flashes: sprites facing the eye at their attachments, added
  const CTimerValue tvNow = _pTimer->GetHighPrecisionTimer();
  INDEX ctKept = 0;
  for (INDEX f = 0; f < _ctFlashes; f++) {
    Flash &fl = _aFlashes[f];
    if (tvNow > fl.tvUntil) continue;
    _aFlashes[ctKept++] = fl;
    _iLastFlashWhy = 1;
    if (fl.iAttachment >= ph->numattachments) continue;
    const StudioAttachment &att = m.At<StudioAttachment>(ph->attachmentindex)[fl.iAttachment];
    const Mat34 &mb = amBones[att.bone >= 0 && att.bone < ph->numbones ? att.bone : 0];
    float w[3], e[3];
    for (int i = 0; i < 3; i++) w[i] = att.org[0] * mb[i][0] + att.org[1] * mb[i][1] + att.org[2] * mb[i][2] + mb[i][3];
    ToEye(w, e);
    memcpy(_afLastFlash, e, sizeof(e));
    _iLastFlashWhy = 2;
    sprite::Sprite *ps = sprite::Load(CTString(0, "muzzleflash%d", fl.iSprite + 1));
    if (ps == NULL || ps->bBroken) continue;
    const INDEX iFrame = fl.iFrame % (INDEX)ps->aFrames.size();
    _iLastFlashWhy = 3;
    if (!sprite::Bind(ps, iFrame)) continue;
    _iLastFlashWhy = 4;
    _ctFlashesDrawn++;
    const sprite::Frame &fr = ps->aFrames[iFrame];
    const float x0 = e[0] + fr.iOriginX * fl.fScale, y0 = e[1] + fr.iOriginY * fl.fScale;
    const float x1 = x0 + fr.iW * fl.fScale, y1 = y0 - fr.iH * fl.fScale;
    const float u1 = (float)fr.iW / fr.iW2, v1 = (float)fr.iH / fr.iH2;
    gl::xglDisable(GL_ALPHA_TEST);
    gl::xglEnable(GL_BLEND);
    gl::xglBlendFunc(GL_ONE, GL_ONE);
    gl::xglDepthMask(GL_FALSE);
    gl::xglColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    gl::xglBegin(GL_QUADS);
    gl::xglTexCoord2f(0.0f, 0.0f); gl::xglVertex3f(x0, y0, e[2]);
    gl::xglTexCoord2f(u1, 0.0f); gl::xglVertex3f(x1, y0, e[2]);
    gl::xglTexCoord2f(u1, v1); gl::xglVertex3f(x1, y1, e[2]);
    gl::xglTexCoord2f(0.0f, v1); gl::xglVertex3f(x0, y1, e[2]);
    gl::xglEnd();
  }
  _ctFlashes = ctKept;

  gl::xglMatrixMode(GL_PROJECTION);
  gl::xglPopMatrix();
  gl::xglMatrixMode(GL_TEXTURE);
  gl::xglPopMatrix();
  gl::xglMatrixMode(GL_MODELVIEW);
  gl::xglPopMatrix();
  _ctTris = ctTris;
};

// the other texture units off while drawing (and back as they were)
typedef void (__stdcall *FActiveTexture)(GLenum);
static FActiveTexture _pglActiveTexture = NULL;
static const INDEX GL_TEXTURE0_ = 0x84C0, GL_ACTIVE_TEXTURE_ = 0x84E0, GL_MAX_TEXTURE_UNITS_ = 0x84E2;

static void Render(CEntity *penPlayer, CPerspectiveProjection3D &pr, CDrawPort *pdp, const FLOAT3D &vLightDir, COLOR colLight, COLOR colAmbient) {
  // the light in the eye's space (the view's rotation taken out)
  CPlacement3D plView = ((CPlayerEntity *)penPlayer)->en_plViewpoint;
  plView.RelativeToAbsolute(penPlayer->GetPlacement());
  FLOATmatrix3D mView;
  MakeRotationMatrixFast(mView, plView.pl_OrientationAngle);
  const FLOAT3D vLightEye = vLightDir * !mView;
  UBYTE ubLR, ubLG, ubLB, ubAR, ubAG, ubAB;
  ColorToRGB(colLight, ubLR, ubLG, ubLB);
  ColorToRGB(colAmbient, ubAR, ubAG, ubAB);
  // (bright enough, as Sam's weapon: at least 32 of each over the darkest)
  const FLOAT fAmbient = Clamp((Max(Max(ubAR, ubAG), ubAB) + 32) / 255.0f, 0.25f, 1.0f);
  const FLOAT fDiffuse = Clamp((Max(Max(ubLR, ubLG), ubLB) + 32) / 255.0f, 0.0f, 1.0f);

  if (_pglActiveTexture == NULL) _pglActiveTexture = (FActiveTexture)gl::xwglGetProcAddress("glActiveTextureARB");
  GLint iActive = GL_TEXTURE0_, ctUnits = 1;
  BOOL abWasOn[4] = { FALSE, FALSE, FALSE, FALSE };
  if (_pglActiveTexture != NULL) {
    gl::xglGetIntegerv((GLenum)GL_ACTIVE_TEXTURE_, &iActive);
    gl::xglGetIntegerv((GLenum)GL_MAX_TEXTURE_UNITS_, &ctUnits);
    ctUnits = Min((INDEX)ctUnits, (INDEX)4);
    for (GLint u = 1; u < ctUnits; u++) {
      _pglActiveTexture((GLenum)(GL_TEXTURE0_ + u));
      abWasOn[u] = gl::xglIsEnabled(GL_TEXTURE_2D);
      gl::xglDisable(GL_TEXTURE_2D);
    }
    _pglActiveTexture((GLenum)GL_TEXTURE0_);
  }
  gl::xglPushAttrib(GL_ALL_ATTRIB_BITS);
  gl::xglPushClientAttrib(GL_CLIENT_ALL_ATTRIB_BITS);
  Draw(*_pmModel, pdp, vLightEye, fAmbient, fDiffuse);
  gl::xglPopClientAttrib();
  gl::xglPopAttrib();
  if (_pglActiveTexture != NULL) {
    for (GLint u = 1; u < ctUnits; u++) {
      if (!abWasOn[u]) continue;
      _pglActiveTexture((GLenum)(GL_TEXTURE0_ + u));
      gl::xglEnable(GL_TEXTURE_2D);
    }
    _pglActiveTexture((GLenum)iActive);
  }
  _ctFrames++;
  _bDrawn = TRUE;
};

// --- CPlayerWeapons::RenderWeaponModel (the Entities library): Half-Life's model there when there's one
typedef void (CEntity::*CRenderWeaponModel)(CPerspectiveProjection3D &, CDrawPort *, FLOAT3D, COLOR, COLOR, BOOL, INDEX);
static CRenderWeaponModel _pRenderWeaponModel = NULL;
static BOOL _bHooked = FALSE;
static const ULONG PROP_WEAPONS = (401UL << 8) | 16;

class CPlayerWeaponsHook : public CEntity {
public:
  void P_RenderWeaponModel(CPerspectiveProjection3D &pr, CDrawPort *pdp, FLOAT3D vLightDir, COLOR colLight, COLOR colAmbient,
      BOOL bRender, INDEX iEye) {
    CEntity *penPlayer = player::Local();
    CEntity *penOurs = penPlayer != NULL ? player::PropEntity(penPlayer, PROP_WEAPONS) : NULL;
    const BOOL bOurs = penOurs != NULL && (this == penOurs || (IsPredictor() && GetPredicted() == penOurs));
    if (!bRender || !bOurs || !Ready() || _pGfx->GetCurrentAPI() != GAT_OGL || !gl::Load() || !(penPlayer->GetFlags() & ENF_ALIVE)) {
      _bDrawn = FALSE;
      (this->*_pRenderWeaponModel)(pr, pdp, vLightDir, colLight, colAmbient, bRender, iEye);
      return;
    }
    Render(penPlayer, pr, pdp, vLightDir, colLight, colAmbient);
  };
};

BOOL Init(void) {
  StructPtr ptr(ClassicsCore_GetEntitiesSymbol(
    "?RenderWeaponModel@CPlayerWeapons@@QAEXAAVCPerspectiveProjection3D@@PAVCDrawPort@@V?$Vector@M$02@@KKHJ@Z"));
  if (ptr.iAddress == NULL) {
    CPrintF(SB_TAG " (CPlayerWeapons::RenderWeaponModel isn't in the Entities library: Sam's weapon models stay)\n");
    return FALSE;
  }
  _pRenderWeaponModel = ptr(CRenderWeaponModel());
  HFuncPatch hPatch = CreatePatch(_pRenderWeaponModel, &CPlayerWeaponsHook::P_RenderWeaponModel, "SeriousHL: CPlayerWeapons::RenderWeaponModel()");
  _bHooked = hPatch != NULL && hPatch->IsPatched();
  return _bHooked;
};

void Shutdown(void) {
  // (the GL textures go with the context; only the memory here)
  for (std::map<std::string, Model *>::iterator it = _mapModels.begin(); it != _mapModels.end(); it++) delete it->second;
  _mapModels.clear();
  _pmModel = NULL;
};

CTString Status(void) {
  CTString str;
  INDEX ctLoaded = 0;
  for (std::map<std::string, Model *>::iterator it = _mapModels.begin(); it != _mapModels.end(); it++) {
    if (!it->second->bBroken) ctLoaded++;
  }
  str.PrintF("view models %s; %d loaded; in hand %s, sequence %d (idle %d), %s, %u frames drawn, %u triangles, %u events, flashes %u drawn (last at %.1f %.1f %.1f, stage %d)",
    !_bHooked ? "unavailable" : hl_bViewModels ? "Half-Life's" : "Sam's", ctLoaded,
    _strModel == "" ? "Sam's" : _strModel.str_String, _iSeq, _iIdle, _bDrawn ? "drawn" : "not drawn", _ctFrames, _ctTris, _ctEvents, _ctFlashesDrawn, _afLastFlash[0], _afLastFlash[1], _afLastFlash[2], _iLastFlashWhy);
  return str;
};

}; // namespace
