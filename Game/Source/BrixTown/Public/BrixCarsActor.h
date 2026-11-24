#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixCars.h"
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
	void Start(TSharedPtr<FBrixTraffic> InTraffic);
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) float ShowDistance = 160.f;   // m
	UPROPERTY(Config) float NearDistance = 45.f;    // m: the near model within (the slice's 45 m)
	UPROPERTY(Config) float RoadUp = 0.f;           // m: the street's height

	int32 Shown() const { return LastShown; }

private:
	void Build(UProceduralMeshComponent* M, int32 k, int32 Lod);

	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Pool;
	TSharedPtr<FBrixTraffic> Traffic;
	struct FShown { UProceduralMeshComponent* M = nullptr; int32 Lod = -1; int32 Seen = 0; };
	TMap<int32, FShown> Live;           // vehicle -> its mesh while near
	int32 Frame = 0, LastShown = 0;
};
