"""The catalog of Brix Town's look references (ADR-0061): checks Art/Reference/catalog.json against the folders and writes
the page to review them, Art/Reference/index.html (a tab per category, each reference with its pictures and details).

    python Tools/reference_catalog.py            check, then write the page (exits 1 and writes nothing on a problem)
    python Tools/reference_catalog.py --check    check only
    python Tools/reference_catalog.py --for <tour stop | lot id | place>   which reference that place is judged against
    python Tools/reference_catalog.py --tour [<tour list>]                 the same for every stop of a look tour

The rules it keeps (an AI agent should get from a thing to its reference in one step):
  - every reference file is in Art/Reference/<Category>/ and named <Category><Subject>[_<Variant>].<ext>, PascalCase
    (Indoor/IndoorHomeRoom_Morning.jpg, Vehicles/VehiclesSoftBox.png);
  - every file there is in the catalog, and every file the catalog names is there;
  - every reference says what it locks, when and by what decision it was approved, and what makes it;
  - retired references are in Art/Retired/, never in Art/Reference/;
  - every kind of place in the game has one line in 'places': its approved reference, or that it has none (a gap), and
    every stop of the look tour (Tools/unreal/light_tour.txt) belongs to exactly one place. A place with no reference
    is never judged against another place's: its reference is made first (docs/process/LOOK_GATE.md).
"""
import glob, html, json, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REF = os.path.join(ROOT, "Art", "Reference")
CATALOG = os.path.join(REF, "catalog.json")
TOUR = os.path.join(ROOT, "Tools", "unreal", "light_tour.txt")
PAGE = os.path.join(REF, "index.html")
KINDS = (".png", ".jpg", ".gif", ".mp4")
NEEDS = ("id", "category", "title", "files", "locks", "approved", "decision", "made_by")


def load(path=CATALOG):
    with open(path, encoding="utf-8") as f:
        return json.load(f)


def problems(cat, root=ROOT):
    """Everything wrong with the catalog and the folders, in plain words ([] when all is well)."""
    out, ref = [], os.path.join(root, "Art", "Reference")
    cats = [c["id"] for c in cat["categories"]]
    seen_ids, listed = set(), set()
    for r in cat["references"]:
        for k in NEEDS:
            if not r.get(k):
                out.append("%s: no %s" % (r.get("id", "?"), k))
        if r.get("id") in seen_ids:
            out.append("%s: the id is used twice" % r["id"])
        seen_ids.add(r.get("id"))
        c = r.get("category")
        if c not in cats:
            out.append("%s: no category %s" % (r.get("id"), c))
            continue
        if not str(r.get("id", "")).startswith(c):
            out.append("%s: the id does not start with its category %s" % (r.get("id"), c))
        for f in r.get("files", []):
            listed.add(f.replace("\\", "/"))
            folder, name = os.path.split(f)
            if folder != c:
                out.append("%s: %s is not in the %s folder" % (r["id"], f, c))
            if not re.fullmatch(re.escape(c) + r"[A-Z][A-Za-z0-9]*(_[A-Za-z0-9]+)?\.(png|jpg|gif|mp4)", name):
                out.append("%s: %s is not named <Category><Subject>[_<Variant>].<ext>" % (r["id"], f))
            if not name.startswith(r["id"]):
                out.append("%s: %s does not start with its reference's id" % (r["id"], f))
            if not os.path.isfile(os.path.join(ref, f)):
                out.append("%s: %s is missing" % (r["id"], f))
    for dirpath, _, names in os.walk(ref):
        for n in names:
            rel = os.path.relpath(os.path.join(dirpath, n), ref).replace("\\", "/")
            if n.lower().endswith(KINDS) and rel not in listed:
                out.append("%s is in Art/Reference but not in the catalog" % rel)
    for g in cat.get("gaps", []):
        if g.get("category") not in cats:
            out.append("a gap names no category: %s" % g.get("what"))
    for t in cat.get("retired", []):
        for f in t.get("files") or [t["file"]]:
            if not f.startswith("Art/Retired/"):
                out.append("%s: an archived reference belongs in Art/Retired/" % f)
            elif not os.path.isfile(os.path.join(root, f)):
                out.append("%s: the archived file is missing" % f)
    approved = {r.get("id") for r in cat["references"] if r.get("status") != "proposed"}
    keys, stops = set(), {}
    for p in cat.get("places", []):
        k = p.get("place", "?")
        if k in keys or not p.get("what"):
            out.append("place %s: named twice, or does not say what it is" % k)
        keys.add(k)
        for i in ([p["reference"]] if p.get("reference") else []) + p.get("also", []):
            if i not in approved:
                out.append("place %s: %s is not an approved reference" % (k, i))
        if not p.get("reference") and not p.get("gap"):
            out.append("place %s: no reference, and no gap saying why" % k)
        for s in p.get("stops", []):
            if s in stops:
                out.append("tour stop %s belongs to two places: %s and %s" % (s, stops[s], k))
            stops[s] = k
    tour = os.path.join(root, "Tools", "unreal", "light_tour.txt")
    if cat.get("places") and os.path.isfile(tour):
        for name in tour_stops(tour):
            if stop_key(name) not in stops:
                out.append("tour stop %s belongs to no place: add it to a place's stops in the catalog" % name)
    return out


