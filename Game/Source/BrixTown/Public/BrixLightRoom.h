#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixLightRoom.generated.h"

// The Blender reference rooms, built again in Unreal to measure the game's indoor light against them (ADR-0055), and
// the golden room, where a room's light is set and approved (ADR-0073).
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
// The golden room (ADR-0073; docs/process/LOOK_GATE.md, A room's light) is two more, built to look like the two approved
// references of a home (Tools/blender/hours_50.py):
//   home  IndoorHomeRoom: the same shell closed on all four sides, the living room's pieces in the curve line
//         (furniture_kit_18.py ROOMS["living"]), the boards the references' greyer wood, the sunlit paving outside;
//   hall  IndoorHallway: corridor(): 1.7 m wide with no window of its own, the stair at its far end, the open door of
//         a side room whose window faces the sun, the hall's pieces in the oldtown line.
// Each is lit as ABrixRooms lights a planned room today: the room's own light over its middle (FillNow x FillRoomNow, in
// FillColourNow) and the game's lamp where the reference's lamps stand, at LampNow of their strength. A grey card and a
// grey ball stand where the reference's person stands: the card is what is measured (Tools/unreal/card_read.py), the
// ball shows where the light comes from. No number of theirs is set against a Blender picture: the owner approves the
// pictures, and other rooms are then measured against the card.
//
// Made only with -BrixLightRoom (or -BrixLightRoom=game for the game's own room colours in place of the references'
// plaster and boards), high above the town on a slab of paving of its own, by UBrixLight. In a tour (-BrixTour=) the
// stop "refroom:<shop|cafe|kitchen|home|hall>" looks through a room's camera, and the time "ref" is when the sun
// stands right.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixLightRoom : public AActor
{
	GENERATED_BODY()

public:
	ABrixLightRoom();
	void Start(const class ABrixGameMode& Mode, bool bGameColours);
	bool Contains(const FVector& WorldCm) const;            // in one of the rooms
	bool Camera(const FString& Room, FTransform& Out, float* FovOut = nullptr) const;   // the references' camera in that room, and its field
	// a golden room's grey card (its four corners) and grey ball (its middle and radius), in the world, centimetres
	bool Card(const FString& Room, TArray<FVector>& Corners, FVector& Ball, float& BallRadius) const;
	static constexpr float Fov = 93.3f;                     // their 17 mm lens on a 36 mm film
	// The rooms' own light as the game's rooms have it now (ABrixRooms::FillNow), or numbers to try in one run (a tour
	// stop's lamp=, tint=, fill= and filltint=): the lamps' strength times LampScale, in Tint, and the room's own light
	// FillLevel strong (under 0: the game's own) in FillTint
	void Try(float Daylight, float LampScale, const TOptional<FLinearColor>& Tint, float FillLevel, const TOptional<FLinearColor>& FillTint);
	double RefMinute = 455.0;                               // the minute of the day the game's sun is as high as the references'

	UPROPERTY(Config) FVector RoomsAt = FVector(-400.0, -400.0, 250.0);   // metres: x east, y south, z up
	UPROPERTY(Config) float RoomsApart = 40.f;
	UPROPERTY(Config) float GoldenAt = 5.f;                 // the golden rooms stand this many RoomsApart along, clear of the others' paving
	UPROPERTY(Config) float WallSmooth = 1.f;               // how smooth a golden room's plaster is: 0 the street's mottle, 1 one even tone, no paint (the owner, 2026-11-28; ADR-0074)
	UPROPERTY(Config) float CardGrey = 0.18f;               // the card's and the ball's grey (a photographer's mid grey)
	UPROPERTY(Config) float CardUp = 1.0f;                  // m: the card's lower edge; it is CardWide x CardHigh, its face to the camera
	UPROPERTY(Config) float CardWide = 0.3f;
	UPROPERTY(Config) float CardHigh = 0.4f;
	UPROPERTY(Config) float BallAcross = 0.3f;              // m: the ball's width, its middle level with the card's

private:
	// a room in the references' frame (x right, y away from the camera, z up; metres): its bounds, its camera, its card
	struct FOne { FString Key; FVector Origin = FVector::ZeroVector; FVector Lo = FVector::ZeroVector, Hi = FVector::ZeroVector;
		FVector CamAt = FVector::ZeroVector, CamLooksAt = FVector::ZeroVector; float Fov = 93.3f; bool bCard = false; FVector CardLo = FVector::ZeroVector, CardHi = FVector::ZeroVector, BallAt = FVector::ZeroVector; };
	TArray<FOne> Rooms;
	FRotator Turn = FRotator::ZeroRotator;
	FVector At(const FOne& R, double X, double Y, double Z) const;   // from the references' frame (x right, y away from the camera, z up; metres)
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Boxes;
	UPROPERTY() TObjectPtr<class UInstancedStaticMeshComponent> Balls;
	UPROPERTY() TObjectPtr<class UMaterialInstanceDynamic> BoxMat;
	UPROPERTY() TArray<TObjectPtr<class UStaticMeshComponent>> Pieces;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Lamps;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> Fills;
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> GoldLamps;   // the golden rooms': lit by the game's rule of today
	UPROPERTY() TArray<TObjectPtr<class UPointLightComponent>> GoldFills;
	float LampCandela = 0.f, GoldCandela = 0.f;
};
