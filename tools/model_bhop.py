"""Offline model of SeriousHL's tick loop (Sam's 20 Hz ticks, Half-Life frames inside): what strafe-hopping gains, to set test_hl.py's expectations."""
import math
import sys

G, JUMP = 800.0, math.sqrt(2 * 800 * 45)
ACC, AIRACC, FRIC, STOP, MAXS = 10, 10, 4, 100, 320


def run(turn_deg_tick, ticks=120, cap=False, fps=100, side=1.0, fwd=0.0):
    tick = 0.05
    n = max(1, round(fps * tick))
    h = tick / n
    vx = vy = vz = 0.0  # y up
    y = 0.0
    yaw = 0.0  # degrees, left positive
    ground = True
    top = 0.0
    for t in range(ticks):
        jumped = False
        if ground:
            if cap:
                sp = math.sqrt(vx * vx + vy * vy + vz * vz)
                if sp > 1.7 * MAXS:
                    f = 1.7 * MAXS / sp * 0.65
                    vx, vy, vz = vx * f, vy * f, vz * f
            vy = JUMP
            ground = False
            jumped = True
        dy = 0.0
        for k in range(n):
            a = math.radians(yaw + turn_deg_tick * (k + 1) / n)
            # forward = (-sin a, -cos a) in x,z; right = (cos a, -sin a)
            fx, fz = -math.sin(a), -math.cos(a)
            rx, rz = math.cos(a), -math.sin(a)
            wx = fx * fwd * 400 + rx * side * 400
            wz = fz * fwd * 400 + rz * side * 400
            ws = math.hypot(wx, wz)
            dx, dz = (wx / ws, wz / ws) if ws > 0 else (0, 0)
            ws = min(ws, MAXS)
            vy -= G * h * 0.5
            add = min(ws, 30) - (vx * dx + vz * dz)
            if add > 0:
                acc = min(AIRACC * ws * h, add)
                vx += dx * acc
                vz += dz * acc
            dy += vy * h
            vy -= G * h * 0.5
        yaw += turn_deg_tick
        y += dy
        if y <= 0 and not jumped:
            y = 0
            vy = 0
            ground = True
        top = max(top, math.hypot(vx, vz))
    return top, math.hypot(vx, vz)


if __name__ == "__main__":
    for turn in (-6, -9, -12, -15, -18, -24, -30):
        print(turn, "deg/tick:", "free %.0f/%.0f" % run(turn), "capped %.0f/%.0f" % run(turn, cap=True), "fps20 %.0f" % run(turn, fps=20)[0])
