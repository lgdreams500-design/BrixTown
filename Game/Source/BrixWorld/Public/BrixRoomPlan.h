#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
struct FBrixLot;
struct FBrixInside;
struct FBrixStoreyRules;
struct FBrixPartition;

// A storey's room plan (BUILD-021 to BUILD-023; F-BLD-DESIGN level 2): a port of the slice's builder.js RoomPlan, itself a
// port of Tools/citysim/roomplan.py, rule for rule and message for message. A plan is the storey's inner walls, each straight
// on the grid along x or z with at most one doorway and maybe its own colour (BUILD-022); the checks keep the walls in the
// room, out of the way of the door and the stairs, and every room reached from the way in; changes are priced line by line
// (BUILD-023). BUILD021_room_plans_match_the_python runs roomplan.check_script() (inside.json builder.room_plan.checks).

struct BRIXWORLD_API FBrixPlanWall
{
	FString Id;
	FString Ax = TEXT("x");                // the wall runs along x or z
	double Fix = 0, A0 = 0, A1 = 0;         // where it stands across, and from where to where along
	TOptional<double> Door;                 // its doorway's middle, along
	FString Colour;                         // a kit colour id, empty: the storey's own
	bool operator==(const FBrixPlanWall& O) const { return Id == O.Id && Ax == O.Ax && Fix == O.Fix && A0 == O.A0 && A1 == O.A1 && Door == O.Door && Colour == O.Colour; }
};
using FBrixPlan = TArray<FBrixPlanWall>;

struct BRIXWORLD_API FBrixRoomPlanRules
{
	double Grid = 0.05, MinLen = 0.6, Thick = 0.12, DoorW = 1.0, DoorMargin = 0.1, Cell = 0.25, PocketM2 = 1.0;
	double WallM2 = 120, TakeDownM2 = 60, MoveM2 = 150, Doorway = 1500, CloseDoorway = 600, PaintM2 = 24;
	bool Load(const TSharedPtr<FJsonObject>& Rules);
};

// What a storey's plan is checked against (play.js roomFor()): the floor inside the outer walls, what must stay clear (the
// way in on the ground floor, the stairwell), the height under the ceiling and where you come in (then the stairs up)
struct BRIXWORLD_API FBrixRoomSpec
{
	FVector4f Room = FVector4f(0, 0, 0, 0);                 // x0 z0 x1 z1
	TArray<FVector4f> Keep;
	TArray<FVector2D> Entries;
	double Ceiling = 2.8;
};

struct BRIXWORLD_API FBrixPlanLine { FString What; double Qty = 0; int64 Chips = 0; };

namespace BrixRoomPlan
{
	BRIXWORLD_API FVector4f Box(const FBrixRoomPlanRules& R, const FBrixPlanWall& W);
	BRIXWORLD_API TArray<TPair<FString, FString>> Problems(const FBrixRoomPlanRules& R, const FBrixRoomSpec& S, const TArray<FString>& Colours, const FBrixPlan& Walls);   // key, message
	BRIXWORLD_API int64 Cost(const FBrixRoomPlanRules& R, const FBrixPlan& Before, const FBrixPlan& After, double Height, TArray<FBrixPlanLine>& OutLines);
	BRIXWORLD_API TArray<FString> Check(const FBrixRoomPlanRules& R, const FBrixRoomSpec& S, const TArray<FString>& Colours, const FBrixPlan& Before, const FBrixPlan& After, TOptional<int64> Funds);
	BRIXWORLD_API FBrixPlan Apply(const FBrixPlan& Before, const TSharedPtr<FJsonObject>& Edit);   // {set: {id: {..}}, remove: [ids], add: [walls]}
	BRIXWORLD_API FBrixPlanWall WallFromJson(const TSharedPtr<FJsonObject>& O);
	BRIXWORLD_API TSharedPtr<FJsonObject> WallToJson(const FBrixPlanWall& W);
	// storeys.js planOf(): the plan the town gave a storey, from its flats' walls (a straight run with at most one doorway is one wall)
	BRIXWORLD_API FBrixPlan PlanOf(const TArray<FBrixPartition>& Segs);
	// storeys.js wallsOf(): what to draw for a plan, the doorway's piece flagged
	BRIXWORLD_API TArray<FBrixPartition> SegsOf(const FBrixRoomPlanRules& R, const FBrixPlan& Plan);
	BRIXWORLD_API double Gridded(const FBrixRoomPlanRules& R, double V);
}
