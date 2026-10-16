"""Oracle for Half-Life's swimming in the real game (the usual build, SERIOUSBRIDGE_DEV=1; the rules are the speedrun
build's too).

    python tools/test_water.py --game "<Serious Sam folder>" [--launch] [--only sink,swim,...] [--level L] [--edge-level L]

Each check starts a level with water afresh (TFE 12_Karnak, TSE LevelsMP/2_1_Ziggurrat) and puts the player in its biggest
water (hl_GoWater: the feet this deep under its top). Half-Life's pm_shared.c:
  sink       no keys: drifting down at 0.8 x 60 = 48 u/s, and really moving that fast (the engine's buoyancy taken out);
  swim       forward, level: 0.8 x 320 = 256 u/s, and really;
  up         jump held: up at 100 u/s set every frame, less the water's friction: ~96 u/s;
  dive       forward looking 45 degrees down: along the view, ~181 u/s down and ~181 ahead;
  fall       8 m down into the water: fast enough to hurt on land (over 580 u/s), no damage in water;
  waterjump  at a wall with room over it (hl_WaterEdge; --edge-level), jump held (up to the surface, waist deep):
             out of the water over the ledge.
Prints PASS/FAIL per check.
"""
import argparse
import math
import re
import time

import test_hl

ALL = ("sink", "swim", "up", "dive", "fall", "waterjump")
UNIT = test_hl.UNIT


def go(game, depth: float) -> str:
    out = game.run(f'hl_GoWater("{depth}");', want="water:")
    time.sleep(1.0)
    return out


def health(game) -> float | None:
    out = game.run("hl_Status();", want="health")
    m = re.findall(r"health ([\d.-]+), armour", out)
    return float(m[-1]) if m else None


def rate(rows, key: str) -> float:
    """u/s of a position over the rows (really moved)."""
    a, b = rows[0], rows[-1]
    return (b[key] - a[key]) / (b["tick"] - a["tick"]) / UNIT


def hrate(rows) -> float:
    a, b = rows[0], rows[-1]
    return math.hypot(b["x"] - a["x"], b["z"] - a["z"]) / (b["tick"] - a["tick"]) / UNIT


def check_sink(game, results):
    go(game, 2.5)
    rows = test_hl.trace(game, 30, *test_hl.hold())
    last = rows[-10:]  # (1-1.5 s: settled, not yet at the bottom)
    rules = sum(r["vy"] for r in last) / len(last) / UNIT
    real = rate(last, "y")
    ok = abs(rules + 48) < 2 and abs(real + 48) < 5 and all(r["water"] >= 2 for r in rows)
    results.append(("sink: no keys, down at 48 u/s (and really)", ok,
                    f"rules {rules:.1f} u/s, really {real:.1f} u/s, water level {int(rows[-1]['water'])}"))


def check_swim(game, results):
    go(game, 6.0)
    rows = test_hl.trace(game, 40, *test_hl.hold(fwd=1.0, ticks=45))
    last = rows[-10:]
    rules = sum(r["hspeed_u"] for r in last) / len(last)
    real = hrate(last)
    vert = rate(last, "y")
    ok = abs(rules - 256) < 3 and abs(real - 256) < 12 and abs(vert) < 8
    results.append(("swim: forward 0.8 x 320 = 256 u/s (and really), level", ok,
                    f"rules {rules:.1f} u/s, really {real:.1f} u/s, vertical {vert:.1f} u/s"))


def check_up(game, results):
    go(game, 7.0)
    rows = test_hl.trace(game, 30, *test_hl.hold(jump=1, ticks=35))
    last = rows[-10:]
    rules = sum(r["vy"] for r in last) / len(last) / UNIT
    real = rate(last, "y")
    ok = abs(rules - 96) < 3 and abs(real - 96) < 8
    results.append(("up: jump held, up at 100 u/s less the friction: ~96 (and really)", ok,
                    f"rules {rules:.1f} u/s, really {real:.1f} u/s"))


