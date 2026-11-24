// The body of M_Greybox's Custom node (Tools/unreal/make_greybox_material.py pastes it in): the slice's stone, brick,
// paving and painted plaster (Prototype/play/play.js: NOISE, paint(), pat(), material 30+), ported line for line from
// GLSL so a wall in Unreal reads as it does in the slice (ART-001 W1, W2; ADR-0017).
// Inputs: Pos, Cam (world, centimetres), N (world normal), Base (the surface's colour), P0 = (type, unit w, unit h,
// joint) and P1 = (vary, bevel, dome, mortar) of the surface's pattern; type < 0 means painted plaster (no pattern).
// Units inside are metres, axes Unreal's (x, y across the ground, z up); the slice's y-up is swapped back below.
struct BrixWall
{
	float wrapm(float x, float m) { return x - m * floor(x / m); }
	float3 wrap3(float3 p) { return p - 256.0 * floor(p / 256.0); }   // keeps the hash's numbers small far from the origin
	float h3(float3 p) { p = wrap3(p); return frac(sin(dot(p, float3(12.9898, 78.233, 37.719))) * 43758.5453); }
	float ns(float3 p)
	{
		float3 i = floor(p), f = frac(p); f = f * f * (3.0 - 2.0 * f);
		return lerp(lerp(lerp(h3(i), h3(i + float3(1, 0, 0)), f.x), lerp(h3(i + float3(0, 1, 0)), h3(i + float3(1, 1, 0)), f.x), f.y),
		            lerp(lerp(h3(i + float3(0, 0, 1)), h3(i + float3(1, 0, 1)), f.x), lerp(h3(i + float3(0, 1, 1)), h3(i + float3(1, 1, 1)), f.x), f.y), f.z);
	}
	// the painted() material: dark tone, a mid tone and a warm highlight blotched together (W1)
	float3 paint(float3 c2, float3 r, float v)
	{
		float3 c1 = c2 * r, mid = (c1 + c2) * float3(0.48, 0.465, 0.53), hi = c2 * float3(1.12, 1.04, 0.92);
		return v < 0.45 ? lerp(c1, mid, v / 0.45) : (v < 0.86 ? lerp(mid, hi, (v - 0.45) / 0.41) : lerp(hi, c2, (v - 0.86) / 0.14));
	}
	float jnt(float em, float jw) { return 1.0 - smoothstep(jw * 0.45, jw, em); }
	// One stone, brick or paving pattern: (joint 0..1, the stone's own number 0..1, relief -1..1, bed shadow) at q (metres
	// along the surface). Types: 0 bond, 1 Flemish bond, 2 coursed rubble, 3 flagstones, 4 herringbone, 5 basket weave.
	float4 pat(float2 q, float4 A, float4 B)
	{
		float t = A.x, w = A.y, h = A.z, jw = A.w, id = 0.0;
		float4 e = float4(1, 1, 1, 1);
		if (t < 0.5)
		{
			float row = floor(q.y / h), u = q.x / w + 0.5 * wrapm(row, 2.0);
			float2 fr = float2(frac(u), frac(q.y / h));
			e = float4(fr.x * w, (1.0 - fr.x) * w, fr.y * h, (1.0 - fr.y) * h); id = h3(float3(floor(u), row, 21.0));
		}
		else if (t < 1.5)
		{
			float row = floor(q.y / h), P = w * 1.5, u = (q.x + wrapm(row, 2.0) * P * 0.5) / P, fa = frac(u) * P;
			bool hd = fa > w; float a = hd ? fa - w : fa, L = hd ? w * 0.5 : w, fy = frac(q.y / h) * h;
			e = float4(a, L - a, fy, h - fy); id = h3(float3(floor(u), row, hd ? 23.0 : 22.0)) * (hd ? 0.55 : 1.0);
		}
		else if (t < 3.5)
		{
			bool rub = t < 2.5; float row = floor(q.y / h), fy = frac(q.y / h) * h;
			float cw = w * 0.5, u = (q.x + h3(float3(row, 4.0, 25.0)) * w) / cw, i = floor(u), fa = frac(u) * cw, l = fa, r = cw - fa, k0 = i;
			for (int s = 0; s < 2; s++) { if (h3(float3(k0 - 1.0, row, 26.0)) < 0.5) break; k0 -= 1.0; l += cw; }
			float k1 = i; for (int s2 = 0; s2 < 2; s2++) { if (h3(float3(k1, row, 26.0)) < 0.5) break; k1 += 1.0; r += cw; }
			e = float4(l, r, fy, h - fy); id = h3(float3(k0, row, 27.0));
			if (rub) e -= float4(0.01 + 0.035 * h3(float3(id, 1.0, 31.0)), 0.01 + 0.035 * h3(float3(id, 2.0, 31.0)), 0.008 + 0.03 * h3(float3(id, 3.0, 31.0)), 0.008 + 0.03 * h3(float3(id, 4.0, 31.0)));
		}
		else if (t < 4.5)
		{
			float2 p = q / h; float s0 = floor((p.x - p.y) / 4.0);
			for (int ds = -1; ds <= 1; ds++)
			{
				float2 pp = p - (s0 + float(ds)) * float2(2.0, -2.0); float m = floor(pp.y), a = pp.x - m;
				if (a >= 0.0 && a < 2.0) { float b = pp.y - m; e = float4(a, 2.0 - a, b, 1.0 - b) * h; id = h3(float3(m, s0 + float(ds), 28.0)); break; }
				m = floor(pp.x); a = pp.y - m;
				if (a >= 1.0 && a < 3.0) { float b = pp.x - m; e = float4(b, 1.0 - b, a - 1.0, 3.0 - a) * h; id = h3(float3(m, s0 + float(ds), 29.0)); break; }
			}
		}
		else
		{
			float2 cell = floor(q / w), fr = frac(q / w); bool turn = wrapm(cell.x + cell.y, 2.0) > 0.5;
			float a = turn ? fr.x : fr.y, b = turn ? fr.y : fr.x, hb = floor(a * 2.0), fa = frac(a * 2.0);
			e = turn ? float4(fa * w * 0.5, (1.0 - fa) * w * 0.5, b * w, (1.0 - b) * w) : float4(b * w, (1.0 - b) * w, fa * w * 0.5, (1.0 - fa) * w * 0.5);
			id = h3(float3(cell, hb + 30.0)) * (turn ? 1.0 : 0.85);
		}
		float em = min(min(e.x, e.y), min(e.z, e.w)), j = jnt(em, jw);
		// relief (W2): each stone's upper edge catches the light, its lower edge sits in shadow, the sides a little of each
		float bw = B.y > 0.0 ? B.y : clamp(min(w, h) * 0.22, 0.012, 0.07), lip = 1.0 + (B.y > 0.0 ? 1.2 : 0.0);
		float bev = lip * (0.13 * (1.0 - smoothstep(jw * 0.5, jw + bw, e.w)) - 0.2 * (1.0 - smoothstep(jw * 0.5, jw + bw, e.z))
		          + 0.05 * (1.0 - smoothstep(jw * 0.5, jw + bw, e.y)) - 0.08 * (1.0 - smoothstep(jw * 0.5, jw + bw, e.x)));
		float4 ep = max(e, float4(0.001, 0.001, 0.001, 0.001));
		float2 cc = float2((ep.x - ep.y) / (ep.x + ep.y), (ep.z - ep.w) / (ep.z + ep.w));
		if (t > 1.5 && t < 2.5) { float rr = min(h * 0.45, 0.09); bev += 0.06 * cc.y - 0.24 * (1.0 - smoothstep(0.0, rr, em)) * (1.0 - smoothstep(0.0, rr, em)); }
		if (B.z > 0.0) bev -= B.z * 2.0 * (pow(abs(cc.x), 4.0) + pow(abs(cc.y), 4.0));
		float sh = e.z <= min(min(e.x, e.y), e.w) ? 1.0 : (e.w <= min(e.x, e.y) ? 0.0 : 0.45);
		return float4(j, id, bev, sh);
	}
	float3 Run(float3 PosCm, float3 N, float3 Base, float4 A, float4 B, float3 CamCm)
	{
		float3 P = PosCm * 0.01, Pn = float3(P.x, P.z, P.y);          // Pn: the slice's frame (y up) for its noise
		float d = length(CamCm * 0.01 - P), nearK = 1.0 - min(1.0, d / 70.0);
		float blot = clamp((ns(Pn * 0.9) * 0.6 + ns(Pn * 2.7 + 7.0) * 0.4 - 0.5) * 2.2 + 0.5, 0.0, 1.0);
		if (A.x < -0.5) return paint(Base, float3(0.69, 0.69, 0.73), blot) * (0.92 + 0.16 * ns(Pn * 0.13));   // painted plaster
		float wall = 1.0 - abs(N.z);
		float2 q = wall > 0.5 ? float2(abs(N.x) > 0.5 ? P.y : P.x, P.z) : P.xy;
		float4 r = pat(q, A, B);
		float fade = 1.0 - min(1.0, d / (A.y < 0.3 ? 45.0 : 110.0));                // small bricks fade to their colour sooner
		float3 c = Base, c1 = c * float3(0.52, 0.54, 0.70);
		float3 tint = lerp(float3(1.06, 1.0, 0.92), float3(0.92, 0.97, 1.05), frac(r.y * 53.7));
		c = lerp(paint(c, float3(0.52, 0.54, 0.70), blot), (c1 + c) * 0.5, r.y * B.x) * tint * (0.8 + 0.36 * frac(r.y * 97.13))
		  * (1.0 + r.z * nearK) * (1.0 - 0.18 * nearK * ns(Pn * 9.0) * ns(Pn * 23.0 + 3.0));
		float3 mortar = lerp(c1 * float3(0.62, 0.62, 0.68), float3(0.55, 0.53, 0.49) * (0.9 + 0.2 * ns(Pn * 17.0)), B.w) * (1.0 - 0.55 * r.w * B.w);
		return lerp(c, mortar, r.x * fade);
	}
};
BrixWall W;
return W.Run(Pos, N, Base, P0, P1, Cam);
