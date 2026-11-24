#include "BrixTownActor.h"

#include "BrixCity.h"
#include "BrixUnits.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

namespace
{
	// What remains of [A0, A1] once the cuts are taken out (kit spans_minus).
	TArray<FVector2f> SpansMinus(float A0, float A1, TArray<FVector2f> Cuts)
	{
		Cuts.Sort([](const FVector2f& P, const FVector2f& Q) { return P.X < Q.X; });
		TArray<FVector2f> Out;
		float Cur = A0;
		for (const FVector2f& C : Cuts)
		{
			if (C.Y <= Cur || C.X >= A1) continue;
			if (C.X > Cur) Out.Add(FVector2f(Cur, C.X));
			Cur = FMath::Max(Cur, C.Y);
		}
		if (Cur < A1) Out.Add(FVector2f(Cur, A1));
		return Out;
	}

	// Slots that take the instance's own colour; the rest are fixed (SlotColours).
	bool IsOwnSlot(const FName S) { return S == TEXT("frame") || S == TEXT("shutter") || S == TEXT("door") || S == TEXT("roof") || S == TEXT("stripe"); }
}

ABrixTownActor::ABrixTownActor()
{
	PrimaryActorTick.bCanEverTick = false;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CubeMesh(TEXT("/Engine/BasicShapes/Cube.Cube"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyboxMat(TEXT("/Game/Greybox/M_Greybox.M_Greybox"));   // Tools/unreal/make_greybox_material.py
	Cube = CubeMesh.Object;
	Cylinder = CylinderMesh.Object;
	Sphere = SphereMesh.Object;
	Greybox = GreyboxMat.Object;
}

UStaticMesh* ABrixTownActor::Kit(const TCHAR* Name, UStaticMesh* Fallback)
{
	if (TObjectPtr<UStaticMesh>* Found = KitMeshes.Find(Name)) return *Found;
	UStaticMesh* M = LoadObject<UStaticMesh>(nullptr, *FString::Printf(TEXT("/Game/Kit/%s.%s"), Name, Name));
	if (!M) UE_LOG(LogTemp, Warning, TEXT("Brix Town: kit piece %s is missing (run Tools/unreal/import_building_kit.py)"), Name);
	if (!M) M = Fallback;
	KitMeshes.Add(Name, M);
	return M;
}

UMaterialInstanceDynamic* ABrixTownActor::Fixed(const FLinearColor& C)
{
	const uint32 Key = GetTypeHash(C.ToFColor(false));
	if (TObjectPtr<UMaterialInstanceDynamic>* Found = Colours.Find(Key)) return *Found;
	UMaterialInstanceDynamic* Mat = UMaterialInstanceDynamic::Create(Greybox, this);
	Mat->SetVectorParameterValue(TEXT("Color"), C);
	Mat->SetScalarParameterValue(TEXT("InstanceColour"), 0.f);
	Colours.Add(Key, Mat);
	return Mat;
}

UMaterialInstanceDynamic* ABrixTownActor::Own()
{
	if (!OwnColour)
	{
		OwnColour = UMaterialInstanceDynamic::Create(Greybox, this);
		OwnColour->SetScalarParameterValue(TEXT("InstanceColour"), 1.f);
	}
	return OwnColour;
}

UInstancedStaticMeshComponent* ABrixTownActor::Group(UStaticMesh* Mesh, bool bCollide, float CullM)
{
	const FString Key = Mesh->GetName() + (bCollide ? TEXT("") : TEXT("_NoCollision"));
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Parts.Find(Key)) return *Found;
	UInstancedStaticMeshComponent* G = NewObject<UInstancedStaticMeshComponent>(this, *Key);
	G->SetupAttachment(RootComponent);
	G->SetStaticMesh(Mesh);
	const bool bPlain = !Mesh->GetName().StartsWith(TEXT("SM_Kit_")) || Mesh->GetName() == TEXT("SM_Kit_Box")
		|| Mesh->GetName() == TEXT("SM_Kit_Cylinder") || Mesh->GetName() == TEXT("SM_Kit_Sphere") || Mesh->GetName() == TEXT("SM_Kit_RoofWedge");
	G->NumCustomDataFloats = Mesh->GetName() == TEXT("SM_Kit_Box") || Mesh == Cube ? 12 : 3;   // the box: colour, pattern, its turn
	const TArray<FStaticMaterial>& Slots = Mesh->GetStaticMaterials();
	for (int32 i = 0; i < Slots.Num(); ++i)
	{
		const FName S = Slots[i].MaterialSlotName;
		if (bPlain || IsOwnSlot(S)) { G->SetMaterial(i, Own()); continue; }
		FLinearColor C = FLinearColor::Gray;
		for (const FBrixSlotColour& F : SlotColours) if (F.Slot == S) C = F.Colour;
		G->SetMaterial(i, Fixed(C));
	}
	G->SetCollisionProfileName(bCollide ? TEXT("BlockAll") : TEXT("NoCollision"));
	if (CullM > 0.f) G->SetCullDistances(0, (int32)(CullM * CmPerM));
	G->RegisterComponent();
	AddInstanceComponent(G);
	Parts.Add(Key, G);
	return G;
}

// A painted name: on a shop's fascia (kit text()), shrunk to fit the fascia
void ABrixTownActor::Name(const FString& Text, const FVector& At, double YawRad, float Size, float MaxWidth)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(RootComponent);
	T->SetText(FText::FromString(Text));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetTextRenderColor(LetterColour.ToFColor(true));
	const float Wide = Text.Len() * Size * 0.55f;            // about the engine font's width
	T->SetWorldSize(Size * FMath::Min(1.f, MaxWidth / FMath::Max(Wide, 0.01f)) * CmPerM);
	T->SetWorldLocationAndRotation(At, FQuat(FVector::UpVector, YawRad + HALF_PI));   // a text faces its +x: turned to face out
	T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	T->RegisterComponent();
	AddInstanceComponent(T);
}

