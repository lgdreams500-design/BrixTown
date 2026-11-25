// FURN-055, BUILD-021 (BUILD-020): the mouse in the building designer. The piece or inner wall under the mouse, where a
// dragged one goes and the outline round it are BrixDesignPick's, ports of the slice's play.js pickPiece(), pickWall(),
// designMarks() and of the dragging in life.js pointer(); these check them on a storey laid out by hand.
#include "BrixDesignPick.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FBrixLayoutPiece Piece(const TCHAR* Id, double X, double Z, double W, double D, double H, double Yaw = 0, bool bRug = false)
	{
		FBrixLayoutPiece P;
		P.Id = Id; P.Kind = TEXT("k17:coffee"); P.Name = Id; P.X = X; P.Z = Z; P.W = W; P.D = D; P.H = H; P.Yaw = Yaw; P.bRug = bRug;
		return P;
	}
	FBrixRay Down(double X, double Z, double From = 10) { return { FVector3d(X, From, Z), FVector3d(0, -1, 0) }; }   // straight down on a point of the floor
	FBrixPlanWall Wall(const TCHAR* Id, const TCHAR* Ax, double Fix, double A0, double A1, TOptional<double> Door = {})
	{
		FBrixPlanWall W;
		W.Id = Id; W.Ax = Ax; W.Fix = Fix; W.A0 = A0; W.A1 = A1; W.Door = Door;
		return W;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFURN055_APieceIsPickedAndDragged, "BrixTown.World.FURN055_a_piece_is_picked_and_dragged_on_the_grid",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FFURN055_APieceIsPickedAndDragged::RunTest(const FString&)
{
	const double F = 3.4;   // the storey's floor over the ground
	// a rug with a table on it, a chair turned 45 degrees beside them
	const FBrixLayout Lay = { Piece(TEXT("rug"), 5, 5, 2.0, 1.5, .006, 0, true), Piece(TEXT("table"), 5, 5, 1.2, .8, .75), Piece(TEXT("chair"), 7, 5, .5, .5, .9, 45) };
	auto Under = [&](const FBrixRay& R) { const int32 i = BrixDesignPick::PickPiece(Lay, F, R); return Lay.IsValidIndex(i) ? Lay[i].Id : FString(TEXT("-")); };
	TestEqual(TEXT("over the table, the table (not the rug under it)"), Under(Down(5.2, 5.1)), FString(TEXT("table")));
	TestEqual(TEXT("over the rug beside the table, the rug"), Under(Down(5.9, 5.6)), FString(TEXT("rug")));
	TestEqual(TEXT("over bare floor, nothing"), Under(Down(8.5, 2.0)), FString(TEXT("-")));
	TestEqual(TEXT("a turned chair is picked where it stands turned"), Under(Down(7.32, 5.0)), FString(TEXT("chair")));
	TestEqual(TEXT("and not where it would stand unturned"), Under(Down(7.22, 5.22)), FString(TEXT("-")));
	// from the side, level with the table's top: the chair is nearer than the table behind it
	TestEqual(TEXT("the nearest piece along the ray"), Under({ FVector3d(10, F + .5, 5), FVector3d(-1, 0, 0) }), FString(TEXT("chair")));
	TestEqual(TEXT("a ray from under the floor's height looking up picks nothing on the floor"), BrixDesignPick::FloorHit({ FVector3d(5, F + 2, 5), FVector3d(0, 1, 0) }, F).IsSet(), false);
	// a slanted ray meets the floor where it should
	const TOptional<FVector2D> Hit = BrixDesignPick::FloorHit({ FVector3d(0, F + 4, 0), FVector3d(1, -1, .5) }, F);
	TestTrue(TEXT("the floor under a slanted ray"), Hit.IsSet() && Hit->Equals(FVector2D(4, 2), 1e-9));

	// dragging: grabbed off its middle, it keeps that distance from the mouse and stands on the 5 cm grid
	FBrixFurnishRulesD Rules;
	const FVector2D Grab = BrixDesignPick::GrabPiece(Lay[1], FVector2D(5.2, 5.1));
	TestTrue(TEXT("how far its middle is from the mouse"), Grab.Equals(FVector2D(-.2, -.1), 1e-9));
	const FVector2D To = BrixDesignPick::DragPiece(Rules, FVector2D(6.03, 5.52), Grab);
	TestTrue(TEXT("it follows the mouse on the grid"), To.Equals(FVector2D(5.85, 5.4), 1e-9));
	TestTrue(TEXT("a mouse that has not moved leaves it where it was"), BrixDesignPick::DragPiece(Rules, FVector2D(5.2, 5.1), Grab).Equals(FVector2D(5, 5), 1e-9));

	// the outline: four strips just outside its footprint; picked, its four corners stand up too
	TestEqual(TEXT("under the mouse: four strips"), BrixDesignPick::Marks(Lay[1], F, false).Num(), 4);
	const TArray<BrixDesignPick::FMarkBox> Picked = BrixDesignPick::Marks(Lay[1], F, true);
	TestEqual(TEXT("picked: four strips and four corners"), Picked.Num(), 8);
	for (const BrixDesignPick::FMarkBox& M : Picked)
	{
		const bool bOutside = FMath::Abs(M.At.X - 5) > .6 + .03 || FMath::Abs(M.At.Z - 5) > .4 + .03;
		TestTrue(TEXT("each part of the outline is outside the piece, on the floor"), bOutside && M.At.Y > F && M.At.Y < F + .05);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FBUILD021_ADraggedWallCatchesOnWalls, "BrixTown.World.BUILD021_a_dragged_wall_stays_in_the_room_and_catches_on_walls",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::EngineFilter)
bool FBUILD021_ADraggedWallCatchesOnWalls::RunTest(const FString&)
{
	FBrixRoomPlanRules R;
	const double F = 0.3;
	const FVector4f Room(0, 0, 10, 8);
	// a wall east to west with a doorway, and one north to south across the room that it can catch on
	const FBrixPlan Plan = { Wall(TEXT("a"), TEXT("x"), 4, 2, 6, 4.0), Wall(TEXT("b"), TEXT("z"), 7, 0, 8) };
	auto Under = [&](const FBrixRay& Ray) { const int32 i = BrixDesignPick::PickWall(R, Plan, F, 1.25, Ray); return Plan.IsValidIndex(i) ? Plan[i].Id : FString(TEXT("-")); };
	TestEqual(TEXT("over the wall, the wall"), Under(Down(3, 4)), FString(TEXT("a")));
	TestEqual(TEXT("beside it, nothing"), Under(Down(3, 4.2)), FString(TEXT("-")));
	TestEqual(TEXT("over the other wall, that one"), Under(Down(7, 6.5)), FString(TEXT("b")));
	TestEqual(TEXT("a ray over the designer's cut passes over the wall"), Under({ FVector3d(-1, F + 1.5, 4), FVector3d(1, 0, 0) }), FString(TEXT("-")));

	const BrixDesignPick::FWallGrab G = BrixDesignPick::GrabWall(Plan[0], FVector2D(3, 4));
	TestTrue(TEXT("grabbed a metre from its start, on its line"), FMath::IsNearlyEqual(G.Off, 0.0) && FMath::IsNearlyEqual(G.OffA, -1.0));
	TArray<double> Stops = BrixDesignPick::WallStops(Plan, Plan[0], 5, 0, 10);
	TestTrue(TEXT("its ends can stop at the room's sides and at the wall across"), Stops.Num() == 3 && Stops[0] == 0 && Stops[1] == 7 && Stops[2] == 10);
	// dragged south and east: on the grid, its far end catching on the wall across; the doorway goes with it
	const FBrixPlanWall A1 = BrixDesignPick::DragWall(R, Plan, Plan[0], Room, FVector2D(3.83, 5.02), G);
	TestTrue(TEXT("across on the grid"), FMath::IsNearlyEqual(A1.Fix, 5.0));
	TestTrue(TEXT("its end catches on the wall across"), FMath::IsNearlyEqual(A1.A0, 3.0) && FMath::IsNearlyEqual(A1.A1, 7.0));
	TestTrue(TEXT("its doorway goes with it"), A1.Door.IsSet() && FMath::IsNearlyEqual(*A1.Door, 5.0));
	// dragged a little, away from any stop: it follows the mouse on the grid and keeps its length
	const FBrixPlanWall A2 = BrixDesignPick::DragWall(R, Plan, Plan[0], Room, FVector2D(2.52, 3.31), G);
	TestTrue(TEXT("it follows the mouse on the grid"), FMath::IsNearlyEqual(A2.Fix, 3.3) && FMath::IsNearlyEqual(A2.A0, 1.5) && FMath::IsNearlyEqual(A2.A1, 5.5));
	// dragged far out of the room: it stays inside it, clear of the outer wall
	const FBrixPlanWall A3 = BrixDesignPick::DragWall(R, Plan, Plan[0], Room, FVector2D(20, 20), G);
	TestTrue(TEXT("it stays in the room"), FMath::IsNearlyEqual(A3.Fix, 7.8) && FMath::IsNearlyEqual(A3.A0, 6.0) && FMath::IsNearlyEqual(A3.A1, 10.0));
	TestTrue(TEXT("a wall keeps its length when dragged"), FMath::IsNearlyEqual(A1.A1 - A1.A0, 4.0) && FMath::IsNearlyEqual(A3.A1 - A3.A0, 4.0));
	// the outline goes round the wall's own box
	const FBrixLayoutPiece Q = BrixDesignPick::WallAsPiece(R, Plan[0]);
	TestTrue(TEXT("the wall as the piece its outline goes round"), FMath::IsNearlyEqual(Q.X, 4.0, 1e-5) && FMath::IsNearlyEqual(Q.Z, 4.0, 1e-5) && FMath::IsNearlyEqual(Q.W, 4.0, 1e-5) && FMath::IsNearlyEqual(Q.D, R.Thick, 1e-5));
	return true;
}

#endif
