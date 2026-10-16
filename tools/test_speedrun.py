"""Oracle for the speedrun tools (timer, LiveSplit, the jump while ducked) in the real game (the usual build,
SERIOUSBRIDGE_DEV=1). A stand-in LiveSplit server listens on 127.0.0.1:16834 and keeps what the game sends it.

    python tools/test_speedrun.py --game "<Serious Sam folder>" [--launch]

One run through the steps, in order:
  start    a new game: the timer waits for control, then starts; LiveSplit gets reset, starttimer, its game time paused
           and set (setgametime);
  igt      running: the IGT goes as the real time; paused (the Pause key) the IGT stops, the RTA doesn't;
  sync     LiveSplit's game time follows the IGT (setgametime ten times a second, within 0.2 s of it);
  split    the level's way out taken: a split (LiveSplit too); the next level's load doesn't count in the IGT;
  finish   the game's end: the last split, the timer stops (RTA and IGT stay);
  reset    a new game again: LiveSplit reset, the timer ready again;
  duckjump jump and duck held together (as Sam's controls give them: one number, up - down): he jumps, ducked.
Prints PASS/FAIL per step.
"""
import argparse
import re
import socket
import threading
import time

import test_hl


class FakeLiveSplit:
    """A LiveSplit server stand-in: accepts connections on 16834, keeps every line."""

    def __init__(self, port=16834):
        self.lines = []
        self.lock = threading.Lock()
        self.server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server.bind(("127.0.0.1", port))
        self.server.listen(4)
        threading.Thread(target=self.accept, daemon=True).start()

    def accept(self):
        while True:
            try:
                conn, _ = self.server.accept()
            except OSError:
                return
            threading.Thread(target=self.read, args=(conn,), daemon=True).start()

    def read(self, conn):
        buf = b""
        while True:
            try:
                data = conn.recv(4096)
            except OSError:
                return
            if not data:
                return
            buf += data
            while b"\r\n" in buf:
                line, buf = buf.split(b"\r\n", 1)
                with self.lock:
                    self.lines.append((time.time(), line.decode("ascii", "replace")))

    def since(self, t0):
        with self.lock:
            return [l for t, l in self.lines if t >= t0]


def status(game):
    out = game.run("hl_Status();", want="[SeriousHL] run")
    m = re.findall(r"run (\w+)[^;]*; RTA ([\d:.]+), IGT ([\d:.]+), (\d+) splits; LiveSplit (\w+ ?\w*)", out)
    if not m:
        return None
    state, rta, igt, splits, ls = m[-1]
    return {"state": state, "rta": secs(rta), "igt": secs(igt), "splits": int(splits), "ls": ls.strip()}


def secs(s: str) -> float:
    parts = [float(p) for p in s.split(":")]
    t = 0.0
    for p in parts:
        t = t * 60 + p
    return t


def gametime(line: str) -> float:
    return secs(line.split(" ", 1)[1])