void ABrixTownActor::Add(UInstancedStaticMeshComponent* G, const FTransform& At, const FLinearColor& C)
{
	FLook L;
	L.Colour = C;
	Add(G, At, L);
}

void ABrixTownActor::Add(UInstancedStaticMeshComponent* G, const FTransform& At, const FLook& Look)
{
	FPending& P = Pending.FindOrAdd(G);
	P.At.Add(At);
	P.Look.Add(Look);
}

// A surface's pattern (World/rules/surfaces.json, picked by BrixWorld as world.surfaces() does) as the material reads it
ABrixTownActor::FLook ABrixTownActor::Surface(const FBrixSurface& S, const FLinearColor& Town) const
{
	FLook L;
	L.Colour = S.Color.IsSet() ? FLinearColor(S.Color->X, S.Color->Y, S.Color->Z) : Town;
	if (!CityData || !CityData->Patterns.IsValidIndex(S.Pattern)) { L.A = FVector4f(-1, 0, 0, 0); return L; }
	const FBrixPattern& P = CityData->Patterns[S.Pattern];
	L.A = FVector4f((float)P.TypeIndex(), (float)P.Unit.X, (float)P.Unit.Y, P.Joint);
	L.B = FVector4f(P.Vary, P.Bevel, P.Dome, P.Mortar);
	return L;
}

