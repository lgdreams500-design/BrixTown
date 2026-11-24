#pragma once

#include "CoreMinimal.h"

// One day of every resident's routine (Game/Data/day.json, written by Tools/worldgen/build_play.py from the living-city
// rules, the same day the browser slice draws). Where anyone is follows from the day and the minute alone (CITY-022):
// nothing is stepped or stored. A port of the slice's resident() (Prototype/play/play.js); checked against citysim
// City.at() by the test CITY022_residents_are_where_their_routine_puts_them.

// Where a resident is at a minute, when they are to be seen in the town (not away, not in a vehicle).
struct BRIXWORLD_API FBrixWhere
{
	FVector2D Pos = FVector2D::ZeroVector;   // metres, x east, z south: on the path's line (no lane offset)
	FVector2D Heading = FVector2D(1, 0);     // the way they face
	bool bWalking = false;
	bool bInside = false;                    // at a place inside a building
	int32 Floor = 0;                         // a place upstairs (stacked flats and offices)
	int32 Act = 0;                           // into FBrixDay::Acts
	int32 Pose = 0;                          // 0 standing, 1 walking, 2 sitting (a bench's seat, a fountain's rim)
	float SeatUp = -1.f;                     // m above the street of a seat, when sitting (-1: on the ground)
	float Along = 0.f;                       // metres walked along this walk so far (the step's phase)
	float WalkLeft = 0.f, WalkDone = 0.f;    // metres to the walk's end and from its start (the slice keeps doorways on the line)
};

struct BRIXWORLD_API FBrixDay
{
	struct FPiece
	{
		float T0 = 0.f;                      // minute of the day it starts
		uint8 Kind = 0;                      // 0 stay, 1 walk, 2 away, 3 in a vehicle
		int32 Act = 0;
		FVector2D Pos = FVector2D::ZeroVector;
		bool bInside = false;
		int32 Lot = -1, Floor = 0;
		FVector2D Facing = FVector2D(1, 0);  // a stay's facing (the slice: hash(i, k) turns), worked out once on loading
		int32 Use = -1;                      // a stay at a place (the slice: p[5] >= 0), not just anywhere on the lot
		uint8 Pose = 0;                      // a stay at an open lot's place: 2 on a seat (SeatBrixDay)
		float SeatUp = -1.f;
		TArray<FVector2D> Route;             // a walk's points
		TArray<float> Cum;                   // metres along the route at each point
	};

	int32 Day = 0;
	FString Season, Weekday;
	TArray<FString> Acts, Roles;
	TArray<FString> Ids, Names;
	TArray<int32> Role;
	TArray<FString> Wear;                    // a job's own clothes, as a role name (CITY-105); empty for the role's own
	TMap<FString, FString> RoleSig;          // each role's walk (a movement signature's name)
	TArray<TArray<FPiece>> Plan;             // per resident, in time order

	int32 Num() const { return Plan.Num(); }
	// Where resident i is at minute m of the day; unset when they are away or in a vehicle.
	TOptional<FBrixWhere> Where(int32 Resident, double Minute) const;
};

struct FBrixCity;
// The places on open lots out of doors the day's stays are at, as the slice finds them (play.js resident() and openLot()):
// a garden's bench seats and fountain rims are sat on, facing the garden's middle or out from the rim, a flowerbed faces
// north, a market stall faces its side of the market. Lots are the city's in its blocks' order (the slice's LOTS).
// Returns how many stays it seated.
BRIXWORLD_API int32 SeatBrixDay(FBrixDay& Day, const FBrixCity& City);

// Reads the day (Game/Data/day.json). On failure returns false and says why.
BRIXWORLD_API bool LoadBrixDay(const FString& Path, FBrixDay& OutDay, FString& OutError);

// The slice's hash() (play.js): the same strings give the same number in 0..1 (a resident's lane, a stay's facing).
BRIXWORLD_API double BrixSliceHash(std::initializer_list<FString> Parts);
