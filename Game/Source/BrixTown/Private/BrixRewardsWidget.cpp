#include "BrixRewardsWidget.h"

#include "Engine/Texture2D.h"
#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"
#include "Styling/SlateBrush.h"

namespace
{
	// an ellipse as a fan of triangles, its middle and its rim in their own colours
	void Fan(TArray<FSlateVertex>& V, TArray<SlateIndex>& I, const FSlateRenderTransform& T, FVector2f C, float Rx, float Ry, FColor Mid, FColor Rim, int32 Seg = 24)
	{
		const SlateIndex Base = (SlateIndex)V.Num();
		V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, C, FVector2f(.5f, .5f), Mid));
		for (int32 k = 0; k < Seg; ++k)
		{
			const float A = k * UE_TWO_PI / Seg;
			V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, C + FVector2f(FMath::Cos(A) * Rx, FMath::Sin(A) * Ry), FVector2f(.5f, .5f), Rim));
		}
		for (int32 k = 0; k < Seg; ++k) I.Append({ Base, (SlateIndex)(Base + 1 + k), (SlateIndex)(Base + 1 + (k + 1) % Seg) });
	}
	// as many sides as keep it round at its size on the screen
	int32 Sides(float R) { return FMath::Clamp(FMath::CeilToInt(R * .9f), 24, 128); }
	FColor A(FColor C, float Alpha) { C.A = (uint8)FMath::Clamp(Alpha * C.A, 0.f, 255.f); return C; }
	// light added to the picture (rewards.js "lighter"): drawn premultiplied with no alpha, so it only adds
	FColor Add(FColor C, float K) { K = FMath::Clamp(K, 0.f, 1.f); return FColor((uint8)(C.R * K), (uint8)(C.G * K), (uint8)(C.B * K), 0); }
	// a four-pointed twinkle (rewards.js glint())
	void Glint(TArray<FSlateVertex>& V, TArray<SlateIndex>& I, const FSlateRenderTransform& T, FVector2f P, float R, float K)
	{
		const FColor W = Add(FColor(255, 240, 200), .85f * K);
		Fan(V, I, T, P, R * .22f, R, W, FColor(0, 0, 0, 0), 12);
		Fan(V, I, T, P, R, R * .22f, W, FColor(0, 0, 0, 0), 12);
		Fan(V, I, T, P, R * .35f, R * .35f, Add(FColor(255, 250, 230), K), FColor(0, 0, 0, 0), 12);
	}

	// The coin's face, painted as rewards.js texture() paints it on a canvas, at twice its size so it stays sharp up close:
	// brass in two close tones blotched together on a light-to-dark diagonal, a raised rim and the groove inside it, a lit
	// arc, a ring of beads, a stamped B (shade below, light above). Straight alpha, its edge smoothed over a pixel.
	struct FPainter
	{
		static constexpr int32 S = 256;
		TArray<FLinearColor> Px;
		FPainter() { Px.Init(FLinearColor(0, 0, 0, 0), S * S); }
		void Blend(int32 x, int32 y, FLinearColor C, float Cov)
		{
			const float a = C.A * Cov;
			if (a <= 0) return;
			FLinearColor& D = Px[y * S + x];
			D.R = FMath::Lerp(D.R, C.R, a); D.G = FMath::Lerp(D.G, C.G, a); D.B = FMath::Lerp(D.B, C.B, a);
		}
		static float Cover(float Dist) { return FMath::Clamp(.5f - Dist, 0.f, 1.f); }   // a signed distance in pixels to coverage
		template <typename F> void Shape(FLinearColor C, F Sdf) { for (int32 y = 0; y < S; ++y) for (int32 x = 0; x < S; ++x) Blend(x, y, C, Cover(Sdf(x + .5f, y + .5f))); }
	};
	FLinearColor Rgb(int32 r, int32 g, int32 b, float a = 1.f) { return FLinearColor(r / 255.f, g / 255.f, b / 255.f, a); }
	float Ring(float x, float y, float cx, float cy, float R, float W) { return FMath::Abs(FMath::Sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) - R) - W / 2; }
	float Disc(float x, float y, float cx, float cy, float R) { return FMath::Sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy)) - R; }
	// the letter B, height H, its middle at (cx, cy): a stem and two bowls
	float LetterB(float x, float y, float cx, float cy, float H)
	{
		const float u = (x - cx) / H, v = (y - cy) / H, Wd = .13f;
		const float Stem = FMath::Max(FMath::Abs(u + .2f) - Wd / 2, FMath::Abs(v) - .5f);
		auto Bowl = [&](float bx, float by, float br) { const float r = Ring(u, v, bx, by, br, Wd); return u < -.2f ? FMath::Max(r, FMath::Abs(v - by) - br - Wd / 2) : r; };
		auto Cut = [&](float d) { return FMath::Max(d, -.2f - u); };   // nothing left of the stem's middle
		const float Up = Cut(Bowl(-.06f, -.255f, .205f)), Lo = Cut(Bowl(-.04f, .235f, .225f));
		return FMath::Min(Stem, FMath::Min(Up, Lo)) * H;
	}

	UTexture2D* PaintFace()
	{
		FPainter P;
		const int32 S = FPainter::S;
		const float R = S / 2.f, k = S / 128.f;
		// the brass, light at the top left to dark at the bottom right
		const FVector2f G0(S * .2f, S * .08f), G1(S * .8f, S * .95f), Gd = G1 - G0;
		const FLinearColor C0 = Rgb(0xed, 0xc4, 0x66), C1 = Rgb(0xd6, 0xa2, 0x42), C2 = Rgb(0xb0, 0x7a, 0x28);
		for (int32 y = 0; y < S; ++y) for (int32 x = 0; x < S; ++x)
		{
			const float t = FMath::Clamp(FVector2f::DotProduct(FVector2f(x + .5f, y + .5f) - G0, Gd) / Gd.SizeSquared(), 0.f, 1.f);
			FLinearColor C = t < .55f ? FMath::Lerp(C0, C1, t / .55f) : FMath::Lerp(C1, C2, (t - .55f) / .45f);
			C.A = FPainter::Cover(Disc(x + .5f, y + .5f, R, R, R - 1));
			P.Px[y * S + x] = C;
		}
		// blotched with a lighter and a darker tone (the slice's seeded marks)
		int32 Seed = 7;
		auto Rand = [&]() { Seed = (int32)(((int64)Seed * 16807) % 2147483647); return Seed / 2147483647.f; };
		for (int32 m = 0; m < 60; ++m)
		{
			const float mx = Rand() * S, my = Rand() * S, mr = (5 + Rand() * 13) * k;
			const FLinearColor C = m % 2 ? Rgb(248, 218, 138, .15f) : Rgb(142, 92, 26, .13f);
			const int32 x0 = FMath::Max(0, (int32)(mx - mr - 1)), x1 = FMath::Min(S - 1, (int32)(mx + mr + 1)), y0 = FMath::Max(0, (int32)(my - mr - 1)), y1 = FMath::Min(S - 1, (int32)(my + mr + 1));
			for (int32 y = y0; y <= y1; ++y) for (int32 x = x0; x <= x1; ++x) if (P.Px[y * S + x].A > 0) P.Blend(x, y, C, FPainter::Cover(Disc(x + .5f, y + .5f, mx, my, mr)));
		}
		auto Inside = [&](auto Sdf) { return [&, Sdf](float x, float y) { return FMath::Max(Sdf(x, y), Disc(x, y, R, R, R - 1)); }; };
		P.Shape(Rgb(196, 144, 54, .9f), Inside([&](float x, float y) { return Ring(x, y, R, R, R - S * .045f, S * .09f); }));   // the raised rim
		P.Shape(Rgb(104, 66, 16, .7f), [&](float x, float y) { return Ring(x, y, R, R, R - S * .1f, 2.5f * k); });              // the groove inside it
		P.Shape(Rgb(255, 236, 172, .5f), [&](float x, float y)                                                                    // a lit arc
		{
			float a = FMath::Atan2(y - R, x - R); if (a < 0) a += UE_TWO_PI;
			return a > PI * .9f && a < PI * 1.7f ? Ring(x, y, R, R, R - S * .1f - 2.5f * k, 1.5f * k) : 1e3f;
		});
		for (int32 b = 0; b < 26; ++b)   // the beads, each with its shade
		{
			const float a = b / 26.f * UE_TWO_PI, bx = R + FMath::Cos(a) * (R - S * .17f), by = R + FMath::Sin(a) * (R - S * .17f);
			P.Shape(Rgb(104, 66, 16, .55f), [&](float x, float y) { return Disc(x, y, bx + .8f * k, by + .8f * k, 2.2f * k); });
			P.Shape(Rgb(244, 212, 128, .8f), [&](float x, float y) { return Disc(x, y, bx, by, 2.f * k); });
		}
		const float H = S * .4f;   // the stamped B: shade below, light above, then the letter
		P.Shape(Rgb(98, 62, 14, .75f), [&](float x, float y) { return LetterB(x, y, R + 2.5f * k, R + 3.5f * k, H); });
		P.Shape(Rgb(255, 238, 178, .6f), [&](float x, float y) { return LetterB(x, y, R - 1.5f * k, R - .5f * k, H); });
		P.Shape(Rgb(0xc0, 0x8d, 0x34), [&](float x, float y) { return LetterB(x, y, R, R + 1.f * k, H); });
		UTexture2D* T = UTexture2D::CreateTransient(S, S, PF_B8G8R8A8);
		if (!T) return nullptr;
		T->SRGB = true;
		T->Filter = TF_Trilinear;
		FColor* Dst = static_cast<FColor*>(T->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE));
		for (int32 i = 0; i < S * S; ++i) Dst[i] = P.Px[i].ToFColor(true);
		T->GetPlatformData()->Mips[0].BulkData.Unlock();
		T->UpdateResource();
		T->AddToRoot();   // painted once, kept for the whole visit
		return T;
	}
}

