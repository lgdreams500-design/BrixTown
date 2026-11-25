#pragma once

#include "CoreMinimal.h"
#include "BrixMall.h"
#include "GameFramework/Actor.h"
#include "BrixMallActor.generated.h"

class ABrixTownActor;
class UInstancedStaticMeshComponent;
class UMaterialInstanceDynamic;
class UMaterialInterface;
class UPointLightComponent;
class UStaticMesh;

// Eastgate Mall, inside and out, as the slice builds it (WLD-057 to WLD-068; ADR-0038, ADR-0039, ADR-0041): its sculpted
// shell of ribbons and pierced eggs, the hall's wall open at its four porches, three floors round the court with their
// parapets, bridges, moving stairs and lifts, the egg halls, the court's fountain, gardens, kiosks, benches and rides, and
// every outlet's front and room with what is sold in it. What is drawn is BrixWorld's BrixMall (a port of court.js,
// shell.js and outlets.js) laid as the town's own faces: meshes made at load time in M_Facets (matte, the vertex colour),
// /Game/Mall/M_MallLit (the lit panels, lines of light, screens and lantern glass) and /Game/Mall/M_MallLattice (the eggs'
// openings, the sky seen through them from inside), cut in squares so what is out of view is not drawn; the slice's solids
// as unseen blockers; its lifts' calls given to ABrixRooms (E). The moving stairs carry whoever stands on them. Near the
// camera the nearest of the mall's lanterns and shop lights give real light. Only the mall whose plan is a court with its
// entrances (BrixMallCan) is built so; the other malls are left as the town draws them. The furniture outlet's hall
// (BrixWorld/BrixOutlet, a port of outlet.js) is laid by a second of these, its car park's cars the traffic's own models.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixMall : public AActor
{
	GENERATED_BODY()

public:
	ABrixMall();
	void Start(TSharedPtr<FBrixMall> InMall, ABrixTownActor* Town);
	virtual void Tick(float DeltaSeconds) override;
	const FBrixMall* Data() const { return Mall.Get(); }
	// Whether a point (Unreal cm) is inside the hall, under its roof
	bool Contains(const FVector& WorldCm) const;
	// The outlet room a point (Unreal cm) is in (outlets.js at(), WLD-066), or null
	const FBrixOutletRoom* OutletAt(const FVector& WorldCm) const;
	int32 Triangles() const { return Tris; }

	UPROPERTY(Config) float CellMetres = 40.f;           // the faces are cut into squares this wide, so what is out of view is not drawn
	UPROPERTY(Config) float LitGlow = 2.5f;              // the lit panels' glow (the slice's material 1)
	UPROPERTY(Config) float LanternGlow = 8.f;           // lantern glass (material 2)
	UPROPERTY(Config) float RoomLight = 0.3f;            // the court's even room light on the indoor faces (play.js roomLight(); M_MallRoom)
	UPROPERTY(Config) FLinearColor WarmLit = FLinearColor(1.f, 0.64f, 0.34f);    // a warm light line, a lit case, a tall window (play.js: a shop window by day)
	UPROPERTY(Config) FLinearColor CoolLit = FLinearColor(0.82f, 0.86f, 0.94f);  // a light panel, a lit screen (.9995)
	UPROPERTY(Config) FLinearColor TreadColour = FLinearColor(0.16f, 0.16f, 0.17f);   // the moving stairs' treads (material 28)
	UPROPERTY(Config) FLinearColor DarkGlass = FLinearColor(0.07f, 0.08f, 0.1f);      // a window that is not lit (material 4)
	UPROPERTY(Config) int32 NearLights = 4;              // the mall's lanterns and shop lights nearest the camera on its floor give real light
	UPROPERTY(Config) float LightCandela = 8.f;
	UPROPERTY(Config) float LightReach = 12.f;           // m
	UPROPERTY(Config) FLinearColor LightColour = FLinearColor(1.f, 0.84f, 0.64f);
	UPROPERTY(Config) float LightSeconds = 0.5f;         // how often the nearest are chosen again
	UPROPERTY(Config) float NameSize = 0.62f;            // m: an outlet's name on its sign band

private:
	TSharedPtr<FBrixMall> Mall;
	int32 Tris = 0;
	float LightWait = 0.f;
	UPROPERTY() TObjectPtr<UInstancedStaticMeshComponent> Blockers;
	UPROPERTY() TArray<TObjectPtr<UStaticMesh>> Made;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> PanelMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> LanternMat;
	UPROPERTY() TObjectPtr<UMaterialInstanceDynamic> RoomMat;
	void PlaceLights(const FVector& Eye);
};
