// Residents getting out of the way of a car the player drives (CITY-070), then telling the driver off (CITY-071).
// F-NPC-DODGE level 2. Presented state only (LIVING_CITY §1): an offset and a pose laid over where the resident's day
// puts them, kept for a few seconds near the player and then forgotten. The day itself is never changed.
// Numbers come from World/rules/dodge.json (window.WASTED.dodge).
window.Dodge = (() => {
  const R = (window.WASTED && window.WASTED.dodge) || null;
  const live = new Map();                                // resident -> what they are doing about the car
  let car = null, now = 0;
  const sm = (a, b, v) => { const t = Math.max(0, Math.min(1, (v - a) / (b - a))); return t * t * (3 - 2 * t); };
  const yaw = (dx, dz) => Math.atan2(dx, -dz);
  const rnd = (i, n) => { const s = Math.sin(i * 12.9898 + n * 78.233) * 43758.5453; return s - Math.floor(s); };
  const clear = (x, y, z) => !blocked([x, y + .9, z]) && Math.abs(groundBase(x, z) - y) < .5;   // somewhere to stand, not a wall or a step up

  function frame(t, p) {                                 // once a frame: where the player's car is and how fast it goes
    now = t; car = null;
    if (!R || p.pose !== 4 || !Cars.speed) return;
    const v = Cars.speed(); if (Math.abs(v) < R.min_speed) return;
    const s = Math.sign(v);
    car = { x: p.x, z: p.z, y: p.y, hx: Math.sin(p.yaw) * s, hz: -Math.cos(p.yaw) * s, v: Math.abs(v) };   // p.yaw = yawOf(heading); reversing counts too
  }

  function threat(x, y, z) {                             // is the car heading at someone standing at (x, z)? [along, across] or null
    if (!car || Math.abs(y - car.y) > 1.5) return null;
    const dx = x - car.x, dz = z - car.z, a = dx * car.hx + dz * car.hz, w = -dx * car.hz + dz * car.hx;
    if (a < -.5 || a > R.see_m || a > car.v * R.see_s + R.car_front_m) return null;
    return Math.abs(w) < R.car_half_w_m + R.lane_pad_m ? [a, w] : null;
  }

  function start(i, r, s, w) {                           // seen it: pick the side with room and scramble that way
    const o = s ? s.off() : [0, 0], nx = -car.hz, nz = car.hx, want = R.car_half_w_m + R.lane_pad_m + R.clear_m;
    const first = Math.abs(w) > .15 ? Math.sign(w) : rnd(i, 1) < .5 ? -1 : 1;
    let to = null;
    for (const side of [first, -first]) {
      const d = side * want - w, x = o[0] + nx * d, z = o[1] + nz * d;
      if (clear(r.x + x, r.y, r.z + z)) { to = [x, z]; break; }
    }
    if (!to) { const d = first * want - w; to = [o[0] + nx * d, o[1] + nz * d]; }   // nowhere good: go anyway, as people do
    const n = live.has(i) ? live.get(i).n + 1 : 0;
    const st = {
      t0: now, from: o, to, n, fist: rnd(i, n + 2) < R.fist_share, shout: R.shouts[Math.floor(rnd(i, n + 3) * R.shouts.length)],
      cx: car.x, cz: car.z,
      off() {                                            // the offset now
        const e = now - st.t0, a = R.startle_s, b = a + R.scramble_s, c = b + R.scold_s;
        if (e < a) return st.from;
        if (e < b) { const u = sm(0, 1, (e - a) / R.scramble_s); return [st.from[0] + (st.to[0] - st.from[0]) * u, st.from[1] + (st.to[1] - st.from[1]) * u]; }
        if (e < c) return st.to;
        const u = 1 - sm(0, 1, (e - c) / R.return_s); return [st.to[0] * u, st.to[1] * u];
      },
    };
    live.set(i, st); return st;
  }

  function apply(i, r) {                                 // the resident as drawn: r from their day, with any dodge laid on top
    if (!R || !r) return r;
    let s = live.get(i);
    if (r.inside || r.pose === 2 || r.pose === 3) { if (s) live.delete(i); return r; }
    const total = R.startle_s + R.scramble_s + R.scold_s + R.return_s;
    if (s && now - s.t0 > total) { live.delete(i); s = null; }
    if (car && (!s || now - s.t0 > R.startle_s + R.scramble_s + R.cooldown_s)) {   // not already mid-scramble
      const o = s ? s.off() : [0, 0], h = threat(r.x + o[0], r.y, r.z + o[1]);
      if (h) s = start(i, r, s, h[1]);
    }
    if (!s) return r;
    if (car) { s.cx = car.x; s.cz = car.z; }              // keep an eye on the car while it is about
    const e = now - s.t0, a = R.startle_s, b = a + R.scramble_s, c = b + R.scold_s, o = s.off();
    const x = r.x + o[0], z = r.z + o[1], q = Object.assign({}, r, { x, z, y: groundBase(x, z), dodge: true });
    const atCar = yaw(s.cx - x, s.cz - z);
    if (e < a) {                                          // startle: a flinch towards the car, hands up, leaning back
      const k = sm(0, a, e);
      Object.assign(q, { pose: 0, yaw: atCar, mod: { arms: [[k, 55, 25, 95], [k, 55, 25, 95]], chest: [-9 * k, 0] } });
    } else if (e < b) {                                   // the scramble: quick running steps sideways and a small hop
      const u = (e - a) / R.scramble_s, dx = s.to[0] - s.from[0], dz = s.to[1] - s.from[1];
      q.y += R.hop_m * Math.sin(Math.PI * Math.min(1, u * 1.15));
      Object.assign(q, { pose: 1, walking: true, ph: (e * 2.6 + i * .37) % 1, yaw: yaw(dx, dz), sig: People.SIG.Run,
        mod: { arms: [[.8, 40, 30, 100], [.8, 30, 34, 105]], chest: [10, 0] } });
    } else if (e < c) {                                   // safe: turn on the driver, fist or both arms up, and shout
      const f = (e - b) / R.scold_s, k = sm(0, .12, f) * (1 - sm(.82, 1, f)), shake = Math.sin(now * 15 + i) * 9 * k;
      const arms = s.fist ? [[k * .5, 10, 14, 70], [k, 168 + shake, 8, 62 + shake]] : [[k, 140 + shake, 38, 25], [k, 140 - shake, 38, 25]];
      Object.assign(q, { pose: 0, yaw: atCar, mod: { arms, chest: [s.fist ? 7 * k : -5 * k, 0] }, shout: f < .85 ? s.shout : null });
    } else {                                              // back to where their day has them
      if (Math.hypot(o[0], o[1]) > .05) Object.assign(q, { pose: 1, walking: true, ph: (e * 1.7 + i * .37) % 1, yaw: yaw(-s.to[0], -s.to[1]) });
    }
    return q;
  }

  return { frame, apply, on: () => !!R, live: () => live.size, debug: () => [...live.entries()].map(([i, s]) => ({ i, e: now - s.t0, to: s.to, fist: s.fist, shout: s.shout })) };
})();
