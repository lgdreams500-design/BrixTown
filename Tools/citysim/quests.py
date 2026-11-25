"""Quest lines, daily jobs and reputation in the first slice (QST-001 to QST-008, ADR-0012), and the story lines of
a career (QST-010 to QST-015, ADR-0056): what the player did is remembered as facts, replies and whole talks depend on
them, several people speak in one talk, and some endings let a line be taken again.

Quest lines are data (World/rules/quests.json). problems() checks them; Quests runs them against an economy.Economy, so
every reward goes through the ledger (QST-004). Which resident offers which job on which day is worked out from the
day and the data (QST-006); only a taken job and the day's finished offers are stored. The browser slice runs a port
(Prototype/play/quests.js).

Standard library only.
"""
import math
import re

from economy import Refused, hash01

GOALS = ("talk", "go", "near", "have", "bank", "money", "stall", "stall_sold", "home")
REPLY_KEYS = {"text", "next", "if", "do", "act", "need"}
NODE_KEYS = {"say", "mood", "options", "who", "dir", "covered"}
SCENE = "scene"                 # a node nobody says: what happens, told to the player (QST-012)
VAR = re.compile(r"\{([^{}]*)\}")


def centre(lot):
    r = lot["rect"]
    return ((r[0] + r[2]) / 2, (r[1] + r[3]) / 2)


def in_window(minute, a, b):
    """minute of the day inside [a, b), where the window may run past midnight (a > b)."""
    return a <= minute < b if a <= b else minute >= a or minute < b


