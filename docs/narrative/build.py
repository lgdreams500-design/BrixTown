"""Checks the quest atlas data in src/ and builds quest_atlas.html (one self-contained page).

    python build.py          check, play it through at random, build
    python build.py --check  check and play through only

The checks mirror QST-001: every reply leads somewhere real, every stage and ending can be reached,
every person, flag and item that is used exists. The random play-throughs use the same rules as the
page's Play mode (needs, acts, retakes), so a branch that can strand the player, an ending nobody can
reach, or two facts that should never both be true shows here. They do not judge the writing: a line
that contradicts the state that led to it passes. Regard is lifted to each gate before a quest line is
tried, so the play-throughs do not test whether a gate can be reached by play.
"""
import copy
import json
import random
import sys
from pathlib import Path

HERE = Path(__file__).parent
SRC = HERE / "src"
OUT = HERE / "quest_atlas.html"
QUEST_FILES = ["live.json", "q1.json", "q2.json", "q3.json", "q4.json", "q5.json",
               "r_m1.json", "r_m2.json", "r_m3.json", "r_w.json"]
MOODS = {"neutral", "happy", "sad", "angry", "surprised", "thinking"}
BAKER = ["four_oclock_door", "feeding_the_mother", "stone_ground", "market_day", "name_on_the_door"]
COURTING = ["lunch_mei", "dinner_mei", "question_mei", "the_wedding"]


def load():
    cast = json.loads((SRC / "cast.json").read_text(encoding="utf-8"))
    world = json.loads((SRC / "world.json").read_text(encoding="utf-8"))
    quests = []
    for name in QUEST_FILES:
        data = json.loads((SRC / name).read_text(encoding="utf-8"))
        quests.extend(data["quests"] if "quests" in data else [data])
    return cast, world, quests


# ---------- the rules shared with the page's Play mode ----------

def new_state():
    return {"flags": set(), "items": {}, "rep": {}, "money": 2000, "done": {}, "home": None, "tried": set(), "again": set(), "awarded": {}}


def need_ok(need, st):
    neg = need.startswith("!")
    if neg:
        need = need[1:]
    kind, _, rest = need.partition(":")
    if kind == "flag":
        ok = rest in st["flags"]
    elif kind == "have":
        ok = st["items"].get(rest, 0) > 0
    elif kind == "money":
        ok = st["money"] >= int(rest)
    elif kind == "rep":
        who, _, n = rest.partition(":")
        ok = st["rep"].get(who, 0) >= int(n)
    elif kind == "done":
        quest, _, ends = rest.partition(":")
        got = st["done"].get(quest)
        ok = got is not None and (ends == "*" or got in ends.split("|"))
    elif kind == "home":
        ok = st.get("home") == rest
    elif kind == "again":
        ok = rest in st["again"]
    else:
        raise ValueError("unknown need " + need)
    return ok != neg


def needs_ok(needs, st):
    return all(need_ok(n, st) for n in (needs or []))


def begin_quest(quest, st):
    """Starts a quest line. Taken again, it starts clean: the flags and items its own replies gave are
    cleared and the regard its replies awarded is taken back. What its ending cost stays."""
    qid = quest["id"]
    if qid in st["tried"]:
        st["again"].add(qid)
        for stage in quest["stages"]:
            for node in stage.get("nodes", {}).values():
                for opt in node["options"]:
                    for act in opt.get("act", []):
                        kind, _, rest = act.partition(":")
                        if kind == "flag":
                            st["flags"].discard(rest)
                        elif kind == "give":
                            st["items"].pop(rest, None)
        for who, n in st["awarded"].get(qid, {}).items():
            st["rep"][who] = max(-100, min(100, st["rep"].get(who, 0) - n))
    st["tried"].add(qid)
    st["awarded"][qid] = {}
    return st["awarded"][qid]


