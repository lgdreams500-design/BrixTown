"use strict";
// People and the weather (F-NPC-REACT level 1, CITY-077 to CITY-080; ADR-0020). citysim decides it all: what each resident wears and
// carries today (D.wear, from World/rules/weather_react.json), and in rain who runs for cover, where to and how fast (already in
// their day, D.plan). This file only shows it: a coat over the role's clothes, umbrellas and hoods up out in the rain, a hand held
// over the head by those with neither, a hurried walk in the wet and a run for cover. Loaded before play.js; play.js's globals
// (pieceAt, lotAt, weatherNow, wxTry) are only touched from Wet.frame and Wet.apply.
var Wet = (() => {
  const D = window.WASTED, W = D.wear || null, SHELTER = D.acts.indexOf("shelter"), cache = new Map();
  const NONE = { L: null };
  let rain = 0, trying = false;
  function frame(m) {                                    // once a frame: how hard it is raining now (the weather key K included)
    if (!W) return;
    const wx = weatherNow(m); rain = wx.rain; trying = wxTry === "rain" && !(D.day.weather || []).includes("rain");
  }
  function wearing(i) { return (trying ? W.wet : W.today)[i]; }   // trying rain on a dry day: what they would have worn on a wet one
  function look(i, L) {                                  // the look with today's coat on, if they wear one
    if (!W) return L;
    const w = wearing(i); if (w[0] < 0 || L.g === "coat" && L.top === People.colour(W.colours[w[0]])) return L;
    const key = i * 2 + (trying ? 1 : 0); let v = cache.get(key);
    if (!v || v.base !== L) { v = Object.assign({}, L, { g: "coat", top: People.colour(W.colours[w[0]]), scarf: L.scarf, base: L }); cache.set(key, v); }
    return v;
  }
  // r: where resident i is now (play.js resident()). Returns { L, sig, mod, hood, umb } for People.draw.
  function apply(i, r, L0, m) {
    if (!W) return NONE;
    const L = look(i, L0), w = wearing(i), out = { L };
    if (rain < .25 || r.inside || r.act === SHELTER || r.pose === 3 || (r.y < 6 && lotAt(r.x, r.z))) return out;   // dry, indoors, under an awning or a roof
    if (w[2] >= 0) { out.umb = People.colour(W.colours[w[2]]); out.mod = { arms: [null, [1, 52, 12, 84]] }; }
    else if (w[1]) out.hood = People.colour(W.colours[w[0]]);
    if (!r.walking || r.sig) return out;                   // standing or sitting out in the rain; or already running from a car (dodge.js)
    const row = D.plan[i], k = pieceAt(row, m), p = row[k];
    if (p[1] !== 1 || !p.c) return out;
    const t1 = k + 1 < row.length ? row[k + 1][0] : 1440, len = p.c[p.c.length - 1], own = D.signatures[D.role_sig[D.roles[D.residents[i].r]]] * D.pace;
    const ratio = len / Math.max(t1 - p[0], 1e-6) / own, d = Math.min(1, Math.max(0, (m - p[0]) / Math.max(t1 - p[0], 1e-6))) * len;
    const sig = ratio > (W.pace.hurry + W.pace.run) / 2 ? People.SIG.Run : out.umb ? null : People.SIG.Hurried;   // running for cover, or hurrying in the wet without an umbrella
    if (sig) { out.sig = sig; out.ph = (d / People.stepLength(sig, L.S)) % 1; }
    if (!out.umb && !out.hood) out.mod = { arms: [[1, 150, 30, 118], null], chest: [6, 0] };   // a hand over the head
    return out;
  }
  return { frame, apply, look, on: () => !!W };
})();
