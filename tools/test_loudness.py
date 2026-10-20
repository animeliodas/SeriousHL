"""Do the guns' sounds really come out? Windows' audio meter of the game's own session (audio_meter.ps1) while each gun
fires once, against the quiet before it (the usual build, SERIOUSBRIDGE_DEV=1, a level already on).

    python tools/test_loudness.py --game "<Serious Sam folder>" [--guns colt,doublecolt,...] [--sam]

--sam: with hl_bWeapons 0 (Sam's own guns and sounds), to compare.
"""
import argparse
import re
import subprocess
import time
from pathlib import Path

import test_hl
import test_weapons

HERE = Path(__file__).resolve().parent


def game_pid(folder: Path) -> int:
    ps = "(Get-Process SeriousSam_Custom -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '" + str(folder.resolve()) + "*' }).Id"
    out = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout.split()
    return int(out[0]) if out else 0


def loudness(game: test_hl.Game, fire: str = "hl_iTestFire=2;") -> tuple[float, float, str]:
    """The quietest and the loudest 0.25 s of the game's audio around a shot, and its session's line."""
    pid = game_pid(game.folder)
    meter = subprocess.Popen(["powershell", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File", str(HERE / "audio_meter.ps1"),
                              "-ProcessId", str(pid), "-Seconds", "6"], stdout=subprocess.PIPE, text=True)
    time.sleep(3.5)  # (the meter compiles its C# first: 1-2 s; then quiet)
    game.run(fire)
    out = meter.communicate()[0]
    peaks = [float(p.replace(",", ".")) for p in re.findall(r"peak ([\d,.]+)", out)]
    session = out.splitlines()[0] if out else "?"
    return (min(peaks) if peaks else 0.0), (max(peaks) if peaks else 0.0), session


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--guns", default="knife,colt,doublecolt,shotgun,tommygun,laser")
    ap.add_argument("--sam", action="store_true")
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    kind = "tse" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "tfe"
    if not game_pid(game.folder):
        raise SystemExit("the game isn't running")
    guns = test_weapons.Guns(game, kind)
    game.run("cht_bGiveAll=1;", f"hl_bWeapons={0 if a.sam else 1};", "hl_bSlots=0;")
    for gun in a.guns.split(","):
        if not guns.take(gun):
            print(f"{gun}: not in hand")
            continue
        quiet, loud, session = loudness(game)
        print(f"{gun}: quiet {quiet:.3f}, loudest {loud:.3f} ({session})")


if __name__ == "__main__":
    main()
