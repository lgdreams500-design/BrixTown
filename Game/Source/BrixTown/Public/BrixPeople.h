#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixBody.h"
#include "BrixPeople.generated.h"

struct FBrixCity;
struct FBrixDay;
class UMaterialInterface;
class UProceduralMeshComponent;

// The town's residents, where their routine puts them at the clock's minute (CITY-021, CITY-022): worked out each frame
// from the day (Game/Data/day.json) and the clock, never stepped or stored. Only those out of doors within ShowDistance of
// the player are drawn (CITY-024), up to MaxBodies. They are the slice's option B people (ART-004), dressed by their role
// and walking by their role's movement signature (ANIM-002), built by BrixWorld's port of people.js (BrixBody): within
// MidDistance a body with a face, to NearDistance a simpler body whose face fades into the skin, the nearest FullCount
// within FullDistance at full detail, and further out a figure of a few blocks, as the slice draws them.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixPeople : public AActor
{
	GENERATED_BODY()

public:
	ABrixPeople();
	void Start(const FBrixCity& InCity, TSharedPtr<FBrixDay> InDay);
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) float ShowDistance = 90.f;    // m; residents further from the player are not drawn (CITY-024)
	UPROPERTY(Config) int32 MaxBodies = 400;
	UPROPERTY(Config) float LaneWidth = 1.0f;       // m to either side of a pavement's line (the slice's lane)
	UPROPERTY(Config) float FullDistance = 5.f;     // m; the slice's FULL_M: full detail (25,000 vertices) ...
	UPROPERTY(Config) int32 FullCount = 1;          // ... for this many of the nearest
	UPROPERTY(Config) float MidDistance = 16.f;     // the slice's LOD1_M
	UPROPERTY(Config) float NearDistance = 35.f;    // the slice's LOD3_M: blocks beyond
	UPROPERTY(Config) float FaceOutDistance = 30.f; // the slice's FACE_OUT_M: the face is gone here
	UPROPERTY(Config) int32 MaxMeshBodies = 48;     // bodies built as meshes at once; the rest of those within NearDistance are blocks
	UPROPERTY(Config) TArray<int32> RebuildEvery;   // frames between rebuilding a body, by detail (0 full, 1, 2); moved every frame
	UPROPERTY(Config) bool bFlipWinding = false;    // the triangles' turn as Unreal culls them

	int32 Shown() const { return LastShown; }
	double TickMs() const { return TickFrames ? TickSeconds * 1000.0 / TickFrames : 0.0; }   // the people's own time a frame, on average
	void ResetTickTime() { TickSeconds = 0; TickFrames = 0; }

private:
	float GroundAt(const FVector2D& P) const;

	UPROPERTY() TObjectPtr<UProceduralMeshComponent> Figures;   // the far figures' boxes, all in one mesh (moving instances cost the render thread far more)
	UPROPERTY() TArray<TObjectPtr<UProceduralMeshComponent>> Meshes;
	UPROPERTY() TObjectPtr<UMaterialInterface> BodyMaterial;
	TSharedPtr<FBrixDay> Day;
	FBrixPeopleRules Rules;
	TArray<FBrixLook> Looks;            // each resident's, from their role (people.js look()), worked out once
	TArray<FBox2D> Paving;              // the blocks' paving, a step above the street
	float Slab = 0.12f;
	TArray<float> Lane;                 // each resident's own, from the slice's hash
	struct FSlot { int32 Resident = -1; int32 Lod = -1; int32 Verts = 0; int32 Age = 0; };
	TArray<FSlot> Slots;
	FBrixBodyMesh Scratch;
	TArray<FBrixFarBox> BoxScratch;
	int32 FigureCap = 0;                // boxes the far figures' mesh has room for (grows and shrinks in steps, so it is mostly updated in place)
	int32 Frame = 0, LastShown = 0;
	double TickSeconds = 0;
	int32 TickFrames = 0;
};
