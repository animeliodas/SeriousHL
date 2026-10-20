"""Oracle for SeriousHL in the real game: Serious Sam Classic with the plugin, its dev hook on (SERIOUSBRIDGE_DEV=1).

    python tools/test_hl.py --game "<Serious Sam folder>" [--launch] [--level 11_AlleyOfSphinxes] [--only run,stop,...]

--launch starts the game first (copying its SaveGame folder aside: starting levels autosaves over quick saves).
Levels by default: TFE 11_AlleyOfSphinxes, TSE LevelsMP/2_1_Ziggurrat (flat, room to run). If the game stops, the
plugin's watchdog writes %TEMP%\\SeriousHL\\hang.txt: the test gives up and closes the game.
Commands go through %TEMP%\\SeriousHL\\cmd.txt, the movement comes back as %TEMP%\\SeriousHL\\trace.csv (a line a tick).
Each check starts the level afresh, holds keys and turns the view through the plugin's test hooks:
  run      forward from standing: Half-Life's top speed, 320 u/s, and he really goes that fast;
  stop     keys let go at 320 u/s: Half-Life's friction stops him in ~0.53 s after ~2 m (Sam's own: ~0.2 s);
  jump     a jump from standing: 45 units up (1.34 m at 1 u = 2.97 cm), ~0.67 s in the air;
  duckjump a jump, then duck in the air: the feet clear 63 units (1.87 m);
  duckhold jump and duck held together: a jump, ducked (Sam alone just stood up);
  ducktap  duck on the ground, let go within 0.4 s: popped up 18 units without a jump;
  duckspeed walking ducked: 133 u/s;
  bhop     hopping on (jump held), strafing right and turning right 15 degrees a tick: speed builds past 1.7x the
           top speed (air strafing + no friction on landing) - a speed Sam can't reach by running;
  bhopcap  the same with hl_bBhopCap 1 (Steam Half-Life): a landing jump over 1.7x cuts the speed;
  sam      the same with hl_bEnable 0: Sam's own movement is back - no gain.
Prints PASS/FAIL per check.
"""
import argparse
import csv
import math
import re
import os
import shutil
import subprocess
import tempfile
import time
from pathlib import Path

DEV = Path(tempfile.gettempdir()) / "SeriousHL"
CMD = DEV / "cmd.txt"
TRACE = DEV / "trace.csv"
HANG = DEV / "hang.txt"  # the plugin's watchdog: the game stopped (core/sam/DevHook.h)
UNIT = 1.9 / 64.0


class Hung(Exception):
    pass
ALL = ("run", "stop", "jump", "duckjump", "duckhold", "ducktap", "duckspeed", "rocketjump", "bhop", "bhopcap", "sam")


class Game:
    def __init__(self, folder: str):
        self.folder = Path(folder)
        self.log = self.folder / "SeriousSam_Custom.log"

    def text(self) -> str:
        try:
            return self.log.read_text(encoding="latin-1", errors="replace")
        except FileNotFoundError:
            return ""

    def run(self, *commands: str, want: str = "", timeout: float = 3.0) -> str:
        """Run console commands; the log written after them, once it has `want`."""
        if HANG.exists():
            raise Hung(HANG.read_text())
        start = len(self.text())
        DEV.mkdir(exist_ok=True)
        with open(CMD, "a", encoding="ascii") as f:
            for c in commands:
                f.write(c + "\n")
        end = time.time() + timeout
        while time.time() < end:
            time.sleep(0.2)
            new = self.text()[start:]
            if want and want in new:
                time.sleep(0.2)
                return self.text()[start:]
        return self.text()[start:]

    def where(self) -> str:
        out = self.run("hl_Where();", want="where:")
        lines = [l for l in out.splitlines() if "[SeriousHL] where:" in l]
        return lines[-1] if lines else ""


def backup_saves(folder: Path) -> None:
    src = folder / "SaveGame"
    if src.exists():
        dst = DEV / "savegame_backup" / f"{folder.name}_{time.strftime('%Y%m%d_%H%M%S')}"
        shutil.copytree(src, dst)
        print(f"saves copied to {dst}")


