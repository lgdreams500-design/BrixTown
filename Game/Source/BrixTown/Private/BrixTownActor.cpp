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

	// The slice's hash() (play.js): the same strings give the same number in 0..1, so a garden gets the same extras here
	// as in the browser slice.
	double SliceHash(std::initializer_list<FString> Parts)
	{
		uint32 h = 2166136261u;
		for (const FString& S : Parts)
		{
			for (TCHAR Ch : S) { h ^= (uint32)Ch; h *= 16777619u; }
			h ^= 124u; h *= 16777619u;
		}
		h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
		return (double)h / 4294967296.0;
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
void ABrixTownActor::Name(const FString& Text, const FVector& At, double YawRad, float Size, float MaxWidth, bool bFaceAlong)
{
	UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
	T->SetupAttachment(RootComponent);
	T->SetText(FText::FromString(Text));
	T->SetHorizontalAlignment(EHTA_Center);
	T->SetVerticalAlignment(EVRTA_TextCenter);
	T->SetTextRenderColor(LetterColour.ToFColor(true));
	const float Wide = Text.Len() * Size * 0.55f;            // about the engine font's width
	T->SetWorldSize(Size * FMath::Min(1.f, MaxWidth / FMath::Max(Wide, 0.01f)) * CmPerM);
	// a text faces its +x: turned to face out of the wall, or along it (a blade sign's faces)
	T->SetWorldLocationAndRotation(At, FQuat(FVector::UpVector, YawRad + (bFaceAlong ? 0.0 : HALF_PI)));
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
			const float Sx = F.Len / 2 - 0.35f;
			Piece(TEXT("SM_Kit_BladeSign"), Sx, 0.f, FVector::OneVector, FLinearColor::Gray, OpeningCullDistance);
			for (int32 Side : { -1, 1 })                    // the cafe's name on both faces of the board (kit text(), 0.17 m)
				Name(L.Name, At(OnWall(Sx + Side * 0.06f, 0.7f), 3.45f), Yaw + Ang + (Side < 0 ? PI : 0.0), BladeNameSize, 0.9f, true);
			for (float T : CafeTableAt) if (-F.Len / 2 + T < F.Len / 2 - 1.f) Piece(TEXT("SM_Kit_CafeTable"), -F.Len / 2 + T, 0.f, FVector::OneVector, FLinearColor::Gray, ShutterCullDistance);
		}

		for (const FVector& Fb : FlowerBoxes) Piece(TEXT("SM_Kit_WindowBox"), Fb.X, Fb.Y, FVector(Fb.Z, 1, 1), FLinearColor::Gray, ShutterCullDistance);
		if (!bHouse && k == DoorSide && F.Len >= MinFacadeForOpenings)
		{
			// kit drainpipe(): an iron pipe down the front's end, its hopper under the eaves, its shoe, a bracket every 1.9 m
			const float Sx = F.Len / 2 - 0.2f;
			if (UStaticMesh* Pipe = Kit(TEXT("SM_Kit_Cylinder"), Cylinder))
				Add(Group(Pipe, true), FTransform(Face, At(OnWall(Sx, 0.14f), H / 2), FVector(0.11f, 0.11f, H - 0.3f)), IronColour);
			Box(IronColour, OnWall(Sx, 0.15f), H - 0.4f, H - 0.12f, Yaw, 0.26f, 0.3f);
			Box(IronColour, OnWall(Sx, 0.24f), 0.f, 0.18f, Yaw, 0.16f, 0.36f);
			for (float Z = 0.9f; Z < H - 0.6f; Z += 1.9f) Box(IronColour, OnWall(Sx, 0.09f), Z, Z + 0.05f, Yaw, 0.16f, 0.22f);
		}
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
		if (!bHouse)
		{
			// kit tile_roof(): a stack in the gable's plaster from the eaves to 1.1 m over the ridge, a stone cap, two pots;
			// one at 70 % of the ridge in the old districts, two near the ends elsewhere
			const FVector2D Xax(FMath::Cos(RYaw), FMath::Sin(RYaw));
			const float Back = bOld ? ChimneyBack : ChimneyBack + 0.2f, Ridge = H + Rise;
			TArray<float> Along;
			if (bOld) Along.Add(-Lr / 2 + Lr * ChimneyAt); else { Along.Add(-Lr / 2 + ChimneyEnds); Along.Add(Lr / 2 - ChimneyEnds); }
			for (float A : Along)
			{
				const FVector2D Cn = Xax * A + Yax * Back;
				Box(Plaster, Cn, H, Ridge + 1.1f, RYaw, 0.76f, 0.64f);
				Box(Dark, Cn, Ridge + 1.1f, Ridge + 1.26f, RYaw, 0.96f, 0.84f);
				if (UStaticMesh* Pot = Kit(TEXT("SM_Kit_Cylinder"), Cylinder))
					for (float Dx : { -0.17f, 0.17f })
						Add(Group(Pot, true), FTransform(FQuat::Identity, At(Cn + Xax * Dx, Ridge + 1.26f + 0.24f), FVector(0.19f, 0.19f, 0.48f)), RoofColour);
			}
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
			BuildGarden(L, *Plan, *Hs, H);
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

// The garden round a designed house (houses.js gardenS(); owner 2026-11-23): a lawn fenced in the line's way with a gate
// in line with the front door, a stone path, planters at the door, a letterbox and a tree; and, by the slice's hash of the
// lot, some of a bench, a bistro table with a parasol, a lounger, a washing line, a flowerbed, a woodpile, a shed. Laid in
// the house's own frame (u across the front, v from front to back, metres from its centre), as the slice lays it.
void ABrixTownActor::BuildGarden(const FBrixLot& L, const FBrixHousePlan& Plan, const FBrixHouseLook& Hs, float HouseTop)
{
	const FVector2D C = L.Centre();
	const double Ang = FMath::DegreesToRadians(L.AngleDeg), Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
	auto At = [&](float U, float V, float Up) { return ToWorld(FVector2D(C.X + U * Ca - V * Sa, C.Y + U * Sa + V * Ca), BlockSlabHeight + Up); };
	UStaticMesh* BoxMesh = Kit(TEXT("SM_Kit_Box"), Cube);
	UInstancedStaticMeshComponent* Solid = Group(BoxMesh, true);
	UInstancedStaticMeshComponent* Fine = Group(BoxMesh, false, GardenCullDistance);
	auto Lin = [](float R, float G, float Bl) { return FLinearColor(R, G, Bl); };
	const FLinearColor Cream = Lin(.66f, .61f, .50f), Stone = Lin(.52f, .48f, .43f), StoneDark = Lin(.37f, .34f, .32f), Charcoal = Lin(.09f, .09f, .11f);
	const FLinearColor Wood = Lin(.34f, .23f, .15f), Brown = Lin(.24f, .16f, .11f), Terra = Lin(.44f, .24f, .18f), Teal = Lin(.20f, .36f, .37f);
	const FLinearColor Blue = Lin(.30f, .38f, .52f), Plum = Lin(.30f, .24f, .34f), Ochre = Lin(.52f, .40f, .20f), Soil = Lin(.2f, .15f, .11f), Path = Lin(.48f, .44f, .38f);
	const FLinearColor Leaf = Lin(.26f, .38f, .20f), LeafLight = Lin(.40f, .48f, .24f), LeafDark = Lin(.24f, .36f, .2f), Lawn = Lin(.29f, .34f, .19f);
	const FLinearColor Door(Hs.Door.X, Hs.Door.Y, Hs.Door.Z), Shut(Hs.Shutter.X, Hs.Shutter.Y, Hs.Shutter.Z), RoofC(Hs.Roof.X, Hs.Roof.Y, Hs.Roof.Z);
	// B: a box from corner to corner in the house's frame (bx1); Y: a box of a size on the ground at a point, turned (yb1)
	auto B = [&](bool bFine, const FLinearColor& Col, float U0, float Y0, float V0, float U1, float Y1, float V1)
	{
		Add(bFine ? Fine : Solid, FTransform(FQuat(FVector::UpVector, Ang), At((U0 + U1) / 2, (V0 + V1) / 2, (Y0 + Y1) / 2),
			FVector(FMath::Abs(U1 - U0), FMath::Abs(V1 - V0), FMath::Abs(Y1 - Y0))), Col);
	};
	auto Y = [&](bool bFine, const FLinearColor& Col, float U, float Y0, float V, float Su, float Sy, float Sv, float Yaw = 0.f)
	{
		Add(bFine ? Fine : Solid, FTransform(FQuat(FVector::UpVector, Ang + Yaw), At(U, V, Y0 + Sy / 2), FVector(Su, Sv, Sy)), Col);
	};
	auto Has = [&](const TCHAR* K) { return SliceHash({ L.Id, TEXT("garden"), K }) < 0.5; };

	const float HU = Plan.HalfU, HV = Plan.HalfV, S0 = HousePlinth, Du = Plan.DoorAt;
	const float GL = Hs.Garden.X, GF = Hs.Garden.Y, GR = Hs.Garden.Z, GB = Hs.Garden.W;
	const float Front = -HV - GF, Back = GB - HV, Right = GR - HU;
	const FString& Line = Hs.Line;
	// the lawn, a shade fresher than the common ground, and the path of stone slabs from the gate to the door step
	B(false, Lawn, GL, 0.f, GF, GR, 0.03f, GB);
	int32 k = 0;
	for (float V = GF + 0.2f; V < -HV - 0.7f; V += 0.75f, ++k) B(false, Path, Du - 0.55f + (k % 2) * 0.08f, 0.03f, V, Du + 0.55f + (k % 2) * 0.08f, 0.06f, V + 0.6f);
	// the fence, in the line's way, with a gate gap in front of the door
	const float Gate0 = Du - 0.75f, Gate1 = Du + 0.75f;
	struct FRun { bool bU; float At, A0, A1; };
	const FRun Runs[5] = { { true, GF, GL, Gate0 }, { true, GF, Gate1, GR }, { true, GB, GL, GR }, { false, GL, GF, GB }, { false, GR, GF, GB } };
	for (const FRun& R : Runs)
	{
		auto Seg = [&](bool bFine, const FLinearColor& Col, float P, float Q, float Y0, float Y1, float D)
		{
			if (R.bU) B(bFine, Col, P, Y0, R.At - D / 2, Q, Y1, R.At + D / 2);
			else B(bFine, Col, R.At - D / 2, Y0, P, R.At + D / 2, Y1, Q);
		};
		auto Post = [&](bool bFine, const FLinearColor& Col, float P, float Y0, float Y1, float D) { Seg(bFine, Col, P - D / 2, P + D / 2, Y0, Y1, D); };
		const float Len = R.A1 - R.A0;
		const int32 N = FMath::Max(1, FMath::RoundToInt(Len / 2.2f));
		for (int32 i = 0; i < N; ++i)
		{
			const float A0 = R.A0 + Len * i / N, A1 = R.A0 + Len * (i + 1) / N;
			if (Line == TEXT("plain"))                     // a cream picket fence: posts, two rails, pickets
			{
				Post(false, Cream, A0, 0.f, 1.0f, 0.1f);
				if (i == N - 1) Post(false, Cream, A1, 0.f, 1.0f, 0.1f);
				for (float Yr : { 0.25f, 0.75f }) Seg(true, Cream, A0, A1, Yr, Yr + 0.07f, 0.05f);
				for (float P = A0 + 0.12f; P < A1 - 0.05f; P += 0.3f) Post(true, Cream * 0.95f, P, 0.f, 0.92f, 0.09f);
			}
			else if (Line == TEXT("oldtown"))              // a low wall of dressed stone with a cap
			{
				Seg(false, Stone, A0, A1, 0.f, 0.8f, 0.32f);
				Seg(false, StoneDark, A0 - 0.02f, A1 + 0.02f, 0.8f, 0.92f, 0.42f);
			}
			else if (Line == TEXT("workshop"))             // iron railings on a stone plinth
			{
				Seg(false, StoneDark, A0, A1, 0.f, 0.35f, 0.3f);
				Seg(true, Charcoal, A0, A1, 0.95f, 1.0f, 0.05f);
				for (float P = A0 + 0.1f; P < A1; P += 0.26f) Post(true, Charcoal, P, 0.35f, 1.0f, 0.035f);
				Post(false, Charcoal, A0, 0.35f, 1.12f, 0.08f);
				if (i == N - 1) Post(false, Charcoal, A1, 0.35f, 1.12f, 0.08f);
			}
			else                                           // a clipped hedge, rounded on top in two steps
			{
				Seg(false, Lin(.22f, .32f, .15f), A0, A1, 0.f, 0.95f, 0.7f);
				Seg(false, Lin(.26f, .36f, .17f), A0 + 0.05f, A1 - 0.05f, 0.95f, 1.1f, 0.55f);
			}
		}
	}
	for (float G : { Gate0, Gate1 })                       // gate piers (or posts)
	{
		if (Line == TEXT("oldtown")) { B(false, StoneDark, G - 0.2f, 0.f, GF - 0.2f, G + 0.2f, 1.15f, GF + 0.2f); Y(false, Stone, G, 1.15f, GF, 0.28f, 0.26f, 0.28f); }
		else B(false, Line == TEXT("plain") ? Cream : Line == TEXT("curve") ? Wood : Charcoal, G - 0.07f, 0.f, GF - 0.07f, G + 0.07f, 1.2f, GF + 0.07f);
	}
	B(true, Line == TEXT("workshop") ? Charcoal : Line == TEXT("plain") ? Cream : Wood, Gate0 + 0.05f, 0.08f, GF + 0.05f, Gate0 + 0.11f, 0.95f, GF + 0.75f);   // the gate, open
	// the letterbox on a post by the gate, planters each side of the door, a tree in a front corner
	Y(true, Wood, Gate1 + 0.55f, 0.f, GF + 0.3f, 0.08f, 1.0f, 0.08f);
	Y(true, Door, Gate1 + 0.55f, 1.0f, GF + 0.3f, 0.4f, 0.28f, 0.3f);
	Y(true, Cream, Gate1 + 0.55f, 1.12f, GF + 0.14f, 0.26f, 0.14f, 0.04f);
	for (float Sd : { -1.f, 1.f }) { const float U = Du + Sd * 1.2f; Y(false, Terra, U, 0.f, -HV - 0.45f, 0.5f, 0.45f, 0.5f); Y(true, Leaf, U, 0.45f, -HV - 0.45f, 0.5f, 0.35f, 0.5f); }
	const bool bTreeRight = Has(TEXT("treeSide"));
	const float Tu = bTreeRight ? GR - 2.2f : GL + 2.2f, Tv = GF + 2.6f;
	if (Front > 3.5f)
	{
		Y(false, Wood, Tu, 0.f, Tv, 0.26f, 2.4f, 0.26f);
		for (int32 q = 0; q < 5; ++q)
			Y(false, q % 2 ? LeafDark : LeafLight, Tu + FMath::Cos(q * 1.3f) * 0.7f, 2.2f + (q % 3) * 0.4f, Tv + FMath::Sin(q * 1.3f) * 0.7f, 1.6f, 1.3f, 1.6f, q * 0.4f);
	}
	// the extras: each in some gardens and not others
	if (Has(TEXT("bench")) && Front > 3.0f)
	{
		const float U = bTreeRight ? GL + 2.75f : GR - 2.75f, V = GF + 2.2f;
		Y(false, Wood, U, 0.f, V, 1.5f, 0.45f, 0.55f, PI); Y(false, Wood, U, 0.45f, V + 0.25f, 1.5f, 0.45f, 0.06f, PI);
	}
	const bool bBistro = Has(TEXT("bistro")) && Back > 2.8f;
	if (bBistro)
	{
		const float U = GR - 2.4f, V = HV + 2.2f;
		Y(false, Charcoal, U, 0.f, V, 0.6f, 0.72f, 0.6f); Y(false, Door, U, 0.72f, V, 0.7f, 0.04f, 0.7f);
		for (float Sd : { -1.f, 1.f }) { Y(false, Charcoal, U + Sd * 0.65f, 0.f, V, 0.42f, 0.45f, 0.42f); Y(true, Wood, U + Sd * 0.65f, 0.45f, V, 0.42f, 0.04f, 0.42f); }
		Y(true, Wood, U, 0.f, V, 0.05f, 2.3f, 0.05f); Y(true, Line == TEXT("plain") ? Cream : Shut, U, 2.0f, V, 2.2f, 0.3f, 2.2f, PI / 4);
	}
	if (Has(TEXT("lounger")) && Back > 2.9f) { const float U = GL + 1.4f, V = HV + 1.8f; Y(false, Wood, U, 0.f, V, 0.65f, 0.35f, 1.9f); Y(true, Shut, U, 0.35f, V + 0.3f, 0.6f, 0.1f, 1.3f); }
	if (Has(TEXT("washing")) && Back > 2.2f)
	{
		for (float U : { -1.5f, 2.5f }) Y(false, Wood, U, 0.f, GB - 1.5f, 0.08f, 1.9f, 0.08f);
		B(true, Charcoal, -1.5f, 1.85f, GB - 1.52f, 2.5f, 1.87f, GB - 1.48f);
		const FLinearColor Wash[4] = { Cream, Blue, Terra, Cream };
		for (int32 q = 0; q < 4; ++q) Y(true, Wash[q], -1.0f + q * 0.95f, 1.25f, GB - 1.5f, 0.55f, 0.6f, 0.03f);
	}
	if (Has(TEXT("flowers")) && Front > 2.5f)
	{
		const float U = Du - 3.4f, V = GF + 1.4f;
		B(false, Soil, U - 1.6f, 0.f, V - 0.45f, U + 1.6f, 0.22f, V + 0.45f);
		const FLinearColor Bloom[4] = { Terra, Cream, Plum, Ochre };
		for (int32 q = 0; q < 9; ++q) Y(true, Bloom[q % 4], U - 1.4f + q * 0.35f, 0.22f, V + (q % 2 ? 0.18f : -0.18f), 0.22f, 0.25f, 0.22f);
	}
	if (Has(TEXT("woodpile")) && Right > 1.1f) { const float U = GR - 0.6f, V = 1.5f; B(false, Brown, U - 0.35f, 0.f, V - 1.0f, U + 0.35f, 1.1f, V + 1.0f); B(false, Wood, U - 0.45f, 1.1f, V - 1.1f, U + 0.45f, 1.18f, V + 1.1f); }
	// the everyday things of a home outside: dustbins and a recycling crate, the doormat, the house number, the door lamp
	const FLinearColor Iron(.2f, .21f, .22f), Lid(.26f, .27f, .28f);
	for (float U : { 4.75f, 5.45f }) { Y(false, Iron, U, 0.f, -HV - 0.5f, 0.55f, 0.85f, 0.55f); Y(true, Lid, U, 0.85f, -HV - 0.5f, 0.62f, 0.06f, 0.62f); }
	Y(false, Lin(.2f, .34f, .3f), 5.1f, 0.f, -HV - 1.15f, 0.6f, 0.3f, 0.42f); Y(true, Cream, 5.0f, 0.3f, -HV - 1.15f, 0.3f, 0.08f, 0.25f);
	B(true, Lin(.36f, .26f, .16f), Du - 0.45f, S0, -HV - 0.5f, Du + 0.45f, S0 + 0.02f, -HV - 0.1f);
	B(true, Line == TEXT("workshop") ? Cream : Charcoal, Du + 0.78f, S0 + 1.7f, -HV - 0.03f, Du + 1.02f, S0 + 1.9f, -HV);
	B(true, Line == TEXT("workshop") ? Charcoal : Cream, Du + 0.85f, S0 + 1.76f, -HV - 0.04f, Du + 0.95f, S0 + 1.84f, -HV - 0.03f);
	B(true, Charcoal, Du - 1.0f, S0 + 2.0f, -HV - 0.2f, Du - 0.82f, S0 + 2.32f, -HV);
	B(true, Lin(1.0f, .8f, .5f), Du - 0.98f, S0 + 2.05f, -HV - 0.18f, Du - 0.84f, S0 + 2.27f, -HV - 0.02f);
	if (Has(TEXT("parcel"))) { Y(true, Lin(.58f, .48f, .32f), Du + 0.45f, S0, -HV - 0.35f, 0.4f, 0.3f, 0.32f, 0.3f); Y(true, Teal, Du + 0.45f, S0 + 0.3f, -HV - 0.35f, 0.4f, 0.02f, 0.08f, 0.3f); }
	if (Has(TEXT("milk"))) for (int32 q = 0; q < 2; ++q) Y(true, Cream, Du - 0.5f + q * 0.12f, S0, -HV - 0.3f, 0.08f, 0.24f, 0.08f);
	if (Has(TEXT("birdbath")) && Front > 3.3f) { Y(false, Stone, 2.6f, 0.f, -7.0f, 0.2f, 0.8f, 0.2f); Y(false, Stone, 2.6f, 0.8f, -7.0f, 0.7f, 0.12f, 0.7f); Y(true, Lin(.3f, .4f, .45f), 2.6f, 0.9f, -7.0f, 0.5f, 0.03f, 0.5f); }
	if (Back > 1.3f)                                       // the compost bin
	{
		const float U = GL + 0.75f, V = GB - 0.75f;
		B(false, Wood, U - 0.5f, 0.f, V - 0.5f, U + 0.5f, 0.9f, V + 0.5f);
		for (int32 q = 0; q < 4; ++q) B(true, Brown, U - 0.52f, 0.1f + q * 0.2f, V - 0.52f, U + 0.52f, 0.16f + q * 0.2f, V - 0.5f);
	}
	{ const float V = 3.3f; B(true, Lin(.2f, .34f, .3f), HU, 0.5f, V - 0.25f, HU + 0.12f, 1.0f, V + 0.25f); Y(true, Lin(.3f, .4f, .25f), HU + 0.2f, 0.5f, V, 0.1f, 0.5f, 0.5f); }   // a hose reel on the side wall
	if (!bBistro && Has(TEXT("shed")) && Back > 2.6f)      // a garden shed in the back corner, its roof two slopes
	{
		const float U0 = GR - 2.4f, U1 = GR - 0.4f, V0 = GB - 2.4f, V1 = GB - 0.4f, Hh = 2.1f;
		B(false, Line == TEXT("plain") ? Cream : Line == TEXT("workshop") ? Lin(.3f, .22f, .15f) : Wood, U0, 0.f, V0, U1, Hh, V1);
		B(true, Door, U0 + 0.7f, 0.f, V0 - 0.02f, U1 - 0.7f, 1.9f, V0);
		if (UStaticMesh* Wedge = Kit(TEXT("SM_Kit_RoofWedge")))
			Add(Group(Wedge, true), FTransform(FQuat(FVector::UpVector, Ang), At((U0 + U1) / 2, V1 + 0.15f, Hh), FVector(U1 - U0 + 0.3f, V1 - V0 + 0.3f, 0.5f)), RoofC);
		if (Has(TEXT("can"))) Y(true, Teal, U0 - 0.4f, 0.f, V0 + 0.3f, 0.3f, 0.28f, 0.18f);
	}
	else if (Has(TEXT("can")) && Right > 0.6f) Y(true, Teal, GR - 0.35f, 0.f, 3.0f, 0.3f, 0.28f, 0.18f);
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
		for (const FBrixProp& P : B.Props) BuildProp(P, B);
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

// A block's street furniture (its 'props'), as the slice draws it (play.js prop(), dressing.js prop()): boxes in the
// palette's colours, the same sizes. Lamps, benches, bins, bollards, planters, postboxes, bike racks, phone boxes, wall
// lanterns, flowerbeds, fountains, market stalls, tram shelters, water towers and wells. Garden planting, playgrounds,
// arches, adverts and the open ground's own things are not brought over yet.
void ABrixTownActor::BuildProp(const FBrixProp& P, const FBrixBlock& Blk)
{
	UStaticMesh* BoxMesh = Kit(TEXT("SM_Kit_Box"), Cube);
	UInstancedStaticMeshComponent* Solid = Group(BoxMesh, true);
	UInstancedStaticMeshComponent* Fine = Group(BoxMesh, false, GardenCullDistance);
	auto Lin = [](float R, float G, float Bl) { return FLinearColor(R, G, Bl); };
	// the slice's colours (play.js C, dressing.js)
	const FLinearColor Stone = Lin(.52f, .48f, .43f), StoneDark = Lin(.37f, .34f, .32f), Metal = Lin(.17f, .18f, .21f), Lantern = Lin(1.f, .55f, .22f);
	const FLinearColor Water = Lin(.2f, .32f, .38f), Soil = Lin(.2f, .15f, .11f), Olive = Lin(.30f, .33f, .20f), Brown = Lin(.24f, .16f, .11f);
	const FLinearColor Wood = Lin(.5f, .32f, .16f), Iron = Lin(.17f, .18f, .21f), Red = Lin(.58f, .12f, .1f), Green = Lin(.16f, .3f, .22f);
	const FLinearColor Teal = Lin(.20f, .36f, .37f), Cream = Lin(.66f, .61f, .50f), Ochre = Lin(.52f, .40f, .20f), Terra = Lin(.44f, .24f, .18f), Plum = Lin(.30f, .24f, .34f);
	const float x = P.Pos.X, z = P.Pos.Y;
	// the ground there: the block's paving, an open lot's surface a little above it
	float g = BlockSlabHeight;
	for (const FBrixLot& L : Blk.Lots)
		if (L.IsOpen() && x >= L.Min.X && x <= L.Max.X && z >= L.Min.Y && z <= L.Max.Y) { g += 0.04f; break; }
	// box: corner to corner in the slice's frame (x, up, z); ybox: a size standing at a point, turned
	auto box = [&](float x0, float y0, float z0, float x1, float y1, float z1, const FLinearColor& C, bool bFine = false)
	{
		Add(bFine ? Fine : Solid, FTransform(FQuat::Identity, ToWorld(FVector2D((x0 + x1) / 2, (z0 + z1) / 2), (y0 + y1) / 2),
			FVector(FMath::Abs(x1 - x0), FMath::Abs(z1 - z0), FMath::Abs(y1 - y0))), C);
	};
	auto ybox = [&](float cx, float y0, float cz, float w, float h, float d, float Yaw, const FLinearColor& C, bool bFine = false)
	{
		Add(bFine ? Fine : Solid, FTransform(FQuat(FVector::UpVector, -Yaw), ToWorld(FVector2D(cx, cz), y0 + h / 2), FVector(w, d, h)), C);
	};
	auto H = [&](const TCHAR* K, int32 Q) { return (float)SliceHash({ P.Id + FString::SanitizeFloat(x + z), FString::FromInt(Q), K }); };
	const FString& k = P.Kind;
	const bool ax = P.Along != TEXT("z");
	const float Oct = UE_PI / 8;
	if (k == TEXT("lamp"))   // a lantern on a post
	{
		box(x - .17f, g, z - .17f, x + .17f, g + .55f, z + .17f, StoneDark); box(x - .08f, g + .55f, z - .08f, x + .08f, g + 3.3f, z + .08f, Metal);
		box(x - .2f, g + 3.3f, z - .2f, x + .2f, g + 3.78f, z + .2f, Lantern, true); box(x - .27f, g + 3.78f, z - .27f, x + .27f, g + 3.88f, z + .27f, Metal, true);
	}
	else if (k == TEXT("bench"))   // a wooden slab on two stone blocks
	{
		const bool bx = P.Along == TEXT("x");
		box(x - (bx ? 1 : .27f), g + .36f, z - (bx ? .27f : 1), x + (bx ? 1 : .27f), g + .47f, z + (bx ? .27f : 1), Wood);
		for (const float sd : { -.68f, .68f })
			box(x + (bx ? sd : 0) - (bx ? .15f : .22f), g, z + (bx ? 0 : sd) - (bx ? .22f : .15f), x + (bx ? sd : 0) + (bx ? .15f : .22f), g + .36f, z + (bx ? 0 : sd) + (bx ? .22f : .15f), Stone);
	}
	else if (k == TEXT("bin"))
	{
		ybox(x, g, z, .42f, .82f, .42f, Oct, Green); ybox(x, g + .82f, z, .5f, .08f, .5f, Oct, Green * .8f, true); box(x - .12f, g + .9f, z - .12f, x + .12f, g + .95f, z + .12f, Iron, true);
	}
	else if (k == TEXT("bollard"))
	{
		ybox(x, g, z, .2f, .78f, .2f, Oct, Iron); ybox(x, g + .78f, z, .24f, .08f, .24f, Oct, Iron, true);
	}
	else if (k == TEXT("postbox"))   // a red pillar box
	{
		ybox(x, g, z, .46f, 1.15f, .46f, Oct, Red); ybox(x, g + 1.15f, z, .52f, .12f, .52f, Oct, Red * .85f, true); ybox(x, g + 1.27f, z, .3f, .08f, .3f, Oct, Red * .85f, true);
	}
	else if (k == TEXT("planter"))
	{
		const float h = P.bBig ? .75f : .48f, sz = P.bBig ? .7f : .45f;
		box(x - sz, g, z - sz, x + sz, g + h, z + sz, P.bBig ? Stone : Wood); box(x - sz + .08f, g + h, z - sz + .08f, x + sz - .08f, g + h + .01f, z + sz - .08f, Soil, true);
		for (int32 q = 0; q < 4; ++q) ybox(x + (q % 2 - .5f) * sz * .9f, g + h, z + (q / 2 - .5f) * sz * .9f, sz * .8f, .3f + .35f * H(TEXT("pl"), q), sz * .8f, H(TEXT("py"), q) * 3, Olive * (.9f + .3f * H(TEXT("pc"), q)), true);
	}
	else if (k == TEXT("bikerack"))   // three hoops
	{
		for (int32 q = -1; q <= 1; ++q)
		{
			const float cx = x + (ax ? q * .8f : 0), cz = z + (ax ? 0 : q * .8f);
			for (const float sd : { -.32f, .32f }) box(cx + (ax ? 0 : sd) - .03f, g, cz + (ax ? sd : 0) - .03f, cx + (ax ? 0 : sd) + .03f, g + .8f, cz + (ax ? sd : 0) + .03f, Iron, true);
			box(cx - (ax ? .03f : .35f), g + .8f, cz - (ax ? .35f : .03f), cx + (ax ? .03f : .35f), g + .86f, cz + (ax ? .35f : .03f), Iron, true);
		}
	}
	else if (k == TEXT("phonebox"))   // a red telephone box with glazed sides and a crown
	{
		box(x - .5f, g, z - .5f, x + .5f, g + .15f, z + .5f, Red);
		for (const float sx : { -.45f, .45f }) for (const float sz : { -.45f, .45f }) box(x + sx - .06f, g, z + sz - .06f, x + sx + .06f, g + 2.35f, z + sz + .06f, Red);
		box(x - .43f, g + .15f, z - .43f, x + .43f, g + .85f, z + .43f, Red);
		box(x - .44f, g + .85f, z - .44f, x + .44f, g + 2.05f, z + .44f, Lin(.36f, .44f, .48f));
		box(x - .55f, g + 2.35f, z - .55f, x + .55f, g + 2.6f, z + .55f, Red); ybox(x, g + 2.6f, z, .9f, .14f, .9f, 0, Red * .9f, true);
	}
	else if (k == TEXT("lantern"))   // a wall lantern: glass on an iron arm
	{
		const float lx = x + P.Out.X * .32f, lz = z + P.Out.Y * .32f, h = P.Height;
		box(FMath::Min(x, lx) - .03f, h + .28f, FMath::Min(z, lz) - .03f, FMath::Max(x, lx) + .03f, h + .33f, FMath::Max(z, lz) + .03f, Metal, true);
		box(lx - .11f, h - .12f, lz - .11f, lx + .11f, h + .26f, lz + .11f, Lantern, true); box(lx - .15f, h + .26f, lz - .15f, lx + .15f, h + .31f, lz + .15f, Metal, true);
	}
	else if (k == TEXT("flowerbed"))
	{
		box(x - 1.3f, g, z - 1.3f, x + 1.3f, g + .3f, z + 1.3f, Stone); box(x - 1.08f, g + .3f, z - 1.08f, x + 1.08f, g + .32f, z + 1.08f, Soil, true);
		for (int32 q = 0; q < 16; ++q)
		{
			const float fx = x - .82f + (q % 4) * .55f, fz = z - .82f + (q / 4) * .55f, hh = .2f + .25f * H(TEXT("fh"), q);
			box(fx - .13f, g + .32f, fz - .13f, fx + .13f, g + .32f + hh, fz + .13f, Olive * (.9f + .3f * H(TEXT("fc"), q)), true);
		}
	}
	else if (k == TEXT("fountain"))   // an eight-sided basin with a bowl on a column
	{
		const float R = P.R * 1.36f, r2 = R - .42f, Seg = 2 * R * FMath::Sin(UE_PI / 8) + .05f;
		for (int32 q = 0; q < 8; ++q)
		{
			const float a = (q + .5f) * UE_PI / 4 + .3927f, mr = (R + r2) / 2 * FMath::Cos(UE_PI / 8);
			Add(Solid, FTransform(FQuat(FVector::UpVector, a), ToWorld(FVector2D(x + FMath::Cos(a) * mr, z + FMath::Sin(a) * mr), g + .275f), FVector(R - r2, Seg, .55f)), Stone);
		}
		for (const float a : { .3927f, .3927f + UE_PI / 4 })
			Add(Fine, FTransform(FQuat(FVector::UpVector, a), ToWorld(FVector2D(x, z), g + .39f), FVector(2 * r2 * FMath::Cos(UE_PI / 8), 2 * r2 * FMath::Cos(UE_PI / 8), .02f)), Water);
		box(x - .28f, g + .4f, z - .28f, x + .28f, g + 1.5f, z + .28f, Stone); box(x - .75f, g + 1.5f, z - .75f, x + .75f, g + 1.72f, z + .75f, Stone);
		box(x - .7f, g + 1.68f, z - .7f, x + .7f, g + 1.73f, z + .7f, Water, true); box(x - .16f, g + 1.72f, z - .16f, x + .16f, g + 2.05f, z + .16f, Stone, true);
		if (P.bGrand)   // a second bowl higher up and a bronze figure
		{
			const FLinearColor Bz = Lin(.3f, .42f, .36f);
			box(x - .2f, g + 2.05f, z - .2f, x + .2f, g + 2.9f, z + .2f, Stone); box(x - .5f, g + 2.9f, z - .5f, x + .5f, g + 3.05f, z + .5f, Stone);
			box(x - .14f, g + 3.05f, z - .1f, x + .14f, g + 3.55f, z + .1f, Bz, true); box(x - .18f, g + 3.5f, z - .13f, x + .18f, g + 3.95f, z + .13f, Bz, true); ybox(x, g + 3.95f, z, .17f, .2f, .17f, UE_PI / 4, Bz, true);
		}
	}
	else if (k == TEXT("stall"))   // a market stall: counter, goods, posts and an awning in an accent
	{
		const float f = P.Face != 0.f ? P.Face : 1.f, tz = z + f * 1.1f;
		const FLinearColor Accent[6] = { Teal, Ochre, Terra, Cream, Plum, Lin(.30f, .38f, .52f) };
		const FLinearColor Col = Accent[P.Color % 6];
		box(x - 1.2f, g, tz - .4f, x + 1.2f, g + .9f, tz + .4f, Wood);
		const FLinearColor Goods[5] = { Ochre, Terra, Olive, Cream, Plum };
		for (int32 q = 0; q < 5; ++q) box(x - 1.05f + q * .45f, g + .9f, tz - .25f, x - .75f + q * .45f, g + 1.04f + .12f * H(TEXT("sg"), q), tz + .25f, Goods[(int32)(H(TEXT("sc"), q) * 5) % 5], true);
		for (const float sx : { -1.4f, 1.4f }) for (const float sz : { -.7f, 1.9f }) box(x + sx - .06f, g, z + f * sz - .06f, x + sx + .06f, g + 2.5f, z + f * sz + .06f, Brown, true);
		box(x - 1.65f, g + 2.5f, FMath::Min(z - f * .95f, z + f * 2.15f), x + 1.65f, g + 2.64f, FMath::Max(z - f * .95f, z + f * 2.15f), Col);
	}
	else if (k == TEXT("shelter"))   // the tram stop's shelter
	{
		for (const float sx : { -6.f, -2.f, 2.f, 6.f }) box(x + sx - .08f, g, z - 1.26f, x + sx + .08f, g + 2.6f, z - 1.1f, Brown);
		box(x - 6.4f, g + 2.6f, z - 1.4f, x + 6.4f, g + 2.74f, z + 1.3f, Teal); box(x - 6.f, g + .9f, z - 1.24f, x + 6.f, g + 2.2f, z - 1.14f, Cream);
	}
	else if (k == TEXT("watertower"))
	{
		for (const float sx : { -2.f, 2.f }) for (const float sz : { -2.f, 2.f }) box(x + sx - .2f, g, z + sz - .2f, x + sx + .2f, g + 9, z + sz + .2f, Brown);
		box(x - 2.8f, g + 9, z - 2.8f, x + 2.8f, g + 13, z + 2.8f, Wood); box(x - 3, g + 13, z - 3, x + 3, g + 13.3f, z + 3, Metal);
	}
	else if (k == TEXT("well"))   // a round stone well with a little roof
	{
		ybox(x, g, z, 1.7f, .8f, 1.7f, Oct, Stone); ybox(x, g + .79f, z, 1.3f, .02f, 1.3f, Oct, Water, true);
		for (const float sx : { -.75f, .75f }) box(x + sx - .06f, g + .8f, z - .06f, x + sx + .06f, g + 2.2f, z + .06f, Wood, true);
		box(x - 1.f, g + 2.2f, z - .6f, x + 1.f, g + 2.35f, z + .6f, Terra);
	}
}
