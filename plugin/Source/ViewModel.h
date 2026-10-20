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
CTString Status(void);

}; // namespace

#endif
