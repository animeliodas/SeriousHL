"""Dev: a water check's rows, printed (the game running in a level with water)."""
import sys
import time

import test_hl

game = test_hl.Game(sys.argv[1])
what = sys.argv[2]
if what == "dive":
    print(game.run('hl_GoWater("3.0");', want="water:").splitlines()[-1])
    time.sleep(1.0)
    game.run("hl_fTestPitch=-45;")
    time.sleep(0.5)
    print(game.run("hl_Where();", want="where:").splitlines()[-1])
    rows = test_hl.trace(game, 25, *test_hl.hold(fwd=1.0, ticks=30))
    game.run("hl_fTestPitch=0;")
elif what == "place":
    game.run('hl_GoWater("3.0");', f"hl_Trace(30);")
    time.sleep(3.0)
    import csv
    rows = [{k: float(v) for k, v in r.items()} for r in csv.DictReader(open(test_hl.TRACE, newline=""))]
elif what == "fall":
    game.run("cht_bGod=0;")
    rows = test_hl.trace(game, 60, 'hl_GoWater("-8.0");')
    print(game.run("hl_Status();", want="health").splitlines()[-3:])
    game.run("cht_bGod=1;")
    print(game.run("hl_Where();", want="where:").splitlines()[-1])
for r in rows:
    print(" ".join(f"{k}={r[k]:.2f}" for k in ("tick", "y", "vx", "vy", "vz", "hspeed_u", "water", "ground")))
