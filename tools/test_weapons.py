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
  sounds   every gun plays Half-Life's own sounds (needs them taken from the player's Half-Life: install.ps1 -AssetsOnly);
  hev      Half-Life's armour: 80% of a hit on it, a point of it stops two, none for falls; Sam's back with hl_bHevArmor 0.
Prints PASS/FAIL per check.
"""
import argparse
import math
import re
import time

import test_hl

ALL = ("damage", "crowbar", "rate", "clip", "gauss", "sounds", "hev")
UNIT = test_hl.UNIT
# Sam's WeaponType and the key slots: TFE / TSE
SAM = {"tfe": {"knife": (1, 1), "colt": (2, 2), "doublecolt": (3, 2), "shotgun": (4, 3), "tommygun": (6, 4), "laser": (14, 8)},
       "tse": {"knife": (1, 1), "colt": (2, 2), "doublecolt": (3, 2), "shotgun": (4, 3), "tommygun": (6, 4), "laser": (12, 7)}}


class Guns:
    def __init__(self, game: test_hl.Game, kind: str):
        self.game = game
        self.sam = SAM[kind]
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
    results.append(("sounds: Half-Life's own (from the player's Half-Life) for every gun", hl >= 6 and sam == 0, f"{hl} Half-Life's, {sam} Sam's"))


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
              "sounds": check_sounds, "hev": check_hev}
    results = []
    for name in a.only.split(","):
        try:
            test_hl.start_level(game, level)
            game.run("cht_bGod=1;", "cht_bInvisible=1;", "cht_bGiveAll=1;", "hl_bWeapons=1;")
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