# ---- from a place in the game to its reference -------------------------------------------------------------------------
WEATHER = {"rain": "Rain", "fog": "Fog", "cloud": "Cloudy", "snow": "Snow"}


def tour_stops(path=TOUR):
    """The names of a look tour's stops (Tools/unreal/light_tour.txt: name|hh:mm|weather|where)."""
    with open(path, encoding="utf-8") as f:
        return [l.split("|")[0].strip() for l in f if l.count("|") >= 3 and not l.lstrip().startswith("#")]


def stop_key(name):
    """alley_1200b -> alley, ref_shop_night -> ref_shop: a stop's name without its hour."""
    return re.sub(r"_(\d{4}b?|night)$", "", name)


def lot_of(lot_id, root=ROOT):
    for path in glob.glob(os.path.join(root, "World", "*", "blocks", "*.json")):
        with open(path, encoding="utf-8") as f:
            for lot in json.load(f).get("lots", []):
                if lot.get("id") == lot_id:
                    return lot
    return None


def places_for(cat, what, root=ROOT):
    """The places a tour stop, a lot id, a place's name or a reference's id belongs to (a home's lot is two: its room
    and its hallway). ([], why) when nothing is recorded for it."""
    places = cat.get("places", [])
    hit = [p for p in places if what in (p["place"], p.get("reference")) or what in p.get("also", [])]
    if hit:
        return hit, ""
    hit = [p for p in places if stop_key(what) in p.get("stops", [])]
    if hit:
        return hit, ""
    lot = lot_of(what, root) if re.match(r"^b_\d+_\d+_l\d+$", what) else None
    if lot:
        best, hit = 0, []
        for p in places:
            for rule in p.get("lots", []):
                score = (4 if rule.get("id") == lot["id"] else 0) or (3 if rule.get("fitout") and rule["fitout"] == lot.get("fitout") else 0) \
                    or (2 if rule.get("has") and rule["has"] in lot and lot.get("kind") in rule.get("kind", []) else 0) \
                    or (1 if not (rule.get("id") or rule.get("fitout") or rule.get("has")) and lot.get("kind") in rule.get("kind", []) else 0)
                if score > best:
                    best, hit = score, [p]
                elif score and score == best and p not in hit:
                    hit.append(p)
        if hit:
            return hit, ""
        return [], "lot %s (%s, %s) is a kind of place the catalog's 'places' does not list: add it there, with its reference or its gap" % (what, lot.get("kind"), lot.get("name"))
    return [], "%s is not a tour stop, a lot, a place or a reference the catalog knows" % what


def setting_file(ref, stop):
    """The one file of a reference a tour stop is judged against, when its name says the hour or the weather."""
    key, hour = stop_key(stop), re.search(r"_(\d{2})(\d{2})b?$", stop)
    want = WEATHER.get(key) or ("NightLamps" if stop.endswith("_night") else None)
    if not want and hour:
        h = int(hour.group(1)) + int(hour.group(2)) / 60.0
        want = "NightLamps" if h >= 21 or h < 5 else "Midday" if 11 <= h <= 13 else None
    for f in ref["files"]:
        if want and f.rsplit(".", 1)[0].endswith("_" + want):
            return f
    return None


