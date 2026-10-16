# SeriousHL — журнал

Half-Life 1 movement for Serious Sam Classic TFE 1.05 / TSE 1.07 (Classics Patch 1.9.4 plugin), a SeriousBridge port.
Route: rules rewritten in the host (pattern "guest's rules headless"), on the SeriousBridge core's movement takeover.

## Факты

1. Sam's ground movement (MovableEntity.es, PreMoving): `AddAccelerationOnPlane(current, desired, en_fAcceleration·dt²,
   en_fDeceleration·dt²)` every tick; Player.es sets `en_fAcceleration = plr_fAcceleration` (100), deceleration 60 in
   ActiveActions every action. Jump: `fJump` from desired y, allowed only after 0.25 s without significant vertical
   movement (`en_tmLastSignificantVerticalMovement`) — kills bunny hopping. Air control 0.5 s after a jump
   (`en_tmMaxJumpControl`, ×0.5). A "walking" player (keys under half speed) has `en_fStepDnHeight = 1.5`: the move
   fails at ledges (`WouldFallInNextPosition`).
2. `plr_fAcceleration` & co. are **not** shell symbols (`#if 0` in Player.es): can't be zeroed from the console.
3. So the core patches `CMovableEntity::PreMoving` (engine export `?PreMoving@CMovableEntity@@UAEXXZ`, both engines)
   with Classics Patch's `CreatePatch`: for the tracked player it sets acceleration/deceleration 0, removes desired y > 0,
   `en_fStepDnHeight = -1`, gives the port's velocity, then calls the original. Player.es's PreMoving → CPlayerEntity →
   CMovableModelEntity → CMovableEntity::PreMoving (direct call): the patch catches it.
4. Gravity: level forces (WorldBase "Normal": 30 m/s², terminal 70) recomputed each tick; Sam adds `gA·dt` in the air
   unless the result passes the terminal speed (then nothing), along the floor plane on the ground, full on a reference
   moving in y. The core subtracts exactly that from what it gives.
5. What the world did = `en_vCurrentTranslationAbsolute` (applied/dt after the move) − what was meant (+ the
   reference's velocity); added to the rules' end velocity: walls, floors, ceilings, explosions, teleports carry over.
6. Impact damage (`en_fCollisionSpeedLimit` 20 m/s, any direction) off with `EPF_NOIMPACTTHISTICK` every tick; HL fall
   damage instead (580..1024 u/s).
7. View heading for the wish: body heading + `en_plViewpoint` heading + `en_aDesiredRotationRelative(1)·dt` (Player.es
   turns the body to the view while moving); interpolated from the last tick across the HL frames.
8. HL at 20 Hz gains far less from strafing (air accel per frame capped by 30 u/s): 5 frames per tick (`hl_iFps` 100)
   — offline model `tools/model_bhop.py`: 15°/tick strafe → 622 u/s at 100 fps vs ~226 at 20.
9. `StartMap` from the menu inherits the intro's flyover mode (spawnflags 0008ffff): TFE 11_AlleyOfSphinxes hangs in
   `CAnimationChanger` events forever (watchdog stack: HandleSentEvents → CAnimationChanger::Main → SendEvent). Set
   `gam_iStartMode=0; gam_iStartDifficulty=1;` first. Not the plugin (hangs with the hook off too).
10. Classics Patch plugin load: `'X.dll' loading cancelled: Mismatching utility flags` lines are the first pass (other
    flags) — normal.

## Журнал

- 2026-10-13: 0.1.0 — run/friction/air strafe/jump/bhop/cap/edge/fall damage; oracle `tools/test_hl.py` 6/6 TFE and
  6/6 TSE (all three ports installed together); installer `install/Install.cmd` tested in a sandbox (patch from zip,
  SHA-256 mismatch refused, uninstall), game discovery finds Steam TFE/TSE.
- 2026-10-13: 0.2.0 — HL duck (in the air at once, feet up 18 u; on the ground after 0.4 s; ducktap pops 18 u),
  Half-Life damage pushes (`CPlayer::DamageImpact` patched: damage × 5 u/s away, ×2 ducked, ≤ 1000), keys from the
  applied action (`CPlayer::ApplyAction`), the rules as one pure step (`Rules.cpp`), the speedrun build (`build.sh all
  run`: ruleset fixed, no `hl_*`, no test hooks, the DLL's SHA-256 on screen). Oracles: `test_hl.py` 10/10 TFE (TSE:
  each check passed), `test_run.py` 4/4 TFE and TSE.

## Факты (0.2.0)

11. `PLACT_SELECT_WEAPON_SHIFT` is 9 in TFE's Player.es, 14 in TSE's (TSE adds use-held, sniper and serious bomb bits);
    the value is the weapon key slot (5: rocket launcher), not the WeaponType.
12. `cht_bGod` stops all damage before `DamageImpact`: tests of damage pushes turn it off.
13. In-game demos are broken in this setup (the user): not used for verification.
