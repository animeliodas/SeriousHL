"""Oracle for SeriousHL's Half-Life weapons in the real game (the usual build, SERIOUSBRIDGE_DEV=1).

    python tools/test_weapons.py --game "<Serious Sam folder>" [--launch] [--only damage,rate,...]

Each check starts the level afresh (single player, normal), gives all weapons, makes the player invisible and godly
(monsters don't fight back), and drives the guns through the plugin's test hooks (select, fire, second fire held, aim
at a target, its health):
  damage   one shot each at a Kleer: .357 40, Glock 8, MP5 5, gauss 20 (sk_plr_*); the shotgun's 6 pellets of 5;
  crowbar  a crowbar blow at 1.2 m: 10;
  rate     the MP5 held 2 s fires every 0.1 s (~20 shots); the Glock every 0.3 s;
  clip     the .357 held: 6 shots, then a 2 s reload, then fires again;
  gauss    the gauss charged 2 s and let go: 100 damage and a push back of 500 u/s along the ground;
  sounds   every gun plays Half-Life's own sounds (needs them taken from the player's Half-Life: install.ps1 -AssetsOnly),
           and they come out: Windows' meter of the game's audio session (audio_meter.ps1) at a .357 shot vs the quiet;
  hev      Half-Life's armour: 80% of a hit on it, a point of it stops two, none for falls; Sam's back with hl_bHevArmor 0;
  egon     the egon (on the minigun) held 0.5 s: 14 damage and a cell every 0.1 s, the beam off on letting go;
  satchel  a satchel (on the cannon) waits past Sam's 3 s fuse, then the first fire sets it off: damage at the target;
  crossbow (TSE, on the sniper rifle) one bolt: 50;
  slots    Half-Life's weapon menu: 3 opens it on the MP5, 3 again the shotgun, fire takes it; the flip key back; with
           hl_bFastSwitch 1, 1 takes the crowbar at once (its slot's only one), 2 still opens the menu (two in it);
           and the fire that takes a gun from the menu shoots nothing of the one in hand;
  grenade  the hand grenade (on the grenade launcher), looking level: thrown 0.5 s after the pin at 400 u/s 10 degrees up,
           half gravity - first touches the floor 7-11 m ahead (Half-Life's arc from the eyes: ~9 m); goes off 3 s after the
           pin with Half-Life's explosion sound, bounces with its.
Prints PASS/FAIL per check.
"""
import argparse
import math
import re
import time

import test_hl
import test_loudness

ALL = ("damage", "crowbar", "rate", "clip", "gauss", "sounds", "hev", "egon", "satchel", "crossbow", "slots", "grenade")
UNIT = test_hl.UNIT
# Sam's WeaponType and the key slots: TFE / TSE
SAM = {"tfe": {"knife": (1, 1), "colt": (2, 2), "doublecolt": (3, 2), "shotgun": (4, 3), "tommygun": (6, 4), "laser": (14, 8),
               "minigun": (7, 4), "cannon": (16, 9)},
       "tse": {"knife": (1, 1), "colt": (2, 2), "doublecolt": (3, 2), "shotgun": (4, 3), "tommygun": (6, 4), "laser": (12, 7),
               "minigun": (7, 4), "cannon": (14, 8), "sniper": (13, 6)}}