# ---------------------------------------------------------------- checking the data (QST-001)
def problems(Q, econ, resident_ids, lot_ids, roles, moods, dialogue_vars):
    out = []
    quests, items = Q["quests"], econ["items"]
    names = set(dialogue_vars) | set(Q["vars"])

    def text_ok(where, text):
        for v in VAR.findall(text):
            if v not in names:
                out.append("%s: unknown {%s}" % (where, v))

    flags_set, flags_read = set(), {}

    def need_ok(where, need):
        for x in need:
            k, _, v = x.lstrip("!").partition(":")
            if k == "have" and v in items:
                continue
            if k == "money" and v.isdigit():
                continue
            if k == "flag" and v:                           # a fact the player's replies or an ending left behind (QST-010)
                flags_read.setdefault(v, where)
                continue
            if k == "rep":
                who, _, n = v.partition(":")
                if who in resident_ids and re.fullmatch(r"-?\d+", n):
                    continue
            if k == "done":
                qn, _, ends = v.partition(":")
                if qn in quests and ends and (ends == "*" or all(e in quests[qn].get("endings", {}) for e in ends.split("|"))):
                    continue
            if k == "home" and v == "owned":
                continue
            if k == "again" and v in quests:
                continue
            out.append("%s: unknown need %s" % (where, x))

    def act_ok(where, a, qid, offer):
        k, _, v = a.partition(":")
        q = quests[qid]
        if k == "start" and offer and not v:
            return
        if k == "stage" and v in q["stages"] and not offer:
            return
        if k == "end" and v in q["endings"] and not offer:
            return
        if k in ("give", "take") and v in items:
            return
        if k == "rep" and re.fullmatch(r"[+-]?\d+", v):
            return
        if k == "rep":                                       # with somebody named, who need not be in the talk
            who, _, n = v.partition(":")
            if who in resident_ids and re.fullmatch(r"[+-]?\d+", n):
                return
        if k == "flag" and v:
            flags_set.add(v)
            return
        if k == "unflag" and v:
            flags_read.setdefault(v, where)
            return
        if k in ("pay", "charge") and v.isdigit() and int(v) > 0:
            return
        out.append("%s: unknown act %s" % (where, a))

    def always_a_reply(replies):
        """True when at least one reply is open whatever is true: tried over every mix of the facts the replies ask
        about, leaving out the mixes the data rules out (flags of one exclusive group, two endings of one quest)."""
        if any(not o.get("if") and not o.get("need") for o in replies):
            return True
        if any(o.get("if") for o in replies):
            return False
        atoms = sorted({x.lstrip("!") for o in replies for x in o.get("need", [])})
        if len(atoms) > 12:
            return False
        groups = [set("flag:" + f for f in g) for g in Q.get("exclusive", [])]
        for bits in range(1 << len(atoms)):
            true = {a for n, a in enumerate(atoms) if bits >> n & 1}
            if any(len(true & g) > 1 for g in groups):
                continue
            done = {}
            for a in true:
                if a.startswith("done:"):
                    qn, _, ends = a[5:].partition(":")
                    done.setdefault(qn, []).append(None if ends == "*" else set(ends.split("|")))
            bad = False
            for qn, sets in done.items():                   # the quest ended one way: the endings asked for must overlap
                real = [x for x in sets if x is not None]
                if real and not set.intersection(*real):
                    bad = True
            for a in atoms:                                 # ended some way means ended at all
                if a.startswith("done:") and a.endswith(":*") and a not in true and any(t.startswith(a[:-1]) for t in true):
                    bad = True
            if bad:
                continue
            if not any(all((x.lstrip("!") in true) != x.startswith("!") for x in o.get("need", [])) for o in replies):
                return False
        return True

    def talk(where, conv, qid, offer):
        """A conversation in node form: replies valid, every node reachable, every node able to end. Returns its acts."""
        nodes, acts = conv.get("nodes", {}), []
        if conv.get("start") not in nodes:
            out.append("%s: start node %s does not exist" % (where, conv.get("start")))
            return acts
        entries = [conv["start"]]
        for alt in conv.get("starts", []):                  # other first lines, the first whose needs hold (QST-011)
            need_ok(where + ".starts", alt.get("need", []))
            if not alt.get("need"):
                out.append("%s: a second first line needs a condition" % where)
            if alt.get("node") not in nodes:
                out.append("%s: start node %s does not exist" % (where, alt.get("node")))
            else:
                entries.append(alt["node"])
        ends = set()
        for nid, nd in nodes.items():
            at = "%s.%s" % (where, nid)
            if set(nd) - NODE_KEYS:
                out.append("%s: unknown keys %s" % (at, sorted(set(nd) - NODE_KEYS)))
            who = nd.get("who")
            if who is not None and who != SCENE and who not in resident_ids:
                out.append("%s: no resident %s" % (at, who))
            if who != SCENE and nd.get("mood") not in moods:
                out.append("%s: unknown mood %s" % (at, nd.get("mood")))
            if not nd.get("say"):
                out.append("%s: nothing is said" % at)
            text_ok(at, nd.get("say", ""))
            if not 1 <= len(nd.get("options", [])) <= 9:
                out.append("%s: needs 1 to 9 replies" % at)
            if not nd.get("covered") and not always_a_reply(nd.get("options", [])):
                out.append("%s: every reply has a condition, so the player could be stuck" % at)
            for o in nd.get("options", []):
                if set(o) - REPLY_KEYS:
                    out.append("%s: unknown keys on a reply %s" % (at, sorted(set(o) - REPLY_KEYS)))
                text_ok(at, o.get("text", ""))
                need_ok(at, o.get("need", []))
                for a in o.get("act", []):
                    act_ok(at, a, qid, offer)
                    acts.append(a)
                    if a.startswith("charge:") and "money:" + a[7:] not in o.get("need", []):
                        out.append("%s: a reply that costs %s must need that money" % (at, a[7:]))
                if o.get("next") is None:
                    ends.add(nid)
                elif o["next"] not in nodes:
                    out.append("%s: reply '%s' goes to %s, which does not exist" % (at, o.get("text"), o["next"]))
        seen, todo = set(entries), list(entries)
        while todo:
            for o in nodes[todo.pop()].get("options", []):
                if o.get("next") in nodes and o["next"] not in seen:
                    seen.add(o["next"])
                    todo.append(o["next"])
        out.extend("%s.%s: cannot be reached from the start" % (where, nid) for nid in nodes if nid not in seen)
        grew = True
        while grew:
            grew = False
            for nid, nd in nodes.items():
                if nid not in ends and any(o.get("next") in ends for o in nd.get("options", [])):
                    ends.add(nid)
                    grew = True
        out.extend("%s.%s: the talk can never end from here" % (where, nid) for nid in nodes if nid not in ends)
        return acts

    if Q.get("first") not in quests:
        out.append("quests: the first quest %s does not exist" % Q.get("first"))
    elif quests[Q["first"]].get("giver") is not None:
        out.append("quests: the first quest starts with a new game, so it has no giver")
    for qid, q in quests.items():
        where = "quest %s" % qid
        for k in ("title", "summary", "start", "stages", "endings"):
            if k not in q:
                out.append("%s: missing %s" % (where, k))
        if any(k not in q for k in ("start", "stages", "endings")):
            continue
        g = q.get("giver")
        if g is not None and g not in resident_ids:
            out.append("%s: no resident %s" % (where, g))
        opens = g is not None and "offer" not in q and q["stages"].get(q["start"], {}).get("goal", {}).get("talk") == g
        if (g is None) != ("offer" not in q) and not opens:  # a story line opens by talking to its giver (QST-011)
            out.append("%s: a quest with a giver needs an offer, and only then" % where)
        for n in q.get("needs", []):
            if n not in quests or n == qid:
                out.append("%s: needs unknown quest %s" % (where, n))
        need_ok(where, q.get("need", []))
        if "offer" in q:
            text_ok(where + ".offer", q["offer"].get("line", ""))
            acts = talk(where + ".offer", q["offer"], qid, True)
            if "start" not in acts:
                out.append("%s.offer: no reply starts the quest" % where)
        if q["start"] not in q["stages"]:
            out.append("%s: start stage %s does not exist" % (where, q["start"]))
            continue
        if q.get("starter") and len(q["endings"]) < 2:
            out.append("%s: a starter quest needs at least two endings (QST-003)" % where)
        nxt = {}                                         # stage -> stages and endings it can lead to
        for sid, st in q["stages"].items():
            at = "%s.%s" % (where, sid)
            text_ok(at, st.get("log", ""))
            goal = st.get("goal", {})
            if len(goal) == 0 or not set(goal) & set(GOALS):
                out.append("%s: needs one goal of %s" % (at, ", ".join(GOALS)))
                continue
            kind = next(k for k in GOALS if k in goal)
            if kind == "talk":
                if goal["talk"] not in resident_ids:
                    out.append("%s: no resident %s" % (at, goal["talk"]))
                acts = talk(at, st, qid, False)
                targets = [a for a in acts if a.startswith(("stage:", "end:"))]
                if not targets:
                    out.append("%s: nothing said here moves the quest on" % at)
            elif "nodes" in st:                              # a scene: it plays when the player is there (QST-012)
                if kind not in ("go", "near"):
                    out.append("%s: a scene needs a place (go or near)" % at)
                if kind == "go" and goal["go"] not in lot_ids:
                    out.append("%s: no lot %s" % (at, goal["go"]))
                if kind == "near" and not (len(goal["near"]) == 2 and goal.get("r", 0) > 0):
                    out.append("%s: near needs a point and a radius" % at)
                if any(nd.get("who") is None for nd in st["nodes"].values()):
                    out.append("%s: every line of a scene names who says it" % at)
                acts = talk(at, st, qid, False)
                targets = [a for a in acts if a.startswith(("stage:", "end:"))]
                if not targets:
                    out.append("%s: nothing said here moves the quest on" % at)
                if any(re.fullmatch(r"rep:[+-]?\d+", a) for a in acts):
                    out.append("%s: nobody is spoken to in a scene, so reputation must name who it is with" % at)
            else:
                if kind == "go" and goal["go"] not in lot_ids:
                    out.append("%s: no lot %s" % (at, goal["go"]))
                if kind == "near" and not (len(goal["near"]) == 2 and goal.get("r", 0) > 0):
                    out.append("%s: near needs a point and a radius" % at)
                if kind == "have" and goal["have"] not in items:
                    out.append("%s: no item %s" % (at, goal["have"]))
                if kind == "bank" and goal["bank"] != "open":
                    out.append("%s: bank goal must be open" % at)
                if kind == "stall" and goal["stall"] != "rented":
                    out.append("%s: stall goal must be rented" % at)
                if kind == "home" and goal["home"] != "owned":
                    out.append("%s: home goal must be owned" % at)
                if kind in ("money", "stall_sold") and not (type(goal[kind]) is int and goal[kind] > 0):
                    out.append("%s: %s must be a whole number above zero" % (at, kind))
                for k in ("from", "to"):
                    if k in goal and not (type(goal[k]) is int and 0 <= goal[k] <= 1440):
                        out.append("%s: %s must be a minute of the day" % (at, k))
                t = st.get("then", "")
                k2, _, v = t.partition(":")
                if not ((k2 == "stage" and v in q["stages"]) or (k2 == "end" and v in q["endings"])):
                    out.append("%s: then %s is not a stage or ending" % (at, t))
                targets = [t]
            for k in ("give", "take"):
                for it in st.get("on_done", {}).get(k, []):
                    if it not in items:
                        out.append("%s: no item %s" % (at, it))
            nxt[sid] = {t.split(":", 1)[1] if t.startswith("stage:") else "end:" + t.split(":", 1)[1] for t in targets}
        seen, todo = {q["start"]}, [q["start"]]
        while todo:
            for t in nxt.get(todo.pop(), ()):
                if t not in seen:
                    seen.add(t)
                    if not t.startswith("end:"):
                        todo.append(t)
        out.extend("%s.%s: cannot be reached" % (where, s) for s in q["stages"] if s not in seen)
        out.extend("%s: ending %s cannot be reached" % (where, e) for e in q["endings"] if "end:" + e not in seen)
        for eid, e in q["endings"].items():
            at = "%s ending %s" % (where, eid)
            text_ok(at, e.get("log", ""))
            for k in ("pay", "charge"):
                if not (type(e.get(k, 0)) is int and e.get(k, 0) >= 0):
                    out.append("%s: %s must be whole chips" % (at, k))
            for r, v in e.get("rep", {}).items():
                if r not in resident_ids or type(v) is not int:
                    out.append("%s: bad reputation change %s %s" % (at, r, v))
            for it in e.get("give", []) + e.get("take", []):
                if it not in items:
                    out.append("%s: no item %s" % (at, it))
            if not all(type(f) is str and f for f in e.get("flags", [])) or type(e.get("retake", False)) is not bool:
                out.append("%s: flags are names and retake is true or false" % at)
            flags_set.update(e.get("flags", []))
    for f, where in sorted(flags_read.items()):
        if f not in flags_set:
            out.append("%s: asks about %s, which nothing ever sets" % (where, f))
    for g in Q.get("exclusive", []):
        for f in g:
            if f not in flags_set:
                out.append("quests: exclusive names %s, which nothing ever sets" % f)
    starters = [q for q in quests.values() if q.get("starter")]
    if len(starters) < 4:
        out.append("quests: at least four starter quest lines (QST-002), found %d" % len(starters))
    J = Q.get("jobs", {})
    sold = {i for s in econ["shops"].values() for i in s["sells"]}
    for kid, k in J.get("kinds", {}).items():
        at = "job %s" % kid
        for r in k.get("roles", []):
            if r not in roles:
                out.append("%s: unknown role %s" % (at, r))
        if not (type(k.get("chance_bp")) is int and 0 <= k["chance_bp"] <= 10000):
            out.append("%s: chance_bp must be 0 to 10000" % at)
        for n in k.get("needs", []) + list(k.get("role_needs", {}).values()):
            qn, _, en = n.partition(":")
            if qn not in quests or (en and en not in quests[qn]["endings"]):
                out.append("%s: needs unknown quest or ending %s" % (at, n))
        for it in k.get("items", []):
            if it not in sold:
                out.append("%s: no shop sells %s" % (at, it))
        for key in ("pay", "hours", "rep", "fail_rep") + (("pay_per_100m",) if kid == "parcel" else ()) + (("minutes",) if kid == "shift" else ()):
            if type(k.get(key)) is not int:
                out.append("%s: %s must be a whole number" % (at, key))
        text_ok(at, k.get("offer", ""))
        text_ok(at, k.get("log", ""))
    for kid in ("parcel", "shopping", "shift"):
        if kid not in J.get("kinds", {}):
            out.append("jobs: missing the %s job" % kid)
    for key in ("ask", "accept", "decline", "none", "busy", "hand_over", "thanks"):
        if key not in J.get("lines", {}):
            out.append("jobs: missing line %s" % key)
    return out


