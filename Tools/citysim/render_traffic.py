"""Writes a self-contained HTML page that plays one day of residents' cars over the starting city, top down.
Only drawing lives here; every position comes from traffic.py through car_at().

    python render_traffic.py <out.html> [day]

The page has two runs of the same day: as the rules are set (World/rules/traffic.json), and a busy version
where every household has a car and drives every trip, to show queues and junctions under load.
"""
import copy, json, os, sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "worldgen"))
import world                                               # noqa: E402


def run(w, day):
    city = world.simulation(w, traffic=True)
    T = city.traffic
    city.plan(day)
    D = T._days[day]
    R = T.roads
    cars = []
    for cid in sorted(T.cars):
        trips = []
        for tr in D["trips"][cid]:
            pts, t = [], tr["t0"]
            while t <= tr["t1"] + 1e-9:
                c = T.car_at(cid, day, min(t, tr["t1"] - 1e-6) / T.pace)
                pts += [round(c["pos"][0], 1), round(c["pos"][1], 1), 1 if c["state"] == "waiting" else 0]
                t += 1.0
            trips.append(dict(t0=tr["t0"], t1=tr["t1"], bay=[round(v, 1) for v in R.bays[tr["bay1"]]["pos"]] if tr["bay1"] else None, p=pts))
        cars.append(dict(home=[round(v, 1) for v in R.bays[T.cars[cid]["bay"]]["pos"]], trips=trips))
    n = sum(len(c["trips"]) for c in cars)
    return dict(cars=cars, pace=T.pace, label="%d cars, %d trips" % (len(cars), n))


def main(out, day=16):
    w = world.load()
    busy = copy.deepcopy(w)
    busy["traffic"] = dict(busy["traffic"], car_share=1.0, drive_min_m=0.0)
    g = w["city"]["grid"]
    blocks = [[b["origin"][0], b["origin"][1], b["size"]] for b in w["blocks"]]
    lots = [[l["rect"], l["kind"]] for b in w["blocks"] for l in b["lots"]]
    roads = w["roads"]
    data = dict(size=g["columns"] * g["pitch"], blocks=blocks, lots=lots,
                junctions=[[j["pos"][0], j["pos"][1], j["control"]] for j in roads["junctions"]],
                bays=[b["pos"] for b in roads["bays"]],
                runs=[dict(run(w, day), name="As set"), dict(run(busy, day), name="Busy: every household drives")],
                day=day, rules=dict(car_share=w["traffic"]["car_share"], drive_min_m=w["traffic"]["drive_min_m"]))
    html = PAGE.replace("__DATA__", json.dumps(data, separators=(",", ":")))
    with open(out, "w", encoding="utf-8") as f:
        f.write(html)
    print("wrote %s (%.1f MB)" % (out, os.path.getsize(out) / 1e6))


