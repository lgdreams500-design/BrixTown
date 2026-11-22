"""The real estate market (F-PROP-ESTATE level 2, ADR-0022).

Rules from docs/specs/GAME_SPEC.md: PROP-002 (buy, sell and rent buildings and the units inside them), PROP-006 (upkeep
and tax), PROP-009 to PROP-020 and BANK-002 (mortgages). Tuning: World/rules/estate.json.

Every building that is not public land is carved into units from its floors. Each unit has an owner (the city, a real
estate company, the household or business that uses it, or the player) and maybe a tenant. Money moves only through
the economy's ledger (economy.Ledger), so the money supply stays computable (ECO-003).

Nothing is ticked. Like the bank (BANK-008), the market is settled one game day at a time, in order, at each day's end,
when the clock is moved on: rents, upkeep and tax, earnings, tenants arriving and leaving, owners listing, buyers buying,
mortgage instalments. What happens on a day is decided from the day, the data and what the player did, through the
same hash as the rest of the town, so the same game gives the same market on every machine. The browser slice runs a
port of this file (Prototype/play/estate.js) and checks it against check_script().

Standard library only.
"""
import copy
import math

from economy import Refused, hash01

PLAYER = ("p:cash", "p:current", "p:savings")
TOWN = "npc:town"


def block_of(lot_id):
    return lot_id.rsplit("_l", 1)[0]


def centre(l):
    return ((l["rect"][0] + l["rect"][2]) / 2, (l["rect"][1] + l["rect"][3]) / 2)


def clamp(v, a, b):
    return a if v < a else b if v > b else v