void ABrixTownActor::BuildLot(const FBrixLot& L, const FBrixBlock& B)
{
	FRandomStream Rng((int32)GetTypeHash(L.Id));
	const float t = WallThickness;
	const int32 Floors = FMath::Max(1, L.Floors);
	// a designed house (lot.house; houses.js outsideS): its plan's size, windows and door, its line's trim, its colourway
	const FBrixHouseLook* Hs = L.House.IsSet() ? &L.House.GetValue() : nullptr;
	const FBrixHousePlan* Plan = Hs && CityData ? CityData->HousePlans.Find(Hs->Plan == TEXT("samples_19") ? FString(TEXT("house_12x9")) : Hs->Plan) : nullptr;
	const bool bHouse = Plan != nullptr;
	auto Lin = [](const FVector3f& V) { return FLinearColor(V.X, V.Y, V.Z); };
	const float S0 = HousePlinth, S1 = HousePlinth + HouseStorey;
	const float H = bHouse ? HousePlinth + 2 * HouseStorey : Floors * StoreyHeight;
	const bool bOld = B.District != TEXT("Outskirts");
	const bool bCivic = L.Kind == TEXT("health") || L.Kind == TEXT("work");
	const bool bFood = L.Kind == TEXT("food"), bShop = L.Kind == TEXT("shop");
	const bool bShutters = L.Kind == TEXT("home") || bFood || bShop;
	const float StoneTo = bHouse ? (Hs->bStone ? H : 0.f)
		: (bCivic || L.Surface.bUpper) ? H : (StoneBaseDistricts.Contains(B.District) ? FMath::Min(StoneBaseHeight, H) : 0.f);
	FBrixSurface HouseStone;                              // a stone line's walls: ashlar in the line's stone (outsideS)
	if (bHouse) { HouseStone.Pattern = CityData->FindPattern(TEXT("ashlar")); HouseStone.Color = Hs->Wall; }
	const FLook Stone = Surface(bHouse ? HouseStone : L.Surface, TownStone);   // the lot's wall pattern (ADR-0017)
	FLinearColor Plaster = PlasterColours.Num() ? PlasterColours[Rng.RandHelper(PlasterColours.Num())] : FLinearColor(0.57f, 0.53f, 0.475f);
	FLinearColor Frame = FrameColours.Num() ? FrameColours[Rng.RandHelper(FrameColours.Num())] : FLinearColor::White;
	FLinearColor Shutter = ShutterColours.Num() ? ShutterColours[Rng.RandHelper(ShutterColours.Num())] : FLinearColor::White;
	FLinearColor DoorPaint = DoorColours.Num() ? DoorColours[Rng.RandHelper(DoorColours.Num())] : FLinearColor::White;
	FLinearColor Roof = RoofColour;
	if (bHouse) { Plaster = Lin(Hs->Wall); Frame = Lin(Hs->Frame); Shutter = Lin(Hs->Shutter); DoorPaint = Lin(Hs->Door); Roof = Lin(Hs->Roof); }   // the colourway (ADR-0032)
	FLinearColor Dark = StoneColour * 0.7f;
	for (const FBrixSlotColour& F : SlotColours) if (F.Slot == TEXT("stone_dark")) Dark = F.Colour;

	UStaticMesh* BoxMesh = Kit(TEXT("SM_Kit_Box"), Cube);
	UInstancedStaticMeshComponent* Boxes = Group(BoxMesh, true);
	const FVector2D C = L.Centre(), S = bHouse ? FVector2D(2 * Plan->HalfU, 2 * Plan->HalfV) : L.Size();
	const double Ang = FMath::DegreesToRadians(L.AngleDeg), Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
	const float Base = BlockSlabHeight;
	auto At = [&](const FVector2D& Local, float Up) { return ToWorld(FVector2D(C.X + Local.X * Ca - Local.Y * Sa, C.Y + Local.X * Sa + Local.Y * Ca), Base + Up); };
	auto BoxLook = [&](const FLook& Look, const FVector2D& Local, float Z0, float Z1, double YawRad, float Len, float Dep)
	{
		Add(Boxes, FTransform(FQuat(FVector::UpVector, YawRad + Ang), At(Local, (Z0 + Z1) * 0.5f), FVector(Len, Dep, Z1 - Z0)), Look);
	};
	auto Box = [&](const FLinearColor& Col, const FVector2D& Local, float Z0, float Z1, double YawRad, float Len, float Dep)
	{
		FLook Look;
		Look.Colour = Col;
		BoxLook(Look, Local, Z0, Z1, YawRad, Len, Dep);
	};

	// the core, set back by the wall's depth all round (the kit's "core": what the reveals look into)
	if (StoneTo >= H) BoxLook(Stone, FVector2D::ZeroVector, 0.f, H, 0.0, S.X - 2 * t, S.Y - 2 * t);
	else Box(Plaster, FVector2D::ZeroVector, 0.f, H, 0.0, S.X - 2 * t, S.Y - 2 * t);

	// which facade has the door: the edge of the footprint nearest the lot's door
	const FVector2D DL((L.Door.X - C.X) * Ca + (L.Door.Y - C.Y) * Sa, -(L.Door.X - C.X) * Sa + (L.Door.Y - C.Y) * Ca);
	struct FFacade { FVector2D Mid, Normal; float Len; };
	const FFacade Facades[4] = {
		{ FVector2D(0, -S.Y / 2), FVector2D(0, -1), (float)S.X }, { FVector2D(0, S.Y / 2), FVector2D(0, 1), (float)S.X },
		{ FVector2D(-S.X / 2, 0), FVector2D(-1, 0), (float)S.Y - 2 * t }, { FVector2D(S.X / 2, 0), FVector2D(1, 0), (float)S.Y - 2 * t } };
	const double DoorGap[4] = { FMath::Abs(DL.Y + S.Y / 2), FMath::Abs(DL.Y - S.Y / 2), FMath::Abs(DL.X + S.X / 2), FMath::Abs(DL.X - S.X / 2) };
	int32 DoorSide = 0;
	for (int32 k = 1; k < 4; ++k) if (DoorGap[k] < DoorGap[DoorSide]) DoorSide = k;

	for (int32 k = 0; k < 4; ++k)
	{
		const FFacade& F = Facades[k];
		const double Yaw = FMath::Atan2(-F.Normal.X, F.Normal.Y);              // turns a piece's +y out of this wall
		const FVector2D Along(FMath::Cos(Yaw), FMath::Sin(Yaw));
		auto OnWall = [&](float Sx, float Out) { return F.Mid + Along * Sx + F.Normal * Out; };

		TArray<FOpening> Ops;
		// the street front of a shop or a cafe, as the kit's shopfront() and cafe_front() build it (two storeys and more:
		// the fascia and the awning reach above the ground floor)
		const bool bShopFront = !bHouse && bShop && k == DoorSide && Floors >= 2 && F.Len >= 4.f;
		const bool bCafeFront = !bHouse && bFood && bOld && k == DoorSide && Floors >= 2;
		float LastBay = F.Len / 2;
		TArray<FVector> FlowerBoxes;                     // a house's window boxes: (along, height, stretch)
		if (bHouse)
		{
			// the plan's windows on this face, both storeys, shutters open on each (outsideS); the front door on the front
			static const TCHAR* Sides[4] = { TEXT("front"), TEXT("back"), TEXT("left"), TEXT("right") };
			auto AlongOf = [&](const FVector2D& P) { return (float)FVector2D::DotProduct(P - F.Mid, Along); };
			for (const FBrixPlanWindow& W : Plan->Windows)
			{
				if (W.Side != Sides[k]) continue;
				const FVector2D P = k < 2 ? FVector2D(W.At, F.Mid.Y) : FVector2D(F.Mid.X, W.At);
				const float Sx = AlongOf(P), Stretch = W.Width / 1.1f;
				Ops.Add({ Sx, S0 + 0.95f, W.Width, 1.6f, Kit(TEXT("SM_Kit_WinUp")), Kit(TEXT("SM_Kit_Shutters160")), &Frame, -1.f, Stretch, 1.f });
				Ops.Add({ Sx, S1 + 0.8f, W.Width, 1.45f, Kit(TEXT("SM_Kit_WinTop")), Kit(TEXT("SM_Kit_Shutters145")), &Frame, -1.f, Stretch, 1.f });
				if (Hs->bBoxes && k != 1) FlowerBoxes.Add(FVector(Sx, S1 + 0.8f, Stretch));
			}
			if (k == 0)
			{
				const float Sx = AlongOf(FVector2D(Plan->DoorAt, F.Mid.Y));
				const bool bArch = Hs->bArches;
				Ops.Add({ Sx, S0, Plan->DoorWidth, 2.4f, Kit(bArch ? TEXT("SM_Kit_DoorArch") : TEXT("SM_Kit_DoorPlain")), nullptr, &DoorPaint, -1.f,
					Plan->DoorWidth / 1.3f, 2.4f / (bArch ? 2.5f : 2.3f) });
			}
		}
		else if (F.Len >= MinFacadeForOpenings)
		{
			const int32 Nb = FMath::Max(1, FMath::RoundToInt(F.Len / Bay));
			int32 DoorBay = -1;
			if (k == DoorSide)
			{
				const double Sd = FVector2D::DotProduct(DL - F.Mid, Along);
				double Best = 1e9;
				for (int32 i = 0; i < Nb; ++i) { const double Si = -F.Len / 2 + (i + 0.5) * F.Len / Nb; if (FMath::Abs(Si - Sd) < Best) { Best = FMath::Abs(Si - Sd); DoorBay = i; } }
			}
			for (int32 i = 0; i < Nb; ++i)
			{
				const float Si = -F.Len / 2 + (i + 0.5f) * F.Len / Nb;
				LastBay = Si;
				// ground storey, as the kit's building() picks it by kind and district
				if (bShopFront) {}                             // the shopfront below takes the whole ground storey
				else if (i == DoorBay)
				{
					if (bCivic) Ops.Add({ Si, 0.f, 1.5f, 2.6f, Kit(TEXT("SM_Kit_DoorCivic")), nullptr, &DoorPaint });
					else if (bOld) Ops.Add({ Si, 0.f, 1.3f, 2.5f, Kit(TEXT("SM_Kit_DoorArch")), nullptr, &DoorPaint });
					else Ops.Add({ Si, 0.f, 1.3f, 2.3f, Kit(TEXT("SM_Kit_DoorPlain")), nullptr, &DoorPaint });
				}
				else if (bCivic) Ops.Add({ Si, CivicSill, 1.25f, 1.65f, Kit(TEXT("SM_Kit_WinCivic")), nullptr, &Frame });
				else if ((bFood || bShop) && bOld) Ops.Add({ Si, CafeSill, 2.0f, 2.45f, Kit(TEXT("SM_Kit_WinCafe")), nullptr, &Frame });
				else if (bOld) Ops.Add({ Si, GroundSill, 1.1f, 1.75f, Kit(TEXT("SM_Kit_WinArch")), nullptr, &Frame });
				else Ops.Add({ Si, GroundSill, 1.1f, 1.6f, Kit(TEXT("SM_Kit_WinUp")), bShutters && Rng.FRand() < ShutterShare ? Kit(TEXT("SM_Kit_Shutters160")) : nullptr, &Frame });
				// upper storeys; the top one has the kit's lower windows
				for (int32 f = 1; f < Floors; ++f)
				{
					const bool bTop = f == Floors - 1;
					if (bCafeFront && f == 1 && i == Nb / 2 && Nb >= 3)     // a French window on a balcony (kit balcony())
					{
						Ops.Add({ Si, f * StoreyHeight + 0.15f, 1.2f, 2.3f, Kit(TEXT("SM_Kit_Balcony")), nullptr, &Frame, f * StoreyHeight });
						continue;
					}
					UStaticMesh* W = bTop ? Kit(TEXT("SM_Kit_WinTop")) : (bOld ? Kit(TEXT("SM_Kit_WinUp")) : Kit(TEXT("SM_Kit_WinPlain")));
					UStaticMesh* Sh = bShutters && Rng.FRand() < ShutterShare ? Kit(bTop ? TEXT("SM_Kit_Shutters145") : TEXT("SM_Kit_Shutters160")) : nullptr;
					Ops.Add({ Si, f * StoreyHeight + UpperSill, 1.1f, bTop ? 1.45f : 1.6f, W, Sh, &Frame });
				}
			}
		}

		const FQuat Face(FVector::UpVector, Yaw + Ang);
		auto Piece = [&](const TCHAR* Mesh, float Sx, float Z, const FVector& Scale, const FLinearColor& Col, float CullM)
		{
			if (UStaticMesh* M = Kit(Mesh)) Add(Group(M, false, CullM), FTransform(Face, At(OnWall(Sx, 0.f), Z), Scale), Col);
		};
		auto Awning = [&](float S0, float S1, float Z, float Out, int32 FirstColour)
		{
			if (S1 - S0 < 1.f || !AwningColours.Num()) return;
			const int32 N = FMath::Max(4, (int32)((S1 - S0) / AwningStrip));
			const float Wd = (S1 - S0) / N, Reach = Out / AwningOut;
			for (int32 i = 0; i < N; ++i)
				Piece(TEXT("SM_Kit_AwningStrip"), S0 + (i + 0.5f) * Wd, Z, FVector(Wd / AwningStrip, Reach, Reach), AwningColours[(i + FirstColour) % AwningColours.Num()], OpeningCullDistance);
			for (float Sx : { S0 + 0.1f, S1 - 0.1f }) Piece(TEXT("SM_Kit_AwningArm"), Sx, Z, FVector(1, Reach, Reach), FLinearColor::Black, ShutterCullDistance);
		};
		if (bShopFront)
		{
			// kit shopfront(): one opening across the ground storey, piers every ShopPane metres, panes, the door in the
			// middle, the fascia with the shop's name, a striped awning, crates of produce on the pavement
			const float X0 = -F.Len / 2, X1 = F.Len / 2, Wd = F.Len;
			Ops.Add({ 0.f, 0.f, Wd - 0.8f, ShopTop, nullptr, nullptr, &Frame });
			const int32 N = FMath::Max(3, FMath::RoundToInt((Wd - 0.8f) / ShopPane));
			for (int32 i = 0; i <= N; ++i) Piece(TEXT("SM_Kit_ShopPier"), X0 + 0.4f + i * (Wd - 0.8f) / N, 0.f, FVector::OneVector, FLinearColor::Gray, OpeningCullDistance);
			for (int32 i = 0; i < N; ++i)
			{
				const float A = X0 + 0.4f + i * (Wd - 0.8f) / N + ShopPier / 2, B2 = X0 + 0.4f + (i + 1) * (Wd - 0.8f) / N - ShopPier / 2;
				Piece(i == N / 2 ? TEXT("SM_Kit_ShopDoor") : TEXT("SM_Kit_ShopPane"), (A + B2) / 2, 0.f, FVector(B2 - A, 1, 1), FLinearColor::Gray, OpeningCullDistance);
			}
			Piece(TEXT("SM_Kit_ShopFascia"), 0.f, 0.f, FVector(Wd - 0.5f, 1, 1), FLinearColor::Gray, OpeningCullDistance);
			Name(L.Name, At(OnWall(0.f, 0.25f), (2.55f + ShopTop) / 2 + 0.02f), Yaw + Ang, NameSize, Wd - 1.2f);
			Awning(X0 + 0.6f, X1 - 0.6f, AwningZ, AwningOut, 0);
			for (int32 i = 0; i < CrateAt.Num(); ++i)
			{
				const float Sx = i < CrateAt.Num() / 2 ? X0 + CrateAt[i] : X1 - CrateAt[i];
				if (Sx > X0 + 0.5f && Sx < X1 - 0.5f) Piece(TEXT("SM_Kit_ProduceCrate"), Sx, 0.f, FVector::OneVector, FLinearColor::Gray, ShutterCullDistance);
			}
		}
		if (bCafeFront)
		{
			// kit cafe_front(): an awning over the arches, a blade sign at the corner, tables on the pavement
			Awning(-F.Len / 2 + 0.3f, LastBay - 0.9f, CafeAwningZ, CafeAwningOut, 1);
			Piece(TEXT("SM_Kit_BladeSign"), F.Len / 2 - 0.35f, 0.f, FVector::OneVector, FLinearColor::Gray, OpeningCullDistance);
			for (float T : CafeTableAt) if (-F.Len / 2 + T < F.Len / 2 - 1.f) Piece(TEXT("SM_Kit_CafeTable"), -F.Len / 2 + T, 0.f, FVector::OneVector, FLinearColor::Gray, ShutterCullDistance);
		}

		for (const FVector& Fb : FlowerBoxes) Piece(TEXT("SM_Kit_WindowBox"), Fb.X, Fb.Y, FVector(Fb.Z, 1, 1), FLinearColor::Gray, ShutterCullDistance);
		if (bHouse)
		{
			// a house's plinth and its string course between the storeys, in the light stone of its sills (outsideS)
			Box(Dark, OnWall(0.f, 0.03f), 0.f, S0 + 0.3f, Yaw, F.Len + 0.1f, 0.06f);
			Box(HouseTrimStone, OnWall(0.f, 0.06f), S1 - 0.1f, S1 + 0.15f, Yaw, F.Len + 0.18f, 0.12f);
		}

		// the pieces
		for (const FOpening& O : Ops)
		{
			if (!O.Mesh) continue;
			const FTransform Tr(FQuat(FVector::UpVector, Yaw + Ang), At(OnWall(O.S, 0.f), O.MeshZ >= 0.f ? O.MeshZ : O.Z0), FVector(O.SX, 1, O.SZ));
			Add(Group(O.Mesh, false, OpeningCullDistance), Tr, *O.Paint);
			if (O.Shutters) Add(Group(O.Shutters, false, ShutterCullDistance), FTransform(Tr.GetRotation(), Tr.GetLocation(), FVector(O.SX, 1, 1)), Shutter);
			++Openings;
		}

		// the wall between the openings (kit masonry and plaster_wall): bands from one opening's edge to the next, and a
		// piece that runs on unchanged through the next band is made one taller piece (a pier between two windows is one
		// box from the ground to the eaves, not one per storey)
		TArray<float> Breaks = { 0.f, H };
		if (StoneTo > 0.f && StoneTo < H) Breaks.Add(StoneTo);
		for (const FOpening& O : Ops) { Breaks.Add(O.Z0); Breaks.Add(FMath::Min(O.Z0 + O.H, H)); }
		Breaks.Sort();
		struct FRun { FVector2f Span; float Z0, Z1; bool bStone; };
		TArray<FRun> Open;
		auto Close = [&](const FRun& R)
		{
			if (R.bStone) BoxLook(Stone, OnWall((R.Span.X + R.Span.Y) / 2, -t / 2), R.Z0, R.Z1, Yaw, R.Span.Y - R.Span.X, t);
			else Box(Plaster, OnWall((R.Span.X + R.Span.Y) / 2, -t / 2), R.Z0, R.Z1, Yaw, R.Span.Y - R.Span.X, t);
		};
		for (int32 b = 0; b + 1 < Breaks.Num(); ++b)
		{
			const float Za = Breaks[b], Zb = Breaks[b + 1];
			if (Zb - Za < 1e-3f) continue;
			TArray<FVector2f> Cuts;
			for (const FOpening& O : Ops) if (O.Z0 < Zb - 1e-3f && O.Z0 + O.H > Za + 1e-3f) Cuts.Add(FVector2f(O.S - O.W / 2, O.S + O.W / 2));
			const bool bStone = (Za + Zb) * 0.5f < StoneTo;
			TArray<FRun> Next;
			for (const FVector2f& Sp : SpansMinus(-F.Len / 2, F.Len / 2, Cuts))
			{
				const int32 Same = Open.IndexOfByPredicate([&](const FRun& R) { return R.bStone == bStone && FMath::Abs(R.Span.X - Sp.X) < 1e-3f && FMath::Abs(R.Span.Y - Sp.Y) < 1e-3f && FMath::Abs(R.Z1 - Za) < 1e-3f; });
				if (Same != INDEX_NONE) { FRun R = Open[Same]; R.Z1 = Zb; Next.Add(R); Open.RemoveAtSwap(Same); }
				else Next.Add({ Sp, Za, Zb, bStone });
			}
			for (const FRun& R : Open) Close(R);
			Open = MoveTemp(Next);
		}
		for (const FRun& R : Open) Close(R);
		// the string course over a stone ground storey
		if (!bHouse && StoneTo > 0.f && StoneTo < H - 0.5f) Box(Dark, OnWall(0.f, BandDepth / 2), StoneTo, StoneTo + BandHeight, Yaw, F.Len + 2 * BandDepth, BandDepth);
	}

	// the roof: tiles on homes and shops, and on everything outside the old districts; a parapet on the rest (kit)
	const bool bTiles = L.Kind == TEXT("home") || bShop || !bOld;
	if (bTiles)
	{
		const bool bAlongX = S.X >= S.Y;
		const float Lr = bAlongX ? S.X : S.Y, D = bAlongX ? S.Y : S.X;
		const double RYaw = bAlongX ? 0.0 : HALF_PI;
		const float P = FMath::DegreesToRadians(RoofPitch), Rise = D / 2 * FMath::Tan(P);
		// the gables: a wedge in the walls' plaster (Unreal's wedge runs y -1..0, so it is moved half its depth)
		const FVector2D Yax(-FMath::Sin(RYaw), FMath::Cos(RYaw));
		if (UStaticMesh* Wedge = Kit(TEXT("SM_Kit_RoofWedge")))
			Add(Group(Wedge, true), FTransform(FQuat(FVector::UpVector, RYaw + Ang), At(Yax * (D / 2), H), FVector(Lr, D, Rise)), Plaster);
		// the two slopes, from the ridge out over the eaves
		const float Half = D / 2 + Eave, Ls = Half / FMath::Cos(P);
		for (int32 Side : { -1, 1 })
		{
			const FVector2D Mid = Yax * (Side * Half / 2);
			const float Zc = H + Rise - Half / 2 * FMath::Tan(P) + RoofThickness / 2 / FMath::Cos(P);
			const FQuat Q = FQuat(FVector::UpVector, RYaw + Ang) * FQuat(FVector::ForwardVector, -Side * P);
			Add(Boxes, FTransform(Q, At(Mid, Zc), FVector(Lr + 2 * RoofSideOverhang, Ls, RoofThickness)), Roof);
		}
		if (bHouse)
		{
			// the chimney on the left gable with two pots (outsideS), and quoins up the corners on a line that has them
			const float Top = H + (D / 2 + Eave) * FMath::Tan(P) + 0.9f, Hu = Plan->HalfU, Hv = Plan->HalfV;
			if (Hs->bStone) BoxLook(Stone, FVector2D(-Hu + 0.3f, -1.8f), H - 0.5f, Top, 0.0, 0.7f, 0.6f);
			else Box(Dark, FVector2D(-Hu + 0.3f, -1.8f), H - 0.5f, Top, 0.0, 0.7f, 0.6f);
			Box(Dark, FVector2D(-Hu + 0.3f, -1.8f), Top, Top + 0.12f, 0.0, 0.9f, 0.8f);
			if (UStaticMesh* Pot = Kit(TEXT("SM_Kit_Cylinder"), Cylinder))
				for (float Dv : { -0.17f, 0.17f })
					Add(Group(Pot, true), FTransform(FQuat::Identity, At(FVector2D(-Hu + 0.3f, -1.8f + Dv), Top + 0.12f + 0.225f), FVector(0.18f, 0.18f, 0.45f)), RoofColour);
			if (Hs->bQuoins)
				for (const FVector2D Cn : { FVector2D(-Hu, -Hv), FVector2D(Hu, -Hv), FVector2D(-Hu, Hv), FVector2D(Hu, Hv) })
				{
					const float Su = FMath::Sign(Cn.X), Sv = FMath::Sign(Cn.Y);
					for (int32 q = 0; S0 + 0.3f + q * 0.5f + 0.45f < H; ++q)
					{
						const float Z0 = S0 + 0.3f + q * 0.5f, La = q % 2 ? 0.3f : 0.55f, Lb = q % 2 ? 0.55f : 0.3f;
						Box(QuoinStone, FVector2D(Cn.X - Su * La / 2 + Su * 0.02f, Cn.Y + Sv * 0.02f), Z0, Z0 + 0.45f, 0.0, La + 0.04f, 0.04f);
						Box(QuoinStone, FVector2D(Cn.X + Su * 0.02f, Cn.Y - Sv * Lb / 2 + Sv * 0.02f), Z0, Z0 + 0.45f, 0.0, 0.04f, Lb + 0.04f);
					}
				}
		}
	}
	else
	{
		FLook Top;
		if (StoneTo >= H) Top = Stone; else Top.Colour = Plaster;
		BoxLook(Top, FVector2D(0, -S.Y / 2 + t / 2), H, H + ParapetHeight, 0.0, S.X, t);
		BoxLook(Top, FVector2D(0, S.Y / 2 - t / 2), H, H + ParapetHeight, 0.0, S.X, t);
		BoxLook(Top, FVector2D(-S.X / 2 + t / 2, 0), H, H + ParapetHeight, 0.0, t, S.Y - 2 * t);
		BoxLook(Top, FVector2D(S.X / 2 - t / 2, 0), H, H + ParapetHeight, 0.0, t, S.Y - 2 * t);
		// the coping is a full slab: the roof inside the parapet is not seen from the street
		Box(StoneColour * 0.8f, FVector2D::ZeroVector, H + ParapetHeight, H + ParapetHeight + CopingHeight, 0.0, S.X + 0.12f, S.Y + 0.12f);
	}
	++Buildings;
}

