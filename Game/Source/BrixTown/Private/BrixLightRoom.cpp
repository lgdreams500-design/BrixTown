#include "BrixLightRoom.h"

#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixRooms.h"
#include "BrixTownActor.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"

namespace
{
	// room_shell("Room", 0, 0, front=2.6) of furniture_kit_17.py: the room in the references' own frame
	constexpr double X0 = -3.2, X1 = 3.2, Y0 = -4.8, Y1 = 2.2, H = 3.0;
	constexpr double WY0 = -0.2, WY1 = 1.2, WZ0 = 0.9, WZ1 = 2.35;   // the window in the right wall
	const FVector SunFrom(0.78, 0.30, 0.42);                          // towards the references' sun
	const FVector CamAt(0.15, -4.4, 1.7), CamLooksAt(0.1, 0.8, 1.0);
	constexpr float PendantLightUp = 1.7f, PendantWatts = 70.f;       // furniture_kit_18.py pendant(): its light, 0.3 m under z = 2

	struct FItem { const TCHAR* Piece; double X, Y, Yaw; };
	struct FRoomSet { const TCHAR* Key; const TCHAR* Line; TArray<FItem> Items; };
	// ROOMS of furniture_kit_18.py
	const TArray<FRoomSet>& Sets()
	{
		static const TArray<FRoomSet> S = {
			{ TEXT("shop"), TEXT("plain"), { { TEXT("p_counter"), 1.4, 1.5, 0 }, { TEXT("p_gondola"), -1.2, 0.3, 0 }, { TEXT("p_rail"), -2.2, 1.9, 0 }, { TEXT("p_table"), 0.6, -0.3, 0 },
				{ TEXT("p_form"), 2.6, 0.2, -0.5 }, { TEXT("p_aboard"), -0.2, -1.4, 0.2 }, { TEXT("d_pendant"), 0.0, 0.5, 0 }, { TEXT("d_pendant"), -2.0, 0.5, 0 } } },
			{ TEXT("cafe"), TEXT("oldtown"), { { TEXT("f_counter"), 1.2, 1.6, 0 }, { TEXT("f_machine"), 2.7, 1.88, 0 }, { TEXT("f_menu"), 1.2, 2.18, 0 }, { TEXT("f_banquette"), -1.9, 1.85, 0 },
				{ TEXT("f_table"), -2.3, 1.15, 0 }, { TEXT("f_table"), -1.3, 1.15, 0 }, { TEXT("f_cakestand"), -0.1, 0.5, 0 }, { TEXT("f_table"), 0.3, -0.7, 0 }, { TEXT("d_barstool"), 0.6, 0.85, 0 },
				{ TEXT("d_pendant"), -1.8, 1.15, 0 }, { TEXT("d_pendant"), 0.3, -0.6, 0 } } },
			{ TEXT("kitchen"), TEXT("plain"), { { TEXT("k_wall"), -2.0, 2.03, 0 }, { TEXT("k_counter"), -2.0, 1.88, 0 }, { TEXT("k_stove"), -0.65, 1.87, 0 }, { TEXT("k_sink"), 0.35, 1.88, 0 },
				{ TEXT("k_wall"), 0.35, 2.03, 0 }, { TEXT("k_fridge"), 1.35, 1.82, 0 }, { TEXT("k_potshelf"), -3.02, -0.2, UE_DOUBLE_HALF_PI }, { TEXT("k17_dining"), 0.2, -0.1, 0 },
				{ TEXT("d_pendant"), 0.2, -0.1, 0 }, { TEXT("o_smallplants"), 2.6, 1.95, 0 } } },
		};
		return S;
	}
}

ABrixLightRoom::ABrixLightRoom()
{
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	RootComponent->SetMobility(EComponentMobility::Movable);
}

FVector ABrixLightRoom::At(const FOne& R, double X, double Y, double Z) const
{
	return R.Origin + Turn.RotateVector(FVector(X, -Y, Z) * 100.0);   // the references' y is away from the camera; a plan seen from above is not mirrored
}

