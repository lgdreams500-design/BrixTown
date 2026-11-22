"""Brix Town - repair crews mending crash damage (docs/specs/domains/LIVING_CITY.md CITY-074, CITY-075; F-VEH-CRASH).

Damage is stored state: a list of what broke, where and when (LIVING_CITY §1). When each thing is mended is not stored
and not stepped: it is worked out from that list and World/rules/crash.json alone, so every machine gets the same answer
for the same list, in any order it was given.

The rule. Each depot sends out `crews_per_depot` crews. Damage is taken in the order it happened (ties by id). Each piece
of damage is reported `report_min` after it happened, and goes to the crew that would finish it first: a crew leaves
when it is free and the report is in, walks from where it last was at its own pace (straight line times `detour`), and
works the time set for that kind. Crews only work between `start` and `end`: a job that would not be finished by `end`
waits for the next morning, when the crew sets out again from where it is.

    plan = repairs(damage, depots, rules, pace)     # {damage id: dict(crew, leave, arrive, done)} in game minutes
    mended(plan, id, t)                             # is that thing whole again at game minute t?

Standard library only, like citysim.py.
"""
import math

from citysim import hm


def _into_hours(t, start, end):
    """The first moment at or after t within working hours."""
    day, tod = divmod(t, 1440)
    if tod < start:
        return day * 1440 + start
    if tod >= end:
        return (day + 1) * 1440 + start
    return t


def repairs(damage, depots, rules, pace=60.0):
    """damage: [dict(id, at, kind, pos)] with `at` in game minutes since day 0 00:00 and pos (x, z) in metres.
    depots: [(depot id, (x, z) of its door)]. pace: real seconds per game minute (citysim.City.pace)."""
    r = rules["repair"]
    start, end = hm(r["start"]), hm(r["end"])
    speed = r["walk_mps"] * pace                                  # metres per game minute
    crews = []
    for did, door in sorted(depots):
        for n in range(r["crews_per_depot"]):
            crews.append(dict(id="%s/%d" % (did, n), free=-1e18, pos=tuple(door)))
    plan = {}
    if not crews:
        return plan
    for d in sorted(damage, key=lambda d: (d["at"], d["id"])):
        work = rules["kinds"].get(d["kind"], {}).get("work_min", 30)
        best = None
        for c in crews:
            walk = math.dist(c["pos"], d["pos"]) * r["detour"] / speed
            leave = _into_hours(max(c["free"], d["at"] + r["report_min"]), start, end)
            if leave % 1440 + walk + work > end:                  # not done by the end of the day: tomorrow morning
                leave = (leave // 1440 + 1) * 1440 + start
            done = leave + walk + work
            if best is None or done < best[0] - 1e-9:
                best = (done, c, leave, walk)
        done, c, leave, walk = best
        plan[d["id"]] = dict(crew=c["id"], leave=leave, arrive=leave + walk, done=done, kind=d["kind"], pos=tuple(d["pos"]))
        c["free"], c["pos"] = done, tuple(d["pos"])
    return plan


def mended(plan, did, t):
    return did in plan and t >= plan[did]["done"]