const FSlateBrush* SBrixCoins::FaceBrush()
{
	static FSlateBrush Brush;
	static bool bMade = false;
	if (!bMade)
	{
		bMade = true;
		if (UTexture2D* T = PaintFace())
		{
			Brush.SetResourceObject(T);
			Brush.ImageSize = FVector2D(FPainter::S, FPainter::S);
			Brush.DrawAs = ESlateBrushDrawType::Image;
		}
	}
	return Brush.GetResourceObject() ? &Brush : nullptr;
}

int32 SBrixCoins::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	if (!Sprites || !Sprites->Num() || !ViewportPx || ViewportPx->X < 1 || !FSlateApplication::IsInitialized()) return Layer;
	const float S = (float)(G.GetLocalSize().X / ViewportPx->X);   // the viewport's pixels to this widget's units
	const FSlateRenderTransform& T = G.GetAccumulatedRenderTransform();
	FSlateRenderer* Rd = FSlateApplication::Get().GetRenderer();
	const FSlateResourceHandle White = Rd->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
	const FSlateBrush* Face = FaceBrush();
	const FSlateResourceHandle FaceH = Face ? Rd->GetResourceHandle(*Face) : FSlateResourceHandle();
	const float B = GlowBoost;
	const FColor Lantern(255, 172, 84), Warm(255, 214, 140);
	// each kind of thing in its own batch, back to front: shadows, then the light (added), then the coins' edges, their
	// faces, and the twinkles over them (added)
	TArray<FSlateVertex> Sh, Li, Ed, Fa, Tw;
	TArray<SlateIndex> ShI, LiI, EdI, FaI, TwI;
	for (const FBrixCoinSprite& C : *Sprites)
	{
		const FVector2f P = FVector2f(C.Px) * S;
		const float R = C.R * S;
		if (C.bSpark) { Glint(Tw, TwI, T, P, R, C.Alpha); continue; }
		if (C.bFloor)   // its shadow on the ground, and the pool of its light while it hovers
		{
			const FVector2f F = FVector2f(C.FloorPx) * S;
			const float Fr = C.FloorR * S;
			if (C.Shadow > 0) Fan(Sh, ShI, T, F, Fr, Fr * .34f, A(FColor(20, 14, 8, 97), C.Shadow * C.Alpha), A(FColor(20, 14, 8, 40), C.Shadow * C.Alpha), Sides(Fr));
			if (C.Pool > 0) Fan(Li, LiI, T, F, Fr * 4, Fr * 4 * .34f, Add(Lantern, .16f * B * C.Pool * C.Alpha), FColor(0, 0, 0, 0), Sides(Fr * 4));
		}
		if (C.Glow > 0)   // lantern light round it (rewards.js halo()): a wide soft glow, and a warmer one close in
		{
			const float g = C.Glow * C.Alpha * B;
			Fan(Li, LiI, T, P, R * 2.8f, R * 2.8f, Add(Lantern, .36f * g), FColor(0, 0, 0, 0), Sides(R * 2.8f));
			if (!C.bHaloOnly) Fan(Li, LiI, T, P, R * 1.45f, R * 1.45f, Add(Warm, .22f * g), FColor(0, 0, 0, 0), Sides(R * 1.45f));
		}
		if (C.bHaloOnly) continue;
		// the coin: its edge, seen as it turns, then its face squeezed to the turn and darker as it turns away
		const float Cs = FMath::Cos(C.Spin), Sn = FMath::Sin(C.Spin), Rx = FMath::Max(R * .09f, R * FMath::Abs(Cs)), Th = R * .22f * FMath::Abs(Sn), Dir = Sn * Cs > 0 ? 1.f : -1.f;
		const FColor Edge[3] = { FColor(140, 97, 32), FColor(165, 118, 44), FColor(122, 82, 25) };
		for (int32 k = 3; k >= 1; --k) Fan(Ed, EdI, T, P + FVector2f(Dir * Th * k / 3, 0), Rx * .97f, R * .97f, A(Edge[k - 1], C.Alpha), A(Edge[k - 1], C.Alpha), Sides(R));
		const float Turned = 1 - FMath::Abs(Cs), Dk = 1 - Turned * .45f;
		const FColor Tint((uint8)(255 * Dk), (uint8)(255 * Dk * .96f), (uint8)(255 * Dk * .9f), (uint8)(255 * C.Alpha));
		if (Face)
		{
			const SlateIndex Bq = (SlateIndex)Fa.Num();
			Fa.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(-Rx, -R), FVector2f(0, 0), Tint));
			Fa.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(Rx, -R), FVector2f(1, 0), Tint));
			Fa.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(Rx, R), FVector2f(1, 1), Tint));
			Fa.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P + FVector2f(-Rx, R), FVector2f(0, 1), Tint));
			FaI.Append({ Bq, (SlateIndex)(Bq + 1), (SlateIndex)(Bq + 2), Bq, (SlateIndex)(Bq + 2), (SlateIndex)(Bq + 3) });
		}
		else Fan(Ed, EdI, T, P, Rx, R, A(FColor(237, 196, 102), C.Alpha), A(FColor(176, 122, 40), C.Alpha), Sides(R));
		if (C.Glint > 0) Glint(Tw, TwI, T, P + FVector2f(R * .55f, -R * .6f), R * .7f * C.Glint, C.Alpha);
	}
	int32 L = Layer;
	if (ShI.Num()) FSlateDrawElement::MakeCustomVerts(Out, ++L, White, Sh, ShI, nullptr, 0, 0);
	if (LiI.Num()) FSlateDrawElement::MakeCustomVerts(Out, ++L, White, Li, LiI, nullptr, 0, 0, ESlateDrawEffect::PreMultipliedAlpha);
	if (EdI.Num()) FSlateDrawElement::MakeCustomVerts(Out, ++L, White, Ed, EdI, nullptr, 0, 0);
	if (FaI.Num()) FSlateDrawElement::MakeCustomVerts(Out, ++L, FaceH, Fa, FaI, nullptr, 0, 0);
	if (TwI.Num()) FSlateDrawElement::MakeCustomVerts(Out, ++L, White, Tw, TwI, nullptr, 0, 0, ESlateDrawEffect::PreMultipliedAlpha);
	return L;
}
