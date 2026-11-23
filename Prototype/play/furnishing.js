"use strict";
// How furnished each home in the slice is (FURN-062, FURN-063; F-HOME-FURNISH level 3; ADR-0033): 1 Bare, 2 Basic,
// 3 Homely, 4 Full. A port of Tools/citysim/furnishing.py; the rule itself lives in estate.js (Estate.furnish) so the
// market's daily furniture transaction and what a house shows come from the same lines. Worked out from
// World/rules/furnishing.json (packed in city_data.js as D.life.estate.furnishing), the household's daily wages and the
// clock day, never ticked or stored. Load after estate.js. No DOM and none of play.js's globals here.
//
//   Furnishing.level(lotId, flatIndexOrNull, day)  -> 1..4
//     lotId: a lot id ("b_3_4_l2"); flatIndexOrNull: null for a building that is one home (a house), or the index of
//     the unit among the lot's units in floor order (0 = the first flat); day: the clock day (today if omitted).
//   Furnishing.unitOf(lotId, flatIndexOrNull)       -> the estate's unit index, or null if the lot has no such unit
//   Furnishing.levelOfUnit(unitIndex, day)          -> 1..4
//   Furnishing.name(level)                          -> "Bare" | "Basic" | "Homely" | "Full"
//   Furnishing.forWage(wage, day)                   -> 1..4, the rule on its own
//   Furnishing.use(market)                          -> uses this Estate.Market (default: window.wasted.life.M)
var Furnishing = (() => {
  let bound = null;
  const NAMES = ["Bare", "Basic", "Homely", "Full"];
  const market = () => bound || (window.wasted && window.wasted.life && window.wasted.life.M) || null;
  const rules = () => { const M = market(); return M ? M.F : null; };

  function use(m) { bound = m; }
  let lotIx = null, lotIxOf = null;                          // lot id -> the market's lot index, made once per market
  function unitOf(lotId, flat) {
    const M = market(); if (!M) return null;
    if (lotIxOf !== M) { lotIx = new Map(M.lots.map((l, k) => [l.id, k])); lotIxOf = M; }
    const lot = lotIx.has(lotId) ? lotIx.get(lotId) : -1; if (lot < 0) return null;
    const ix = M.byLot.get(lot) || [];
    if (flat === null || flat === undefined) return ix.length ? ix[0] : null;
    return flat >= 0 && flat < ix.length ? ix[flat] : null;
  }
  function levelOfUnit(i, day) {
    const M = market(); if (!M || i === null || i === undefined) return 1;
    return M.levelOf(i, day === undefined || day === null ? M.E.day : day);
  }
  // Before a game starts there is no market yet (the title screen, the look bench): a house's level on day 0 from its
  // residents' wages, the same rule (estate.js Estate.furnish) the market will use once it exists
  let early = null;
  function earlyLevel(lotId) {
    const D = window.WASTED, F = D && D.life && D.life.estate && D.life.estate.furnishing;
    if (!F || typeof Estate === "undefined" || !Estate.furnish) return null;
    if (!early) {
      const W = D.life.estate.town.wages, lots = D.blocks.flatMap(b => b.lots), wage = new Map();
      for (const r of D.residents) { const id = lots[r.h] && lots[r.h].id; if (id) wage.set(id, (wage.get(id) || 0) + (W[D.roles[r.r]] || 0)); }
      early = wage;
    }
    return early.has(lotId) ? Estate.furnish.level(F, early.get(lotId), 0) : F.empty_level;
  }
  function level(lotId, flat, day) {
    if (!market() && (flat === null || flat === undefined)) { const e = earlyLevel(lotId); if (e !== null) return e; }
    const F = rules(), i = unitOf(lotId, flat);
    if (i === null) return F ? F.empty_level : 1;
    return levelOfUnit(i, day);
  }
  function forWage(wage, day) { const F = rules(); return F ? Estate.furnish.level(F, wage, day) : 1; }
  function name(lv) { const F = rules(); return (F ? F.levels : NAMES)[lv - 1]; }
  return { use, unitOf, level, levelOfUnit, forWage, name, NAMES };
})();
window.Furnishing = Furnishing;
