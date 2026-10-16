"""Oracle for the speedrun build (plugin/dist/Run_<config>/SeriousHL.dll installed): the ruleset can't be changed, the
movement is Half-Life's, and the mark on the screen names the build and its DLL.

    python tools/test_run.py --game "<Serious Sam folder>" [--launch] [--shot out.png]

  hash     the log's SHA-256 is the installed SeriousHL.dll's, and the mark shows its first 8 digits;
  locked   no hl_* to set (hl_bAutoJump=0 is refused), no test hooks (hl_TestKeys), the ruleset stays Half-Life's;
  moving   the player's movement is taken by the rules;
  mark     the mark is drawn on the screen (bottom left, orange).
"""
import argparse
import hashlib
import re
import subprocess
import time
from pathlib import Path

import test_hl


def window_shot(game: test_hl.Game, out: Path) -> bool:
    ps = ("(Get-Process SeriousSam_Custom -ErrorAction SilentlyContinue | Where-Object { $_.Path -like '"
          + str(game.folder.resolve()) + "*' }).MainWindowHandle")
    hwnd = subprocess.run(["powershell", "-NoProfile", "-Command", ps], capture_output=True, text=True).stdout.split()
    if not hwnd:
        return False
    r = subprocess.run(["bash", "-lc", f'um win shot --hwnd {hwnd[0]} "{out.as_posix()}"'], capture_output=True, text=True)
    return out.exists()


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    ap.add_argument("--shot", default=str(Path(__file__).resolve().parents[2] / "_shots" / "hl_run.png"))
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    if a.launch:
        test_hl.launch(game)
    results = []
    level = "LevelsMP/2_1_Ziggurrat" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "11_AlleyOfSphinxes"
    test_hl.start_level(game, level)

    # hash
    dll = game.folder / "Bin" / "Plugins" / "SeriousHL.dll"
    digest = hashlib.sha256(dll.read_bytes()).hexdigest()
    m = re.search(r"speedrun build (\S+): .*SHA-256 ([0-9a-f]{64})", game.text())
    status = game.run("hl_Status();", want="[SeriousHL] SeriousHL")
    mark = re.search(r"SeriousHL \S+ RUN ([0-9a-f]{8})", status)
    ok = m is not None and m.group(2) == digest and mark is not None and mark.group(1) == digest[:8]
    results.append(("hash: the mark and the log name the installed DLL", ok,
                    f"file {digest[:16]}..., log {m.group(2)[:16] if m else None}..., mark {mark.group(1) if mark else None}"))

    # locked
    out = game.run("hl_bAutoJump=0;", "hl_fMaxSpeed=9000;", 'hl_TestKeys("j5");', "hl_Status();", want="[SeriousHL] SeriousHL")
    refused = "hl_TestKeys" not in out or "not declared" in out or "nknown" in out
    st = re.search(r"autojump (\d), bhop cap (\d), (\d+) fps, 1 u = ([\d.]+)", out)
    keys = "test keys" in out
    ok = st is not None and st.groups() == ("1", "0", "100", "0.0297") and not keys
    results.append(("locked: hl_* and test hooks don't exist, the ruleset stays", ok,
                    f"ruleset {st.groups() if st else None}, test keys taken {keys}; console said: "
                    + " | ".join(l.strip() for l in out.splitlines() if "hl_" in l and "dev>" not in l)[:200]))

    # moving
    out = game.run("hl_Status();", want="[SeriousHL] SeriousHL")
    ok = "movement taken" in out
    results.append(("moving: the rules move the player", ok, re.sub(r".*?(movement [^;]*);.*", r"\1", out.strip().splitlines()[-1])))

    # mark
    shot = Path(a.shot)
    shot.unlink(missing_ok=True)
    seen = False
    if window_shot(game, shot):
        from PIL import Image
        im = Image.open(shot).convert("RGB")
        w, h = im.size
        box = im.crop((0, int(h * 0.80), int(w * 0.35), h))
        orange = sum(1 for (r, g, b) in box.getdata() if r > 200 and 140 < g < 215 and b < 110)
        seen = orange > 30
    results.append(("mark: drawn in the corner", seen, f"screenshot {shot}"))

    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}: {name} ({detail})")


if __name__ == "__main__":
    main()
