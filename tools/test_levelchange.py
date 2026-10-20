"""Oracle: level changes with the plugin's sounds playing on the player (the usual build, SERIOUSBRIDGE_DEV=1).

    python tools/test_levelchange.py --game "<Serious Sam folder>" [--launch] [--rounds 6]

Sam's level change (a WorldLink's: the player carried over, the old world cleared) while a long HEV sentence plays
on the player (hl_TestSay), round after round between two levels; the game must stay up and the player come into each.
A sound object whose owner is gone from the old world is asked by Sam's mixer (its timer thread) where it is: a crash
in no module.
"""
import argparse
import time

import test_hl

# (the console's strings take "\\" for a backslash)
PAIRS = {"tse": ("LevelsMP/1_1_Palenque", r"Levels\\LevelsMP\\1_2_Palenque.wld", r"Levels\\LevelsMP\\1_1_Palenque.wld"),
         "tfe": ("01_Hatshepsut", r"Levels\\02_SandCanyon.wld", r"Levels\\01_Hatshepsut.wld")}


def alive(game) -> bool:
    """the player in the new level (Netricsa closed, an intro skipped: a paused game changes no level)"""
    end = time.time() + 40.0
    while time.time() < end:
        try:
            last = game.run("hl_Where();", want="where:", timeout=8.0)
        except Exception:
            return False
        if "netricsa" in last:
            game.run("hl_CloseNetricsa();")
        elif "cinematic" in last:
            game.run("hl_iTestFire=2;")
        elif "where:" in last:
            return True
        time.sleep(1.0)
    return False


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    ap.add_argument("--rounds", type=int, default=6)
    ap.add_argument("--set", default="", help="tse or tfe levels (default: the game's own)")
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    kind = a.set or ("tse" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "tfe")
    first, there, back = PAIRS[kind]
    if a.launch:
        test_hl.launch(game)
    test_hl.start_level(game, first)
    game.run("cht_bGod=1;", "cht_bInvisible=1;")
    ok = 0
    for r in range(a.rounds):
        game.run('hl_TestSay("HEV_A0");')
        time.sleep(2.0)
        game.run(f'hl_TestChangeLevel("{there if r % 2 == 0 else back}");')
        time.sleep(12.0)
        if not alive(game):
            print(f"FAIL: round {r + 1}: the game is gone or doesn't answer after the level change")
            return
        ok += 1
        print(f"round {r + 1}: up")
    print(f"PASS: {ok} level changes with the suit speaking, the game up")


if __name__ == "__main__":
    main()