def launch(game: Game) -> None:
    backup_saves(game.folder)
    mark = len(game.text())
    env = dict(os.environ, SERIOUSBRIDGE_DEV="1")
    subprocess.Popen([str(game.folder / "Bin" / "SeriousSam_Custom.exe")], cwd=str(game.folder / "Bin"), env=env,
                     creationflags=subprocess.DETACHED_PROCESS)
    end = time.time() + 90
    while time.time() < end:
        text = game.text()
        if len(text) < mark:  # (the game starts its log afresh)
            mark = 0
        if "[SeriousHL] dev hook on" in text[mark:]:
            time.sleep(8.0)  # the menu
            return
        time.sleep(1.0)
    raise SystemExit("the game didn't come up with SeriousHL's dev hook (is the plugin in Bin/Plugins?)")


def kill(game: Game) -> None:
    """Stop this game's process (by its exact PID)."""
    ps = "(Get-Process SeriousSam_Custom -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '" + str(game.folder.resolve()) + "*' }).Id"
    for pid in subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout.split():
        subprocess.run(["powershell", "-NoProfile", "-Command", f"Stop-Process -Id {int(pid)} -Force"])
        print(f"stopped the game (pid {pid})")


def start_level(game: Game, level: str, timeout: float = 150.0) -> None:
    """StartMap, then past the level's intro (fire skips it) and Netricsa, until the player stands in the level.
    StartMap takes the game mode and difficulty last set: from the menu, the intro's "flyover" - some levels hang in it
    (TFE 11_AlleyOfSphinxes: its animation changers send events to each other forever). So single player, normal."""
    game.run("gam_iStartMode=0;", "gam_iStartDifficulty=1;", f'StartMap("{level}");')
    time.sleep(10.0)
    end = time.time() + timeout
    calm = 0
    while time.time() < end:
        last = game.where()
        if "netricsa" in last:  # (first: with Netricsa up, fire doesn't skip an intro)
            game.run("hl_CloseNetricsa();")
            calm = 0
        elif "cinematic" in last:
            game.run("hl_iTestFire=2;")
            calm = 0
        elif last and "no player" not in last:
            calm += 1
            if calm >= 2:
                return
        time.sleep(1.5)
    raise RuntimeError(f"{level}: the player didn't come")


def trace(game: Game, ticks: int, *setup: str) -> list[dict]:
    """Trace the next ticks while the given test hooks act; the rows."""
    TRACE.unlink(missing_ok=True)
    game.run(*setup, f"hl_Trace({ticks});", want="trace written", timeout=ticks * 0.05 + 10.0)
    with open(TRACE, newline="") as f:
        rows = list(csv.DictReader(f))
    return [{k: float(v) for k, v in r.items()} for r in rows]


def hold(fwd=0.0, side=0.0, jump=0, duck=0, turn=0.0, ticks=0) -> tuple:
    return (f"hl_fTestForward={fwd};", f"hl_fTestSide={side};", f"hl_bTestJump={jump};", f"hl_bTestDuck={duck};",
            f"hl_fTestTurn={turn};", f"hl_iTestTicks={ticks};")


def settle(game: Game) -> None:
    game.run(*hold())
    time.sleep(1.5)


def check_run(game, results):
    rows = trace(game, 50, *hold(fwd=1.0, ticks=60))
    last = rows[-10:]
    top = sum(r["hspeed_u"] for r in last) / len(last)
    # what he really covered in the last second, against what the rules say
    a, b = rows[-21], rows[-1]
    real = math.hypot(b["x"] - a["x"], b["z"] - a["z"]) / (b["tick"] - a["tick"]) / UNIT
    ok = abs(top - 320) < 4 and abs(real - 320) < 16
    results.append(("run: Half-Life's top speed 320 u/s, and really that fast", ok, f"{top:.1f} u/s by the rules, {real:.1f} u/s covered"))


def check_stop(game, results):
    # forward for 1.5 s, then nothing (one trace: the keys go on the tick the hook runs out)
    rows = trace(game, 60, *hold(fwd=1.0, ticks=30))
    fast = next((i for i, r in enumerate(rows) if r["hspeed_u"] > 315), 0)
    start = next((i for i, r in enumerate(rows) if i > fast and r["hspeed_u"] < 300), None)
    stop = next((i for i, r in enumerate(rows) if start is not None and i > start and r["hspeed_u"] < 1), None)
    if start is None or stop is None:
        results.append(("stop: Half-Life's friction", False, f"speeds {[round(r['hspeed_u']) for r in rows[:16]]}"))
        return
    t = (stop - start + 1) * 0.05
    slide = math.hypot(rows[stop]["x"] - rows[start - 1]["x"], rows[stop]["z"] - rows[start - 1]["z"]) if start > 0 else 0.0
    ok = 0.4 <= t <= 0.7 and 1.5 <= slide <= 2.5
    results.append(("stop: Half-Life's friction (0.53 s, ~2 m from 320 u/s)", ok, f"stopped in {t:.2f} s after {slide:.2f} m"))


