#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

// The coins of a reward or of interest, drawn over the picture (SND-019, SND-020; rewards.js draw()): a brass coin squeezed
// as it turns, its edge showing its thickness, a lantern glow round it, sparks where coins land. ABrixLife works out where
// each is (in the viewport's pixels) each frame; this only paints them.
struct FBrixCoinSprite
{
	FVector2D Px = FVector2D::ZeroVector;   // the viewport's pixels
	float R = 6.f, Spin = 0.f, Glow = 0.f, Alpha = 1.f;
	bool bSpark = false;
};

class BRIXTOWN_API SBrixCoins : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBrixCoins) {}
		SLATE_ARGUMENT(const TArray<FBrixCoinSprite>*, Sprites)
		SLATE_ARGUMENT(const FVector2D*, ViewportPx)
	SLATE_END_ARGS()
	void Construct(const FArguments& A) { Sprites = A._Sprites; ViewportPx = A._ViewportPx; SetVisibility(EVisibility::HitTestInvisible); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1, 1); }

private:
	const TArray<FBrixCoinSprite>* Sprites = nullptr;
	const FVector2D* ViewportPx = nullptr;
};
