#include "BrixRewardsWidget.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

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
	FColor A(FColor C, float Alpha) { C.A = (uint8)FMath::Clamp(Alpha * C.A, 0.f, 255.f); return C; }
}

int32 SBrixCoins::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	if (!Sprites || !Sprites->Num() || !ViewportPx || ViewportPx->X < 1 || !FSlateApplication::IsInitialized()) return Layer;
	const float S = (float)(G.GetLocalSize().X / ViewportPx->X);   // the viewport's pixels to this widget's units
	const FSlateRenderTransform& T = G.GetAccumulatedRenderTransform();
	const FSlateResourceHandle H = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
	TArray<FSlateVertex> V;
	TArray<SlateIndex> I;
	for (const FBrixCoinSprite& C : *Sprites)
	{
		const FVector2f P = FVector2f(C.Px) * S;
		const float R = C.R * S;
		if (C.bSpark)   // a small four-pointed twinkle
		{
			const FColor W = A(FColor(255, 240, 200, 216), C.Alpha);
			Fan(V, I, T, P, R * .25f, R, W, A(W, 0.f), 8);
			Fan(V, I, T, P, R, R * .25f, W, A(W, 0.f), 8);
			continue;
		}
		if (C.Glow > 0) Fan(V, I, T, P, R * 2.8f, R * 2.8f, A(FColor(255, 172, 84, 92), C.Glow * C.Alpha), A(FColor(255, 172, 84, 0), 0.f));   // lantern light round it
		const float Cs = FMath::Cos(C.Spin), Sn = FMath::Sin(C.Spin), Rx = FMath::Max(R * .09f, R * FMath::Abs(Cs)), Th = R * .22f * FMath::Abs(Sn), Dir = Sn * Cs > 0 ? 1.f : -1.f;
		const FColor Edge[3] = { FColor(140, 97, 32), FColor(165, 118, 44), FColor(122, 82, 25) };
		for (int32 k = 3; k >= 1; --k) Fan(V, I, T, P + FVector2f(Dir * Th * k / 3, 0), Rx, R, A(Edge[k - 1], C.Alpha), A(Edge[k - 1], C.Alpha));
		const float Away = 1 - FMath::Abs(Cs);   // darker as it turns away from the eye
		const FColor Face = FColor((uint8)FMath::Lerp(237.f, 120.f, Away * .4f), (uint8)FMath::Lerp(196.f, 90.f, Away * .4f), (uint8)FMath::Lerp(102.f, 40.f, Away * .4f));
		Fan(V, I, T, P, Rx, R, A(Face, C.Alpha), A(FColor(176, 122, 40), C.Alpha));
		Fan(V, I, T, P, Rx * .72f, R * .72f, A(FColor(214, 162, 66), C.Alpha), A(FColor(196, 144, 54), C.Alpha));   // the raised rim and the face inside it
	}
	FSlateDrawElement::MakeCustomVerts(Out, Layer + 1, H, V, I, nullptr, 0, 0);
	return Layer + 1;
}