class Guns:
    def __init__(self, game: test_hl.Game, kind: str):
        self.game = game
        self.sam = dict(SAM[kind])
        self.kind = kind
        self.target = None

    def in_hand(self) -> int:
        out = self.game.run("hl_Status();", want="weapons")
        w = re.findall(r"in hand Sam's (\d+)", out)
        return int(w[-1]) if w else -1

    def take(self, gun: str) -> bool:
        weapon, slot = self.sam[gun]
        for _ in range(4):
            if self.in_hand() == weapon:
                time.sleep(0.6)
                return True
            self.game.run(f"hl_iTestSelect={slot};")
            time.sleep(1.6)
        return self.in_hand() == weapon

    def spawn(self, name="Boneman", distance=6.0) -> None:
        out = self.game.run(f'hl_SpawnTarget("{name} {distance}");', want="target")
        m = re.findall(r"target \S+ \((\d+)\)", out)
        self.target = m[-1] if m else None
        time.sleep(1.5)

    def health(self) -> float | None:
        out = self.game.run(f'hl_Aim("{self.target}");', want="aim:")
        m = re.findall(r"aim: \S+ \(\d+\) at [\d.]+ m, health ([\d.-]+)", out)
        return float(m[-1]) if m else None

    def shot(self, fire: str = "hl_iTestFire=1;", wait: float = 1.0) -> float | None:
        h0 = self.health()
        time.sleep(0.3)
        self.health()  # (aim again: the target may have moved)
        self.game.run(fire)
        time.sleep(wait)
        h1 = self.health()
        return None if h0 is None or h1 is None else h0 - h1

    def status(self) -> dict:
        out = self.game.run("hl_Status();", want="weapons")
        m = re.findall(r"clip (-?\d+), reserve (-?\d+)(, charging)?(, beam on)?; (\d+) satchels out; (\d+) shots", out)
        if not m:
            return {}
        c = m[-1]
        return {"clip": int(c[0]), "reserve": int(c[1]), "beam": bool(c[3]), "satchels": int(c[4]), "shots": int(c[5])}

    def shots(self) -> int:
        out = self.game.run("hl_Status();", want="weapons")
        m = re.findall(r"(\d+) shots", out)
        return int(m[-1]) if m else -1


def check_damage(game, guns, results):
    guns.spawn()
    got = {}
    for gun, want in (("colt", 40), ("doublecolt", 8), ("tommygun", 5), ("laser", 20)):
        got[gun] = guns.shot() if guns.take(gun) else None
    pellets = guns.shot() if guns.take("shotgun") else None
    ok = got == {"colt": 40, "doublecolt": 8, "tommygun": 5, "laser": 20} and pellets is not None \
        and 10 <= pellets <= 30 and pellets % 5 == 0
    results.append(("damage: Half-Life's per shot (.357 40, Glock 8, MP5 5, gauss 20; shotgun pellets of 5)", ok,
                    f".357 {got.get('colt')}, Glock {got.get('doublecolt')}, MP5 {got.get('tommygun')}, gauss {got.get('laser')}, shotgun {pellets}"))


def check_crowbar(game, guns, results):
    guns.spawn("Boneman", 1.2)
    lost = guns.shot(wait=0.8) if guns.take("knife") else None
    results.append(("crowbar: a blow at 1.2 m takes 10", lost == 10, f"lost {lost}"))


def check_rate(game, guns, results):
    out = []
    for gun, hold, want in (("tommygun", 40, 20), ("doublecolt", 40, 7)):
        if not guns.take(gun):
            out.append((gun, None))
            continue
        n0 = guns.shots()
        game.run(f"hl_iTestFire={hold};")
        time.sleep(hold * 0.05 + 1.0)
        out.append((gun, guns.shots() - n0, want))
    ok = len(out) == 2 and all(len(o) == 3 and abs(o[1] - o[2]) <= 2 for o in out)
    results.append(("rate: MP5 every 0.1 s, Glock every 0.3 s (2 s held)", ok,
                    ", ".join(f"{o[0]} {o[1]} shots (want ~{o[2] if len(o) > 2 else '?'})" for o in out)))


def check_clip(game, guns, results):
    if not guns.take("colt"):
        results.append(("clip", False, "no colt"))
        return
    mark = len(game.text())
    game.run("hl_iTestFire=140;")  # 7 s held
    time.sleep(8.0)
    log = game.text()[mark:]
    fires = [m.start() for m in re.finditer(r"\.357 fire: clip", log)]
    reload = log.find(".357 reload")
    before = sum(1 for f in fires if reload < 0 or f < reload)
    after = sum(1 for f in fires if reload >= 0 and f > reload)
    # 0.75 s apart: 6 in 3.75 s, the reload 2 s, then a few more
    ok = before == 6 and reload >= 0 and after >= 1
    results.append(("clip: the .357 fires 6, reloads, fires again", ok, f"{before} shots, reload {'yes' if reload >= 0 else 'no'}, then {after} shots"))


