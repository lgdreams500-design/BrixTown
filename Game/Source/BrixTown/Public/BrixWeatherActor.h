#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixWeather.h"
#include "BrixWeatherActor.generated.h"

class ABrixTownActor;
class UMaterialInterface;
class UProceduralMeshComponent;
struct FBrixCity;

// The weather in the town (WLD-037, ADR-0016), worked out each frame from the clock and the day's weather (BrixWeatherNow):
// the game mode dims the sun, fills the shade with grey light and thickens the haze by it (which greys the sky); here the
// paving gets wet and dries again after the rain (ABrixTownActor::SetWet), and rain falls in the world: drops in rings
// round the camera, from 1.6 to 11 m, falling straight down whichever way you look, not where a roof is overhead and not
// while the camera is indoors. ABrixPeople dresses the residents for it (CITY-077 to CITY-080, through Wear).
// -BrixWeather=<clear|cloud|rain|fog> tries one kind for the whole day (the slice's weather key), with what residents
// would wear on a day of it.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixWeather : public AActor
{
	GENERATED_BODY()

public:
	ABrixWeather();
	void Start(const FBrixCity& City, ABrixTownActor* InTown);
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) int32 MaxDrops = 3000;          // drops at full rain
	UPROPERTY(Config) float NearDrops = 1.6f;         // m: the rings' inner and outer edge round the camera (ADR-0016)
	UPROPERTY(Config) float FarDrops = 11.f;
	UPROPERTY(Config) float DropWidth = 0.012f;       // m
	UPROPERTY(Config) float FallHeight = 9.f;         // m: the band they fall through, around the camera's height
	UPROPERTY(Config) FLinearColor DropColour = FLinearColor(0.34f, 0.36f, 0.39f);
	UPROPERTY(Config) float OvercastSkyGain = 2.5f;   // how much of the sunlight the clouds stop comes down as grey sky light (x sky.json overcast_fill)

	FBrixWeatherRules Rules;
	FBrixWear Wear;
	FString Trying;                                    // one kind of weather tried for the whole day, or empty
	FBrixWeatherNow Now;                               // this frame's
	bool bOn = false;
	bool IsTrying() const { return !Trying.IsEmpty() && !Wear.Weather.Contains(Trying); }   // a kind the day does not have: wet-day clothes
	bool Roofed(const FVector2D& P) const;             // under a building's roof (not an open lot)

private:
	ABrixTownActor* Town = nullptr;
	struct FRoof { FVector2D C, Half; double Cos, Sin; };
	TArray<FRoof> Roofs;
	TMultiMap<FIntPoint, int32> RoofGrid;              // 16 m cells
	struct FDrop { float A, R, Phase, Speed; };
	TArray<FDrop> Drops;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Rain;
	UPROPERTY() TObjectPtr<UMaterialInterface> RainMaterial;
	float LastWet = -1.f;
	int32 RainTris = -1;
};
