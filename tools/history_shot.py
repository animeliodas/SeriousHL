"""Dev: the HUD's pickup history - a fresh level, then all the guns and ammo given (cht_bGiveAll) and some health and
armour taken away and given back: a screenshot of the list on the right (um win shot) and the HUD's status.

    python tools/history_shot.py --game "<Serious Sam folder>" [--launch] [--out _shots/history]
"""
import argparse
import re
import subprocess
import time
from pathlib import Path

import test_hl


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    ap.add_argument("--out", default=str(Path(__file__).resolve().parents[2] / "_shots" / "history"))
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    kind = "tse" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "tfe"
    if a.launch:
        test_hl.launch(game)
    test_hl.start_level(game, "LevelsMP/2_1_Ziggurrat" if kind == "tse" else "11_AlleyOfSphinxes")
    game.run("cht_bGod=1;", "cht_bInvisible=1;", "hl_bWeapons=1;")
    time.sleep(1.0)
    game.run('hl_TestDamage("30 bullet 60 20");')
    time.sleep(0.5)
    game.run("cht_bGiveAll=1;")
    time.sleep(0.8)
    out = f"{a.out}_{kind}.png"
    Path(out).parent.mkdir(parents=True, exist_ok=True)
    subprocess.run(["bash", "-c", f'um win shot --exe SeriousSam_Custom.exe "{out}" --scale 0.5'], check=False, capture_output=True)
    st = game.run("hl_Status();", want="pickups")
    print(re.findall(r"pickups: .*", st)[-1:], out)


if __name__ == "__main__":
    main()