PAGE = r"""<!doctype html>
<html lang="en"><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Old Town traffic</title>
<style>
:root{--paper:#ece8e2;--ink:#1e2440;--mute:#6e7082;--block:#d8cfc3;--street:#c4bdb4;--lot:#b9a48f;--bay:#a39c94;
 --move:#2f6e6a;--wait:#c8742c;--park:#8a8696;--sig:#b03a2e;--pri:#3a5a9a;--give:#7a7a7a;--panel:#f6f3ee}
@media (prefers-color-scheme:dark){:root:not([data-theme="light"]){--paper:#1b1d24;--ink:#e8e4dc;--mute:#9a98a4;--block:#2c2f38;
 --street:#23252c;--lot:#4a4038;--bay:#4a4a52;--move:#5fc0b4;--wait:#f0a050;--park:#77737f;--sig:#e2685a;--pri:#7f9fe0;--give:#9a9a9a;--panel:#24262e}}
:root[data-theme="dark"]{--paper:#1b1d24;--ink:#e8e4dc;--mute:#9a98a4;--block:#2c2f38;--street:#23252c;--lot:#4a4038;--bay:#4a4a52;
 --move:#5fc0b4;--wait:#f0a050;--park:#77737f;--sig:#e2685a;--pri:#7f9fe0;--give:#9a9a9a;--panel:#24262e}
*{box-sizing:border-box}body{margin:0;background:var(--paper);color:var(--ink);font:15px/1.45 system-ui,"Segoe UI",sans-serif}
main{max-width:1100px;margin:0 auto;padding:20px 16px 40px}h1{font-size:22px;margin:0 0 4px}p{margin:4px 0 12px;color:var(--mute)}
.wrap{display:grid;grid-template-columns:minmax(0,1fr) 260px;gap:16px}@media(max-width:820px){.wrap{grid-template-columns:1fr}}
canvas{width:100%;height:auto;aspect-ratio:1;background:var(--street);border-radius:8px;display:block}
.panel{background:var(--panel);border-radius:8px;padding:14px}.clock{font-size:34px;font-weight:600;font-variant-numeric:tabular-nums}
select{max-width:100%}button,select{font:inherit;color:var(--ink);background:var(--paper);border:1px solid var(--mute);border-radius:6px;padding:4px 10px;cursor:pointer}
input[type=range]{width:100%}.row{display:flex;gap:8px;align-items:center;flex-wrap:wrap;margin:8px 0}
.key{display:grid;grid-template-columns:16px 1fr;gap:4px 8px;align-items:center;font-size:13px;margin-top:10px}
.sw{width:12px;height:12px;border-radius:3px}.n{font-variant-numeric:tabular-nums}
</style></head><body><main>
<h1>Old Town traffic</h1>
<p>One day of residents' cars, calculated by <code>Tools/citysim/traffic.py</code>. Each car's position is a lookup from that day's plan.</p>
<div class="wrap"><canvas id="c" width="1600" height="1600"></canvas>
<div class="panel"><div class="clock" id="clock">00:00</div>
<div class="row"><button id="play">Pause</button><select id="speed"><option value="30">30× real time</option><option value="120" selected>120×</option><option value="600">600×</option></select></div>
<input type="range" id="t" min="0" max="1440" step="0.1" value="420">
<div class="row"><select id="run"></select></div>
<div class="n" id="count"></div>
<div class="key">
<span class="sw" style="background:var(--move)"></span><span>Driving</span>
<span class="sw" style="background:var(--wait)"></span><span>Waiting (junction or queue)</span>
<span class="sw" style="background:var(--park)"></span><span>Parked</span>
<span class="sw" style="background:var(--sig)"></span><span>Signals</span>
<span class="sw" style="background:var(--pri)"></span><span>Priority for the through street</span>
<span class="sw" style="background:var(--give)"></span><span>Give way, first come</span>
</div>
<p id="note" style="margin-top:12px;font-size:13px"></p>
<p style="font-size:13px">Drag on the map to look closer; double-click to reset.</p></div></div>
</main>
<script>
const D=__DATA__;const cv=document.getElementById('c'),ctx=cv.getContext('2d');
const css=n=>getComputedStyle(document.documentElement).getPropertyValue(n).trim();
let run=0,minute=420,playing=true,zoom={x:0,z:0,s:1};
const sel=document.getElementById('run');D.runs.forEach((r,i)=>{const o=document.createElement('option');o.value=i;o.textContent=r.name+' ('+r.label+')';sel.appendChild(o)});
sel.onchange=()=>{run=+sel.value};
document.getElementById('note').textContent='Rules as set: '+Math.round(D.rules.car_share*100)+'% of households have a car; tours with a trip over '+D.rules.drive_min_m+' m are driven. Day '+D.day+'.';
const M=()=>cv.width/(D.size/zoom.s);const P=(x,z)=>[(x-zoom.x)*M(),(z-zoom.z)*M()];
function carAt(c,t){let bay=c.home;for(const tr of c.trips){if(t<tr.t0)break;if(t<=tr.t1){const i=Math.min(Math.floor(t-tr.t0),tr.p.length/3-2),f=(t-tr.t0)-i,
 a=i*3,b=Math.min(a+3,tr.p.length-3);const x=tr.p[a]+(tr.p[b]-tr.p[a])*f,z=tr.p[a+1]+(tr.p[b+1]-tr.p[a+1])*f;
 let hx=tr.p[b]-tr.p[a],hz=tr.p[b+1]-tr.p[a+1];if(Math.hypot(hx,hz)<1e-3&&a>=3){hx=tr.p[a]-tr.p[a-3];hz=tr.p[a+1]-tr.p[a-2]}
 return{x,z,hx,hz,s:tr.p[a+2]?'wait':'move'}}bay=tr.bay}return bay?{x:bay[0],z:bay[1],hx:1,hz:0,s:'park'}:null}
function draw(){const W=cv.width,m=M();ctx.fillStyle=css('--street');ctx.fillRect(0,0,W,W);
 ctx.fillStyle=css('--block');for(const[x,z,s]of D.blocks){const[a,b]=P(x,z);ctx.fillRect(a,b,s*m,s*m)}
 ctx.fillStyle=css('--lot');for(const[r,k]of D.lots){if(k==='garden'||k==='market'||k==='stop')continue;const[a,b]=P(r[0],r[1]);ctx.fillRect(a,b,(r[2]-r[0])*m,(r[3]-r[1])*m)}
 if(zoom.s>2.5){ctx.fillStyle=css('--bay');for(const[x,z]of D.bays){const[a,b]=P(x,z);ctx.fillRect(a-1.2*m,b-1.2*m,2.4*m,2.4*m)}}
 for(const[x,z,k]of D.junctions){if(k==='none')continue;const[a,b]=P(x,z);ctx.fillStyle=css(k==='signals'?'--sig':k==='priority'?'--pri':'--give');
  ctx.beginPath();ctx.arc(a,b,Math.max(3,2.2*m),0,7);ctx.fill()}
 const t=minute*D.runs[run].pace;let mv=0,wt=0,pk=0;const L=Math.max(4.2*m,18),Wd=Math.max(1.9*m,9);
 let aw=0;for(const c of D.runs[run].cars){const p=carAt(c,t);if(!p){aw++;continue}if(p.s==='move')mv++;else if(p.s==='wait')wt++;else pk++;
  const[a,b]=P(p.x,p.z);const ang=Math.atan2(p.hz,p.hx);ctx.save();ctx.translate(a,b);ctx.rotate(ang);
  ctx.fillStyle=css(p.s==='move'?'--move':p.s==='wait'?'--wait':'--park');ctx.fillRect(-L/2,-Wd/2,L,Wd);ctx.restore()}
 const h=Math.floor(minute/60)%24,mm=Math.floor(minute%60);document.getElementById('clock').textContent=String(h).padStart(2,'0')+':'+String(mm).padStart(2,'0');
 document.getElementById('count').textContent=mv+' driving · '+wt+' waiting · '+pk+' parked · '+aw+' out of town';document.getElementById('t').value=minute}
let last=performance.now();function tick(now){const dt=(now-last)/1000;last=now;if(playing){minute+=dt*(+document.getElementById('speed').value)/D.runs[run].pace;if(minute>=1440)minute=0}draw();requestAnimationFrame(tick)}
document.getElementById('play').onclick=e=>{playing=!playing;e.target.textContent=playing?'Pause':'Play'};
document.getElementById('t').oninput=e=>{minute=+e.target.value};
let drag=null;cv.onmousedown=e=>{const r=cv.getBoundingClientRect();drag=[(e.clientX-r.left)/r.width,(e.clientY-r.top)/r.height]};
cv.onmouseup=e=>{if(!drag)return;const r=cv.getBoundingClientRect(),b=[(e.clientX-r.left)/r.width,(e.clientY-r.top)/r.height];
 const w=Math.max(Math.abs(b[0]-drag[0]),Math.abs(b[1]-drag[1]));if(w>0.02){const span=D.size/zoom.s;zoom={x:zoom.x+Math.min(drag[0],b[0])*span,z:zoom.z+Math.min(drag[1],b[1])*span,s:zoom.s/w}}drag=null};
cv.ondblclick=()=>{zoom={x:0,z:0,s:1}};
(function(){let best=[420,0];for(let m=300;m<1400;m+=0.5){const t=m*D.runs[0].pace;let n=0;for(const c of D.runs[0].cars)for(const tr of c.trips)if(t>=tr.t0&&t<=tr.t1)n++;if(n>best[1])best=[m,n]}minute=Math.max(0,best[0]-3)})();
draw();requestAnimationFrame(tick);
</script></body></html>"""

if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "traffic_day.html", int(sys.argv[2]) if len(sys.argv) > 2 else 16)
