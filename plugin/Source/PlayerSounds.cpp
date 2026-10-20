/* SeriousHL - Half-Life's player sounds. See PlayerSounds.h. */
#include "StdH.h"
#include "PlayerSounds.h"
#include "Player.h"
#include "Sounds.h"
#include "SBridge.h"

#include <stdlib.h>

namespace playersounds {

INDEX hl_bPlayerSounds = TRUE;

// Player.es's own sound objects (class 401): the mouth (jumps, pain), the feet, the body (splashes)
static const ULONG PROP_SO_MOUTH = (401UL << 8) | 70, PROP_SO_FOOTL = (401UL << 8) | 71;
static const ULONG PROP_SO_FOOTR = (401UL << 8) | 72, PROP_SO_BODY = (401UL << 8) | 73;

// pm_shared.c's step kinds and their four sounds (as PM_PlayStepSound orders them: right foot 0-1, left 2-3)
enum Step { STEP_CONCRETE, STEP_METAL, STEP_DIRT, STEP_TILE, STEP_SLOSH, STEP_WADE, STEP_COUNT };
static const char *_aastrStep[STEP_COUNT][4] = {
  { "player\\pl_step1.wav", "player\\pl_step3.wav", "player\\pl_step2.wav", "player\\pl_step4.wav" },
  { "player\\pl_metal1.wav", "player\\pl_metal3.wav", "player\\pl_metal2.wav", "player\\pl_metal4.wav" },
  { "player\\pl_dirt1.wav", "player\\pl_dirt3.wav", "player\\pl_dirt2.wav", "player\\pl_dirt4.wav" },
  { "player\\pl_tile1.wav", "player\\pl_tile3.wav", "player\\pl_tile2.wav", "player\\pl_tile4.wav" },
  { "player\\pl_slosh1.wav", "player\\pl_slosh3.wav", "player\\pl_slosh2.wav", "player\\pl_slosh4.wav" },
  { "player\\pl_wade1.wav", "player\\pl_wade2.wav", "player\\pl_wade3.wav", "player\\pl_wade4.wav" },
};
static const char *_astrStepName[STEP_COUNT] = { "concrete", "metal", "dirt", "tile", "slosh", "wade" };

static CSoundObject *_psoStep = NULL, *_psoVoice = NULL, *_psoBody = NULL;
static FLOAT _fStepTime = 0.0f, _fSwimTime = 0.0f; // milliseconds, as pmove's timers
static BOOL _bStepLeft = FALSE;
static INDEX _iSkipStep = 0;
static FLOAT _fLastHealth = -1.0f;
static TIME _tmLastPain = -1.0f;
static ULONG _ctSteps = 0, _ctPains = 0, _ctLandings = 0;
static Step _stLast = STEP_CONCRETE;

void Init(void) {
  _psoStep = new CSoundObject;
  _psoVoice = new CSoundObject;
  _psoBody = new CSoundObject;
};

static void Play(CSoundObject *pso, CEntity *pen, const char *strSound, FLOAT fVolume) {
  CSoundData *psd = sounds::Data(strSound);
  if (pso == NULL || psd == NULL || fVolume <= 0.0f) return;
  pso->SetOwner(pen);
  // (ATTN_NORM: heard well round him)
  pso->Set3DParameters(30.0f, 3.0f, fVolume, 1.0f);
  pso->Play(psd, SOF_3D | SOF_VOLUMETRIC);
};

// what he stands on: Sam's surface's name (Sand, Red sand, Grass, Snow...: dirt; Water: slosh; Metal; else concrete)
static Step Surface(CEntity *pen) {
  CMovableEntity *pmen = (CMovableEntity *)pen;
  CWorld *pwo = pen->GetWorld();
  const INDEX i = pmen->en_iReferenceSurface;
  if (pwo == NULL || i < 0 || i >= pwo->wo_astSurfaceTypes.Count()) return STEP_CONCRETE;
  char str[64];
  strncpy(str, pwo->wo_astSurfaceTypes[i].st_strName.str_String, sizeof(str) - 1);
  str[sizeof(str) - 1] = 0;
  _strlwr(str);
  if (strstr(str, "sand") || strstr(str, "grass") || strstr(str, "snow") || strstr(str, "dirt") || strstr(str, "earth")) return STEP_DIRT;
  if (strstr(str, "water")) return STEP_SLOSH;
  if (strstr(str, "metal")) return STEP_METAL;
  if (strstr(str, "tile")) return STEP_TILE;
  return STEP_CONCRETE;
};

static Step StepHere(CEntity *pen, const rules::Output &out) {
  if (out.iWaterLevel >= 2) return STEP_WADE;
  if (out.iWaterLevel == 1) return STEP_SLOSH;
  return Surface(pen);
};

// PM_PlayStepSound: the feet in turn, one of two sounds each (wading: every fourth step left out... as Half-Life's)
static void PlayStep(CEntity *pen, Step st, FLOAT fVolume) {
  _bStepLeft = !_bStepLeft;
  if (st == STEP_WADE) {
    if (_iSkipStep == 0) {
      _iSkipStep++;
      return;
    }
    if (_iSkipStep++ == 3) _iSkipStep = 0;
  }
  const INDEX iRand = (rand() & 1) + (_bStepLeft ? 2 : 0);
  Play(_psoStep, pen, _aastrStep[st][iRand], fVolume);
  _stLast = st;
  _ctSteps++;
};

static void Quiet(CEntity *pen, ULONG ulProp) {
  CSoundObject *pso = (CSoundObject *)player::Prop(pen, ulProp);
  if (pso != NULL && pso->IsPlaying()) pso->Stop();
};

void Tick(CEntity *pen, const rules::Input &in, const rules::Output &out, const rules::Memory &mem) {
  if (!hl_bPlayerSounds || pen == NULL) return;
  // Sam's own: his steps, his jump's and pain's voice, his splashes (all of this tick's are still to be mixed)
  Quiet(pen, PROP_SO_FOOTL);
  Quiet(pen, PROP_SO_FOOTR);
  Quiet(pen, PROP_SO_BODY);
  Quiet(pen, PROP_SO_MOUTH);

  const FLOAT fMs = in.tmTick * 1000.0f;
  _fStepTime = Max(_fStepTime - fMs, 0.0f);
  _fSwimTime = Max(_fSwimTime - fMs, 0.0f);

  // PM_Jump: a step at full volume; in water the swim sound, not more than once a second
  if (out.bJumped) PlayStep(pen, StepHere(pen, out), 1.0f);
  if (out.iWaterLevel >= 2 && in.bJump && _fSwimTime <= 0.0f) {
    _fSwimTime = 1000.0f;
    static const char *astrSwim[4] = { "player\\pl_swim1.wav", "player\\pl_swim2.wav", "player\\pl_swim3.wav", "player\\pl_swim4.wav" };
    Play(_psoBody, pen, astrSwim[rand() & 3], 1.0f);
  }

  // PM_CheckFalling: landing from 350 u/s - a step, louder from 580 with the pain of it
  if (out.fFall >= 350.0f) {
    FLOAT fVolume = 0.85f;
    if (out.fFall > 580.0f) {
      fVolume = 1.0f;
      Play(_psoVoice, pen, "player\\pl_fallpain3.wav", 1.0f);
    }
    _fStepTime = 0.0f;
    PlayStep(pen, StepHere(pen, out), fVolume);
    _ctLandings++;
  }

  // PM_UpdateStepSound: on the ground and moving, when the last step's time is up
  if (_fStepTime <= 0.0f && out.bGround && out.fSpeed > 0.0f) {
    const BOOL bDucked = mem.bDucked;
    const FLOAT fVelWalk = bDucked ? 60.0f : 120.0f, fVelRun = bDucked ? 80.0f : 210.0f, fDuck = bDucked ? 100.0f : 0.0f;
    const BOOL bWalking = out.fSpeed < fVelRun;
    const Step st = StepHere(pen, out);
    FLOAT fVolume;
    if (st == STEP_WADE) {
      fVolume = 0.65f;
      _fStepTime = 600.0f;
    } else {
      fVolume = st == STEP_DIRT ? (bWalking ? 0.25f : 0.55f) : (bWalking ? 0.2f : 0.5f);
      _fStepTime = bWalking ? 400.0f : 300.0f;
    }
    _fStepTime += fDuck;
    if (bDucked) fVolume *= 0.35f;
    // (moving slower than a walk still steps: the time was up, as Half-Life's !pmove->flTimeStepSound)
    (void)fVelWalk;
    PlayStep(pen, st, fVolume);
  }

  // pain: health lost to anything but a fall (pl_pain5..7)
  const FLOAT fHealth = ((CLiveEntity *)pen)->en_fHealth;
  const TIME tmNow = _pTimer->CurrentTick();
  if (_fLastHealth >= 0.0f && fHealth < _fLastHealth - 0.5f && fHealth > 0.0f && out.fFallDamage <= 0.0f && tmNow - _tmLastPain > 0.3f) {
    static const char *astrPain[3] = { "player\\pl_pain5.wav", "player\\pl_pain6.wav", "player\\pl_pain7.wav" };
    Play(_psoVoice, pen, astrPain[rand() % 3], 1.0f);
    _tmLastPain = tmNow;
    _ctPains++;
  }
  _fLastHealth = fHealth;
};

void Release(void) {
  CSoundObject *apso[3] = { _psoStep, _psoVoice, _psoBody };
  for (INDEX i = 0; i < 3; i++) {
    if (apso[i] == NULL) continue;
    apso[i]->Stop();
    apso[i]->SetOwner(NULL);
  }
  _fLastHealth = -1.0f;
  _fStepTime = _fSwimTime = 0.0f;
};

CTString Status(void) {
  CTString str;
  str.PrintF("player sounds %s; %u steps (last on %s), %u landings, %u pains",
    hl_bPlayerSounds ? "Half-Life's" : "Sam's", _ctSteps, _astrStepName[_stLast], _ctLandings, _ctPains);
  return str;
};

}; // namespace
