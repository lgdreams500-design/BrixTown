// Skateboards (F-VEH-SKATES; LIVING_CITY CITY-098 to CITY-101): which boards the player owns, the board in use, whether it is
// carried in the hand or ridden, and how fast it is going. Kinds, speeds, sizes and colours are data in World/rules/boards.json.
// A manual board goes only while a foot pushes it (W held); the electric and Neon Boost boards drive themselves while W is held.
// The skate store and buying (CITY-102, CITY-103) come later; until then only ?auto=1 gives boards (all three, electric in use).
const Boards = (() => {
  const R = window.WASTED.boards, MS = 1 / 3.6;           // km/h to m/s
  let own = [], kind = null, riding = false, v = 0;
  const K = () => kind ? Object.assign({ id: kind }, R.kinds[kind]) : null;
  // CITY-099: the speed after dt seconds. W speeds up to the kind's top speed (a lower one indoors), letting go glides to a stop,
  // pulling back brakes; going faster than allowed (just come indoors) brakes down to it.
  function step(dt, go, back, indoor) {
    const k = R.kinds[kind], top = (indoor ? Math.min(R.indoor_top_kmh, k.top_kmh) : k.top_kmh) * MS;
    if (v > top) v = Math.max(top, v - k.brake_kmh_s * MS * dt);
    else if (go && !back) v = Math.min(top, v + k.accel_kmh_s * MS * dt);
    else v = Math.max(0, v - (back ? k.brake_kmh_s : k.glide_kmh_s) * MS * dt);
    return v;
  }
  return {
    R, kind: K, owned: () => own.slice(), get riding() { return riding && !!kind; }, get v() { return v; }, step,
    give(list, use) { own = R.order.filter(k => list.includes(k)); kind = own.includes(use) ? use : own[own.length - 1] || null; riding = false; v = 0; },
    use(k) { if (own.includes(k)) { kind = k; v = Math.min(v, R.kinds[k].top_kmh * MS); } },
    toggle() { if (!kind) return null; riding = !riding; v = 0; return riding; },   // Y: from the hand onto the board and back
    slow(f) { v *= f; },                                   // a knock against a wall or a car
    save: () => ({ own: own.slice(), kind, riding }),
    load(s) { own = R.order.filter(k => s && (s.own || []).includes(k)); kind = s && own.includes(s.kind) ? s.kind : own[own.length - 1] || null; riding = !!(s && s.riding && kind); v = 0; },
  };
})();
