// The body of M_Lawn's Custom node (Tools/unreal/make_lawn_material.py pastes it in): a lawn as the slice draws it,
// material 24 (Prototype/play/play.js, ART-001, World/rules/vegetation.json "lawn"): muted patches of three tones, the
// mower's faint stripes, clumps of clover, blades close up, and daisies gathered in drifts; all of it painted (W1).
// Inputs: Pos, Cam (world, centimetres), Base (the face's colour: the lawn's grass or its darker edge band),
// T0, T1, T2 (vegetation.json lawn.tones), L = (patch_m, stripe_m, stripe, clover), Daisy = (daisy_color, daisies).
// Units inside are metres; the slice's vP (x, y up, z) is Unreal's (x, z, y).
struct BrixLawn
{
	float3 wrap3(float3 p) { return p - 256.0 * floor(p / 256.0); }   // keeps the hash's numbers small far from the origin
	float h3(float3 p) { p = wrap3(p); return frac(sin(dot(p, float3(12.9898, 78.233, 37.719))) * 43758.5453); }
	float ns(float3 p)
	{
		float3 i = floor(p), f = frac(p); f = f * f * (3.0 - 2.0 * f);
		return lerp(lerp(lerp(h3(i), h3(i + float3(1, 0, 0)), f.x), lerp(h3(i + float3(0, 1, 0)), h3(i + float3(1, 1, 0)), f.x), f.y),
		            lerp(lerp(h3(i + float3(0, 0, 1)), h3(i + float3(1, 0, 1)), f.x), lerp(h3(i + float3(0, 1, 1)), h3(i + float3(1, 1, 1)), f.x), f.y), f.z);
	}
	float3 paint(float3 c2, float3 r, float v)
	{
		float3 c1 = c2 * r, mid = (c1 + c2) * float3(0.48, 0.465, 0.53), hi = c2 * float3(1.12, 1.04, 0.92);
		return v < 0.45 ? lerp(c1, mid, v / 0.45) : (v < 0.86 ? lerp(mid, hi, (v - 0.45) / 0.41) : lerp(hi, c2, (v - 0.86) / 0.14));
	}
	float3 Run(float3 Pos, float3 Cam, float3 c, float3 T0, float3 T1, float3 T2, float4 L, float4 Daisy)
	{
		float3 vP = float3(Pos.x, Pos.z, Pos.y) / 100.0;
		float d = length(Pos - Cam) / 100.0;
		float blot = saturate((ns(vP * 0.9) * 0.6 + ns(vP * 2.7 + 7.0) * 0.4 - 0.5) * 2.2 + 0.5);
		float k2 = 1.0 - min(1.0, d / 35.0), k3 = 1.0 - min(1.0, d / 140.0);
		float big = ns(float3(vP.xz / L.x, 3.0)) * 0.65 + ns(float3(vP.xz / (L.x * 0.37), 9.0)) * 0.35;
		float3 g = lerp(lerp(T0, T1, smoothstep(0.3, 0.46, big)), T2, smoothstep(0.56, 0.72, big));
		g *= c / T1;                                                     // an edge band or a darker lawn keeps its own brightness
		float sx = frac(vP.x / L.y);
		g *= 1.0 + L.z * (smoothstep(0.42, 0.5, sx) - smoothstep(0.92, 1.0, sx) - 0.5) * 2.0 * k3;   // the mower's passes, light and dark
		float cl = smoothstep(0.6, 0.72, ns(float3(vP.xz * 0.9, 5.0)) * 0.7 + ns(float3(vP.xz * 3.1, 6.0)) * 0.3);
		g = lerp(g, g * float3(0.78, 0.92, 0.76), cl * L.w * k3);       // clumps of clover, darker and cooler
		g *= 0.9 + 0.2 * lerp(0.5, ns(float3(vP.xz * 11.0, 7.0)) * 0.6 + ns(float3(vP.xz * 29.0, 8.0)) * 0.4, k2);   // blades, close up
		float3 o = paint(g, float3(0.74, 0.78, 0.72), blot);
		float2 dq = vP.xz / 0.32, dc = floor(dq);
		float dk = h3(float3(dc, 41.0)), dm = smoothstep(0.45, 0.75, ns(float3(vP.xz * 0.35, 12.0)));   // daisies, gathered in drifts
		if (dk < Daisy.w * 3.0 * dm && k2 > 0.0)
		{
			float2 off = frac(dq) - 0.5 - (float2(h3(float3(dc, 42.0)), h3(float3(dc, 43.0))) - 0.5) * 0.5;
			float r = length(off);
			o = lerp(o, lerp(Daisy.rgb, float3(0.62, 0.5, 0.2), 1.0 - smoothstep(0.03, 0.06, r)), (1.0 - smoothstep(0.1, 0.15, r)) * k2);
		}
		return o;
	}
};
BrixLawn B;
return B.Run(Pos, Cam, Base, T0, T1, T2, L, Daisy);
