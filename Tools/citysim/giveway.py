"""Brix Town - resident cars giving way to what a player controls (docs/specs/domains/LIVING_CITY.md CITY-049).

A resident car's place is a lookup on its timeline (traffic.py car_at, the Unreal build's FBrixTraffic::At). Near a player
that lookup is shifted: each car may run `late` minutes behind its timeline, and is shown where its timeline had it that
long ago. This is presented state near players only (LIVING_CITY §1); the day's plan is never changed.

    slow, wait or stop   a moving car with something a player controls (their car, or the player on foot) ahead in its
                         path, or a resident car already held up ahead of it, slows over `see_m` and stands `stop_m` short
                         of it: its late grows by the share of each moment it is held back
    never speeds         while it is on the move its late never shrinks, so it never goes faster than its timeline,
                         which keeps to the limits
    catches up           only at its next stop: standing still where it parks (a bus: at a stop or its layover), it leaves
                         as soon as its timeline is there too; a stop shorter than the delay is skipped
    placed back          a car more than `reset_min` behind and further than `unload_m` from every player is put back on
                         its timeline out of sight (Q-25)

Numbers in World/rules/traffic.json "give_way". Standard library only.
"""
import math


class GiveWay:
    def __init__(self, rules, car_at, dims, kind=lambda k: 0):
        """car_at(k, minute) -> None or dict(pos=(x, z), head=(hx, hz), moving, trip); dims(k) -> (length, width, ahead):
        ahead is how far the car's middle is in front of its point. kind(k): 2 for a bus."""
        self.r, self.car_at, self.dims, self.kind = rules, car_at, dims, kind
        self.late = {}

    def at(self, k, m):
        """Where car k is shown at minute m: its timeline, `late` minutes ago."""
        return self.car_at(k, max(0.0, m - self.late.get(k, 0.0)))

    def circles(self, k, c):
        """Car k standing at c as three circles along its length: (x, z, r)."""
        L, W, ahead = self.dims(k)
        return [(c["pos"][0] + c["head"][0] * (ahead + f * (L - W) / 2), c["pos"][1] + c["head"][1] * (ahead + f * (L - W) / 2), W / 2) for f in (-1, 0, 1)]

    def share(self, k, c, obstacles):
        """How much of its timeline's speed car k may keep now: 1 nothing in its way, 0 stand."""
        L, W, ahead = self.dims(k)
        hx, hz = c["head"]
        fx, fz = c["pos"][0] + hx * (ahead + L / 2), c["pos"][1] + hz * (ahead + L / 2)
        best = 1.0
        for ox, oz, r in obstacles:
            dx, dz = ox - fx, oz - fz
            a, w = dx * hx + dz * hz, abs(-dx * hz + dz * hx)
            if w > W / 2 + self.r["pad_m"] + r or a < -L or a - r > self.r["see_m"]:
                continue
            gap = a - r
            best = min(best, max(0.0, min(1.0, (gap - self.r["stop_m"]) / (self.r["see_m"] - self.r["stop_m"]))))
        return best

    def catch_up(self, k, m, L, c):
        """Standing at a stop `L` behind: the least delay at which its timeline still has it standing there."""
        def same(q):
            return q is not None and not q["moving"] and math.dist(q["pos"], c["pos"]) < .05
        if same(self.car_at(k, m)):
            return 0.0
        lo, hi = 0.0, L
        for _ in range(24):
            mid = (lo + hi) / 2
            if same(self.car_at(k, max(0.0, m - mid))):
                hi = mid
            else:
                lo = mid
        return hi

    def step(self, m, dt, players, cars):
        """One moment: players is a list of what players control near the cars, each (x, z, r) (their car as circles
        too); cars the ids to consider; dt in game minutes."""
        held = [o for k, L in self.late.items() if L > 0 for o in self.circles(k, self.at(k, m) or {"pos": (1e9, 1e9), "head": (1, 0)})]
        for k in cars:
            c = self.at(k, m)
            L = self.late.get(k, 0.0)
            if c is None:
                continue
            near = min((math.dist(c["pos"], (p[0], p[1])) for p in players), default=1e9)
            if L > self.r["reset_min"] and near > self.r["unload_m"]:
                self.late[k] = 0.0                                  # placed back on its timeline out of sight (Q-25)
                continue
            if not c["moving"]:
                if L > 0 and (self.kind(k) == 2 or not c["trip"]):
                    self.late[k] = self.catch_up(k, m, L, c)     # at its next stop: leaves when its timeline does
                continue
            if near > self.r["near_m"]:
                continue
            own = self.circles(k, c) if L > 0 else []
            s = self.share(k, c, players + [o for o in held if o not in own])
            if s < 1:
                self.late[k] = L + dt * (1 - s)                    # held back; never less late while moving