def check_gauss(game, guns, results):
    if not guns.take("laser"):
        results.append(("gauss", False, "no laser"))
        return
    mark = len(game.text())
    rows = test_hl.trace(game, 70, "hl_iTestAlt=40;")
    m = re.findall(r"gauss charged ([\d.]+) s: (\d+) damage, pushed back (\d+) u/s", game.text()[mark:])
    if not m:
        results.append(("gauss: the charged shot and its push", False, "no charged shot"))
        return
    t, damage, push = float(m[-1][0]), int(m[-1][1]), int(m[-1][2])
    top = max(r["hspeed_u"] for r in rows)
    want = 200.0 * min(t / 4.0, 1.0)
    ok = abs(damage - want) <= 1 and abs(push - damage * 5) <= 5 and top >= 0.8 * push
    results.append(("gauss: charged 2 s -> 100 damage, pushed back damage x 5 along the ground", ok,
                    f"charged {t:.2f} s, {damage} damage, push {push} u/s, top speed {top:.0f} u/s"))


def check_sounds(game, guns, results):
    # with Half-Life's sounds taken from the player's Half-Life (install.ps1 -AssetsOnly): every gun sounds like Half-Life
    for gun in ("knife", "colt", "doublecolt", "shotgun", "tommygun", "laser"):
        if guns.take(gun):
            game.run("hl_iTestFire=2;")
            time.sleep(1.2)
    out = game.run("hl_Status();", want="sounds:")
    m = re.findall(r"sounds: (\d+) Half-Life's, (\d+) Sam's", out)
    hl, sam = (int(m[-1][0]), int(m[-1][1])) if m else (0, -1)
    # (asked for is not heard: Sam's guns leave their sound objects muted)
    quiet, loud = 0.0, 0.0
    if guns.take("colt"):
        quiet, loud, _ = test_loudness.loudness(game)
    heard = loud >= 0.02 and loud >= 3 * max(quiet, 0.002)
    results.append(("sounds: Half-Life's own (from the player's Half-Life) for every gun, heard", hl >= 6 and sam == 0 and heard,
                    f"{hl} Half-Life's, {sam} Sam's; a .357 shot peaks {loud:.3f} over the quiet {quiet:.3f}"))


def check_egon(game, guns, results):
    guns.spawn("Boneman", 5.0)
    if not guns.take("minigun"):
        results.append(("egon", False, "no minigun"))
        return
    h0 = guns.health()
    time.sleep(0.3)
    guns.health()
    s0 = guns.status()
    mark = len(game.text())
    game.run("hl_iTestFire=10;")  # 0.5 s (a Boneman has 125)
    time.sleep(1.5)
    s1 = guns.status()
    h1 = guns.health()
    log = game.text()[mark:]
    on, off = log.find("egon on"), log.find("egon off")
    pulses = s1.get("shots", 0) - s0.get("shots", 0)
    cells = s0.get("reserve", 0) - s1.get("reserve", 0)
    lost = None if h0 is None or h1 is None else h0 - h1
    ok = 0 <= on < off and not s1.get("beam") and 4 <= pulses <= 6 and cells == pulses and lost is not None \
        and abs(lost - 14 * pulses) < 0.5
    results.append(("egon: 14 damage and a cell every 0.1 s while held, off when let go", ok,
                    f"beam {'on, then off' if 0 <= on < off else 'not on and off'}; {pulses} pulses, {cells} cells, target lost {lost}"))


def check_satchel(game, guns, results):
    guns.spawn("Boneman", 4.0)
    if not guns.take("cannon"):
        results.append(("satchel", False, "no cannon"))
        return
    h0 = guns.health()
    game.run("hl_iTestFire=1;")
    time.sleep(4.5)  # past Sam's grenade fuse (3 s)
    out = guns.status().get("satchels")
    h1 = guns.health()
    game.run("hl_iTestFire=1;")
    time.sleep(1.5)
    after = guns.status().get("satchels")
    h2 = guns.health()
    waited = h0 is not None and h1 == h0
    # (gone: killed by it)
    lost = None if h1 is None else h1 - (h2 if h2 is not None else 0.0)
    ok = out == 1 and waited and after == 0 and lost is not None and 0 < lost <= 150
    results.append(("satchel: no fuse, the first fire sets it off (up to 150 over 2.5 x 150 u)", ok,
                    f"{out} out after 4.5 s, target {'untouched' if waited else 'hit'} meanwhile; set off: {after} out, target lost {lost}"))