def apply_acts(acts, st, ledger=None):
    """Applies acts; returns ('stage', id), ('end', id) or None. Regard given is noted in the ledger."""
    move = None
    for act in acts or []:
        kind, _, rest = act.partition(":")
        if kind == "flag":
            st["flags"].add(rest)
        elif kind == "unflag":
            st["flags"].discard(rest)
        elif kind == "give":
            st["items"][rest] = st["items"].get(rest, 0) + 1
        elif kind == "take":
            if st["items"].get(rest, 0) > 0:
                st["items"][rest] -= 1
        elif kind == "rep":
            who, _, n = rest.partition(":")
            st["rep"][who] = max(-100, min(100, st["rep"].get(who, 0) + int(n)))
            if ledger is not None:
                ledger[who] = ledger.get(who, 0) + int(n)
        elif kind == "pay":
            st["money"] += int(rest)
        elif kind == "charge":
            st["money"] -= int(rest)
        elif kind == "home":
            st["home"] = rest
        elif kind in ("stage", "end"):
            move = (kind, rest)
        else:
            raise ValueError("unknown act " + act)
    return move


def apply_ending(quest, end_id, st):
    end = next(e for e in quest["endings"] if e["id"] == end_id)
    st["money"] += end.get("pay", 0) - end.get("charge", 0)
    for who, n in end.get("rep", {}).items():
        st["rep"][who] = max(-100, min(100, st["rep"].get(who, 0) + n))
    for item in end.get("give", []):
        st["items"][item] = st["items"].get(item, 0) + 1
    for item in end.get("take", []):
        if st["items"].get(item, 0) > 0:
            st["items"][item] -= 1
    st["flags"].update(end.get("flags", []))
    st["done"][quest["id"]] = end_id


def start_node(stage, st):
    for alt in stage.get("starts", []):
        if needs_ok(alt["need"], st):
            return alt["node"]
    return stage["start"]


def play(quest, st, rng, problems, trace=None):
    """One random play-through of a quest. Returns the ending id, or None if it got stuck."""
    stages = {s["id"]: s for s in quest["stages"]}
    stage = stages[quest["start"]]
    ledger = begin_quest(quest, st)
    for _ in range(600):
        if "nodes" not in stage:
            apply_acts(stage.get("sim"), st)
            kind, target = stage["then"].split(":")
            if kind == "end":
                apply_ending(quest, target, st)
                return target
            stage = stages[target]
            continue
        node_id = start_node(stage, st)
        moved = False
        for _ in range(200):
            node = stage["nodes"][node_id]
            open_opts = [o for o in node["options"] if needs_ok(o.get("need"), st)]
            if not open_opts:
                problems.add(f"{quest['id']}/{stage['id']}/{node_id}: no reply is open (flags {sorted(st['flags'])})")
                return None
            # prefer replies that go somewhere, so a play-through does not dither at the door
            going = [o for o in open_opts if o["next"] or any(a.startswith(("stage:", "end:")) for a in o.get("act", []))]
            opt = rng.choice(going or open_opts)
            if trace is not None:
                trace.append((stage["id"], node_id, opt["text"]))
            move = apply_acts(opt.get("act"), st, ledger)
            if move and move[0] == "end":
                apply_ending(quest, move[1], st)
                return move[1]
            if move:
                stage = stages[move[1]]
                moved = True
                break
            if opt["next"] is None:
                break
            node_id = opt["next"]
        if not moved:
            continue
    problems.add(f"{quest['id']}: a play-through never finished")
    return None


# ---------- static checks ----------

