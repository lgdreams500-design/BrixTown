#pragma once

#include "CoreMinimal.h"
#include "Widgets/SLeafWidget.h"
#include "BrixBuyMenu.h"

// One thing for sale drawn as an object in a buy menu's cell or showcase (UI-020; buymenu.js shot() and paint()): its
// triangles seen from the rules' camera (fov, pitch), turned about its middle, lit as the slice lights them (a warm low
// sun against a cool sky, a grey bounce from below, darker at its foot), standing in its own soft shade. It turns slowly
// when Spin is on, and by hand when dragged. Drawn with Slate's own triangles: no render target, nothing to load.
class BRIXTOWN_API SBrixThing : public SLeafWidget
{
public:
	SLATE_BEGIN_ARGS(SBrixThing) : _Rules(nullptr), _Spin(false), _Grey(false), _Drag(false), _Size(FVector2D(96, 96)) {}
		SLATE_ARGUMENT(TSharedPtr<const FBrixThingMesh>, Mesh)
		SLATE_ARGUMENT(const FBrixBuyMenuRules*, Rules)
		SLATE_ARGUMENT(bool, Spin)
		SLATE_ARGUMENT(bool, Grey)        // sold out: greyed and darkened
		SLATE_ARGUMENT(bool, Drag)        // can be turned by hand (the showcase)
		SLATE_ARGUMENT(FVector2D, Size)
		SLATE_ARGUMENT(TSharedPtr<double>, Yaw)   // kept by the menu, so the turn carries on across redrawing
	SLATE_END_ARGS()

	void Construct(const FArguments& Args);
	virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Culling, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle& Style, bool bParentEnabled) const override;
	virtual FVector2D ComputeDesiredSize(float) const override { return Size; }
	virtual FReply OnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseButtonUp(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply OnMouseMove(const FGeometry& Geometry, const FPointerEvent& Event) override;

private:
	double YawNow() const;
	TSharedPtr<const FBrixThingMesh> Mesh;
	const FBrixBuyMenuRules* Rules = nullptr;
	bool bSpin = false, bGrey = false, bDrag = false, bHeld = false;
	FVector2D Size;
	TSharedPtr<double> Yaw;
	mutable double Last = -1;
	double DragX = 0, DragYaw = 0;
	bool bDragging = false;
};
