#pragma once

#include "CoreMinimal.h"
#include "BrixFurnishPlan.h"
#include "BrixRoomPlan.h"

// The mouse in the building designer (BUILD-020, BUILD-021, FURN-055; F-BLD-DESIGN 2): the piece or the inner wall under
// the mouse, where a dragged one goes, and the outline round the one picked. Ports of the slice's play.js pickPiece(),
// pickWall() and designMarks() and of the dragging in life.js pointer(), wallStops(). Everything is in the data's frame:
// x east, y up, z south, metres; a ray is the camera's through the point of the screen under the mouse.

struct BRIXWORLD_API FBrixRay
{
	FVector3d O = FVector3d::ZeroVector;   // x, y (up), z
	FVector3d D = FVector3d(0, -1, 0);
};

namespace BrixDesignPick
{
	// where the ray meets a floor at height FloorY (x, z); none when it looks level or up, or the floor is behind it
	BRIXWORLD_API TOptional<FVector2D> FloorHit(const FBrixRay& Ray, double FloorY);
	// the piece under the mouse: the nearest box the ray goes into, a rug only when nothing stands there; its place in the
	// layout, or INDEX_NONE
	BRIXWORLD_API int32 PickPiece(const FBrixLayout& Lay, double FloorY, const FBrixRay& Ray);
	// the inner wall under the mouse, from the floor up to the designer's cut (CutM above the floor)
	BRIXWORLD_API int32 PickWall(const FBrixRoomPlanRules& R, const FBrixPlan& Plan, double FloorY, double CutM, const FBrixRay& Ray);

	// a piece grabbed at Hit: how far its middle is from there; and where it stands, on the grid, with the mouse over Hit
	BRIXWORLD_API FVector2D GrabPiece(const FBrixLayoutPiece& P, const FVector2D& Hit);
	BRIXWORLD_API FVector2D DragPiece(const FBrixFurnishRulesD& F, const FVector2D& Hit, const FVector2D& Grab);

	// a wall grabbed at Hit: how far it stands across from there, and its start along
	struct FWallGrab { double Off = 0, OffA = 0; };
	BRIXWORLD_API FWallGrab GrabWall(const FBrixPlanWall& W, const FVector2D& Hit);
	// where a wall's ends can stop on the line Fix: the room's sides and every wall across it, in order
	BRIXWORLD_API TArray<double> WallStops(const FBrixPlan& Plan, const FBrixPlanWall& W, double Fix, double Lo, double Hi);
	// the wall as it follows the mouse: across and along on the grid, inside the room (x0 z0 x1 z1), an end within reach of a
	// stop catching on it; its doorway goes with it
	BRIXWORLD_API FBrixPlanWall DragWall(const FBrixRoomPlanRules& R, const FBrixPlan& Plan, const FBrixPlanWall& W, const FVector4f& Room, const FVector2D& Hit, const FWallGrab& G);

	// the outline on the floor round a piece (four strips; picked: wider, and its corners stand up so it reads from any side)
	struct FMarkBox { FVector3d At = FVector3d::ZeroVector; FVector3d Size = FVector3d::OneVector; double YawDeg = 0; };   // its middle on the plan and its bottom (x, y, z); across, up, deep
	BRIXWORLD_API TArray<FMarkBox> Marks(const FBrixLayoutPiece& P, double FloorY, bool bPicked);
	// a wall as the piece its outline goes round (designMarks: 1.2 m high, not turned)
	BRIXWORLD_API FBrixLayoutPiece WallAsPiece(const FBrixRoomPlanRules& R, const FBrixPlanWall& W);
}