def check(cast, world, quests):
    problems = []
    people = {p["id"] for p in cast["people"]}
    quest_ids = {q["id"]: q for q in quests}
    if len(people) != len(cast["people"]):
        problems.append("cast: an id is used twice")
    for p in cast["people"]:
        for tie in p.get("ties", []):
            if tie[0] not in people and tie[0] != "player":
                problems.append(f"cast/{p['id']}: tie to unknown {tie[0]}")
        if p["id"] not in cast["map"]:
            problems.append(f"cast/{p['id']}: no place on the map")

    flags_set, flags_read, items_given, items_used = set(), {}, set(), {}

    def note_need(need, where):
        raw = need[1:] if need.startswith("!") else need
        kind, _, rest = raw.partition(":")
        if kind == "flag":
            flags_read.setdefault(rest, where)
        elif kind == "have":
            items_used.setdefault(rest, where)
        elif kind == "money":
            int(rest)
        elif kind == "again":
            if rest not in quest_ids:
                problems.append(f"{where}: unknown quest {rest}")
        elif kind == "home":
            if rest != "owned":
                problems.append(f"{where}: unknown home state {rest}")
        elif kind == "rep":
            who, _, n = rest.partition(":")
            int(n)
            if who not in people:
                problems.append(f"{where}: rep of unknown {who}")
        elif kind == "done":
            quest, _, ends = rest.partition(":")
            if quest not in quest_ids:
                problems.append(f"{where}: unknown quest {quest}")
            elif ends != "*":
                known = {e["id"] for e in quest_ids[quest]["endings"]}
                for e in ends.split("|"):
                    if e not in known:
                        problems.append(f"{where}: {quest} has no ending {e}")
        else:
            problems.append(f"{where}: unknown need {need}")

    for q in quests:
        qid = q["id"]
        stages = {s["id"]: s for s in q["stages"]}
        endings = {e["id"] for e in q["endings"]}
        if len(stages) != len(q["stages"]):
            problems.append(f"{qid}: a stage id is used twice")
        if q["start"] not in stages:
            problems.append(f"{qid}: start stage missing")
        if q["giver"] not in people:
            problems.append(f"{qid}: unknown giver {q['giver']}")
        for n in q.get("needs", []):
            note_need(n, qid)
        for e in q["endings"]:
            flags_set.update(e.get("flags", []))
            items_given.update(e.get("give", []))
            for who in e.get("rep", {}):
                if who not in people:
                    problems.append(f"{qid}/ending {e['id']}: rep of unknown {who}")

        def note_act(act, where):
            kind, _, rest = act.partition(":")
            if kind == "flag":
                flags_set.add(rest)
            elif kind == "unflag":
                flags_read.setdefault(rest, where)
            elif kind == "give":
                items_given.add(rest)
            elif kind == "take":
                items_used.setdefault(rest, where)
            elif kind == "rep":
                who, _, n = rest.partition(":")
                int(n)
                if who not in people:
                    problems.append(f"{where}: rep of unknown {who}")
            elif kind in ("pay", "charge"):
                int(rest)
            elif kind == "home":
                pass
            elif kind == "stage":
                if rest not in stages:
                    problems.append(f"{where}: no stage {rest}")
            elif kind == "end":
                if rest not in endings:
                    problems.append(f"{where}: no ending {rest}")
            else:
                problems.append(f"{where}: unknown act {act}")

        stage_links = {sid: set() for sid in stages}
        ends_hit = set()
        for s in q["stages"]:
            sid = s["id"]
            where = f"{qid}/{sid}"
            for a in s.get("sim", []):
                note_act(a, where)
            if "nodes" not in s:
                if "then" not in s:
                    problems.append(f"{where}: no 'then'")
                    continue
                note_act(s["then"], where)
                kind, target = s["then"].split(":")
                (stage_links[sid] if kind == "stage" else ends_hit).add(target)
                continue
            nodes = s["nodes"]
            if s.get("with") not in people and s["kind"] == "talk":
                problems.append(f"{where}: talk stage with unknown {s.get('with')}")
            entry = [s["start"]] + [alt["node"] for alt in s.get("starts", [])]
            for alt in s.get("starts", []):
                for n in alt["need"]:
                    note_need(n, where)
            for nid in entry:
                if nid not in nodes:
                    problems.append(f"{where}: start node {nid} missing")
            seen, todo, exits = set(), [n for n in entry if n in nodes], 0
            while todo:
                nid = todo.pop()
                if nid in seen:
                    continue
                seen.add(nid)
                node = nodes[nid]
                nwhere = f"{where}/{nid}"
                if node["who"] != "scene":
                    if node["who"] not in people:
                        problems.append(f"{nwhere}: unknown speaker {node['who']}")
                    if node.get("mood") not in MOODS:
                        problems.append(f"{nwhere}: mood {node.get('mood')}")
                if not node.get("say"):
                    problems.append(f"{nwhere}: nothing said")
                if not node.get("options"):
                    problems.append(f"{nwhere}: no replies")
                if len(node.get("options", [])) > 9:
                    problems.append(f"{nwhere}: more than 9 replies")
                for o in node.get("options", []):
                    for n in o.get("need", []):
                        note_need(n, nwhere)
                    for a in o.get("act", []):
                        note_act(a, nwhere)
                        kind, _, rest = a.partition(":")
                        if kind == "stage":
                            stage_links[sid].add(rest)
                            exits += 1
                        elif kind == "end":
                            ends_hit.add(rest)
                            exits += 1
                    if o["next"] is not None:
                        if o["next"] not in nodes:
                            problems.append(f"{nwhere}: reply leads to missing node {o['next']}")
                        else:
                            todo.append(o["next"])
            for nid in nodes:
                if nid not in seen:
                    problems.append(f"{where}/{nid}: node can never be reached")
            if not exits:
                problems.append(f"{where}: the talk can never move the quest on")
        reach, todo = set(), [q["start"]]
        while todo:
            sid = todo.pop()
            if sid in reach or sid not in stages:
                continue
            reach.add(sid)
            todo.extend(stage_links[sid])
        for sid in stages:
            if sid not in reach:
                problems.append(f"{qid}/{sid}: stage can never be reached")
        for e in endings - ends_hit:
            problems.append(f"{qid}: ending {e} can never be reached")

    for q in quests:
        if q.get("next") and q["next"] not in quest_ids:
            problems.append(f"{q['id']}: next quest {q['next']} is unknown")
    for flag, where in flags_read.items():
        if flag not in flags_set:
            problems.append(f"{where}: reads flag {flag}, which nothing sets")
    for item, where in items_used.items():
        if item not in items_given:
            problems.append(f"{where}: uses item {item}, which nothing gives")
    for t in world["threads"]:
        if t["id"].startswith("done:"):
            note_need(t["id"], "world/threads")
        elif t["id"] not in flags_set:
            problems.append(f"world/threads: {t['id']} is not a flag")
    for group in world.get("exclusive", []):
        for flag in group:
            if flag not in flags_set:
                problems.append(f"world/exclusive: {flag} is not a flag")
    for d in world["decisions"]:
        if d["quest"] not in quest_ids:
            problems.append(f"world/decisions: unknown quest {d['quest']}")
    for c in world["careers_ahead"]:
        for who in c["shares"] + [c["lead"]]:
            if who not in people:
                problems.append(f"world/careers_ahead: unknown {who}")
    return problems, sorted(flags_set - set(flags_read))


