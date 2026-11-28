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
struct FBrixGoodsModels;

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
	// The places to sit and lie its furniture gives (play.js use(): a room set's sofa, armchair and bed in the furniture
	// outlet, its cafe's chairs; a mall's benches and rides; FURN-022, CITY-030): the seat's top in the data's frame
	// (x, z, up; metres), the way the sitter faces as people.js turns a body, and what E offers there
	struct FSeat { FVector At = FVector::ZeroVector; double Yaw = 0; bool bLie = false; FString Label; };
	const TArray<FSeat>& Seats() const { return SeatList; }
	TArray<FSeat> SeatList;
	// the seat nearest where the player stands and within reach (play.js: 2 m either way, 1.2 m up or down), or INDEX_NONE
	int32 SeatNear(const FVector& PawnCm, double& OutMetres) const;
	UPROPERTY(Config) float SeatReach = 1.6f;            // m from a seat at which E offers it
	int32 Triangles() const { return Tris; }
	TSharedPtr<const FBrixGoodsModels> GoodsModels;     // the goods' models, for the outlets' windows (set before Start; FURN-065)

	UPROPERTY(Config) float CellMetres = 40.f;           // the faces are cut into squares this wide, so what is out of view is not drawn
	UPROPERTY(Config) float LitGlow = 1.4f;              // the lit panels' glow (the slice's material 1)
	UPROPERTY(Config) float LanternGlow = 8.f;           // lantern glass (material 2)
	UPROPERTY(Config) float RoomLight = 0.3f;            // the court's even room light on the indoor faces (play.js roomLight(); M_MallRoom)
	UPROPERTY(Config) FLinearColor WarmLit = FLinearColor(1.f, 0.64f, 0.34f);    // a warm light line, a lit case, a tall window (play.js: a shop window by day)
	UPROPERTY(Config) FLinearColor CoolLit = FLinearColor(0.82f, 0.86f, 0.94f);  // a light panel, a lit screen (.9995)
	UPROPERTY(Config) FLinearColor TreadColour = FLinearColor(0.16f, 0.16f, 0.17f);   // the moving stairs' treads (material 28)
	// The moving stairs built to their reference (Art/Reference/Malls/MallsEastgateEscalators): the walked steps drawn, each
	// tread ribbed with a brass nose, steel floor plates with combs, steel skirts with a line of light, brass handrails.
	// And a court mall's floor as its reference's stone with hidden joints, not tiles (MallsEastgateCourt, ADR-0069).
	UPROPERTY(Config) bool StairSteps = true;
	UPROPERTY(Config) bool FloorTiles = false;           // the slice's two-toned tiles on a court mall's floor (the outlet's hall keeps its own)
	UPROPERTY(Config) FLinearColor FloorStone = FLinearColor(0.6f, 0.575f, 0.54f);   // the polished stone's tone, on the floor's own colour (M_MallRoom's Stone)
	UPROPERTY(Config) float FloorRough = 0.1f;           // how polished it is (M_MallRoom's StoneRough; every other face 0.85)
	UPROPERTY(Config) int32 StairRibs = 24;              // ribs across a lane's tread
	UPROPERTY(Config) float StairRib = 1.4f;             // a rib's tone, times the tread's
	UPROPERTY(Config) float StairNose = 0.06f;           // m: the brass nose along a tread's front
	UPROPERTY(Config) float StairPlate = 1.2f;           // m: the floor plate at each end (hours_50.py ESCALATOR plate)
	UPROPERTY(Config) FLinearColor StairBrass = FLinearColor(0.56f, 0.38f, 0.13f);
	UPROPERTY(Config) FLinearColor StairSteel = FLinearColor(0.55f, 0.55f, 0.53f);
	UPROPERTY(Config) FLinearColor StairPlateColour = FLinearColor(0.27f, 0.27f, 0.265f);
	UPROPERTY(Config) FLinearColor DarkGlass = FLinearColor(0.07f, 0.08f, 0.1f);      // a window that is not lit (material 4)
	UPROPERTY(Config) int32 NearLights = 6;              // the mall's lanterns and shop lights nearest the camera on its floor give real light
	UPROPERTY(Config) float LightCandela = 25.f;
	UPROPERTY(Config) float LightReach = 16.f;           // m
	UPROPERTY(Config) FLinearColor LightColour = FLinearColor(1.f, 0.84f, 0.64f);
	UPROPERTY(Config) float LightSeconds = 0.5f;         // how often the nearest are chosen again
	// The look gate's G1 to G3 (ADR-0058): the hall's faces are mottled (RoomMottle: two close tones blotched, W1), a
	// face that looks down has RoomUnder of the hall's even light (the underside of a gallery is darker than its top),
	// and the LightShadows nearest of its lanterns shine down out of their shades (LightCone) and cast shadows, so that
	// people and things stand on the floor in a pool of light with a shadow, not in an even glow.
	UPROPERTY(Config) float RoomMottle = 0.2f;
	UPROPERTY(Config) float RoomUnder = 0.35f;
	UPROPERTY(Config) int32 LightShadows = 1;
	UPROPERTY(Config) float LightCone = 75.f;
	// The hall's faces glow by themselves (RoomLight), which lights nobody: people in it were lit only by what of the
	// sky's light came in, and with the night dark (ADR-0055) they were black in the court. So with the camera in the
	// hall there is a light for people alone (lighting channel 1, which the player's and the residents' bodies are on):
	// from over the camera's shoulder, PeopleLux at night and PeopleLuxDay of that with the day's light also coming in.
	UPROPERTY(Config) float PeopleLux = 0.6f;
	UPROPERTY(Config) float PeopleLuxDay = 0.2f;
	UPROPERTY() TObjectPtr<class UDirectionalLightComponent> PeopleLight;
	float PeopleLevel = 0.f;
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
