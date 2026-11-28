"""Skip time (LIVING_CITY WLD-073, WLD-074; F-TIME-SKIP level 1).      python -m unittest test_time_skip -v

A skip moves the world clock forward and nothing else. Light, weather, snow, residents, shops, plants and needs are all
worked out from the clock (TECH-004), so after a skip they are simply whatever the clock says at the new time: nothing is
stepped through the skipped minutes. Steps and limits are data in World/rules/time_skip.json.

    target(rules, cal, t, step)                    the clock after that step, or None for an unknown step
    next_weather(rules, cal, t)                    the start of the first six-hour slot after t whose weather is not t's
    ask(rules, cal, t, step, asker, host, busy)    the server's answer to a skip request: (new t, None) or (t, why not)
"""
from citysim import Clock, weather


def next_weather(rules, cal, t):
    """Start of the first weather slot after t whose weather differs from now; if none within max_days, t + max_days."""
    nw = rules["next_weather"]
    size = nw["slot_minutes"]
    clock = Clock(cal["calendar"])
    now = weather(cal, clock, t)
    first = (int(t // size) + 1) * size
    limit = t + nw["max_days"] * 1440
    b = first
    while b < limit:
        if weather(cal, clock, b) != now:
            return b
        b += size
    return limit


def target(rules, cal, t, step):
    for s in rules["steps"]:
        if s["id"] == step:
            if "minutes" in s:
                return t + s["minutes"]
            if s.get("to") == "next_weather":
                return next_weather(rules, cal, t)
    return None


def ask(rules, cal, t, step, asker, host, busy=()):
    """A client asks the server to skip (WLD-074). Only the host may skip on a shared server, and not while busy."""
    if rules.get("who") == "host" and asker != host:
        return t, "not_host"
    for b in busy:
        if b in rules.get("not_during", ()):
            return t, "busy:" + b
    new = target(rules, cal, t, step)
    if new is None:
        return t, "unknown_step"
    return new, None
