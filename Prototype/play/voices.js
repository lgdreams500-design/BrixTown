"use strict";
// Spoken dialogue (SND-018, ADR-0019). Every line was voiced in advance by Tools/voice/make_dialogue_voices.py, in pieces
// cut at the line's blanks, for every voice that can say it; audio/voices/index.js lists the pieces (window.VOICES).
// Voices.speak(who, template, vars, mood) looks up the pieces of a line for that speaker (a resident's index, or
// "player"), plays them one after another and says how long it will take, so the mouth can finish with the voice.
// The index names each resident's voice by their id (ADR-0060), so it still fits after the town's list has changed; a
// resident who is not in it (a role with no voices yet) has no voice and their lines stay wordless syllables.
// A blank with no piece (a typed player name other than the default) is a short pause; the caption still shows it.
// Loaded before conversations.js and play.js; plays through <audio> so it also works when index.html is opened as a file.
var Voices = (() => {
  const V = window.VOICES;
  if (!V) return { ready: false, speak: () => null, stop() { }, plan: () => null };
  const DIR = "audio/voices/";
  // a piece as it is looked up: no leading punctuation or spaces, single spaces (the generator does the same)
  const norm = t => t.replace(/^[\s,;:.!?—-]+/, "").replace(/\s+/g, " ").trim();
  const speakable = t => /[A-Za-z0-9]/.test(t);
  let playing = [], timer = null, playerVoice = V.player_default;
  try { playerVoice = localStorage.getItem("wasted.voice") || V.player_default; } catch (e) { }
  const idOf = i => { const r = window.WASTED && window.WASTED.residents[i]; return r ? r.id : undefined; };   // the resident's id (city_data.js)
  const cfgOf = who => who === "player" ? V.configs[V.player[playerVoice] !== undefined ? V.player[playerVoice] : V.player[V.player_default]] : V.configs[V.residents[idOf(who)]];
  function plan(who, template, vars, mood) {             // [[file, seconds] | [null, gap]] and the total length
    const cfg = cfgOf(who); if (!cfg) return null;
    const out = []; let found = 0;
    for (const tok of template.split(/(\{\w+\})/)) {
      const m = /^\{(\w+)\}$/.exec(tok);
      const text = m ? (vars && vars[m[1]] !== undefined ? String(vars[m[1]]) : "") : tok;
      const p = norm(text); if (!speakable(p)) continue;
      const clip = V.clips[cfg + "|" + mood + "|" + p] || V.clips[cfg + "|neutral|" + p];
      if (clip) { out.push(clip); found++; } else out.push([null, V.play.blank_gap_s]);
    }
    if (!found) return null;
    const dur = out.reduce((a, c) => a + c[1], 0) + V.play.gap_s * (out.length - 1);
    return { cfg, parts: out, dur };
  }
  function stop() {
    clearTimeout(timer); timer = null;
    for (const a of playing) { try { a.pause(); } catch (e) { } }
    playing = [];
  }
  function speak(who, template, vars, mood) {
    const pl = plan(who, template, vars, mood || "neutral"); if (!pl) return null;
    stop();
    const on = !window.Sound || !Sound.state || Sound.state().on !== false;
    const els = pl.parts.map(([f]) => { if (!f) return null; const a = new Audio(DIR + f); a.preload = "auto"; a.volume = on ? V.play.volume : 0; return a; });
    playing = els.filter(Boolean);
    let k = 0;
    const next = () => {
      if (k >= els.length) { playing = []; return; }
      const a = els[k], gap = pl.parts[k][1]; k++;
      if (!a) { timer = setTimeout(next, gap * 1000); return; }
      a.onended = () => { timer = setTimeout(next, V.play.gap_s * 1000); };
      a.onerror = () => { timer = setTimeout(next, gap * 1000); };
      a.play().catch(() => { timer = setTimeout(next, gap * 1000); });
    };
    next();
    return { dur: pl.dur, parts: pl.parts.length, cfg: pl.cfg };
  }
  return {
    ready: true, speak, stop, plan,
    get playerVoice() { return playerVoice; },
    set playerVoice(v) { if (V.player[v] !== undefined) { playerVoice = v; try { localStorage.setItem("wasted.voice", v); } catch (e) { } } },
    playerVoices: () => Object.keys(V.player),
  };
})();
