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

struct BRIXWORLD_API FBrixTraffic
{
	float Step = 0.1f;                       // minutes between a trip's points
	float BusLayover = 0.f;                  // minutes a bus waits at its first stop before a run, and after its last
	TArray<FString> Lines;                   // each bus line's colour (a palette name)
	TArray<FBrixVehicle> Vehicles;
	TArray<FString> Parts;
	TMap<FString, FBrixCarModel> Models;

	int32 Num() const { return Vehicles.Num(); }
	// Where vehicle k is at minute m of the day; unset when it is not in the town (away, out of service, parked off the street).
	TOptional<FBrixCarAt> At(int32 k, double Minute) const;
	// The slice's choice of model for vehicle k (cars.js model()): a van's, else small, estate or pickup by its hash. None for a bus.
	const FBrixCarModel* ModelOf(int32 k) const;
};

// Reads Game/Data/cars.json and car_models.json. On failure returns false and says why.
BRIXWORLD_API bool LoadBrixTraffic(const FString& CarsPath, const FString& ModelsPath, FBrixTraffic& Out, FString& OutError);
