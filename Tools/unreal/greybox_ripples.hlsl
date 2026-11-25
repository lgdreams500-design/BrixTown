// The body of M_Greybox's ripple node (Tools/unreal/greybox_wet.py pastes it in): rings spreading where raindrops land on
// wet ground (WLD-037, ADR-0016). Inputs: Pos (world, centimetres), T (seconds), K (0..1, how hard it rains). Returns
// (x, y) the way the rings tilt the surface and z how much ring is here. Drops land in cells of 0.3 m, each at its own
// moment and its own place in its cell; a ring grows to about 0.3 m across and fades as it goes.
float2 p = Pos.xy / 30.0;
float2 id = floor(p);
float3 acc = float3(0, 0, 0);
if (K <= 0.0) return acc;
for (int dx = -1; dx <= 1; dx++)
{
	for (int dy = -1; dy <= 1; dy++)
	{
		float2 c = id + float2(dx, dy);
		float2 w = c - 256.0 * floor(c / 256.0);
		float h = frac(sin(dot(w, float2(12.9898, 78.233))) * 43758.5453);
		if (h > K * 0.75) continue;
		float2 jit = float2(frac(sin(dot(w, float2(39.346, 11.135))) * 43758.5453), frac(sin(dot(w, float2(73.156, 52.235))) * 43758.5453)) - 0.5;
		float ph = frac(T * (0.9 + h) + h * 9.0);
		float2 o = p - (c + 0.5 + jit * 0.5);
		float d = length(o);
		float r = ph * 0.55;
		float ring = saturate(1.0 - abs(d - r) / 0.09) * (1.0 - ph) * (1.0 - ph);
		acc.z += ring;
		acc.xy += (d > 0.0001 ? o / d : float2(0, 0)) * ring * (d > r ? 1.0 : -1.0);
	}
}
return acc;
