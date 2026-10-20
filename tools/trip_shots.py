"""Dev: a tripmine put on the floor ahead, powered, looked at from a few steps back (um win shot): Half-Life's model on
Sam's floor and its beam going up.

    python tools/trip_shots.py --game "<Serious Sam folder>" [--launch] [--out _shots/trip]
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
    ap.add_argument("--out", default=str(Path(__file__).resolve().parents[2] / "_shots" / "trip"))
    ap.add_argument("--back", type=float, default=2.0, help="metres back from where it was put")
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    kind = "tse" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "tfe"
    if a.launch:
        test_hl.launch(game)
    test_hl.start_level(game, "LevelsMP/2_1_Ziggurrat" if kind == "tse" else "11_AlleyOfSphinxes")
    game.run("cht_bGod=1;", "cht_bInvisible=1;", "cht_bGiveAll=1;", "hl_bWeapons=1;", "hl_bSlots=1;", "hl_bFastSwitch=0;")
    guns = test_weapons.Guns(game, kind)
    guns.take("cannon")
    for _ in range(3):
        game.run("hl_iTestSelect=5;")
        time.sleep(0.4)
    game.run("hl_iTestFire=1;")
    time.sleep(1.5)
    mark = len(game.text())
    game.run("hl_fTestPitch=-45;")
    time.sleep(0.5)
    game.run("hl_iTestFire=1;")
    test_weapons.wait_for(game, mark, "tripmine on", 8.0)
    at = re.findall(r"tripmine placed at ([\d.-]+) ([\d.-]+) ([\d.-]+)", game.text()[mark:])
    where = re.findall(r"where: ([\d.-]+) ([\d.-]+) ([\d.-]+)", game.run("hl_Where();", want="where:"))
    if at and where:
        dx, dz = float(at[-1][0]) - float(where[-1][0]), float(at[-1][2]) - float(where[-1][2])
        n = max((dx * dx + dz * dz) ** 0.5, 0.01)
        game.run(f'hl_Move("{-dx / n * a.back:.2f} 0 {-dz / n * a.back:.2f}");', want="player at")
    game.run("hl_fTestPitch=-25;", "hl_bWorldDebug=1;")
    time.sleep(1.0)
    st = game.run("hl_Status();", want="in the world")
    print([l for l in st.splitlines() if "in the world" in l][-1:])
    Path(a.out).parent.mkdir(parents=True, exist_ok=True)
    for name, pitch in (("low", -25), ("up", 20)):
        game.run(f"hl_fTestPitch={pitch};")
        time.sleep(0.8)
        out = f"{a.out}_{kind}_{name}.png"
        subprocess.run(["bash", "-c", f'um win shot --exe SeriousSam_Custom.exe "{out}" --scale 0.5'], check=False, capture_output=True)
        print(out)
    game.run("hl_fTestPitch=0;", "hl_bSlots=0;")
    st = game.run("hl_Status();", want="tripmines")
    print([l for l in st.splitlines() if "tripmines" in l or "in the world" in l][-2:])
    print([l for l in game.run("hl_Where();", want="where:").splitlines() if "where:" in l][-1:])


if __name__ == "__main__":
    main()