def check_jump(game, results):
    rows = trace(game, 30, *hold(jump=1, ticks=2))
    y0 = rows[0]["y"]
    apex = max(r["y"] for r in rows) - y0
    air = [r for r in rows if r["ground"] == 0]
    t_air = len(air) * 0.05
    ok = 1.25 <= apex <= 1.42 and 0.55 <= t_air <= 0.8
    results.append(("jump: 45 units (1.34 m), ~0.67 s in the air", ok, f"{apex:.3f} m up, {t_air:.2f} s in the air"))


def bhop(game, *extra: str) -> list[dict]:
    return trace(game, 120, *extra, *hold(side=1.0, jump=1, turn=-15.0, ticks=125))


def check_bhop(game, results):
    rows = bhop(game, "hl_bAutoJump=1;", "hl_bBhopCap=0;")
    top = max(r["hspeed_u"] for r in rows)
    jumps = int(sum(r["jumped"] for r in rows))
    ok = top > 1.75 * 320 and jumps >= 6
    results.append(("bhop: air strafing builds speed past 1.75x 320 u/s", ok, f"top {top:.0f} u/s ({top * UNIT:.1f} m/s), {jumps} hops"))


def check_bhopcap(game, results):
    rows = bhop(game, "hl_bAutoJump=1;", "hl_bBhopCap=1;")
    game.run("hl_bBhopCap=0;")
    top = max(r["hspeed_u"] for r in rows)
    cuts = sum(1 for p, r in zip(rows, rows[1:]) if r["jumped"] and r["hspeed_u"] < 0.8 * p["hspeed_u"])
    ok = cuts >= 1 and top < 1.7 * 320 * 1.1
    results.append(("bhopcap: hl_bBhopCap 1 cuts the speed on landing past 1.7x", ok, f"top {top:.0f} u/s, {cuts} cuts"))


def scripted(game, ticks: int, keys: str) -> list[dict]:
    """Trace while the plugin plays a key script (hl_TestKeys: "j2,d20" - jump 2 actions, then duck 20)."""
    return trace(game, ticks, f'hl_TestKeys("{keys}");')


def check_duckjump(game, results):
    # jump, then duck in the air: the feet come up 18 units more - 63 units (1.87 m) instead of 45
    rows = scripted(game, 30, "j2,d28")
    rise = max(r["y"] for r in rows) - rows[0]["y"]
    ducked = any(r["ducked"] for r in rows if r["ground"] == 0)
    ok = 1.75 <= rise <= 1.95 and ducked
    results.append(("duckjump: a jump with a duck in the air clears 63 units (1.87 m)", ok, f"feet {rise:.3f} m up, ducked in the air {ducked}"))


def check_duckhold(game, results):
    # jump and duck held together (Sam's controls add them into one number: up - down = nothing): he jumps, ducked
    rows = scripted(game, 30, "jd24")
    rise = max(r["y"] for r in rows) - rows[0]["y"]
    ducked_air = any(r["ducked"] and r["ground"] == 0 for r in rows)
    jumps = int(sum(r["jumped"] for r in rows))
    ok = jumps >= 1 and ducked_air and rise > 1.5
    results.append(("duckhold: jump and duck held together jump ducked", ok, f"{jumps} jumps, ducked in the air {ducked_air}, feet {rise:.2f} m up"))


def check_ducktap(game, results):
    # duck on the ground and let go before the duck is done (0.4 s): the standing hull pops up 18 units (0.53 m)
    rows = scripted(game, 20, "d3,-17")
    rise = max(r["y"] for r in rows) - rows[0]["y"]
    air = sum(1 for r in rows if r["ground"] == 0)
    jumped = int(sum(r["jumped"] for r in rows))
    ok = 0.45 <= rise <= 0.62 and air >= 2 and jumped == 0
    results.append(("ducktap: let go of duck within 0.4 s - up 18 units without a jump", ok, f"{rise:.3f} m up, {air} ticks off the ground, {jumped} jumps"))


