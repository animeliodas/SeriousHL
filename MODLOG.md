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
- 2026-10-16: 0.4.1 — the timer, LiveSplit and the speedometer moved out into their own plugin, SeriousSpeedrun (for
  runs of the game as it is too); SeriousHL keeps the jump while ducked (`test_hl.py duckhold`).

## Факты (0.5.0, полный перенос HL)

25. The engine's WAV loader wants RIFF/WAVE, `fmt `, an optional `fact`, then `data` - Half-Life's own wavs carry
    `LIST`/`cue ` chunks (8-bit 22050 Hz mono): the installer rewrites each into a plain RIFF (fmt 16 + data).
26. `CEntity::PlaySound(so, fnm, flags)` plays any file by name: Half-Life's sounds go from Sounds/SeriousHL/ next to the
    game, copied by the installer from the player's own Half-Life (valve/sound), never shipped.
27. A PowerShell script's new optional parameter takes positional arguments before the old ones: `install.ps1 <folder>`
    bound the folder to `-HalfLife` and the script went looking for the games on its own. `[CmdletBinding(
    PositionalBinding = $false)]` now: no parameter binds by position.
28. Player.es ReceiveDamage: difficulty first (`sp_fDamageStrength`, not one's own when > 1), then armour 2/3 a point
    for a point, then `DamageImpact` with the health part, then CPlayerEntity::ReceiveDamage. Half-Life's armour sits in
    front: Sam's code gets the health part (difficulty taken back out) with the armour set to 0 for the call.
29. `DMT_IMPACT` is also the monsters' (EnemyBase, the Devil, rolling stones): only the movement's own one (inflictor =
    the player) counts as a fall. The gauss overcharging goes through the armour (`hev::SelfDamage`).

## Журнал (0.5.0)

- 2026-10-16: Half-Life's weapon sounds from the player's Half-Life (`install.ps1` asset step, `-AssetsOnly`); the HEV
  armour (`Hev.cpp`, `hl_bHevArmor`). Oracle `test_weapons.py sounds,hev,gauss` TFE and TSE PASS.
30. Sam's guns set their sound object's 3D parameters before each sound (`Set3DParameters(25, 1, 1.5, 1)` for a shot)
    and leave them as they were: the tommygun and the minigun end on "mute fire" (volume 0), the laser on pitch 0.31.
    A sound played into the object without setting them is silent - while the plugin's own counter said "played".
    Now the plugin sets them every time. Oracle: Windows' audio session meter of the game process
    (`tools/audio_meter.ps1`, IAudioMeterInformation), a .357 shot against the quiet (`test_weapons.py sounds`).
31. The sound's data is held by the plugin (`_pSoundStock->Obtain_t`, released at shutdown) and played with
    `CSoundObject::Play` - `CEntity::PlaySound(file)` releases the data right after starting it.
32. Egon: single player's wide beam (`m_fireMode = FIRE_WIDE`), 14 (`sk_plr_egon_wide`) and a cell every 0.1 s, the
    first after 0.1 s; windup, the run loop after 2 s, off. Satchel: Sam's grenade (`FireGrenade`) turned: no fuse (its
    `wait(m_fFlyTime)` timer moved an hour on; set off by moving it to now), desired translation 0 (it stops instead of
    going off when stuck), debris collision (only brushes stop it, nothing sets it off), 150 over 2.5 × 150 units.
    Crossbow: TSE's `FireSniperBullet`, 50 (`sk_plr_xbow_bolt_monster`), from the view's middle while scoped.
33. Rare flake: once in four runs the gauss's single shot counted 40 on the target (twice 20).
34. Water (the core takes it with `movement::TakeWater`): Half-Life's `PM_CheckWater` tests points - so does the core,
    against the BSP of the swimmable sectors the player is in (a point's side of each split plane down to an inside or
    outside leaf). The engine's own fluid physics (MovableEntity.es PreMovingNew): buoyancy = 1 - contents' densities
    over the player's (Player.es: 500 swimming at the surface, 1000 diving) by immersion; swimming (buoyancy x gravity
    < 0.5 in swimmable content) pulls the whole velocity towards the top sinking/floating speed by at most
    buoyancy x gravity a tick - inverted exactly (given that much less towards it); fluid friction and control are the
    acceleration and deceleration, zero. Contents are last tick's (TestFields runs inside PreMoving, after the patch).
35. Sam swims in another box: SWIM/SWIMSMALL (index 2/3, -0.75..+0.75 m around his position, camera 0.4 over it;
    diving 0.0), standing 0..2.25 m (camera 1.9). He switches when the water passes half the box he's in: the middle
    jumps by most of a metre. While the rules move him in water he keeps the standing box (if it fits) and the
    standing camera - Half-Life's hull is 72 units (2.14 m), its eyes 64 over the feet.
