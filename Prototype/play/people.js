/* Wasted: people. Bodies follow character option B "Soft" (docs/art/ART_DIRECTION.md §3): larger head, full eyes, chunky
   limbs, whole garments, solid hair, a silhouette per role. They move by the movement signatures of
   docs/art/CHARACTER_MOTION.md; the walk is a port of Tools/blender/motion_lib.py with the same numbers and rules
   (feet placed first, legs solved to reach them, hips, chest, arms and head layered on top).
   Maths here uses motion_lib's axes: the character faces -Y, +Z is up, +X is the character's left. */
"use strict";
const People = (() => {
  const rad = d => d * Math.PI / 180, TAU = 2 * Math.PI, cl = (v, a, b) => Math.max(a, Math.min(b, v));
  const sm = x => { x = cl(x, 0, 1); return x * x * (3 - 2 * x); };
  const Rx = a => { const c = Math.cos(a), s = Math.sin(a); return [[1, 0, 0], [0, c, -s], [0, s, c]]; };
  const Ry = a => { const c = Math.cos(a), s = Math.sin(a); return [[c, 0, s], [0, 1, 0], [-s, 0, c]]; };
  const Rz = a => { const c = Math.cos(a), s = Math.sin(a); return [[c, -s, 0], [s, c, 0], [0, 0, 1]]; };
  const I3 = [[1, 0, 0], [0, 1, 0], [0, 0, 1]];
  const mm = (a, b) => a.map(r => [r[0] * b[0][0] + r[1] * b[1][0] + r[2] * b[2][0], r[0] * b[0][1] + r[1] * b[1][1] + r[2] * b[2][1], r[0] * b[0][2] + r[1] * b[1][2] + r[2] * b[2][2]]);
  const mv = (a, v) => [a[0][0] * v[0] + a[0][1] * v[1] + a[0][2] * v[2], a[1][0] * v[0] + a[1][1] * v[1] + a[1][2] * v[2], a[2][0] * v[0] + a[2][1] * v[1] + a[2][2] * v[2]];
  const add = (a, b) => [a[0] + b[0], a[1] + b[1], a[2] + b[2]], sub = (a, b) => [a[0] - b[0], a[1] - b[1], a[2] - b[2]], sc = (a, k) => [a[0] * k, a[1] * k, a[2] * k];
  const dot = (a, b) => a[0] * b[0] + a[1] * b[1] + a[2] * b[2], cross = (a, b) => [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]];
  const len = a => Math.hypot(a[0], a[1], a[2]), unit = a => { const l = len(a) || 1; return [a[0] / l, a[1] / l, a[2] / l]; };

  // ---- movement signatures (CHARACTER_MOTION §3). Run is an extrapolation for the player and is not an approved signature.
  const BASE = { cycle: 24, stride: .84, duty: .62, fore: .42, bounce: .024, sway: .018, hip_yaw: 6, hip_roll: 3.5, counter: 1, lean: 3, width: .75, toe_out: 7, lift: .085, strike: 16, toe_off: 42,
    arm_swing: 18, arm_bias: 2, arm_out: 2.5, elbow: 16, elbow_swing: 14, arm_l: 1, arm_r: 1, head_pitch: 0, head_bob: 1.2, head_follow: .25, head_lag: .06 };
  const SIG = {};
  for (const [k, v] of Object.entries({
    Springy: { cycle: 24, stride: .80, bounce: .034, sway: .014, hip_yaw: 7, hip_roll: 4.5, lift: .11, arm_swing: 26, elbow: 22, elbow_swing: 22, lean: 1.5, head_pitch: -4, head_bob: 2.2, toe_off: 50, arm_out: 4.5 },
    Easy: { cycle: 30, stride: .86, bounce: .022, sway: .034, hip_yaw: 9, hip_roll: 5.5, counter: 1.25, lift: .07, arm_swing: 24, arm_bias: -3, elbow: 10, elbow_swing: 8, lean: -1.5, head_pitch: -2, head_bob: 1, toe_out: 13, width: 1, arm_out: 4.5, head_follow: .4 },
    Hurried: { cycle: 20, stride: .86, duty: .60, bounce: .022, sway: .010, hip_yaw: 7, hip_roll: 2.5, lift: .075, arm_swing: 22, arm_bias: 6, elbow: 38, elbow_swing: 18, lean: 9, head_pitch: 3, head_bob: .8, toe_out: 3, width: .55, arm_l: .25, head_follow: .15 },
    Careful: { cycle: 30, stride: .50, duty: .68, bounce: .008, sway: .026, hip_yaw: 3, hip_roll: 2, counter: .6, lift: .045, arm_swing: 7, arm_bias: 4, elbow: 28, elbow_swing: 4, lean: 5, head_pitch: 8, head_bob: .6, toe_out: 10, width: 1, strike: 7, toe_off: 22, arm_out: 3 },
    Run: { cycle: 13, stride: 1.5, duty: .34, fore: .38, bounce: .045, sway: .008, hip_yaw: 10, hip_roll: 3, lift: .2, arm_swing: 38, arm_bias: 8, elbow: 78, elbow_swing: 10, lean: 11, head_pitch: -2, head_bob: 1.5, toe_out: 3, width: .5, strike: 8, toe_off: 55, arm_out: 5 },
  })) SIG[k] = Object.assign({ name: k }, BASE, v);

  const geo = S => ({ S, L1: .3895 * S, L2: .399 * S, leg: .7885 * S, hipx: .085 * S, hipdz: -.0285 * S, pelz: .8835 * S, ankz: .0665 * S });
  const stepLength = (sig, S) => 2 * sig.stride * .7885 * S;            // metres covered by one full cycle (two steps)

  function foot(sig, g, ph, side, total) {                            // ankle target and foot pitch (toes up +) at phase ph, 0 = heel strike
    const D = sig.duty, ballL = .115 * g.S, strike = rad(sig.strike), toe = rad(sig.toe_off), x = side * g.hipx * sig.width;
    let u, z, pitch;
    if (ph < D) {
      const s = ph / D; u = total * (sig.fore - s);
      pitch = s < .16 ? strike * (1 - s / .16) : s < .58 ? 0 : -toe * sm((s - .58) / .42);
      z = g.ankz + (pitch < 0 ? ballL * Math.sin(-pitch) : .03 * g.S * Math.sin(pitch));
      if (pitch < 0) u += ballL * (1 - Math.cos(pitch));
    } else {
      const s = (ph - D) / (1 - D), e = s - Math.sin(TAU * s) / TAU, u0 = -total * (1 - sig.fore) + ballL * (1 - Math.cos(toe));
      u = u0 + (total * sig.fore - u0) * e; pitch = -toe + (toe + strike) * sm(s * 1.15);
      z = g.ankz + ballL * Math.sin(toe) * (1 - sm(s * 2.2)) + sig.lift * g.S * Math.pow(Math.max(0, Math.sin(Math.PI * Math.min(1, s * 1.08))), 1.2) + .03 * g.S * Math.sin(strike) * sm((s - .7) / .3);
    }
    return [x, -u, z, pitch];
  }
  function pelvis(sig, g, ph, base) {
    const yaw = -rad(sig.hip_yaw) * Math.cos(TAU * ph), roll = -rad(sig.hip_roll) * Math.sin(TAU * (ph - .03));
    return { loc: [sig.sway * g.S * Math.sin(TAU * (ph - .06)), 0, base - sig.bounce * g.S * Math.cos(2 * TAU * (ph - .045))], R: mm(Rz(yaw), Ry(roll)), yaw, roll };
  }
  const baseCache = new Map();
  function baseHeight(sig, g, total) {                                // hip height that lets the planted leg reach the ground through the step
    const key = sig.name + "|" + g.S.toFixed(2); let best = baseCache.get(key); if (best !== undefined) return best;
    best = 1e9; const lmax = g.leg * .9975;
    for (let i = 0; i < 64; i++) {
      const ph = i / 64, p = pelvis(sig, g, ph, 0);
      for (const [side, off] of [[1, 0], [-1, .5]]) {
        const q = (ph + off) % 1; if (q > sig.duty * .62) continue;
        const t = foot(sig, g, q, side, total), hip = add(p.loc, mv(p.R, [side * g.hipx, 0, g.hipdz])), dx = t[0] - hip[0], dy = t[1] - hip[1];
        best = Math.min(best, t[2] + Math.sqrt(Math.max(lmax * lmax - dx * dx - dy * dy, 1e-6)) - hip[2]);
      }
    }
    baseCache.set(key, best); return best;
  }
  function leg(P, k, g, hip, tgt, n0, pitch, toeOut) {                 // two-bone leg: knee bends forward
    const v = sub(tgt, hip), d = cl(len(v), Math.abs(g.L1 - g.L2) + 1e-4, (g.L1 + g.L2) * .9985), vh = unit(v);
    const a = Math.acos(cl((g.L1 * g.L1 + d * d - g.L2 * g.L2) / (2 * g.L1 * d), -1, 1)), n = unit(sub(n0, sc(vh, dot(vh, n0)))), b = cross(vh, n);
    P.hip[k] = hip; P.kn[k] = add(hip, add(sc(vh, g.L1 * Math.cos(a)), sc(b, g.L1 * Math.sin(a)))); P.an[k] = add(hip, sc(vh, d));
    const Rf = mm(Rz(toeOut), Rx(-pitch)), f = mv(Rf, [0, -1, 0]), dn = mv(Rf, [0, 0, -1]);
    P.heel[k] = add(P.an[k], add(sc(dn, .03 * g.S), sc(f, -.035 * g.S))); P.toe[k] = add(P.an[k], add(sc(dn, .036 * g.S), sc(f, .135 * g.S)));
  }
  function upper(P, g, W, pel, Rp, chest, arms, head) {                // chest, arms and head on top of the hips
    const S = g.S, Rt = mm(Rp, mm(mm(Rz(chest[0]), Rx(chest[1])), Ry(chest[2]))), sh = (.172 * W + .016) * S;
    P.pel = pel; P.Rp = Rp; P.Rt = Rt; P.spine = add(pel, mv(Rp, [0, 0, .098 * S])); P.neck = add(P.spine, mv(Rt, [0, .005 * S, .3528 * S]));
    for (const k of [0, 1]) {
      const sd = k ? -1 : 1, a = arms[k], Ru = mm(Rt, mm(Rx(-rad(a[0])), Ry(-sd * rad(a[1])))), Rl = mm(Ru, Rx(-rad(a[2])));
      P.sh[k] = add(P.spine, mv(Rt, [sd * sh, 0, .3381 * S])); P.el[k] = add(P.sh[k], mv(Ru, [sd * .0264 * S, -.0096 * S, -.2367 * S])); P.wr[k] = add(P.el[k], mv(Rl, [sd * .0286 * S, -.0104 * S, -.2564 * S]));
    }
    P.Rh = mm(mm(Rz(head[0]), Rx(head[1])), Ry(head[2]));              // the head is held in space; the neck takes 38% of the difference
    P.headJ = add(P.neck, sc(unit(add(sc(mv(Rt, [0, 0, 1]), .62), sc(mv(P.Rh, [0, 0, 1]), .38))), .064 * S));
  }
  const blank = () => ({ hip: [], kn: [], an: [], heel: [], toe: [], sh: [], el: [], wr: [] });
  const wander = (t, seed) => [rad(1.6) * Math.sin(.9 * t + seed) + rad(.9) * Math.sin(2.3 * t + 1.7 * seed), rad(1) * Math.sin(1.3 * t + 2.1 * seed), rad(1.2) * Math.sin(.7 * t + .6 * seed)];

  function walk(sig, g, W, ph, hm) {
    const P = blank(), w = TAU, total = 2 * sig.duty * sig.stride * g.leg, p = pelvis(sig, g, ph, baseHeight(sig, g, total)), n0 = mv(p.R, [1, 0, 0]);
    for (const [k, side, off] of [[0, 1, 0], [1, -1, .5]]) {
      const t = foot(sig, g, (ph + off) % 1, side, total);
      leg(P, k, g, add(p.loc, mv(p.R, [side * g.hipx, 0, g.hipdz])), t, n0, t[3], side * rad(sig.toe_out));
    }
    const chest = [-(1 + sig.counter) * p.yaw, rad(sig.lean) + rad(.8) * Math.cos(2 * w * (ph - .10)), -.85 * p.roll + .25 * p.roll * Math.cos(w * ph)], arms = [];
    for (const [k, off] of [[0, 0], [1, .5]]) {
      const q = (ph + off) % 1, kk = k ? sig.arm_r : sig.arm_l, sw = -Math.cos(w * (q - .04));
      arms.push([sig.arm_bias + sig.arm_swing * kk * sw, sig.arm_out, sig.elbow + sig.elbow_swing * kk * Math.max(0, sw) + 4 * kk * Math.max(0, -sw)]);
    }
    const lag = sig.head_lag, yawB = -sig.counter * (-rad(sig.hip_yaw) * Math.cos(w * (ph - lag)));
    upper(P, g, W, p.loc, p.R, chest, arms, [sig.head_follow * yawB + hm[0], rad(sig.head_pitch) + rad(sig.head_bob) * Math.cos(2 * w * (ph - .045 - lag)) + .35 * rad(sig.lean) + hm[1], .3 * sig.head_follow * p.roll + hm[2]]);
    return P;
  }
  function stand(sig, g, W, t, hm, seed) {                              // not in motion_lib yet: a quiet stance in the same terms
    const P = blank(), br = Math.sin(t * 1.3 + seed), pel = [.006 * g.S * Math.sin(t * .5 + seed), 0, g.pelz - .012 * g.S + .002 * br];
    for (const [k, side] of [[0, 1], [1, -1]]) leg(P, k, g, add(pel, [side * g.hipx, 0, g.hipdz]), [side * g.hipx * Math.max(.8, sig.width), .01 * side, g.ankz], [1, 0, 0], 0, side * rad(sig.toe_out + 4));
    upper(P, g, W, pel, I3, [0, rad(sig.lean * .5) + .004 * br, 0], [[sig.arm_bias, sig.arm_out + 1.5, sig.elbow * .7 + 4], [sig.arm_bias, sig.arm_out + 1.5, sig.elbow * .7 + 4]], [hm[0], rad(sig.head_pitch * .5) + hm[1], hm[2]]);
    return P;
  }
  function sit(sig, g, W, t, hm, seed) {                                // origin is the seat top
    const P = blank(), pel = [0, 0, .095 * g.S - g.hipdz];
    for (const [k, side] of [[0, 1], [1, -1]]) {
      const hip = add(pel, [side * g.hipx, 0, g.hipdz]);
      leg(P, k, g, hip, add(hip, [side * .035 * g.S, -g.L1 * .97, -g.L2 * .98]), [1, 0, 0], 0, side * rad(8));
    }
    upper(P, g, W, pel, I3, [0, rad(4) + .004 * Math.sin(t * 1.3 + seed), 0], [[26, 6, 62], [26, 6, 62]], [hm[0], rad(4) + hm[1], hm[2]]);
    return P;
  }

  // ---- shapes: everything is lofted rings with smooth normals, so forms read as soft
  const JIT = new Float32Array(64); for (let i = 0; i < 64; i++) JIT[i] = .93 + .14 * (Math.abs(Math.sin(i * 12.9898) * 43758.5453) % 1);
  const scratch = new Float32Array(16 * 16 * 6);
  function loft(ms, rings, n, col, fl, ctr, seed) {                   // ring: [centre, u, v] in world space; ctr gives ball normals
    const m = rings.length, need = (m - 1) * n * 60;
    if (ms.n + need > ms.a.length) { const b = new Float32Array(Math.max(ms.a.length * 2, ms.n + need)); b.set(ms.a.subarray(0, ms.n)); ms.a = b; }
    for (let k = 0; k < m; k++) {
      const [c, u, v] = rings[k];
      for (let i = 0; i < n; i++) {
        const a = TAU * i / n, ca = Math.cos(a), sa = Math.sin(a), o = (k * n + i) * 6;
        const px = c[0] + u[0] * ca + v[0] * sa, py = c[1] + u[1] * ca + v[1] * sa, pz = c[2] + u[2] * ca + v[2] * sa;
        let nx, ny, nz; if (ctr) { nx = px - ctr[0]; ny = py - ctr[1]; nz = pz - ctr[2]; } else { nx = px - c[0]; ny = py - c[1]; nz = pz - c[2]; }
        const l = Math.hypot(nx, ny, nz) || 1; scratch[o] = px; scratch[o + 1] = py; scratch[o + 2] = pz; scratch[o + 3] = nx / l; scratch[o + 4] = ny / l; scratch[o + 5] = nz / l;
      }
    }
    const a = ms.a; let q = ms.n;
    for (let k = 0; k < m - 1; k++) for (let i = 0; i < n; i++) {
      const j = (i + 1) % n, A = (k * n + i) * 6, B = (k * n + j) * 6, Cc = ((k + 1) * n + j) * 6, Dd = ((k + 1) * n + i) * 6;
      for (const o of [A, B, Cc, A, Cc, Dd]) {
        const jt = JIT[(o / 6 * 7 + seed) & 63];
        a[q] = scratch[o]; a[q + 1] = scratch[o + 1]; a[q + 2] = scratch[o + 2]; a[q + 3] = scratch[o + 3]; a[q + 4] = scratch[o + 4]; a[q + 5] = scratch[o + 5];
        a[q + 6] = col[0] * jt; a[q + 7] = col[1] * jt; a[q + 8] = col[2] * jt; a[q + 9] = fl; q += 10;
      }
    }
    ms.n = q;
  }
  function limb(ms, A, B, ra, rb, n, col, fl, caps, ref, seed) {        // a tapered tube from A to B, rounded at the ends
    const d = unit(sub(B, A)); let u = cross(d, ref); if (len(u) < .05) u = cross(d, [ref[1], ref[2], ref[0]]); u = unit(u); const v = cross(d, u);
    const ring = (c, r) => [c, sc(u, r), sc(v, r)];
    loft(ms, caps ? [ring(add(A, sc(d, -ra * .92)), ra * .3), ring(add(A, sc(d, -ra * .55)), ra * .82), ring(A, ra), ring(B, rb), ring(add(B, sc(d, rb * .55)), rb * .82), ring(add(B, sc(d, rb * .92)), rb * .3)] : [ring(A, ra), ring(B, rb)], n, col, fl, null, seed);
  }
  function ball(ms, c, ax, ay, az, n, m, col, fl, seed) {               // an ellipsoid with half-axes ax, ay, az
    const rings = [];
    for (let k = 0; k <= m; k++) { const f = -Math.PI / 2 + Math.PI * k / m, cf = Math.max(.02, Math.cos(f)); rings.push([add(c, sc(az, Math.sin(f))), sc(ax, cf), sc(ay, cf)]); }
    loft(ms, rings, n, col, fl, c, seed);
  }

  // ---- who people are: designer values per role (ART_DIRECTION §3.4 and rule C5)
  const K = { cream: [.66, .61, .50], ochre: [.52, .40, .20], terra: [.44, .24, .18], teal: [.20, .36, .37], blue: [.30, .38, .52], brown: [.24, .16, .11], olive: [.30, .33, .20], plum: [.30, .24, .34], charcoal: [.09, .09, .11], indigo: [.10, .14, .24], black: [.03, .03, .035], sand: [.58, .52, .44], slate: [.25, .3, .4] };
  const SKINS = [[.62, .42, .30], [.30, .19, .13], [.78, .56, .44], [.84, .64, .52], [.70, .50, .38], [.47, .31, .22], [.20, .12, .08], [.55, .36, .25]];
  const HAIRS = [[.03, .03, .035], [.10, .06, .04], [.28, .10, .05], [.55, .40, .18], [.18, .1, .06], [.03, .03, .035]];
  const EYES = [[.2, .1, .05], [.14, .36, .20], [.18, .34, .55], [.12, .08, .05], [.3, .2, .08]];
  const ROLE = {
    shopkeeper: { g: "long", top: ["cream", "sand"], bottom: ["brown", "charcoal"], apron: ["terra", "ochre"], hair: ["side", "short", "bob"], shoes: ["black", "brown"], h: [1.64, 1.78], b: [1, 1.16], tash: .35 },
    cafe: { g: "long", top: ["blue", "teal", "sand"], bottom: ["brown", "indigo"], apron: ["cream"], hair: ["short", "bun", "bob", "afro"], shoes: ["brown"], h: [1.6, 1.8], b: [.9, 1.06] },
    courier: { g: "tee", top: ["blue", "teal"], shorts: 1, bottom: ["charcoal", "brown"], cap: ["terra", "ochre"], bag: 1, hair: ["short"], shoes: ["terra"], h: [1.68, 1.8], b: [.94, 1.04] },
    elder: { g: "tunic", top: ["plum", "teal", "brown"], skirt: .5, bottom: ["olive", "brown"], scarf: ["cream", "ochre"], scarfP: .8, hair: ["bun", "short"], grey: 1, shoes: ["brown"], h: [1.56, 1.66], b: [1, 1.12] },
    clerk: { g: "coat", top: ["indigo", "charcoal"], bottom: ["charcoal"], hair: ["bob", "short", "side"], shoes: ["black"], h: [1.62, 1.8], b: [.92, 1.04] },
    commuter: { g: "coat", top: ["indigo", "plum", "brown", "slate"], bottom: ["charcoal", "brown", "indigo"], hair: ["bob", "short", "side", "long", "afro"], shoes: ["black", "brown"], scarf: ["ochre", "cream"], scarfP: .25, h: [1.6, 1.82], b: [.9, 1.1] },
    student: { g: "tee", top: ["teal", "ochre", "terra", "cream"], bottom: ["indigo", "olive", "blue"], shorts: .3, scarf: ["ochre", "terra"], scarfP: .3, hair: ["long", "bob", "afro", "short"], shoes: ["cream"], bag: .4, h: [1.58, 1.76], b: [.88, 1] },
    maker: { g: "tunic", top: ["ochre", "olive", "terra", "sand"], bottom: ["olive", "brown"], shorts: .4, bag: .5, hair: ["afro", "short", "side", "bun"], shoes: ["sand", "brown"], h: [1.62, 1.82], b: [.96, 1.14] },
    nurse: { g: "tunic", top: ["cream"], bottom: ["teal"], hair: ["bun", "bob"], shoes: ["cream"], h: [1.6, 1.76], b: [.92, 1.04] },
    gardener: { g: "tunic", top: ["olive"], bottom: ["brown"], cap: ["ochre"], hair: ["short", "side"], shoes: ["brown"], h: [1.64, 1.8], b: [1, 1.12] },
    utility: { g: "long", top: ["charcoal"], bottom: ["indigo"], cap: ["ochre"], hair: ["short"], shoes: ["black"], h: [1.66, 1.82], b: [1, 1.12] },
  };
  function look(id, role, sigName) {
    const R = ROLE[role] || ROLE.commuter, r = k => hash(id, k), pk = (list, k) => K[list[Math.min(list.length - 1, Math.floor(r(k) * list.length))]];
    const height = R.h[0] + (R.h[1] - R.h[0]) * r("h");
    return { S: height / 1.72, W: (R.b[0] + (R.b[1] - R.b[0]) * r("b")) * 1.04, skin: SKINS[Math.floor(r("s") * SKINS.length)], hair: R.hair[Math.floor(r("hs") * R.hair.length)],
      hairC: R.grey ? [.5, .5, .51] : HAIRS[Math.floor(r("hc") * HAIRS.length)], eyes: EYES[Math.floor(r("e") * EYES.length)], g: R.g, top: pk(R.top, "t"), bottom: pk(R.bottom, "bt"),
      legs: R.skirt && r("sk") < R.skirt ? "skirt" : R.shorts && r("sh") < R.shorts ? "shorts" : "trousers", shoes: pk(R.shoes, "so"), apron: R.apron ? pk(R.apron, "ap") : null, cap: R.cap ? pk(R.cap, "cp") : null,
      bag: R.bag && r("bg") < R.bag, scarf: R.scarf && r("sf") < (R.scarfP || 1) ? pk(R.scarf, "sc") : null, tash: R.tash && r("ta") < R.tash, sig: SIG[sigName] || SIG.Easy, seed: Math.floor(r("seed") * 1000) };
  }
  const player = () => ({ S: 1.68 / 1.72, W: .92 * 1.04, skin: [.62, .42, .30], hair: "long", hairC: [.28, .10, .05], eyes: [.14, .36, .20], g: "tee", top: K.teal, bottom: K.indigo, legs: "trousers", shoes: K.cream, scarf: K.ochre, sig: SIG.Springy, seed: 7 });

  // pose: 0 stand, 1 walk, 2 sit, 3 lie. o: { ph: cycle phase 0..1, t: seconds, lod: 0 near | 1 mid | 2 far, fl: material, look: [yaw, pitch] the head adds, sig: override, sleep }
  function draw(ms, x, y, z, yaw, L, pose, o) {
    const sig = o.sig || L.sig, g = geo(L.S), S = L.S, Sh = S * 1.09, W = L.W * 1.06, lod = o.lod, fl = o.fl, t = o.t, sd = L.seed;
    const wd = wander(t, sd * .37), lk = o.look || [0, 0], hm = [wd[0] + lk[0], wd[1] + lk[1] + (o.face ? o.face.nod || 0 : 0), wd[2]];   // a speaker nods on stressed words (ANIM-006)
    const P = pose === 1 ? walk(sig, g, W, o.ph, hm) : pose === 2 ? sit(sig, g, W, t, hm, sd) : stand(sig, g, W, t, pose === 3 ? [0, 0, 0] : hm, sd);
    // where the character's axes point in the world: standing (x left, y back, z up) or lying on the back, head to +z
    const c = Math.cos(yaw), s = Math.sin(yaw), ex = pose === 3 ? [-1, 0, 0] : [-c, 0, -s], ey = pose === 3 ? [0, -1, 0] : [-s, 0, c], ez = pose === 3 ? [0, 0, 1] : [0, 1, 0];
    const O = pose === 3 ? [x, y + .1 * S, z - .86 * S] : [x, y, z];
    const Wp = p => [O[0] + ex[0] * p[0] + ey[0] * p[1] + ez[0] * p[2], O[1] + ex[1] * p[0] + ey[1] * p[1] + ez[1] * p[2], O[2] + ex[2] * p[0] + ey[2] * p[1] + ez[2] * p[2]];
    const Wd = p => [ex[0] * p[0] + ey[0] * p[1] + ez[0] * p[2], ex[1] * p[0] + ey[1] * p[1] + ez[1] * p[2], ex[2] * p[0] + ey[2] * p[1] + ez[2] * p[2]];
    const n = lod === 0 ? 10 : lod === 1 ? 7 : 5, nl = lod === 0 ? 8 : lod === 1 ? 6 : 4, caps = lod === 0, fat = 1.34 * S, fwd = Wd([0, -1, 0]);
    const long = L.g !== "tee", legCol = L.legs === "skirt" ? L.skin : L.bottom;
    // legs and shoes
    for (const k of [0, 1]) {
      const hip = Wp(P.hip[k]), kn = Wp(P.kn[k]), an = Wp(P.an[k]);
      limb(ms, hip, kn, .066 * fat, .05 * fat, nl, legCol, fl, caps, fwd, sd + k);
      limb(ms, kn, an, .05 * fat, .038 * fat, nl, L.legs === "trousers" ? L.bottom : L.skin, fl, caps, fwd, sd + k + 2);
      limb(ms, Wp(P.heel[k]), Wp(P.toe[k]), .04 * fat, .036 * fat, nl, L.shoes, fl, lod < 2, [0, 1, 0], sd + k + 4);
    }
    // the garment: one soft form from hem to shoulders
    const tx = Wd(mv(P.Rt, [1, 0, 0])), ty = Wd(mv(P.Rt, [0, 1, 0])), tz = Wd(mv(P.Rt, [0, 0, 1])), px = Wd(mv(P.Rp, [1, 0, 0])), py = Wd(mv(P.Rp, [0, 1, 0])), pz = Wd(mv(P.Rp, [0, 0, 1]));
    const pel = Wp(P.pel), sp = Wp(P.spine), swing = Math.abs(P.an[0][1] - P.an[1][1]);
    const ringP = (dz, rx, ry, oy) => [add(pel, add(sc(pz, dz * S), sc(py, (oy || 0) * S))), sc(px, rx * S), sc(py, ry * S)], ringT = (dz, rx, ry) => [add(sp, sc(tz, dz * S)), sc(tx, rx * S), sc(ty, ry * S)];
    const body = [];
    if (L.g === "coat") body.push(ringP(-.47, .19 * W, .145 + swing * .22), ringP(-.2, .175 * W, .13));
    else if (L.g === "tunic") body.push(ringP(-.23, .172 * W, .128 + swing * .12));
    else body.push(ringP(-.09, .152 * W, .108));
    body.push(ringP(0, .155 * W, .112), ringT(.06, .138 * W, .1), ringT(.25, .172 * W, .112), ringT(.315, .192 * W, .104), ringT(.36, .14 * W, .085), ringT(.39, .06, .055));
    loft(ms, body, n, L.top, fl, null, sd);
    if (L.legs === "skirt") loft(ms, [ringP(-.44, .185 * W, .15 + swing * .2), ringP(-.2, .168 * W, .125), ringP(-.02, .15 * W, .108)], n, L.bottom, fl, null, sd + 9);
    // arms and hands
    for (const k of [0, 1]) {
      const a = add(Wp(P.sh[k]), sc(tz, -.035 * S)), b = Wp(P.el[k]), w = Wp(P.wr[k]);
      limb(ms, a, b, .047 * fat, .04 * fat, nl, L.top, fl, false, fwd, sd + k + 6);
      limb(ms, b, w, .04 * fat, .032 * fat, nl, long ? L.top : L.skin, fl, caps, fwd, sd + k + 8);
      if (lod < 2) ball(ms, add(w, sc(unit(sub(w, b)), .035 * S)), sc(tx, .034 * fat), sc(ty, .03 * fat), sc(tz, .042 * fat), 6, 4, L.skin, fl, sd);
    }
    // neck and head
    const hx = Wd(mv(P.Rh, [1, 0, 0])), hy = Wd(mv(P.Rh, [0, 1, 0])), hz = Wd(mv(P.Rh, [0, 0, 1])), nk = Wp(P.neck), hj = Wp(P.headJ), hc = add(hj, sc(hz, .1 * Sh));
    const H = (a, b, cc) => add(hc, add(add(sc(hx, a * Sh), sc(hy, b * Sh)), sc(hz, cc * Sh)));
    const blob = (a, b, cc, rx, ry, rz, col, nn, mm2, s2) => ball(ms, H(a, b, cc), sc(hx, rx * Sh), sc(hy, ry * Sh), sc(hz, rz * Sh), nn, mm2, col, fl, s2 || sd);
    limb(ms, nk, hj, .046 * S, .044 * S, nl, L.skin, fl, false, fwd, sd);
    blob(0, 0, 0, .098, .108, .125, L.skin, lod === 0 ? 12 : lod === 1 ? 8 : 6, lod === 0 ? 8 : lod === 1 ? 5 : 4);
    const hn = lod === 0 ? 10 : lod === 1 ? 7 : 5, hm2 = lod === 0 ? 6 : 4, hairC = L.hairC;
    if (L.hair === "afro") blob(0, .02, .04, .142, .14, .135, hairC, hn, hm2);
    else if (L.hair !== "bald") {
      blob(0, .03, .04, .106, .116, .122, hairC, hn, hm2);
      if (L.hair === "bob") blob(0, .038, -.035, .112, .098, .105, hairC, hn, hm2);
      else if (L.hair === "long") blob(0, .055, -.1, .104, .072, .165, hairC, hn, hm2);
      else if (L.hair === "bun") blob(0, .075, .105, .05, .05, .048, hairC, 6, 4);
      else if (L.hair === "side") blob(.03, .0, .075, .085, .1, .06, hairC, 6, 4);
    }
    if (L.cap) { blob(0, .018, .078, .109, .118, .075, L.cap, hn, hm2); blob(0, -.105, .06, .075, .06, .012, L.cap, 6, 3); }
    if (lod === 0 && o.face) face(blob, o.face, L, t, sd, lk, o.sleep);   // someone in a conversation: the full control set (CHR-007)
    else if (lod === 0) {                                             // the face: full eyes, brows, small nose, mouth at rest
      const period = 2.4 + 3.4 * ((sd * .618) % 1), shut = o.sleep || ((t + sd) % period) < .17, gx = cl(-lk[0] / rad(34), -1, 1) * .006, gz = cl(-lk[1] / rad(34), -1, 1) * .004;
      for (const e of [-1, 1]) {
        if (shut) { blob(e * .037, -.0975, .012, .0225, .0105, .0235, L.skin, 8, 4, sd + 3); continue; }
        blob(e * .037, -.0975, .012, .0215, .008, .022, [.86, .84, .8], 8, 4);
        blob(e * .037 + gx, -.1035, .0065 + gz, .0158, .004, .0158, L.eyes, 8, 3);
        blob(e * .037 + gx, -.1065, .0065 + gz, .0078, .002, .0078, [.01, .01, .012], 6, 3);
        blob(e * .037 + gx + .005, -.108, .012 + gz, .0032, .0012, .0032, [1, 1, 1], 5, 2);
      }
      for (const e of [-1, 1]) blob(e * .038, -.1, .012 + .05, .021, .0055, .0048, sc(hairC, .8), 6, 3);
      blob(0, -.111, -.014, .0105, .011, .0115, sc(L.skin, .93), 6, 4);
      blob(0, -.1, -.053, .024, .0045, .0048, [.34, .13, .11], 8, 3);
      if (L.tash) blob(0, -.104, -.037, .024, .006, .007, hairC, 6, 3);
    }
    // what the role wears on top (C5)
    if (L.scarf) { ball(ms, add(nk, sc(tz, .012 * S)), sc(tx, .078 * S), sc(ty, .072 * S), sc(tz, .036 * S), hn, 4, L.scarf, fl, sd + 5); if (lod < 2) limb(ms, add(nk, add(sc(ty, -.07 * S), sc(tx, .03 * S))), add(nk, add(add(sc(ty, -.105 * S), sc(tx, .035 * S)), sc(tz, -.2 * S))), .024 * S, .022 * S, 5, L.scarf, fl, false, tx, sd + 5); }
    if (lod < 2) {
      if (L.apron) ball(ms, add(sp, add(sc(ty, -.1 * S), sc(tz, -.03 * S))), sc(tx, .125 * W * S), sc(ty, .022 * S), sc(tz, .27 * S), 8, 5, L.apron, fl, sd + 11);
      if (L.bag) { const bg = add(pel, add(add(sc(px, .2 * W * S), sc(py, -.02 * S)), sc(pz, -.04 * S))); ball(ms, bg, sc(px, .05 * S), sc(py, .075 * S), sc(pz, .085 * S), 7, 4, K.brown, fl, sd + 12); limb(ms, add(Wp(P.sh[1]), sc(tz, .03 * S)), bg, .012 * S, .012 * S, 4, K.brown, fl, false, fwd, sd); }
    }
  }
  // The face control set of docs/art/CHARACTER_FACE.md §1, drawn in head units (about metres on a natural head). f: { mouth: { width,
  // opening, round, teeth 0 none | 1 upper | 2 both, tongue }, curve, brow_l, brow_r: [inner, outer], lid, squint, open, gaze: [x, z] },
  // mouth sizes already at the larger-than-life drawing scale. Lips are a ring; the dark inside, teeth and tongue sit in the plane
  // of the opening (CHARACTER_FACE §1 construction notes).
  const MOUTH = { lip: [.34, .13, .11], dark: [.11, .035, .035], teeth: [.9, .88, .82], tongue: [.62, .26, .26] };
  function face(blob, f, L, t, sd, lk, sleep) {
    const period = 2.4 + 3.4 * ((sd * .618) % 1), blink = !sleep && ((t + sd) % period) < .17;
    const gx = cl(-lk[0] / rad(34), -1, 1) * .006 + f.gaze[0], gz = cl(-lk[1] / rad(34), -1, 1) * .004 + f.gaze[1];
    const lid = sleep || blink ? 1 : f.lid, op = f.open, sq = f.squint;
    for (const e of [-1, 1]) {                                         // e = +1 is the character's left
      const ex = e * .037;
      if (lid > .97) { blob(ex, -.0975, .012, .0225, .0105, .0235, L.skin, 8, 4, sd + 3); continue; }
      // the open part of the eye: a squint lifts its lower edge, the upper lid lowers its top edge (the face skin shows where it closes)
      const R0 = .022 * op, top = .012 + R0 * (1 - 2 * lid * .85), bot = .012 - R0 * (1 - sq * .9), rz = (top - bot) / 2, cz = (top + bot) / 2;
      blob(ex, -.0975, cz, .0215 * op, .008, rz, [.86, .84, .8], 8, 4);
      const ir = Math.min(.0158 * op, rz * .9), iz = Math.min(.0065 + gz, top - ir * .7) + (cz - .012) * .3;
      blob(ex + gx, -.1035, iz, .0158 * op, .004, ir, L.eyes, 8, 3);
      blob(ex + gx, -.1065, iz, .0078, .002, Math.min(.0078, ir * .55), [.01, .01, .012], 6, 3);
      if (top - iz > .006) blob(ex + gx + .005, -.108, iz + .0055, .0032, .0012, .0032, [1, 1, 1], 5, 2);
      if (lid >= .03) blob(ex, -.1062, top - .0008, .0205 * op, .0025, .0016, [.08, .06, .05], 8, 3);   // the lash line along the lowered lid
    }
    for (const e of [-1, 1]) {                                         // each brow: inner and outer end move on their own
      const b = e > 0 ? f.brow_l : f.brow_r, z0 = .062 + (op - 1) * .02, bc = sc(L.hairC, .8);
      for (let k = 0; k < 5; k++) { const u = k / 4; blob(e * (.026 + .024 * u), -.1 + .0015 * u, z0 + .002 * u + b[0] + (b[1] - b[0]) * u, .0115, .0055, .0047, bc, 6, 3); }
    }
    blob(0, -.111, -.014, .0105, .011, .0115, sc(L.skin, .93), 6, 4);   // nose
    const m = f.mouth, w = m.width, oh = Math.max(m.opening, .0005), mz = -.053, rd = m.round || 0, dep = 1 + rd * .45, cu = cl(f.curve, -1, 1) * .0045;
    if (oh > .0012) {
      blob(0, -.0992, mz, w * .9, .0028, oh, MOUTH.dark, 8, 3);
      if (m.teeth >= 1 && oh > .002) blob(0, -.1002, mz + oh * .6, w * .66, .0022, Math.min(oh * .4, .0032), MOUTH.teeth, 8, 3);
      if (m.teeth >= 2 && oh > .002) blob(0, -.1002, mz - oh * .62, w * .58, .0022, Math.min(oh * .32, .0026), MOUTH.teeth, 8, 3);
      if (m.tongue && oh > .004) blob(0, -.0998, mz - oh * .42, w * .5, .0026, oh * .34, MOUTH.tongue, 8, 3);
    }
    // the lips: a closed ring of overlapping pieces round the opening; the corners carry the smile or frown
    const N = 14, th = .0024 + rd * .0008, lw = Math.max(w * .3, .006);
    for (let k = 0; k < N; k++) {
      const a = (k + .5) / N * 2 * Math.PI, cx = Math.cos(a), sz = Math.sin(a), x = w * cx;
      const z = mz + (oh + th * (sz > 0 ? .5 : .65)) * sz + cu * cx * cx;
      blob(x, -.0998 - .0006 * Math.abs(sz), z, lw * (.55 + .45 * Math.abs(sz)), .0046 * dep, th * (sz < 0 ? 1.15 : 1), MOUTH.lip, 6, 3);
    }
    if (L.tash) blob(0, -.104, -.037 + Math.max(0, oh - .002) * .4, .024, .006, .007, L.hairC, 6, 3);
  }
  return { SIG, look, player, draw, stepLength };
})();