def answer(cat, what, root=ROOT):
    """A few plain lines: what place that is and which reference it is judged against."""
    hit, why = places_for(cat, what, root)
    if not hit:
        return ["%s: NOT KNOWN. %s" % (what, why)]
    refs, out = {r["id"]: r for r in cat["references"]}, []
    if len(hit) > 1:
        out.append("%s is %d places: say which part before comparing anything." % (what, len(hit)))
    for p in hit:
        out.append("%s: %s. %s" % (what, p["place"], p["what"]))
        if not p.get("reference"):
            out.append("  NO REFERENCE. %s Make and approve its reference first; do not judge it against another place's." % p["gap"])
            continue
        r = refs[p["reference"]]
        one = setting_file(r, what)
        out.append("  reference %s (approved %s): %s" % (r["id"], r["approved"], "Art/Reference/" + one if one else "%d settings, Art/Reference/%s" % (len(r["files"]), r["files"][0].rsplit("_", 1)[0] + "_*")))
        if p.get("also"):
            out.append("  also: " + ", ".join(p["also"]))
        if r.get("rules"):
            out.append("  rules: " + r["rules"])
        if r.get("issues"):
            out.append("  %d known issues in the reference itself: read them in the catalog before calling a difference a defect" % len(r["issues"]))
        if p.get("note"):
            out.append("  note: " + p["note"])
    return out


def e(s):
    return html.escape(str(s), quote=True)


def code_or_text(s):
    """Paths and file names in backticks-free text shown as code."""
    return re.sub(r"((?:Tools|Game|World|Prototype|docs|Art)/[\w./()<>*\-]+|ADR-\d{4})", r"<code>\1</code>", e(s))


HOURS = {"Morning": "Morning", "Midday": "Midday", "Dusk": "Dusk", "NightLamps": "Night, lamps on",
         "NightFullMoon": "Night, lamps off, full moon", "NightNewMoon": "Night, lamps off, new moon", "NightFullMoonFireOut": "Night, lamps and fire off, full moon",
         "NightNewMoonFireOut": "Night, lamps and fire off, new moon", "Cloudy": "Cloudy",
         "Rain": "Rain", "Fog": "Fog", "Snow": "Snow"}       # the hours of Tools/blender/hours_50.py


def label(f):
    """What a picture shows, in words: its hour when it is one of a place's hours, else its variant (IndoorRooms_Living:
    Living; IndoorHouse12x9_Ground: Ground), else the whole picture."""
    stem = os.path.splitext(os.path.basename(f))[0]
    if "_" not in stem:
        return "The whole sheet"
    v = stem.rsplit("_", 1)[-1]
    return HOURS.get(v) or re.sub(r"(?<=[a-z])(?=[A-Z0-9])|(?<=[0-9])(?=[A-Z])", " ", v)


def caption(f):
    """A picture's label in words, and its file name under it."""
    return "<b>%s</b><br><code>%s</code>" % (e(label(f)), e(os.path.basename(f)))


def state(r):
    """For your review, or approved (and when): how the page marks a reference."""
    if r.get("status") == "proposed":
        return 0, "review", "For your review"
    d = re.search(r"\d{4}-\d{2}-\d{2}", r.get("approved", ""))
    return 1, "ok", "Approved" + (" " + d.group(0) if d else "")


def order(refs):
    """For your review first, then the latest approved at the top (by its date; of one day, the one listed last)."""
    def key(ir):
        i, r = ir
        if r.get("status") == "proposed":
            return (0, "", -i)
        d = re.search(r"\d{4}-\d{2}-\d{2}", r.get("approved", ""))
        return (1, "".join(chr(255 - ord(ch)) for ch in (d.group(0) if d else "0000-00-00")), -i)
    return [r for _, r in sorted(enumerate(refs), key=key)]


