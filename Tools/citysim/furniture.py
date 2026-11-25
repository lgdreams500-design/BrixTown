"""Buying furniture at the furniture outlet (docs/specs/domains/FURNITURE.md FURN-022 to FURN-029; F-FURN-STORE level 1).

The rules, proved here before they are built in the slice and the engine (FEATURE_PROCESS stage 5). Data:
World/rules/furniture.json (R) and the list of pieces, World/rules/furnishing.json (F).

    store = furniture.Store(R, F, econ, holds, route_m)
    store.tag(kind, line)                 # FURN-022: what a piece's tag says
    store.checkout(basket)                # FURN-025, FURN-026: pay at the till, each piece instant or booked for delivery
    store.van(order, day, minute)         # FURN-027: where that delivery's van is, worked out from the clock
    store.advance(day, minute)            # FURN-028: what has been carried in by now is in the unit (or with the belongings)
    store.assemble(pid, day, minute)      # FURN-029: the player puts a flat-packed piece together; store.fitter(...) pays for it
    store.settle(day)                     # FURN-024: at a day's end the company pays its staff and restocks from its takings

`econ` is the player's Economy (economy.py): the clock, and spend(), one ledger transaction from cash and then the current
account, refused whole if both fall short. `holds(unit)` says whether the player owns or rents a unit now (estate.py);
`route_m(unit)` is how far it is by road from the outlet's delivery bay to that unit's building.

Nothing is ticked: a delivery is its booking, and where the van is, what has been carried in and whether a piece is put
together are worked out from the booking and the clock (TECH-004). A piece the player owns is in exactly one place: with
the belongings (`owned`), in a unit where it was set down (`arrived[unit]`, for the home to place on its floor with
furnish.py), or on its way (in an order, not yet carried in).
"""
import math

from economy import Refused, fmt


def _vol(P):
    w, d, h = P["size"]
    return w * d * h


def problems(R, F):
    """What is wrong with the rules, in words; [] when they can be used (ECO-012: checked before use)."""
    out = []
    c = R.get("company", {})
    for k in ("id", "name", "account"):
        if not c.get(k):
            out.append("furniture: the company has no %s" % k)
    if not str(c.get("account", "")).startswith("co:"):
        out.append("furniture: the company's account is not a company's (co:)")
    for path, v in (("company.float", c.get("float")), ("price_level_bp", R.get("price_level_bp")), ("instant_fee", R.get("instant_fee")),
                    ("assembly.fee_bp", R.get("assembly", {}).get("fee_bp")), ("assembly.least_fee", R.get("assembly", {}).get("least_fee")),
                    ("company.wages_bp", c.get("wages_bp"))):
        if type(v) is not int or v < 0:
            out.append("furniture: %s is not a whole number of chips or basis points" % path)
    if type(c.get("wages_bp")) is int and c["wages_bp"] > 10000:
        out.append("furniture: the company cannot pay more than all its takings in wages")
    D = R.get("delivery", {})
    bands = D.get("fee_by_m3", [])
    if not bands or bands[-1][0] is not None:
        out.append("furniture: the delivery fees do not end with a fee for any size")
    for k, (upto, fee) in enumerate(bands):
        if type(fee) is not int or fee < 0:
            out.append("furniture: delivery fee %d is not whole chips" % k)
        if k and upto is not None and bands[k - 1][0] is not None and upto <= bands[k - 1][0]:
            out.append("furniture: the delivery fees' sizes do not rise")
        if k and fee < bands[k - 1][1]:
            out.append("furniture: a bigger delivery costs less than a smaller one")
    slots = D.get("slots", [])
    if not slots:
        out.append("furniture: there are no delivery slots")
    for k, (a, b) in enumerate(slots):
        if not (0 <= a < b <= 1440):
            out.append("furniture: slot %d is not within a day" % k)
        if k and a < slots[k - 1][1]:
            out.append("furniture: slots %d and %d overlap" % (k - 1, k))
    if D.get("first_day_after", 0) < 1:
        out.append("furniture: a delivery cannot be booked for the day it is bought")   # the slot might be past
    if D.get("days_ahead", 0) < D.get("first_day_after", 1):
        out.append("furniture: no day can be booked")
    if not D.get("van_m_per_min", 0) > 0 or not D.get("carry_min", 0) > 0:
        out.append("furniture: the van's speed and the time to carry a piece must be more than nothing")
    P = R.get("flat_pack", {})
    if not P.get("from_m3", 0) > 0 or len(P.get("box", [])) != 3 or min(P.get("box", [0])) <= 0 or P.get("most_boxes", 0) < 1 or not P.get("packs_to", 0) >= 1:
        out.append("furniture: the flat pack's box has no size")
    A = R.get("assembly", {})
    if not A.get("min_per_m3", 0) > 0 or not A.get("least_min", 0) > 0:
        out.append("furniture: putting a piece together takes no time")
    if not [k for k, p in F["pieces"].items() if not p.get("from")]:
        out.append("furniture: nothing is sold")
    return out


