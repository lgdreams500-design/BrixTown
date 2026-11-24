// Skates (F-VEH-SKATES; LIVING_CITY CITY-098 to CITY-101): which pairs the player owns, which pair they use, whether it is carried
// in the hand or worn, and how fast they are going. Kinds, speeds and colours are data in World/rules/skates.json (city_data.js).
// The skate store and buying (CITY-102, CITY-103) come later; until then only ?auto=1 gives skates (all three, electric in use).
const Skates = (() => {
  const R = window.WASTED.skates, MS = 1 / 3.6;           // km/h to m/s
  let own = [], kind = null, worn = false, v = 0;
  const K = () => kind ? Object.assign({ id: kind, boost: kind === "neon_boost" }, R.kinds[kind]) : null;
  // CITY-099: the speed after dt seconds. Pushing speeds up to the kind's top speed (a lower one indoors), letting go glides to a
  // stop, pulling back brakes; going faster than allowed (just come indoors) brakes down to it.
  function step(dt, push, back, indoor) {
    const k = R.kinds[kind], top = (indoor ? Math.min(R.indoor_top_kmh, k.top_kmh) : k.top_kmh) * MS;
    if (v > top) v = Math.max(top, v - k.brake_kmh_s * MS * dt);
    else if (push && !back) v = Math.min(top, v + k.accel_kmh_s * MS * dt);
    else v = Math.max(0, v - (back ? k.brake_kmh_s : k.glide_kmh_s) * MS * dt);
    return v;
  }
  return {
    R, kind: K, owned: () => own.slice(), get worn() { return worn && !!kind; }, get v() { return v; }, step,
    give(list, use) { own = R.order.filter(k => list.includes(k)); kind = own.includes(use) ? use : own[own.length - 1] || null; worn = false; v = 0; },
    use(k) { if (own.includes(k)) { kind = k; v = Math.min(v, R.kinds[k].top_kmh * MS); } },
    toggle() { if (!kind) return null; worn = !worn; v = 0; return worn; },   // Y: from the hand onto the feet and back
    slow(f) { v *= f; },                                   // a knock against a wall or a car
    stop() { v = 0; },
    save: () => ({ own: own.slice(), kind, worn }),
    load(s) { own = R.order.filter(k => s && (s.own || []).includes(k)); kind = s && own.includes(s.kind) ? s.kind : own[own.length - 1] || null; worn = !!(s && s.worn && kind); v = 0; },
  };
})();
