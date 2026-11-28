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
	constexpr float FloorLampUp = 1.45f, TableLampUp = 1.0f;          // where a floor lamp's and a table lamp's light is

	// corridor() of hours_50.py: the hallway in its own frame (half its width, its height, its ends, the stair's foot)
	constexpr double CW = 0.85, CH = 2.7, CY0 = -2.4, CY1 = 7.4, CST = 4.6;
	constexpr double CD0 = 0.6, CD1 = 1.9, CDH = 2.2;                             // the side room's open door, in the right wall
	constexpr double RX0 = CW + 0.15, RX1 = CW + 4.2, RY0 = -0.6, RY1 = 3.6;      // the side room
	constexpr double SWY0 = 0.4, SWY1 = 2.2, SWZ0 = 0.9, SWZ1 = 2.3;              // its window, in the wall facing the sun
	const FVector HallCamAt(-0.5, CY0 + 0.2, 1.6), HallCamLooksAt(0.35, CY1, 1.5);
	constexpr float HallFov = 96.7f;                                              // its 16 mm lens on a 36 mm film

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
	// the golden rooms' pieces: ROOMS["living"] of furniture_kit_18.py in the curve line, and corridor()'s in oldtown
	const TArray<FItem>& LivingItems()
	{
		static const TArray<FItem> S = { { TEXT("l_fireplace"), -0.3, 1.98, 0 }, { TEXT("l_piano"), -2.2, 1.9, 0 }, { TEXT("l_records"), 1.7, 1.95, 0 },
			{ TEXT("k17_sofa"), -2.6, -0.4, UE_DOUBLE_HALF_PI }, { TEXT("k17_armchair"), 1.4, 0.4, -UE_DOUBLE_HALF_PI }, { TEXT("l_sidetable"), 1.4, -0.5, 0 },
			{ TEXT("l_magrack"), -1.4, -0.6, 0 }, { TEXT("k17_rug"), -0.3, 0.6, 0 }, { TEXT("o_painting"), -0.3, 2.14, 0 }, { TEXT("k17_floor_lamp"), 2.6, 1.5, 0 } };   // the painting 4 cm further off the wall than the reference's 2.18: at 2 cm the engine drew ragged bits of the wall through it
		return S;
	}
	const TArray<FItem>& HallItems()
	{
		static const TArray<FItem> S = { { TEXT("h_coatstand"), -CW + 0.3, 3.9, UE_DOUBLE_HALF_PI }, { TEXT("h_console"), -CW + 0.2, 2.4, UE_DOUBLE_HALF_PI },
			{ TEXT("h_mirror"), -CW + 0.05, 2.4, UE_DOUBLE_HALF_PI }, { TEXT("k17_rug"), 0.0, 1.0, 0 }, { TEXT("d_pendant"), 0.0, 1.2, 0 } };
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
	// the golden rooms' boards: the references' greyer wood (hours_50.py greyer(M_Boards 0.4), "so the bounced light is not orange")
	const FLinearColor BoardsGrey = bGameColours ? RoomDefaults->BoardsColour : FLinearColor(0.303f, 0.237f, 0.180f);
	const FLinearColor Wood = bGameColours ? RoomDefaults->FrameColour : FLinearColor(0.30f, 0.20f, 0.13f);
	const FLinearColor StoneDark(0.275f, 0.26f, 0.27f), Teal(0.20f, 0.36f, 0.37f);
	const FLinearColor Paving(0.34f, 0.315f, 0.31f);                  // M_OutsidePaving, the middle of its pair: the sunlit street outside
	const FLinearColor Grey(CardGrey, CardGrey, CardGrey);
	const FVector4 PlasterP(-1, 0, 0, 0), NoneP(0, 0, 0, 0);
	const FVector4 SmoothP(-1, WallSmooth, 0, 0), EvenP(-1, 1, 0, 0);   // a golden room's paint, and the card's one even grey
	// the references' floor (furniture_kit_17.py M_Boards: a brick texture at 0.25): wide boards 2 m by 1 m, a dark joint between
	const FVector4 WideBoardsA(0, 2.0, 1.0, 0.02), WideBoardsB(0.35, 0, 0, 0);

	UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Game/Kit/SM_Kit_Box.SM_Kit_Box"));
	if (!Cube) Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
	// the rooms' copy of the town's material, where painted plaster has its "how smooth" (Tools/unreal/make_greybox_material.py
	// with BRIX_MATERIAL=M_Room); without it the town's own, whose plaster is the street's strong mottle
	UMaterialInterface* Greybox = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Room.M_Room"));
	if (!Greybox) Greybox = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Greybox.M_Greybox"));
	if (Greybox)
	{
		BoxMat = UMaterialInstanceDynamic::Create(Greybox, this);
		BoxMat->SetScalarParameterValue(TEXT("InstanceColour"), 1.f);
	}
	auto Group = [&](const TCHAR* Name, UStaticMesh* Mesh)
	{
		UInstancedStaticMeshComponent* G = NewObject<UInstancedStaticMeshComponent>(this, Name);
		G->SetupAttachment(RootComponent);
		G->SetMobility(EComponentMobility::Movable);
		G->SetStaticMesh(Mesh);
		G->NumCustomDataFloats = 12;   // M_Greybox: colour, pattern, its turn (as the rooms' boxes)
		if (BoxMat) for (int32 i = 0; i < G->GetNumMaterials(); ++i) G->SetMaterial(i, BoxMat);
		G->RegisterComponent();
		AddInstanceComponent(G);
		return G;
	};
	Boxes = Group(TEXT("RoomBoxes"), Cube);

	TArray<FTransform> Ats, BallAts;
	TArray<TArray<float>> Data, BallData;
	const float YawRad = FMath::DegreesToRadians((float)Turn.Yaw);
	auto Box = [&](const FOne& R, double x0, double x1, double y0, double y1, double z0, double z1, const FLinearColor& C, const FVector4& A, const FVector4& B)
	{
		Ats.Add(FTransform(Turn.Quaternion(), At(R, (x0 + x1) / 2, (y0 + y1) / 2, (z0 + z1) / 2), FVector(x1 - x0, y1 - y0, z1 - z0)));
		Data.Add({ C.R, C.G, C.B, (float)A.X, (float)A.Y, (float)A.Z, (float)A.W, (float)B.X, (float)B.Y, (float)B.Z, (float)B.W, YawRad });
	};
	// room_shell(): the room all of these stand in. A golden room is closed behind the camera too (hours_50.py gives the
	// rooms a fourth wall), its floor is the greyer boards, and the sunlit street lies outside its window.
	auto Shell = [&](const FOne& R, bool bGolden)
	{
		const FVector4 Paint = bGolden ? SmoothP : PlasterP, Plain = bGolden ? SmoothP : NoneP;   // a golden room's plain surfaces are painted: with no pattern at all a box draws black
		if (bGolden) Box(R, -30, 30, -30, 30, -0.6, -0.1, Paving, PlasterP, NoneP);
		else Box(R, -30, 30, -30, 30, -0.6, -0.1, Town->TownPave, NoneP, NoneP);                 // the paving it stands on, to give the sun back as a street does
		if (bGolden) Box(R, X0, X1, Y0, Y1, -0.1, 0, BoardsGrey, WideBoardsA, WideBoardsB);       // floor
		else Box(R, X0, X1, Y0, Y1, -0.1, 0, BoardsC, Town->FloorBoardsA, Town->FloorBoardsB);
		Box(R, X0 - 0.3, X1 + 0.3, Y1, Y1 + 0.3, 0, H, Plaster, Paint, NoneP);                  // back wall
		Box(R, X0, X1, Y1 - 0.03, Y1, 0, 0.14, Wood, Paint, NoneP);                             // its skirting
		Box(R, X0 - 0.3, X0, Y0, Y1, 0, H, PlasterWarm, Paint, NoneP);                          // left wall
		Box(R, X1, X1 + 0.45, Y0, WY0, 0, H, Plaster, Paint, NoneP);                            // right wall, round its window
		Box(R, X1, X1 + 0.45, WY1, Y1, 0, H, Plaster, Paint, NoneP);
		Box(R, X1, X1 + 0.45, WY0, WY1, 0, WZ0, Plaster, Paint, NoneP);
		Box(R, X1, X1 + 0.45, WY0, WY1, WZ1, H, Plaster, Paint, NoneP);
		Box(R, X1 - 0.08, X1 + 0.5, WY0 - 0.08, WY1 + 0.08, WZ0 - 0.06, WZ0, StoneDark, Plain, NoneP);   // sill
		Box(R, X1 + 0.45, X1 + 0.5, WY0 - 0.5, WY0 - 0.02, WZ0, WZ1, Teal, Plain, NoneP);          // shutters, open
		Box(R, X1 + 0.45, X1 + 0.5, WY1 + 0.02, WY1 + 0.5, WZ0, WZ1, Teal, Plain, NoneP);
		Box(R, X0 - 0.3, X1 + 0.45, Y0 - (bGolden ? 0.3 : 0.0), Y1 + 0.3, H, H + 0.2, Plaster, Paint, NoneP);   // ceiling
		for (int32 i = 0; i < 5; ++i)
		{
			const double bx = X0 + 0.5 + i * (X1 - X0 - 1.0) / 4;
			Box(R, bx - 0.09, bx + 0.09, Y0, Y1, H - 0.18, H, Wood, Paint, NoneP);              // beams
		}
		if (bGolden) Box(R, X0 - 0.3, X1 + 0.45, Y0 - 0.3, Y0, 0, H, Plaster, Paint, NoneP);    // the wall behind the camera
	};
	// a kit's piece, where the reference stands it. A piece's own frame: x across it, its back towards +y in Blender,
	// which the importer turns to -y
	auto Stand = [&](const FOne& R, const FItem& It, const TCHAR* Line)
	{
		const FString Name = FString::Printf(TEXT("SM_%s_%s"), It.Piece, Line);
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
		else UE_LOG(LogTemp, Warning, TEXT("Brix Town: the reference %s has no %s"), *R.Key, *Name);
	};
	// the room's own light, made as ABrixRooms makes it (PlaceLights): even, without shadows, fading out at its reach
	auto OwnLight = [&](const FOne& R, double X, double Y, double Z, float ReachCm, TArray<TObjectPtr<UPointLightComponent>>& Into)
	{
		UPointLightComponent* F = NewObject<UPointLightComponent>(this);
		F->SetupAttachment(RootComponent);
		F->Mobility = EComponentMobility::Movable;
		F->bUseInverseSquaredFalloff = false;
		F->LightFalloffExponent = 2.f;
		F->Intensity = 0.f;
		F->AttenuationRadius = ReachCm;
		F->LightColor = RoomDefaults->RoomFillColour.ToFColor(true);
		F->CastShadows = false;
		F->SourceRadius = 30.f;
		F->SetVisibility(false);
		F->RegisterComponent();
		AddInstanceComponent(F);
		F->SetWorldLocation(At(R, X, Y, Z));
		Into.Add(F);
	};
	// the game's own room lamp where a lamp of the reference gives its light
	auto GoldLamp = [&](const FOne& R, double X, double Y, double Z, bool bShadows)
	{
		UPointLightComponent* L = ABrixRooms::MakeLamp(this, RootComponent, RoomDefaults->RoomLampWatts, RoomDefaults->RoomLampCone, bShadows);
		L->SetWorldLocation(At(R, X, Y, Z));
		L->SetVisibility(true);
		GoldCandela = L->Intensity;
		GoldLamps.Add(L);
	};
	// the grey card on its post, its face to the camera (which looks along +y), and the grey ball beside it
	auto CardAt = [&](FOne& R, double X, double Y)
	{
		R.bCard = true;
		R.CardLo = FVector(X - CardWide / 2, Y, CardUp);
		R.CardHi = FVector(X + CardWide / 2, Y, CardUp + CardHigh);
		R.BallAt = FVector(X + CardWide / 2 + 0.1 + BallAcross / 2, Y, CardUp + CardHigh / 2);
		Box(R, R.CardLo.X, R.CardHi.X, Y, Y + 0.01, R.CardLo.Z, R.CardHi.Z, Grey, EvenP, NoneP);
		Box(R, X - 0.01, X + 0.01, Y + 0.01, Y + 0.03, 0, R.CardLo.Z + 0.05, Wood, PlasterP, NoneP);
		Box(R, R.BallAt.X - 0.01, R.BallAt.X + 0.01, Y - 0.01, Y + 0.01, 0, R.BallAt.Z - BallAcross / 2 + 0.02, Wood, PlasterP, NoneP);
		BallAts.Add(FTransform(Turn.Quaternion(), At(R, R.BallAt.X, R.BallAt.Y, R.BallAt.Z), FVector(BallAcross)));   // the engine's sphere is 1 m across
		BallData.Add({ Grey.R, Grey.G, Grey.B, -1.f, 1.f, 0.f, 0.f, 0.f, 0.f, 0.f, 0.f, YawRad });
	};
	// a planned room's own light reaches RoomFillReach of its lot's diagonal (ABrixRooms::PlaceLights): here the room's own floor's
	auto ReachCm = [&](double Wide, double Deep)
	{
		return FMath::Max(RoomDefaults->RoomLampReach, RoomDefaults->RoomFillReach * (float)FMath::Sqrt(Wide * Wide + Deep * Deep)) * 100.f;
	};

	for (int32 k = 0; k < Sets().Num(); ++k)
	{
		FOne R;
		R.Key = Sets()[k].Key;
		R.Origin = (RoomsAt + FVector(RoomsApart * k, 0, 0)) * 100.0;
		R.Lo = FVector(X0 - 0.5, Y0 - 1.0, -0.5); R.Hi = FVector(X1 + 0.5, Y1 + 0.5, H + 0.5);
		R.CamAt = CamAt; R.CamLooksAt = CamLooksAt; R.Fov = Fov;
		Shell(R, false);
		for (const FItem& It : Sets()[k].Items)
		{
			Stand(R, It, Sets()[k].Line);
			if (FCString::Strcmp(It.Piece, TEXT("d_pendant")) == 0)   // the game's own room lamp where the pendant's light is
			{
				UPointLightComponent* L = ABrixRooms::MakeLamp(this, RootComponent, PendantWatts, RoomDefaults->RoomLampCone, true);
				L->SetWorldLocation(At(R, It.X, It.Y, PendantLightUp));
				L->SetVisibility(true);
				LampCandela = L->Intensity;
				Lamps.Add(L);
			}
		}
		OwnLight(R, 0, -1.3, 1.5, 1200.f, Fills);   // the room's own light, as ABrixRooms gave its rooms (ADR-0055)
		Rooms.Add(R);
	}

	// ---- the golden room (ADR-0073): the home room ----
	{
		FOne R;
		R.Key = TEXT("home");
		R.Origin = (RoomsAt + FVector(RoomsApart * GoldenAt, 0, 0)) * 100.0;
		R.Lo = FVector(X0 - 0.5, Y0 - 0.5, -0.5); R.Hi = FVector(X1 + 0.5, Y1 + 0.5, H + 0.5);
		R.CamAt = CamAt; R.CamLooksAt = CamLooksAt; R.Fov = Fov;
		Shell(R, true);
		for (const FItem& It : LivingItems()) Stand(R, It, TEXT("curve"));
		GoldLamp(R, 2.6, 1.5, FloorLampUp, true);       // the floor lamp
		GoldLamp(R, 1.4, -0.5, TableLampUp, false);     // the lamp on the side table
		OwnLight(R, (X0 + X1) / 2, (Y0 + Y1) / 2, H / 2, ReachCm(X1 - X0, Y1 - Y0), GoldFills);
		CardAt(R, 0.9, -1.4);                           // where the reference's person stands
		Rooms.Add(R);
	}
	// ---- the golden room: its hallway (corridor() of hours_50.py, box for box) ----
	{
		FOne R;
		R.Key = TEXT("hall");
		R.Origin = (RoomsAt + FVector(RoomsApart * (GoldenAt + 2.f), 0, 0)) * 100.0;
		R.Lo = FVector(-CW - 0.5, CY0 - 0.5, -0.5); R.Hi = FVector(RX1 + 0.6, CY1 + 0.5, 2 * CH + 0.6);
		R.CamAt = HallCamAt; R.CamLooksAt = HallCamLooksAt; R.Fov = HallFov;
		Box(R, -30, 30, -30, 30, -0.6, -0.1, Paving, PlasterP, NoneP);                                        // the sunlit street outside
		Box(R, -CW, CW, CY0, CY1, -0.1, 0.0, BoardsGrey, WideBoardsA, WideBoardsB);             // floor
		Box(R, -CW - 0.3, CW + 0.3, CY0 - 0.3, CST, CH, CH + 0.2, Plaster, SmoothP, NoneP);                  // ceiling
		Box(R, -CW - 0.3, CW + 0.45, CST - 0.3, CST, CH, 2 * CH + 0.4, Plaster, SmoothP, NoneP);             // the stairwell closed above
		Box(R, -CW - 0.3, CW + 0.3, CST - 0.3, CY1 + 0.3, 2 * CH + 0.2, 2 * CH + 0.4, Plaster, SmoothP, NoneP);   // over the stairwell
		Box(R, -CW - 0.3, CW + 0.3, CY0 - 0.3, CY0, 0, CH, Plaster, SmoothP, NoneP);                         // behind the camera
		Box(R, -CW - 0.3, CW + 0.3, CY1, CY1 + 0.3, 0, 2 * CH + 0.2, Plaster, SmoothP, NoneP);               // the far end
		Box(R, -CW - 0.3, -CW, CY0, CY1, 0, 2 * CH + 0.2, PlasterWarm, SmoothP, NoneP);                      // left wall
		Box(R, CW, CW + 0.15, CY0, CD0, 0, CH, Plaster, SmoothP, NoneP);                                     // right wall, round the door
		Box(R, CW, CW + 0.15, CD1, CY1, 0, 2 * CH + 0.2, Plaster, SmoothP, NoneP);
		Box(R, CW, CW + 0.15, CD0, CD1, CDH, CH, Plaster, SmoothP, NoneP);
		Box(R, CW - 0.02, CW + 0.17, CD0 - 0.06, CD0, 0, CDH + 0.06, Wood, SmoothP, NoneP);                  // the door's frame
		Box(R, CW - 0.02, CW + 0.17, CD1, CD1 + 0.06, 0, CDH + 0.06, Wood, SmoothP, NoneP);
		Box(R, CW - 0.02, CW + 0.17, CD0 - 0.06, CD1 + 0.06, CDH, CDH + 0.06, Wood, SmoothP, NoneP);
		Box(R, -CW, -CW + 0.03, CY0, CY1, 0, 0.14, Wood, SmoothP, NoneP);                                    // skirting
		// the room beside it: its window, in the wall facing the sun, lets the day in
		Box(R, RX0, RX1, RY0, RY1, -0.1, 0.0, BoardsGrey, WideBoardsA, WideBoardsB);
		Box(R, RX0, RX1 + 0.3, RY0 - 0.3, RY1 + 0.3, CH, CH + 0.2, Plaster, SmoothP, NoneP);
		Box(R, RX0, RX1 + 0.3, RY0 - 0.3, RY0, 0, CH, Plaster, SmoothP, NoneP);
		Box(R, RX0, RX1 + 0.3, RY1, RY1 + 0.3, 0, CH, Plaster, SmoothP, NoneP);
		Box(R, RX1, RX1 + 0.4, RY0, SWY0, 0, CH, Plaster, SmoothP, NoneP);
		Box(R, RX1, RX1 + 0.4, SWY1, RY1, 0, CH, Plaster, SmoothP, NoneP);
		Box(R, RX1, RX1 + 0.4, SWY0, SWY1, 0, SWZ0, Plaster, SmoothP, NoneP);
		Box(R, RX1, RX1 + 0.4, SWY0, SWY1, SWZ1, CH, Plaster, SmoothP, NoneP);
		Box(R, RX1 - 0.05, RX1 + 0.45, SWY0, SWY1, SWZ0 - 0.04, SWZ0, Wood, SmoothP, NoneP);                 // sill
		// the stair up at the far end, under the stairwell
		const int32 Steps = 13;
		const double Run = (CY1 - 0.2 - CST) / Steps, Rise = CH / Steps;
		for (int32 i = 0; i < Steps; ++i)
			Box(R, -CW, CW, CST + i * Run, CY1 - 0.2, 0, (i + 1) * Rise, i % 2 == 0 ? Wood : BoardsGrey, SmoothP, NoneP);
		Box(R, -CW, CW, CY1 - 0.2, CY1, 0, CH + 0.02, Wood, SmoothP, NoneP);                                  // landing
		for (const FItem& It : HallItems()) Stand(R, It, TEXT("oldtown"));
		GoldLamp(R, 0.0, 1.2, PendantLightUp, true);            // its pendant
		GoldLamp(R, -CW + 0.2, 2.4, TableLampUp + 0.1, false);  // the lamp on the hall table
		OwnLight(R, 0, (CY0 + CY1) / 2, CH / 2, ReachCm(RX1 + CW, CY1 - CY0), GoldFills);   // over the hallway's middle: the camera's room
		CardAt(R, -0.1, 1.6);
		Rooms.Add(R);
	}

	Boxes->AddInstances(Ats, false, true);
	for (int32 n = 0; n < Data.Num(); ++n) Boxes->SetCustomData(n, Data[n], false);
	Boxes->MarkRenderStateDirty();
	if (UStaticMesh* Sphere = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Sphere.Sphere")))
	{
		Balls = Group(TEXT("GreyBalls"), Sphere);
		Balls->AddInstances(BallAts, false, true);
		for (int32 n = 0; n < BallData.Num(); ++n) Balls->SetCustomData(n, BallData[n], false);
		Balls->MarkRenderStateDirty();
	}
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d reference rooms built %.0f m up (%s colours), %d pieces, %d lamps; their sun at %02d:%02d; the golden room and its hallway among them, a grey card and ball in each"),
		Rooms.Num(), RoomsAt.Z, bGameColours ? TEXT("the game's") : TEXT("the references'"), Pieces.Num(), Lamps.Num() + GoldLamps.Num(), (int32)RefMinute / 60, (int32)RefMinute % 60);
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
	// the golden rooms: lit as ABrixRooms lights a planned room now (PlaceLights), unless a stop tries other numbers
	for (UPointLightComponent* L : GoldLamps)
	{
		const float Level = GoldCandela * D->LampNow(Daylight) * LampScale;
		L->SetIntensity(Level);
		L->SetLightColor(Tint ? *Tint : D->RoomLampColour);
		L->SetVisibility(Level > 0.f);
	}
	for (UPointLightComponent* F : GoldFills)
	{
		const float Level = FillLevel < 0.f ? D->FillNow(Daylight) * D->FillRoomNow(true, Daylight) : FillLevel;
		F->SetIntensity(Level);
		F->SetLightColor(FillTint ? *FillTint : D->FillColourNow(Daylight));
		F->SetVisibility(Level > 0.f);
	}
}