# ---------------------------------------------------------------- running quests
class Quests:
    def __init__(self, rules, econ, residents, lots):
        """residents: [{id, name, role, home, work}] in packed order (home and work are lot ids or None); lots as in Economy."""
        self.Q = rules
        self.E = econ
        self.residents = residents
        self.rid_index = {r["id"]: n for n, r in enumerate(residents)}
        self.lots = lots
        self.active = {}            # quest id -> {stage, since}
        self.done = {}              # quest id -> ending id
        self.log = []               # (day, minute, text)
        self.job = None
        self.jobs_done = []         # "resident index|day" offers already taken today
        self.flags = []             # what the player did, as facts replies ask about (QST-010); kept sorted
        self.again = []             # quest lines taken a second time (QST-013)
        self.awarded = {}           # quest id -> {resident id: reputation its replies gave}, taken back on a retake
        self.owns_home = lambda: False   # set by whoever holds the property market (PROP-009)
        self.shops = [n for n, l in enumerate(lots) if l["kind"] == "shop"]
        self.homes = [n for n, l in enumerate(lots) if l["kind"] == "home"]

    def note(self, text):
        self.log.append((self.E.day, self.E.minute, text))

    # ---- reputation (QST-007)
    def rep(self, rid):
        return self.E.rep.get(rid, 0)

    def rep_add(self, rid, n):
        R = self.E.R["reputation"]
        self.E.rep[rid] = max(R["min"], min(R["max"], self.rep(rid) + n))

    # ---- quest lines
    def met(self, need):
        q, _, e = need.partition(":")
        return q in self.done and (not e or self.done[q] == e)

    def new_game(self):
        self.E.new_character()
        self.start(self.Q["first"])

    def retakes(self, qid):
        """True when the line ended in a way that lets it be taken again (QST-013)."""
        e = self.done.get(qid)
        return e is not None and bool(self.Q["quests"][qid]["endings"][e].get("retake"))

    def start(self, qid):
        q = self.Q["quests"][qid]
        if qid in self.active or (qid in self.done and not self.retakes(qid)):
            raise Refused("already taken")
        if qid in self.done:                                # taken again: it starts clean (QST-013)
            if qid not in self.again:
                self.again.append(qid)
            for st in q["stages"].values():
                for nd in st.get("nodes", {}).values():
                    for o in nd["options"]:
                        for a in o.get("act", []):
                            k, _, v = a.partition(":")
                            if k == "flag" and v in self.flags:
                                self.flags.remove(v)
                            elif k == "give" and self.E.inv.get(v):
                                self.E.take_item(v, self.E.inv[v])
            for who, n in self.awarded.get(qid, {}).items():
                self.rep_add(who, -n)
        self.awarded[qid] = {}
        self.active[qid] = {}
        self.note("Started: " + q["title"])
        self.goto(qid, "stage:" + q["start"])

    def goto(self, qid, target):
        k, _, v = target.partition(":")
        if k == "end":
            return self.end(qid, v)
        self.active[qid] = dict(stage=v, since=self.E.stall_sold_total)

    def stage(self, qid):
        a = self.active.get(qid)
        return a and self.Q["quests"][qid]["stages"][a["stage"]]

    def offers(self, rid):
        """Quest lines this resident would offer the player now."""
        return [qid for qid, q in self.Q["quests"].items() if q.get("giver") == rid and qid not in self.active
                and (qid not in self.done or self.retakes(qid))
                and all(self.met(n) for n in q.get("needs", [])) and self.need_ok(q.get("need", []))]

    def opens(self, qid):
        """True for a line with no offer talk: it starts when the player speaks to its giver (QST-011)."""
        q = self.Q["quests"][qid]
        return q.get("giver") is not None and "offer" not in q

    def talking_to(self, rid):
        """Active quests whose current stage is a talk with this resident."""
        return [qid for qid in self.active if self.stage(qid)["goal"].get("talk") == rid]

    def scenes(self, ctx):
        """Active quests whose current stage is a scene at the place the player is in (QST-012)."""
        return [qid for qid in self.active if "nodes" in self.stage(qid) and "talk" not in self.stage(qid)["goal"] and self.goal_met(qid, ctx)]

    def first_line(self, qid):
        """The node a stage's talk opens on: the first of its 'starts' whose needs hold, else its start (QST-011)."""
        st = self.stage(qid)
        return next((a["node"] for a in st.get("starts", []) if self.need_ok(a["need"])), st["start"])

    def replies(self, qid, nid):
        """The replies of a node the player can give now, as indexes into its options."""
        return [n for n, o in enumerate(self.stage(qid)["nodes"][nid]["options"]) if self.need_ok(o.get("need", []))]

    def need_ok(self, need):
        for x in need:
            neg, x = x.startswith("!"), x.lstrip("!")
            k, _, v = x.partition(":")
            if k == "have":
                ok = self.E.inv.get(v, 0) > 0
            elif k == "money":
                ok = self.E.total_money() >= int(v)
            elif k == "flag":
                ok = v in self.flags
            elif k == "rep":
                who, _, n = v.partition(":")
                ok = self.rep(who) >= int(n)
            elif k == "done":
                qn, _, ends = v.partition(":")
                ok = qn in self.done and (ends == "*" or self.done[qn] in ends.split("|"))
            elif k == "home":
                ok = bool(self.owns_home())
            elif k == "again":
                ok = v in self.again
            else:
                raise ValueError("unknown need " + x)
            if ok == neg:
                return False
        return True

    def act(self, qid, acts, rid):
        """A reply's acts, in order, said to resident rid (None in a scene). Refused whole if it costs more than is held."""
        cost = sum(int(a[7:]) for a in acts if a.startswith("charge:"))
        if cost and self.E.L.balance("p:cash") + self.E.L.balance("p:current") < cost:
            raise Refused("not enough money")
        title = self.Q["quests"][qid]["title"]
        for a in acts:
            k, _, v = a.partition(":")
            if k == "start":
                self.start(qid)
            elif k in ("stage", "end"):
                self.goto(qid, a)
            elif k == "give":
                self.E.give_item(v)
            elif k == "take":
                self.E.take_item(v)
            elif k == "rep":
                who, _, n = v.rpartition(":")
                who = who or rid
                self.rep_add(who, int(n))
                gave = self.awarded.setdefault(qid, {})
                gave[who] = gave.get(who, 0) + int(n)
            elif k == "flag":
                if v not in self.flags:
                    self.flags.append(v)
                    self.flags.sort()
            elif k == "unflag":
                if v in self.flags:
                    self.flags.remove(v)
            elif k == "pay":                                # QST-004: through the ledger, from the rewards account
                self.E.L.post(self.E.day, self.E.minute, "Reward: " + title, [("sys:rewards", -int(v)), ("p:cash", int(v))])
            elif k == "charge":
                self.E.spend(int(v), "sys:residents", title)

    def end(self, qid, eid):
        q = self.Q["quests"][qid]
        e = q["endings"][eid]
        if e.get("charge"):
            self.E.spend(e["charge"], "sys:residents", q["title"])
        if e.get("pay"):                                    # QST-004: through the ledger, from the rewards account
            self.E.L.post(self.E.day, self.E.minute, "Reward: " + q["title"], [("sys:rewards", -e["pay"]), ("p:cash", e["pay"])])
        for it in e.get("give", []):
            self.E.give_item(it)
        for it in e.get("take", []):
            if self.E.inv.get(it):
                self.E.take_item(it)
        for r, n in e.get("rep", {}).items():
            self.rep_add(r, n)
        for f in e.get("flags", []):
            if f not in self.flags:
                self.flags.append(f)
        self.flags.sort()
        self.active.pop(qid, None)
        self.done[qid] = eid
        self.note("Finished: %s. %s" % (q["title"], e["log"]))

    def goal_met(self, qid, ctx):
        g, a = self.stage(qid)["goal"], self.active[qid]
        if "go" in g:
            return ctx.get("lot") == g["go"] and in_window(ctx["minute"], g.get("from", 0), g.get("to", 1440))
        if "near" in g:
            return ctx.get("pos") is not None and math.dist(ctx["pos"], g["near"]) <= g["r"] and in_window(ctx["minute"], g.get("from", 0), g.get("to", 1440))
        if "have" in g:
            return self.E.inv.get(g["have"], 0) > 0
        if "bank" in g:
            return "current" in self.E.accounts
        if "money" in g:
            return self.E.total_money() >= g["money"]
        if "stall" in g:
            return self.E.stall is not None
        if "stall_sold" in g:
            return self.E.stall_sold_total - a["since"] >= g["stall_sold"]
        if "home" in g:
            return bool(self.owns_home())
        return False

    def check(self, ctx):
        """Goals met by where the player is and what they have. ctx: {lot: lot id or None, pos: (x, z), minute}."""
        moved = True
        while moved:
            moved = False
            for qid in list(self.active):
                st = self.stage(qid)
                if "nodes" in st or not self.goal_met(qid, ctx):   # talks and scenes move on by what is said
                    continue
                for it in st.get("on_done", {}).get("give", []):
                    self.E.give_item(it)
                for it in st.get("on_done", {}).get("take", []):
                    self.E.take_item(it)
                self.goto(qid, st["then"])
                moved = True
        self.check_job(ctx)

    # ---- daily jobs (QST-006)
    def job_offer(self, i, day):
        """The job resident i offers on that day, or None. Worked out from (i, day) and the data; nothing stored."""
        J = self.Q["jobs"]
        r = self.residents[i]
        kid = next((k for k, v in J["kinds"].items() if r["role"] in v["roles"]), None)
        if kid is None:
            return None
        K = J["kinds"][kid]
        if hash01(i, day, "job") * 10000 >= K["chance_bp"]:
            return None
        rn = K.get("role_needs", {}).get(r["role"])
        if not all(self.met(n) for n in K.get("needs", [])) or (rn and not self.met(rn)):
            return None
        if self.rep(r["id"]) < J["min_rep"] or "%d|%d" % (i, day) in self.jobs_done:
            return None
        o = dict(kind=kid, giver=i, day=day, pay=K["pay"], hours=K["hours"])
        if kid == "parcel":
            frm = self.shops[int(hash01(i, day, "from") * len(self.shops))]
            to = self.homes[int(hash01(i, day, "to") * len(self.homes))]
            o.update(frm=frm, to=to, pay=K["pay"] + K["pay_per_100m"] * int(math.dist(centre(self.lots[frm]), centre(self.lots[to]))) // 100)
        elif kid == "shopping":
            o.update(item=K["items"][int(hash01(i, day, "item") * len(K["items"]))])
        elif kid == "shift":
            if not r.get("work"):
                return None
            o.update(to=self.E.lot_index[r["work"]], minutes=K["minutes"])
        return o

    def take_job(self, o):
        if self.job:
            raise Refused("one job at a time")
        now = self.E.day * 1440 + self.E.minute
        self.job = dict(o, deadline=now + o["hours"] * 60, step="pickup" if o["kind"] == "parcel" else "work", worked=0, last=None)
        self.jobs_done.append("%d|%d" % (o["giver"], o["day"]))
        self.note("Job taken: " + self.Q["jobs"]["kinds"][o["kind"]]["title"])

    def finish_job(self, extra=0):
        j, K = self.job, self.Q["jobs"]["kinds"][self.job["kind"]]
        if extra:                                           # paying back what the player spent (shopping), from residents' money
            self.E.L.post(self.E.day, self.E.minute, "Paid back", [("sys:residents", -extra), ("p:cash", extra)])
        self.E.L.post(self.E.day, self.E.minute, "Job: " + K["title"], [("sys:rewards", -j["pay"]), ("p:cash", j["pay"])])
        self.rep_add(self.residents[j["giver"]]["id"], K["rep"])
        self.note("Job done: %s" % K["title"])
        self.job = None

    def fail_job(self):
        j, K = self.job, self.Q["jobs"]["kinds"][self.job["kind"]]
        if j["kind"] == "parcel" and j["step"] == "deliver" and self.E.inv.get("parcel"):
            self.E.take_item("parcel")
        self.rep_add(self.residents[j["giver"]]["id"], K["fail_rep"])
        self.note("Job failed: %s" % K["title"])
        self.job = None

    def hand_over(self):
        """Shopping: give the giver their item (said in conversation with them)."""
        j = self.job
        if not j or j["kind"] != "shopping" or not self.E.inv.get(j["item"]):
            raise Refused("nothing to hand over")
        self.E.take_item(j["item"])
        self.finish_job(self.E.items[j["item"]]["price"])

    def check_job(self, ctx):
        j = self.job
        if not j:
            return
        now = self.E.day * 1440 + self.E.minute
        if now > j["deadline"]:
            return self.fail_job()
        lot = ctx.get("lot")
        if j["kind"] == "parcel":
            if j["step"] == "pickup" and lot == self.lots[j["frm"]]["id"]:
                self.E.give_item("parcel")
                j["step"] = "deliver"
            elif j["step"] == "deliver" and lot == self.lots[j["to"]]["id"] and self.E.inv.get("parcel"):
                self.E.take_item("parcel")
                self.finish_job()
        elif j["kind"] == "shift":
            inside = lot == self.lots[j["to"]]["id"]
            if inside and j["last"] is not None:
                j["worked"] += now - j["last"]
            j["last"] = now if inside else None
            if j["worked"] >= j["minutes"]:
                self.finish_job()

    def save(self):
        return dict(active=self.active, done=self.done, log=self.log, job=self.job, jobs_done=self.jobs_done,
                    flags=self.flags, again=self.again, awarded=self.awarded)

    def load(self, s):
        for k, v in s.items():
            setattr(self, k, v)
