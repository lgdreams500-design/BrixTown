"use strict";
/* The look bench (ADR-0032, ADR-0035): one designed house of the slice on its own, to try its line, colourway, colours and
   furnishing level and see them as the game draws them. Open index.html?bench (or ?bench=<lot id>); the panel's choices
   go into the page's address and the page loads again with them, so what you see is the real build of the house, a few
   seconds after each change. Nothing is saved: "Copy colourway" puts the colourway as JSON on the clipboard, to paste into
   World/rules/house_styles.json (then run Tools/worldgen/build_play.py). Colours can only be the palette's (ART_DIRECTION
   2.3); a new colour needs a look test and the owner's approval (ADR-0004).
   Loaded after city_data.js and before play.js: the first part changes the house's data before the town is built; the
   second, on page load, holds the camera and draws the panel. Without ?bench in the address it does nothing. */
window.Bench = (() => {
  const q = new URLSearchParams(location.search);
  if (!q.has("bench")) return null;
  const D = window.WASTED, S = D.house_styles;
  const lots = D.blocks.flatMap(b => b.lots);
  const lot = lots.find(l => l.id === (q.get("bench") || "b_0_0_l7")) || lots.find(l => l.house && l.house.interior);
  const B = { lot: lot.id, view: q.get("view") || "outside", level: +q.get("level") || 0 };
  // the house as asked: its line, colourway and any colour tried in place of the colourway's own
  if (q.get("line") && S.lines[q.get("line")]) lot.house.furniture = q.get("line");
  if (q.get("style") && S.colourways[lot.house.furniture][q.get("style")]) lot.house.style = q.get("style");
  const line = lot.house.furniture, style = lot.house.style || "a", way = S.colourways[line][style];
  for (const part of ["wall", "shutter", "door", "frame", "roof"]) if (q.get(part)) way[part] = q.get(part);
  B.line = line; B.style = style; B.way = way;

  const VIEWS = {                                             // where to stand: Blender x, y of the plan, storey, and which way to look
    outside: null,
    "Living room": [-3.45, -3.9, 0, 0, 1], Kitchen: [-1.6, 0.95, 0, -1, 1], Hall: [0.1, -3.9, 0, 0, 1], "Dining room": [3.75, -3.9, 0, 0, 1],
    Bathroom: [2.2, 0.95, 0, 1, 1], "Main bedroom": [-3.45, -3.9, 1, 0, 1], Study: [-1.6, 0.95, 1, -1, 1], Landing: [0.1, -3.9, 1, 0, 1],
    "Children's room": [2.1, -3.9, 1, 1, 1], "Guest room": [2.2, 0.95, 1, 1, 1],
  };
  function frame() {                                          // the house's own frame as houses.js has it: Blender (u, v) to the world
    const [x0, z0, x1, z1] = lot.rect, cx = (x0 + x1) / 2, cz = (z0 + z1) / 2, t = (lot.angle || 0) * Math.PI / 180, c = Math.cos(t), s = Math.sin(t);
    return { cx, cz, P: (u, v) => [cx - u * c - v * s, cz - u * s + v * c] };
  }
  function go(change) {
    const p = new URLSearchParams(location.search);
    for (const [k, v] of Object.entries(change)) v === null || v === "" ? p.delete(k) : p.set(k, v);
    location.search = p.toString();
  }
  function panel() {
    const css = document.createElement("style");
    css.textContent = `#start, #keys, #track { display: none !important; }
      #bench { position: fixed; top: 12px; left: 12px; z-index: 50; width: 300px; max-width: calc(100vw - 24px); background: rgba(24,22,20,.9); color: #eee6dc;
               font: 13px/1.45 system-ui, sans-serif; border-radius: 10px; padding: 12px 14px; box-shadow: 0 6px 24px rgba(0,0,0,.35); }
      #bench h2 { margin: 0 0 6px; font-size: 15px; } #bench label { display: grid; grid-template-columns: 92px 1fr; align-items: center; gap: 8px; margin: 5px 0; }
      #bench select, #bench button { font: inherit; background: #3a342e; color: #eee6dc; border: 1px solid #5a5048; border-radius: 6px; padding: 3px 6px; }
      #bench button { cursor: pointer; margin: 8px 6px 0 0; } #bench .note { color: #a99d90; font-size: 12px; margin-top: 8px; }
      #bench .sw { display: inline-block; width: 12px; height: 12px; border-radius: 3px; vertical-align: -2px; margin-right: 4px; border: 1px solid #0005; }`;
    document.head.appendChild(css);
    const el = document.createElement("div"); el.id = "bench";
    const rgb = c => "rgb(" + c.map(v => Math.round(255 * Math.pow(Math.max(0, v), 1 / 2.2))).join(",") + ")";   // linear to screen, roughly
    const opt = (list, cur, label) => list.map(k => `<option value="${k}"${k === cur ? " selected" : ""}>${label ? label(k) : k}</option>`).join("");
    const parts = [["wall", S.lines[line].stone ? ["stone"] : Object.keys(S.plasters)], ["shutter", Object.keys(S.palette)], ["door", Object.keys(S.palette)],
                   ["frame", Object.keys(S.palette)], ["roof", Object.keys(S.roofs)]];
    const col = (part, k) => part === "wall" ? (k === "stone" ? [.56, .53, .5] : S.plasters[k][1]) : part === "roof" ? S.roofs[k][1] : S.palette[k];
    el.innerHTML = `<h2>Look bench: ${lot.name}</h2>
      <label>House line<select data-k="line">${opt(Object.keys(S.lines), line, k => S.lines[k].name)}</select></label>
      <label>Colourway<select data-k="style">${opt(Object.keys(S.colourways[line]), style, k => k + ": " + S.colourways[line][k].name)}</select></label>
      ${parts.map(([p, list]) => `<label><span><span class="sw" style="background:${rgb(col(p, way[p]))}"></span>${p}</span><select data-k="${p}">${opt(list, way[p])}</select></label>`).join("")}
      <label>Furnishing<select data-k="level">${opt(["", "1", "2", "3", "4"], B.level ? String(B.level) : "", k => k ? k + " " + ({ 1: "Bare", 2: "Basic", 3: "Homely", 4: "Full" })[k] : "the household's")}</select></label>
      <label>View<select data-k="view">${opt(Object.keys(VIEWS), B.view)}</select></label>
      <button data-a="copy">Copy colourway</button><button data-a="reset">Back to the data</button>
      <div class="note">Each change loads the house again (a few seconds). Colours are the palette's only; copy the colourway into World/rules/house_styles.json and run build_play.py to keep it.</div>`;
    el.querySelectorAll("select").forEach(s => s.addEventListener("change", () => {
      const k = s.dataset.k, ch = { [k]: s.value };
      if (k === "line") Object.assign(ch, { style: null, wall: null, shutter: null, door: null, frame: null, roof: null });
      if (k === "style") Object.assign(ch, { wall: null, shutter: null, door: null, frame: null, roof: null });
      go(ch);
    }));
    el.querySelector('[data-a="copy"]').addEventListener("click", ev => {
      const txt = `"${style}": ` + JSON.stringify({ name: way.name, wall: way.wall, shutter: way.shutter, door: way.door, frame: way.frame, roof: way.roof });
      (navigator.clipboard ? navigator.clipboard.writeText(txt) : Promise.reject()).then(() => { ev.target.textContent = "Copied"; }, () => { window.prompt("The colourway as JSON:", txt); });
    });
    el.querySelector('[data-a="reset"]').addEventListener("click", () => go({ line: null, style: null, wall: null, shutter: null, door: null, frame: null, roof: null, level: null }));
    document.body.appendChild(el);
  }
  window.addEventListener("load", () => {
    if (!window.wasted) return;
    panel();
    const W = window.wasted, F = frame(), v = VIEWS[B.view];
    W.clock.m = 11 * 60; W.clock.rate = 0;
    if (!v) {                                                 // outside: a slow turn round the house from the street side
      const [fx, fz] = F.P(0, -1);
      const base = Math.atan2(fx - F.cx, fz - F.cz);
      window.stepCamera = () => {
        const a = base + Math.sin(performance.now() / 9000) * .9, r = 20;
        W.cam.eye = [F.cx + Math.sin(a) * r, 7.5, F.cz + Math.cos(a) * r];
        const d = [F.cx - W.cam.eye[0], 3.2 - W.cam.eye[1], F.cz - W.cam.eye[2]], n = Math.hypot(...d); W.cam.dir = d.map(x => x / n);
      };
    } else {                                                  // inside: standing in the room's corner, looking across it
      const [u, vv, f, du, dv] = v, [ex, ez] = F.P(u, vv), [tx, tz] = F.P(u + du * 3, vv + dv * 3), y = f ? 3.6 + 1.62 : .3 + 1.62;
      window.stepCamera = () => {
        W.cam.eye = [ex, y, ez]; W.cam.indoor = true;
        const d = [tx - ex, y - .9 - y, tz - ez], n = Math.hypot(...d); W.cam.dir = d.map(x => x / n);
      };
    }
  });
  return B;
})();
