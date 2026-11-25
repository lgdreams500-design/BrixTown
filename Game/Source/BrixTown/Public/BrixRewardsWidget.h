#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"

class UTexture2D;
struct FSlateBrush;

// The coins of a reward or of interest, drawn over the picture (SND-019, SND-020; rewards.js draw()): a brass coin squeezed
// as it turns, its edge showing its thickness, its face painted once as the slice paints it (mottled brass, a raised rim
// and a groove, a ring of beads, a stamped B) into a texture, so it stays round and sharp however near it is; lantern
// light round it and on the ground under it, added to the picture as light is; its shadow; glints as it hovers; sparks
// where coins land. ABrixLife works out where each is (in the viewport's pixels) each frame, far ones first; this only
// paints them.
struct FBrixCoinSprite
{
	FVector2D Px = FVector2D::ZeroVector;   // the viewport's pixels
	float R = 6.f, Spin = 0.f, Glow = 0.f, Alpha = 1.f;
	bool bSpark = false;
	bool bHaloOnly = false;                 // a trail of light behind a coin (rewards.js halo()), no coin
	float Glint = 0.f;                      // a twinkle on its face, 0..1
	bool bFloor = false;                    // its shadow (and its pool of light when Pool > 0) on the ground below it
	FVector2D FloorPx = FVector2D::ZeroVector;
	float FloorR = 0.f, Shadow = 0.f, Pool = 0.f;
};

class BRIXTOWN_API SBrixCoins : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBrixCoins) {}
		SLATE_ARGUMENT(const TArray<FBrixCoinSprite>*, Sprites)
		SLATE_ARGUMENT(const FVector2D*, ViewportPx)
		SLATE_ARGUMENT(float, GlowBoost)
	SLATE_END_ARGS()
	void Construct(const FArguments& A) { Sprites = A._Sprites; ViewportPx = A._ViewportPx; GlowBoost = A._GlowBoost; SetVisibility(EVisibility::HitTestInvisible); }
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(1, 1); }

	static const FSlateBrush* FaceBrush();   // the coin's face (rewards.js texture()), painted once

private:
	const TArray<FBrixCoinSprite>* Sprites = nullptr;
	const FVector2D* ViewportPx = nullptr;
	float GlowBoost = 1.f;
};
