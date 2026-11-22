"""Quest lines, daily jobs and reputation in the first slice (QST-001 to QST-008, ADR-0012).

Quest lines are data (World/rules/quests.json). problems() checks them; Quests runs them against an economy.Economy, so
every reward goes through the ledger (QST-004). Which resident offers which job on which day is worked out from the
day and the data (QST-006); only a taken job and the day's finished offers are stored. The browser slice runs a port
(Prototype/play/quests.js).

Standard library only.
"""
import math
import re

from economy import Refused, hash01

GOALS = ("talk", "go", "near", "have", "bank", "money", "stall", "stall_sold")
REPLY_KEYS = {"text", "next", "if", "do", "act", "need"}
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

    def need_ok(where, need):
        for x in need:
            k, _, v = x.lstrip("!").partition(":")
            if k == "have" and v in items:
                continue
            if k == "money" and not x.startswith("!") and v.isdigit():
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
        if k == "rep" and re.fullmatch(r"-?\d+", v):
            return
        out.append("%s: unknown act %s" % (where, a))

    def talk(where, conv, qid, offer):
        """A conversation in node form: replies valid, every node reachable, every node able to end. Returns its acts."""
        nodes, acts = conv.get("nodes", {}), []
        if conv.get("start") not in nodes:
            out.append("%s: start node %s does not exist" % (where, conv.get("start")))
            return acts
        ends = set()
        for nid, nd in nodes.items():
            at = "%s.%s" % (where, nid)
            if nd.get("mood") not in moods:
                out.append("%s: unknown mood %s" % (at, nd.get("mood")))
            text_ok(at, nd.get("say", ""))
            if not 1 <= len(nd.get("options", [])) <= 9:
                out.append("%s: needs 1 to 9 replies" % at)
            if not any(not o.get("if") and not o.get("need") for o in nd.get("options", [])):
                out.append("%s: every reply has a condition, so the player could be stuck" % at)
            for o in nd.get("options", []):
                if set(o) - REPLY_KEYS:
                    out.append("%s: unknown keys on a reply %s" % (at, sorted(set(o) - REPLY_KEYS)))
                text_ok(at, o.get("text", ""))
                need_ok(at, o.get("need", []))
                for a in o.get("act", []):
                    act_ok(at, a, qid, offer)
                    acts.append(a)
                if o.get("next") is None:
                    ends.add(nid)
                elif o["next"] not in nodes:
                    out.append("%s: reply '%s' goes to %s, which does not exist" % (at, o.get("text"), o["next"]))
        seen, todo = {conv["start"]}, [conv["start"]]
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
        if (g is None) != ("offer" not in q):
            out.append("%s: a quest with a giver needs an offer, and only then" % where)
        for n in q.get("needs", []):
            if n not in quests or n == qid:
                out.append("%s: needs unknown quest %s" % (where, n))
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

    def start(self, qid):
        q = self.Q["quests"][qid]
        if qid in self.active or qid in self.done:
            raise Refused("already taken")
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
        return [qid for qid, q in self.Q["quests"].items() if q.get("giver") == rid and qid not in self.active and qid not in self.done
                and all(self.met(n) for n in q.get("needs", []))]

    def talking_to(self, rid):
        """Active quests whose current stage is a talk with this resident."""
        return [qid for qid in self.active if self.stage(qid)["goal"].get("talk") == rid]

    def need_ok(self, need):
        for x in need:
            neg, x = x.startswith("!"), x.lstrip("!")
            k, _, v = x.partition(":")
            ok = self.E.inv.get(v, 0) > 0 if k == "have" else self.E.total_money() >= int(v)
            if ok == neg:
                return False
        return True

    def act(self, qid, acts, rid):
        """A reply's acts, in order, said to resident rid."""
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
                self.rep_add(rid, int(v))

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
        return False

    def check(self, ctx):
        """Goals met by where the player is and what they have. ctx: {lot: lot id or None, pos: (x, z), minute}."""
        moved = True
        while moved:
            moved = False
            for qid in list(self.active):
                st = self.stage(qid)
                if "talk" in st["goal"] or not self.goal_met(qid, ctx):
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
        return dict(active=self.active, done=self.done, log=self.log, job=self.job, jobs_done=self.jobs_done)

    def load(self, s):
        for k, v in s.items():
            setattr(self, k, v)
