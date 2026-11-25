#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixMap.h"
#include "Styling/SlateBrush.h"
#include "BrixMapActor.generated.h"

class SWidget;
class UTexture2D;
struct FBrixCity;
struct FBrixDay;

// The maps and the words over the street (UI-012, UI-018, QST-005; play.js drawBase(), drawMaps(), hud(); life.js mark(),
// marks(), markAt(), waypointTag()). The town is drawn once from its data by BrixWorld's BrixDrawMap into a texture; each
// frame the round minimap at the top right shows the 230 m round the player (residents, the people and places quests want,
// the mark, the player's arrow turned the way the camera looks), and M (or the menu's Map page) opens the big map: the whole
// town, zoomed about the pointer with the wheel (1 to 6 times; + and - about the middle, 0 or the middle button for the
// whole town), moved by dragging, a click marking a place (again on the mark clears it). Over the street: the names of
// residents within 9 m (name, job, what they are doing), "◆ quest" or "◆ has something for you" over those a quest wants
// within 45 m, and the mark with how far it is (it goes when reached). What shows is the player's settings: display.minimap,
// display.names and the map group (World/rules/settings.json). For test shots: -BrixMap opens the big map,
// -BrixMapZoom=z,x,z zooms it on a place, -BrixMark=x,z sets the mark.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixMap : public AActor
{
	GENERATED_BODY()

public:
	ABrixMap();
	void Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, const FString& CityDir);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	bool IsOpen() const { return bOpen; }
	void Open();
	void Close();
	void Toggle() { if (bOpen) Close(); else Open(); }
	FString MarkAt(const FVector2D& At, const FString& Name = FString());   // UI-006, UI-012: from a click, or Properties

	UPROPERTY(Config) int32 MapPixels = 2700;      // the big map's texture (the slice's baseHi)
	UPROPERTY(Config) float TreeRadius = 2.5f;     // m, a tree's crown on the map [ASSUMPTION: the slice takes the day's from vegetation.json]
	UPROPERTY(Config) float MiniSize = 204.f;      // px across the round map
	UPROPERTY(Config) float MiniSpan = 230.f;      // m across it
	UPROPERTY(Config) float NameReach = 9.f;       // m: names over residents this near (play.js), on the player's level within NameRise
	UPROPERTY(Config) float NameRise = 3.f;
	UPROPERTY(Config) float QuestReach = 45.f;     // m: the quest marks over heads (life.js mark())
	UPROPERTY(Config) float FolkEvery = 0.25f;     // s between working out where everyone is for the dots

	// for the painters (BrixMapActor.cpp)
	struct FDot { FVector2D Pos; int32 Kind; };   // 0 someone with a line to give, 1 a quest's person or place, 2 the mark
	struct FTag { FVector World; FString Text; float Alpha = 1.f; int32 Size = 14; };
	TArray<FVector2D> Folk;
	TArray<FDot> Dots;
	TArray<FTag> Tags;
	FBrixMapImage Image;
	FBrixMapView View;
	FBrixMark Mark;
	FSlateBrush Brush;
	TArray<const FBrixLot*> Lots;                  // every lot, in the city's order (the economy's lot index)
	FVector2D Player = FVector2D::ZeroVector;
	double Yaw = 0;                                // the camera's, the slice's way: 0 north, clockwise
	bool bMini = true, bNames = true, bPeople = true, bQuests = true, bShops = false, bWaypoint = true;
	const FBrixLot* LotAt(const FVector2D& P) const;
	bool ProjectToScreen(const FVector& World, FVector2D& Out) const;

private:
	void Settings();
	void Gather(float Dt);
	TSharedPtr<FBrixDay> Day;
	UPROPERTY() TObjectPtr<UTexture2D> Texture;
	TSharedPtr<SWidget> Hud, Big;
	bool bOpen = false;
	float FolkLeft = 0.f, TargetLeft = 0.f;
	TArray<FBrixMapTarget> Targets;
	TMap<int32, bool> Marked;                      // resident -> has a line to give (else a quest's)
};