bool ABrixLightRoom::Contains(const FVector& WorldCm) const
{
	for (const FOne& R : Rooms)
	{
		const FVector L = Turn.UnrotateVector(WorldCm - R.Origin) / 100.0;   // (x, -y, z) of the references' frame
		if (L.X > R.Lo.X && L.X < R.Hi.X && -L.Y > R.Lo.Y && -L.Y < R.Hi.Y && L.Z > R.Lo.Z && L.Z < R.Hi.Z) return true;
	}
	return false;
}

bool ABrixLightRoom::Camera(const FString& Room, FTransform& Out, float* FovOut) const
{
	for (const FOne& R : Rooms)
		if (R.Key == Room)
		{
			const FVector From = At(R, R.CamAt.X, R.CamAt.Y, R.CamAt.Z), To = At(R, R.CamLooksAt.X, R.CamLooksAt.Y, R.CamLooksAt.Z);
			Out = FTransform((To - From).Rotation(), From);
			if (FovOut) *FovOut = R.Fov;
			return true;
		}
	return false;
}

bool ABrixLightRoom::Card(const FString& Room, TArray<FVector>& Corners, FVector& Ball, float& BallRadius) const
{
	for (const FOne& R : Rooms)
		if (R.Key == Room && R.bCard)
		{
			Corners = { At(R, R.CardLo.X, R.CardLo.Y, R.CardLo.Z), At(R, R.CardHi.X, R.CardLo.Y, R.CardLo.Z), At(R, R.CardHi.X, R.CardLo.Y, R.CardHi.Z), At(R, R.CardLo.X, R.CardLo.Y, R.CardHi.Z) };
			Ball = At(R, R.BallAt.X, R.BallAt.Y, R.BallAt.Z);
			BallRadius = BallAcross * 50.f;
			return true;
		}
	return false;
}
