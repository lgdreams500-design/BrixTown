// What the slice draws of the road network: zebra crossings, parking bays and traffic lights (CITY-043, CITY-045,
// CITY-046). The places come from World/city_01/roads.json through build_play.py (D.roads); the lights run on the same
// cycle as Tools/citysim/traffic.py, so a car in the slice stops on the red it was simulated to stop on.
//
// Loaded before play.js. Roads.build(mesh, Y, cell) lays the static paint, bays and poles of one block square into the world mesh;
// Roads.draw(dyn, minute, near, reach) lights each signal head's current lamp every frame. Material 23 is a lamp lit in
// its own colour (play.js shader).
const Roads = (() => {
  const D = window.WASTED, R = D.roads || { crossings: [], bays: [], signals: [], cycle: { green_s: 20, amber_s: 3, all_red_s: 2 }, width: 3, bay_length: 6 };
  const SW = D.city.grid.street_width, HALF = SW / 2 - 4.5;      // the carriageway either side of a centre line (play.js PAVE 4.5)
  const PAINT = [.86, .84, .78], LINE = [.8, .78, .72];
  const LAMP = { red: [1, .12, .08], amber: [1, .6, .1], green: [.25, 1, .45] }, DARK = [.09, .09, .1];
  const cyc = R.cycle, CYCLE = 2 * (cyc.green_s + cyc.amber_s + cyc.all_red_s);
  const heads = [];                                              // [x, y, z, axis, signal index, face x, face z]

  function phase(k, axis, minute) {                              // traffic.py signal(): "green", "amber" or "red"
    const g = cyc.green_s, a = cyc.amber_s, red = cyc.all_red_s;
    let tt = (((D.day.index * 1440 + minute) * D.pace + R.signals[k][2]) % CYCLE + CYCLE) % CYCLE;
    const first = tt < g + a + red ? "ns" : "ew";
    if (first === "ew") tt -= g + a + red;
    return axis === first && tt < g ? "green" : axis === first && tt < g + a ? "amber" : "red";
  }

  const P = D.city.grid.pitch, cellOf = (x, z) => Math.floor(x / P) + "," + Math.floor(z / P);
  function cells() {                                             // block squares that hold any of it, so play.js can cull by square
    const k = new Set();
    for (const [x, z] of R.crossings.concat(R.bays)) k.add(cellOf(x, z));
    for (const [X, Z] of R.signals) for (const [dx, dz] of [[-11, 3.95], [11, -3.95], [-3.95, -11], [3.95, 11]]) k.add(cellOf(X + dx, Z + dz));
    return [...k];
  }

  function build(world, Y, cell) {                               // everything of the road network in one block square
    const y = Y + .006, w = R.width / 2;
    for (const [x, z, ew] of R.crossings) {
      if (cellOf(x, z) !== cell) continue;                      // zebra: bars along the traffic, spaced across the road
      for (let s = -HALF + .25; s < HALF - .2; s += 1.0) {
        if (ew) world.flat(x - w, z + s, x + w, z + s + .5, .012, PAINT, 9);
        else world.flat(x + s, z - w, x + s + .5, z + w, .012, PAINT, 9);
      }
    }
    const L = R.bay_length / 2, d0 = HALF, d1 = HALF + 2.4;      // a bay: road setts in the pavement edge, white ends and back line
    for (const [x, z, alongX] of R.bays) {
      if (cellOf(x, z) !== cell) continue;
      const side = alongX ? Math.sign(z - Math.round(z / D.city.grid.pitch) * D.city.grid.pitch) : Math.sign(x - Math.round(x / D.city.grid.pitch) * D.city.grid.pitch);
      const c = alongX ? Math.round(z / D.city.grid.pitch) * D.city.grid.pitch : Math.round(x / D.city.grid.pitch) * D.city.grid.pitch;
      const a = c + side * d0, b = c + side * d1, lo = Math.min(a, b), hi = Math.max(a, b);
      if (alongX) {
        world.flat(x - L, lo, x + L, hi, y, [.36, .34, .34], 20);
        for (const e of [-L, L - .1]) world.flat(x + e, lo, x + e + .1, hi, y + .002, LINE, 9);
        world.flat(x - L, side > 0 ? hi - .1 : lo, x + L, side > 0 ? hi : lo + .1, y + .002, LINE, 9);
      } else {
        world.flat(lo, z - L, hi, z + L, y, [.36, .34, .34], 20);
        for (const e of [-L, L - .1]) world.flat(lo, z + e, hi, z + e + .1, y + .002, LINE, 9);
        world.flat(side > 0 ? hi - .1 : lo, z - L, side > 0 ? hi : lo + .1, z + L, y + .002, LINE, 9);
      }
    }
    const metal = [.17, .18, .21], k0 = 12 - 1, kerb = HALF + .45;  // a pole on the right-hand kerb just before each stop line
    R.signals.forEach(([X, Z], k) => {
      for (const [ux, uz] of [[1, 0], [-1, 0], [0, 1], [0, -1]]) {   // heading of the traffic the head faces
        const px = X - ux * k0 - uz * kerb, pz = Z - uz * k0 + ux * kerb;   // right of heading (ux, uz) is (-uz, ux)
        if (cellOf(px, pz) !== cell) continue;
        const axis = ux !== 0 ? "ew" : "ns";
        world.box(px - .07, Y, pz - .07, px + .07, 3.3, pz + .07, metal, 9);
        world.box(px - .22, 2.25, pz - .22, px + .22, 3.4, pz + .22, metal, 9);
        const fx = -ux * .23, fz = -uz * .23, tx = Math.abs(ux) ? .1 : .13, tz = Math.abs(uz) ? .1 : .13;   // the face toward oncoming cars
        for (const h of [3.15, 2.83, 2.51]) world.box(px + fx - tx, h - .13, pz + fz - tz, px + fx + tx, h + .13, pz + fz + tz, DARK, 9);
        heads.push([px - ux * .35, 0, pz - uz * .35, axis, k, ux, uz]);   // just proud of the head's face
      }
    });
  }

  function draw(dyn, minute, near, reach) {
    let lit = 0;
    const r2 = (reach || 220) * (reach || 220);
    for (const [x, , z, axis, k, ux, uz] of heads) {
      if (near && (x - near[0]) * (x - near[0]) + (z - near[1]) * (z - near[1]) > r2) continue;
      const p = phase(k, axis, minute), h = p === "red" ? 3.15 : p === "amber" ? 2.83 : 2.51;
      const ex = Math.abs(ux) ? .025 : .1, ez = Math.abs(uz) ? .025 : .1;   // thin toward the traffic, wide across it
      dyn.box(x - ex, h - .09, z - ez, x + ex, h + .09, z + ez, LAMP[p], 23);
      lit++;
    }
    return lit;
  }

  return { cells, build, draw, phase, data: R };
})();