def subsequence(lines, want):
    i = 0
    for l in lines:
        if i < len(want) and l.startswith(want[i]):
            i += 1
    return i == len(want)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--game", required=True)
    ap.add_argument("--launch", action="store_true")
    a = ap.parse_args()
    game = test_hl.Game(a.game)
    kind = "tse" if (game.folder / "Bin" / "EntitiesMP.dll").exists() else "tfe"
    level = "LevelsMP/2_1_Ziggurrat" if kind == "tse" else "11_AlleyOfSphinxes"
    ls = FakeLiveSplit()
    test_hl.HANG.unlink(missing_ok=True)
    if a.launch:
        test_hl.launch(game)
    results = []
    try:
        game.run("sr_bLiveSplit=1;", 'sr_strLiveSplit="127.0.0.1:16834";', "sr_iStartOn=1;")
        time.sleep(4.0)  # (the line to LiveSplit is tried every 3 s)

        # start
        t0 = time.time()
        test_hl.start_level(game, level)
        game.run("cht_bGod=1;")
        time.sleep(1.0)
        st = status(game)
        lines = ls.since(t0)
        ok = st is not None and st["state"] == "running" and subsequence(lines, ["reset", "starttimer", "initgametime", "pausegametime", "setgametime"]) \
            and st["ls"] == "connected"
        results.append(("start: on control; LiveSplit reset, started, game time paused and set", ok,
                        f"state {st and st['state']}, LiveSplit {st and st['ls']}, got {[l for l in lines if not l.startswith('setgametime')][:8]}"))

        # igt
        s0 = status(game)
        time.sleep(4.0)
        s1 = status(game)
        game.run("hl_TestPause();")
        time.sleep(3.0)
        s2 = status(game)
        game.run("hl_TestPause();")
        time.sleep(1.0)
        run_ratio = (s1["igt"] - s0["igt"]) / max(s1["rta"] - s0["rta"], 0.01)
        paused_igt, paused_rta = s2["igt"] - s1["igt"], s2["rta"] - s1["rta"]
        # (the pause comes a few ticks after the command: what the IGT gains is those ticks, not the pause)
        ok = 0.9 <= run_ratio <= 1.1 and paused_rta > 2.5 and paused_igt < 0.1 * paused_rta
        results.append(("igt: goes as real time; stops on pause while the RTA goes on", ok,
                        f"running IGT/RTA {run_ratio:.3f}; paused 3 s: IGT +{paused_igt:.2f} s, RTA +{paused_rta:.2f} s"))

        # sync
        t1 = time.time()
        time.sleep(2.0)
        st = status(game)
        sets = [gametime(l) for l in ls.since(t1) if l.startswith("setgametime")]
        ok = len(sets) >= 12 and sets == sorted(sets) and abs(sets[-1] - st["igt"]) < 0.4
        results.append(("sync: LiveSplit's game time follows the IGT", ok,
                        f"{len(sets)} setgametime in 2 s, last {sets[-1] if sets else None} vs IGT {st['igt']:.2f}"))

        # split
        s3 = status(game)
        t2 = time.time()
        game.run("hl_TestLevelExit();", want="level exit")
        end = time.time() + 120
        while time.time() < end:
            time.sleep(2.0)
            where = game.where()
            if "netricsa" in where:
                game.run("hl_CloseNetricsa();")  # (Netricsa pauses the game: no ticks, no game time)
                continue
            if where and "no player" not in where and "cinematic" not in where:
                break
            if "cinematic" in where:
                game.run("hl_iTestFire=2;")
        time.sleep(2.0)
        s4 = status(game)
        lines = ls.since(t2)
        load_rta, load_igt = s4["rta"] - s3["rta"], s4["igt"] - s3["igt"]
        ok = s4["splits"] == 1 and "split" in lines and load_igt < load_rta - 1.0
        results.append(("split: the level's way out -> a split; the load not in the IGT", ok,
                        f"{s4['splits']} splits, LiveSplit split {'split' in lines}; across the change RTA +{load_rta:.1f} s, IGT +{load_igt:.1f} s"))

        # finish
        t3 = time.time()
        game.run("hl_TestGameEnd();")
        time.sleep(1.5)
        s5 = status(game)
        time.sleep(1.5)
        s6 = status(game)
        lines = ls.since(t3)
        ok = s5["state"] == "finished" and s5["splits"] == 2 and "split" in lines and s5["rta"] == s6["rta"] and s5["igt"] == s6["igt"]
        results.append(("finish: the game's end -> the last split, the timer stops", ok,
                        f"state {s5['state']}, {s5['splits']} splits, RTA {s5['rta']} then {s6['rta']}"))

        # reset
        t4 = time.time()
        test_hl.start_level(game, level)
        time.sleep(1.0)
        st = status(game)
        lines = ls.since(t4)
        ok = "reset" in lines and st["state"] in ("running", "ready") and st["splits"] == 0
        results.append(("reset: a new game resets LiveSplit and the timer", ok, f"state {st['state']}, splits {st['splits']}, reset sent {'reset' in lines}"))

        # duckjump
        game.run("cht_bGod=1;")
        rows = test_hl.trace(game, 30, 'hl_TestKeys("jd24");')
        rise = max(r["y"] for r in rows) - rows[0]["y"]
        ducked_air = any(r["ducked"] and r["ground"] == 0 for r in rows)
        jumps = int(sum(r["jumped"] for r in rows))
        ok = jumps >= 1 and ducked_air and rise > 1.5
        results.append(("duckjump: jump and duck held together jump ducked", ok, f"{jumps} jumps, ducked in the air {ducked_air}, feet {rise:.2f} m up"))
    except test_hl.Hung as e:
        results.append(("the game", False, f"stopped answering: {e}"))
        test_hl.kill(game)
    except Exception as e:
        results.append(("the run", False, f"{type(e).__name__}: {e}"))
    for name, ok, detail in results:
        print(f"{'PASS' if ok else 'FAIL'}: {name} ({detail})")


if __name__ == "__main__":
    main()