void ABrixLightRoom::Start(const ABrixGameMode& Mode, bool bGameColours)
{
	const FBrixCity* City = Mode.CityData();
	if (!City) return;
	// when the game's sun is as high as the references' (in the morning), and the turn that brings it in at the window
	const FBrixSky& Sky = City->Sky;
	const int32 Season = Sky.SeasonOf(Mode.StartDay);
	const double Want = SunFrom.GetSafeNormal().Z;
	FVector ToSun = FVector::UpVector;
	double Best = 1e9;
	for (double M = Sky.Sunrise[Season] * 60.0; M < (Sky.Sunrise[Season] + Sky.Sunset[Season]) * 30.0; M += 1.0)
	{
		FVector3d P = Sky.SunPlace(Season, Sky.HourAngle(Season, M));   // east +x, up +y, south +z
		P.Normalize();
		if (FMath::Abs(P.Y - Want) < Best) { Best = FMath::Abs(P.Y - Want); RefMinute = M; ToSun = FVector(P.X, P.Z, P.Y); }
	}
	const FVector Local(SunFrom.X, -SunFrom.Y, SunFrom.Z);
	Turn = FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(ToSun.Y, ToSun.X) - FMath::Atan2(Local.Y, Local.X)), 0.0);

	const ABrixRooms* RoomDefaults = GetDefault<ABrixRooms>();
	const ABrixTownActor* Town = GetDefault<ABrixTownActor>();
	// the references' materials (the middle of each painted pair), or the game's own room colours
	const FLinearColor Plaster = bGameColours ? RoomDefaults->PartitionColour : FLinearColor(0.57f, 0.53f, 0.475f);
	const FLinearColor PlasterWarm = bGameColours ? RoomDefaults->PartitionColour : FLinearColor(0.60f, 0.49f, 0.395f);
	const FLinearColor BoardsC = bGameColours ? RoomDefaults->BoardsColour : FLinearColor(0.34f, 0.23f, 0.135f);
	const FLinearColor Wood = bGameColours ? RoomDefaults->FrameColour : FLinearColor(0.30f, 0.20f, 0.13f);
	const FLinearColor StoneDark(0.275f, 0.26f, 0.27f), Teal(0.20f, 0.36f, 0.37f);
	const FVector4 PlasterP(-1, 0, 0, 0), NoneP(0, 0, 0, 0);

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Kit/SM_Kit_Box.SM_Kit_Box"));
	if (!Cube) Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	UMaterialInterface* Greybox = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Greybox.M_Greybox"));
	Boxes = NewObject<UInstancedStaticMeshComponent>(this, TEXT("RoomBoxes"));
	Boxes->SetupAttachment(RootComponent);
	Boxes->SetMobility(EComponentMobility::Movable);
	Boxes->SetStaticMesh(Cube);
	Boxes->NumCustomDataFloats = 12;   // M_Greybox: colour, pattern, its turn (as the rooms' boxes)
	if (Greybox)
	{
		BoxMat = UMaterialInstanceDynamic::Create(Greybox, this);
		BoxMat->SetScalarParameterValue(TEXT("InstanceColour"), 1.f);
		for (int32 i = 0; i < Boxes->GetNumMaterials(); ++i) Boxes->SetMaterial(i, BoxMat);
	}
	Boxes->RegisterComponent();
	AddInstanceComponent(Boxes);

	TArray<FTransform> Ats;
	TArray<TArray<float>> Data;
	const float YawRad = FMath::DegreesToRadians((float)Turn.Yaw);
	for (int32 k = 0; k < Sets().Num(); ++k)
	{
		FOne R;
		R.Key = Sets()[k].Key;
		R.Origin = (RoomsAt + FVector(RoomsApart * k, 0, 0)) * 100.0;
		Rooms.Add(R);
		auto Box = [&](double x0, double x1, double y0, double y1, double z0, double z1, const FLinearColor& C, const FVector4& A, const FVector4& B)
		{
			Ats.Add(FTransform(Turn.Quaternion(), At(R, (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), FVector(x1 - x0, y1 - y0, z1 - z0)));
			Data.Add({ C.R, C.G, C.B, (float)A.X, (float)A.Y, (float)A.Z, (float)A.W, (float)B.X, (float)B.Y, (float)B.Z, (float)B.W, YawRad });
		};
		Box(-30, 30, -30, 30, -0.6, -0.1, Town->TownPave, NoneP, NoneP);                       // the paving it stands on, to give the sun back as a street does
		Box(X0, X1, Y0, Y1, -0.1, 0, BoardsC, Town->FloorBoardsA, Town->FloorBoardsB);          // floor
		Box(X0 - 0.3, X1 + 0.3, Y1, Y1 + 0.3, 0, H, Plaster, PlasterP, NoneP);                  // back wall
		Box(X0, X1, Y1 - 0.03, Y1, 0, 0.14, Wood, PlasterP, NoneP);                             // its skirting
		Box(X0 - 0.3, X0, Y0, Y1, 0, H, PlasterWarm, PlasterP, NoneP);                          // left wall
		Box(X1, X1 + 0.45, Y0, WY0, 0, H, Plaster, PlasterP, NoneP);                            // right wall, round its window
		Box(X1, X1 + 0.45, WY1, Y1, 0, H, Plaster, PlasterP, NoneP);
		Box(X1, X1 + 0.45, WY0, WY1, 0, WZ0, Plaster, PlasterP, NoneP);
		Box(X1, X1 + 0.45, WY0, WY1, WZ1, H, Plaster, PlasterP, NoneP);
		Box(X1 - 0.08, X1 + 0.5, WY0 - 0.08, WY1 + 0.08, WZ0 - 0.06, WZ0, StoneDark, NoneP, NoneP);   // sill
		Box(X1 + 0.45, X1 + 0.5, WY0 - 0.5, WY0 - 0.02, WZ0, WZ1, Teal, NoneP, NoneP);          // shutters, open
		Box(X1 + 0.45, X1 + 0.5, WY1 + 0.02, WY1 + 0.5, WZ0, WZ1, Teal, NoneP, NoneP);
		Box(X0 - 0.3, X1 + 0.45, Y0, Y1 + 0.3, H, H + 0.2, Plaster, PlasterP, NoneP);           // ceiling
		for (int32 i = 0; i < 5; ++i)
		{
			const double bx = X0 + 0.5 + i * (X1 - X0 - 1.0) / 4;
			Box(bx - 0.09, bx + 0.09, Y0, Y1, H - 0.18, H, Wood, PlasterP, NoneP);              // beams
		}
		// the kit's pieces, where the reference stands them. A piece's own frame: x across it, its back towards +y in
		// Blender, which the importer turns to -y
		for (const FItem& It : Sets()[k].Items)
		{
			const FString Name = FString::Printf(TEXT("SM_%s_%s"), It.Piece, Sets()[k].Line);
			if (UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Furniture/%s.%s"), *Name, *Name)))
			{
				UStaticMeshComponent* C = NewObject<UStaticMeshComponent>(this);
				C->SetupAttachment(RootComponent);
				C->SetMobility(EComponentMobility::Movable);
				C->SetStaticMesh(Mesh);
				C->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				C->SetWorldLocationAndRotation(At(R, It.X, It.Y, 0), FRotator(0.0, Turn.Yaw - FMath::RadiansToDegrees(It.Yaw), 0.0));
				C->RegisterComponent();
				AddInstanceComponent(C);
				Pieces.Add(C);
			}
			else UE_LOG(LogTemp, Warning, TEXT("Brix Town: the reference %s has no %s"), Sets()[k].Key, *Name);
			if (FCString::Strcmp(It.Piece, TEXT("d_pendant")) == 0)   // the game's own room lamp where the pendant's light is
			{
				UPointLightComponent* L = ABrixRooms::MakeLamp(this, RootComponent, PendantWatts, RoomDefaults->RoomLampCone, true);
				L->SetWorldLocation(At(R, It.X, It.Y, PendantLightUp));
				L->SetVisibility(true);
				LampCandela = L->Intensity;
				Lamps.Add(L);
			}
		}
		// the room's own light, as ABrixRooms gives its rooms
		UPointLightComponent* F = NewObject<UPointLightComponent>(this);
		F->SetupAttachment(RootComponent);
		F->Mobility = EComponentMobility::Movable;
		F->bUseInverseSquaredFalloff = false;
		F->LightFalloffExponent = 2.f;
		F->Intensity = 0.f;
		F->AttenuationRadius = 1200.f;
		F->LightColor = RoomDefaults->RoomFillColour.ToFColor(true);
		F->CastShadows = false;
		F->SourceRadius = 30.f;
		F->SetVisibility(false);
		F->RegisterComponent();
		AddInstanceComponent(F);
		F->SetWorldLocation(At(R, 0, -1.3, 1.5));
		Fills.Add(F);
	}
	Boxes->AddInstances(Ats, false, true);
	for (int32 n = 0; n < Data.Num(); ++n) Boxes->SetCustomData(n, Data[n], false);
	Boxes->MarkRenderStateDirty();
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d reference rooms built %.0f m up (%s colours), %d pieces, %d lamps; their sun at %02d:%02d"),
		Rooms.Num(), RoomsAt.Z, bGameColours ? TEXT("the game's") : TEXT("the references'"), Pieces.Num(), Lamps.Num(), (int32)RefMinute / 60, (int32)RefMinute % 60);
}