void ABrixTownActor::Build(const FBrixCity& City)
{
	CityData = &City;
	UInstancedStaticMeshComponent* Boxes = Group(Kit(TEXT("SM_Kit_Box"), Cube), true);
	UInstancedStaticMeshComponent* Trunks = Group(Kit(TEXT("SM_Kit_Cylinder"), Cylinder), true);
	UInstancedStaticMeshComponent* Crowns = Group(Kit(TEXT("SM_Kit_Sphere"), Sphere), false);
	const FVector2D MapSize(City.Columns * City.Pitch, City.Rows * City.Pitch);
	FLook Road;   // the streets: setts in courses (slice material 20)
	Road.Colour = RoadColour;
	Road.A = FVector4f(RoadPatternA);
	Road.B = FVector4f(RoadPatternB);
	Add(Boxes, FTransform(FQuat::Identity, ToWorld(MapSize * 0.5, -0.5f), FVector(MapSize.X, MapSize.Y, 1.f)), Road);
	for (const FBrixBlock& B : City.Blocks)
	{
		const bool bGreen = GreenTemplates.Contains(B.Template);
		const FTransform Slab(FQuat::Identity, ToWorld(B.Origin + B.Extent * 0.5, BlockSlabHeight * 0.5f), FVector(B.Extent.X, B.Extent.Y, BlockSlabHeight));
		if (bGreen) Add(Boxes, Slab, GreenColour);
		else Add(Boxes, Slab, Surface(B.Pavement, TownPave));   // the block's paving (ADR-0017)
		for (const FBrixLot& L : B.Lots)
		{
			if (L.IsOpen())
			{
				const FTransform Top(FQuat(FVector::UpVector, FMath::DegreesToRadians(L.AngleDeg)), ToWorld(L.Centre(), BlockSlabHeight + 0.02f), FVector(L.Size().X, L.Size().Y, 0.04f));
				if (L.Kind == TEXT("garden")) Add(Boxes, Top, GreenColour);
				else Add(Boxes, Top, Surface(L.Surface, TownPave));   // the market's setts, a tram stop's slabs
				continue;
			}
			BuildLot(L, B);
		}
		for (const FBrixTree& T : B.Trees)
		{
			Add(Trunks, FTransform(FQuat::Identity, ToWorld(T.Pos, TreeTrunkHeight * 0.5f), FVector(TreeTrunkWidth, TreeTrunkWidth, TreeTrunkHeight)), TrunkColour);
			Add(Crowns, FTransform(FQuat::Identity, ToWorld(T.Pos, TreeTrunkHeight + TreeCrownWidth * 0.4f), FVector(TreeCrownWidth)), CrownColour);
		}
	}
	// Scales are in metres: the shapes and the kit's pieces are 1 m to a unit of scale, so a scale is a size.
	int32 Instances = 0;
	for (TPair<UInstancedStaticMeshComponent*, FPending>& P : Pending)
	{
		UInstancedStaticMeshComponent* G = P.Key;
		const int32 First = G->GetInstanceCount();
		G->AddInstances(P.Value.At, false);
		const bool bPattern = G->NumCustomDataFloats >= 12;
		for (int32 i = 0; i < P.Value.Look.Num(); ++i)
		{
			const FLook& L = P.Value.Look[i];
			const float Turn = FMath::DegreesToRadians((float)P.Value.At[i].GetRotation().Rotator().Yaw);   // the courses run along the box
			if (bPattern) G->SetCustomData(First + i, { L.Colour.R, L.Colour.G, L.Colour.B, L.A.X, L.A.Y, L.A.Z, L.A.W, L.B.X, L.B.Y, L.B.Z, L.B.W, Turn }, false);
			else G->SetCustomData(First + i, { L.Colour.R, L.Colour.G, L.Colour.B }, false);
		}
		G->MarkRenderStateDirty();
		Instances += P.Value.At.Num();
	}
	Pending.Empty();
	CityData = nullptr;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: built %s from data: %d blocks, %d buildings, %d openings, %d instances in %d groups"),
		*City.Name, City.Blocks.Num(), Buildings, Openings, Instances, Parts.Num());
}
