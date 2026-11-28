#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixTubes.h"
#include "BrixTubesActor.generated.h"

class UProceduralMeshComponent;
class UMaterialInstanceDynamic;

// The glass travel tubes over the town and their booths (CITY-108 to CITY-114; F-VEH-TUBE level 2; ADR-0076), built
// once from World/city_01/tubes.json in the look of the approved reference VehiclesGlassTube: clear glass tubes as slim
// as a rider, brass bands every few metres, iron posts with a cradle under the level runs; at each stop a slim glass
// booth on a stone plinth, three brass ribs, a cream cap narrowing through a glass funnel into the tube, a lantern by
// the door. The rides themselves are the server's (ABrixGameMode::RideTube) and the rider's (ABrixCharacter).
UCLASS()
class BRIXTOWN_API ABrixTubes : public AActor
{
	GENERATED_BODY()
public:
	ABrixTubes();
	bool Start(const FString& NetPath, const FString& RulesPath, class ABrixTownActor* Town);
	const FBrixTubeNet& Net() const { return TubeNet; }
	const FBrixTubeRules& Rules() const { return TubeRules; }
	bool IsReady() const { return bReady; }
	// the stop whose booth Pos (metres, the map's x and z) stands in, or empty
	FString StopAt(const FVector2D& Pos) const { return bReady ? BrixTubeStopAt(TubeNet, TubeRules, Pos) : FString(); }
	// the stop chosen from From when the rider has stepped Pick stops on (wrapping), never From itself
	FString Choice(const FString& From, int32 Pick) const;

	UPROPERTY(EditAnywhere) float CullM = 1500.f;   // how far off the tubes are drawn
	UPROPERTY(EditAnywhere) int32 TubeSides = 20;   // round the tube
	UPROPERTY(EditAnywhere) float LanternLight = 1.3f;   // the light it gives at night, as a street lamp's entry in LampLights
private:
	struct FMeshPart { TArray<FVector> V; TArray<int32> T; TArray<FVector> N; };
	void Sweep(FMeshPart& M, const TArray<FVector>& Pts, double R, int32 Sides, bool bClosed) const;
	void Cylinder(FMeshPart& M, const FVector& Base, double R, double H, int32 Sides, double RTop = -1, double GapDeg = 0, double GapYawDeg = 0, bool bCaps = true) const;
	void Ring(FMeshPart& M, const FVector& C, const FVector& Axis, double R, double Thick) const;
	void BoxAt(FMeshPart& M, const FVector& C, const FVector& Half) const;
	void Booth(const FBrixTubeStop& S);
	void Tube(const TArray<FVector>& Line);
	void Flush();

	FBrixTubeNet TubeNet;
	FBrixTubeRules TubeRules;
	bool bReady = false;
	FMeshPart Glass, Brass, Iron, Stone, StoneDark, Cream, BoothWall, IronPost;
	TArray<FVector> LanternAt;                                   // engine units
	TArray<FVector> Posts;                                       // where a post stands (metres), one where tubes run together
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Mesh;        // drawn, no collision
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Lanterns;   // the lantern glass, lit at night with the street lamps' (ABrixLamps)
	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Solid;       // what the player walks against: plinths, booth glass, posts
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> GlassMat;
	UPROPERTY() TArray<TObjectPtr<UMaterialInstanceDynamic>> Mats;
	UPROPERTY() TObjectPtr<class ABrixTownActor> TownRef;
};
