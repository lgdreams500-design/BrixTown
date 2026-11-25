#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixCrash.h"
#include "BrixCrashActor.generated.h"

class ABrixTownActor;
class UMaterialInterface;
class UProceduralMeshComponent;
class UTextRenderComponent;
struct FBrixCity;

// Crash damage in the town (CITY-072 to CITY-076; the slice's smash.js): the player's car breaks the street things and
// trees it hits (ABrixTownActor's Breakables) at any speed, and chips building corners. A broken thing is hidden, its
// solids and its light with it; a copy of it topples the way it was hit and lies there, or it bursts into pieces in its
// own colours that bounce and settle as rubble; a corner shows its rough core and stones at its foot. The list of what
// broke, where and when is kept (Saved/BrixDamage.json, CITY-074) and when each thing is mended is worked out from it
// alone (BrixRepairs, CITY-075): a crew member works crouched at it from when they arrive, and then it is whole again.
// Residents near a smash turn and shout (CITY-076, laid on in ABrixPeople).
UCLASS(Config = Game)
class BRIXTOWN_API ABrixCrash : public AActor
{
	GENERATED_BODY()

public:
	ABrixCrash();
	void Start(ABrixTownActor* InTown, const FBrixCity& City, int32 StartDay);
	// the prop kinds (and "tree") World/rules/crash.json breaks, by their own names: ABrixTownActor keeps handles for them
	static TSet<FString> BreakableKinds();
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) float DrawDistance = 140.f;   // m: damage drawn this near the camera (smash.js)
	UPROPERTY(Config) float TagDistance = 12.f;     // m: the repair tag shows this near the player
	UPROPERTY(Config) float TagSize = 0.18f;        // m, its letters
	UPROPERTY(Config) FLinearColor TagColour = FLinearColor(0.16f, 0.12f, 0.10f);

	bool On() const { return bOn; }
	// the car hitting things (smash.js probe(), smash(), chip(); called from ABrixCars)
	int32 Probe(double X, double Y, double Z) const;      // the unbroken breakable standing at this point, or -1
	double BreakSpeed() const { return bOn ? R.BreakKmh / 3.6 : 1e9; }
	double Smash(int32 b, double Vx, double Vz);         // breaks it; returns the share of its speed the car keeps
	void Chip(double X, double Z, double Vx, double Vz);  // a hit on a building wall: chips the corner if it is one
	// smashes residents may still be reacting to (CITY-076): where and when (world seconds)
	struct FRecent { FVector2D P; double T; };
	TArray<FRecent> Recent;
	const FBrixCrashRules& Rules() const { return R; }
	// a repair crew member at work (smash.js worker()): where, facing what, which of the two looks
	struct FWorker { FVector2D P; FVector2D Toward; int32 K; };
	TArray<FWorker> Workers;
	int32 Broken() const { return Damage.Num(); }

private:
	struct FEffect
	{
		FString How;
		int32 B = -1;                   // the breakable, or -1 (a corner)
		FBrixDamage D;
		double T0 = -1;                 // world seconds of the hit; -1 from a save (lying, at rest)
		TArray<FBrixPiece> P;
		UProceduralMeshComponent* M = nullptr;   // a toppled copy
		FVector Axis = FVector::ZeroVector;      // what it topples about (Unreal's frame)
	};
	void Init();
	void Replan();
	void Save() const;
	void Off(int32 b);
	void On(int32 b);
	FEffect Rest(int32 b, const FBrixDamage& D);
	FEffect ChipRest(const FBrixDamage& D);
	void Topple(FEffect& E);
	TArray<FVector3f> Colours(int32 b, int32 N, double Seed) const;
	double Size(int32 b) const;
	double Ground(double X, double Z) const;
	double Now() const;

	ABrixTownActor* Town = nullptr;
	FBrixCrashRules R;
	bool bOn = false, bReady = false, bDirty = true;
	int32 Day0 = 0;
	double Pace = 10, Last = 0;
	TArray<FBrixDamage> Damage;
	TMap<FString, FBrixRepair> Plan;
	TMap<FString, int32> ById;
	TMultiMap<FIntPoint, int32> Grid;   // 4 m cells: the breakables over each
	TMap<FString, FEffect> Effects;
	TArray<TPair<FString, FVector2D>> Depots;
	struct FLotRect { FString Id, Kind; FVector2D Min, Max; };
	TArray<FLotRect> Lots;
	TArray<FBox2D> Paving;
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Debris;   // every piece, chip and rubble at once
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Pool;
	UPROPERTY() TArray<TObjectPtr<UTextRenderComponent>> RepairTags;
	UPROPERTY() TObjectPtr<UMaterialInterface> Material;
	int32 DebrisVerts = -1;
	uint32 LastNearSet = 1;
};