class Store:
    def __init__(self, R, F, econ, holds=None, route_m=None):
        bad = problems(R, F)
        if bad:
            raise ValueError("; ".join(bad))
        self.R, self.F, self.E = R, F, econ
        self.holds = holds or (lambda unit: True)
        self.route_m = route_m or (lambda unit: 600.0)
        self.acct = R["company"]["account"]
        self.n = 0                  # how many pieces have been sold: the next one's number
        self.owned = []             # bought and with the belongings: [{id, kind, line, name, w, d, h, rug, boxed, ready}]
        self.orders = []            # deliveries: {id, unit, day, slot, pieces, landed}
        self.arrived = {}           # unit -> the pieces set down there, each as the home will place it (a box until put together)
        self.notes = []             # what the player is told: [day, minute, words]
        self.has_catalogue = False  # FURN-023: the printed catalogue, handed out free at the outlet
        self.settled = -1           # the last day whose end the company's takings were paid out for

    # ---------------------------------------------------------------- the catalogue and the tags (FURN-022, FURN-023)
    def sells(self):
        return [k for k, p in self.F["pieces"].items() if not p.get("from")]

    def price(self, kind):
        return max(1, (self.F["pieces"][kind]["price"] * self.R["price_level_bp"] + 5000) // 10000)

    def flat_packed(self, kind):
        P = self.F["pieces"][kind]
        return not P.get("rug") and _vol(P) >= self.R["flat_pack"]["from_m3"]

    def boxes(self, kind):
        """How many flat-pack boxes a piece comes in, and the stack they make (width, depth, height)."""
        if not self.flat_packed(kind):
            return 0, None
        w, d, h = self.R["flat_pack"]["box"]
        n = min(self.R["flat_pack"]["most_boxes"], max(1, math.ceil(_vol(self.F["pieces"][kind]) / self.R["flat_pack"]["packs_to"] / (w * d * h) - 1e-9)))
        return n, [w, d, round(h * n, 2)]

    def tag(self, kind, line):
        """FURN-022: a piece's tag on the showroom floor."""
        if kind not in self.sells():
            raise Refused("the outlet does not sell that")
        if line not in self.F["lines"]:
            raise Refused("there is no such line")
        P = self.F["pieces"][kind]
        return dict(name=P["name"], price=self.price(kind), size=list(P["size"]), line=line, other_lines=[x for x in self.F["lines"] if x != line],
                    flat_packed=self.flat_packed(kind), boxes=self.boxes(kind)[0],
                    assembly_fee=self.assembly_fee(kind) if self.flat_packed(kind) else 0, assembly_min=self.assembly_min(kind) if self.flat_packed(kind) else 0)

    def take_catalogue(self):
        """FURN-023: the outlet hands out its printed catalogue, free; one is enough."""
        self.has_catalogue = True

    # ---------------------------------------------------------------- fees and times
    def delivery_fee(self, kinds):
        v = sum(_vol(self.F["pieces"][k]) for k in kinds)
        for upto, fee in self.R["delivery"]["fee_by_m3"]:
            if upto is None or v <= upto + 1e-9:
                return fee
        return self.R["delivery"]["fee_by_m3"][-1][1]

    def assembly_fee(self, kind):
        A = self.R["assembly"]
        return max(A["least_fee"], (self.price(kind) * A["fee_bp"] + 5000) // 10000)

    def assembly_min(self, kind):
        A = self.R["assembly"]
        return max(A["least_min"], int(math.floor(_vol(self.F["pieces"][kind]) * A["min_per_m3"] + .5)))

    def slots(self):
        """The slots that can be booked now: [day, slot number], soonest first."""
        D = self.R["delivery"]
        return [[d, k] for d in range(self.E.day + D["first_day_after"], self.E.day + D["days_ahead"] + 1) for k in range(len(D["slots"]))]

    # ---------------------------------------------------------------- the till (FURN-025, FURN-026)
    def quote(self, basket, at_home=False):
        """What a basket costs, line by line, and the deliveries it books; raises Refused when it cannot be bought.
        basket: [{"kind", "line", "how": "instant" | "delivery", "unit", "day", "slot", "assembled", "at": [x, z, yaw]}]."""
        if not basket:
            raise Refused("the basket is empty")
        if at_home and not self.has_catalogue:
            raise Refused("you have no catalogue to order from")
        lines, runs, sells, ok = [], {}, self.sells(), self.slots()
        for it in basket:
            kind, line = it.get("kind"), it.get("line")
            if kind not in sells:
                raise Refused("the outlet does not sell that")
            if line not in self.F["lines"]:
                raise Refused("there is no such line")
            name = self.F["pieces"][kind]["name"]
            lines.append([name, 1, self.price(kind)])
            how = it.get("how")
            if how == "instant":
                if at_home:
                    raise Refused("ordering from home books delivery only")     # FURN-023
                if self.R["instant_fee"]:
                    lines.append(["Take it now: " + name, 1, self.R["instant_fee"]])
            elif how == "delivery":
                unit = it.get("unit")
                if not unit or not self.holds(unit):
                    raise Refused("that is not a home of yours to deliver to")
                if [it.get("day"), it.get("slot")] not in ok:
                    raise Refused("that delivery slot cannot be booked")
                if it.get("assembled") and self.flat_packed(kind):
                    lines.append(["Put together: " + name, 1, self.assembly_fee(kind)])
                runs.setdefault((unit, it["day"], it["slot"]), []).append(it)
            else:
                raise Refused("each piece is taken now or delivered")
        for key in sorted(runs):                                                # one van, one fee, for all that goes to a home in a slot
            lines.append(["Delivery, day %d" % (key[1] + 1), len(runs[key]), self.delivery_fee([it["kind"] for it in runs[key]])])
        return sum(c for _, _, c in lines), lines, runs

    def _piece(self, kind, line):
        self.n += 1
        P = self.F["pieces"][kind]
        w, d, h = P["size"]
        return {"id": "f%d" % self.n, "kind": kind, "line": line, "name": P["name"], "w": w, "d": d, "h": h, "rug": bool(P.get("rug")), "boxed": False, "ready": None}

    def checkout(self, basket, at_home=False):
        """FURN-025: pay for the whole basket in one ledger transaction to the company's account, or nothing happens.
        FURN-026: each piece taken now is with the belongings at once; each one for delivery is booked."""
        total, lines, runs = self.quote(basket, at_home)
        self.E.spend(total, self.acct, "%s: %d piece%s" % (self.R["company"]["name"], len(basket), "" if len(basket) == 1 else "s"))
        got = []
        for it in basket:
            p = self._piece(it["kind"], it["line"])
            it["_p"] = p
            got.append(p["id"])
            if it["how"] == "instant":
                self.owned.append(p)
        for key in sorted(runs):
            unit, day, slot = key
            ps = []
            for it in runs[key]:
                p = it["_p"]
                p["boxed"] = self.flat_packed(p["kind"])
                p["fitted"] = bool(it.get("assembled")) and p["boxed"]             # the fitter comes with the van and puts it together in place
                if it.get("at"):
                    p["at"] = list(it["at"])                                      # where the player marked it to stand (FURN-028)
                ps.append(p)
            self.orders.append({"id": "o%d" % (len(self.orders) + 1), "unit": unit, "day": day, "slot": slot, "pieces": ps, "landed": 0})
        for it in basket:
            del it["_p"]
        return total, got

    # ---------------------------------------------------------------- the delivery (FURN-027, FURN-028)
    def _legs(self, order):
        """The run's times in minutes from the slot's start: when the van arrives, when each piece is set down (and, with a
        fitter, put together), when it leaves and when it is back."""
        D = self.R["delivery"]
        drive = self.route_m(order["unit"]) / D["van_m_per_min"]
        t, down = drive, []
        for p in order["pieces"]:
            t += D["carry_min"]
            if p.get("fitted"):
                t += self.assembly_min(p["kind"])
            down.append(t)
        return drive, down, t, t + drive

    def van(self, order, day, minute):
        """FURN-027: where the delivery's van is at a moment: {"phase": "booked" | "out" | "at the door" | "back" | "done",
        "f": how far through that phase (0 to 1), "landed": pieces set down so far}. Worked out, never stored."""
        t = (day - order["day"]) * 1440 + minute - self.R["delivery"]["slots"][order["slot"]][0]
        drive, down, leave, back = self._legs(order)
        landed = sum(1 for x in down if t >= x - 1e-9)
        if t < 0:
            return {"phase": "booked", "f": 0.0, "landed": 0}
        if t < drive:
            return {"phase": "out", "f": t / drive if drive else 1.0, "landed": 0}
        if t < leave:
            return {"phase": "at the door", "f": (t - drive) / (leave - drive) if leave > drive else 1.0, "landed": landed}
        if t < back:
            return {"phase": "back", "f": (t - leave) / (back - leave) if back > leave else 1.0, "landed": landed}
        return {"phase": "done", "f": 1.0, "landed": landed}

    def advance(self, day, minute):
        """FURN-028: bring the deliveries up to the clock. Each piece carried in by now is set down in its unit (boxed if it
        came flat-packed and no fitter came with it); if the player no longer owns or rents the unit when it is carried in,
        it goes to the belongings and they are told. Returns the pieces that landed just now."""
        new = []
        for o in self.orders:
            n = self.van(o, day, minute)["landed"]
            while o["landed"] < n:
                p = dict(o["pieces"][o["landed"]])                               # (the order keeps the piece as it was booked: the van's times stay what they were)
                o["landed"] += 1
                if p.pop("fitted", False):
                    p["boxed"] = False
                if self.holds(o["unit"]):
                    self.arrived.setdefault(o["unit"], []).append(p)
                else:
                    self.owned.append(p)
                    self.notes.append([day, minute, "Your %s could not be delivered: %s is no longer yours. It is with your belongings." % (p["name"].lower(), o["unit"])])
                new.append(p["id"])
        return new

    def on_its_way(self):
        return [p["id"] for o in self.orders for p in o["pieces"][o["landed"]:]]

    def move_out(self, unit):
        """PROP-013 with FURN-053's rule for a renter: what was set down in a unit the player leaves comes with them."""
        self.owned.extend(self.arrived.pop(unit, []))

    # ---------------------------------------------------------------- putting a flat-packed piece together (FURN-029)
    def _find(self, pid):
        for where, ps in [("belongings", self.owned)] + [(u, ps) for u, ps in sorted(self.arrived.items())]:
            for p in ps:
                if p["id"] == pid:
                    return where, p
        raise Refused("you have no such piece here")

    def is_boxed(self, pid, day, minute):
        """Whether a piece is still in its boxes at a moment (a piece being put together is whole when its time is up)."""
        _, p = self._find(pid)
        return bool(p["boxed"]) and not (p["ready"] is not None and day * 1440 + minute >= p["ready"] - 1e-9)

    def as_placed(self, pid, day, minute):
        """The piece as the home places it on its floor (furnish.py): itself, or the stack of its boxes."""
        _, p = self._find(pid)
        q = {k: p[k] for k in ("id", "kind", "line", "name", "w", "d", "h", "rug")}
        if self.is_boxed(pid, day, minute):
            _, (w, d, h) = self.boxes(p["kind"])
            q.update(name=p["name"] + " (in its boxes)", w=w, d=d, h=h, boxed=True)
        return q

    def assemble(self, pid, day, minute):
        """The player puts a piece together: it is whole after the piece's assembly time. Returns the minutes it takes."""
        where, p = self._find(pid)
        if not self.is_boxed(pid, day, minute):
            raise Refused("that piece is not in its boxes")
        if where == "belongings":
            raise Refused("put it down in a home of yours first")
        if p["ready"] is not None:
            raise Refused("that piece is being put together")
        m = self.assembly_min(p["kind"])
        p["ready"] = day * 1440 + minute + m
        return m

    def fitter(self, pid, day, slot):
        """Pay a fitter to come in a slot and put a piece together: the fee in one ledger transaction, the piece whole once
        the fitter has been in for its assembly time."""
        where, p = self._find(pid)
        if not self.is_boxed(pid, self.E.day, self.E.minute):
            raise Refused("that piece is not in its boxes")
        if where == "belongings":
            raise Refused("put it down in a home of yours first")
        if p["ready"] is not None:
            raise Refused("that piece is being put together")
        if [day, slot] not in self.slots():
            raise Refused("that slot cannot be booked")
        fee = self.assembly_fee(p["kind"])
        self.E.spend(fee, self.acct, "%s: a fitter for the %s" % (self.R["company"]["name"], p["name"].lower()))
        p["ready"] = day * 1440 + self.R["delivery"]["slots"][slot][0] + self.assembly_min(p["kind"])
        return fee

    # ---------------------------------------------------------------- the company's money (FURN-024)
    def found(self, day=0, minute=0):
        """When the town begins the company has its float, made as the shop tills are (sys:founding)."""
        f = self.R["company"]["float"]
        if f and not self.E.L.balance(self.acct):
            self.E.L.post(day, minute, "%s opens" % self.R["company"]["name"], [("sys:founding", -f), (self.acct, f)])

    def settle(self, day):
        """At the end of each day up to `day`, what the company holds above its float pays its staff (sys:wages) and its
        stock (sys:spend), each day its own transaction dated at the day's end. With no customer nothing moves, so the money
        supply is as it was (PROP-018)."""
        c = self.R["company"]
        for d in range(self.settled + 1, day + 1):
            over = self.E.L.balance(self.acct) - c["float"]
            if over > 0:
                wages = over * c["wages_bp"] // 10000
                self.E.L.post(d + 1, 0, "%s: wages and stock" % c["name"], [(self.acct, -over), ("sys:wages", wages), ("sys:spend", over - wages)])
            self.settled = d

    # ---------------------------------------------------------------- the save (QST-008)
    def save(self):
        return dict(n=self.n, owned=[dict(p) for p in self.owned], orders=[dict(o, pieces=[dict(p) for p in o["pieces"]]) for o in self.orders],
                    arrived={u: [dict(p) for p in ps] for u, ps in self.arrived.items()}, notes=[list(x) for x in self.notes],
                    has_catalogue=self.has_catalogue, settled=self.settled)

    def load(self, s):
        self.n, self.has_catalogue, self.settled = s["n"], s["has_catalogue"], s["settled"]
        self.owned = [dict(p) for p in s["owned"]]
        self.orders = [dict(o, pieces=[dict(p) for p in o["pieces"]]) for o in s["orders"]]
        self.arrived = {u: [dict(p) for p in ps] for u, ps in s["arrived"].items()}
        self.notes = [list(x) for x in s["notes"]]


def set_down(F, furnish, room, keep, ceiling, layout, piece, entry):
    """FURN-028: where a delivered piece stands in the unit: where the player marked it (piece["at"]) if nothing is wrong
    with it there, else the free spot on the grid nearest the way in. Returns the layout's new piece, or None when there
    is no room on the floor (it then waits by the door, as arrived)."""
    was = {k for k, _ in furnish.problems(F, room, keep, ceiling, layout)}

    def fits(x, z, yaw):
        q = dict({k: v for k, v in piece.items() if k != "at"}, x=x, z=z, yaw=yaw)
        return q if not [k for k, _ in furnish.problems(F, room, keep, ceiling, layout + [q]) if k not in was] else None

    if piece.get("at"):
        q = fits(*piece["at"])
        if q:
            return q
    g, best = 0.25, None
    x = room[0] + g
    while x < room[2]:
        z = room[1] + g
        while z < room[3]:
            dist = math.hypot(x - entry[0], z - entry[1])
            if best is None or dist < best[0] - 1e-9:
                q = fits(round(x, 2), round(z, 2), 0)
                if q:
                    best = (dist, q)
            z += g
        x += g
    return best[1] if best else None


def check_script():
    """A fixed run for the ports (the slice's and the engine's) to reproduce exactly: [op, arguments...] in order, the
    player given 600,000 chips and a current account first. Units "u:mine" (theirs) and "u:lost" (theirs until day 3)."""
    D = lambda kind, line, unit, day, slot, assembled=False, at=None: dict(kind=kind, line=line, how="delivery", unit=unit, day=day, slot=slot, assembled=assembled, **({"at": at} if at else {}))  # noqa: E731
    Inst = lambda kind, line: dict(kind=kind, line=line, how="instant")  # noqa: E731
    return [
        ["tag", "k17:sofa", "oldtown"], ["tag", "o_vases", "plain"], ["tag", "spaceship", "plain"], ["tag", "k17:sofa", "chrome"],
        ["checkout", [Inst("k17:coffee", "curve"), Inst("l_sidetable", "workshop")]],                                    # f1, f2
        ["checkout", [D("k17:sofa", "plain", "u:mine", 1, 0), D("k17:bookcase", "plain", "u:mine", 1, 0, True, [2.0, 3.0, 90]), Inst("k17:floor_lamp", "plain")]],   # f3, f4 (o1), f5
        ["checkout", []],
        ["checkout", [Inst("o_vases", "plain")]],                                 # sold at the mall, not here
        ["checkout", [D("k17:bed", "plain", "u:mine", 0, 0)]],                    # today: too late
        ["checkout", [D("k17:bed", "plain", "u:mine", 1, 7)]],                    # no such slot
        ["checkout", [D("k17:bed", "plain", "u:other", 1, 1)]],                   # not theirs
        ["checkout", [Inst("k17:wardrobe", "plain")] * 60],                      # more than they have: nothing is bought
        ["home", [D("k17:armchair", "curve", "u:mine", 2, 2)]],                   # no catalogue yet
        ["catalogue"],
        ["home", [Inst("k17:armchair", "curve")]],                                # from home: delivery only
        ["home", [D("k17:armchair", "curve", "u:lost", 3, 1)]],                   # f6 (o2)
        ["clock", 1, 539], ["van", "o1"], ["clock", 1, 541], ["van", "o1"], ["clock", 1, 546], ["van", "o1"], ["clock", 1, 560], ["van", "o1"],
        ["clock", 1, 568], ["van", "o1"], ["clock", 1, 700], ["van", "o1"],
        ["boxed", "f3"], ["boxed", "f4"], ["assemble", "f3"], ["assemble", "f3"], ["boxed", "f3"], ["clock", 1, 760], ["boxed", "f3"],
        ["assemble", "f1"], ["assemble", "f4"], ["assemble", "f99"],
        ["lose", "u:lost"], ["clock", 3, 800], ["van", "o2"], ["clock", 4, 0],
        ["checkout", [D("k17:dining", "oldtown", "u:mine", 5, 1)]],               # f7 (o3)
        ["fitter", "f7", 5, 2],                                                   # not here yet
        ["clock", 5, 800], ["fitter", "f7", 5, 2], ["fitter", "f7", 6, 0], ["fitter", "f7", 6, 0], ["boxed", "f7"], ["clock", 6, 700], ["boxed", "f7"],
    ]


SCRIPT_MONEY = 600000


def script_player(econ):
    """The player the fixed run starts with: a new character at 07:20 on day 0 with a current account and 600,000 chips in it."""
    econ.advance(0, 440)
    econ.new_character()
    econ.open_account("current")
    econ.L.post(0, 440, "Test money", [("sys:start", -SCRIPT_MONEY), ("p:current", SCRIPT_MONEY)])


def run_script(R, F, econ, script):
    """Each op of check_script: what it gave or why it was refused, then the store as it stands. `econ` is a town's
    Economy with nobody in it yet (script_player() makes the player)."""
    script_player(econ)
    held = {"u:mine": True, "u:lost": True}
    store = Store(R, F, econ, lambda u: held.get(u, False), lambda u: 800.0 if u == "u:mine" else 2000.0)
    store.found()
    out = []
    for op in script:
        try:
            k = op[0]
            if k == "tag":
                r = store.tag(op[1], op[2])
            elif k in ("checkout", "home"):
                total, got = store.checkout([dict(it) for it in op[1]], at_home=k == "home")
                r = [total, got]
            elif k == "catalogue":
                r = store.take_catalogue()
            elif k == "clock":
                econ.advance(op[1], op[2])
                store.settle(op[1] - 1)
                r = store.advance(op[1], op[2])
            elif k == "van":
                o = next(o for o in store.orders if o["id"] == op[1])
                v = store.van(o, econ.day, econ.minute)
                r = [v["phase"], round(v["f"], 6), v["landed"]]
            elif k == "assemble":
                r = store.assemble(op[1], econ.day, econ.minute)
            elif k == "boxed":
                r = store.is_boxed(op[1], econ.day, econ.minute)
            elif k == "fitter":
                r = store.fitter(op[1], op[2], op[3])
            elif k == "lose":
                held[op[1]] = False
                r = None
            else:
                raise Refused("no such op")
            out.append(["ok", r])
        except Refused as e:
            out.append(["refused", str(e)])
    return dict(results=out, owned=[p["id"] for p in store.owned], arrived={u: [p["id"] for p in ps] for u, ps in sorted(store.arrived.items())},
                on_its_way=store.on_its_way(), notes=store.notes, company=econ.L.balance(store.acct), player=econ.total_money(),
                supply=econ.L.supply(), words=fmt(econ.L.balance(store.acct)))