def lift_regard(q, st):
    """Stand-in for shifts, rounds and gifts: lifts regard to the level a quest line asks for."""
    for need in q.get("needs", []):
        if need.startswith("rep:"):
            _, who, n = need.split(":")
            st["rep"][who] = max(st["rep"].get(who, 0), int(n))


def play_many(quests, world, runs=6000, seed=20261125):
    by_id = {q["id"]: q for q in quests}
    rng = random.Random(seed)
    problems, reached = set(), {q["id"]: {} for q in quests}
    retake = {(q["id"], e["id"]) for q in quests for e in q["endings"] if e.get("retake")}

    def run(qid, st):
        end = play(by_id[qid], st, rng, problems)
        if end:
            reached[qid][end] = reached[qid].get(end, 0) + 1
            for group in world.get("exclusive", []):
                both = [f for f in group if f in st["flags"]]
                if len(both) > 1:
                    problems.add(f"{qid}, ending {end}: {' and '.join(both)} are both set")
        return end

    def attempt(qid, st):
        """Plays a quest line, and takes it again, as the game would, while it ends on a retakeable ending."""
        for n in range(8):
            if n and qid == "question_mei":       # between two askings, some players go and get the house or the savings
                if rng.random() < 0.5:
                    st["home"] = "owned"
                if rng.random() < 0.5:
                    st["money"] += 15000
            lift_regard(by_id[qid], st)
            if not needs_ok(by_id[qid].get("needs"), st):
                return None
            end = run(qid, st)
            if end is None or (qid, end) not in retake:
                return end
        return None

    def court(st):
        for qid in COURTING:
            if qid == "question_mei":
                roll = rng.random()           # some players have the house, the savings or both before they ask
                if roll < 0.4:
                    st["home"] = "owned"
                if 0.25 < roll < 0.6:
                    st["money"] += 15000
            if attempt(qid, st) is None:
                return

    for _ in range(runs):
        st = new_state()
        run("new_in_town", st)
        if rng.random() < 0.8:
            run("errand_runner", st)
            if rng.random() < 0.7:
                run("own_stall", st)
        if rng.random() < 0.7:
            run("night_gardener", st)
        court_after = rng.randrange(len(BAKER))        # courting can begin after any quest of the career
        for n, qid in enumerate(BAKER):
            if not needs_ok(by_id[qid].get("needs"), st) or attempt(qid, st) is None:
                break
            if n == court_after:
                court(st)
    for q in quests:
        for e in q["endings"]:
            if e["id"] not in reached[q["id"]]:
                problems.add(f"{q['id']}: no play-through reached ending {e['id']}")
    return sorted(problems), reached


