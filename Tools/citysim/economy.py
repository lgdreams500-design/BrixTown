"""Money in the first slice: the ledger, the bank, shops and the player's market stall (ADR-0012).

Rules from docs/specs/GAME_SPEC.md: ECO-001 to ECO-003 (brix in integer chips, double-entry ledger, money made only by
named system accounts), ECO-005 and ECO-010 (stock and prices worked out from the clock), ECO-009 (statement), ECO-011
(selling to shops), BANK-001, BANK-002, BANK-007, BANK-008 (accounts, interest, loans, settled by the day), PROP-001
(items owned) and PROP-008 (a stall on the market square). Tuning: World/rules/economy.json.

Nothing here is ticked. What residents buy is worked out from their shop visits in the day plan; what the bank owes or
is owed is settled, day by day in order, when the clock is moved on. The browser slice runs a port of this file
(Prototype/play/economy.js); build_play.py packs the results of check_script() so the page can check itself.

Standard library only.
"""
import copy

M32 = 0xFFFFFFFF
PLAYER = ("p:cash", "p:current", "p:savings")


def hash01(*args):
    """The same number in 0..1 for the same inputs, exactly as hash() in Prototype/play/play.js."""
    h = 2166136261
    for a in args:
        for ch in str(a):
            h = ((h ^ ord(ch)) * 16777619) & M32
        h = ((h ^ 124) * 16777619) & M32
    h ^= h >> 15
    h = (h * 2246822519) & M32
    h ^= h >> 13
    h = (h * 3266489917) & M32
    h ^= h >> 16
    return h / 4294967296


