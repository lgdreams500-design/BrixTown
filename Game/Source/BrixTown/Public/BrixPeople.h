#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixPeople.generated.h"

struct FBrixCity;
struct FBrixDay;
class UInstancedStaticMeshComponent;

// The town's residents, where their routine puts them at the clock's minute (CITY-021, CITY-022): worked out each frame
// from the day (Game/Data/day.json) and the clock, never stepped or stored. Only the nearest, out of doors and within
// ShowDistance of the player, have a body (CITY-024), up to MaxBodies at once. The bodies are stand-ins (a body and a head
// in a palette colour, a bob as they walk) until the option B people are brought over (ART-004).
UCLASS(Config = Game)
class BRIXTOWN_API ABrixPeople : public AActor
{
	GENERATED_BODY()

public:
	ABrixPeople();
	void Start(const FBrixCity& InCity, TSharedPtr<FBrixDay> InDay);
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) float ShowDistance = 90.f;    // m; residents further from the player have no body (CITY-024)
	UPROPERTY(Config) int32 MaxBodies = 400;
	UPROPERTY(Config) float StepLength = 0.75f;     // m a step, for the bob of a walk
	UPROPERTY(Config) float LaneWidth = 1.0f;       // m to either side of a pavement's line (the slice's lane)
	UPROPERTY(Config) TArray<FLinearColor> Clothes; // palette colours (ART_DIRECTION 2.3: cream, dusty blue, olive, plum, ...)
	UPROPERTY(Config) FLinearColor Skin = FLinearColor(0.8f, 0.62f, 0.5f);

	int32 Shown() const { return LastShown; }

private:
	float GroundAt(const FVector2D& P) const;

	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Bodies;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Heads;
	TSharedPtr<FBrixDay> Day;
	TArray<FBox2D> Blocks;              // the blocks' paving, a step above the street
	float Slab = 0.12f;
	TArray<int32> SlotOf;               // which resident each body shows (-1: none)
	TArray<float> Lane, Tall;           // each resident's own, from the slice's hash, worked out once
	TArray<int32> Wear;
	int32 LastShown = 0;
};