void ABrixLightRoom::Try(float Daylight, float LampScale, const TOptional<FLinearColor>& Tint, float FillLevel, const TOptional<FLinearColor>& FillTint)
{
	const ABrixRooms* D = GetDefault<ABrixRooms>();
	for (UPointLightComponent* L : Lamps)
	{
		L->SetIntensity(LampCandela * LampScale);
		L->SetLightColor(Tint ? *Tint : D->RoomLampColour);
		L->SetVisibility(LampScale > 0.f);
	}
	for (UPointLightComponent* F : Fills)
	{
		const float Level = FillLevel < 0.f ? D->FillNow(Daylight) : FillLevel;
		F->SetIntensity(Level);
		F->SetLightColor(FillTint ? *FillTint : D->RoomFillColour);
		F->SetVisibility(Level > 0.f);
	}
}

bool ABrixLightRoom::Contains(const FVector& WorldCm) const
{
	for (const FOne& R : Rooms)
	{
		const FVector L = Turn.UnrotateVector(WorldCm - R.Origin) / 100.0;   // (x, -y, z) of the references' frame
		if (L.X > X0 - 0.5 && L.X < X1 + 0.5 && -L.Y > Y0 - 1.0 && -L.Y < Y1 + 0.5 && L.Z > -0.5 && L.Z < H + 0.5) return true;
	}
	return false;
}

bool ABrixLightRoom::Camera(const FString& Room, FTransform& Out) const
{
	for (const FOne& R : Rooms)
		if (R.Key == Room)
		{
			const FVector From = At(R, CamAt.X, CamAt.Y, CamAt.Z), To = At(R, CamLooksAt.X, CamLooksAt.Y, CamLooksAt.Z);
			Out = FTransform((To - From).Rotation(), From);
			return true;
		}
	return false;
}