def check_dive(game, results):
    game.run("hl_fTestPitch=-45;")
    game.run('hl_GoWater("1.5");')
    rows = test_hl.trace(game, 14, *test_hl.hold(fwd=1.0, ticks=20))
    game.run("hl_fTestPitch=0;")
    last = rows[8:14]  # (a good way down by then: not yet at the bottom)
    down = -sum(r["vy"] for r in last) / len(last) / UNIT
    ahead = sum(r["hspeed_u"] for r in last) / len(last)
    real = -rate(last, "y")
    want = 256 * math.sqrt(0.5)
    ok = abs(down - want) < 6 and abs(ahead - want) < 6 and abs(real - want) < 15
    results.append(("dive: forward looking 45 degrees down swims along the view (~181 down, ~181 ahead)", ok,
                    f"down {down:.1f} u/s (really {real:.1f}), ahead {ahead:.1f} u/s"))


def check_fall(game, results):
    game.run("cht_bGod=0;")
    h0 = health(game)
    rows = test_hl.trace(game, 60, 'hl_GoWater("-8.0");')
    time.sleep(1.0)
    h1 = health(game)
    game.run("cht_bGod=1;")
    fastest = -min(r["vy"] for r in rows) / UNIT
    wet = max(r["water"] for r in rows)
    ok = h0 is not None and h1 == h0 and fastest > 580 and wet >= 2
    results.append(("fall: 8 m into water - over 580 u/s (a land fall would hurt), no damage", ok,
                    f"fastest {fastest:.0f} u/s, water level up to {int(wet)}, health {h0} -> {h1}"))


def check_waterjump(game, results):
    mark = len(game.text())
    out = ""
    for under in ("0.1", "0.3", "0.5", "0.7"):  # (the wall looked for this deep under the surface)
        out = game.run(f'hl_WaterEdge("{under}");', want="water edge")
        if "feet at" in out:
            break
    m = re.findall(r"feet at [\d.-]+ ([\d.-]+) [\d.-]+", out)
    if not m:
        results.append(("waterjump: out at a ledge", False, "no wall with room over it in this level (--edge-level)"))
        return
    y0 = float(m[-1])
    rows = test_hl.trace(game, 70, *test_hl.hold(jump=1, ticks=70))
    jumped = "water jump" in game.text()[mark:]
    rise = rows[-1]["y"] - y0
    ok = jumped and rows[-1]["water"] == 0 and rise > 0.8
    results.append(("waterjump: waist deep at a wall with room over it - out of the water", ok,
                    f"{'jumped' if jumped else 'no water jump'}, {rise:.2f} m up, water level at the end {int(rows[-1]['water'])}"))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    ap.add_argument("--only", default=",".join(ALL))
    ap.add_argument("--level", default="")
    ap.add_argument("--edge-level", default="")
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    tse = (game.folder / "Bin" / "EntitiesMP.dll").exists()
    level = a.level or ("LevelsMP/2_1_Ziggurrat" if tse else "12_Karnak")
    edge = a.edge_level or ("LevelsMP/1_1_Palenque" if tse else level)
    if not level:
        raise SystemExit("--level: a TSE level with water")
    test_hl.HANG.unlink(missing_ok=True)
    if a.launch:
        test_hl.launch(game)
    checks = {"sink": check_sink, "swim": check_swim, "up": check_up, "dive": check_dive, "fall": check_fall,
              "waterjump": check_waterjump}
    results = []
    for name in a.only.split(","):
        try:
            test_hl.start_level(game, edge if name == "waterjump" else level)
            game.run("cht_bGod=1;", "cht_bInvisible=1;", "hl_bEnable=1;", "hl_bAutoJump=1;", "hl_bBhopCap=0;")
            test_hl.settle(game)
            checks[name](game, results)
        except test_hl.Hung as e:
            results.append((name, False, f"the game stopped answering: {e}"))
            test_hl.kill(game)
            break
        except Exception as e:  # one broken check mustn't hide the others
            results.append((name, False, f"{type(e).__name__}: {e}"))
    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}: {name} ({detail})")


if __name__ == "__main__":
    main()
