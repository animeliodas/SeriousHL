"""Which levels have water deep enough for test_hl.py's water checks (the usual build, SERIOUSBRIDGE_DEV=1).

    python tools/probe_water.py --game "<Serious Sam folder>" [--launch] level [level ...]
"""
import argparse
import time

import test_hl


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    ap.add_argument("levels", nargs="+")
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    if a.launch:
        test_hl.launch(game)
    for level in a.levels:
        test_hl.start_level(game, level)
        out = game.run('hl_GoWater("2.5");', want="water:")
        print(level, [l for l in out.splitlines() if "water:" in l][-1:])
        time.sleep(1.5)
        out = game.run("hl_Status();", want="water level")
        print("   ", [l[l.find("water level"):l.find("water level") + 14] for l in out.splitlines() if "water level" in l][-1:])
        out = game.run("hl_WaterEdge(\"0.2\");", want="water edge")
        print("   ", [l for l in out.splitlines() if "water edge" in l][-1:])


if __name__ == "__main__":
    main()
