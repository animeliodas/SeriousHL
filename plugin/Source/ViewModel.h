/* SeriousHL - Half-Life's view models (the gun in your hands) drawn instead of Sam's.
 *
 * The models are Half-Life's own v_*.mdl (Studio models, version 10), taken by the installer from the player's own
 * Half-Life into Models\SeriousHL (never shipped). Read here as the Half-Life SDK's studio.h lays them out: bones,
 * sequences with their compressed animation (StudioCalcRotations: runs of values per bone and channel, Euler angles to
 * quaternions, slerp between frames), body parts, meshes as triangle strips and fans, 8-bit textures with a palette.
 * Drawn in Sam's own place for the weapon (CPlayerWeapons::RenderWeaponModel, after the world, before the HUD) with
 * OpenGL - the engine's API - in the view's space: Half-Life's model space (x ahead, y left, z up, units) turned to the
 * eye's, the vertical field of view Half-Life's 90 degrees at 4:3 gives. All of the GL state is saved and put back.
 * The weapons' code (Weapons.cpp) plays the sequences: the Half-Life SDK's weapon animation numbers.
 */
#ifndef SERIOUSHL_VIEWMODEL_H
#define SERIOUSHL_VIEWMODEL_H

#include "StdH.h"

namespace viewmodel {

extern INDEX hl_bViewModels; // 1: Half-Life's view models where there is one; 0: Sam's weapon models
extern FLOAT hl_fViewModelFov; // the horizontal field of view at 4:3 the view model is drawn with (Half-Life: 90)
extern FLOAT hl_fMuzzleFlashTime; // how long a muzzle flash shows, seconds (Half-Life's: till the next frame)

// Plugin startup: the weapon model's drawing patched. FALSE: Sam's models stay.
BOOL Init(void);
void Shutdown(void);

// The model in hand (a file in Models\SeriousHL, "v_crowbar.mdl"; NULL: Sam's own); a new one starts its draw sequence.
void SetModel(const char *strModel, INDEX iDraw, INDEX iIdle);
// A sequence (the SDK's number) from now; when it ends (not a looping one), the idle one.
void Play(INDEX iSequence);
// The idle sequence to come back to.
void SetIdle(INDEX iIdle);
// The body group values (bodypart's sub-model numbers packed as the SDK's pev->body).
void SetBody(INDEX iBody);
BOOL Ready(void); // a model is loaded and drawn for the gun in hand
// The model's own sound events (5004: "items/9mmclip1.wav", Half-Life's sound folder's) go to this.
typedef void (*SoundCallback)(const char *strSound);
void SetSoundCallback(SoundCallback pCallback);
CTString Status(void);

// --- Half-Life's things in the world (a tripmine on a wall and its beam), drawn after Sam's world (the OnRenderView
// event), in its depth.
extern INDEX hl_bWorldModels; // 1: drawn
extern INDEX hl_bWorldDebug;  // tests: Sam's depth buffer read back, in the status
// A model: its file, sequence and frame, body groups; where (Sam's world), its axes there (Half-Life's x ahead, y left,
// z up), metres a unit, how lit (0..1).
struct Placed {
  const char *strModel;
  INDEX iSeq;
  FLOAT fFrame;
  INDEX iBody;
  FLOAT3D vPos, vAhead, vLeft, vUp;
  FLOAT fUnit, fLight;
};
// A beam: a sprite (Models\SeriousHL\sprites, "laserbeam") from one end to the other, this wide (metres), its colour
// (0..1, added); bFromEye: vFrom is in the eye's space (metres: x right, y up, z back - a view model's attachment).
struct Beam {
  FLOAT3D vFrom, vTo;
  FLOAT fWidth;
  FLOAT fR, fG, fB;
  const char *strSprite;
  BOOL bFromEye;
};
// A glow: a sprite's frame facing the eye at a place, metres a pixel of it, its colour (added).
struct Glow {
  FLOAT3D vPos;
  FLOAT fSize;
  const char *strSprite;
  INDEX iFrame;
  FLOAT fR, fG, fB;
};
void DrawWorld(CAnyProjection3D &apr, CDrawPort *pdp, const Placed *aPlaced, INDEX ctPlaced, const Beam *aBeams, INDEX ctBeams,
  const Glow *aGlows, INDEX ctGlows);
// Where the gun in hand's attachment was last drawn: the eye's space, Half-Life's units. FALSE: none such.
BOOL Attachment(INDEX iAttachment, FLOAT3D &vEye);

}; // namespace

#endif
