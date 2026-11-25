// The body of M_Clouds' node (Tools/unreal/make_clouds_material.py pastes it in): the clouds of ADR-0014 (WLD-035,
// WLD-036), soft heaps on one flat layer overhead, bright on the side towards the sun and shaded underneath, lost in the
// haze towards the horizon. Inputs: Dir (the view direction, Unreal's frame: z up), Cl = (cover, edge softness, size,
// shape phase), Off (where the wind has carried them), Sun (towards the sun), SunCol (its light), Zen, Hor (the sky's
// colour overhead and at the horizon), LitShade = (their sunny side's share of the sun's light, their shade's of the
// sky's). Returns the cloud's colour and how much of the sky it hides. Everything that places them comes from the clock
// (ABrixWeather: BrixCloudsNow, checked against world.clouds_now()).
struct BrixCloud
{
	float3 wrap3(float3 p) { return p - 256.0 * floor(p / 256.0); }
	float h3(float3 p) { p = wrap3(p); return frac(sin(dot(p, float3(12.9898, 78.233, 37.719))) * 43758.5453); }
	float ns(float3 p)
	{
		float3 i = floor(p), f = frac(p); f = f * f * (3.0 - 2.0 * f);
		return lerp(lerp(lerp(h3(i), h3(i + float3(1, 0, 0)), f.x), lerp(h3(i + float3(0, 1, 0)), h3(i + float3(1, 1, 0)), f.x), f.y),
		            lerp(lerp(h3(i + float3(0, 0, 1)), h3(i + float3(1, 0, 1)), f.x), lerp(h3(i + float3(0, 1, 1)), h3(i + float3(1, 1, 1)), f.x), f.y), f.z);
	}
	float low(float3 q) { return (ns(q) * 0.5 + ns(q * 2.03 + 3.1) * 0.25) / 0.75; }
};
BrixCloud B;
float3 d = normalize(Dir);
if (d.z <= 0.015 || Cl.x <= 0.0) return float4(0, 0, 0, 0);
float2 q = d.xy / (d.z + 0.12) * Cl.z + Off;
float2 sd = normalize(Sun.xy + float2(0.0001, 0.0001)) * 0.3;
float3 q3 = float3(q.x, Cl.w, q.y), qs = float3(q.x + sd.x, Cl.w, q.y + sd.y);
float lo = B.low(q3);
float n = lo * 0.8 + B.ns(q3 * 4.01 + 7.3) * 0.133 + B.ns(q3 * 8.1 + 1.7) * 0.067;
float th = 0.72 - 0.44 * Cl.x;   // the noise crowds round its middle value, so this spread makes cover about the share of sky hidden
float a = smoothstep(th - Cl.y, th + Cl.y, n) * smoothstep(0.015, 0.2, d.z);
float lit = saturate(0.55 + (lo - B.low(qs)) * 3.0) * (1.0 - 0.4 * smoothstep(th, th + 0.3, n));   // thinner towards the sun: that edge is bright
float3 c = lerp((Zen * 0.45 + Hor * 0.55) * LitShade.y + SunCol * LitShade.x * 0.2, SunCol * LitShade.x + Hor * 0.6, lit);
return float4(c, a);
