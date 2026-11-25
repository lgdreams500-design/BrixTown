#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixCars.h"
#include "BrixCity.h"
#include "BrixCarsActor.generated.h"

class UMaterialInterface;
class UProceduralMeshComponent;

// The day's vehicles in the town (CITY-040, CITY-091): every car, van and bus where the traffic puts it at the clock's
// minute (BrixWorld FBrixTraffic, a port of the slice's Cars.at()), worked out each frame, never stepped or stored. Cars
// and vans are the Soft box models (ADR-0015) in their own colours as the slice paints them (cars.js meshCar()): the near
// model within NearDistance, the light one further out, none beyond ShowDistance. Buses are the slice's long soft box in
// their line's colour (busBody()). Each vehicle near enough is a procedural mesh built once and then only moved.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixCars : public AActor
{
	GENERATED_BODY()

public:
	ABrixCars();
	void Start(TSharedPtr<FBrixTraffic> InTraffic, const FBrixTram& InTram, const TArray<FBox2D>& InPaving);
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) float ShowDistance = 160.f;   // m
	UPROPERTY(Config) float NearDistance = 45.f;    // m: the near model within (the slice's 45 m)
	UPROPERTY(Config) float RoadUp = 0.f;           // m: the street's height

	int32 Shown() const { return LastShown; }

	// The player driving (CITY-061 stand-in, as the slice's cars.js target(), enter(), drive(), leave()): E at a parked car
	// gets in; W/S speed and brake, A/D steer, Space the handbrake; a wall, a street thing or another car stops it and it
	// rolls back a little; E gets out by the driver's door, and the car stays where it was left. Buses are not ridden yet.
	struct FDrive { int32 K = -1; double X = 0, Z = 0, A = 0, V = 0; };
	const FDrive& Driving() const { return Drive; }
	int32 TargetCar(const FVector2D& P, double& OutD) const;
	bool IsVan(int32 k) const { return Traffic && Traffic->Vehicles.IsValidIndex(k) && Traffic->Vehicles[k].Kind == 1; }   // the nearest parked car within reach of its door, or -1
	bool Enter(int32 k);
	void Leave(FVector2D& OutSpot, double& OutYaw);
	void Step(float Dt, float Throttle, float Steer, bool bHandbrake);
	// where the driver sits in car k: its seat (along, across), how high, and the floor below the seat
	bool DriverSeat(FVector2D& OutPos, double& OutYaw, float& OutUp, float& OutFloor, float Head) const;

private:
	void Build(UProceduralMeshComponent* M, int32 k, int32 Lod);

	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Pool;
	TSharedPtr<FBrixTraffic> Traffic;
	struct FShown { UProceduralMeshComponent* M = nullptr; int32 Lod = -1; int32 Seen = 0; };
	TMap<int32, FShown> Live;           // vehicle -> its mesh while near
	FBrixTram Tram;                     // the trams (CITY-041), drawn as the slice's cars.js tram()
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Trams;
	FDrive Drive;
	TArray<FBox2D> Paving;              // the blocks' paving: a car leaving its bay steps down off it onto the road
	bool Free(double X, double Z, double A) const;
	int32 Frame = 0, LastShown = 0;
};
