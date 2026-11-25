#pragma once

#include "CoreMinimal.h"

// The day's vehicles (CITY-040, CITY-043 to CITY-049, CITY-091): residents' cars, delivery vans, cars passing through and
// buses, each with its home bay and its trips as Tools/citysim/traffic.py works them out (Game/Data/cars.json, written by
// Tools/worldgen/build_play.py with the slice's city_data.js). Where a vehicle is follows from the day and the minute alone:
// a port of the slice's Cars.at() (Prototype/play/cars.js), with traffic.py's car_at() for buses waiting at their first
// stop and out of service after the last run; checked by CITY040_vehicles_are_where_the_traffic_puts_them.
// The Soft box car meshes (ADR-0015) are Tools/blender/car_export_11.py's, copied to Game/Data/car_models.json.
// Coordinates are the data's: metres, x east, z south; a heading (hx, hz) is the way the car's front points.

struct BRIXWORLD_API FBrixVehicleTrip
{
	float T0 = 0.f, T1 = 0.f;                // game minutes
	TOptional<FVector4f> Bay1;               // where it parks at the end [x, z, hx, hz]; unset: it leaves the map
	TArray<FVector4f> Pts;                   // [x, z, hx, hz] every Step minutes along the way
	FVector2f Stowed = FVector2f(-1, -1);    // parked off the street from, to (CITY-093)
};

struct BRIXWORLD_API FBrixVehicle
{
	TOptional<FVector4f> Home;
	TArray<FBrixVehicleTrip> Trips;
	int32 Kind = 0;                          // 0 car, 1 van, 2 bus
	int32 Line = -1;                         // a bus's line
	int32 Owner = -1;                        // the resident whose car it is
};

struct BRIXWORLD_API FBrixCarAt
{
	FVector2D Pos = FVector2D::ZeroVector;
	FVector2D Heading = FVector2D(1, 0);
	bool bMoving = false;
	bool bTrip = false;                      // on a trip (moving or waiting), not parked
};

// One Soft box model: its triangles as exported, each tagged with the part it belongs to (body, roof, tyre, ...).
struct BRIXWORLD_API FBrixCarModel
{
	struct FLod { TArray<FVector3f> P, N; TArray<uint8> Part; };   // three P and N a triangle: along (front +), across (right +), up
	FString Name;
	FVector2f Seat = FVector2f::ZeroVector;  // the driver's seat: along, across
	float SeatY = 0.8f;
	TOptional<float> Back;                   // a back-seat row, along
	FVector4f Box = FVector4f(0, 0, 0, 0);   // along from, to; across from, to
	TArray<FLod> Lods;                       // near, far
	TOptional<float> Roof, Floor;            // the underside of the roof over the seats, the body's bottom under the feet (cars.js)
};

// CITY-049: how resident cars give way to what a player controls (World/rules/traffic.json "give_way"; the rule is
// proved in Tools/citysim/giveway.py): metres from the car's front, game minutes.
struct BRIXWORLD_API FBrixGiveWayRules
{
	double SeeM = 14, StopM = 2.5, PadM = .6, NearM = 120, PersonR = .4, ResetMin = 10, UnloadM = 160;
};

struct BRIXWORLD_API FBrixTraffic
{
	float Step = 0.1f;                       // minutes between a trip's points
	float BusLayover = 0.f;                  // minutes a bus waits at its first stop before a run, and after its last
	float PlayerTopKmh = 50.f;               // the top speed of a car the player drives (World/rules/traffic.json)
	// Cars the player has driven (or is driving): they stay where they were left for the rest of the visit and no longer
	// follow their owner's day (the slice's taken and drv; CITY-061 stand-in).
	TMap<int32, FBrixCarAt> Moved;
	// CITY-049: minutes each resident car runs behind its timeline after giving way near a player (presented state, never
	// stored): it is shown where its timeline had it that long ago. Grows while it is held back, shrinks only at its next
	// stop (GiveWay()).
	TMap<int32, double> Late;
	FBrixGiveWayRules GiveWayRules;
	TArray<FString> Lines;                   // each bus line's colour (a palette name)
	TArray<FBrixVehicle> Vehicles;
	TArray<FString> Parts;
	TMap<FString, FBrixCarModel> Models;

	int32 Num() const { return Vehicles.Num(); }
	// Where vehicle k is at minute m of the day; unset when it is not in the town (away, out of service, parked off the street).
	TOptional<FBrixCarAt> At(int32 k, double Minute) const;
	// The same on its own timeline, without any delay from giving way.
	TOptional<FBrixCarAt> OnTimeline(int32 k, double Minute) const;
	// CITY-049, a port of giveway.py GiveWay.step(): Players are what players control, as circles (x, z, radius): the player
	// on foot, the car they drive as three along its length. Cars, the vehicles to consider (those near players). Dt in
	// game minutes. A moving car with one of them, or a car already held up, ahead in its path slows over SeeM and stands
	// StopM short of it; standing at its next stop it leaves as soon as its timeline is there too; more than ResetMin behind
	// and further than UnloadM from every player it is put back on its timeline.
	void GiveWay(double Minute, double Dt, const TArray<FVector3d>& Players, const TArray<int32>& Cars);
	// Vehicle k standing at C as three circles along its length (giveway.py circles()).
	void Circles(int32 k, const FBrixCarAt& C, TArray<FVector3d>& Out) const;
	// The slice's choice of model for vehicle k (cars.js model()): a van's, else small, estate or pickup by its hash. None for a bus.
	const FBrixCarModel* ModelOf(int32 k) const;
	// Vehicle k's length, width and how far its middle sits ahead of its point (cars.js dims()).
	FVector3f Dims(int32 k) const;
	// Is the point (x, z) inside vehicle k standing at C, grown by Pad (cars.js hits())?
	bool Hits(const FBrixCarAt& C, int32 k, const FVector2D& P, double Pad) const;
};

// Reads Game/Data/cars.json and car_models.json. On failure returns false and says why.
BRIXWORLD_API bool LoadBrixTraffic(const FString& CarsPath, const FString& ModelsPath, FBrixTraffic& Out, FString& OutError);

// Reads the "give_way" rules of World/rules/traffic.json (CITY-049). On failure returns false and says why.
BRIXWORLD_API bool LoadBrixGiveWay(const FString& TrafficRulesPath, FBrixGiveWayRules& Out, FString& OutError);