def check_crossbow(game, guns, results):
    if "sniper" not in guns.sam:
        results.append(("crossbow: TSE's sniper rifle only (none in TFE)", True, "skipped"))
        return
    guns.spawn()
    lost = guns.shot() if guns.take("sniper") else None
    st = guns.status()
    results.append(("crossbow: a bolt takes 50, the clip 5", lost == 50 and st.get("clip") == 4, f"lost {lost}, clip {st.get('clip')}"))


def check_slots(game, guns, results):
    game.run("hl_bSlots=1;", "hl_bFastSwitch=0;")
    picks = []

    def press(cmd: str, wait: float = 0.4) -> None:
        game.run(cmd)
        time.sleep(wait)

    def hand() -> int:
        time.sleep(3.0)  # (Sam's put away and draw)
        return guns.in_hand()

    s = guns.sam
    start = guns.in_hand()
    press("hl_iTestSelect=3;")
    out = game.run("hl_Status();", want="weapons")
    press("hl_iTestSelect=3;")
    press("hl_iTestFire=1;")
    picks.append(("3, 3, fire: the shotgun", hand() in (s["shotgun"][0], 5)))
    press("hl_iTestSelect=0;")
    game.run("hl_iTestSelect=0;")
    # (the flip key: Sam's "flip weapon" bit, 1 << 4, sent for one action)
    press('hl_TestButtons("16");')
    picks.append(("flip: back to the one before", hand() == start))
    game.run("hl_bFastSwitch=1;")
    press("hl_iTestSelect=1;")
    picks.append(("fast 1: the crowbar at once", hand() == s["knife"][0]))
    press("hl_iTestSelect=2;")
    picks.append(("fast 2: still the menu (no switch)", hand() == s["knife"][0]))
    press("hl_iTestFire=1;")
    picks.append(("... fire takes the Glock", hand() == s["doublecolt"][0]))
    # the menu's fire doesn't shoot the gun in hand (held a while, as a click is)
    game.run("hl_bFastSwitch=0;")
    n0 = guns.shots()
    press("hl_iTestSelect=3;")
    press("hl_iTestFire=6;", 1.0)
    picks.append(("the fire that takes the MP5 shoots no Glock", guns.shots() == n0 and hand() == s["tommygun"][0]))
    game.run("hl_bSlots=0;")
    results.append(("slots: Half-Life's weapon menu on Sam's guns", all(ok for _, ok in picks),
                    ", ".join(f"{name} {'yes' if ok else 'NO'}" for name, ok in picks)))


def check_grenade(game, guns, results):
    sam = {"tfe": (9, 6), "tse": (9, 5)}[guns.kind]
    guns.sam["grenade"] = sam
    if not guns.take("grenade"):
        results.append(("grenade", False, "no grenade launcher"))
        return
    game.run("hl_fTestPitch=0.01;")
    time.sleep(0.5)
    where = re.findall(r"where: ([\d.-]+) ([\d.-]+) ([\d.-]+)", game.run("hl_Where();", want="where:"))
    x0, z0 = float(where[-1][0]), float(where[-1][2])
    mark = len(game.text())
    game.run("hl_iTestFire=2;")
    time.sleep(5.0)
    log = game.text()[mark:]
    game.run("hl_fTestPitch=0;")
    thrown = re.findall(r"thrown \(hand grenade\) at [\d.-]+ [\d.-]+ [\d.-]+ \(([\d.]+) s\), ([\d.]+) m/s, fuse ([\d.]+) s", log)
    went = re.findall(r"thrown went off at ([\d.-]+) ([\d.-]+) ([\d.-]+) \(([\d.]+) s\)(, Half-Life's sound)?", log)
    first = re.findall(r"thrown bounced first at ([\d.-]+) ([\d.-]+) ([\d.-]+)", log)
    st = game.run("hl_Status();", want="thrown:")
    bounces = re.findall(r"(\d+) bounces", st)
    if not thrown or not went:
        results.append(("grenade: Half-Life's hand grenade", False, f"thrown {len(thrown)}, went off {len(went)}"))
        return
    tm_throw, speed, fuse = float(thrown[-1][0]), float(thrown[-1][1]), float(thrown[-1][2])
    tm_off = float(went[-1][3])
    dist = math.hypot(float(first[-1][0]) - x0, float(first[-1][2]) - z0) if first else -1.0
    ok = abs(speed - 400 * UNIT) < 0.6 and abs(fuse - 2.5) < 0.11 and abs(tm_off - tm_throw - fuse) < 0.2 and 7 <= dist <= 11 \
        and went[-1][4] != "" and bounces and int(bounces[-1]) > 0
    results.append(("grenade: thrown as Half-Life's (400 u/s, fuse 3 s from the pin), lands ahead, its sounds", ok,
                    f"{speed:.1f} m/s, fuse {fuse:.2f} s after a 0.5 s throw, off {tm_off - tm_throw:.2f} s later, first touch {dist:.1f} m ahead, "
                    f"explosion sound {'Half-Life' if went[-1][4] else 'Sam'}, {bounces[-1] if bounces else 0} bounce sounds"))


