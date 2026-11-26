"""The catalog of Brix Town's look references (ADR-0061): checks Art/Reference/catalog.json against the folders and writes
the page to review them, Art/Reference/index.html (a tab per category, each reference with its pictures and details).

    python Tools/reference_catalog.py            check, then write the page (exits 1 and writes nothing on a problem)
    python Tools/reference_catalog.py --check    check only

The rules it keeps (an AI agent should get from a thing to its reference in one step):
  - every reference file is in Art/Reference/<Category>/ and named <Category><Subject>[_<Variant>].<ext>, PascalCase
    (Indoor/IndoorRooms_Kitchen.png, Vehicles/VehiclesSoftBox.png);
  - every file there is in the catalog, and every file the catalog names is there;
  - every reference says what it locks, when and by what decision it was approved, and what makes it;
  - retired references are in Art/Retired/, never in Art/Reference/.
"""
import html, json, os, re, sys

ROOT = os.path.normpath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
REF = os.path.join(ROOT, "Art", "Reference")
CATALOG = os.path.join(REF, "catalog.json")
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
        if not t["file"].startswith("Art/Retired/"):
            out.append("%s: a retired reference belongs in Art/Retired/" % t["file"])
        elif not os.path.isfile(os.path.join(root, t["file"])):
            out.append("%s: the retired file is missing" % t["file"])
    return out


def e(s):
    return html.escape(str(s), quote=True)


def code_or_text(s):
    """Paths and file names in backticks-free text shown as code."""
    return re.sub(r"((?:Tools|Game|World|Prototype|docs|Art)/[\w./()<>*\-]+|ADR-\d{4})", r"<code>\1</code>", e(s))


HOURS = {"Morning": "Morning", "Midday": "Midday", "Dusk": "Dusk", "NightLamps": "Night, lamps on",
         "NightFullMoon": "Night, lamps off, full moon", "NightNewMoon": "Night, lamps off, new moon", "Cloudy": "Cloudy",
         "Rain": "Rain", "Fog": "Fog", "Snow": "Snow"}       # the hours of Tools/blender/hours_50.py


def caption(f):
    """A picture's name, and its hour when it is one of a place's hours."""
    stem = os.path.splitext(os.path.basename(f))[0]
    hour = HOURS.get(stem.rsplit("_", 1)[-1]) if "_" in stem else None
    return ("<b>%s</b> " % e(hour) if hour else "") + "<code>%s</code>" % e(os.path.basename(f))


def media(f, alt):
    if f.endswith(".mp4"):
        return '<video src="%s" controls muted loop preload="metadata" title="%s"></video>' % (e(f), e(alt))
    return '<img src="%s" alt="%s" loading="lazy" data-full="%s">' % (e(f), e(alt), e(f))


