"""Build the player's download: release/SeriousHL-<version>.zip with Install.cmd, both games' DLLs and a short readme.

    python tools/make_release.py        (after plugin/build.sh)

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

README = f"""SeriousHL {VERSION} - Half-Life movement for Serious Sam Classic (The First Encounter, The Second Encounter)
https://github.com/animeliodas/SeriousHL

Install: double-click Install.cmd. It finds the games in Steam (or GOG; or drop the game's folder on Install.cmd),
adds Serious Sam Classics Patch 1.9.4 if it isn't there (downloaded from its GitHub release, checked by SHA-256),
puts SeriousHL.dll into Bin\\Plugins and a shortcut on the desktop. Play from that shortcut
(or Bin\\SeriousSam_Custom.exe; in Steam: launch option +game ClassicsPatchMod).

Console (~):
  hl_bAutoJump 1   holding jump hops on every landing (0: a jump per press, as Half-Life)
  hl_bBhopCap 0    no speed cap (1: Steam Half-Life's - over 1.7x 320 u/s a landing jump cuts the speed)
  hl_bEnable 1     Half-Life movement (0: Sam's own)
  hl_Status()      what it does now
Uninstall: Uninstall.cmd (Classics Patch stays).

Single player. Made with Claude Code (AI); tested by scripts in the real games (README on GitHub).
GPL-2.0. Not affiliated with Valve or Croteam.
"""


def main() -> None:
    out = ROOT / "release" / f"SeriousHL-{VERSION}.zip"
    out.parent.mkdir(exist_ok=True)
    files = {
        "Install.cmd": ROOT / "install" / "Install.cmd",
        "Uninstall.cmd": ROOT / "install" / "Uninstall.cmd",
        "install.ps1": ROOT / "install" / "install.ps1",
        "TFE/SeriousHL.dll": ROOT / "plugin" / "dist" / "Release_TFE105" / "SeriousHL.dll",
        "TSE/SeriousHL.dll": ROOT / "plugin" / "dist" / "Release_TSE107" / "SeriousHL.dll",
    }
    with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
        for name, path in files.items():
            z.write(path, f"SeriousHL/{name}")
        z.writestr("SeriousHL/README.txt", README.replace("\n", "\r\n"))
    digest = hashlib.sha256(out.read_bytes()).hexdigest()
    print(f"{out} ({out.stat().st_size} bytes)\nsha256 {digest}")


if __name__ == "__main__":
    main()
