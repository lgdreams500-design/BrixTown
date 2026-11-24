"use strict";
// Achievements (QST-009, F-UI-REWARD level 2): named moments in the player's life, earned once and kept. A port of
// Tools/citysim/achievements.py, rule for rule: the facts are worked out from the ledger, the quests, the loans and the
// property market, never counted up as the game runs; only which were earned, and on which day, is kept (in the save).
// Achievements.selfTest() runs the fixed facts build_play.py ran in Python and compares (D.life.achievements.checks).
// The banner and the fanfare are rewards.js and sound.js (SND-021). Loaded after economy.js and before life.js.
var Achievements = (() => {
  const R = () => window.WASTED.life.achievements;
  function facts(econ, quests, market) {
    const memo = R().job_memo;
    return {
      quests_done: Object.keys(quests.done).length,
      jobs_paid: econ.L.tx.filter(t => t[3].startsWith(memo) && t[4].some(([a, v]) => a.startsWith("p:") && v > 0)).length,
      savings: econ.L.balance("p:savings"),
      loans_repaid: econ.loans.filter(l => l.status === "repaid").length,
      stall_sold: econ.stall_sold_total,
      homes_rented: market ? Object.keys(market.tenancy).length : 0,
      homes_owned: market ? market.owned().length : 0,
      money: econ.totalMoney(),
    };
  }
  const newly = (f, have) => R().list.filter(a => !(a.id in have) && f[a.when.fact] >= a.when.at_least).map(a => a.id);
  function award(f, have, day) { const got = newly(f, have); for (const i of got) have[i] = day; return got; }
  function selfTest() {
    const K = R() && R().checks; if (!K) return ["no achievement checks in city_data.js"];
    const have = {}, got = K.steps.map((f, day) => award(f, have, day));
    return JSON.stringify([got, have]) === JSON.stringify([K.want, K.have]) ? [] : ["achievements differ from Python: " + JSON.stringify([got, have]) + " not " + JSON.stringify([K.want, K.have])];
  }
  return { facts, newly, award, selfTest, get: id => R().list.find(a => a.id === id), list: () => R().list, rules: R };
})();