def count(quests):
    stages = nodes = replies = endings = words = 0
    for q in quests:
        endings += len(q["endings"])
        for s in q["stages"]:
            stages += 1
            for n in s.get("nodes", {}).values():
                nodes += 1
                words += len(n["say"].split())
                replies += len(n["options"])
                words += sum(len(o["text"].split()) for o in n["options"])
    return stages, nodes, replies, endings, words


def main():
    cast, world, quests = load()
    problems, unread = check(cast, world, quests)
    sim_problems, reached = play_many(quests, world) if not problems else ([], {})
    baker = [q for q in quests if q["group"] == "baker"]
    romance = [q for q in quests if q["group"] == "romance"]
    s, n, r, e, w = count(baker + romance)
    stats = {"quests": len(baker), "courting_quests": len(romance), "stages": s, "lines": n, "replies": r, "endings": e, "words": w,
             "new_people": sum(1 for p in cast["people"] if p["status"] == "new"), "play_throughs": 6000}
    for name, group in (("Baker career", baker), ("Courting", romance)):
        gs, gn, gr, ge, gw = count(group)
        print(f"{name}: {len(group)} quests, {gs} stages, {gn} lines, {gr} replies, {ge} endings, about {gw} words of dialogue")
    for qid, ends in reached.items():
        print(f"  {qid}: " + ", ".join(f"{k} {v}" for k, v in sorted(ends.items())))
    if unread:
        print("Flags set but not read by any reply (colour only): " + ", ".join(unread))
    for p in problems + sim_problems:
        print("PROBLEM", p)
    if problems or sim_problems:
        sys.exit(1)
    print("No problems.")
    if "--check" in sys.argv:
        return
    data = json.dumps({"cast": cast, "world": world, "quests": quests, "stats": stats}, ensure_ascii=False)
    data = data.replace("</", "<\\/")
    head = (SRC / "head.html").read_text(encoding="utf-8")
    app = "\n".join((SRC / name).read_text(encoding="utf-8") for name in ("app.js", "app3.js", "app2.js"))
    page = f"{head}\n<script>\nwindow.ATLAS = {data};\n</script>\n<script>\n{app}\n</script>\n"
    shell = '<!doctype html>\n<html lang="en">\n<head>\n<meta charset="utf-8">\n<meta name="viewport" content="width=device-width, initial-scale=1">\n'
    title, _, rest = page.partition("</style>")
    OUT.write_text(f"{shell}{title}</style>\n</head>\n<body>{rest}</body>\n</html>\n", encoding="utf-8")
    print(f"Wrote {OUT.name} ({OUT.stat().st_size // 1024} KB)")
    if "--fragment" in sys.argv:
        # the same page without the document shell, for publishing where the host supplies one
        target = Path(sys.argv[sys.argv.index("--fragment") + 1])
        target.write_text(page, encoding="utf-8")
        print(f"Wrote {target}")


if __name__ == "__main__":
    main()