def check_duckspeed(game, results):
    # walking ducked: a third of the keys' 400 u/s - 133 u/s - once ducked (0.4 s)
    rows = scripted(game, 40, "fd40")
    last = rows[-10:]
    speed = sum(r["hspeed_u"] for r in last) / len(last)
    ducked = all(r["ducked"] for r in last)
    ok = 125 <= speed <= 140 and ducked
    results.append(("duckspeed: ducked, 133 u/s", ok, f"{speed:.1f} u/s, ducked {ducked}"))


def check_rocketjump(game, results):
    # a rocket at his own feet: Half-Life's push, damage * 5 u/s away from the blast (Sam's own push taken out)
    game.run("cht_bGod=0;", "cht_bGiveAll=1;", "cht_SetHealth(500);", "hl_iTestSelect=5;")
    time.sleep(2.5)
    game.run("hl_fTestPitch=-85;")
    time.sleep(1.0)
    mark = len(game.text())
    rows = trace(game, 50, "hl_iTestFire=1;")
    game.run("hl_fTestPitch=0;")
    m = re.findall(r"damage push: ([\d.]+) damage -> (\d+) u/s", game.text()[mark:])
    top = max(r["vy"] for r in rows) / UNIT
    rise = max(r["y"] for r in rows) - rows[0]["y"]
    if not m:
        results.append(("rocketjump: Half-Life's damage push", False, f"no push (top vertical {top:.0f} u/s, {rise:.2f} m up)"))
        return
    damage, push = float(m[0][0]), float(m[0][1])
    want = min(damage * 5.0, 1000.0)
    ok = abs(push - want) < 2 and 0.8 * want <= top <= 1.05 * want and rise > 0.2
    results.append(("rocketjump: a rocket at the feet pushes him by Half-Life's damage x 5", ok,
                    f"{damage:.1f} damage -> {push:.0f} u/s pushed (HL: {want:.0f}), top vertical {top:.0f} u/s, {rise:.2f} m up"))


def check_sam(game, results):
    rows = bhop(game, "hl_bEnable=0;")
    game.run("hl_bEnable=1;")
    top = max(r["hspeed_u"] for r in rows)
    status = game.run("hl_Status();", want="[SeriousHL] Half-Life")
    ok = top < 12.0 / UNIT and "movement taken" in status
    results.append(("sam: hl_bEnable 0 gives Sam's own movement back (no gain), 1 takes it again", ok,
                    f"top {top:.0f} u/s ({top * UNIT:.1f} m/s); back on: {'taken' if 'movement taken' in status else 'NOT taken'}"))


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True, help="the Serious Sam Classic folder")
    ap.add_argument("--launch", action="store_true", help="start the game first")
    ap.add_argument("--level", default="", help="started afresh before each check (TFE 11_AlleyOfSphinxes, TSE LevelsMP/2_1_Ziggurrat)")
    ap.add_argument("--only", default=",".join(ALL))
    a = ap.parse_args()
    game = Game(a.game)
    level = a.level or ("LevelsMP/2_1_Ziggurrat" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "11_AlleyOfSphinxes")
    HANG.unlink(missing_ok=True)
    if a.launch:
        launch(game)
    checks = {"run": check_run, "stop": check_stop, "jump": check_jump, "duckjump": check_duckjump, "duckhold": check_duckhold, "ducktap": check_ducktap,
              "duckspeed": check_duckspeed, "rocketjump": check_rocketjump, "bhop": check_bhop, "bhopcap": check_bhopcap,
              "sam": check_sam}
    results = []
    for name in a.only.split(","):
        try:
            start_level(game, level)
            game.run("cht_bGod=1;", "hl_bEnable=1;", "hl_bAutoJump=1;", "hl_bBhopCap=0;", "hl_bSlots=0;")
            settle(game)
            checks[name](game, results)
        except Hung as e:
            # the game stopped: don't leave it hanging on the player's screen
            results.append((name, False, f"the game stopped answering: {e}"))
            kill(game)
            break
        except Exception as e:  # one broken check mustn't hide the others
            results.append((name, False, f"{type(e).__name__}: {e}"))
    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}: {name} ({detail})")


if __name__ == "__main__":
    main()
