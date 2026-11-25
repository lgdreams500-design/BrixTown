#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixLightRoom.generated.h"

// The Blender reference rooms, built again in Unreal to measure the game's indoor light against them (ADR-0055).
//
// Tools/blender/furniture_kit_18.py renders a shop, a cafe and a kitchen (Art/StyleTests/furniture_kit_18/room_shop.png,
// room_cafe.png, room_kitchen.png) in one shell under one recipe: the kit daylight through a side window (sun 8.5 W/m2
// from (0.78, 0.30, 0.42), sky 0.25), pendants of 70 W in the lantern's colour, the camera 1.3 stops open. This builds
// the same shell (room_shell(): 6.4 x 7 x 3 m, open behind the camera, a deep window in the right wall, five beams), stands
// the same kit pieces at the same places (the meshes are the kits' own, /Game/Furniture), hangs the game's own room lamp
// (ABrixRooms::MakeLamp) where each pendant hangs, turns the room so the game's sun comes in at the window as the
// references' does, and gives the references' camera. A shot of it beside the reference says what the game's lamps,
// materials and eye do differently, in numbers (Tools/unreal/light_reference.py).
//
// Made only with -BrixLightRoom (or -BrixLightRoom=game for the game's own room colours in place of the references'
// plaster and boards), high above the town on a slab of paving of its own, by UBrixLight. In a tour (-BrixTour=) the
// stop "refroom:<shop|cafe|kitchen>" looks through a room's camera, and the time "ref" is when the sun stands right.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixLightRoom : public AActor
{
	GENERATED_BODY()

public:
	ABrixLightRoom();
	void Start(const class ABrixGameMode& Mode, bool bGameColours);
	bool Contains(const FVector& WorldCm) const;            // in one of the rooms
	bool Camera(const FString& Room, FTransform& Out) const;   // the references' camera in that room
	static constexpr float Fov = 93.3f;                     // their 17 mm lens on a 36 mm film
	// The rooms' own light as the game's rooms have it now (ABrixRooms::FillNow), or numbers to try in one run (a tour
	// stop's lamp=, tint=, fill= and filltint=): the lamps' strength times LampScale, in Tint, and the room's own light
	// FillLevel strong (under 0: the game's own) in FillTint
	void Try(float Daylight, float LampScale, const TOptional<FLinearColor>& Tint, float FillLevel, const TOptional<FLinearColor>& FillTint);
	double RefMinute = 455.0;                               // the minute of the day the game's sun is as high as the references'

	UPROPERTY(Config) FVector RoomsAt = FVector(-400.0, -400.0, 250.0);   // metres: x east, y south, z up
	UPROPERTY(Config) float RoomsApart = 40.f;

private:
	struct FOne { FString Key; FVector Origin = FVector::ZeroVector; };
	TArray<FOne> Rooms;
	FRotator Turn = FRotator::ZeroRotator;
	FVector At(const FOne& R, double X, double Y, double Z) const;   // from the references' frame (x right, y away from the camera, z up; metres)
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Boxes;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BoxMat;
	UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Pieces;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Lamps;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Fills;
	float LampCandela = 0.f;
};