def page(cat):
    cats = cat["categories"]
    tabs, panels = [], []
    for i, c in enumerate(cats + [{"id": "Retired", "title": "Retired", "about": "References taken out of use, kept for the record in Art/Retired/. Never build against these."},
                                  {"id": "Naming", "title": "How it is kept", "about": ""}]):
        refs = [r for r in cat["references"] if r["category"] == c["id"]]
        gaps = [g for g in cat.get("gaps", []) if g["category"] == c["id"]]
        n = len(refs) if c["id"] not in ("Retired", "Naming") else (len(cat.get("retired", [])) if c["id"] == "Retired" else "")
        tabs.append('<button role="tab" id="tab-%s" aria-controls="p-%s" aria-selected="%s" data-tab="%s">%s<span class="n">%s</span></button>'
                    % (c["id"], c["id"], "true" if i == 0 else "false", c["id"], e(c["title"]), n))
        body = ['<p class="about">%s</p>' % e(c["about"])] if c["about"] else []
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
            body.append('<article class="ref%s" id="%s"><header><h2>%s</h2><code class="rid">%s</code>%s%s</header><div class="pics">%s</div>'
                        '<dl>%s</dl>%s</article>' % (" proposed" if r.get("status") == "proposed" else "", e(r["id"]), e(r["title"]), e(r["id"]),
                        '<span class="prop">Proposed, for your review</span>' if r.get("status") == "proposed" else "",
                        '<span class="flag">%d issue%s seen</span>' % (len(r["issues"]), "" if len(r["issues"]) == 1 else "s") if r.get("issues") else "",
                        pics, "".join("<dt>%s</dt><dd>%s</dd>" % (k, v) for k, v in rows),
                        '<div class="issues"><h3>Seen wrong, for the owner to judge</h3><ul>%s</ul></div>' % issues if issues else ""))
        if gaps:
            body.append('<section class="gaps"><h3>No reference yet</h3><ul>%s</ul></section>' % "".join(
                "<li><b>%s</b> %s</li>" % (e(g["what"]), code_or_text(g["why"])) for g in gaps))
        if c["id"] == "Retired":
            for t in cat.get("retired", []):
                body.append('<article class="ref retired"><header><h2>%s</h2></header><div class="pics"><figure>%s</figure></div><dl><dt>Was</dt><dd>%s</dd><dt>Why retired</dt><dd>%s</dd></dl></article>'
                            % (e(os.path.basename(t["file"])), media("../Retired/" + os.path.basename(t["file"]), t["was"]), code_or_text(t["was"]), code_or_text(t["why"])))
        if c["id"] == "Naming":
            body.append('<article class="ref doc"><h2>How the references are kept</h2><ul>'
                        "<li>Every approved look reference is in <code>Art/Reference/&lt;Category&gt;/</code>, named <code>&lt;Category&gt;&lt;Subject&gt;[_&lt;Variant&gt;].&lt;ext&gt;</code> in PascalCase: <code>Indoor/IndoorRooms_Kitchen.png</code>, <code>Vehicles/VehiclesSoftBox.png</code>. A reference's id is the name without the variant.</li>"
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
figcaption b{color:var(--ink);font-weight:600;margin-right:4px}
#big{position:fixed;inset:0;background:rgba(0,0,0,.88);display:none;align-items:center;justify-content:center;z-index:5;cursor:zoom-out}
#big img{max-width:96vw;max-height:94vh}#big.on{display:flex}
@media (max-width:640px){dl{grid-template-columns:1fr}dt{margin-top:6px}}
</style></head><body>
<header class="top"><h1>Brix Town references</h1><p>The look every made thing is checked against. {COUNT}. Written from <code>Art/Reference/catalog.json</code>.</p></header>
<nav><div role="tablist">{TABS}</div></nav>
<main>{PANELS}</main>
<div id="big"><img alt=""></div>
<script>
const tabs=[...document.querySelectorAll('[role=tab]')];
function show(id){tabs.forEach(t=>{const on=t.dataset.tab===id;t.setAttribute('aria-selected',on);document.getElementById('p-'+t.dataset.tab).hidden=!on});try{history.replaceState(null,'','#'+id)}catch(e){}}
tabs.forEach(t=>t.addEventListener('click',()=>show(t.dataset.tab)));
const want=(location.hash||'').slice(1);if(want){const t=tabs.find(t=>t.dataset.tab===want);if(t)show(want);else{const a=document.getElementById(want);if(a){show(a.closest('[role=tabpanel]').id.slice(2));a.scrollIntoView()}}}
const big=document.getElementById('big');document.querySelectorAll('img[data-full]').forEach(i=>i.addEventListener('click',()=>{big.firstChild.src=i.dataset.full;big.classList.add('on')}));
big.addEventListener('click',()=>big.classList.remove('on'));document.addEventListener('keydown',ev=>{if(ev.key==='Escape')big.classList.remove('on')});
</script></body></html>
"""


def main(argv):
    cat = load()
    bad = problems(cat)
    for p in bad:
        print("problem:", p)
    if bad:
        return 1
    if "--check" not in argv:
        with open(PAGE, "w", encoding="utf-8", newline="\n") as f:
            f.write(page(cat))
        print("wrote %s: %d references" % (os.path.relpath(PAGE, ROOT), len(cat["references"])))
    else:
        print("the catalog and the folders agree: %d references" % len(cat["references"]))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