def fmt(chips):
    """ECO-001: 1234 chips -> 'Bx 12.34'."""
    sign = "-" if chips < 0 else ""
    c = abs(chips)
    return "%sBx %d.%02d" % (sign, c // 100, c % 100)


def is_system(acct):
    return acct.startswith("sys:")


class Refused(Exception):
    """A movement that cannot be made. Nothing has been written (BANK-007: never partly made)."""


class Ledger:
    """ECO-002: every movement is one transaction whose lines add up to zero, written whole or not at all, never edited.
    Ordinary accounts cannot go below zero; system accounts can (they are where money comes from and goes to, ECO-003)."""

    def __init__(self):
        self.tx = []
        self.bal = {}

    def balance(self, acct):
        return self.bal.get(acct, 0)

    def post(self, day, minute, memo, lines):
        if not lines:
            raise Refused("nothing to post")
        total = 0
        after = {}
        for acct, amt in lines:
            if type(amt) is not int:                       # ECO-001: integer chips only, no floats, no bools
                raise Refused("amounts are whole chips")
            total += amt
            after[acct] = after.get(acct, self.balance(acct)) + amt
        if total != 0:
            raise Refused("lines do not balance")
        for acct, b in after.items():
            if b < 0 and not is_system(acct):
                raise Refused("%s would go below zero" % acct)
        t = (len(self.tx), day, minute, memo, tuple((a, v) for a, v in lines if v != 0))
        self.tx.append(t)
        self.bal.update(after)
        return t

    def supply(self):
        """ECO-003: all the money there is, held anywhere but in a system account."""
        return sum(v for a, v in self.bal.items() if not is_system(a))

    def system_total(self):
        return sum(v for a, v in self.bal.items() if is_system(a))

    def statement(self, accts, n=None):
        """ECO-009: the lines that touched these accounts, newest first: (tx no, day, minute, memo, change)."""
        out = []
        for no, day, minute, memo, lines in reversed(self.tx):
            ch = sum(v for a, v in lines if a in accts)
            if any(a in accts for a, _ in lines):
                out.append((no, day, minute, memo, ch))
            if n and len(out) >= n:
                break
        return out


def visits_from_packed(plan, acts, visit_acts):
    """Shop visits from the slice's packed day plan (build_play.routines): {lot index: [(minute, resident index), ...]}.
    A visit is a stay doing one of visit_acts; a run of stays doing the same thing at the same lot counts once."""
    want = {acts.index(a) for a in visit_acts if a in acts}
    out = {}
    for i, row in enumerate(plan):
        prev = None
        for p in row:
            key = (p[4], p[7]) if p[1] == 0 else None
            if key and key[0] in want and key != prev:
                out.setdefault(p[7], []).append((p[0], i))
            prev = key
    for v in out.values():
        v.sort()
    return out


def outlets_of(lot):
    """The outlets of a lot whose inside is planned (a mall, WLD-063): [{id, name, kind, level}] in the plan's order.
    An outlet is a unit with a kind from World/rules/mall_outlets.json (`outlet`)."""
    out = []
    for lv in (lot.get("plan") or {}).get("levels", ()):
        for u in lv["units"]:
            if u.get("outlet"):
                out.append(dict(id=u["id"], name=u.get("name") or u["outlet"], kind=u["outlet"], level=lv["level"]))
    return out


class Economy:
    """One player's money, items, accounts, loans and stall in one town, with the town's shop tills and bank vault."""

    def __init__(self, rules, lots, visits, staff=None, goods=None, boards=None):
        """rules: World/rules/economy.json. lots: the town's lots in packed order ({id, kind, name}; a mall's `plan` gives
        its outlets). visits: from visits_from_packed(). staff: {lot index: [resident ids who work there]} for reputation
        prices (QST-007). goods: World/rules/mall_goods.json, what a mall's outlets sell (ECO-013, ECO-014); without it
        a mall is bought from as one shop, as before. boards: World/rules/boards.json, the skateboards a skate store sells
        (CITY-102, CITY-103)."""
        self.R = rules
        self.lots = lots
        self.lot_index = {l["id"]: n for n, l in enumerate(lots)}
        self.visits = visits
        self.staff = staff or {}
        self.items = rules["items"]
        self.G = goods
        self.outlets = {}                      # lot index -> {unit id: {id, name, kind, level}}, in plan order (ECO-013)
        self.clothes = {}                      # good id -> how many: bought clothes, for the wardrobe (ECO-014)
        self.homeware = {}                     # good id -> how many bought for the home (each one is a belonging to place)
        self.B = boards
        self.boards = []                       # the kinds of skateboard owned, in the store's order (CITY-100, CITY-103)
        if goods:
            self.items = dict(rules["items"])  # what is carried: the town's items and the outlets' goods that are carried or eaten
            for g, G in goods["goods"].items():
                if G["goes"] in ("carry", "eat"):
                    self.items[g] = dict(name=G["name"], price=G["price"], sell=G["goes"] == "carry")
            for n, l in enumerate(lots):
                us = outlets_of(l)
                if us:
                    self.outlets[n] = {u["id"]: u for u in us}
        self.L = Ledger()
        self.day, self.minute = 0, 0
        self.settled = 0                       # the next day to settle
        self.inv = {}
        self.accounts = []                     # opened: "current", "savings"
        self.welcomed = False
        self.loans = []
        self.barred = False
        self.trades = {}                       # "lot|restock day" -> {item: [bought, sold]}
        self.rep = {}                          # resident id -> -100..100 (QST-007)
        self.stall = None
        self.stall_sold_total = 0
        self.hooks = []                        # more rules settled at each day's end, after the bank (estate.py)
        self.bank_lot = self.lot_index[rules["bank"]["lot"]]
        self.market_lot = self.lot_index[rules["stall"]["lot"]]
        lines = [("bank:vault", rules["bank"]["vault"])]
        for n, l in enumerate(lots):
            k = self.shop_kind(n)
            if k:
                lines.append(("till:%s" % l["id"], rules["shops"][k]["till"]))
        lines.append(("sys:founding", -sum(v for _, v in lines)))
        self.L.post(0, 0, "The town's bank and shops open", lines)

    # ---------------------------------------------------------------- the player
    def new_character(self):
        self.L.post(self.day, self.minute, "Starting cash", [("sys:start", -self.R["start"]["cash"]), ("p:cash", self.R["start"]["cash"])])

    def total_money(self):
        return sum(self.L.balance(a) for a in PLAYER)

    def give_item(self, item, n=1):
        if item not in self.items:
            raise Refused("no such item %s" % item)
        self.inv[item] = self.inv.get(item, 0) + n

    def take_item(self, item, n=1):
        if self.inv.get(item, 0) < n:
            raise Refused("you have no %s" % item)
        self.inv[item] -= n
        if not self.inv[item]:
            del self.inv[item]

    def spend(self, amount, to, memo):
        """Pay from cash first, then the current account, in one transaction (refused whole if both together fall short)."""
        if amount <= 0:
            raise Refused("nothing to pay")
        cash = self.L.balance("p:cash")
        a = min(cash, amount)
        lines = [("p:cash", -a)] if a else []
        if amount > a:
            if "current" not in self.accounts:
                raise Refused("not enough cash")
            lines.append(("p:current", -(amount - a)))
        lines.append((to, amount))
        return self.L.post(self.day, self.minute, memo, lines)

    # ---------------------------------------------------------------- the clock and the bank
    def advance(self, day, minute):
        """Move the clock on. Every day that ended on the way is settled, in order (BANK-008)."""
        if (day, minute) < (self.day, self.minute):
            raise ValueError("the clock only goes forward")
        if self.stall and self.stall["day"] < day:
            self.run_stall(self.stall["day"], 1440)
        while self.settled < day:
            self.day, self.minute = self.settled, 1440
            self.settle(self.settled)
            self.settled += 1
        self.day, self.minute = day, minute
        if self.stall and self.stall["day"] == day:
            self.run_stall(day, minute)

    def settle(self, d):
        B = self.R["bank"]
        for kind in self.accounts:                          # BANK-001: interest on each account
            rate = B["accounts"][kind]["rate_bp"]
            acct = "p:" + kind
            gain = self.L.balance(acct) * rate // 10000
            if gain > 0 and self.L.balance("bank:vault") >= gain:
                self.L.post(d, 1440, "Interest, %s" % B["accounts"][kind]["name"].lower(), [("bank:vault", -gain), (acct, gain)])
        for loan in self.loans:                             # BANK-002: instalments, late fees, default
            if loan["status"] == "active":
                self._loan_day(loan, d)
        if self.stall and self.stall["day"] == d:           # the stall closes: what is left comes home
            for s in self.stall["slots"]:
                if not s["sold"]:
                    self.give_item(s["item"])
            self.stall = None
        for hook in self.hooks:
            hook(d)

    def open_account(self, kind):
        if kind not in self.R["bank"]["accounts"]:
            raise Refused("no such account")
        if kind in self.accounts:
            raise Refused("already open")
        if kind == "savings" and "current" not in self.accounts:
            raise Refused("open a current account first")
        self.accounts.append(kind)
        if kind == "current" and not self.welcomed:
            self.welcomed = True
            w = self.R["start"]["welcome_deposit"]
            self.L.post(self.day, self.minute, "Linden Bank welcome deposit", [("sys:start", -w), ("p:current", w)])

    def move(self, src, dst, amount):
        """BANK-007: between cash, current and savings."""
        for a in (src, dst):
            if a not in PLAYER or (a != "p:cash" and a[2:] not in self.accounts):
                raise Refused("no such account open: %s" % a)
        if src == dst or amount <= 0:
            raise Refused("nothing to move")
        return self.L.post(self.day, self.minute, "Transfer", [(src, -amount), (dst, amount)])

    def loan_terms(self, principal, term):
        C = self.R["bank"]["loan"]
        total = principal + principal * C["rate_bp"] * term // 10000
        inst = total // term
        return total, [inst] * (term - 1) + [total - inst * (term - 1)]

    def take_loan(self, principal, term):
        C = self.R["bank"]["loan"]
        if "current" not in self.accounts:
            raise Refused("a loan needs a current account")
        if self.barred:
            raise Refused("the bank will not lend to you again")
        if sum(1 for l in self.loans if l["status"] == "active") >= C["max_loans"]:
            raise Refused("you already have a loan")
        if not (C["min"] <= principal <= C["max"]) or principal % C["step"] or term not in C["terms"]:
            raise Refused("the bank does not offer that loan")
        if self.L.balance("bank:vault") < principal:
            raise Refused("the bank cannot lend that today")
        total, plan = self.loan_terms(principal, term)
        loan = dict(id=len(self.loans) + 1, day=self.day, principal=principal, total=total, plan=plan, paid=0, fees=0, fees_paid=0,
                    missed=0, taken=0, status="active")
        self.L.post(self.day, self.minute, "Loan %d paid out" % loan["id"], [("bank:vault", -principal), ("p:current", principal)])
        self.loans.append(loan)
        return loan

    def loan_owed(self, loan):
        return loan["total"] + loan["fees"] - loan["paid"] - loan["fees_paid"] - loan["taken"]

    def _pay_loan(self, loan, amount, d, minute, memo):
        """amount from the current account: fees first (to sys:fees), the rest toward the loan (to the vault)."""
        fee = min(amount, loan["fees"] - loan["fees_paid"])
        lines = [("p:current", -amount)]
        if fee:
            lines.append(("sys:fees", fee))
        if amount - fee:
            lines.append(("bank:vault", amount - fee))
        self.L.post(d, minute, memo, lines)
        loan["fees_paid"] += fee
        loan["paid"] += amount - fee
        if self.loan_owed(loan) <= 0:
            loan["status"] = "repaid"

    def _loan_day(self, loan, d):
        C = self.R["bank"]["loan"]
        k = d - loan["day"]                                 # instalments due by the end of day d
        if k < 1:
            return
        due_sched = sum(loan["plan"][:k]) - loan["paid"] - loan["taken"]
        due = max(0, due_sched) + loan["fees"] - loan["fees_paid"]
        if due <= 0:
            return
        if "current" in self.accounts and self.L.balance("p:current") >= due:
            self._pay_loan(loan, due, d, 1440, "Loan %d repayment" % loan["id"])
            loan["missed"] = 0
            return
        loan["missed"] += 1
        loan["fees"] += C["late_fee"]
        if loan["missed"] >= C["default_after_missed"]:
            self._default(loan, d)

    def _default(self, loan, d):
        """BANK-002 default: the bank takes what is in the current account, then repossesses the player's items at their
        sell price toward the debt, writes off the rest and lends no more."""
        cur = self.L.balance("p:current")
        if cur > 0:
            self._pay_loan(loan, min(cur, self.loan_owed(loan)), d, 1440, "Loan %d: account seized" % loan["id"])
        share = self.R["price"]["sell_share_bp"]
        for item in sorted(self.inv):
            I = self.items[item]
            if not I["sell"] or I["price"] <= 0:
                continue
            while self.inv.get(item) and self.loan_owed(loan) > 0:
                self.take_item(item)
                loan["taken"] += I["price"] * share // 10000
        loan["status"] = "defaulted"
        self.barred = True

    def repay(self, loan_id):
        loan = next((l for l in self.loans if l["id"] == loan_id and l["status"] == "active"), None)
        if not loan:
            raise Refused("no such loan")
        owed = self.loan_owed(loan)
        if self.L.balance("p:current") < owed:
            raise Refused("not enough in the current account")
        self._pay_loan(loan, owed, self.day, self.minute, "Loan %d repaid early" % loan["id"])

    # ---------------------------------------------------------------- shops (ECO-005, ECO-010, ECO-011)
    def shop_kind(self, n):
        l = self.lots[n]
        for k, S in self.R["shops"].items():
            m = S["match"]
            if l["kind"] == m["kind"] and m.get("name_has", "") in (l.get("name") or ""):
                return k
        return None

    def restock(self, day=None, minute=None):
        """The day of the latest restock at or before (day, minute)."""
        day = self.day if day is None else day
        minute = self.minute if minute is None else minute
        return day if minute >= self.R["stock"]["restock_minute"] else day - 1

    def resident_buys(self, n, item, day=None, minute=None):
        """How many of item residents have bought at lot n since the latest restock (from their routine)."""
        day = self.day if day is None else day
        minute = self.minute if minute is None else minute
        S = self.R["shops"][self.shop_kind(n)]["sells"]
        rd, r0 = self.restock(day, minute), self.R["stock"]["restock_minute"]
        spans = [(rd, r0, minute if rd == day else 1440)] + ([(day, 0, minute)] if rd < day else [])
        c = 0
        for d, a, b in spans:                               # visits from a up to and including b
            for t, i in self.visits.get(n, ()):
                if a <= t <= b and S[int(hash01(i, d, n, "buy") * len(S))] == item:
                    c += 1
        return c

    def stock(self, n, item):
        k = self.shop_kind(n)
        if not k or item not in self.R["shops"][k]["sells"]:
            return 0
        bought, sold = self.trades.get("%d|%d" % (n, self.restock()), {}).get(item, [0, 0])
        return max(0, self.R["stock"]["target"] - self.resident_buys(n, item) - bought + sold)

    def rep_with_lot(self, n):
        reps = [self.rep[r] for r in self.staff.get(n, ()) if r in self.rep]
        return max(reps) if reps else 0

    def factor_bp(self, n, item):
        P, T = self.R["price"], self.R["stock"]["target"]
        f = 10000 + P["slope_bp"] * (T - self.stock(n, item)) // T
        return max(P["min_bp"], min(P["max_bp"], f))

    def price(self, n, item):
        """What the shop at lot n asks the player for item now, in chips."""
        Q = self.R["reputation"]
        r = self.rep_with_lot(n)
        mod = 10000 - Q["discount_bp_at_max"] * r // Q["max"] if r >= 0 else 10000 + Q["markup_bp_at_min"] * r // Q["min"]
        return max(1, (self.items[item]["price"] * self.factor_bp(n, item) * mod + 50000000) // 100000000)

    def offer(self, n, item):
        """What the shop at lot n pays the player for item now (ECO-011); 0 when it does not buy it."""
        k = self.shop_kind(n)
        I = self.items.get(item)
        if not k or not I or not I["sell"] or item not in self.R["shops"][k]["buys"]:
            return 0
        f = self.factor_bp(n, item) if item in self.R["shops"][k]["sells"] else 10000
        return I["price"] * f * self.R["price"]["sell_share_bp"] // 100000000

    def _trade(self, n, item, k):
        t = self.trades.setdefault("%d|%d" % (n, self.restock()), {}).setdefault(item, [0, 0])
        t[k] += 1

    def buy(self, n, item):
        k = self.shop_kind(n)
        if n in self.outlets:                               # ECO-013: a mall with outlets sells at each of them, not as one shop
            raise Refused("buy at one of its outlets")
        if not k or item not in self.R["shops"][k]["sells"]:
            raise Refused("this shop does not sell that")
        if self.stock(n, item) < 1:
            raise Refused("sold out")
        p = self.price(n, item)
        self.spend(p, "till:%s" % self.lots[n]["id"], "%s, %s" % (self.items[item]["name"], self.lots[n]["name"]))
        self._trade(n, item, 0)
        self.give_item(item)
        return p

    def sell(self, n, item):
        p = self.offer(n, item)
        if p <= 0:
            raise Refused("this shop does not buy that")
        if self.inv.get(item, 0) < 1:
            raise Refused("you have none")
        till = "till:%s" % self.lots[n]["id"]
        if self.L.balance(till) < p:
            raise Refused("the till is empty")
        self.L.post(self.day, self.minute, "Sold %s, %s" % (self.items[item]["name"].lower(), self.lots[n]["name"]), [(till, -p), ("p:cash", p)])
        self.take_item(item)
        if item in self.R["shops"][self.shop_kind(n)]["sells"]:
            self._trade(n, item, 1)
        return p

    # ---------------------------------------------------------------- a mall's outlets (ECO-013, ECO-014)
    def good(self, g):
        """A good an outlet sells: {name, price, goes} and, by where it goes, its `piece` or `parts`. An id that is one
        of the town's items (bread, the novel) is carried, or eaten when no shop buys it back."""
        G = self.G["goods"].get(g)
        if G:
            return G
        I = self.R["items"][g]
        return dict(name=I["name"], price=I["price"], goes="carry" if I["sell"] else "eat")

    def outlet_sells(self, n, u):
        """What outlet u of lot n sells, in the catalogue's order: per_outlet goods of its kind's list, the ones a fixed
        hash of the outlet and the good puts first, so two outlets of a kind differ and one outlet never changes."""
        L = self.G["sells"].get(self.outlets[n][u]["kind"], [])
        k = self.G["per_outlet"]
        if len(L) <= k:
            return list(L)
        keep = set(sorted(L, key=lambda g: (hash01(u, g, "range"), g))[:k])
        return [g for g in L if g in keep]

    def outlet_resident_buys(self, n, u, day=None, minute=None):
        """{good: how many} residents have bought at outlet u since the latest restock: each of their visits to the mall
        (from their routine) is one thing bought at one outlet, both chosen by a fixed hash (ECO-010, per outlet)."""
        day = self.day if day is None else day
        minute = self.minute if minute is None else minute
        ids = list(self.outlets[n])
        S = self.outlet_sells(n, u)
        rd, r0 = self.restock(day, minute), self.R["stock"]["restock_minute"]
        spans = [(rd, r0, minute if rd == day else 1440)] + ([(day, 0, minute)] if rd < day else [])
        c = {}
        if not S:
            return c
        for d, a, b in spans:
            for t, i in self.visits.get(n, ()):
                if a <= t <= b and ids[int(hash01(i, d, n, "outlet") * len(ids))] == u:
                    g = S[int(hash01(i, d, u, "buy") * len(S))]
                    c[g] = c.get(g, 0) + 1
        return c

    def outlet_stock(self, n, u, g):
        if n not in self.outlets or u not in self.outlets[n] or g not in self.outlet_sells(n, u):
            return 0
        bought = self.trades.get("%d|%d|%s" % (n, self.restock(), u), {}).get(g, [0, 0])[0]
        return max(0, self.G["stock"]["target"] - self.outlet_resident_buys(n, u).get(g, 0) - bought)

    def outlet_price(self, n, u, g):
        """What outlet u asks for good g now, in chips: its usual price by its stock and by what the mall's staff think of
        the player, exactly as a street shop's (ECO-005, QST-007)."""
        P, T, Q = self.R["price"], self.G["stock"]["target"], self.R["reputation"]
        f = max(P["min_bp"], min(P["max_bp"], 10000 + P["slope_bp"] * (T - self.outlet_stock(n, u, g)) // T))
        r = self.rep_with_lot(n)
        mod = 10000 - Q["discount_bp_at_max"] * r // Q["max"] if r >= 0 else 10000 + Q["markup_bp_at_min"] * r // Q["min"]
        return max(1, (self.good(g)["price"] * f * mod + 50000000) // 100000000)

    def buy_at(self, n, u, g):
        """Buy good g at outlet u of lot n (ECO-013). Paid into the mall's till through the ledger; then the good goes
        where its kind goes (ECO-014): carried, to the wardrobe, or to the belongings as a piece for the home."""
        if n not in self.outlets or u not in self.outlets[n]:
            raise Refused("no such outlet")
        if g not in self.outlet_sells(n, u):
            raise Refused("this outlet does not sell that")
        if self.outlet_stock(n, u, g) < 1:
            raise Refused("sold out")
        G, p = self.good(g), self.outlet_price(n, u, g)
        self.spend(p, "till:%s" % self.lots[n]["id"], "%s, %s" % (G["name"], self.outlets[n][u]["name"]))
        t = self.trades.setdefault("%d|%d|%s" % (n, self.restock(), u), {}).setdefault(g, [0, 0])
        t[0] += 1
        if G["goes"] == "wear":
            self.clothes[g] = self.clothes.get(g, 0) + 1
        elif G["goes"] == "home":
            self.homeware[g] = self.homeware.get(g, 0) + 1
        else:
            self.give_item(g)
        return p

    # ---------------------------------------------------------------- the skate store (CITY-102, CITY-103)
    def board_price(self, kind):
        """What the skate store asks for a kind of board, in chips (boards.json): the same at every skate store, whoever buys."""
        return self.B["kinds"][kind]["price"]

    def buy_board(self, n, kind):
        """Buy a skateboard at the skate store at lot n, paid into its till through the ledger (cash first, then the current
        account). A kind already owned is not sold twice; the board is the player's from then on (CITY-100)."""
        if self.shop_kind(n) != "skate":
            raise Refused("this shop does not sell skateboards")
        if not self.B or kind not in self.B["kinds"]:
            raise Refused("no such board")
        if kind in self.boards:
            raise Refused("you have that board already")
        p = self.board_price(kind)
        self.spend(p, "till:%s" % self.lots[n]["id"], "%s, %s" % (self.B["kinds"][kind]["name"], self.lots[n]["name"]))
        self.boards = [k for k in self.B["order"] if k in self.boards or k == kind]
        return p

    # ---------------------------------------------------------------- the market stall (PROP-008)
    def rent_stall(self):
        if self.stall:
            raise Refused("you already have a stall today")
        self.spend(self.R["stall"]["rent"], "sys:fees", "Market stall rent")
        self.stall = dict(day=self.day, slots=[], done=self.minute)

    def set_out(self, item, price):
        S = self.R["stall"]
        I = self.items.get(item)
        if not self.stall:
            raise Refused("rent a stall first")
        if not I or I.get("quest") or I["price"] <= 0:
            raise Refused("that cannot be sold here")
        if len(self.stall["slots"]) >= S["slots"]:
            raise Refused("the stall is full")
        if type(price) is not int or not 1 <= price <= I["price"] * S["max_price_factor_bp"] // 10000:
            raise Refused("that price is not allowed")
        self.take_item(item)
        self.stall["slots"].append(dict(item=item, price=price, sold=False))

    def take_back(self, k):
        s = self.stall and k < len(self.stall["slots"]) and self.stall["slots"][k]
        if not s or s["sold"]:
            raise Refused("nothing there")
        self.stall["slots"].pop(k)
        self.give_item(s["item"])

    def stall_chance_bp(self, item, price):
        S, base = self.R["stall"], self.items[item]["price"]
        e = S["elasticity"]
        return min(9500, S["buy_bp"] * base ** e // price ** e)

    def run_stall(self, day, minute):
        """Residents who came to the market since last time, while the stall was open, look at what is set out."""
        st, S = self.stall, self.R["stall"]
        a, b = max(st["done"], S["open"][0]), min(minute, S["open"][1])
        for t, i in self.visits.get(self.market_lot, ()):
            if not a < t <= b:
                continue
            for k, s in enumerate(st["slots"]):
                if s["sold"]:
                    continue
                if hash01(i, day, "stall", k, s["item"]) * 10000 < self.stall_chance_bp(s["item"], s["price"]):
                    s["sold"] = True
                    self.stall_sold_total += 1
                    self.L.post(day, t, "Stall: %s sold" % self.items[s["item"]]["name"].lower(), [("sys:residents", -s["price"]), ("p:cash", s["price"])])
                    break                                   # one thing each
        st["done"] = max(st["done"], minute)

    # ---------------------------------------------------------------- saving (QST-008)
    def save(self):
        return copy.deepcopy(dict(tx=self.L.tx, day=self.day, minute=self.minute, settled=self.settled, inv=self.inv, accounts=self.accounts,
                                  welcomed=self.welcomed, loans=self.loans, barred=self.barred, trades=self.trades, rep=self.rep, stall=self.stall,
                                  stall_sold_total=self.stall_sold_total, clothes=self.clothes, homeware=self.homeware, boards=self.boards))

    def load(self, s):
        s = copy.deepcopy(s)
        self.L = Ledger()
        for no, day, minute, memo, lines in s.pop("tx"):    # balances are worked out again from the ledger itself
            self.L.post(day, minute, memo, [tuple(x) for x in lines])
        for k, v in s.items():
            setattr(self, k, v)


def board_in_use(boards, in_use, bought):
    """CITY-103: the board in use after buying one: the bought board when it is faster than the one in use (or none was),
    else the one in use stays."""
    if not in_use or boards["kinds"][bought]["top_kmh"] > boards["kinds"][in_use]["top_kmh"]:
        return bought
    return in_use


def board_script():
    """A fixed run of buying skateboards (CITY-102, CITY-103) whose results the Unreal port must reproduce: too little money,
    a board bought with cash and the current account, the same kind again, a kind that is not sold, a shop that is no skate
    store, a loan for a faster board bought while a slower one is in use, and the dearest without the money for it."""
    return [
        ["advance", 0, 440], ["new"], ["board", "skate", "manual"], ["open", "current"], ["board", "skate", "manual"],
        ["board", "skate", "manual"], ["board", "skate", "skateboard"], ["board", "grocer", "electric"],
        ["loan", 30000, 7], ["board", "skate", "electric"], ["advance", 2, 600], ["board", "skate", "neon_boost"],
    ]


def run_boards(econ, ops, shops):
    """Runs board_script() ops (as run_script(), with "board": shop kind, board kind). Returns a summary to compare."""
    out, in_use, uses = [], None, []
    for op in ops:
        name, a = op[0], op[1:]
        try:
            if name == "new":
                econ.new_character()
            elif name == "open":
                econ.open_account(a[0])
            elif name == "advance":
                econ.advance(a[0], a[1])
            elif name == "loan":
                econ.take_loan(a[0], a[1])
            elif name == "board":
                econ.buy_board(shops[a[0]], a[1])
                in_use = board_in_use(econ.B, in_use, a[1])
            out.append("ok")
        except Refused:
            out.append("refused")
        uses.append(in_use)
    till = "till:%s" % econ.lots[shops["skate"]]["id"]
    return dict(results=out, boards=list(econ.boards), uses=uses, balances={a: econ.L.balance(a) for a in PLAYER + (till,)},
                tx=len(econ.L.tx), supply=econ.L.supply(), memos=[t[3] for t in econ.L.tx[-3:]])


def check_script():
    """A fixed run of the rules whose results the browser port must reproduce exactly (packed by build_play.py).
    Operations: [name, args...]; the port runs the same list against the same data."""
    return [
        ["advance", 0, 440], ["new"], ["buy", "grocer", "bread"], ["buy", "grocer", "bread"], ["sell", "grocer", "bread"],
        ["open", "current"], ["open", "savings"], ["move", "p:current", "p:savings", 2000],
        ["advance", 0, 1200], ["advance", 1, 600], ["loan", 5000, 3], ["buy", "arcade", "hat"], ["buy", "kiosk", "map"],
        ["rent"], ["setout", "map", 250], ["setout", "hat", 2600], ["advance", 1, 1000],
        ["advance", 4, 700], ["move", "p:savings", "p:cash", 500], ["advance", 6, 400],
    ]


def outlet_script(econ):
    """More of the fixed run, for a town whose mall has outlets (ECO-013): at the first outlet of each of five kinds (food,
    clothes, music, things for the home), buy the first thing of each way of going that it sells, then try something
    it does not sell, and last an outlet that is not there. Appended to check_script()."""
    ops = []
    for n in sorted(econ.outlets):
        seen = set()
        for u, U in econ.outlets[n].items():
            if U["kind"] in seen or U["kind"] not in ("burger", "cafe", "clothes", "music", "home"):
                continue
            seen.add(U["kind"])
            done = set()
            for g in econ.outlet_sells(n, u):
                goes = econ.good(g)["goes"]
                if goes not in done:
                    done.add(goes)
                    ops.append(["buyat", n, u, g])
            ops.append(["buyat", n, u, "newspaper"])
        ops.append(["buyat", n, "no_such_unit", "bread"])
        break
    return ops


def run_script(econ, ops, shops):
    """Runs check_script() ops. shops: {shop kind: lot index} to use for each kind. Returns a summary to compare."""
    out = []
    for op in ops:
        name, a = op[0], op[1:]
        try:
            if name == "new":
                econ.new_character()
            elif name == "buy":
                econ.buy(shops[a[0]], a[1])
            elif name == "sell":
                econ.sell(shops[a[0]], a[1])
            elif name == "open":
                econ.open_account(a[0])
            elif name == "move":
                econ.move(a[0], a[1], a[2])
            elif name == "advance":
                econ.advance(a[0], a[1])
            elif name == "loan":
                econ.take_loan(a[0], a[1])
            elif name == "rent":
                econ.rent_stall()
            elif name == "setout":
                econ.set_out(a[0], a[1])
            elif name == "buyat":
                econ.buy_at(a[0], a[1], a[2])
            out.append("ok")
        except Refused as e:
            out.append("refused")
    return dict(results=out, balances={a: econ.L.balance(a) for a in PLAYER + ("bank:vault", "sys:fees", "sys:residents")},
                tx=len(econ.L.tx), supply=econ.L.supply(), inv=dict(sorted(econ.inv.items())), sold=econ.stall_sold_total,
                loans=[[l["status"], l["paid"], l["fees"], l["missed"]] for l in econ.loans],
                prices={k: econ.price(n, econ.R["shops"][k]["sells"][0]) for k, n in sorted(shops.items())},
                clothes=dict(sorted(econ.clothes.items())), homeware=dict(sorted(econ.homeware.items())),
                outlets=[[n, u, econ.outlet_sells(n, u), [econ.outlet_stock(n, u, g) for g in econ.outlet_sells(n, u)],
                          [econ.outlet_price(n, u, g) for g in econ.outlet_sells(n, u)]] for n in sorted(econ.outlets) for u in list(econ.outlets[n])[:12]],
                hashes=[round(hash01(*x), 12) for x in (("a",), (1, 2, "b"), ("r0504", 16, "job"), (523, 3, 117, "buy"))])