36. `CCastRay` with an origin entity walks the sectors from that entity's: a ray from far away misses everything.
    `CCastRay(NULL, ...)` tests the whole world (slow: a test hook doing ~2000 of them stalled the game past the
    watchdog's 5 s).
37. Pools: TFE 12_Karnak (open, 15 m deep), 14_SacredYards, 09_Sewers; TSE LevelsMP/2_1_Ziggurrat (10 m), 1_1_Palenque.
    Karnak's banks have a lip under the water; HL's water jump needs a near vertical wall at 8 units over the hull's
    middle and nothing at its top.
- 2026-10-16: egon (minigun), satchel (cannon), crossbow (TSE sniper); the HL sounds really heard (3D parameters).
  `test_weapons.py` 10/10 TSE; TFE 9/9 + crossbow n/a.
- 2026-10-16: Half-Life's swimming (the speedrun build too): water levels, PM_WaterMove, the jump up, the water jump,
  no fall damage into water. `test_water.py` 6/6 TFE (Karnak) and TSE (Ziggurrat; the water jump in Palenque);
  `test_hl.py` 11/11 TFE and TSE.
38. View models: `CPlayerWeapons::RenderWeaponModel` (Entities, exported in TFE and TSE: `...KKHJ@Z`, the 7-argument one;
    TFE's 6-argument one just calls it) is where Sam draws his gun - after the world, before the HUD. Patched: Half-Life's
    v_*.mdl drawn there instead (Studio version 10, read as the SDK's studio.h; StudioCalcRotations' runs of animation
    values, Euler angles to quaternions, slerp; the SDK's weapon animation numbers). Half-Life's model space (x ahead,
    y left, z up) to the eye's: (-y, z, -x); vertical field of view 73.74 degrees (90 at 4:3), depth range 0..0.1 as Sam's
    weapon.
39. The engine's own GL types (gl_types.h: GLenum is an enum) clash with the Windows SDK's GL/gl.h, and its GL pointers
    (pglBegin...) aren't exported: the plugin loads its own from opengl32.dll (GetProcAddress) and uses the engine's
    enums. All the GL state is pushed and popped; texture units above 0 off while drawing.
40. Screenshots of the game for checks: `um win shot --exe SeriousSam_Custom.exe out.png --scale 0.5` (gfxcapture).
- 2026-10-20: Half-Life's view models with their animations (draw, idle, fire, reload, holster; gauss spin, egon
  beam, satchel radio). Seen in TFE and TSE (`tools/view_shots.py`): .357, crowbar, Glock, MP5, shotgun, gauss, egon,
  crossbow, satchel.
41. Half-Life's HUD (Hud.cpp) after the game's picture (OnPostDraw), with Sam's status line off (`hud_bShowInfo` 0 -
    not persistent; given back) and his crosshair skipped (`CPlayerWeapons::RenderCrosshair` patched). Sprites: IDSP
    version 2, the palette after the header (a short count), the first frame; SPR_ALPHTEST's colour 255 is clear (made
    black: the HUD is drawn added). Half-Life 25th anniversary's hud.txt has 640, 1280 and 2560 sets (1280 at 1920x1080).
42. Two games at once: the one in the background doesn't get to skip its level intro (no focus) - one game at a time
    in tests.
- 2026-10-20: Half-Life's HUD (health, HEV battery, ammo, second ammo, the gun's crosshair) and the view model's bob.
43. View model events (mstudioevent_t: frame, event, type, options[64]; 76 bytes) in the sequences: 5001/5011/5021/5031
    a muzzle flash at attachment 0..3 (options "31": sprites/muzzleflash(1 % 3 + 1).spr at scale 0.3), 5004 a sound
    (the Glock's, the .357's, the MP5's reload sounds come this way). Attachments: 88 bytes (name, type, bone, org,
    vectors); the header's numattachments is its 19th int after flags.
- 2026-10-20: muzzle flashes and the models' own reload sounds; sprites shared (Sprite.cpp: all frames).
44. Player sounds: Player.es plays its steps, jump grunt, landing, splashes and pain on m_soFootL/R (71, 72), m_soBody
    (73), m_soMouth (70), resetting the mouth's 3D parameters before each - so they're stopped instead, in the rules'
    tick (PreMoving: after the actions and the tick's timers and attacks, before the sound is mixed). Half-Life's own
    play on the plugin's CSoundObjects with the player as owner (SetOwner; let go of on a level change).
- 2026-10-20: Half-Life's player sounds (steps by surface and pace, jump, landing, fall pain, swim, pain). Steps heard:
  peak 0.054 over 0.008 quiet (Sam's 0.061).
45. Weapon selection in Player.es is by the buttons' edge (ulNewButtons = now & ~before, before = the last applied
    action's): to ask Sam for a weapon by his slot key again (the other of a pair), a tick without it between. Half-Life's
    hud_fastswitch takes a gun at once only when it's the only one in its slot (SelectSlot); its menu doesn't time out.
- 2026-10-20: Half-Life's weapon menu (slots 1-5, the wheel, the flip key as lastinv, fire takes; hl_bSlots,
  hl_bFastSwitch) drawn as ammo.cpp's DrawWList. `test_weapons.py slots` PASS TFE.