def media(f, alt):
    if f.endswith(".mp4"):
        return '<video src="%s" controls muted loop preload="metadata" title="%s"></video>' % (e(f), e(alt))
    return '<img src="%s" alt="%s" loading="lazy" data-full="%s">' % (e(f), e(alt), e(f))


def page(cat):
    cats = cat["categories"]
    tabs, panels = [], []
    for i, c in enumerate(cats + [{"id": "Retired", "title": "Archived", "about": "References taken out of use, kept for the record in Art/Retired/. Never build against these."},
                                  {"id": "Naming", "title": "How it is kept", "about": ""}]):
        refs = [r for r in cat["references"] if r["category"] == c["id"]]
        gaps = [g for g in cat.get("gaps", []) if g["category"] == c["id"]]
        n = len(refs) if c["id"] not in ("Retired", "Naming") else (len(cat.get("retired", [])) if c["id"] == "Retired" else "")
        tabs.append('<button role="tab" id="tab-%s" aria-controls="p-%s" aria-selected="%s" data-tab="%s">%s<span class="n">%s</span></button>'
                    % (c["id"], c["id"], "true" if i == 0 else "false", c["id"], e(c["title"]), n))
        body = ['<p class="about">%s</p>' % e(c["about"])] if c["about"] else []
        refs = order(refs)                                       # for review first, then the latest approved
        if refs:                                                  # the tab's list: pick an entry, jump to it
            body.append('<ol class="index">%s</ol>' % "".join(
                '<li><a href="#%s">%s<span><b>%s</b><code>%s</code><em class="st %s">%s</em><small>%d picture%s</small></span></a></li>'
                % (e(r["id"]), ('<img src="%s" alt="" loading="lazy">' % e(r["files"][0])) if not r["files"][0].endswith(".mp4") else "",
                   e(r["title"]), e(r["id"]), state(r)[1], e(state(r)[2]), len(r["files"]), "" if len(r["files"]) == 1 else "s")
                for r in refs))
        for r in refs:
            pics = "".join('<figure>%s<figcaption>%s</figcaption></figure>' % (media(f, r["title"]), caption(f)) for f in r["files"])
            rows = [("Locks", code_or_text(r["locks"])), ("Approved", e(r["approved"])), ("Decision", code_or_text(r["decision"])),
                    ("Made by", code_or_text(r["made_by"]))]
            if r.get("models"):
                rows.append(("Models built from it", "<ul>%s</ul>" % "".join("<li>%s</li>" % code_or_text(m) for m in r["models"])))
            if r.get("rules"):
                rows.append(("Rules", code_or_text(r["rules"])))
            if r.get("was"):
                rows.append(("Was called", ", ".join("<code>%s</code>" % e(w) for w in r["was"])))
            issues = "".join("<li>%s</li>" % code_or_text(x) for x in r.get("issues", []))
            n, cls, words = state(r)
            body.append('<article class="ref%s" id="%s"><header><h2>%s</h2><code class="rid">%s</code><em class="st %s">%s</em>%s</header>'
                        '<div class="pics">%s</div><dl>%s</dl>%s</article>'
                        % (" proposed" if cls == "review" else "", e(r["id"]), e(r["title"]), e(r["id"]), cls, e(words),
                           '<span class="flag">%d issue%s seen</span>' % (len(r["issues"]), "" if len(r["issues"]) == 1 else "s") if r.get("issues") else "",
                           pics, "".join("<dt>%s</dt><dd>%s</dd>" % (k, v) for k, v in rows),
                           '<div class="issues"><h3>Seen wrong, for the owner to judge</h3><ul>%s</ul></div>' % issues if issues else ""))
        if gaps:
            body.append('<section class="gaps"><h3>No reference yet</h3><ul>%s</ul></section>' % "".join(
                "<li><b>%s</b> %s</li>" % (e(g["what"]), code_or_text(g["why"])) for g in gaps))
        if c["id"] == "Retired":
            for t in reversed(cat.get("retired", [])):           # the latest archived at the top
                fs = t.get("files") or [t["file"]]
                body.append('<article class="ref retired"><header><h2>%s</h2></header><div class="pics">%s</div><dl><dt>Was</dt><dd>%s</dd><dt>Why archived</dt><dd>%s</dd></dl></article>'
                            % (e(t.get("title") or os.path.basename(fs[0])),
                               "".join('<figure>%s<figcaption>%s</figcaption></figure>' % (media("../Retired/" + f[len("Art/Retired/"):], t["was"]), caption(f)) for f in fs),
                               code_or_text(t["was"]), code_or_text(t["why"])))
        if c["id"] == "Naming":
            body.append('<article class="ref doc"><h2>How the references are kept</h2><ul>'
                        "<li>Every approved look reference is in <code>Art/Reference/&lt;Category&gt;/</code>, named <code>&lt;Category&gt;&lt;Subject&gt;[_&lt;Variant&gt;].&lt;ext&gt;</code> in PascalCase: <code>Indoor/IndoorHomeRoom_Morning.jpg</code>, <code>Vehicles/VehiclesSoftBox.png</code>. A reference's id is the name without the variant.</li>"
                        "<li><code>Art/Reference/catalog.json</code> is the one list: what each locks, its approval and decision, the script that makes it, the game models built from it, issues seen, its old names. Agents read it there.</li>"
                        "<li>This page is written from it by <code>python Tools/reference_catalog.py</code>, which first checks that every file is listed and named by the rule. Never edit the page by hand.</li>"
                        "<li>A new reference: render it with its script, put it in its category's folder under its name, add it to the catalog, run the script. Changing the look needs a new ADR and the owner's approval (ADR-0004).</li>"
                        "<li>A reference taken out of use moves to <code>Art/Retired/</code> and is listed under Retired.</li>"
                        "<li>Look tests and rejected ideas stay in <code>Art/StyleTests/</code>; they are not references.</li></ul></article>")
        panels.append('<section role="tabpanel" id="p-%s" aria-labelledby="tab-%s"%s>%s</section>' % (c["id"], c["id"], "" if i == 0 else " hidden", "".join(body) or '<p class="about">Nothing here yet.</p>'))
    total = sum(len(r["files"]) for r in cat["references"])
    return TEMPLATE.replace("{TABS}", "".join(tabs)).replace("{PANELS}", "".join(panels)).replace(
        "{COUNT}", "%d references in %d files, %d gaps" % (len(cat["references"]), total, len(cat.get("gaps", []))))


