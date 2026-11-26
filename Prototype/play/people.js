/* Brix Town: people. Bodies follow character option B "Soft" (docs/art/ART_DIRECTION.md §3): larger head, full eyes, chunky
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

  function gesture(arms, chest, md) {                                 // CHARACTER_MOTION §4 layers: arm_l / arm_r = [weight, raise, out, elbow], chest = [pitch, yaw] in degrees
    if (!md) return;
    if (md.arms) for (const k of [0, 1]) { const a = md.arms[k]; if (a && a[0] > 0) for (let j = 0; j < 3; j++) arms[k][j] += (a[j + 1] - arms[k][j]) * a[0]; }
    if (md.chest) { chest[1] += rad(md.chest[0]); chest[0] += rad(md.chest[1] || 0); }
  }
  function walk(sig, g, W, ph, hm, md) {
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
    gesture(arms, chest, md);
    const lag = sig.head_lag, yawB = -sig.counter * (-rad(sig.hip_yaw) * Math.cos(w * (ph - lag)));
    upper(P, g, W, p.loc, p.R, chest, arms, [sig.head_follow * yawB + hm[0], rad(sig.head_pitch) + rad(sig.head_bob) * Math.cos(2 * w * (ph - .045 - lag)) + .35 * rad(sig.lean) + hm[1], .3 * sig.head_follow * p.roll + hm[2]]);
    return P;
  }
  function stand(sig, g, W, t, hm, seed, md) {                              // not in motion_lib yet: a quiet stance in the same terms
    const P = blank(), br = Math.sin(t * 1.3 + seed), pel = [.006 * g.S * Math.sin(t * .5 + seed), 0, g.pelz - .012 * g.S + .002 * br];
    for (const [k, side] of [[0, 1], [1, -1]]) leg(P, k, g, add(pel, [side * g.hipx, 0, g.hipdz]), [side * g.hipx * Math.max(.8, sig.width), .01 * side, g.ankz], [1, 0, 0], 0, side * rad(sig.toe_out + 4));
    const arms = [[sig.arm_bias, sig.arm_out * .3, sig.elbow * .7 + 4], [sig.arm_bias, sig.arm_out * .3, sig.elbow * .7 + 4]], chest = [0, rad(sig.lean * .5) + .004 * br, 0];
    gesture(arms, chest, md);
    upper(P, g, W, pel, I3, chest, arms, [hm[0], rad(sig.head_pitch * .5) + hm[1], hm[2]]);
    return P;
  }
  // On a board (CITY-099): side-on to the way of travel, feet across the deck either side of its middle, knees a little bent, arms
  // out a little for balance, the head turned to look ahead. rd.push (0 to 1) blends to the push of a manual board: turned to face
  // ahead, the front foot along the deck by the front truck and the back foot down on the ground beside it, pushing back from in
  // front of the back truck to behind the tail and lifted back through the air (rd.ph, 0 to 1 a push). rd.h: the deck top's height.
  function ride(sig, g, W, t, hm, seed, rd) {
    const P = blank(), b = rd.push || 0, h = rd.h, S = g.S, br = Math.sin(t * 1.3 + seed);
    const a = rad(78) * (1 - b) + rad(14) * b, Rp = Rz(a), n0 = mv(Rp, [1, 0, 0]), ph = rd.ph || 0;
    const pel = [0, -.02 * S * b, g.pelz + h - .055 * S * (1 - b) - (.06 * S + h) * b + .003 * br];   // pushing, the knee on the board bends so the other foot reaches the ground
    const on = ph < .55, s = on ? ph / .55 : (ph - .55) / .45;   // the push foot: on the ground pushing, then lifted back
    const py = on ? -.06 * S + .32 * S * s : .26 * S - .32 * S * (s - Math.sin(TAU * s) / TAU), pz = g.ankz + (on ? 0 : .07 * S * Math.sin(Math.PI * s));
    const back = [mix0(0, .12 * S, b), mix0(.2 * S, py, b), mix0(g.ankz + h, pz, b)], front = [0, -.2 * S - .02 * S * b, g.ankz + h];
    leg(P, 0, g, add(pel, mv(Rp, [g.hipx, 0, g.hipdz])), back, n0, 0, a * (1 - b) + rad(6) * b);
    leg(P, 1, g, add(pel, mv(Rp, [-g.hipx, 0, g.hipdz])), front, n0, 0, a * (1 - b) + rad(10) * b);
    const arms = [[12 - 4 * b, 22 - 10 * b, 24], [12 - 4 * b, 22 - 10 * b, 24]], chest = [-a * .25, rad(6) + rad(8) * b + .004 * br, 0];
    upper(P, g, W, pel, Rp, chest, arms, [hm[0], rad(4) + hm[1], hm[2]]);   // the head is held in space: it keeps looking ahead
    return P;
  }
  const mix0 = (x, y, t) => x + (y - x) * t;
  function sit(sig, g, W, t, hm, seed, md) {                            // origin is the seat top
    const P = blank(), pel = [0, 0, .095 * g.S - g.hipdz];
    for (const [k, side] of [[0, 1], [1, -1]]) {
      const hip = add(pel, [side * g.hipx, 0, g.hipdz]);
      let foot = add(hip, [side * .035 * g.S, -g.L1 * .97, -g.L2 * .98]);   // on a chair: thighs level, shins down
      if (md && md.floor != null) {                                    // in a car: feet out along the floor, never through it (md.floor: the floor below the seat top)
        const dn = Math.min(g.L2 * .98, Math.max(.04 * g.S, hip[2] - g.ankz - md.floor)), fw = Math.sqrt(Math.max(0, (.93 * (g.L1 + g.L2)) ** 2 - dn * dn));
        foot = add(hip, [side * .05 * g.S, -fw, -dn]);
      } else if (md && md.straddle) foot = add(hip, [side * .42 * g.S, -g.L1 * .45, -g.L2 * .95]);   // astride a plank: a leg down each side of it
      leg(P, k, g, hip, foot, [1, 0, 0], 0, side * rad(8));
    }
    const arms = [[26, 6, 62], [26, 6, 62]], chest = [0, rad(4) + .004 * Math.sin(t * 1.3 + seed), 0];
    gesture(arms, chest, md);                                         // an umbrella held up while sitting out in the rain
    upper(P, g, W, pel, I3, chest, arms, [hm[0], rad(4) + hm[1], hm[2]]);
    return P;
  }

  // ---- shapes: everything is lofted rings with smooth normals, so forms read as soft
  const JIT = new Float32Array(64); for (let i = 0; i < 64; i++) JIT[i] = .9 + .2 * (Math.abs(Math.sin(i * 12.9898) * 43758.5453) % 1);
  const scratch = new Float32Array(32 * 32 * 6), vjit = new Float32Array(32 * 32), cu = new Float32Array(32 * 32), cv = new Float32Array(32 * 32); let run = 0, ru = 0, rv = 0;
  // Cloth texture (play.js reads it): near people send, in place of a colour, the colour packed into one number and two coordinates that
  // stay on the cloth as the body moves. TEX: 0 plain colour, 1 textured. Skin gets the same, marked by a minus sign, for a fainter mottle.
  let TEX = 0, SKINREF = null, PLAIN = false; const SHUT = new URLSearchParams(location.search).has("blink"), TRUEN = !new URLSearchParams(location.search).has("oldnormals");   // PLAIN: one even colour, for eyes
  const pack = c => { const e = v => Math.round(Math.sqrt(Math.min(1, Math.max(0, v))) * 127) + 64; return e(c[0]) * 65536 + e(c[1]) * 256 + e(c[2]); };
  // Self-shadow (near people only). Before a body is built, its big shapes are listed as simple occluders: the head, the hair or hat (a shell
  // with the face cut away), the torso, each arm and each leg. Every point of the body then asks two things. Can it see the sun past the
  // others (hair shades the face, the head the neck and shoulders, an arm the body)? And how open is it to the sky (under a fringe, beside
  // an arm)? The answers ride to the shader in the material number (sun) and the length of the normal (sky), so they blend softly between points.
  let OCC = null, OSKIP = 0, SUN3 = [[0, 1, 0], [0, 1, 0], [0, 1, 0]];
  const svv = new Float32Array(32 * 32), aov = new Float32Array(32 * 32);
  const oEll = (id, c, ax, ay, az, keep) => { const inv = v => sc(v, 1 / dot(v, v)); return { id, t: keep ? 2 : 0, c, ix: inv(ax), iy: inv(ay), iz: inv(az), r: Math.cbrt(len(ax) * len(ay) * len(az)), keep }; };
  const oCap = (id, a, b, r) => ({ id, t: 1, a, b, v: sub(b, a), r });
  // The nearest face. One near person a frame, the one whose head is closest to the camera, has the shadows on the face worked out for
  // every pixel instead of every point: the hair's shadow, the nose's, the hat brim's and the locks'. This says who it is and hands the shader
  // those shapes; that person's head then leaves them out of the per-point test above, so nothing is shaded twice.
  let EYE = [0, 0, 0], heroPrev = null, heroBest = null, HERO = null, HEROSKIP = 0;
  function setSun(d, eye) {
    if (eye) EYE = eye; heroPrev = heroBest && heroBest.d < 7 ? heroBest.c : null; heroBest = null; HERO = null;
    setSunDir(d);
  }
  function setSunDir(d) {                                                // the sun, and two more rays a few degrees off it, so a shadow's edge is soft
    const u = unit(cross(d, Math.abs(d[1]) < .9 ? [0, 1, 0] : [1, 0, 0])), w = cross(d, u), k = .16;
    SUN3 = [d, unit(add(d, sc(u, k))), unit(add(d, sc(u, -k))), unit(add(d, sc(w, k))), unit(add(d, sc(w, -k)))];
  }
  function exitKept(o, px, py, pz, d) {                               // a shell: does the ray from p along d leave (or enter) it through a part that is there?
    const qx = px - o.c[0], qy = py - o.c[1], qz = pz - o.c[2];
    const ox = qx * o.ix[0] + qy * o.ix[1] + qz * o.ix[2], oy = qx * o.iy[0] + qy * o.iy[1] + qz * o.iy[2], oz = qx * o.iz[0] + qy * o.iz[1] + qz * o.iz[2];
    const dx = d[0] * o.ix[0] + d[1] * o.ix[1] + d[2] * o.ix[2], dy = d[0] * o.iy[0] + d[1] * o.iy[1] + d[2] * o.iy[2], dz = d[0] * o.iz[0] + d[1] * o.iz[1] + d[2] * o.iz[2];
    const a = dx * dx + dy * dy + dz * dz, b = ox * dx + oy * dy + oz * dz, c = ox * ox + oy * oy + oz * oz - 1, disc = b * b - a * c;
    if (disc <= 0) return false; const sq = Math.sqrt(disc), t2 = (-b + sq) / a; if (t2 <= 1e-4) return false;
    if (o.keep(ox + t2 * dx, oy + t2 * dy, oz + t2 * dz)) return true;
    const t1 = (-b - sq) / a; return t1 > 1e-4 && o.keep(ox + t1 * dx, oy + t1 * dy, oz + t1 * dz);
  }
  function sunSeen(px, py, pz) {                                      // 1 in full sun, 0 in the shade of another part of the same body
    let v = 1;
    for (const o of OCC) {
      if (o.id & OSKIP) continue;
      if (o.t === 2) { let h = 0; for (let k = 0; k < 5; k++) if (exitKept(o, px, py, pz, SUN3[k])) h++; v *= 1 - h / 5; }
      else if (o.t === 1) { const d = SUN3[0], wx = px - o.a[0], wy = py - o.a[1], wz = pz - o.a[2], B = d[0] * o.v[0] + d[1] * o.v[1] + d[2] * o.v[2], C = o.v[0] * o.v[0] + o.v[1] * o.v[1] + o.v[2] * o.v[2];
        const D = d[0] * wx + d[1] * wy + d[2] * wz, E = o.v[0] * wx + o.v[1] * wy + o.v[2] * wz, den = C - B * B;
        let sg = den > 1e-9 ? cl((E - B * D) / den, 0, 1) : 0, t = Math.max(0, B * sg - D); if (t < .012) continue; sg = cl((E + B * t) / C, 0, 1);
        const ex = wx + t * d[0] - sg * o.v[0], ey = wy + t * d[1] - sg * o.v[1], ez = wz + t * d[2] - sg * o.v[2], dist = Math.hypot(ex, ey, ez);
        const q = cl((o.r * 1.55 - dist) / (o.r * 1.15), 0, 1); v *= 1 - q * q * (3 - 2 * q); }
      else { const d = SUN3[0], qx = px - o.c[0], qy = py - o.c[1], qz = pz - o.c[2];
        const ox = qx * o.ix[0] + qy * o.ix[1] + qz * o.ix[2], oy = qx * o.iy[0] + qy * o.iy[1] + qz * o.iy[2], oz = qx * o.iz[0] + qy * o.iz[1] + qz * o.iz[2];
        const dx = d[0] * o.ix[0] + d[1] * o.ix[1] + d[2] * o.ix[2], dy = d[0] * o.iy[0] + d[1] * o.iy[1] + d[2] * o.iy[2], dz = d[0] * o.iz[0] + d[1] * o.iz[1] + d[2] * o.iz[2];
        const tt = -(ox * dx + oy * dy + oz * dz) / (dx * dx + dy * dy + dz * dz); if (tt <= 0) { if (ox * ox + oy * oy + oz * oz < .9) v = 0; continue; }
        const m = Math.hypot(ox + tt * dx, oy + tt * dy, oz + tt * dz), q = cl((1.35 - m) / .75, 0, 1); v *= 1 - q * q * (3 - 2 * q); }
      if (v < .02) return 0;
    }
    return v;
  }
  const UPV = [0, 1, 0];
  function skySeen(px, py, pz, nx, ny, nz) {                          // 1 open to the sky, less where another part of the body stands close in front
    let v = 1;
    for (const o of OCC) {
      if (o.id & OSKIP) continue;
      if (o.t === 2) { const l = Math.hypot(nx * .6, ny * .6 + .8, nz * .6) || 1, sx = nz, sz = -nx, sl = Math.hypot(sx, sz) || 1, tx2 = sx / sl * .7, tz2 = sz / sl * .7;
        if (exitKept(o, px, py, pz, [nx, ny, nz])) v -= .1; if (exitKept(o, px, py, pz, [nx * .6 / l, (ny * .6 + .8) / l, nz * .6 / l])) v -= .14;
        if (exitKept(o, px, py, pz, unit([nx * .7 + tx2, ny * .7 + .25, nz * .7 + tz2]))) v -= .1; if (exitKept(o, px, py, pz, unit([nx * .7 - tx2, ny * .7 + .25, nz * .7 - tz2]))) v -= .1; }
      else { let qx, qy, qz;
        if (o.t === 1) { const wx = px - o.a[0], wy = py - o.a[1], wz = pz - o.a[2], sg = cl((o.v[0] * wx + o.v[1] * wy + o.v[2] * wz) / (o.v[0] * o.v[0] + o.v[1] * o.v[1] + o.v[2] * o.v[2]), 0, 1); qx = o.a[0] + sg * o.v[0] - px; qy = o.a[1] + sg * o.v[1] - py; qz = o.a[2] + sg * o.v[2] - pz; }
        else { qx = o.c[0] - px; qy = o.c[1] - py; qz = o.c[2] - pz; }
        const d = Math.hypot(qx, qy, qz); if (d < 1e-4 || d > o.r * 3.5) continue;
        v *= 1 - .55 * cl((nx * qx + ny * qy + nz * qz) / d, 0, 1) * Math.min(1, o.r * o.r / (d * d)); }
    }
    return Math.max(.3, v);
  }
  function loft(ms, rings, n, col, fl, ctr, seed, sq, mask) {               // ring: [centre, u, v] in world space; ctr gives ball normals; sq < 1 squares the ring off towards a soft box
    const m = rings.length, need = (m - 1) * n * 60;
    if (ms.n + need > ms.a.length) { const b = new Float32Array(Math.max(ms.a.length * 2, ms.n + need)); b.set(ms.a.subarray(0, ms.n)); ms.a = b; }
    for (let k = 0; k < m; k++) {
      const [c, u, v] = rings[k];
      for (let i = 0; i < n; i++) {
        const a = TAU * i / n, o = (k * n + i) * 6; let ca = Math.cos(a), sa = Math.sin(a);
        if (sq) { ca = Math.sign(ca) * Math.pow(Math.abs(ca), sq); sa = Math.sign(sa) * Math.pow(Math.abs(sa), sq); }
        const px = c[0] + u[0] * ca + v[0] * sa, py = c[1] + u[1] * ca + v[1] * sa, pz = c[2] + u[2] * ca + v[2] * sa;
        let nx, ny, nz; if (ctr) { nx = px - ctr[0]; ny = py - ctr[1]; nz = pz - ctr[2]; } else { nx = px - c[0]; ny = py - c[1]; nz = pz - c[2]; }
        vjit[k * n + i] = JIT[((k * n + i) * 7 + seed * 13) & 63];   // one shade per vertex: the mottle blends softly across the form (C2)
        if (TEX) { if (i === 0) { if (k === 0) run = 0; else { const p = rings[k - 1][0]; run += Math.hypot(c[0] - p[0], c[1] - p[1], c[2] - p[2]); } ru = Math.hypot(u[0], u[1], u[2]); rv = Math.hypot(v[0], v[1], v[2]); }
          cu[k * n + i] = ru * ca + seed * .37; cv[k * n + i] = run + rv * sa * .7 + seed * .21; }
        const l = Math.hypot(nx, ny, nz) || 1; scratch[o] = px; scratch[o + 1] = py; scratch[o + 2] = pz; scratch[o + 3] = nx / l; scratch[o + 4] = ny / l; scratch[o + 5] = nz / l;
      }
    }
    // true surface normals, from the shape itself: the old ones pointed straight out from each ring's centre, so a flat chest shaded like a round
    // tube and a rounded end shaded like the side of a cylinder. ?oldnormals=1 brings those back to compare.
    if (TRUEN) for (let k = 0; k < m; k++) { const k0 = Math.max(0, k - 1), k1 = Math.min(m - 1, k + 1);
      for (let i = 0; i < n; i++) { const o = (k * n + i) * 6, a0 = (k * n + (i + n - 1) % n) * 6, a1 = (k * n + (i + 1) % n) * 6, b0 = (k0 * n + i) * 6, b1 = (k1 * n + i) * 6;
        const ax = scratch[a1] - scratch[a0], ay = scratch[a1 + 1] - scratch[a0 + 1], az = scratch[a1 + 2] - scratch[a0 + 2], bx = scratch[b1] - scratch[b0], by = scratch[b1 + 1] - scratch[b0 + 1], bz = scratch[b1 + 2] - scratch[b0 + 2];
        let nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx; const l = Math.hypot(nx, ny, nz); if (l < 1e-10) continue;
        const sg = nx * scratch[o + 3] + ny * scratch[o + 4] + nz * scratch[o + 5] < 0 ? -1 / l : 1 / l; scratch[o + 3] = nx * sg; scratch[o + 4] = ny * sg; scratch[o + 5] = nz * sg; } }
    if (OCC && PLAIN) { const c0 = rings[m >> 1][0], o = ((m >> 1) * n) * 6, sv0 = sunSeen(c0[0], c0[1], c0[2]), ao0 = skySeen(c0[0], c0[1], c0[2], scratch[o + 3], scratch[o + 4], scratch[o + 5]); svv.fill(sv0, 0, m * n); aov.fill(ao0, 0, m * n); }
    else if (OCC) for (let k = 0; k < m; k++) for (let i = 0; i < n; i++) { const o = (k * n + i) * 6, x = scratch[o], y = scratch[o + 1], z = scratch[o + 2];
      svv[k * n + i] = sunSeen(x, y, z); aov[k * n + i] = skySeen(x, y, z, scratch[o + 3], scratch[o + 4], scratch[o + 5]); }
    const a = ms.a, pk = TEX ? (col === SKINREF ? -pack(col) : pack(col)) : 0; let q = ms.n;
    for (let k = 0; k < m - 1; k++) for (let i = 0; i < n; i++) {
      if (mask && !mask(k, i)) continue;
      const j = (i + 1) % n, A = (k * n + i) * 6, B = (k * n + j) * 6, Cc = ((k + 1) * n + j) * 6, Dd = ((k + 1) * n + i) * 6;
      for (const o of [A, B, Cc, A, Cc, Dd]) {
        const jt = PLAIN ? 1 : vjit[o / 6];
        const nk2 = OCC ? .5 + .5 * aov[o / 6] : 1;                    // the normal's length carries how open the point is to the sky
        a[q] = scratch[o]; a[q + 1] = scratch[o + 1]; a[q + 2] = scratch[o + 2]; a[q + 3] = scratch[o + 3] * nk2; a[q + 4] = scratch[o + 4] * nk2; a[q + 5] = scratch[o + 5] * nk2;
        if (TEX) { a[q + 6] = pk; a[q + 7] = cu[o / 6]; a[q + 8] = cv[o / 6]; } else { a[q + 6] = col[0] * jt; a[q + 7] = col[1] * jt; a[q + 8] = col[2] * jt; }
        a[q + 9] = OCC ? fl - .4 * (1 - svv[o / 6]) : fl; q += 10;       // the material number carries how much of the sun the point sees
      }
    }
    ms.n = q;
  }
  function limb(ms, A, B, ra, rb, n, col, fl, caps, ref, seed) {        // a tapered tube from A to B, rounded at the ends
    const d = unit(sub(B, A)); let u = cross(d, ref); if (len(u) < .05) u = cross(d, [ref[1], ref[2], ref[0]]); u = unit(u); const v = cross(d, u);
    const ring = (c, r) => [c, sc(u, r), sc(v, r)];
    loft(ms, caps ? [ring(add(A, sc(d, -ra * .92)), ra * .3), ring(add(A, sc(d, -ra * .55)), ra * .82), ring(A, ra), ring(B, rb), ring(add(B, sc(d, rb * .55)), rb * .82), ring(add(B, sc(d, rb * .92)), rb * .3)] : [ring(A, ra), ring(B, rb)], n, col, fl, null, seed);
  }
  function tube(ms, pts, rs, n, col, fl, ref, seed, capA, capB) {      // one soft form through several points, bending at each: an arm with no joint showing (C1)
    const rings = [], m = pts.length;
    for (let i = 0; i < m; i++) {
      const din = i > 0 ? unit(sub(pts[i], pts[i - 1])) : null, dout = i < m - 1 ? unit(sub(pts[i + 1], pts[i])) : null, d = unit(din && dout ? add(din, dout) : din || dout);
      let u = cross(d, ref); if (len(u) < .05) u = cross(d, [ref[1], ref[2], ref[0]]); u = unit(u); const v = cross(d, u);
      const r = rs[i] * (din && dout ? Math.min(1.35, 1 / Math.max(.5, dot(d, dout))) : 1);   // a bend keeps its thickness
      if (i === 0 && capA) rings.push([add(pts[0], sc(d, -r * .92)), sc(u, r * .3), sc(v, r * .3)], [add(pts[0], sc(d, -r * .55)), sc(u, r * .82), sc(v, r * .82)]);
      rings.push([pts[i], sc(u, r), sc(v, r)]);
      if (i === m - 1 && capB) rings.push([add(pts[i], sc(d, r * .55)), sc(u, r * .82), sc(v, r * .82)], [add(pts[i], sc(d, r * .92)), sc(u, r * .3), sc(v, r * .3)]);
    }
    loft(ms, rings, n, col, fl, null, seed);
  }
  const jaw = zu => zu < 0 ? 1 - .3 * Math.pow(-zu, 1.4) : 1;          // the head narrows to the chin (character_test_02.py)
  function ball(ms, c, ax, ay, az, n, m, col, fl, seed, opt) {          // an ellipsoid with half-axes ax, ay, az. opt.taper: a head with a jaw; opt.keep(x, y, z): which part of the unit sphere to draw
    const rings = [];
    for (let k = 0; k <= m; k++) { const f = -Math.PI / 2 + Math.PI * k / m, cf = Math.max(.02, Math.cos(f)) * (opt && opt.taper ? jaw(Math.sin(f)) : 1); const sf = Math.sin(f), hd = opt && opt.hood && sf < 0;
      rings.push([add(c, sc(az, hd ? sf * 1.575 : sf)), sc(ax, cf), hd ? add(sc(ay, cf), sc(az, sf * .325)) : sc(ay, cf)]); }
    const keep = opt && opt.keep;
    loft(ms, rings, n, col, fl, c, seed, 0, keep ? (k, i) => { const f = -Math.PI / 2 + Math.PI * (k + .5) / m, a = TAU * (i + .5) / n, cf = Math.cos(f); return keep(cf * Math.cos(a), cf * Math.sin(a), Math.sin(f)); } : null);
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
    child: { g: "tee", top: ["teal", "ochre", "terra", "cream", "blue", "plum"], bottom: ["indigo", "olive", "blue", "brown"], shorts: .55, skirt: .25, hair: ["short", "bob", "afro", "long", "bun"], shoes: ["cream", "brown"], h: [1.12, 1.45], b: [.8, .92] },   // WLD-032
    utility: { g: "long", top: ["charcoal"], bottom: ["indigo"], cap: ["ochre"], hair: ["short"], shoes: ["black"], h: [1.66, 1.82], b: [1, 1.12] },
    binman: { g: "long", top: ["olive", "ochre"], bottom: ["charcoal"], cap: ["ochre"], hair: ["short", "side", "bun"], shoes: ["black"], h: [1.64, 1.84], b: [1, 1.14] },   // CITY-088: bins at dawn, parcels by day
    guard: { g: "long", top: ["indigo"], bottom: ["charcoal"], cap: ["charcoal"], hair: ["short", "side", "bun"], shoes: ["black"], h: [1.68, 1.84], b: [1, 1.14], tash: .3 },   // CITY-105: a job's own clothes (careers.json 'wear'): a guard's dark uniform and cap,
    janitor: { g: "long", top: ["teal"], bottom: ["teal"], cap: ["sand"], hair: ["short", "side", "bun", "bob"], shoes: ["brown"], h: [1.6, 1.8], b: [.94, 1.12] },   // a janitor's overalls, one colour from collar to ankle
    creative: { g: "tee", top: ["plum", "ochre", "teal", "terra", "cream"], bottom: ["indigo", "olive", "charcoal"], scarf: ["ochre", "terra", "plum"], scarfP: .45, bag: .35, hair: ["long", "afro", "bun", "side", "bob"], shoes: ["brown", "cream"], h: [1.6, 1.8], b: [.9, 1.06] },   // CITY-083: works from home
  };
  function look(id, role, sigName) {
    const R = ROLE[role] || ROLE.commuter, r = k => hash(id, k), pk = (list, k) => K[list[Math.min(list.length - 1, Math.floor(r(k) * list.length))]];
    const height = R.h[0] + (R.h[1] - R.h[0]) * r("h");
    return { S: height / 1.72, W: (R.b[0] + (R.b[1] - R.b[0]) * r("b")) * 1.04, skin: SKINS[Math.floor(r("s") * SKINS.length)], hair: R.hair[Math.floor(r("hs") * R.hair.length)],
      hairC: R.grey ? [.5, .5, .51] : HAIRS[Math.floor(r("hc") * HAIRS.length)], eyes: EYES[Math.floor(r("e") * EYES.length)], g: R.g, top: pk(R.top, "t"), bottom: pk(R.bottom, "bt"),
      legs: R.skirt && r("sk") < R.skirt ? "skirt" : R.shorts && r("sh") < R.shorts ? "shorts" : "trousers", shoes: pk(R.shoes, "so"), apron: R.apron ? pk(R.apron, "ap") : null, cap: R.cap ? pk(R.cap, "cp") : null,
      bag: R.bag && r("bg") < R.bag, scarf: R.scarf && r("sf") < (R.scarfP || 1) ? pk(R.scarf, "sc") : null, tash: R.tash && r("ta") < R.tash, sig: SIG[sigName] || SIG.Easy, seed: Math.floor(r("seed") * 1000) };
  }
  const FM0 = { face_w: 1, eye_size: 1, eye_open: 1, brow: 0, nose: 1, lips: 1 };
  function design(v, d) {                                             // v: one character's designer values; d: World/rules/designer.json
    const P = d.palette, c = k => v[k] ? P[v[k]] : null;
    return { S: v.height / 1.72, W: v.build * 1.04, skin: d.skin[v.skin], hair: v.hair, hairC: d.hair_color[v.hair_color], eyes: d.eyes[v.eyes], g: v.garment,
      top: P[v.top], bottom: P[v.bottom], legs: v.bottom_type, shoes: P[v.shoes], scarf: c("scarf"), cap: c("cap"), apron: c("apron"), bag: !!v.bag, tash: !!v.tash,
      sig: SIG[v.walk] || SIG.Springy, seed: 7, fm: { face_w: v.face_w, eye_size: v.eye_size, eye_open: v.eye_open, brow: v.brow, nose: v.nose, lips: v.lips } };
  }
  const cast = () => [                                                // left to right as in Art/Reference/People/PeopleLineup.png
    { S: 1.68 / 1.72, W: .92 * 1.04, skin: [.62, .42, .30], hair: "long", hairC: [.28, .10, .05], eyes: [.14, .36, .20], g: "tee", top: K.teal, bottom: K.indigo, legs: "trousers", shoes: K.cream, scarf: K.ochre, sig: SIG.Springy, seed: 7 },
    { S: 1.80 / 1.72, W: 1.06 * 1.04, skin: [.30, .19, .13], hair: "afro", hairC: K.black, eyes: [.2, .1, .05], g: "tunic", top: K.ochre, bottom: K.olive, legs: "shorts", shoes: [.6, .57, .5], bag: true, sig: SIG.Easy, seed: 11 },
    { S: 1.72 / 1.72, W: 1.14 * 1.04, skin: [.78, .56, .44], hair: "side", hairC: [.10, .06, .04], eyes: [.2, .1, .05], g: "long", top: K.cream, bottom: K.brown, legs: "trousers", shoes: [.1, .08, .06], apron: K.terra, tash: true, sig: SIG.Easy, seed: 13, fm: Object.assign({}, FM0, { face_w: 1.05, nose: 1.15 }) },
    { S: 1.78 / 1.72, W: 1.0 * 1.04, skin: [.84, .64, .52], hair: "short", hairC: [.55, .40, .18], eyes: [.18, .34, .55], g: "tee", top: K.blue, bottom: K.charcoal, legs: "shorts", shoes: K.terra, cap: K.terra, bag: true, sig: SIG.Hurried, seed: 17 },
    { S: 1.58 / 1.72, W: 1.06 * 1.04, skin: [.70, .50, .38], hair: "bun", hairC: [.55, .55, .56], eyes: [.2, .1, .05], g: "coat", top: K.plum, bottom: K.olive, legs: "skirt", shoes: K.brown, scarf: K.cream, sig: SIG.Careful, seed: 19 },
    { S: 1.74 / 1.72, W: .96 * 1.04, skin: [.47, .31, .22], hair: "bob", hairC: K.black, eyes: [.2, .1, .05], g: "coat", top: K.indigo, bottom: K.charcoal, legs: "trousers", shoes: K.black, sig: SIG.Easy, seed: 23 },
  ];
  const player = () => ({ S: 1.68 / 1.72, W: .92 * 1.04, skin: [.62, .42, .30], hair: "long", hairC: [.28, .10, .05], eyes: [.14, .36, .20], g: "tee", top: K.teal, bottom: K.indigo, legs: "trousers", shoes: K.cream, scarf: K.ochre, sig: SIG.Springy, seed: 7 });

  // pose: 0 stand, 1 walk, 2 sit, 3 lie. o: { ph: cycle phase 0..1, t: seconds, lod: 0 near | 1 mid | 2 far, fl: material, look: [yaw, pitch] the head adds, sig: override, sleep }
  // lod 3, far away (45 m and more): a figure of a few blocks in the person's own colours, legs striding when walking. At that
  // distance it is a few pixels tall and reads the same as the full body, for a hundredth of the cost (the 60 fps work).
  function far(ms, x, y, z, yaw, L, pose, ph, fl) {
    const S = L.S, W = L.W, B = { 2: null }, legC = L.legs === "skirt" ? L.skin : L.bottom;
    if (pose === 3) { ms.box(x - .22 * S, y + .05, z - .9 * S, x + .22 * S, y + .3, z + .8 * S, L.top, fl, B); return; }   // lying down
    // sitting, the origin is the seat top (sit()), so the body starts there and the head ends about 1.04 S above it, as near
    const sit = pose === 2, hip = sit ? .2 * S : .88 * S, top = hip + .5 * S, fx = Math.sin(yaw), fz = -Math.cos(yaw);   // the way they face (draw's -ey)
    if (!sit) for (const e of [-1, 1]) {                                // two legs, a stride apart when walking
      const sw = pose === 1 ? Math.sin(ph * 6.283) * .16 * e * S : 0, lx = x + fx * sw - fz * e * .07 * S, lz = z + fz * sw + fx * e * .07 * S;
      ms.box(lx - .065 * S, y, lz - .065 * S, lx + .065 * S, y + hip, lz + .065 * S, legC, fl, B);
    }
    const w = .19 * W * S;
    ms.box(x - w, y + (sit ? .02 * S : hip), z - w, x + w, y + top, z + w, L.top, fl, B);   // the body
    ms.box(x - .1 * S, y + top + .04 * S, z - .1 * S, x + .1 * S, y + top + .3 * S, z + .1 * S, L.skin, fl, B);   // the head
    ms.box(x - .11 * S, y + top + .22 * S, z - .11 * S, x + .11 * S, y + top + .33 * S, z + .11 * S, L.cap || L.hairC, fl, B);   // the hair
  }

  // A skateboard (CITY-098, CITY-099; the look of Tools/blender/skateboards_35.py): a rounded deck with its kicktails up, grip
  // on top, two trucks and four wheels; the electric board a battery box under the deck and a status light, the Neon Boost glowing
  // wheels, a glowing strip under the deck edge and a booster at the tail. C: the middle of the deck's top; f: nose-forward,
  // up: the deck's normal, side: across; K: the kind (deck_m, width_m, top_m and the colours, World/rules/boards.json).
  function board(ms, C, f, up, side, K, lod, fl, sd) {
    const Lh = K.deck_m / 2, Wh = K.width_m / 2, th = .012, wr = K.wheel_m / 2, at = (a, b, u) => add(add(add(C, sc(f, a)), sc(side, b)), sc(up, u));
    const seg = lod === 0 ? 12 : 6, rg = lod === 0 ? 4 : 3, glow = K.glow;
    OSKIP = 0;
    ball(ms, at(0, 0, -th), sc(side, Wh), sc(f, Lh * .82), sc(up, th), seg, rg, K.deck, fl, sd);   // the deck
    for (const e of [-1, 1]) ball(ms, add(at(e * Lh * .8, 0, -th + .012), sc(up, 0)), sc(side, Wh * .92), sc(f, Lh * .24), add(sc(up, th), sc(f, e * th * 2.2)), seg, rg, K.deck, fl, sd + 1);   // nose and tail, kicked up
    ball(ms, at(0, 0, -th * .2), sc(side, Wh * .9), sc(f, Lh * .78), sc(up, th * .35), seg, rg, K.grip, fl, sd + 2);   // the grip
    const tz = -(K.top_m - wr - th * 2), wb = K.wheelbase_m / 2;
    for (const e of [-1, 1]) {
      ball(ms, at(e * wb, 0, (tz - th * 2) / 2 - th), sc(side, Wh * .35), sc(f, .03), sc(up, Math.abs(tz) / 2), 6, 3, K.truck, fl, sd + 3);   // the truck
      for (const g of [-1, 1]) ball(ms, at(e * wb, g * K.track_m / 2, -K.top_m + wr), sc(side, wr * .5), sc(f, wr), sc(up, wr), lod === 0 ? 10 : 6, rg, K.wheel, fl, sd + 4 + g);   // the wheels
    }
    if (K.battery && lod < 2) { ball(ms, at(0, 0, -th * 2 - .022), sc(side, Wh * .6), sc(f, Lh * .45), sc(up, .022), 8, 3, K.battery, fl, sd + 6);   // the battery box
      if (K.band) ball(ms, at(0, 0, -th * 2 - .022), sc(side, Wh * .62), sc(f, .02), sc(up, .023), 8, 3, K.band, fl, sd + 7);
      if (K.light) ball(ms, at(Lh * .3, Wh * .6, -th * 2 - .015), sc(side, .008), sc(f, .008), sc(up, .008), 6, 3, K.light, fl, sd + 8); }
    if (glow && lod < 2) ball(ms, at(0, 0, -th * 2.3), sc(side, Wh * 1.02), sc(f, Lh * .84), sc(up, .004), seg, 3, glow, fl, sd + 9);   // the strip of light under the edge
    if (K.boost && lod < 2) { limb(ms, at(-Lh * .9, 0, .01), at(-Lh * 1.02, 0, .02), .026, .032, 8, K.truck, fl, true, f, sd + 10);
      ball(ms, at(-Lh * 1.03, 0, .02), sc(side, .022), sc(f, .005), sc(up, .022), 8, 3, K.core || glow, fl, sd + 11); }   // the booster
  }
  function draw(ms, x, y, z, yaw, L, pose, o) {
    if (o.lod === 3) return far(ms, x, y, z, yaw, L, pose, o.ph || 0, o.fl);
    const sig = o.sig || L.sig, g = geo(L.S), S = L.S, Sh = S * 1.22, W = L.W, T = 1.12 * S, lod = o.lod, fl = o.fl, t = o.t, sd = L.seed;
    const ff = o.ff == null ? 1 : o.ff, midFace = lod === 1 || (lod === 2 && ff > .02);   // ff: how much of the face shows (play.js fades it in with distance)
    const wd = wander(t, sd * .37), lk = o.look || [0, 0], hm = [wd[0] + lk[0], wd[1] + lk[1] + (o.face ? o.face.nod || 0 : 0), wd[2]];   // a speaker nods on stressed words (ANIM-006)
    TEX = lod < 2 ? 1 : 0; SKINREF = L.skin; OCC = null; OSKIP = 0; HEROSKIP = 0;
    const P = pose === 5 ? ride(sig, g, W, t, hm, sd, o.ride) : pose === 1 ? walk(sig, g, W, o.ph, hm, o.mod) : pose === 2 ? sit(sig, g, W, t, hm, sd, o.mod) : stand(sig, g, W, t, pose === 3 ? [0, 0, 0] : hm, sd, o.mod);
    // where the character's axes point in the world: standing (x left, y back, z up) or lying on the back, head to +z
    const c = Math.cos(yaw), s = Math.sin(yaw), ex = pose === 3 ? [-1, 0, 0] : [-c, 0, -s], ey = pose === 3 ? [0, -1, 0] : [-s, 0, c], ez = pose === 3 ? [0, 0, 1] : [0, 1, 0];
    const O = pose === 3 ? [x, y + .1 * S, z - .86 * S] : [x, y, z];
    const Wp = p => [O[0] + ex[0] * p[0] + ey[0] * p[1] + ez[0] * p[2], O[1] + ex[1] * p[0] + ey[1] * p[1] + ez[1] * p[2], O[2] + ex[2] * p[0] + ey[2] * p[1] + ez[2] * p[2]];
    const Wd = p => [ex[0] * p[0] + ey[0] * p[1] + ez[0] * p[2], ex[1] * p[0] + ey[1] * p[1] + ez[1] * p[2], ex[2] * p[0] + ey[2] * p[1] + ez[2] * p[2]];
    const n = lod === 0 ? 16 : lod === 1 ? 8 : 5, nl = lod === 0 ? 12 : lod === 1 ? 6 : 4, caps = lod === 0, fat = 1.34 * S, fwd = Wd([0, -1, 0]);
    const long = L.g !== "tee", legCol = L.legs === "skirt" ? L.skin : L.bottom;
    // ---- the body, part for part as Tools/blender/character_face_05.py builds the people of Art/Reference/People/PeopleLineup.png (option B: limbs x1.12).
    // Heights there are in the same skeleton: hips .8835, shoulders and yoke +.436 above them, neck +.451.
    const kind = L.legs, bare = kind !== "trousers";
    if (lod === 0) {                                                  // the body's big shapes, for self-shadow: 1 head, 2 hair or hat, 4 torso, 8 and 16 arms, 32 and 64 legs
      const fw0 = (L.fm || FM0).face_w, ohx = sc(Wd(mv(P.Rh, [1, 0, 0])), fw0), ohy = Wd(mv(P.Rh, [0, 1, 0])), ohz = Wd(mv(P.Rh, [0, 0, 1])), ohc = add(Wp(P.headJ), sc(ohz, .156 * Sh));
      const at = (a, b, c2) => add(ohc, add(add(sc(ohx, a * Sh), sc(ohy, b * Sh)), sc(ohz, c2 * Sh))), ell = (id, ctr, rx, ry, rz, keep) => oEll(id, ctr, sc(ohx, rx * Sh), sc(ohy, ry * Sh), sc(ohz, rz * Sh), keep);
      const nsz = (L.fm || FM0).nose * .72, noseO = ell(128, at(0, -.108, -.0226), .0115 * nsz, .013 * nsz, .017 * nsz), occ = [ell(1, ohc, .104, .11, .14), noseO], hs = L.hair; let hairO = null, brimO = null, stId = 0; const lockO = [];
      if (L.cap) { hairO = ell(2, at(0, .006, .016), .134, .142, .168, (x, y, z) => z >= .12); brimO = ell(2, at(0, -.135, .067), .09, .075, .012); stId = 5; occ.push(hairO, brimO); }
      else if (hs === "afro") { hairO = ell(2, at(0, .012, .035), .159, .165, .2, (x, y, z) => !(z < -.5 || (y < -.35 && z < .22))); stId = 4; occ.push(hairO); }
      else if (hs !== "bald") { stId = hs === "side" ? 2 : hs === "bob" || hs === "long" ? 3 : 1; occ.push(hairO = ell(2, at(0, .012, .014), .124, .132, .16, hs === "side" ? (x, y, z) => !(z < -.35 || (y < -.25 && z < .3 + .28 * x) || (y < .25 && z < .05))
          : hs === "bob" || hs === "long" ? (x, y, z) => !(z < -.8 || (y < -.3 && z < .38)) : (x, y, z) => !(z < -.35 || (y < -.25 && z < .45) || (y < .25 && z < .05))));
        if (hs === "long") { occ.push(ell(2, at(0, .05, -.12), .12, .085, .17)); for (const e of [-1, 1]) { const lk = oCap(2, at(e * .106, 0, 0), at(e * .1, -.018, -.19), .019 * Sh); lockO.push(lk); occ.push(lk); } } }
      const otx = Wd(mv(P.Rt, [1, 0, 0])), oty = Wd(mv(P.Rt, [0, 1, 0])), otz = Wd(mv(P.Rt, [0, 0, 1])), osp = Wp(P.spine);
      occ.push(oEll(4, add(osp, sc(otz, .11 * S)), sc(otx, .18 * W * S), sc(oty, .118 * W * S), sc(otz, .22 * S)));
      for (const k of [0, 1]) { const a = Wp(P.sh[k]), b = Wp(P.el[k]), w = Wp(P.wr[k]), hp = Wp(P.hip[k]), kn = Wp(P.kn[k]), an = Wp(P.an[k]);
        occ.push(oCap(8 << k, add(a, sc(sub(b, a), .22)), b, .048 * T), oCap(8 << k, b, w, .04 * T), oCap(32 << k, add(hp, sc(sub(kn, hp), .2)), kn, .08 * T), oCap(32 << k, kn, an, .064 * T)); }
      OCC = occ;
      const dE = Math.hypot(ohc[0] - EYE[0], ohc[1] - EYE[1], ohc[2] - EYE[2]);
      if (!heroBest || dE < heroBest.d) heroBest = { d: dE, c: ohc };
      const hero = !HERO && heroPrev && Math.hypot(ohc[0] - heroPrev[0], ohc[1] - heroPrev[1], ohc[2] - heroPrev[2]) < .25;
      HEROSKIP = hero ? 130 : 0;
      if (hero) HERO = { zone: hairO || ell(0, at(0, .012, .014), .124, .132, .16), st: stId, nose: noseO, brim: brimO, locks: lockO };
    }
    // capsule: the script's limb() with flat (depth as a share of width) and cap (how round the ends are), built on the hips below the waist and on the chest above
    const capsule = (z0, r0, z1, r1, flat, cap, col, seed, y0, y1, xo, sway, lip) => {   // lip: the bottom is a hem that turns under, not a rounded end
      const rings = [], at = (z, r, k) => { const f = (z - z0) / (z1 - z0), y = (y0 || 0) + ((y1 || 0) - (y0 || 0)) * Math.min(1, Math.max(0, f)), d = r * flat * k + (sway ? sway * Math.max(0, 1 - f * 2.2) : 0);
        rings.push(z < .1 ? [add(pel, add(add(sc(pz, z * S), sc(py, y * S)), sc(px, (xo || 0) * S))), sc(px, r * k * S), sc(py, d * S)] : [add(sp, add(add(sc(tz, (z - .098) * S), sc(ty, y * S)), sc(tx, (xo || 0) * S))), sc(tx, r * k * S), sc(ty, d * S)]); };
      if (lip) at(z0 + .004, r0, .84); else { at(z0 - r0 * cap * .92, r0, .3); at(z0 - r0 * cap * .55, r0, .82); } at(z0, r0, 1);
      for (const f of [.25, .5, .75]) at(z0 + (z1 - z0) * f, r0 + (r1 - r0) * f, 1);
      at(z1, r1, 1); at(z1 + r1 * cap * .55, r1, .82); at(z1 + r1 * cap * .92, r1, .3);
      loft(ms, rings, n, col, fl, null, seed);
    };
    // legs: one form from hip to ankle, bending at the knee; bare legs are thinner, shorts cover the thigh; shoes are rounded ovals
    for (const k of [0, 1]) {
      const hip = Wp(P.hip[k]), kn = Wp(P.kn[k]), an = Wp(P.an[k]); OSKIP = 32 << k;
      if (bare) tube(ms, [hip, kn, an], [.08 * T, .0625 * T, .045 * T], nl, L.skin, fl, fwd, sd + k, lod < 2, false);
      else tube(ms, [hip, kn, an], [.088 * T, .073 * T, .058 * T], nl, L.bottom, fl, fwd, sd + k, lod < 2, false);
      if (kind === "shorts") limb(ms, hip, add(hip, sc(sub(kn, hip), .82)), .092 * T, .086 * T, nl, L.bottom, fl, lod < 2, fwd, sd + k + 2);
      const he = Wp(P.heel[k]), to = Wp(P.toe[k]), f = unit(sub(to, he)), up0 = Wd([0, 0, 1]), side = unit(cross(f, up0)), up = cross(side, f);
      ball(ms, add(add(he, sc(f, .105 * T)), sc(up, .004 * S)), sc(side, .05 * T), sc(f, .12 * T), sc(up, .045 * T), lod === 0 ? 10 : 6, lod === 0 ? 6 : 4, L.shoes, fl, sd + k + 4);

    }
    const tx = Wd(mv(P.Rt, [1, 0, 0])), ty = Wd(mv(P.Rt, [0, 1, 0])), tz = Wd(mv(P.Rt, [0, 0, 1])), px = Wd(mv(P.Rp, [1, 0, 0])), py = Wd(mv(P.Rp, [0, 1, 0])), pz = Wd(mv(P.Rp, [0, 0, 1]));
    const pel = Wp(P.pel), sp = Wp(P.spine), swing = Math.abs(P.an[0][1] - P.an[1][1]);
    const chest = .172 * W, flat = .62 * (1 + .25 * .12), front = -chest * flat;
    // hips, in the colour of what is worn below
    OSKIP = 4;
    if (kind !== "skirt") ball(ms, add(pel, sc(py, -.01 * T)), sc(px, .16 * W * S), sc(py, .098 * T), sc(pz, .1156 * S), n, lod === 0 ? 6 : 4, L.bottom, fl, sd + 13);   // front where it was; the back
    // only as deep as the legs, so from behind no seat stands out over them with a crease round where they leave it
    // the garment: a tee or long top to the waist, a tunic to the thigh, a skirt under either, a coat over it all
    const hem = z => pose === 2 ? Math.max(z, -.06) : z;           // seated, a hem ends at the seat instead of hanging down through it (and out under a car)
    // a skirt under a tee or a tunic goes up only a little past its hem, and inside it (drawn to the chest, it stood out through
    // a tunic's front); under a coat, as before
    const topR = z => L.g === "tunic" ? (.19 * W + (chest - .19 * W) * (z + .1235) / .545) : (.142 * W + (chest - .142 * W) * (z - .088) / .3334);
    const SK = L.g === "coat" ? [-.5, .21 * W, .4214, chest - .004] : L.g === "tunic" ? [-.44, .225 * W, 0, topR(0) - .012] : [-.44, .225 * W, .2, topR(.2) - .012];
    if (kind === "skirt") capsule(hem(SK[0]), SK[1], SK[2], SK[3], flat, .28, L.bottom, sd + 9, 0, 0, 0, swing * .3, true);
    const coat = L.g === "coat", outer = L.top;
    if (coat) { capsule(hem(kind === "skirt" ? -.29 : -.33), .218 * W, .431, chest + .014, flat * 1.06, .25, L.top, sd, .006, .006, 0, swing * .3, true);
      ball(ms, add(sp, sc(tz, .387 * S)), sc(tx, .085 * T), sc(ty, .08 * T), sc(tz, .035 * S), n, 4, L.top, fl, sd + 14); }   // the collar
    else if (L.g === "tunic") capsule(hem(-.1235), .19 * W, .4214, chest, flat, .28, L.top, sd, 0, 0, 0, swing * .18, true);
    else capsule(.088, .142 * W, .4214, chest, flat, .28, L.top, sd, 0, 0, 0, 0, true);
    // the yoke: a rounded bar across the shoulders that the arms hang from
    const sh0 = Wp(P.sh[0]), sh1 = Wp(P.sh[1]);
    OSKIP = 28; limb(ms, sh1, sh0, .052 * T, .052 * T, nl, outer, fl, lod < 2, fwd, sd + 15);
    // arms: one form from shoulder to wrist; a tee has a sleeve to above the elbow; hands are long ovals
    for (const k of [0, 1]) {
      const a = k ? sh1 : sh0, b = Wp(P.el[k]), w = Wp(P.wr[k]), dir = unit(sub(w, b)); OSKIP = 8 << k;
      tube(ms, [a, b, w], [.052 * T, .0435 * T, .035 * T], nl, long ? outer : L.skin, fl, fwd, sd + k + 6, lod < 2, lod < 2);
      if (!long) limb(ms, a, add(a, sc(sub(b, a), .875)), .057 * T, .052 * T, nl, L.top, fl, lod < 2, fwd, sd + k + 8);
      if (lod < 2) ball(ms, add(w, sc(dir, .058 * S)), sc(tx, .03 * T), sc(ty, .042 * T), sc(dir, .055 * T), 8, 5, L.skin, fl, sd);
    }
    if (o.board && o.board.carry) {                                 // a board carried (CITY-099): held by its edge in the right hand, deck upright, wheels out
      const w = Wp(P.wr[1]), dn = Wd([0, 0, -1]), fw = unit(sub(fwd, sc(dn, dot(fwd, dn)))), out = cross(Wd([0, 0, 1]), fw), K = o.board.kind;
      board(ms, add(add(w, sc(dn, K.width_m / 2 - .01)), sc(out, .02)), fw, out, Wd([0, 0, 1]), K, lod, fl, sd + 40);
    }
    if (o.board && !o.board.carry) {                               // a board ridden (CITY-099): under the feet along the way of travel
      const K = o.board.kind; board(ms, Wp([0, 0, K.top_m]), Wd([0, -1, 0]), Wd([0, 0, 1]), Wd([1, 0, 0]), K, lod, fl, sd + 40);
    }
    // neck and head
    const fm = L.fm || FM0, hx = sc(Wd(mv(P.Rh, [1, 0, 0])), fm.face_w), hy = Wd(mv(P.Rh, [0, 1, 0])), hz = Wd(mv(P.Rh, [0, 0, 1])), nk = Wp(P.neck), hj = Wp(P.headJ), hc = add(hj, sc(hz, .156 * Sh));   // the head's centre, where character_face_05.py puts it
    // things on the front of the face follow the jaw as it narrows towards the chin
    const H = (a, b, cc) => { if (b < -.05) { cc *= 1.08; if (cc < 0) b *= 1 - .27 * Math.pow(Math.min(1, -cc / .14), 1.4); } return add(hc, add(add(sc(hx, a * Sh), sc(hy, b * Sh)), sc(hz, cc * Sh))); };
    const blob = (a, b, cc, rx, ry, rz, col, nn, mm2, s2, opt) => ball(ms, H(a, b, cc), sc(hx, rx * Sh), sc(hy, ry * Sh), sc(hz, rz * Sh), nn, mm2, col, fl, s2 || sd, opt);
    OSKIP = 4; limb(ms, nk, add(hj, sc(hz, .03 * S)), .046 * T, .044 * T, nl, L.skin, fl, false, fwd, sd); OSKIP = 1 | HEROSKIP;
    const RX = .104, RY = .11, RZ = .14;                              // the head of character_face_05.py, an egg with a jaw and ears; a little longer than its 0.13 at the owner's word
    blob(0, 0, 0, RX, RY, RZ, L.skin, lod === 0 ? 32 : midFace ? 10 : 6, lod === 0 ? 26 : midFace ? 7 : 4, 0, { taper: true });   // a head with a face keeps the mid divisions, so the face sits on it
    if (lod === 0) for (const e of [-1, 1]) blob(e * RX * .97, .012, -.012, .012, .022, .032, L.skin, 6, 4);
    // hair and hats are shells over the head with the face cut away (hair_cap() in character_test_02.py), so the fringe is a clean line
    OSKIP = 3;
    const hn = lod === 0 ? 24 : midFace ? 10 : 6, hm2 = lod === 0 ? 16 : midFace ? 6 : 4, hairC = L.hairC, st = L.hair;
    const shell = (b, cc, rx, ry, rz, col, keep, s2, hood) => blob(0, b, cc, rx, ry, rz, col, hn, hm2, s2, { keep, hood });
    if (o.hood) {                                                     // a coat's hood up in the rain (CITY-080): the long-hair shell, a size up, in the coat's colour
      shell(.016, .018, RX + (st === "afro" ? .06 : .036), RY + (st === "afro" ? .06 : .04), RZ + (st === "afro" ? .06 : .034), o.hood, (x, y, z) => !(z < -.85 || (y < -.28 && z < .44)), sd + 23, true);
      blob(0, -.104, .118, .074, .03, .03, o.hood, hn, 5, sd + 23);   // the hood's front edge over the brow
    }
    else if (st === "afro") shell(.012, .035, RX + .055, RY + .055, RZ + .06, hairC, (x, y, z) => !(z < -.5 || (y < -.35 && z < .22)));
    else if (st !== "bald") {
      shell(.012, .014, RX + .02, RY + .022, RZ + .02, hairC,
        st === "side" ? (x, y, z) => !(z < -.35 || (y < -.25 && z < .3 + .28 * x) || (y < .25 && z < .05))
        : st === "bob" || st === "long" ? (x, y, z) => !(z < -.8 || (y < -.3 && z < .38))
        : (x, y, z) => !(z < -.35 || (y < -.25 && z < .45) || (y < .25 && z < .05)), 0, st === "long");
      if (st === "short" || st === "side") blob(st === "side" ? .022 : 0, -.018, .108, .086, .104, .052, hairC, hn, 5);
      // long hair is the same shell hanging on as a hood, further down the back than at the sides (hair_cap() in character_test_02.py), with a lock each side
      // of the face; what shows between the face and the locks is the dark inside of the hood, not loose pieces
      if (st === "long" && lod < 2) for (const e of [-1, 1]) limb(ms, H(e * .106, 0, 0), H(e * .1, -.018, -.19), .021 * Sh, .013 * Sh, 10, hairC, fl, true, fwd, sd + 20);
      if (st === "bun") blob(0, .085, .135, .052, .052, .046, hairC, 8, 5);
    }
    if (L.cap && !o.hood) { shell(.006, .016, RX + .03, RY + .032, RZ + .028, L.cap, (x, y, z) => z >= .12, sd + 21); blob(0, -.135, .062, .09, .075, .009, L.cap, 8, 3, sd + 21); }
    if (o.umb) {                                                      // an umbrella up (CITY-080): held in the right hand, the canopy over the head
      const wr = Wp(P.wr[1]), up = Wd([0, 0, 1]), top = add(hc, sc(up, .36 * S)), rim = add(top, sc(up, -.13 * S)), R0 = .5 * S;
      const fx = unit(sub(Wd([1, 0, 0]), sc(up, dot(Wd([1, 0, 0]), up)))), fy = cross(up, fx), seg = lod === 0 ? 8 : 6;
      OSKIP = 0; limb(ms, wr, add(top, sc(up, .07 * S)), .009 * S, .007 * S, 5, [.12, .1, .09], fl, false, fwd, sd);
      const rings = [], under = [];
      for (const [h, r] of [[.13, .03], [.115, .2], [.08, .36], [.035, .46], [0, .5]]) { const c = add(rim, sc(up, h * S)); rings.push([c, sc(fx, r * S), sc(fy, r * S)]); under.unshift([add(c, sc(up, -.008 * S)), sc(fx, r * S * .98), sc(fy, r * S * .98)]); }
      loft(ms, rings, seg, o.umb, fl, add(rim, sc(up, -.35 * S)), sd + 31);           // panels: a soft octagon, normals out of the dome
      loft(ms, under, seg, sc(o.umb, .75), fl, add(rim, sc(up, .6 * S)), sd + 31);    // the underside, facing down
      if (R0 && lod === 0) for (let k = 0; k < seg; k++) { const a = TAU * k / seg, d = add(sc(fx, Math.cos(a)), sc(fy, Math.sin(a))); ball(ms, add(rim, sc(d, R0)), sc(fx, .008 * S), sc(fy, .008 * S), sc(up, .008 * S), 4, 3, [.12, .1, .09], fl, sd); }   // the rib tips
    }
    const tex0 = TEX; TEX = 0;                                       // the face is plain: eyes, lids and lips keep their own colours
    OSKIP = 1 | HEROSKIP;
    if (lod === 0 && o.face) face(blob, o.face, L, t, sd, lk, o.sleep);   // someone in a conversation: the full control set (CHR-007)
    else if (lod === 0) {                                             // the face: full eyes, brows, small nose, mouth at rest
      const period = 2.4 + 3.4 * ((sd * .618) % 1), bp = ((t + sd) % period) / .17, lid = o.sleep || SHUT ? 1 : bp < 1 ? Math.sin(Math.PI * bp) : 0, gx = cl(-lk[0] / rad(34), -1, 1) * .006, gz = cl(-lk[1] / rad(34), -1, 1) * .004;
      const es = fm.eye_size * 1.15, eo = fm.eye_open, ns = fm.nose;   // the reference's eyes are 1.15 times natural size
      // Eyes exactly as pose() in Tools/blender/character_face_05.py builds them, in head units: a round white set 5 mm into the face and turned to
      // follow the head (outer corner a little up), a large iris and pupil lying on it and resting low, a glint, and a dark shape just behind and
      // above the white that shows as the lash line. Finely divided, so every edge is round. Brows are tapered strokes on the face.
      const ex = .043, ez = .008 * RZ / .13, ew = .0245 * es, eh = .021 * es * eo, ir = .0155 * es, irz = Math.min(ir, eh * .97), yawE = Math.asin(Math.min(ex / RX, 1)) * .55, tilt = .1;
      const FS = (x, z, out) => { const zu = z / RZ, sj = jaw(zu); return [x, -RY * sj * Math.sqrt(Math.max(1 - (x / (RX * sj)) ** 2 - zu * zu, 0)) - out, z]; };   // a point on the face
      const HR = p => add(hc, add(add(sc(hx, p[0] * Sh), sc(hy, p[1] * Sh)), sc(hz, p[2] * Sh)));
      PLAIN = true;
      for (const e of [-1, 1]) {
        const th = e * yawE, ph = -e * tilt, c = Math.cos(th), sn = Math.sin(th), cp = Math.cos(ph), sp2 = Math.sin(ph);
        const R = [[c * cp, sn * cp, -sp2], [-sn, c, 0], [c * sp2, sn * sp2, cp]];                 // where the eye's own across, out and up point, in head axes
        const ax = k => add(add(sc(hx, R[k][0]), sc(hy, R[k][1])), sc(hz, R[k][2])), AX = ax(0), AY = ax(1), AZ = ax(2);
        const part = (ctr, rx, ry, rz, col, nn, mm2, opt) => ball(ms, ctr, sc(AX, rx * Sh), sc(AY, ry * Sh), sc(AZ, rz * Sh), nn, mm2, col, fl, sd, opt);
        const wc = FS(e * ex, ez, -.005);
        const onEye = (u, w, lift) => { const uu = cl(u / ew, -.9, .9), ww = cl(w / eh, -.9, .9), d = -(.011 * Math.sqrt(Math.max(.05, 1 - uu * uu - ww * ww)) + lift);
          return HR([wc[0] + R[0][0] * u + R[1][0] * d + R[2][0] * w, wc[1] + R[0][1] * u + R[1][1] * d + R[2][1] * w, wc[2] + R[0][2] * u + R[1][2] * d + R[2][2] * w]); };
        if (lid >= .03) {                                              // the upper lid of pose(): a skin shape that grows down over the eye from its top edge, with the lash line on its lower edge
          part(HR(FS(e * ex, ez + eh * (1 - lid), .005)), ew * 1.03, .0055, Math.max(eh * lid * 1.08, .0004), L.skin, 28, 10);
          part(HR(FS(e * ex, ez + eh * (1 - 2.08 * lid) - .001, .0075)), ew * 1.04, .005, Math.max(eh, .019) * .1, [.03, .02, .02], 24, 6);
          if (lid > .97) continue;
        } else part(HR(FS(e * ex, ez + .0012, -.005)), ew * 1.05, .0092, eh * 1.05, [.03, .02, .02], 28, 16, { keep: (x, y, z) => z > .18 && y < .3 });   // open: the lash line is a thin dark arc over the top of the white
        part(HR(wc), ew, .011, eh, [.78, .76, .72], 28, 14);                                                // the white
        part(onEye(gx, gz - .001 * es, .0003), ir, .0014, irz, L.eyes, 28, 6);
        part(onEye(gx, gz - .001 * es, .0013), .0068 * es, .001, Math.min(.0068 * es, irz), [.03, .02, .02], 24, 5);
        part(onEye(gx + .005, gz + .005 - .001 * es, .002), .0034, .001, .0034, [.78, .76, .72], 12, 4);
      }
      PLAIN = false;
      const by = (.008 + .0445 * Math.max(1, es * .9)) * RZ / .13 + fm.brow;
      for (const e of [-1, 1]) limb(ms, HR(FS(e * (ex - .02), by, .0045)), HR(FS(e * (ex + .025), by + .004, .0045)), .0048 * Sh, .0032 * Sh, 12, sc(hairC, .6), fl, true, hy, sd);
      TEX = tex0; OSKIP = 129 | HEROSKIP;
      ball(ms, HR(FS(0, -.021 * RZ / .13, .003)), sc(hx, .0115 * ns * .72 * Sh), sc(hy, .013 * ns * .72 * Sh), sc(hz, .017 * ns * .72 * Sh), 18, 12, L.skin, fl, sd);
      TEX = 0; OSKIP = 1 | HEROSKIP; PLAIN = true;
      cheeks(blob, L);
      blob(0, -.1, -.053, .032 * fm.lips, .005, .0062, [.34, .13, .11], 10, 3);
      if (L.tash) blob(0, -.104, -.037, .024, .006, .007, hairC, 6, 3);
      PLAIN = false;
    } else if (midFace) {                                             // the middle distance (to 16 m): the same face in few divisions, eyes,
      const fc = c => ff >= 1 ? c : [L.skin[0] + (c[0] - L.skin[0]) * ff, L.skin[1] + (c[1] - L.skin[1]) * ff, L.skin[2] + (c[2] - L.skin[2]) * ff];   // beyond 16 m faded into the skin
      const es = fm.eye_size * 1.15, ex = .043, ez = .008 * RZ / .13, ew = .0245 * es, eh = .021 * es * fm.eye_open, ir = .0155 * es;   // brows and lips where lod 0 has them
      const FS = (x, z, out) => { const zu = z / RZ, sj = jaw(zu); return [x, -RY * sj * Math.sqrt(Math.max(1 - (x / (RX * sj)) ** 2 - zu * zu, 0)) - out, z]; };
      const HR = p => add(hc, add(add(sc(hx, p[0] * Sh), sc(hy, p[1] * Sh)), sc(hz, p[2] * Sh)));
      PLAIN = true;
      for (const e of [-1, 1]) {
        ball(ms, HR(FS(e * ex, ez, -.005)), sc(hx, ew * Sh), sc(hy, .011 * Sh), sc(hz, eh * Sh), 10, 6, fc([.78, .76, .72]), fl, sd);   // the white
        ball(ms, HR(FS(e * ex, ez - .001 * es, .0045)), sc(hx, ir * Sh), sc(hy, .002 * Sh), sc(hz, Math.min(ir, eh * .97) * Sh), 10, 4, fc(L.eyes), fl, sd);   // the iris
        ball(ms, HR(FS(e * ex, ez - .001 * es, .0058)), sc(hx, .0068 * es * Sh), sc(hy, .0015 * Sh), sc(hz, Math.min(.0068 * es, ir) * Sh), 8, 3, fc([.03, .02, .02]), fl, sd);   // the pupil
        ball(ms, HR(FS(e * ex, ez + eh * .95, -.002)), sc(hx, ew * 1.05 * Sh), sc(hy, .004 * Sh), sc(hz, .003 * Sh), 10, 3, fc([.03, .02, .02]), fl, sd);   // the lash line
      }
      PLAIN = false;
      const by = (.008 + .0445 * Math.max(1, es * .9)) * RZ / .13 + fm.brow;
      for (const e of [-1, 1]) limb(ms, HR(FS(e * (ex - .02), by, .0045)), HR(FS(e * (ex + .025), by + .004, .0045)), .0048 * Sh, .0032 * Sh, 6, fc(sc(hairC, .6)), fl, true, hy, sd);
      ball(ms, HR(FS(0, -.021 * RZ / .13, .003)), sc(hx, .0115 * fm.nose * .72 * Sh), sc(hy, .013 * fm.nose * .72 * Sh), sc(hz, .017 * fm.nose * .72 * Sh), 8, 5, L.skin, fl, sd);   // the nose, as lod 0 has it
      PLAIN = true;
      cheeks(blob, L, 6, 3, ff);
      blob(0, -.1, -.053, .032 * fm.lips, .005, .0062, fc([.34, .13, .11]), 8, 3);
      if (L.tash) blob(0, -.104, -.037, .024, .006, .007, fc(hairC), 6, 3);
      PLAIN = false;
    }
    TEX = tex0; OSKIP = 4;
    // What the role wears on top (C5) lies on the outside of the clothes under it. wear(z, x) is that outside at height z (the garment
    // capsules' units: S above the hips below .1, along the chest above) and x across: [front, back] depth in S. Without it the apron
    // sat inside a coat, its straps and the bag strap ran straight through the chest, and the scarf's tail sank into a coat.
    const layers = [];                                                // [z0, r0, z1, r1, flat, cap, back offset, sway]: as the capsules above
    if (kind === "skirt") layers.push([hem(SK[0]), SK[1], SK[2], SK[3], flat, .28, 0, swing * .3]);
    if (coat) layers.push([hem(kind === "skirt" ? -.29 : -.33), .218 * W, .431, chest + .014, flat * 1.06, .25, .006, swing * .3]);
    else if (L.g === "tunic") layers.push([hem(-.1235), .19 * W, .4214, chest, flat, .28, 0, swing * .18]);
    else layers.push([.088, .142 * W, .4214, chest, flat, .28, 0, 0]);
    const wear = (z, x) => {
      let f = 0, b = 0;
      if (kind !== "skirt") { const e = 1 - (z / .1156) ** 2 - (x / (.16 * W)) ** 2; if (e > 0) { const r = .1098 * Math.sqrt(e); f = r + .0112; b = r - .0112; } }   // the hips
      for (const [z0, r0, z1, r1, fl0, cap, yo, sw] of layers) {
        if (z < z0) continue;
        const q = Math.min(1, (z - z0) / (z1 - z0)), c = z > z1 ? (z - z1) / (r1 * cap * .92) : 0; if (c >= 1) continue;
        const k = Math.sqrt(1 - c * c), rx = (r0 + (r1 - r0) * q) * k, ex = 1 - (x / rx) ** 2; if (ex <= 0) continue;
        const d = (rx * fl0 + (sw ? sw * Math.max(0, 1 - q * 2.2) : 0)) * Math.sqrt(ex);
        f = Math.max(f, d - yo); b = Math.max(b, d + yo);
      }
      return [f, b];
    };
    const Tp = (z, x, y) => z < .1 ? add(pel, add(add(sc(pz, z * S), sc(py, y * S)), sc(px, x * S))) : add(sp, add(add(sc(tz, (z - .098) * S), sc(ty, y * S)), sc(tx, x * S)));
    const Np = (x, y, z) => add(nk, add(add(sc(tx, x * S), sc(ty, y * S)), sc(tz, z * S)));   // round the neck, over the yoke
    // the apron: a flat bib from the chest to the thigh, hung so it clears the clothes and the legs as they swing forward
    let apY = null; const AZ0 = pose === 2 ? .08 : -.2565, AZ1 = .2842, apD = z => (.118 + (.1 - .118) * (z - AZ0) / (AZ1 - AZ0)) * W * .26;   // seated, it stops at the lap
    if (L.apron && lod < 2) {
      const legR = bare ? [.08, .0625, .045] : [.088, .073, .058], legFront = z => {     // the front of the legs at height z, as the bib's y (negative is forward)
        let m = 1; const zh = P.pel[2] + z * S;
        for (const k of [0, 1]) { const h = P.hip[k], kn = P.kn[k], an = P.an[k], up = zh >= kn[2];
          const a = up ? h : kn, b = up ? kn : an, q = cl((a[2] - zh) / ((a[2] - b[2]) || 1e-6), 0, 1), r = (up ? legR[0] + (legR[1] - legR[0]) * q : legR[1] + (legR[2] - legR[1]) * q) * T;
          if (zh <= h[2] + r) m = Math.min(m, (a[1] + (b[1] - a[1]) * q - r - P.pel[1]) / S); }
        return m;
      };
      const need = z => Math.min(-(wear(z, 0)[0] + apD(z) + .006), legFront(z) - apD(z) - .006);
      let y1 = 1; for (const z of [.2, .25, AZ1]) y1 = Math.min(y1, need(z));
      let y0 = need(AZ0); for (let z = AZ0 + .05; z < AZ1 - .04; z += .05) { const q = (z - AZ0) / (AZ1 - AZ0); y0 = Math.min(y0, (need(z) - y1 * q) / (1 - q)); }
      apY = z => y0 + (y1 - y0) * cl((z - AZ0) / (AZ1 - AZ0), 0, 1);
      capsule(AZ0, .118 * W, AZ1, .1 * W, .26, .2, L.apron, sd + 11, y0, y1, 0, 0);
      for (const e of [-1, 1]) {                                      // the straps: up from the bib's corners, over the chest, round the back of the neck
        const zc = .38, xs = e * .075 * W;
        tube(ms, [Tp(.28, e * .07 * W, y1 - apD(AZ1) * .6), Tp(zc, xs, -(wear(zc, xs)[0] + .014)), Np(e * .068, -.012, .062), Np(e * .045, .064, .034), Np(0, .07, .026)],
          [.008 * S, .008 * S, .008 * S, .008 * S, .008 * S], 4, L.apron, fl, fwd, sd, false, false);
      }
    }
    const over = (z, x) => {                                          // the outside at the front, apron and scarf tail included
      let f = wear(z, x)[0];
      if (apY && z > AZ0 - .02 && z < AZ1 + .02 && Math.abs(x) < .118 * W) f = Math.max(f, -apY(z) + apD(z));
      if (scarfY && z > SZ0 - .01 && z < SZ1 && Math.abs(x - .042) < .045) f = Math.max(f, -scarfY(z) + .012);
      return f;
    };
    const yokeF = z => Math.sqrt(Math.max(0, (.052 * 1.12) ** 2 - (z - .436) ** 2));   // the front of the yoke, the round bar across the shoulders
    // the scarf tail lies on the chest: a thin flat strip from under the wrap down the front, in short pieces that each follow
    // the chest's curve (one straight piece either cut through the bust or stood out from it like a stick, its top under the chin)
    const SZ0 = apY ? AZ1 + .035 : .2893, SZ1 = .47, sNeed = z => -(Math.max(wear(z, .042)[0], yokeF(z)) + .012);   // its middle, just off the front
    let scarfY = null, scarfSegs = [];
    if (L.scarf && lod < 2) {
      const n = 4, zs = [...Array(n + 1)].map((_, k) => SZ0 + (SZ1 - SZ0) * k / n);
      for (let k = 0; k < n; k++) {                                   // each piece's ends on the chest, moved out as far as its middle needs
        const a = zs[k], b = zs[k + 1], ya = sNeed(a), yb = sNeed(b), ex = Math.max(0, (ya + yb) / 2 - sNeed((a + b) / 2));
        scarfSegs.push([a, b, ya - ex, yb - ex]);
      }
      scarfY = z => { const g = scarfSegs.find(q => z <= q[1]) || scarfSegs[scarfSegs.length - 1]; return g[2] + (g[3] - g[2]) * cl((z - g[0]) / (g[1] - g[0]), 0, 1); };
    }
    if (L.scarf) {                                                    // a wrap round the neck and a flat tail down the front of the chest
      const big = coat ? 1.15 : 1;                                    // over a coat's collar
      ball(ms, add(nk, sc(tz, .049 * S)), sc(tx, .088 * T * big), sc(ty, .084 * T * big), sc(tz, .04 * S), hn, 4, L.scarf, fl, sd + 5);
      if (scarfSegs.length) {                                         // one strip through the pieces' ends (over an apron it ends above the bib)
        const pts = [[scarfSegs[0][0], scarfSegs[0][2]], ...scarfSegs.map((q, k) => [q[1], Math.min(q[3], k + 1 < scarfSegs.length ? scarfSegs[k + 1][2] : q[3])])];
        const r = .029, fl2 = .25, rings = [], ring = (z, y, k) => rings.push([add(sp, add(add(sc(tz, (z - .098) * S), sc(ty, y * S)), sc(tx, .042 * S))), sc(tx, r * k * S), sc(ty, r * fl2 * k * S)]);
        ring(pts[0][0] - r * .37, pts[0][1], .3); ring(pts[0][0] - r * .2, pts[0][1], .82);
        for (const [z, y] of pts) ring(z, y, 1);
        const e = pts[pts.length - 1]; ring(e[0] + r * .2, e[1], .82); ring(e[0] + r * .37, e[1], .3);
        loft(ms, rings, n, L.scarf, fl, null, sd + 5);
      }
    }
    if (L.bag && lod < 2) {                                           // the bag at the left hip, its strap over the right shoulder, lying on whatever is worn
      const bg = add(pel, add(sc(px, .225 * S), sc(py, -.1 * T))), sr = .012;
      ball(ms, bg, sc(px, .05 * S), sc(py, .1 * S), sc(pz, .09 * S), 8, 5, K.brown, fl, sd + 12);
      const pts = [add(bg, add(sc(pz, .085 * S), sc(py, -.02 * S)))];   // from the top of the bag, up across the front to the right shoulder
      for (const f of [.12, .3, .48, .66, .84]) { const z = .1 + .33 * f, x = (.16 - .26 * f) * W; pts.push(Tp(z, x, -(over(z, x) + sr + .004))); }
      pts.push(Tp(.515, -.1, 0));                                     // over the shoulder, above the yoke
      for (const f of [.16, .45, .75]) { const z = .43 - .31 * f, x = (-.1 + .26 * f) * W; pts.push(Tp(z, x, wear(z, x)[1] + sr + .004)); }   // and down the back
      pts.push(add(bg, add(sc(pz, .075 * S), sc(py, .06 * S))));       // to the back of the bag
      tube(ms, pts, pts.map(() => sr * S), 4, K.brown, fl, fwd, sd, false, false);
    }
  }
  // The face control set of docs/art/CHARACTER_FACE.md §1, drawn in head units (about metres on a natural head). f: { mouth: { width,
  // opening, round, teeth 0 none | 1 upper | 2 both, tongue }, curve, brow_l, brow_r: [inner, outer], lid, squint, open, gaze: [x, z] },
  // mouth sizes already at the larger-than-life drawing scale. Lips are a ring; the dark inside, teeth and tongue sit in the plane
  // of the opening (CHARACTER_FACE §1 construction notes).
  function cheeks(blob, L, n = 10, m = 4, f = 1) {                                  // rosy cheeks (ADR-0010): a redder version of the person's own skin
    const c = L.cheek || (L.cheek = [Math.min(L.skin[0] * 1.1, .9), L.skin[1] * .66, L.skin[2] * .62]);
    for (const e of [-1, 1]) blob(e * .056, -.0835, -.027, .0175, .0062, .0115, f >= 1 ? c : c.map((v, k) => L.skin[k] + (v - L.skin[k]) * f), n, m);
  }
  const MOUTH = { lip: [.34, .13, .11], dark: [.11, .035, .035], teeth: [.9, .88, .82], tongue: [.62, .26, .26] };
  function face(blob, f, L, t, sd, lk, sleep) {
    const period = 2.4 + 3.4 * ((sd * .618) % 1), blink = !sleep && ((t + sd) % period) < .17;
    const gx = cl(-lk[0] / rad(34), -1, 1) * .006 + f.gaze[0], gz = cl(-lk[1] / rad(34), -1, 1) * .004 + f.gaze[1];
    const fm = L.fm || FM0, es = fm.eye_size, lid = sleep || blink ? 1 : f.lid, op = f.open * fm.eye_open, sq = f.squint;
    for (const e of [-1, 1]) {                                         // e = +1 is the character's left
      const ex = e * .042;
      if (lid > .97) { blob(ex, -.0975, .012, .0225 * es, .0105, .0235 * es, L.skin, 8, 4, sd + 3); continue; }
      // the open part of the eye: a squint lifts its lower edge, the upper lid lowers its top edge (the face skin shows where it closes)
      const R0 = .022 * op * es, top = .012 + R0 * (1 - 2 * lid * .85), bot = .012 - R0 * (1 - sq * .9), rz = (top - bot) / 2, cz = (top + bot) / 2;
      blob(ex, -.0975, cz, .0215 * op * es, .008, rz, [.86, .84, .8], 8, 4);
      const ir = Math.min(.0172 * op * es, rz * .9), iz = Math.min(.0045 + gz, top - ir * .7) + (cz - .012) * .3;
      blob(ex + gx, -.1035, iz, .0172 * op * es, .004, ir, L.eyes, 8, 3);
      blob(ex + gx, -.1065, iz, .0078, .002, Math.min(.0078, ir * .55), [.01, .01, .012], 6, 3);
      if (top - iz > .006) blob(ex + gx + .005, -.108, iz + .0055, .0032, .0012, .0032, [1, 1, 1], 5, 2);
      if (lid >= .03) blob(ex, -.1062, top - .0008, .0205 * op, .0025, .0016, [.08, .06, .05], 8, 3);   // the lash line along the lowered lid
    }
    for (const e of [-1, 1]) {                                         // each brow: inner and outer end move on their own
      const b = e > 0 ? f.brow_l : f.brow_r, z0 = .051 + (op - 1) * .02 + fm.brow + (es - 1) * .02, bc = sc(L.hairC, .8);
      for (let k = 0; k < 5; k++) { const u = k / 4; blob(e * (.031 + .024 * u), -.1 + .0015 * u, z0 + .002 * u + b[0] + (b[1] - b[0]) * u, .0115, .0055, .0047, bc, 6, 3); }
    }
    blob(0, -.111, -.014, .0105 * fm.nose, .011 * fm.nose, .0115 * fm.nose, sc(L.skin, .93), 6, 4);   // nose
    cheeks(blob, L);
    const m = f.mouth, w = m.width * fm.lips * 1.3, oh = Math.max(m.opening, .0005), mz = -.053, rd = m.round || 0, dep = 1 + rd * .45, cu = cl(f.curve, -1, 1) * .0045;
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
  return { SIG, look, player, design, draw, stepLength, cast, setSun, hero: () => HERO, colour: n => K[n] };
})();
