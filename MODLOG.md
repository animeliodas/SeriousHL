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

## Факты (0.3.0, оружие)

14. Sam's PlayerWeapons (class 402) exports its fire helpers (FireOneBullet, FireBullets, FireMachineBullet,
    CutWithKnife, FireRocket, FireGrenade, DoRecoil, SetFlare, SpawnRangeSound, ...): calling them on the weapons entity
    gives Sam's bullets, holes, blood and projectiles with any damage and spread. `FireBullets`' positions are offsets
    × range × stretch: Half-Life's cone per pellet with stretch 1 and no jitter.
15. A component's id is `(class << 8) | n` (n as in the .es), e.g. PlayerWeapons' SOUND_COLT_FIRE = (402 << 8) | 37.
    With the bare n `ComponentForTypeAndID` returns NULL - and Classics Patch's Accessibility plugin patches
    `CEntity::PlaySound` and dereferences it: a crash inside Accessibility.dll.
16. Weapon model animations by name at run time (`CAnimObject::GetAnimsCt/GetAnimInfo`): knife PULL, ATTACK01/02;
    colt FIRE1-3, RELOAD; shotguns FIRE1/FIRE1FAST, FIRE/FIREFAST; tommygun, rocket and grenade launchers, laser FIRE.
17. Ticks' times are float sums: `t + 0.1` can come out a hair short of two ticks later - compare times with 0.01 s of
    slack, or a 0.1 s gun fires every 0.15 s.
18. The client sends an action every frame, more often than the game ticks: test hooks count ticks, and a tick counts
    only once an action carrying the keys has gone (a console command can land between them).
19. `PLACT_USE|PLACT_COMPUTER` come together on one press of Sam's "Use/Invoke NETRICSA" (right mouse button), only on
    the press: a held second fire is read from the key itself (`GetAsyncKeyState`) and those bits are taken out.

## Журнал (0.3.0)

- 2026-10-16: 0.3.0 — Half-Life's weapons on Sam's guns (crowbar, .357, Glock, shotgun, MP5 + grenade, RPG, hand
  grenades, gauss with charged shot and boost), the core's action filter. Oracles: `test_weapons.py` 5/5 TFE and TSE,
  `test_hl.py` 10/10 TFE and TSE.

## Факты (0.4.0, таймер)

20. Sam's controls add jump and crouch into one number (`pa_vTranslation(2)` = up - down): both held come out as 0 and
    Sam stands up. `ctl_bMoveUp` / `ctl_bMoveDown` are shell symbols (the local controls): the core puts both into the
    action (crouch, plus its own button bit 31), and takes the bit out before Player.es.
21. Player.es (401): `m_bEndOfLevel` (96) is set when the level's stats are recorded (its end), `m_bEndOfGame` (97) at
    the end of the game - the splits. The intro behind the menu is a game too, started with `gam_iStartMode` -1
    (flyover): not a run.
22. Single player pauses (no ticks) with a menu, the console or Netricsa up, and on the Pause key (`TogglePause`): the
    IGT is the ticks.
23. WorldLink (class 214, its name "World link"; m_strWorld = 2) changes the level on an ETrigger.
24. LiveSplit's server: one command a line (`reset`, `starttimer`, `initgametime`, `pausegametime`, `setgametime
    h:mm:ss.fff`, `split`), TCP 16834; with its game time paused LiveSplit shows exactly what is set.

## Журнал (0.4.0)

- 2026-10-16: 0.4.0 — the speedrun tools (core Speedrun: RTA/IGT timer, LiveSplit; the speedometer), the jump while
  ducked. Oracle `test_speedrun.py` 7/7 TFE and TSE.