def carve(R, lots, residents):
    """PROP-009: the units of every building, in lot order. A unit is {id, lot, lot_id, block, kind, floors, area,
    loc_bp, floor_bp, members (resident ids of the household living there), tenant0}."""
    by_home = {}
    for r in residents:
        if r.get("home"):
            by_home.setdefault(r["home"], []).append(r)
    L = R["location"]
    cl = next(l for l in lots if l["id"] == L["centre_lot"])
    cx, cz = centre(cl)
    F = R["floor"]
    out = []
    for n, l in enumerate(lots):
        if l["kind"] in R["public_kinds"] or not l.get("floors"):
            continue
        rule = R["units"].get(l["kind"], R["units"]["default"])
        floors = l["floors"]
        st = l.get("storeys") or (["main"] + ["rooms"] * (floors - 1))
        groups = []                                         # (unit kind, [floors])
        if "whole" in rule and not any(s == rule.get("flat_use") for s in st[1:]):
            groups.append((rule["whole"], list(range(floors))))
        elif "per_floor" in rule:
            groups += [(rule["per_floor"], [f]) for f in range(floors)]
        else:
            if "main" in rule:
                groups.append((rule["main"], [f for f in range(floors) if f == 0 or st[f] in rule.get("with_main", [])]))
            groups += [("flat", [f]) for f in range(1, floors) if st[f] == rule.get("flat_use")]
        x, z = centre(l)
        dx, dz = x - cx, z - cz
        dist = int(math.sqrt(dx * dx + dz * dz))
        loc = L["near_bp"] - (L["near_bp"] - L["far_bp"]) * min(dist, L["radius"]) // L["radius"]
        fw, fh = l["rect"][2] - l["rect"][0], l["rect"][3] - l["rect"][1]
        people = by_home.get(l["id"], [])
        beds = (l.get("levels") or {}).get("bed") or []
        work = (l.get("levels") or {}).get("work") or []
        for k, (kind, fl) in enumerate(groups):
            f0 = fl[0]
            if f0 == 0:
                fb = 10000
            elif l.get("lift") or f0 <= F["no_lift_above"]:
                fb = 10000 + F["up_bp"] * f0
            else:
                fb = 10000 + F["up_bp"] * F["no_lift_above"] - F["no_lift_down_bp"] * (f0 - F["no_lift_above"])
            uid = "%s:u%d" % (l["id"], k)
            if kind == "house":
                members = [r["id"] for r in people]
            elif kind == "flat" and l["kind"] == "home":
                members = [r["id"] for i, r in enumerate(people)
                           if (beds[i] if i < len(beds) else 1 + i // R["household_size"]) == f0]
            else:
                members = []
            if members:
                tenant = "hh"
            elif kind == "flat" or (kind == "office" and f0 > 0 and f0 not in work):
                tenant = "lodger" if hash01(uid, "lodger") * 1000 < R["letting"]["lodger_occupied_permille"] else None
            elif kind == "house":
                tenant = None
            else:
                tenant = "firm"
            out.append(dict(id=uid, lot=n, lot_id=l["id"], block=block_of(l["id"]), kind=kind, floors=fl,
                            area=int(fw * fh * len(fl) * R["usable_bp"] / 10000), loc_bp=loc, floor_bp=fb,
                            members=members, tenant0=tenant))
    return out


class Estate:
    """The town's buildings and units, their owners and tenants, the companies, and the player's deeds, tenancies and
    mortgages. Works on an economy.Economy and its ledger; settles each day the economy settles."""

    def __init__(self, rules, econ, lots, residents):
        self.R = rules
        self.E = econ
        self.lots = lots
        self.units = carve(rules, lots, residents)
        self.by_id = {u["id"]: i for i, u in enumerate(self.units)}
        self.by_lot = {}
        for i, u in enumerate(self.units):
            self.by_lot.setdefault(u["lot"], []).append(i)
        self.blocks = []
        for u in self.units:
            if u["block"] not in self.blocks:
                self.blocks.append(u["block"])
        roles = {r["id"]: r["role"] for r in residents}
        W = rules["town"]["wages"]
        self.wage = [sum(W.get(roles[m], 0) for m in u["members"]) for u in self.units]
        self.cos = {c["id"]: c for c in rules["companies"]}
        self.office = {c["office"]: c["id"] for c in rules["companies"]}
        self.base = [u["area"] * rules["kinds"][u["kind"]]["per_m2"] * u["loc_bp"] // 10000 * u["floor_bp"] // 10000 for u in self.units]
        self.base_total = sum(self.base)
        self.started = False
        self.owner = [self._first_owner(i) for i in range(len(self.units))]
        self.tenant = [u["tenant0"] for u in self.units]
        self.rent = [0] * len(self.units)                   # what the player asks, on units the player owns
        self.to_let = [False] * len(self.units)             # the player's units: offered to tenants
        self.tenancy = {}                                   # unit index (str) -> {rent, since, missed} for units the player rents
        self.listing = {}                                   # unit index (str) -> {price, since, agent, by}
        self.missed = {}                                    # unit index (str) -> days of the player's upkeep and tax unpaid
        self.owed = {}                                      # unit index (str) -> chips of it
        self.mortgages = []
        self.buys = []                                      # [day, block] the player bought, for the index (PROP-016)
        self.idx = {b: 10000 for b in self.blocks}
        self.history = []                                   # [day, town index bp, money supply, sales, earned-in, spent-out]
        self.sales = []                                     # the last sales: [day, unit, price, seller, buyer]
        self.short = {}                                     # company id -> True if it could not pay its tax yesterday
        self.income = 0                                     # the town's earnings on the last settled day
        self.econ_hook()

    @property
    def L(self):
        return self.E.L                                     # the economy's ledger (a loaded game has a new one)

    def econ_hook(self):
        if self.settle not in self.E.hooks:
            self.E.hooks.append(self.settle)

    def _first_owner(self, i):
        """PROP-010: who owns each unit when the town begins, from the unit's kind, worked out from its id."""
        u = self.units[i]
        w = self.R["kinds"][u["kind"]]["owners"]
        opts = [(k, v) for k, v in w.items() if v > 0 and not (k == "household" and u["tenant0"] != "hh") and not (k == "firm" and u["tenant0"] != "firm")]
        if not opts:
            opts = [("city", 1)]
        t = hash01(u["id"], "owner") * sum(v for _, v in opts)
        pick = opts[-1][0]
        for k, v in opts:
            if t < v:
                pick = k
                break
            t -= v
        if pick == "household":
            return "hh"
        if pick == "firm":
            return "firm"
        if pick == "city":
            return "city"
        return "co:" + self._company_for(u["kind"], u["id"], "first")

    def _company_for(self, kind, *seed):
        cs = [c for c in self.R["companies"] if kind in c["focus"]] or self.R["companies"]
        best, bs = None, -1.0
        for c in cs:
            s = hash01(c["id"], *seed) * c["weight"]
            if s > bs:
                best, bs = c["id"], s
        return best

    def start(self):
        """The companies open with their capital and the town with a few days of its earnings (ECO-003: from sys:founding)."""
        if self.started:
            return
        self.started = True
        lines = [("co:" + c["id"], c["cash"]) for c in self.R["companies"]]
        lines.append((TOWN, self.R["town"]["pool_days"] * self.day_income()))
        lines.append(("sys:founding", -sum(v for _, v in lines)))
        self.L.post(self.E.day, self.E.minute, "The estate agents open; the town's households and businesses begin", lines)

    # ---------------------------------------------------------------- values (PROP-011)
    def value(self, i):
        return self.base[i] * self.idx[self.units[i]["block"]] // 10000

    def market_rent(self, i):
        return max(1, self.value(i) * self.R["kinds"][self.units[i]["kind"]]["yield_bp"] // 10000)

    def rent_of(self, i):
        """What the tenant of unit i pays a day. A household or business that lives or works there pays no more than
        the market rent, whatever the player asks (PROP-014)."""
        if self.tenant[i] == "player":
            return self.tenancy[str(i)]["rent"]
        if self.owner[i] == "player":
            r = self.rent[i] or self.market_rent(i)
            return min(r, self.market_rent(i)) if self.tenant[i] in ("hh", "firm") else r
        return self.market_rent(i)

    def town_index(self):
        return sum(self.value(i) for i in range(len(self.units))) * 10000 // self.base_total

    def day_income(self):
        """The town's earnings for a day: household wages, and businesses' and lodgers' takings as a share of rent."""
        T = self.R["town"]
        n = 0
        for i in range(len(self.units)):
            t = self.tenant[i]
            if t == "hh":
                n += self.wage[i]
            elif t == "firm":
                n += self.rent_of(i) * T["firm_cover_bp"] // 10000 if self.owner[i] != "firm" else 0
            elif t == "lodger":
                n += self.rent_of(i) * T["lodger_cover_bp"] // 10000
        return n

    def owned(self):
        return [i for i, o in enumerate(self.owner) if o == "player"]

    def cost_bp(self, i):
        C = self.R["costs"]
        bp = C["upkeep_bp"] + C["tax_bp"]
        if self.owner[i] == "player":
            bp += min(C["portfolio_cap_bp"], C["portfolio_bp_per_unit"] * max(0, len(self.owned()) - C["portfolio_free"]))
        return bp

    def cost_of(self, i):
        return self.value(i) * self.cost_bp(i) // 10000

    def acct(self, owner):
        """The ledger account an owner pays from and is paid into."""
        if owner == "city":
            return "sys:city"
        if owner in ("hh", "firm"):
            return TOWN
        if owner == "player":
            return "p:current" if "current" in self.E.accounts else "p:cash"
        return owner

    # ---------------------------------------------------------------- the day (PROP-012, PROP-015, PROP-016)
    def _wave(self, b, d):
        I = self.R["index"]
        n = I["wave_days"]
        w, t = d // n, d % n
        h0 = int(hash01(b, w, "wave") * 2001) - 1000
        h1 = int(hash01(b, w + 1, "wave") * 2001) - 1000
        return (h0 * (n - t) + h1 * t) * I["wave_bp"] // (n * 1000)

    def update_index(self, d):
        I = self.R["index"]
        occ, tot = {}, {}
        for i, u in enumerate(self.units):
            tot[u["block"]] = tot.get(u["block"], 0) + 1
            if self.tenant[i] is not None:
                occ[u["block"]] = occ.get(u["block"], 0) + 1
        bump = {}
        for day, b in self.buys:
            if d - day < I["player_buy_days"]:
                bump[b] = min(I["player_buy_cap_bp"], bump.get(b, 0) + I["player_buy_bp"])
        for b in self.blocks:
            o = occ.get(b, 0) * 1000 // tot[b]
            v = 10000 + self._wave(b, d) + (o - I["occupancy_target_permille"]) * I["occupancy_bp_per_permille"] + bump.get(b, 0)
            self.idx[b] = clamp(v, I["min_bp"], I["max_bp"])
        self.buys = [x for x in self.buys if d - x[0] < I["player_buy_days"]]

    def settle(self, d):
        if not self.started:
            return
        self.update_index(d)
        earned, spent = self._rents_and_costs(d)
        self._letting(d)
        sales = self._market(d)
        self._mortgages(d)
        self._companies(d)
        spent += self._town_spend(d)
        self.history.append([d, self.town_index(), self.L.supply(), sales, earned, spent])
        self.history = self.history[-self.R["history_days"]:]

    def _rents_and_costs(self, d):
        T = self.R["town"]
        npc = {}                                            # account -> change, in the order first touched

        def add(a, v):
            if v:
                npc[a] = npc.get(a, 0) + v
        p_costs, p_rent_in, p_rent_out = [], 0, []
        income = 0
        for i in range(len(self.units)):
            o, t = self.owner[i], self.tenant[i]
            if o != "city":
                c = self.cost_of(i)
                if o == "player":
                    p_costs.append((i, c))
                else:
                    add(self.acct(o), -c)
                    add("sys:tax", c)
            if t is not None and not (t == "hh" and o == "hh") and not (t == "firm" and o == "firm"):
                r = self.rent_of(i)
                if t == "player":
                    p_rent_out.append((i, r))
                elif o == "player":
                    p_rent_in += r
                else:
                    add(TOWN, -r)
                    add(self.acct(o), r)
            if t == "hh":
                w = self.wage[i]
            elif t == "firm" and o != "firm":
                w = self.rent_of(i) * T["firm_cover_bp"] // 10000
            elif t == "lodger":
                w = self.rent_of(i) * T["lodger_cover_bp"] // 10000
            else:
                w = 0
            add(TOWN, w)
            add("sys:wages", -w)
            income += w
        # a company that cannot pay all its tax pays what it can; the rest is waived and it sells sooner (PROP-017)
        self.short = {}
        for a in list(npc):
            if a.startswith("co:") and self.L.balance(a) + npc[a] < 0:
                gap = -(self.L.balance(a) + npc[a])
                npc[a] += gap
                npc["sys:tax"] = npc.get("sys:tax", 0) - gap
                self.short[a[3:]] = True
        if self.L.balance(TOWN) + npc.get(TOWN, 0) - p_rent_in < 0:
            gap = -(self.L.balance(TOWN) + npc.get(TOWN, 0) - p_rent_in)
            npc[TOWN] = npc.get(TOWN, 0) + gap
            npc["sys:wages"] = npc.get("sys:wages", 0) - gap
            income += gap
        lines = [(a, v) for a, v in npc.items() if v]
        if lines:
            self.L.post(d, 1440, "Rents, upkeep, tax and earnings across town", lines)
        self.income = income
        if p_rent_in:
            self.L.post(d, 1440, "Rent from your tenants", [(TOWN, -p_rent_in), (self.acct("player"), p_rent_in)])
        for i, r in p_rent_out:                               # the player's own rent (PROP-013)
            k = str(i)
            ten = self.tenancy[k]
            if self._pay(d, r, self.acct(self.owner[i]), "Rent, %s" % self.name(i)):
                ten["missed"] = 0
            else:
                ten["missed"] += 1
                if ten["missed"] >= self.R["letting"]["missed_before_eviction"]:
                    self.tenant[i] = None
                    del self.tenancy[k]
        for i, c in p_costs:                                  # upkeep and tax on the player's property (PROP-006, PROP-017)
            k = str(i)
            due = c + self.owed.get(k, 0)
            if due and self._pay(d, due, "sys:tax", "Upkeep and tax, %s" % self.name(i)):
                self.owed.pop(k, None)
                self.missed.pop(k, None)
            elif due:
                self.owed[k] = due
                self.missed[k] = self.missed.get(k, 0) + 1
                if self.missed[k] >= self.R["costs"]["missed_before_seizure"]:
                    self._seize(i, d)
        return income, 0

    def _pay(self, d, amount, to, memo):
        """From the player's cash, then the current account, whole or not at all."""
        if amount <= 0:
            return True
        cash = self.L.balance("p:cash")
        cur = self.L.balance("p:current") if "current" in self.E.accounts else 0
        if cash + cur < amount:
            return False
        a = min(cash, amount)
        lines = [("p:cash", -a)] if a else []
        if amount > a:
            lines.append(("p:current", -(amount - a)))
        lines.append((to, amount))
        self.L.post(d, self.E.minute if d == self.E.day and self.E.minute < 1440 else 1440, memo, lines)
        return True

    def _letting(self, d):
        """PROP-015: unseen lodgers take empty units that are offered and leave in time; rent above the market keeps
        them away and drives them out."""
        T = self.R["letting"]
        for i in range(len(self.units)):
            o, t = self.owner[i], self.tenant[i]
            mine = o == "player"
            ratio = (self.rent_of(i) if mine else self.market_rent(i)) * 10000 // self.market_rent(i)
            if t == "lodger":
                ch = T["leave_bp"] + (T["leave_slope_bp"] * max(0, ratio - 10000) // 10000 if mine else 0)
                if hash01(self.units[i]["id"], d, "leave") * 10000 < ch:
                    self.tenant[i] = None
            elif t is None and (not mine or self.to_let[i]):
                ch = clamp(T["arrive_bp"] - T["arrive_slope_bp"] * (ratio - 10000) // 10000, 0, 10000) if mine else T["arrive_bp"]
                if hash01(self.units[i]["id"], d, "arrive") * 10000 < ch:
                    self.tenant[i] = "lodger"

    def _market(self, d):
        """PROP-012, PROP-016: listings sell or are cut; owners list; buyers buy."""
        M = self.R["trade"]
        sold = 0
        for i in range(len(self.units)):
            k = str(i)
            ls = self.listing.get(k)
            if ls:
                ratio = self.value(i) * 10000 // ls["price"]
                ch = clamp(M["sale_bp"] + M["sale_slope_bp"] * (ratio - 10000) // 10000, 0, M["sale_max_bp"])
                if hash01(self.units[i]["id"], d, "sale") * 10000 < ch:
                    b = self._buyer(i, ls["price"], d)
                    if b:
                        self._transfer(i, b, ls["price"], d, ls["agent"], "Sold: %s" % self.name(i))
                        sold += 1
                        continue
                if self.owner[i] != "player" and (d - ls["since"]) > 0 and (d - ls["since"]) % M["listing_days"] == 0:
                    ls["price"] = ls["price"] * (10000 - M["price_cut_bp"]) // 10000
            elif self.owner[i] not in ("player", "city"):
                o = self.owner[i]
                typ = "company" if o.startswith("co:") else "household" if o == "hh" else "firm"
                ch = M["list_bp"][typ] + max(0, self.idx[self.units[i]["block"]] - 10000) * M["list_high_index_bp"] // 100
                if typ == "company" and self.short.get(o[3:]):
                    ch += M["list_low_cash_bp"]
                if typ == "company" and self.L.balance(o) < self.cos[o[3:]]["reserve"]:
                    ch += M["list_low_cash_bp"]
                if hash01(self.units[i]["id"], d, "list") * 10000 < ch:
                    lo, hi = M["ask_bp"]
                    ask = lo + int(hash01(self.units[i]["id"], d, "ask") * (hi - lo + 1))
                    agent = o[3:] if typ == "company" else self._company_for(self.units[i]["kind"], self.units[i]["id"], d, "agent")
                    self.listing[k] = dict(price=self.value(i) * ask // 10000, since=d, agent=agent, by=o)
        return sold

    def _buyer(self, i, price, d):
        """Who buys a listed unit today: the household living in it if it can, else the company that wants it most and
        can pay, keeping its reserve. None if nobody can."""
        M = self.R["trade"]
        u = self.units[i]
        tax = price * M["transfer_tax_bp"] // 10000
        if self.tenant[i] == "hh" and self.owner[i] != "hh":
            if hash01(u["id"], d, "hh") * 10000 < M["household_buy_bp"] and \
                    self.L.balance(TOWN) >= price + tax + M["household_min_pool_days"] * self.income:
                return "hh"
        best, bs = None, -1.0
        for c in self.R["companies"]:
            a = "co:" + c["id"]
            if a == self.owner[i] or u["kind"] not in c["focus"]:
                continue
            if self.L.balance(a) < price + tax + c["reserve"]:
                continue
            s = hash01(c["id"], u["id"], d, "buyer") * c["weight"]
            if s > bs:
                best, bs = a, s
        return best

    def _transfer(self, i, buyer, price, d, agent, memo, extra=None, minute=1440):
        """One sale as one transaction: the buyer pays price and transfer tax; the seller gets the price less the agent's
        fee (none when a company sells its own) and less any mortgage on the unit, which goes back to the bank."""
        M = self.R["trade"]
        seller = self.owner[i]
        tax = price * M["transfer_tax_bp"] // 10000
        fee = 0 if (agent is None or seller == "co:" + agent) else price * M["agent_fee_bp"] // 10000
        lines = extra[:] if extra else [(self.acct(buyer), -(price + tax))]
        lines.append(("sys:tax", tax))
        debt = 0
        if seller == "player":
            m = self.mortgage_on(i)
            if m:
                debt = self.mortgage_owed(m)
                if price - fee < debt:
                    raise Refused("the price would not pay off the mortgage")
                lines.append(("bank:vault", debt))
        lines.append((self.acct(seller), price - fee - debt))
        if fee:
            lines.append(("co:" + agent, fee))
        self.L.post(d, minute, memo, lines)
        if seller == "player" and debt:
            m = self.mortgage_on(i)
            m["paid"] += debt
            m["status"] = "repaid"
        self.owner[i] = buyer
        if buyer == "player":
            self.rent[i] = self.market_rent(i)
            self.to_let[i] = True
            self.buys.append([d, self.units[i]["block"]])
        else:
            self.to_let[i] = False
        k = str(i)
        self.listing.pop(k, None)
        self.owed.pop(k, None)
        self.missed.pop(k, None)
        self.sales.append([d, i, price, seller, buyer])
        self.sales = self.sales[-40:]

    def _seize(self, i, d):
        """PROP-017: three days of unpaid upkeep and tax and the city sells the unit to a company at the forced bid; what is
        owed and the fee come out of the price, the rest goes to the player."""
        C = self.R["costs"]
        k = str(i)
        owed = self.owed.get(k, 0) + C["seizure_fee"]
        self._forced_sale(i, d, owed, "sys:tax", "Seized for unpaid tax: %s" % self.name(i))

    def _forced_sale(self, i, d, owed, to, memo):
        M = self.R["trade"]
        price = self.value(i) * M["forced_bid_bp"] // 10000
        tax = price * M["transfer_tax_bp"] // 10000
        buyer = None
        for c in self.R["companies"]:
            if self.L.balance("co:" + c["id"]) >= price + tax:
                buyer = "co:" + c["id"]
                break
        if buyer is None:
            buyer = "city"
        m = self.mortgage_on(i)
        debt = self.mortgage_owed(m) if m else 0
        take = min(price, owed)
        lines = [(self.acct(buyer), -(price + tax)), ("sys:tax", tax), (to, take)]
        back = min(price - take, debt)
        if back:
            lines.append(("bank:vault", back))
        rest = price - take - back
        if rest:
            lines.append((self.acct("player"), rest))
        self.L.post(d, 1440, memo, lines)
        if m:
            m["paid"] += back
            m["status"] = "repaid" if back >= debt else "defaulted"
        self.owner[i] = buyer
        self.to_let[i] = False
        k = str(i)
        self.listing.pop(k, None)
        self.owed.pop(k, None)
        self.missed.pop(k, None)
        self.sales.append([d, i, price, "player", buyer])
        self.sales = self.sales[-40:]

    def _companies(self, d):
        """PROP-018: what a company holds above its cap is paid out to its owners (sys:dividends)."""
        for c in self.R["companies"]:
            a = "co:" + c["id"]
            over = self.L.balance(a) - c["cap"]
            if over > 0:
                pay = over * c["dividend_bp"] // 10000
                if pay:
                    self.L.post(d, 1440, "%s pays a dividend" % c["name"], [(a, -pay), ("sys:dividends", pay)])

    def _town_spend(self, d):
        """PROP-018: the town spends what it holds beyond a few days of its earnings."""
        T = self.R["town"]
        over = self.L.balance(TOWN) - T["pool_days"] * self.income
        if over <= 0:
            return 0
        pay = over * T["spend_bp"] // 10000
        if pay:
            self.L.post(d, 1440, "What the town's households and businesses spend", [(TOWN, -pay), ("sys:spend", pay)])
        return pay

    # ---------------------------------------------------------------- mortgages (BANK-002, PROP-019)
    def mortgage_terms(self, principal, term):
        C = self.R["mortgage"]
        total = principal + principal * C["rate_bp"] * term // 10000
        inst = total // term
        return total, [inst] * (term - 1) + [total - inst * (term - 1)]

    def mortgage_on(self, i):
        return next((m for m in self.mortgages if m["status"] == "active" and i in m["units"]), None)

    def mortgage_owed(self, m):
        return m["total"] + m["fees"] - m["paid"] - m["fees_paid"]

    def _mortgages(self, d):
        C = self.R["mortgage"]
        for m in self.mortgages:
            if m["status"] != "active":
                continue
            k = d - m["day"]
            if k < 1:
                continue
            due = max(0, sum(m["plan"][:k]) - m["paid"]) + m["fees"] - m["fees_paid"]
            due = min(due, self.mortgage_owed(m))
            if due <= 0:
                continue
            if "current" in self.E.accounts and self.L.balance("p:current") >= due:
                fee = m["fees"] - m["fees_paid"]
                lines = [("p:current", -due)]
                if fee:
                    lines.append(("sys:fees", fee))
                if due - fee:
                    lines.append(("bank:vault", due - fee))
                self.L.post(d, 1440, "Mortgage %d repayment" % m["id"], lines)
                m["fees_paid"] += fee
                m["paid"] += due - fee
                m["missed"] = 0
                if self.mortgage_owed(m) <= 0:
                    m["status"] = "repaid"
                continue
            m["missed"] += 1
            m["fees"] += C["late_fee"]
            if m["missed"] >= C["default_after_missed"]:
                for i in list(m["units"]):
                    if self.owner[i] == "player" and m["status"] == "active":
                        self._forced_sale(i, d, 0, "sys:fees", "Repossessed by the bank: %s" % self.name(i))
                if m["status"] == "active":
                    m["status"] = "defaulted"
                self.E.barred = True

    # ---------------------------------------------------------------- what the player sees
    def name(self, i):
        u = self.units[i]
        l = self.lots[u["lot"]]
        K = self.R["kinds"][u["kind"]]["name"]
        if len(self.by_lot[u["lot"]]) == 1:
            return l["name"]
        f = u["floors"][0]
        return "%s, %s%s" % (l["name"], K.lower(), "" if f == 0 and len(u["floors"]) > 1 else " on floor %d" % f if f else " on the ground floor")

    def owner_name(self, i):
        o = self.owner[i]
        if o == "player":
            return "you"
        if o == "city":
            return "the town"
        if o == "hh":
            return "the household living there"
        if o == "firm":
            return "the business there"
        return self.cos[o[3:]]["name"]

    def ask(self, i):
        """PROP-011: what the player would pay for unit i today, or None if it is not for sale to them."""
        if self.owner[i] == "player":
            return None
        ls = self.listing.get(str(i))
        if ls:
            return ls["price"]
        o = self.owner[i]
        M = self.R["trade"]
        if o == "city" and self.units[i]["kind"] not in M["city_sells"]:
            return None
        typ = "company" if o.startswith("co:") else "household" if o == "hh" else o
        return self.value(i) * M["offer_bp"][typ] // 10000

    def bid(self, i):
        return self.value(i) * self.R["trade"]["bid_bp"] // 10000

    # ---------------------------------------------------------------- what the player does
    def _need_office(self, agent):
        if agent is not None and agent not in self.cos:
            raise Refused("no such estate agent")

    def buy(self, units, agent=None, deposit=None, term=None):
        """PROP-011, PROP-019: buy units (one, or a whole building's) at their ask, with the transfer tax; with a deposit
        and a term, the bank lends the rest as a mortgage. One transaction per unit, refused whole beforehand."""
        self._need_office(agent)
        if not units:
            raise Refused("nothing to buy")
        M = self.R["trade"]
        prices = []
        for i in units:
            p = self.ask(i)
            if p is None:
                raise Refused("%s is not for sale" % self.name(i))
            prices.append(p)
        if len(units) > 1:
            lot = self.units[units[0]]["lot"]
            if any(self.units[i]["lot"] != lot for i in units) or sorted(units) != sorted(i for i in self.by_lot[lot] if self.owner[i] != "player"):
                raise Refused("a whole building is bought together")
            prices = [p * (10000 - M["whole_building_discount_bp"]) // 10000 for p in prices]
        total = sum(prices)
        tax = sum(p * M["transfer_tax_bp"] // 10000 for p in prices)
        loan = 0
        if deposit is not None or term is not None:
            C = self.R["mortgage"]
            if "current" not in self.E.accounts:
                raise Refused("a mortgage needs a current account")
            if self.E.barred:
                raise Refused("the bank will not lend to you again")
            if sum(1 for m in self.mortgages if m["status"] == "active") >= C["max"]:
                raise Refused("you already have %d mortgages" % C["max"])
            if term not in C["terms"] or deposit is None or deposit * 10000 < total * C["deposit_bp"] or deposit > total:
                raise Refused("the bank does not offer that mortgage")
            loan = total - deposit
            if self.L.balance("bank:vault") < loan:
                raise Refused("the bank cannot lend that today")
        need = total + tax - loan
        cash = self.L.balance("p:cash")
        cur = self.L.balance("p:current") if "current" in self.E.accounts else 0
        if cash + cur < need:
            raise Refused("you cannot afford it")
        if loan:
            total_m, plan = self.mortgage_terms(loan, term)
            m = dict(id=len(self.mortgages) + 1, day=self.E.day, principal=loan, total=total_m, plan=plan, paid=0, fees=0, fees_paid=0,
                     missed=0, status="active", units=list(units))
        left_loan = loan
        for i, p in zip(units, prices):
            t = p * M["transfer_tax_bp"] // 10000
            from_bank = min(left_loan, p)
            left_loan -= from_bank
            own = p + t - from_bank
            a = min(self.L.balance("p:cash"), own)
            lines = [("p:cash", -a)] if a else []
            if own > a:
                lines.append(("p:current", -(own - a)))
            if from_bank:
                lines.append(("bank:vault", -from_bank))
            ls = self.listing.get(str(i))
            self._transfer(i, "player", p, self.E.day, ls["agent"] if ls else agent if self.owner[i] in ("hh", "firm") else None,
                           "Bought: %s" % self.name(i), extra=lines, minute=self.E.minute)
        if loan:
            self.mortgages.append(m)
        return total + tax

    def sell_now(self, i, agent=None):
        """PROP-012: sell to the company that bids most and can pay, at its bid, at once."""
        self._need_office(agent)
        if self.owner[i] != "player":
            raise Refused("that is not yours")
        price = self.bid(i)
        tax = price * self.R["trade"]["transfer_tax_bp"] // 10000
        best = None
        for c in self.R["companies"]:
            a = "co:" + c["id"]
            if self.units[i]["kind"] in c["focus"] and self.L.balance(a) >= price + tax + c["reserve"]:
                if best is None or self.L.balance(a) > self.L.balance(best):
                    best = a
        if best is None:
            raise Refused("no company will buy it today")
        self._transfer(i, best, price, self.E.day, None, "Sold: %s" % self.name(i), minute=self.E.minute)
        return price

    def list_for_sale(self, i, price, agent):
        """PROP-012: offer a unit through an agent at a price; buyers come on the days that follow."""
        self._need_office(agent)
        if agent is None:
            raise Refused("list it with an estate agent")
        if self.owner[i] != "player":
            raise Refused("that is not yours")
        lo, hi = self.R["trade"]["player_ask_range_bp"]
        v = self.value(i)
        if type(price) is not int or not (v * lo // 10000 <= price <= v * hi // 10000):
            raise Refused("the agent will not list it at that price")
        self.listing[str(i)] = dict(price=price, since=self.E.day, agent=agent, by="player")

    def unlist(self, i):
        if self.owner[i] != "player" or str(i) not in self.listing:
            raise Refused("it is not listed")
        del self.listing[str(i)]

    def set_rent(self, i, rent, to_let=True):
        """PROP-014: the rent the player asks for a unit they own, within data bounds around the market rent."""
        if self.owner[i] != "player":
            raise Refused("that is not yours")
        lo, hi = self.R["letting"]["rent_range_bp"]
        m = self.market_rent(i)
        if type(rent) is not int or not (m * lo // 10000 <= rent <= m * hi // 10000):
            raise Refused("that rent is out of bounds")
        self.rent[i] = rent
        self.to_let[i] = bool(to_let)

    def rent_unit(self, i, agent=None):
        """PROP-013: rent an empty unit someone else owns, at its market rent; the first day is paid now."""
        self._need_office(agent)
        if self.owner[i] == "player":
            raise Refused("it is yours already")
        if self.tenant[i] is not None:
            raise Refused("it is let")
        if len(self.tenancy) >= self.R["letting"]["max_tenancies"]:
            raise Refused("you rent as many places as you may")
        r = self.market_rent(i)
        if not self._pay(self.E.day, r, self.acct(self.owner[i]), "Rent, %s" % self.name(i)):
            raise Refused("you cannot afford the first day's rent")
        self.tenant[i] = "player"
        self.tenancy[str(i)] = dict(rent=r, since=self.E.day, missed=0)

    def leave(self, i):
        if self.tenant[i] != "player":
            raise Refused("you do not rent it")
        self.tenant[i] = None
        del self.tenancy[str(i)]

    def repay(self, mid):
        m = next((m for m in self.mortgages if m["id"] == mid and m["status"] == "active"), None)
        if not m:
            raise Refused("no such mortgage")
        owed = self.mortgage_owed(m)
        if "current" not in self.E.accounts or self.L.balance("p:current") < owed:
            raise Refused("not enough in the current account")
        fee = m["fees"] - m["fees_paid"]
        lines = [("p:current", -owed)] + ([("sys:fees", fee)] if fee else []) + [("bank:vault", owed - fee)]
        self.L.post(self.E.day, self.E.minute, "Mortgage %d repaid early" % m["id"], lines)
        m["fees_paid"] += fee
        m["paid"] += owed - fee
        m["status"] = "repaid"

    # ---------------------------------------------------------------- saving
    def save(self):
        return copy.deepcopy(dict(started=self.started, owner=self.owner, tenant=self.tenant, rent=self.rent, to_let=self.to_let,
                                  tenancy=self.tenancy, listing=self.listing, missed=self.missed, owed=self.owed,
                                  mortgages=self.mortgages, buys=self.buys, idx=self.idx, history=self.history, sales=self.sales,
                                  short=self.short, income=self.income))

    def load(self, s):
        for k, v in copy.deepcopy(s).items():
            setattr(self, k, v)


def check_script():
    """A fixed run whose results the browser port must reproduce exactly (packed by build_play.py)."""
    return [
        ["advance", 0, 440], ["new"], ["start"], ["open", "current"], ["give", 900000],
        ["advance", 3, 600], ["buy_cheapest", "flat", "lark", 0, 0], ["advance", 6, 500],
        ["buy_cheapest", "house", "lark", 25, 60], ["rent_cheapest", "flat"], ["setrent", 0, 12000],
        ["advance", 12, 700], ["list", 0, 10500, "harrow"], ["advance", 20, 300], ["sell_now", 1], ["advance", 30, 900],
    ]


def run_script(est, ops):
    """Runs check_script() ops on an Estate. Returns a summary to compare with the port."""
    E = est.E
    out = []
    mine = []
    for op in ops:
        name, a = op[0], op[1:]
        try:
            if name == "advance":
                E.advance(a[0], a[1])
            elif name == "new":
                E.new_character()
            elif name == "start":
                est.start()
            elif name == "open":
                E.open_account(a[0])
            elif name == "give":
                E.L.post(E.day, E.minute, "Test money", [("sys:start", -a[0]), ("p:current", a[0])])
            elif name == "buy_cheapest":
                kind, agent, dep_pc, term = a
                c = [i for i in range(len(est.units)) if est.units[i]["kind"] == kind and est.ask(i) is not None]
                i = min(c, key=lambda j: (est.ask(j), j))
                if dep_pc:
                    p = est.ask(i)
                    est.buy([i], agent, deposit=p * dep_pc // 100, term=term)
                else:
                    est.buy([i], agent)
                mine.append(i)
            elif name == "rent_cheapest":
                c = [i for i in range(len(est.units)) if est.units[i]["kind"] == a[0] and est.tenant[i] is None and est.owner[i] != "player"]
                est.rent_unit(min(c, key=lambda j: (est.market_rent(j), j)))
            elif name == "setrent":
                i = mine[a[0]]
                est.set_rent(i, est.market_rent(i) * a[1] // 10000)
            elif name == "list":
                i = mine[a[0]]
                est.list_for_sale(i, est.value(i) * a[1] // 10000, a[2])
            elif name == "sell_now":
                est.sell_now(mine[a[0]])
            out.append("ok")
        except (Refused, IndexError):
            out.append("refused")
    accts = sorted(a for a in E.L.bal if a.startswith(("co:", "npc:", "sys:", "p:", "bank:")))
    return dict(results=out, balances={a: E.L.balance(a) for a in accts}, tx=len(E.L.tx), supply=E.L.supply(),
                owner=[o for o in est.owner], tenant=[t for t in est.tenant], idx=dict(sorted(est.idx.items())),
                history=est.history, mortgages=[[m["status"], m["paid"], m["fees"], m["missed"]] for m in est.mortgages],
                listing={k: est.listing[k]["price"] for k in sorted(est.listing, key=int)}, units=len(est.units), mine=mine)