def hit(game, args: str):
    """health before, after, armour before, after, the difficulty's damage strength"""
    out = game.run(f'hl_TestDamage("{args}");', want="test damage")
    m = re.findall(r"test damage [\d.]+ \w+: health ([\d.-]+) -> ([\d.-]+), armour ([\d.-]+) -> ([\d.-]+), difficulty ([\d.]+)", out)
    return tuple(float(x) for x in m[-1]) if m else None


def check_hev(game, guns, results):
    game.run("cht_bGod=0;")
    time.sleep(0.5)
    rows = {}
    for name, args in (("bullet", "50 bullet 100 100"), ("runs out", "50 bullet 100 10"), ("fall", "30 fall 100 100"),
                       ("overcharge", "50 shock 100 100")):
        rows[name] = hit(game, args)
        time.sleep(0.3)
    game.run("hl_bHevArmor=0;")
    rows["Sam's"] = hit(game, "30 bullet 100 100")
    game.run("hl_bHevArmor=1;", "cht_bGod=1;")
    if any(r is None for r in rows.values()):
        results.append(("hev: Half-Life's armour", False, f"no answer: {rows}"))
        return
    st = rows["bullet"][4]
    own = min(st, 1.0)  # (one's own damage isn't made bigger by the difficulty)

    def lost(r):
        return r[0] - r[1], r[2] - r[3]

    want = {"bullet": (0.2 * 50 * st, 0.4 * 50 * st), "runs out": (50 * st - 20, 10), "fall": (30, 0),
            "overcharge": (0.2 * 50 * own, 0.4 * 50 * own), "Sam's": (30 * st / 3, 30 * st * 2 / 3)}
    ok = all(abs(lost(rows[k])[0] - w[0]) < 0.05 and abs(lost(rows[k])[1] - w[1]) < 0.05 for k, w in want.items())
    results.append(("hev: 80% on armour, a point stops two, none for falls; Sam's 2/3 with hl_bHevArmor 0", ok,
                    f"difficulty x{st:.2f}; " + ", ".join(f"{k} health -{lost(r)[0]:.1f} armour -{lost(r)[1]:.1f} (want -{want[k][0]:.1f}/-{want[k][1]:.1f})"
                                                       for k, r in rows.items())))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    ap.add_argument("--only", default=",".join(ALL))
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    kind = "tse" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "tfe"
    level = "LevelsMP/2_1_Ziggurrat" if kind == "tse" else "11_AlleyOfSphinxes"
    test_hl.HANG.unlink(missing_ok=True)
    if a.launch:
        test_hl.launch(game)
    checks = {"damage": check_damage, "crowbar": check_crowbar, "rate": check_rate, "clip": check_clip, "gauss": check_gauss,
              "sounds": check_sounds, "hev": check_hev, "egon": check_egon, "satchel": check_satchel,
              "crossbow": check_crossbow, "slots": check_slots, "grenade": check_grenade}
    results = []
    for name in a.only.split(","):
        try:
            test_hl.start_level(game, level)
            game.run("cht_bGod=1;", "cht_bInvisible=1;", "cht_bGiveAll=1;", "hl_bWeapons=1;", "hl_bSlots=0;")
            time.sleep(1.0)
            checks[name](game, Guns(game, kind), results)
        except test_hl.Hung as e:
            results.append((name, False, f"the game stopped answering: {e}"))
            test_hl.kill(game)
            break
        except Exception as e:  # one broken check mustn't hide the others
            results.append((name, False, f"{type(e).__name__}: {e}"))
    game.run("cht_bInvisible=0;")
    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}: {name} ({detail})")


if __name__ == "__main__":
    main()
