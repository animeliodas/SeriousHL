"""Dev: a screenshot of each Half-Life view model in hand (um win shot), and the view model's status.

    python tools/view_shots.py --game "<Serious Sam folder>" [--launch] [--guns colt,knife,...] [--out _shots/vm]
"""
import argparse
import re
import subprocess
import time
from pathlib import Path

import test_hl
import test_weapons


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    ap.add_argument("--guns", default="colt")
    ap.add_argument("--out", default=str(Path(__file__).resolve().parents[2] / "_shots" / "vm"))
    ap.add_argument("--fire", type=int, default=0, help="fire for this many ticks, then shoot the picture mid-way")
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    kind = "tse" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "tfe"
    if a.launch:
        test_hl.launch(game)
        test_hl.start_level(game, "LevelsMP/2_1_Ziggurrat" if kind == "tse" else "11_AlleyOfSphinxes")
    game.run("cht_bGod=1;", "cht_bInvisible=1;", "cht_bGiveAll=1;", "hl_bWeapons=1;")
    guns = test_weapons.Guns(game, kind)
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    for gun in a.guns.split(","):
        if not guns.take(gun):
            print(f"{gun}: not in hand")
            continue
        time.sleep(1.5)
        if a.fire:
            game.run(f"hl_iTestFire={a.fire};")
            time.sleep(0.25)
        out = f"{a.out}_{kind}_{gun}.png"
        subprocess.run(["bash", "-c", f'um win shot --exe SeriousSam_Custom.exe "{out}" --scale 0.5'], check=False, capture_output=True)
        st = game.run("hl_Status();", want="view models")
        print(gun, [l for l in st.splitlines() if "view models" in l][-1:], out)


if __name__ == "__main__":
    main()