TEMPLATE = """<!doctype html>
<!-- Written by Tools/reference_catalog.py from Art/Reference/catalog.json (ADR-0061). Do not edit: change the catalog and run the script. -->
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>Brix Town references</title>
<style>
:root{--bg:#f6f1ea;--card:#fffaf3;--ink:#2b2420;--dim:#6f6259;--line:#e2d6c8;--accent:#a4532e;--flag:#b5462e;--flagbg:#f8e1d8;--code:#efe5d8}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--bg:#1d1916;--card:#26211d;--ink:#eee3d6;--dim:#b3a493;--line:#3a322b;--accent:#e08a5c;--flag:#f29a7c;--flagbg:#3d2620;--code:#332b25}}
:root[data-theme="dark"]{--bg:#1d1916;--card:#26211d;--ink:#eee3d6;--dim:#b3a493;--line:#3a322b;--accent:#e08a5c;--flag:#f29a7c;--flagbg:#3d2620;--code:#332b25}
*{box-sizing:border-box}body{margin:0;background:var(--bg);color:var(--ink);font:15px/1.5 "Segoe UI",system-ui,sans-serif}
header.top{padding:20px 16px 8px;max-width:1400px;margin:auto}h1{margin:0;font-size:24px}header.top p{margin:4px 0 0;color:var(--dim)}
nav{position:sticky;top:0;z-index:2;background:var(--bg);border-bottom:1px solid var(--line)}
nav div{max-width:1400px;margin:auto;padding:0 16px;display:flex;gap:4px;overflow-x:auto}
nav button{border:0;background:none;color:var(--dim);font:inherit;font-weight:600;padding:12px 12px 10px;border-bottom:3px solid transparent;cursor:pointer;white-space:nowrap}
nav button[aria-selected="true"]{color:var(--ink);border-bottom-color:var(--accent)}nav .n{margin-left:6px;font-weight:400;font-size:12px;color:var(--dim)}
main{max-width:1400px;margin:auto;padding:16px}.about{color:var(--dim);margin:0 0 16px}
.ref{background:var(--card);border:1px solid var(--line);border-radius:12px;padding:16px;margin:0 0 18px}
.ref header{display:flex;align-items:baseline;gap:10px;flex-wrap:wrap}.ref h2{margin:0;font-size:18px}
.rid{font-size:12px;color:var(--dim)}.flag{font-size:12px;color:var(--flag);background:var(--flagbg);padding:1px 8px;border-radius:10px}
.pics{display:grid;grid-template-columns:repeat(auto-fill,minmax(260px,1fr));gap:10px;margin:12px 0}
.pics figure{margin:0}.pics img,.pics video{width:100%;border-radius:8px;display:block;background:#000;cursor:zoom-in}
figcaption{font-size:12px;color:var(--dim);margin-top:3px;word-break:break-all}
dl{display:grid;grid-template-columns:170px 1fr;gap:4px 14px;margin:0}dt{color:var(--dim)}dd{margin:0}dd ul{margin:0;padding-left:18px}
code{background:var(--code);padding:0 4px;border-radius:4px;font-size:.9em;word-break:break-word}
.issues{margin-top:12px;border-left:3px solid var(--flag);padding:4px 12px;background:var(--flagbg);border-radius:0 8px 8px 0}
.issues h3,.gaps h3{margin:4px 0;font-size:14px}.issues ul,.gaps ul{margin:4px 0 8px;padding-left:18px}
.gaps{border:1px dashed var(--line);border-radius:12px;padding:8px 16px;margin-bottom:18px}
.retired img{filter:grayscale(.6)}
.prop{font-size:12px;color:#fff;background:var(--accent);padding:1px 8px;border-radius:10px}.ref.proposed{border-color:var(--accent);border-width:2px}
figcaption b{color:var(--ink);font-weight:600;font-size:13px}
.index{list-style:none;padding:0;margin:0 0 20px;display:grid;grid-template-columns:repeat(auto-fill,minmax(300px,1fr));gap:8px}
.index a{display:flex;gap:10px;align-items:center;text-decoration:none;color:var(--ink);background:var(--card);border:1px solid var(--line);border-radius:10px;padding:6px}
.index a:hover{border-color:var(--accent)}.index img{width:96px;height:60px;object-fit:cover;border-radius:6px;background:#000;flex:none}
.index span{display:flex;flex-direction:column;gap:1px;min-width:0}.index b{font-size:14px}.index code{font-size:11px;align-self:flex-start}.index small{color:var(--dim);font-size:12px}
.st{font-style:normal;font-size:12px;padding:1px 8px;border-radius:10px;align-self:flex-start}.st.review{color:#fff;background:var(--accent)}.st.ok{color:var(--dim);background:var(--code)}
#big{position:fixed;inset:0;background:rgba(0,0,0,.9);display:none;flex-direction:column;align-items:center;justify-content:center;z-index:5}
#big.on{display:flex}#big img{max-width:92vw;max-height:84vh;border-radius:6px}
#big .cap{color:#eee;margin-top:10px;font-size:14px;text-align:center;padding:0 60px}#big .cap .k{color:#aaa;margin-left:8px}
#big button{position:absolute;border:0;background:rgba(255,255,255,.12);color:#fff;font-size:30px;line-height:1;width:52px;height:64px;border-radius:10px;cursor:pointer}
#big button:hover{background:rgba(255,255,255,.25)}#big button:disabled{opacity:.25;cursor:default}
#big .prev{left:14px;top:calc(50% - 32px)}#big .next{right:14px;top:calc(50% - 32px)}#big .close{right:14px;top:14px;width:44px;height:44px;font-size:24px}
@media (max-width:640px){#big button.prev,#big button.next{width:40px;height:52px;font-size:24px}}
@media (max-width:640px){dl{grid-template-columns:1fr}dt{margin-top:6px}}
</style></head><body>
<header class="top"><h1>Brix Town references</h1><p>The look every made thing is checked against. {COUNT}. Written from <code>Art/Reference/catalog.json</code>.</p></header>
<nav><div role="tablist">{TABS}</div></nav>
<main>{PANELS}</main>
<div id="big" role="dialog" aria-label="Picture at full size"><button class="close" title="Close (Esc)">&#x2715;</button>
<button class="prev" title="Previous (left arrow)">&#x2039;</button><img alt=""><button class="next" title="Next (right arrow)">&#x203A;</button>
<div class="cap"></div></div>
<script>
const tabs=[...document.querySelectorAll('[role=tab]')];
function show(id){tabs.forEach(t=>{const on=t.dataset.tab===id;t.setAttribute('aria-selected',on);document.getElementById('p-'+t.dataset.tab).hidden=!on});try{history.replaceState(null,'','#'+id)}catch(e){}}
tabs.forEach(t=>t.addEventListener('click',()=>show(t.dataset.tab)));
const want=(location.hash||'').slice(1);if(want){const t=tabs.find(t=>t.dataset.tab===want);if(t)show(want);else{const a=document.getElementById(want);if(a){show(a.closest('[role=tabpanel]').id.slice(2));a.scrollIntoView()}}}
// the full-size view: the pictures of one reference in turn, with back and next (buttons, the arrow keys, a swipe)
const big=document.getElementById('big'),bigImg=big.querySelector('img'),cap=big.querySelector('.cap'),prev=big.querySelector('.prev'),next=big.querySelector('.next');
let set=[],at=0;
function open(i){bigImg.src=set[i].dataset.full;at=i;const fc=set[i].closest('figure').querySelector('figcaption');
  cap.innerHTML=(fc?fc.innerHTML:'')+'<span class="k">'+(i+1)+' / '+set.length+'</span>';prev.disabled=i===0;next.disabled=i===set.length-1;big.classList.add('on')}
function step(d){const i=at+d;if(i>=0&&i<set.length)open(i)}
document.querySelectorAll('img[data-full]').forEach(i=>i.addEventListener('click',()=>{set=[...i.closest('.pics').querySelectorAll('img[data-full]')];open(set.indexOf(i))}));
prev.addEventListener('click',ev=>{ev.stopPropagation();step(-1)});next.addEventListener('click',ev=>{ev.stopPropagation();step(1)});
big.querySelector('.close').addEventListener('click',()=>big.classList.remove('on'));
big.addEventListener('click',ev=>{if(ev.target===big)big.classList.remove('on')});
document.addEventListener('keydown',ev=>{if(!big.classList.contains('on'))return;if(ev.key==='Escape')big.classList.remove('on');else if(ev.key==='ArrowLeft')step(-1);else if(ev.key==='ArrowRight')step(1)});
let x0=null;big.addEventListener('touchstart',ev=>{x0=ev.touches[0].clientX},{passive:true});
big.addEventListener('touchend',ev=>{if(x0===null)return;const dx=ev.changedTouches[0].clientX-x0;if(Math.abs(dx)>40)step(dx<0?1:-1);x0=null});
</script></body></html>
"""


def main(argv):
    cat = load()
    bad = problems(cat)
    for p in bad:
        print("problem:", p)
    if bad:
        return 1
    if "--for" in argv or "--tour" in argv:
        if "--for" in argv:
            asked = argv[argv.index("--for") + 1:]
        else:
            rest = argv[argv.index("--tour") + 1:]
            asked = tour_stops(rest[0] if rest else TOUR)
        seen = set()
        for a in asked:
            if "--tour" in argv:                     # one line a stop
                hit, why = places_for(cat, a)
                p = hit[0] if hit else None
                print("%-18s %-16s %s" % (a, p["place"] if p else "?", (p.get("reference") or "NO REFERENCE") if p else "NOT KNOWN: " + why))
            elif a not in seen:
                print("\n".join(answer(cat, a)))
            seen.add(a)
        return 0
    if "--check" not in argv:
        with open(PAGE, "w", encoding="utf-8", newline="\n") as f:
            f.write(page(cat))
        print("wrote %s: %d references" % (os.path.relpath(PAGE, ROOT), len(cat["references"])))
    else:
        print("the catalog and the folders agree: %d references" % len(cat["references"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
