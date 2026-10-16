"""Build the player's download: release/SeriousHL-<version>.zip with Install.cmd, both games' DLLs and a short readme.

    python tools/make_release.py [run]  (after plugin/build.sh [all run])

run: the speedrun build (plugin/dist/Run_<config>): SeriousHL-<version>-run.zip

Inside: SeriousHL/Install.cmd, Uninstall.cmd, install.ps1, TFE/SeriousHL.dll, TSE/SeriousHL.dll, README.txt. Nothing
of the games' or Classics Patch's own: the installer fetches Classics Patch from its GitHub release when it's missing.
"""
import hashlib
import re
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "plugin" / "Source" / "Main.cpp").read_text(encoding="utf-8")
VERSION = ".".join(re.search(r"MakeVersion\((\d+), (\d+), (\d+)\)", MAIN).groups())

README = f"""SeriousHL {VERSION} - Half-Life movement and weapons for Serious Sam Classic (The First Encounter, The Second Encounter)
https://github.com/animeliodas/SeriousHL

Install: double-click Install.cmd. It finds the games in Steam (or GOG; or drop the game's folder on Install.cmd),
adds Serious Sam Classics Patch 1.9.4 if it isn't there (downloaded from its GitHub release, checked by SHA-256),
puts SeriousHL.dll into Bin\\Plugins and a shortcut on the desktop. Play from that shortcut
(or Bin\\SeriousSam_Custom.exe; in Steam: launch option +game ClassicsPatchMod).

Console (~):
  hl_bAutoJump 1   holding jump hops on every landing (0: a jump per press, as Half-Life)
  hl_bBhopCap 0    no speed cap (1: Steam Half-Life's - over 1.7x 320 u/s a landing jump cuts the speed)
  hl_bEnable 1     Half-Life movement (0: Sam's own)
  hl_bWeapons 1    Half-Life's weapons on Sam's guns (right mouse button: second fire; 0: Sam's weapons)
  hl_Status()      what it does now
Timer, LiveSplit, speedometer: SeriousSpeedrun (github.com/animeliodas/SeriousSpeedrun), sr_iSpeedUnits 1.
Uninstall: Uninstall.cmd (Classics Patch stays).

Single player. Made with Claude Code (AI); tested by scripts in the real games (README on GitHub).
GPL-2.0. Not affiliated with Valve or Croteam.
"""


RUN_README = f"""SeriousHL {VERSION} RUN - the speedrun build of Half-Life movement for Serious Sam Classic
https://github.com/animeliodas/SeriousHL

The same movement as SeriousHL {VERSION} (the movement only: Sam's weapons stay), with Half-Life's ruleset fixed: hold jump to hop (autojump), no bunny hop
cap, 100 fps rules, Half-Life's speeds, friction, gravity, ducking and damage pushes. There are no hl_* console
variables to change and no test hooks. The timer (RTA, IGT), LiveSplit and a speedometer: SeriousSpeedrun
(github.com/animeliodas/SeriousSpeedrun; sr_iSpeedUnits 1 for Half-Life units). In the bottom left corner the game shows "SeriousHL {VERSION} RUN <8 digits>":
the first digits of this DLL's SHA-256 (the full one is in SeriousSam_Custom.log) - moderators compare them with the
release page.

Install: double-click Install.cmd (it replaces the usual SeriousHL.dll; the usual zip puts that back).
Single player. Made with Claude Code (AI). GPL-2.0. Not affiliated with Valve or Croteam.
"""


def main() -> None:
    import sys
    run = len(sys.argv) > 1 and sys.argv[1] == "run"
    flavor = "Run_" if run else ""
    out = ROOT / "release" / (f"SeriousHL-{VERSION}-run.zip" if run else f"SeriousHL-{VERSION}.zip")
    out.parent.mkdir(exist_ok=True)
    files = {
        "Install.cmd": ROOT / "install" / "Install.cmd",
        "Uninstall.cmd": ROOT / "install" / "Uninstall.cmd",
        "install.ps1": ROOT / "install" / "install.ps1",
        "TFE/SeriousHL.dll": ROOT / "plugin" / "dist" / f"{flavor}Release_TFE105" / "SeriousHL.dll",
        "TSE/SeriousHL.dll": ROOT / "plugin" / "dist" / f"{flavor}Release_TSE107" / "SeriousHL.dll",
    }
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name, path in files.items():
            z.write(path, f"SeriousHL/{name}")
        z.writestr("SeriousHL/README.txt", (RUN_README if run else README).replace("\n", "\r\n"))
    digest = hashlib.sha256(out.read_bytes()).hexdigest()
    print(f"{out} ({out.stat().st_size} bytes)\nsha256 {digest}")
    for game in ("TFE", "TSE"):
        dll = files[f"{game}/SeriousHL.dll"]
        print(f"  {game} SeriousHL.dll sha256 {hashlib.sha256(dll.read_bytes()).hexdigest()}")


if __name__ == "__main__":
    main()
