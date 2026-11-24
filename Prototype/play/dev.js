"use strict";
// Dev mode: index.html?dev (a test switch like ?rich and ?bench, not a game rule; does nothing otherwise). A bar down the
// left with a button for every piece of feedback, so each can be seen and heard without playing up to it: reward coins of
// each size, the bank's interest, every achievement's banner, and each reward sound on its own.
//
// Nothing here moves money or earns anything: the buttons play the showing only (Rewards.show, Rewards.achieve), so the
// ledger and the save are untouched. The purse counts up as if paid, then goes back to the real balance.
//
// The game holds the mouse for looking round. The ` key (top left of the keyboard) lets it go without opening the menu,
// so the buttons can be clicked; a click on the town takes it back. Loaded before play.js.
var Dev = (() => {
  const api = { on: new URLSearchParams(location.search).has("dev"), free: false };
  if (!api.on) return api;
  const h = (tag, cls, text) => { const e = document.createElement(tag); if (cls) e.className = cls; if (text) e.textContent = text; return e; };
  const css = h("style"); css.textContent = `
    #dev { position: fixed; left: 12px; top: 250px; bottom: 64px; width: 196px; z-index: 40; display: flex; flex-direction: column; background: rgba(23,23,28,.86); border: 1px solid rgba(240,163,90,.5); border-radius: 8px; font: 13px Bahnschrift, "Segoe UI", sans-serif; color: #d9cfb6; }
    #dev.shut { bottom: auto; }
    #dev header { display: flex; align-items: center; justify-content: space-between; padding: 7px 10px; color: #f0a35a; font-weight: 600; letter-spacing: .06em; text-transform: uppercase; font-size: 12px; cursor: pointer; }
    #dev .scroll { overflow-y: auto; padding: 0 8px 8px; flex: 1; } #dev.shut .scroll, #dev.shut footer { display: none; }
    #dev h4 { margin: 10px 2px 5px; font-size: 11px; font-weight: 600; letter-spacing: .08em; text-transform: uppercase; color: #a79f8c; }
    #dev button { display: block; width: 100%; margin: 0 0 4px; padding: 6px 9px; text-align: left; font: inherit; color: #d9cfb6; background: #2a2a33; border: 1px solid #3d3d48; border-radius: 5px; cursor: pointer; }
    #dev button:hover { border-color: #f0a35a; color: #f0c35a; } #dev button:active { background: #f0a35a; color: #17171c; }
    #dev footer { padding: 7px 10px; font-size: 11px; line-height: 1.4; color: #a79f8c; border-top: 1px solid #34343d; } #dev kbd { color: #f0a35a; border: 1px solid rgba(240,163,90,.6); border-radius: 3px; padding: 0 5px; }
    body.designing #dev { display: none; }`;
  document.head.appendChild(css);
  const bar = h("div"), head = h("header", "", "Dev · feedback"), list = h("div", "scroll"), foot = h("footer");
  bar.id = "dev"; head.appendChild(h("span", "", "▾")); foot.innerHTML = "<kbd>`</kbd> frees the mouse; click the town to take it back. Nothing here changes your money or your save.";
  bar.append(head, list, foot); document.body.appendChild(bar);
  head.addEventListener("click", () => { bar.classList.toggle("shut"); head.lastChild.textContent = bar.classList.contains("shut") ? "▸" : "▾"; });
  for (const ev of ["mousedown", "mouseup", "click", "wheel"]) bar.addEventListener(ev, e => e.stopPropagation());   // the bar's clicks are not the game's

  const group = t => list.appendChild(h("h4", "", t));
  function button(text, fn) {
    const b = h("button", "", text); b.type = "button"; list.appendChild(b);
    b.addEventListener("click", () => { if (!window.Life || !Life.ready) { foot.textContent = "Start the game first (Walk into town)."; return; } Sound.start(); fn(); b.blur(); });
  }
  const bx = c => "Bx " + (c / 100).toLocaleString("en-US");
  function fill() {
    const tiers = window.WASTED.sound.rewards.coins.reward_tiers;
    group("Quest or job reward");
    tiers.forEach(([least, coins]) => { const amount = Math.max(least, 500); button(bx(amount) + " · " + coins + " coins", () => Rewards.show("reward", amount)); });
    group("Bank");
    button("Interest · " + bx(1200), () => Rewards.show("interest", 1200));
    group("Achievements");
    for (const a of Achievements.list()) button(a.name, () => Rewards.achieve(a));
    button("Three in a row", () => Achievements.list().slice(0, 3).forEach(a => Rewards.achieve(a)));
    group("Sounds only");
    for (const [name, what, i, n] of [["Reward fanfare", "quest_start"], ["Coin springs out", "coin_out"], ["Coin lands", "coin_land"], ["Coin taken (low)", "coin_take", 0, 8], ["Coin taken (high)", "coin_take", 7, 8],
      ["Reward chord", "quest_end"], ["Till drawer", "interest_start"], ["Interest coin", "interest_coin", 2], ["Interest bell", "interest_end"], ["Achievement fanfare", "achievement"]]) button(name, () => Sound.reward(what, i, n));
  }
  fill();

  window.addEventListener("keydown", e => {                // ` lets the mouse go without opening the menu (play.js asks Dev.free)
    if (e.code !== "Backquote" || !document.pointerLockElement) return;
    e.preventDefault(); api.free = true; document.exitPointerLock();
  }, true);
  document.addEventListener("pointerlockchange", () => { if (document.pointerLockElement) api.free = false; });
  return api;
})();
