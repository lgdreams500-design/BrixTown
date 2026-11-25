#include "BrixTownActor.h"
#include "BrixRoofs.h"
#include "Engine/Texture2D.h"

#include "BrixBuilder.h"
#include "BrixCity.h"
#include "BrixHomes.h"
#include "BrixInside.h"
#include "BrixStreets.h"
#include "BrixUnits.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Dom/JsonObject.h"
#include "Components/StaticMeshComponent.h"
#include "MeshDescription.h"
#include "StaticMeshAttributes.h"
#include "Components/TextRenderComponent.h"
#include "Engine/StaticMesh.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"
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

UInstancedStaticMeshComponent* ABrixTownActor::Group(UStaticMesh* Mesh, bool bCollide, float CullM, bool bClear)
{
	const FString Key = Mesh->GetName() + (bCollide ? TEXT("") : TEXT("_NoCollision")) + (bClear ? TEXT("_Clear") : TEXT(""));
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
		if (bClear && (S == TEXT("glass") || S == TEXT("lit")))
		{
			if (!ClearGlass) ClearGlass = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Clear.M_Clear"));   // Tools/unreal/make_clear_material.py
			if (ClearGlass) { G->SetMaterial(i, ClearGlass); continue; }
		}
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

UInstancedStaticMeshComponent* ABrixTownActor::GlassGroup()
{
	if (Glass) return Glass;
	Glass = NewObject<UInstancedStaticMeshComponent>(this, TEXT("LampGlass"));
	Glass->SetupAttachment(RootComponent);
	Glass->SetStaticMesh(Cube);
	Glass->NumCustomDataFloats = 3;
	Glass->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Glass->SetCastShadow(false);
	if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Glow.M_Glow")))
	{
		GlassMat = UMaterialInstanceDynamic::Create(Glow, this);
		Glass->SetMaterial(0, GlassMat);
	}
	else
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: /Game/Greybox/M_Glow is missing (run Tools/unreal/make_glow_material.py)"));
		Glass->SetMaterial(0, Own());
	}
	Glass->RegisterComponent();
	AddInstanceComponent(Glass);
	return Glass;
}

UInstancedStaticMeshComponent* ABrixTownActor::BlockerGroup()
{
	if (Blockers) return Blockers;
	Blockers = NewObject<UInstancedStaticMeshComponent>(this, TEXT("Blockers"));
	Blockers->SetupAttachment(RootComponent);
	Blockers->SetStaticMesh(Cube);
	Blockers->SetCollisionProfileName(TEXT("BlockAll"));
	Blockers->SetHiddenInGame(true);                 // still in the way, never drawn
	Blockers->SetVisibility(false);
	Blockers->SetCastShadow(false);
	Blockers->RegisterComponent();
	AddInstanceComponent(Blockers);
	return Blockers;
}

UInstancedStaticMeshComponent* ABrixTownActor::NightPaneGroup()
{
	if (NightPanes) return NightPanes;
	NightPanes = NewObject<UInstancedStaticMeshComponent>(this, TEXT("NightPanes"));
	NightPanes->SetupAttachment(RootComponent);
	NightPanes->SetStaticMesh(Cube);
	NightPanes->NumCustomDataFloats = 3;
	NightPanes->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	NightPanes->SetCastShadow(false);
	NightPanes->SetVisibility(false);   // shown after dusk (SetNight)
	NightPanes->bAffectDynamicIndirectLighting = false;   // out of Lumen's scene: tens of thousands of small cards overfilled its cache
	NightPanes->bAffectDistanceFieldLighting = false;
	if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Glow.M_Glow")))
	{
		NightMat = UMaterialInstanceDynamic::Create(Glow, this);
		NightPanes->SetMaterial(0, NightMat);
	}
	else NightPanes->SetMaterial(0, Own());
	NightPanes->RegisterComponent();
	AddInstanceComponent(NightPanes);
	return NightPanes;
}

void ABrixTownActor::SetNight(float Night01)
{
	if (!NightPanes || FMath::IsNearlyEqual(Night01, LastNightPane, 0.002f)) return;
	LastNightPane = Night01;
	NightPanes->SetVisibility(Night01 > 0.02f);
	if (NightMat) NightMat->SetScalarParameterValue(TEXT("Glow"), NightGlow * Night01);
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

// BUILD-020 (play.js: the designer's cut): a building cut open at CutCm. What stands wholly above the cut is not drawn; an
// upright box standing through it ends there; a kit piece through it (a window, a door, a roof's slope) is not drawn.
bool ABrixTownActor::CutAt(const UInstancedStaticMeshComponent* G, FTransform& At) const
{
	const UStaticMesh* M = G ? G->GetStaticMesh() : nullptr;
	if (!M) return true;
	const FBox B = M->GetBoundingBox();
	const FQuat Q = At.GetRotation();
	if (FMath::Abs(Q.X) > 1e-4 || FMath::Abs(Q.Y) > 1e-4) return B.TransformBy(At).Max.Z <= CutCm + 0.5;   // leaning (a roof's slope)
	const double Sz = At.GetScale3D().Z, Z0 = At.GetLocation().Z + B.Min.Z * Sz, Z1 = At.GetLocation().Z + B.Max.Z * Sz;
	if (Z1 <= CutCm + 0.5) return true;
	if (Z0 >= CutCm - 0.5) return false;
	if (M != Cube && M->GetName() != TEXT("SM_Kit_Box")) return false;
	const double Hgt = B.Max.Z - B.Min.Z;
	if (Hgt < 1e-3) return false;
	FVector S = At.GetScale3D();
	S.Z = (CutCm - Z0) / Hgt;
	FVector L = At.GetLocation();
	L.Z = Z0 - B.Min.Z * S.Z;
	At.SetScale3D(S);
	At.SetLocation(L);
	return true;
}

void ABrixTownActor::Add(UInstancedStaticMeshComponent* G, const FTransform& Whole, const FLook& Look)
{
	FTransform At = Whole;
	if (CutCm >= 0.f && !CutAt(G, At)) return;
	FPending& P = Pending.FindOrAdd(G);
	P.At.Add(At);
	P.Look.Add(Look);
	if (Wrapping) Wrapping->Parts.Add({ G, P.At.Num() - 1, At, Look.Colour });   // its index among the pending, made the instance's below
	if (Recording) Recording->Add({ G, P.At.Num() - 1, At, Look.Colour });   // a building's own instances, to repaint it (BUILD-005)
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
	bool bShutters = L.Kind == TEXT("home") || bFood || bShop;
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
	// BUILD-002: the building's design (inside.json's dsg, or a remodel being tried in the designer) gives its colours, as the
	// slice builds from it (play.js building()): each facade's plaster and its stone or brick (to the eaves or not), the
	// roof's tiles, the window frames and the shutters (or none); a designed house keeps its colourway
	const FBrixInside* InD = Insides && Insides->Lots.IsValidIndex(LotIndex) && Insides->Lots[LotIndex].IsSet() ? &Insides->Lots[LotIndex].GetValue() : nullptr;
	const TSharedPtr<FJsonObject>* Over = DesignOver.Find(LotIndex);
	const TSharedPtr<FJsonObject> Dsg = Over ? *Over : InD ? InD->Design : TSharedPtr<FJsonObject>();
	TOptional<FBrixBuiltLook> Bt;
	if (Dsg && Insides && Insides->Builder.IsSet()) Bt = BrixBuilder::Built(Insides->Builder, Dsg);
	FLinearColor PlasterOf[4] = { Plaster, Plaster, Plaster, Plaster };   // per facade: -y (n), +y (s), -x (w), +x (e)
	FLook StoneOf[4] = { Stone, Stone, Stone, Stone };
	float StoneToOf[4] = { StoneTo, StoneTo, StoneTo, StoneTo };
	if (Bt && !bHouse)
	{
		Frame = Lin(Bt->Frame); Shutter = Lin(Bt->Shutter); Roof = Lin(Bt->Roof);
		Plaster = Lin(Bt->Walls[FBrixBuiltLook::FacadeIndex(Bt->Side)].Plaster);
		if (!Bt->bShutters) bShutters = false;
		static const TCHAR* const Of[4] = { TEXT("n"), TEXT("s"), TEXT("w"), TEXT("e") };
		for (int32 q = 0; q < 4; ++q)
		{
			const FBrixBuiltLook::FFacade& Fa = Bt->Walls[FBrixBuiltLook::FacadeIndex(Of[q])];
			PlasterOf[q] = Lin(Fa.Plaster);
			if (!Fa.Pattern.IsEmpty() && CityData)
			{
				FBrixSurface Sf;
				Sf.Pattern = CityData->FindPattern(Fa.Pattern);
				Sf.Color = Fa.Stone;
				StoneOf[q] = Surface(Sf, TownStone);
			}
			StoneToOf[q] = (Fa.bStoneToEaves || Fa.bBrickToEaves) ? H : (StoneBaseDistricts.Contains(B.District) ? FMath::Min(StoneBaseHeight, H) : 0.f);
		}
	}
	const bool bAllStone = StoneToOf[0] >= H && StoneToOf[1] >= H && StoneToOf[2] >= H && StoneToOf[3] >= H;
	FBrixInside InOver;                                    // the inside's finishes of a remodel being tried (BUILD-019)
	if (Over && InD && Bt) { InOver = *InD; InOver.FloorFinish = Bt->FloorFinish; InOver.FloorColour = Bt->FloorColour; InOver.WallColour = Bt->WallColour; }

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

	// an enterable building (CITY-013) is hollow on its ground floor, as the slice builds it (play.js building()): an open
	// doorway at the door, a floor in the storey's finish, the walls plastered inside in its colour, a ceiling; what is
	// above the ceiling stays the solid core until its storeys are built
	const FBrixInside* In = Over && InD && Bt ? &InOver : InD;
	const bool bHollow = CityData && In && BrixIsHollow(L, In);
	FBrixDoorway Dw;
	if (bHollow) Dw = BrixDoorwayOf(*CityData, L, B.District, FBrixDoorRules::FromConfig());
	const float FloorTop = bHouse ? S0 : InsideFloor;
	// WLD-050, WLD-052: a building of two storeys or more with a stairwell (a designed house: its hall stair) is hollow to its
	// roof, every storey reached by its stair (storeys.js shell()); its storeys' floors and ceilings where BrixWorld has them
	const bool bStoreys = bHollow && (bHouse || (Floors >= 2 && In && In->Stair.IsSet() && L.AngleDeg == 0.f && Insides));
	const int32 NFl = bStoreys ? (bHouse ? 2 : Floors) : 1;
	const FBrixDoorRules DR = FBrixDoorRules::FromConfig();
	auto FloorTopOf = [&](int32 f) { return BrixFloorUp(L, f, DR) - Base; };
	auto CeilOf = [&](int32 f) { return bStoreys ? BrixCeilingUp(L, f, DR) - Base : FMath::Min(H, (bHouse ? S1 : StoreyHeight) - CeilingDrop); };
	auto ColourOf = [&](const TArray<FVector3f>& A, int32 f, const FLinearColor& Else) { return A.Num() ? Lin(A[FMath::Clamp(f, 0, A.Num() - 1)]) : Else; };
	const FLinearColor InWall = In && In->WallColour.Num() ? Lin(In->WallColour[0]) : FLinearColor(0.77f, 0.72f, 0.62f);
	const FLinearColor InFloor = In && In->FloorColour.Num() ? Lin(In->FloorColour[0]) : FLinearColor(0.4f, 0.27f, 0.17f);
	auto BlockAt = [&](const FVector2D& Local, float Z0, float Z1, double YawRad, float Len, float Dep)
	{
		Add(BlockerGroup(), FTransform(FQuat(FVector::UpVector, YawRad + Ang), At(Local, (Z0 + Z1) * 0.5f), FVector(Len, Dep, Z1 - Z0)), FLinearColor::Black);
	};

	// the core, set back by the wall's depth all round (the kit's "core": what the reveals look into)
	const float HollowTop = CeilOf(NFl - 1);
	const float CoreFrom = bHollow ? HollowTop : 0.f;
	if (bAllStone) BoxLook(StoneOf[0], FVector2D::ZeroVector, CoreFrom, H, 0.0, S.X - 2 * t, S.Y - 2 * t);
	else Box(Plaster, FVector2D::ZeroVector, CoreFrom, H, 0.0, S.X - 2 * t, S.Y - 2 * t);
	if (bHollow)
	{
		FLook Fl;   // boards or flags by the storey's finish (the slice's floor materials 15 and 16)
		Fl.Colour = InFloor;
		const bool bBoards = !In || !In->FloorFinish.Num() || In->FloorFinish[0] == 15;
		Fl.A = FVector4f(bBoards ? FloorBoardsA : FloorFlagsA);
		Fl.B = FVector4f(bBoards ? FloorBoardsB : FloorFlagsB);
		BoxLook(Fl, FVector2D::ZeroVector, 0.f, FloorTop, 0.0, S.X - 2 * t, S.Y - 2 * t);
		Box(FMath::Lerp(ColourOf(In ? In->WallColour : TArray<FVector3f>(), NFl - 1, InWall), FLinearColor::White, CeilingWhite), FVector2D::ZeroVector, HollowTop - 0.02f, HollowTop, 0.0, S.X - 2 * t, S.Y - 2 * t);
		++Hollow;
	}
	if (bStoreys)
	{
		// the stair: the flights' rectangles in the lot's own frame (x0, x1, y0, y1), as storeys.js stairOf() and houses.js stepsS()
		FBrixStairGeo G;
		FVector4f Well(0, 0, 0, 0);
		float StepOff = 0.f;
		const bool bWell = bHouse ? BrixHouseWell(*Insides, Well, StepOff) : true;
		if (!bHouse) G = BrixStairOf(*In->Stair, Insides->Rules);
		auto UvRect = [&](float U0, float U1, float V0, float V1)   // a stairwell rectangle (u, v) to the lot's frame
		{
			const FVector2D A = G.P(FMath::Min(U0, U1), FMath::Min(V0, V1)) - C, B2 = G.P(FMath::Max(U0, U1), FMath::Max(V0, V1)) - C;
			return FVector4f((float)A.X, (float)B2.X, (float)A.Y, (float)B2.Y);
		};
		auto HouseRect = [&](float U0, float U1, float V0, float V1) { return FVector4f(-U1, -U0, V0, V1); };   // houses.js: u mirrored
		auto RectBox = [&](const FLinearColor& Col, const FVector4f& R, float Z0, float Z1)
		{
			if (R.Y - R.X > 0.005f && R.W - R.Z > 0.005f) Box(Col, FVector2D((R.X + R.Y) / 2, (R.Z + R.W) / 2), Z0, Z1, 0.0, R.Y - R.X, R.W - R.Z);
		};
		// the floors above the ground: a slab over the whole storey but for the hole the flight below comes up through
		const FVector4f Inner(-S.X / 2 + t, S.X / 2 - t, -S.Y / 2 + t, S.Y / 2 - t);
		for (int32 f = 1; f < NFl; ++f)
		{
			FVector4f Hole(0, 0, 0, 0);
			if (bHouse && bWell) Hole = HouseRect(Well.X, Well.Y, Well.Z, Well.W);
			else if (!bHouse)
			{
				// over the flight below; a straight stair's hole is wider on its open side, so a head clears the floor's edge
				const FBrixStairGeo::FFlight Fb = G.Flight(f - 1);
				const bool bOpenHigh = G.bStraight && FMath::Abs(G.Open - Fb.Lane.Y) < 1e-3f, bOpenLow = G.bStraight && !bOpenHigh;
				Hole = UvRect(G.ULo, G.UHi, Fb.Lane.X - (bOpenLow ? StairHeadroom : 0.f), Fb.Lane.Y + (bOpenHigh ? StairHeadroom : 0.f));
			}
			const float Za = CeilOf(f - 1), Zb = FloorTopOf(f);
			FLook Fl;
			Fl.Colour = ColourOf(In->FloorColour, f, InFloor);
			const bool bBoards = !In->FloorFinish.IsValidIndex(f) || In->FloorFinish[f] == 15;
			Fl.A = FVector4f(bBoards ? FloorBoardsA : FloorFlagsA);
			Fl.B = FVector4f(bBoards ? FloorBoardsB : FloorFlagsB);
			const FLinearColor Ceil = FMath::Lerp(ColourOf(In->WallColour, f - 1, InWall), FLinearColor::White, CeilingWhite);
			const FVector4f Strips[4] = { FVector4f(Inner.X, Inner.Y, Inner.Z, Hole.Z), FVector4f(Inner.X, Inner.Y, Hole.W, Inner.W),
				FVector4f(Inner.X, Hole.X, Hole.Z, Hole.W), FVector4f(Hole.Y, Inner.Y, Hole.Z, Hole.W) };
			for (const FVector4f& R : (Hole.Y > Hole.X ? TArrayView<const FVector4f>(Strips, 4) : TArrayView<const FVector4f>(&Inner, 1)))
			{
				RectBox(Ceil, R, Za, Za + 0.02f);
				if (R.Y - R.X > 0.005f && R.W - R.Z > 0.005f)
					BoxLook(Fl, FVector2D((R.X + R.Y) / 2, (R.Z + R.W) / 2), Za + 0.02f, Zb, 0.0, R.Y - R.X, R.W - R.Z);
			}
		}
		if (bHouse && bWell)
		{
			// houses.js insideS(): sixteen steps up the hall's side to the landing, solid beneath; a rail round the well upstairs,
			// open where you step off at the top
			const int32 N = 16;
			for (int32 k = 0; k < N; ++k)
			{
				const float Va = Well.Z + (Well.W - Well.Z) * k / N, Vb = Well.Z + (Well.W - Well.Z) * (k + 1) / N, Top = S0 + HouseStorey * (k + 1) / N;
				RectBox(k % 2 ? StairWood : StairWoodLight, HouseRect(Well.X, Well.Y, Va, Vb), S0, Top);
			}
			RectBox(StairRail, HouseRect(Well.X - 0.04f, Well.X + 0.02f, Well.Z, StepOff), S1, S1 + 1.0f);
			RectBox(StairRail, HouseRect(Well.X, Well.Y, Well.Z - 0.035f, Well.Z + 0.035f), S1, S1 + 1.0f);
		}
		if (!bHouse && In->Lift)
		{
			// WLD-051 (storeys.js shell(), build()): a shaft from the ground to the top floor, on each floor its steel doors and a
			// lit call button on the side toward the room, and the lift house on the roof
			const FVector2D Lc = *In->Lift - C;
			const float Ix = In->Lift->X < C.X ? 1.f : -1.f;
			const FVector4f Shaft(Lc.X - 1.1f, Lc.X + 1.1f, Lc.Y - 1.1f, Lc.Y + 1.1f);
			RectBox(LiftShaft, Shaft, FloorTopOf(0), CeilOf(NFl - 1));
			for (int32 f = 0; f < NFl; ++f)
			{
				const float Y = FloorTopOf(f), Xd = Lc.X + Ix * 1.1f;
				for (const float Dz : { -1.f, 1.f })   // the two leaves
					RectBox(LiftSteel, FVector4f(FMath::Min(Xd, Xd + Ix * 0.03f), FMath::Max(Xd, Xd + Ix * 0.03f), Lc.Y + (Dz < 0 ? -0.55f : 0.01f), Lc.Y + (Dz < 0 ? -0.01f : 0.55f)), Y + 0.05f, Y + 2.2f);
				RectBox(IronColour, FVector4f(FMath::Min(Xd, Xd + Ix * 0.04f), FMath::Max(Xd, Xd + Ix * 0.04f), Lc.Y - 0.3f, Lc.Y + 0.3f), Y + 2.3f, Y + 2.45f);
				const FVector Button = At(FVector2D(Xd + Ix * 0.02f, Lc.Y + 0.75f), Y + 1.22f);
				Add(GlassGroup(), FTransform(FQuat::Identity, Button, FVector(0.04f, 0.1f, 0.15f)), FLinearColor(1.f, .8f, .5f));
			}
			RectBox(LiftShaft, Shaft, H, H + ParapetHeight + CopingHeight + 2.7f);   // the lift house, up through the roof
		}
		if (!bHouse)
		{
			const float Step = 0.19f;   // storeys.js STEP
			for (int32 f = 0; f + 1 < NFl; ++f)
			{
				const FBrixStairGeo::FFlight Fl = G.Flight(f);
				const float Y0 = FloorTopOf(f), Rise = FloorTopOf(f + 1) - Y0;
				const int32 N = FMath::CeilToInt(Rise / Step);
				const float Tr = (G.UHi - G.ULo) / N;
				for (int32 k = 0; k < N; ++k)   // each step solid down to the floor (a straight stair's upper ones thin, over the flight below)
				{
					const float Ua = Fl.Ub + Fl.Dir * k * Tr, Ub = Fl.Ub + Fl.Dir * (k + 1) * Tr, Top = Y0 + (k + 1) * Rise / N;
					RectBox(k % 2 ? StairWood : StairWoodLight, UvRect(Ua, Ub, Fl.Lane.X, Fl.Lane.Y), G.bStraight && f > 0 ? Top - 0.2f : Y0, Top);
				}
			}
			for (int32 f = 0; f < NFl; ++f)
			{
				if (!G.bStraight) RectBox(StairSpine, UvRect(G.ULo, G.UHi, G.Spine.X, G.Spine.Y), FloorTopOf(f), CeilOf(f));   // the spine wall
				else if (f > 0)
				{
					const float Edge = FMath::Abs(G.Open - G.Lane0.Y) < 1e-3f ? G.Open + StairHeadroom : G.Open - StairHeadroom;
					RectBox(StairRail, UvRect(G.ULo, G.UHi, Edge - 0.03f, Edge + 0.03f), FloorTopOf(f), FloorTopOf(f) + 1.0f);
				}
				if (f > 0 && !(G.bStraight && f < NFl - 1))   // a rail where the floor stops at the hole
				{
					const FBrixStairGeo::FFlight Fb = G.Flight(f - 1);
					RectBox(StairRail, UvRect(Fb.Ub - 0.03f, Fb.Ub + 0.03f, Fb.Lane.X, Fb.Lane.Y), FloorTopOf(f), FloorTopOf(f) + 1.0f);
				}
			}
		}
	}

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
				UStaticMesh* DoorMesh = bHollow ? Kit(bArch ? TEXT("SM_Kit_DoorArchOpen") : TEXT("SM_Kit_DoorPlainOpen"), Kit(bArch ? TEXT("SM_Kit_DoorArch") : TEXT("SM_Kit_DoorPlain")))
					: Kit(bArch ? TEXT("SM_Kit_DoorArch") : TEXT("SM_Kit_DoorPlain"));
				Ops.Add({ Sx, S0, Plan->DoorWidth, 2.4f, DoorMesh, nullptr, &DoorPaint, -1.f,
					Plan->DoorWidth / 1.3f, 2.4f / (bArch ? 2.5f : 2.3f), bHollow });
			}
		}
		else if (F.Len >= MinFacadeForOpenings)
		{
			const int32 Nb = FMath::Max(1, FMath::RoundToInt(F.Len / Bay));
			int32 DoorBay = -1;
			if (k == DoorSide && !bHollow)
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
		if (bHollow && !bHouse && !bShopFront && k == Dw.Side)
		{
			// the open doorway at the lot's door (BrixDoorwayOf), the ground storey's windows it would cut into left out
			const float Ga = Dw.Along - Dw.Width / 2 - 0.3f, Gb = Dw.Along + Dw.Width / 2 + 0.3f;
			Ops.RemoveAll([&](const FOpening& O) { return O.Z0 < 2.f && O.S + O.W / 2 > Ga && O.S - O.W / 2 < Gb; });
			const bool bCiv = Dw.Kind == FBrixDoorway::EKind::Civic, bArc = Dw.Kind == FBrixDoorway::EKind::Arch;
			UStaticMesh* Way = Kit(bCiv ? TEXT("SM_Kit_DoorCivicOpen") : bArc ? TEXT("SM_Kit_DoorArchOpen") : TEXT("SM_Kit_DoorPlainOpen"),
				Kit(bCiv ? TEXT("SM_Kit_DoorCivic") : bArc ? TEXT("SM_Kit_DoorArch") : TEXT("SM_Kit_DoorPlain")));
			Ops.Add({ Dw.Along, 0.f, Dw.Width, Dw.Height, Way, nullptr, &DoorPaint, -1.f, 1.f, 1.f, true });
		}
		if (L.AngleDeg == 0.f)   // no window where an advert or a hoarding covers this wall (dressing.js covers(), WLD-034)
			for (const FBrixProp& Bp : B.Props)
			{
				FBrixBoardSpan Bs;
				const TOptional<FBrixBoardWall> Wl = Bp.Set == TEXT("dress") ? BrixBoardWallOf(B, Bp, StoneBaseDistricts) : TOptional<FBrixBoardWall>();
				if (!Wl || Wl->Min != L.Min || !BrixBoardSpanOf(Bp, *Wl, Bs) || TEXT("nswe")[k] != Bs.Side) continue;
				const bool bNs = Bs.Side == 'n' || Bs.Side == 's';
				auto SOf = [&](double A) { const FVector2D P = bNs ? FVector2D(A, 0) : FVector2D(0, A); return FVector2D::DotProduct((bNs ? FVector2D(P.X - C.X, 0) : FVector2D(0, P.Y - C.Y)) - F.Mid * (bNs ? FVector2D(1, 0) : FVector2D(0, 1)), Along); };
				const double Sa0 = FMath::Min(SOf(Bs.A0), SOf(Bs.A1)), Sa1 = FMath::Max(SOf(Bs.A0), SOf(Bs.A1));
				Ops.RemoveAll([&](const FOpening& O) { const double Ya = O.Z0 + Base, Yb = O.Z0 + O.H + Base; return !O.bDoorway && O.S > Sa0 - .9 && O.S < Sa1 + .9 && Yb > Bs.Y0 - .2 && Ya < Bs.Y1 + .2; });
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
				if (!bHollow)
				{
					Piece(i == N / 2 ? TEXT("SM_Kit_ShopDoor") : TEXT("SM_Kit_ShopPane"), (A + B2) / 2, 0.f, FVector(B2 - A, 1, 1), FLinearColor::Gray, OpeningCullDistance);
					continue;
				}
				// an enterable shop: its door is the pane nearest the lot's door (BrixDoorwayOf), standing open; the rest are glass
				if (FMath::Abs((A + B2) / 2 - Dw.Along) < 1e-3f) { Piece(TEXT("SM_Kit_ShopDoorOpen"), (A + B2) / 2, 0.f, FVector(B2 - A, 1, 1), FLinearColor::Gray, OpeningCullDistance); continue; }
				Piece(TEXT("SM_Kit_ShopPane"), (A + B2) / 2, 0.f, FVector(B2 - A, 1, 1), FLinearColor::Gray, OpeningCullDistance);
				BlockAt(OnWall((A + B2) / 2, -t / 2), 0.f, ShopTop, Yaw, B2 - A + ShopPier, t);
			}
			Piece(TEXT("SM_Kit_ShopFascia"), 0.f, 0.f, FVector(Wd - 0.5f, 1, 1), FLinearColor::Gray, OpeningCullDistance);
			Name(L.Name, At(OnWall(0.f, 0.25f), (2.55f + ShopTop) / 2 + 0.02f), Yaw + Ang, NameSize, Wd - 1.2f);
			Awning(X0 + 0.6f, X1 - 0.6f, AwningZ, AwningOut, 0);
			for (int32 i = 0; i < CrateAt.Num(); ++i)
			{
				const float Sx = i < CrateAt.Num() / 2 ? X0 + CrateAt[i] : X1 - CrateAt[i];
				if (Sx > X0 + 0.5f && Sx < X1 - 0.5f)
				{
					const FVector Wp = At(OnWall(Sx, 0.f), 0.f) / CmPerM;
					Wrap(TEXT("crates"), FVector2D(Wp.X, Wp.Y), BlockSlabHeight, [&]() { Piece(TEXT("SM_Kit_ProduceCrate"), Sx, 0.f, FVector::OneVector, FLinearColor::Gray, ShutterCullDistance); });
				}
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
			for (float T : CafeTableAt) if (-F.Len / 2 + T < F.Len / 2 - 1.f)
			{
				const FVector Wp = At(OnWall(-F.Len / 2 + T, 0.f), 0.f) / CmPerM;
				Wrap(TEXT("table"), FVector2D(Wp.X, Wp.Y), BlockSlabHeight, [&]() { Piece(TEXT("SM_Kit_CafeTable"), -F.Len / 2 + T, 0.f, FVector::OneVector, FLinearColor::Gray, ShutterCullDistance); });
			}
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
			if (bHollow && k == 0)   // open at the doorway, so the step up to the door is the plinth's 0.3 m
			{
				const float Dsx = (float)FVector2D::DotProduct(FVector2D(Plan->DoorAt, F.Mid.Y) - F.Mid, Along);
				const float A0 = -F.Len / 2 - 0.05f, A1 = Dsx - Plan->DoorWidth / 2, B0 = Dsx + Plan->DoorWidth / 2, B1 = F.Len / 2 + 0.05f;
				if (A1 > A0) Box(Dark, OnWall((A0 + A1) / 2, 0.03f), 0.f, S0 + 0.3f, Yaw, A1 - A0, 0.06f);
				if (B1 > B0) Box(Dark, OnWall((B0 + B1) / 2, 0.03f), 0.f, S0 + 0.3f, Yaw, B1 - B0, 0.06f);
			}
			else Box(Dark, OnWall(0.f, 0.03f), 0.f, S0 + 0.3f, Yaw, F.Len + 0.1f, 0.06f);
			Box(HouseTrimStone, OnWall(0.f, 0.06f), S1 - 0.1f, S1 + 0.15f, Yaw, F.Len + 0.18f, 0.12f);
		}

		// the pieces
		for (const FOpening& O : Ops)
		{
			if (!O.Mesh) continue;
			const FTransform Tr(FQuat(FVector::UpVector, Yaw + Ang), At(OnWall(O.S, 0.f), O.MeshZ >= 0.f ? O.MeshZ : O.Z0), FVector(O.SX, 1, O.SZ));
			// a hollow storey's windows are clear, so the room looks out and the sun comes in (M_Clear)
			Add(Group(O.Mesh, false, OpeningCullDistance, bHollow && !O.bDoorway && O.Z0 < HollowTop), Tr, *O.Paint);
			if (bHollow && !O.bDoorway) BlockAt(OnWall(O.S, -t / 2), O.Z0, O.Z0 + O.H, Yaw, O.W, t);   // a window is not a way in
			if (bHollow && !O.bDoorway && O.Z0 < HollowTop)   // where the room's windows are, for its furniture (homes.js dropped())
			{
				const FVector Wa = At(OnWall(O.S - O.W / 2, -t), 0.f), Wb = At(OnWall(O.S + O.W / 2, -t), 0.f);
				int32 Fw = 0;
				while (Fw + 1 < NFl && O.Z0 >= FloorTopOf(Fw + 1) - 0.5f) ++Fw;
				WindowSpans.FindOrAdd(LotIndex * 64 + Fw).Add(FVector4f(Wa.X / CmPerM, Wa.Y / CmPerM, Wb.X / CmPerM, Wb.Y / CmPerM));
			}
			// lit by night (play.js windows(): each window of a lot lit by its hash against its kind's share, warm, a shopfront
			// always): a pane just behind the glass that glows after dusk (NightPanes)
			const bool bShopWin = O.Mesh == Kit(TEXT("SM_Kit_WinCafe"));
			if (!O.bDoorway && O.Mesh != Kit(TEXT("SM_Kit_Balcony")) && O.W < 2.6f && (bShopWin || SliceHash({ L.Id, FString::FromInt(k), FString::SanitizeFloat(O.Z0), FString::SanitizeFloat(O.S) }) < (LitShare.Contains(L.Kind) ? LitShare[L.Kind] : 0.3f)))
			{
				const bool bArched = O.Mesh == Kit(TEXT("SM_Kit_WinArch")) || O.Mesh == Kit(TEXT("SM_Kit_WinCafe")) || O.Mesh == Kit(TEXT("SM_Kit_WinCivic"));
				const float Top = O.Z0 + O.H - (bArched ? O.W / 2 : 0.f), Seed = (float)SliceHash({ L.Id, FString::SanitizeFloat(O.S), TEXT("warm") });
				const FLinearColor Warm = FMath::Lerp(NightWarm[0], NightWarm[1], Seed);
				if (Top - O.Z0 > 0.2f)
					Add(NightPaneGroup(), FTransform(FQuat(FVector::UpVector, Yaw + Ang), At(OnWall(O.S, -t + 0.055f), (O.Z0 + Top) * 0.5f), FVector(O.W * O.SX, 0.004f, Top - O.Z0)), Warm);   // just in front of the glass, behind its bars
			}
			if (O.Shutters) Add(Group(O.Shutters, false, ShutterCullDistance), FTransform(Tr.GetRotation(), Tr.GetLocation(), FVector(O.SX, 1, 1)), Shutter);
			++Openings;
		}

		// the wall between the openings (kit masonry and plaster_wall): bands from one opening's edge to the next, and a
		// piece that runs on unchanged through the next band is made one taller piece (a pier between two windows is one
		// box from the ground to the eaves, not one per storey)
		TArray<float> Breaks = { 0.f, H };
		if (StoneToOf[k] > 0.f && StoneToOf[k] < H) Breaks.Add(StoneToOf[k]);
		for (const FOpening& O : Ops) { Breaks.Add(O.Z0); Breaks.Add(FMath::Min(O.Z0 + O.H, H)); }
		Breaks.Sort();
		struct FRun { FVector2f Span; float Z0, Z1; bool bStone; };
		TArray<FRun> Open;
		auto Close = [&](const FRun& R)
		{
			if (R.bStone) BoxLook(StoneOf[k], OnWall((R.Span.X + R.Span.Y) / 2, -t / 2), R.Z0, R.Z1, Yaw, R.Span.Y - R.Span.X, t);
			else Box(PlasterOf[k], OnWall((R.Span.X + R.Span.Y) / 2, -t / 2), R.Z0, R.Z1, Yaw, R.Span.Y - R.Span.X, t);
			// inside a hollow storey the wall is plastered in the storey's colour (BUILD-019)
			for (int32 f = 0; bHollow && f < NFl; ++f)
			{
				const float Za = FMath::Max(R.Z0, FloorTopOf(f)), Zb = FMath::Min(R.Z1, CeilOf(f));
				if (Zb - Za > 0.01f) Box(ColourOf(In ? In->WallColour : TArray<FVector3f>(), f, InWall), OnWall((R.Span.X + R.Span.Y) / 2, -t - LiningDepth / 2), Za, Zb, Yaw, R.Span.Y - R.Span.X, LiningDepth);
			}
		};
		for (int32 b = 0; b + 1 < Breaks.Num(); ++b)
		{
			const float Za = Breaks[b], Zb = Breaks[b + 1];
			if (Zb - Za < 1e-3f) continue;
			TArray<FVector2f> Cuts;
			for (const FOpening& O : Ops) if (O.Z0 < Zb - 1e-3f && O.Z0 + O.H > Za + 1e-3f) Cuts.Add(FVector2f(O.S - O.W / 2, O.S + O.W / 2));
			const bool bStone = (Za + Zb) * 0.5f < StoneToOf[k];
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
		if (!bHouse && StoneToOf[k] > 0.f && StoneToOf[k] < H - 0.5f) Box(Dark, OnWall(0.f, BandDepth / 2), StoneToOf[k], StoneToOf[k] + BandHeight, Yaw, F.Len + 2 * BandDepth, BandDepth);
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
		if (bAllStone) Top = StoneOf[0]; else Top.Colour = Plaster;
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
	LotIndex = 0;
	if (!Today.IsValid())   // how the gardens look today (Game/Data/streets.json, build_play.py)
	{
		Today = MakeShared<FBrixStreetsToday>();
		FString Err;
		if (!LoadBrixStreetsToday(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/streets.json")), *Today, Err)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: %s (run Tools/worldgen/build_play.py)"), *Err);
		if (Today->LawnTones.Num() < 3) Today->LawnTones = { FVector3d(.18, .27, .13), FVector3d(.29, .34, .19), FVector3d(.42, .39, .21) };
	}
	if (!FacetsMat) FacetsMat = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Facets.M_Facets"));
	if (!FacetsMat) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: /Game/Greybox/M_Facets is missing (run Tools/unreal/make_facets_material.py)")); FacetsMat = Greybox; }
	if (!LawnMat)   // a lawn's own pattern (material 24; ART-001): patches, mowing stripes, clover, daisies, from vegetation.json "lawn"
	{
		if (UMaterialInterface* LM = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Lawn.M_Lawn")))
		{
			LawnMat = UMaterialInstanceDynamic::Create(LM, this);
			auto V = [](const FVector3d& c, double w) { return FLinearColor((float)c.X, (float)c.Y, (float)c.Z, (float)w); };
			LawnMat->SetVectorParameterValue(TEXT("T0"), V(Today->LawnTones[0], 1));
			LawnMat->SetVectorParameterValue(TEXT("T1"), V(Today->LawnTones[1], 1));
			LawnMat->SetVectorParameterValue(TEXT("T2"), V(Today->LawnTones[2], 1));
			LawnMat->SetVectorParameterValue(TEXT("Lawn"), FLinearColor((float)Today->PatchM, (float)Today->StripeM, (float)Today->Stripe, (float)Today->Clover));
			LawnMat->SetVectorParameterValue(TEXT("Daisy"), V(Today->DaisyColor, Today->Daisies));
		}
		else UE_LOG(LogTemp, Warning, TEXT("Brix Town: /Game/Greybox/M_Lawn is missing (run Tools/unreal/make_lawn_material.py); the lawns are one even green"));
	}
	BlockIndex = -1;
	bNoPlants = FParse::Param(FCommandLine::Get(), TEXT("BrixNoPlants"));   // to compare: the town without its planting and open ground
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
		++BlockIndex;
		const bool bGreen = GreenTemplates.Contains(B.Template);
		if (BrixIsWildTemplate(B.Template) && !bNoPlants)   // the open ground's leaf litter or grass, and its tracks (WLD-056)
		{
			TArray<FBrixShape> S;
			FBrixGround G;
			G.Wild = BlockSlabHeight + WildUp;
			BrixGreenGroundShapes(B, G, *Today, S);
			Shapes(S);
		}
		const FTransform Slab(FQuat::Identity, ToWorld(B.Origin + B.Extent * 0.5, BlockSlabHeight * 0.5f), FVector(B.Extent.X, B.Extent.Y, BlockSlabHeight));
		if (bGreen) Add(Boxes, Slab, GreenColour);
		else Add(Boxes, Slab, Surface(B.Pavement, TownPave));   // the block's paving (ADR-0017)
		for (const FBrixLot& L : B.Lots)
		{
			ON_SCOPE_EXIT { ++LotIndex; };
			if (L.IsOpen())
			{
				const FTransform Top(FQuat(FVector::UpVector, FMath::DegreesToRadians(L.AngleDeg)), ToWorld(L.Centre(), BlockSlabHeight + 0.02f), FVector(L.Size().X, L.Size().Y, 0.04f));
				if (L.Kind == TEXT("garden") && bNoPlants) Add(Boxes, Top, GreenColour);
				else if (L.Kind == TEXT("garden"))   // its lawn (ART-001): the grass, its longer edge, the tufts the mower missed
				{
					TArray<FBrixShape> S;
					BrixLawnShapes(L.Min.X, L.Min.Y, L.Max.X, L.Max.Y, LawnUp, BrixSlice::Grass * 1.08, B, true, *Today, S);
					Shapes(S);
				}
				else Add(Boxes, Top, Surface(L.Surface, TownPave));   // the market's setts, a tram stop's slabs
				continue;
			}
			if (OwnBuilt.Contains(L.Id)) continue;   // a building drawn by its own actor (Eastgate Mall: ABrixMall)
			Recording = &LotParts.FindOrAdd(LotIndex);
			BuildLot(L, B);
			Recording = nullptr;
		}
		if (!Today->Trees.Num()) for (const FBrixTree& T : B.Trees)   // (no Game/Data/streets.json trees: a trunk and a ball)
		{
			Wrap(TEXT("tree"), T.Pos, 0.f, [&]()
			{
				Add(Trunks, FTransform(FQuat::Identity, ToWorld(T.Pos, TreeTrunkHeight * 0.5f), FVector(TreeTrunkWidth, TreeTrunkWidth, TreeTrunkHeight)), TrunkColour);
				Add(Crowns, FTransform(FQuat::Identity, ToWorld(T.Pos, TreeTrunkHeight + TreeCrownWidth * 0.4f), FVector(TreeCrownWidth)), CrownColour);
			});
		}
		for (const FBrixProp& P : B.Props) Wrap(P.Kind, P.Pos, BlockSlabHeight, [&]() { BuildProp(P, B); });   // street things a car can break (CITY-072)
	}
	BuildTreesAndRoads(City);
	BuildRoofMap(City);
	// each block's faces (tufts, tracks, ponds; each square's tree crowns and road markings) as one mesh made now
	int32 FacetTris = 0;
	for (TPair<int32, FFacets>& F : Facets)
	{
		if (!F.Value.P.Num()) continue;
		UStaticMesh* M = FacetMesh(FString::Printf(TEXT("Facets_%d"), F.Key), F.Value.P, F.Value.N, F.Value.C, &F.Value.Lawn);
		UStaticMeshComponent* SC = NewObject<UStaticMeshComponent>(this, *FString::Printf(TEXT("FacetsOf_%d"), F.Key));
		SC->SetupAttachment(RootComponent);
		SC->SetStaticMesh(M);
		SC->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SC->SetCullDistance((F.Key >= TreeKey ? TreeCullDistance : FacetCullDistance) * CmPerM);
		SC->bAffectDistanceFieldLighting = false;
		SC->SetAffectDynamicIndirectLighting(false);
		SC->RegisterComponent();
		AddInstanceComponent(SC);
		FacetTris += F.Value.P.Num() / 3;
		if (F.Value.Owner.ContainsByPredicate([](int32 o) { return o != INDEX_NONE; })) KeptFacets.Add(F.Key, { MoveTemp(F.Value), SC });
	}
	Facets.Empty();
	// Scales are in metres: the shapes and the kit's pieces are 1 m to a unit of scale, so a scale is a size.
	const int32 Instances = FlushPending();
	KeptCity = &City;
	CityData = nullptr;
	{   // what a car can break, by kind (CITY-072)
		TMap<FString, int32> ByKind;
		for (const FBrixBreakable& Bk : Breakables) ++ByKind.FindOrAdd(Bk.Kind);
		ByKind.KeySort([](const FString& A, const FString& B) { return A < B; });
		FString List;
		for (const TPair<FString, int32>& K : ByKind) List += FString::Printf(TEXT("%s%s %d"), List.IsEmpty() ? TEXT("") : TEXT(", "), *K.Key, K.Value);
		UE_LOG(LogTemp, Display, TEXT("Brix Town: %d things a car can break: %s"), Breakables.Num(), *List);
	}
	UE_LOG(LogTemp, Display, TEXT("Brix Town: built %s from data: %d blocks, %d buildings (%d you can walk into), %d openings, %d instances in %d groups, %d faces in the blocks' facet meshes"),
		*City.Name, City.Blocks.Num(), Buildings, Hollow, Openings, Instances, Parts.Num(), FacetTris);
}

// smash.js wrap(): what Fn adds is one thing a car can break, if its kind breaks; its footprint near the ground and its
// top worked out from its boxes (each box 1 m to a unit of scale, turned about z)
void ABrixTownActor::Wrap(const FString& Kind, const FVector2D& Pos, float Ground, TFunctionRef<void()> Fn)
{
	if (Wrapping || !Breakable.Contains(Kind)) { Fn(); return; }   // inside another breakable: part of it
	FBrixBreakable B;
	B.PropKind = Kind; B.Kind = Kind; B.Pos = Pos; B.Ground = Ground; B.Top = Ground;
	const int32 Lights0 = LampLights.Num();
	Wrapping = &B;
	Fn();
	Wrapping = nullptr;
	if (!B.Parts.Num())
	{
		if (FFacets* F = Facets.Find(B.FacetKey)) for (int32& o : F->Owner) if (o == Breakables.Num()) o = INDEX_NONE;
		return;
	}
	if (LampLights.Num() > Lights0) B.Light = Lights0;
	for (int32 Pass = 0; Pass < 2 && !B.Box.bIsValid; ++Pass)
		for (const FBrixBreakable::FPart& P : B.Parts)
		{
			const FVector L = P.At.GetLocation() / CmPerM, S = P.At.GetScale3D();
			const float Bottom = (float)(L.Z - S.Z / 2);
			B.Top = FMath::Max(B.Top, (float)(L.Z + S.Z / 2));
			if (!Pass && Bottom >= Ground + 1.3f) continue;
			for (const double cx : { -.5, .5 }) for (const double cy : { -.5, .5 })
			{
				const FVector C = P.At.GetRotation().RotateVector(FVector(cx * S.X, cy * S.Y, 0));
				B.Box += FVector2D(L.X + C.X, L.Y + C.Y);
			}
		}
	Breakables.Add(MoveTemp(B));
}

void ABrixTownActor::BuildRoofMap(const FBrixCity& City)   // WLD-037, WLD-070: no rain or snow under a roof
{
	if (RoofCell <= 0.f) return;   // (RoofCell=0: the wet and the snow everywhere, as before, to compare)
	const double T0 = FPlatformTime::Seconds();
	const FBrixRoofMap M = BrixRoofMap(City, RoofCell, .5, [&](const FBrixLot& L)
	{
		if (OwnBuilt.Contains(L.Id)) return 0.0;   // (Eastgate Mall draws its own)
		const double H = L.House.IsSet() ? HousePlinth + 2 * HouseStorey : FMath::Max(1, L.Floors) * StoreyHeight;   // as BuildLot's H
		return BlockSlabHeight + H - RoofUnder;
	});
	if (!M.W) return;
	RoofTex = UTexture2D::CreateTransient(M.W, M.H, PF_G8, TEXT("BrixRoofs"));
	if (!RoofTex) return;
	RoofTex->SRGB = false;
	RoofTex->Filter = TF_Nearest;
	RoofTex->AddressX = TA_Clamp;
	RoofTex->AddressY = TA_Clamp;
	RoofTex->NeverStream = true;
	void* Data = RoofTex->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
	FMemory::Memcpy(Data, M.Px.GetData(), M.Px.Num());
	RoofTex->GetPlatformData()->Mips[0].BulkData.Unlock();
	RoofTex->UpdateResource();
	RoofAt = FLinearColor((float)M.Origin.X, (float)M.Origin.Y, (float)(1.0 / (M.W * M.Cell)), (float)(1.0 / (M.H * M.Cell)));
	SetWet(0.f, 0.f, 0.f, 0.f, 0.f);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the roof map, %d x %d cells of %.2f m, in %.2f s"), M.W, M.H, M.Cell, FPlatformTime::Seconds() - T0);
}

void ABrixTownActor::SetWet(float Dark, float Sheen, float Puddles, float Ripples, float Snow)
{
	TArray<UMaterialInstanceDynamic*> All;
	if (OwnColour) All.Add(OwnColour);
	for (const TPair<uint32, TObjectPtr<UMaterialInstanceDynamic>>& C : Colours) if (C.Value) All.Add(C.Value);
	for (UMaterialInstanceDynamic* M : All)
	{
		M->SetScalarParameterValue(TEXT("WetDark"), Dark);
		M->SetScalarParameterValue(TEXT("WetSheen"), Sheen);
		M->SetScalarParameterValue(TEXT("Puddles"), Puddles);
		M->SetScalarParameterValue(TEXT("Ripples"), Ripples);
		M->SetScalarParameterValue(TEXT("Snow"), Snow);
		if (RoofTex) { M->SetTextureParameterValue(TEXT("Roofs"), RoofTex); M->SetVectorParameterValue(TEXT("RoofsAt"), RoofAt); }
	}
}

int32 ABrixTownActor::FlushPending()   // the instances gathered per group, added in one go, their looks in their custom data
{
	int32 Instances = 0;
	for (TPair<UInstancedStaticMeshComponent*, FPending>& P : Pending)
	{
		UInstancedStaticMeshComponent* G = P.Key;
		const int32 First = G->GetInstanceCount();
		G->AddInstances(P.Value.At, false);
		Instances += P.Value.At.Num();
		for (FBrixBreakable& Bk : Breakables) for (FBrixBreakable::FPart& Pt : Bk.Parts) if (Pt.G == G && !Pt.bPlaced) { Pt.Index += First; Pt.bPlaced = true; }   // pending index to the instance's
		for (auto& Lp : LotParts) for (FBrixBreakable::FPart& Pt : Lp.Value) if (Pt.G == G && !Pt.bPlaced) { Pt.Index += First; Pt.bPlaced = true; }
		if (G->NumCustomDataFloats == 0) { G->MarkRenderStateDirty(); continue; }   // the unseen blockers
		const bool bPattern = G->NumCustomDataFloats >= 12;
		for (int32 i = 0; i < P.Value.Look.Num(); ++i)
		{
			const FLook& L = P.Value.Look[i];
			const float Turn = FMath::DegreesToRadians((float)P.Value.At[i].GetRotation().Rotator().Yaw);   // the courses run along the box
			if (bPattern) G->SetCustomData(First + i, { L.Colour.R, L.Colour.G, L.Colour.B, L.A.X, L.A.Y, L.A.Z, L.A.W, L.B.X, L.B.Y, L.B.Z, L.B.W, Turn }, false);
			else G->SetCustomData(First + i, { L.Colour.R, L.Colour.G, L.Colour.B }, false);
		}
		G->MarkRenderStateDirty();
	}
	Pending.Empty();
	return Instances;
}

// BUILD-005 (play.js repaint()): the building drawn again from a design, a remodel being tried or one paid for. Its old
// instances are folded away under the ground and it is built afresh, so the rest of the town is untouched.
void ABrixTownActor::Repaint(int32 Lot, const TSharedPtr<FJsonObject>& Design, float CutM)
{
	if (!KeptCity) return;
	const double Began = FPlatformTime::Seconds();
	TArray<FBrixBreakable::FPart>* Old = LotParts.Find(Lot);
	if (Old)
	{
		// each group's instances of this building in runs that follow on from one another, moved a run at a time: moved one by
		// one, a group of the whole town's boxes was gone over again for every instance (seconds for one building)
		TMap<UInstancedStaticMeshComponent*, TArray<TPair<int32, FTransform>>> By;
		for (const FBrixBreakable::FPart& P : *Old)
		{
			if (!P.bPlaced) continue;
			FTransform T = P.At;
			T.SetLocation(FVector(T.GetLocation().X, T.GetLocation().Y, -6000.0));
			T.SetScale3D(FVector(.001));
			By.FindOrAdd(P.G).Add({ P.Index, T });
		}
		for (TPair<UInstancedStaticMeshComponent*, TArray<TPair<int32, FTransform>>>& G : By)
		{
			G.Value.Sort([](const TPair<int32, FTransform>& A, const TPair<int32, FTransform>& B) { return A.Key < B.Key; });
			for (int32 a = 0; a < G.Value.Num();)
			{
				TArray<FTransform> Run;
				int32 b = a;
				for (; b < G.Value.Num() && G.Value[b].Key == G.Value[a].Key + (b - a); ++b) Run.Add(G.Value[b].Value);
				G.Key->BatchUpdateInstancesTransforms(G.Value[a].Key, Run, false, false, true);
				a = b;
			}
			G.Key->MarkRenderStateDirty();
		}
		Old->Reset();
	}
	if (Design) DesignOver.Add(Lot, Design); else DesignOver.Remove(Lot);
	int32 k = 0;
	for (const FBrixBlock& B : KeptCity->Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			if (k++ != Lot) continue;
			if (OwnBuilt.Contains(L.Id)) return;   // drawn by its own actor (Eastgate Mall)
			CityData = KeptCity;
			LotIndex = Lot;
			Recording = &LotParts.FindOrAdd(Lot);
			CutCm = CutM >= 0.f ? CutM * CmPerM : -1.f;
			BuildLot(L, B);
			CutCm = -1.f;
			Recording = nullptr;
			FlushPending();
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %s drawn again in %.2f s%s"), *L.Id, FPlatformTime::Seconds() - Began, CutM >= 0.f ? *FString::Printf(TEXT(", cut open %.2f m up"), CutM) : TEXT(""));
			CityData = nullptr;
			return;
		}
}

void ABrixTownActor::SetBroken(int32 b, bool bBroken)
{
	if (!Breakables.IsValidIndex(b) || Breakables[b].bBroken == bBroken) return;
	FBrixBreakable& B = Breakables[b];
	B.bBroken = bBroken;
	TSet<UInstancedStaticMeshComponent*> Touched;
	for (const FBrixBreakable::FPart& P : B.Parts)
	{
		FTransform T = P.At;
		if (bBroken) { T.SetLocation(FVector(T.GetLocation().X, T.GetLocation().Y, -6000.0)); T.SetScale3D(FVector(.001)); }   // folded away under the ground
		P.G->UpdateInstanceTransform(P.Index, T, false, false, true);
		Touched.Add(P.G);
	}
	for (UInstancedStaticMeshComponent* G : Touched) G->MarkRenderStateDirty();
	if (B.FacetKey != INDEX_NONE) RemakeFacets(B.FacetKey);   // a tree's crown
	if (LampLights.IsValidIndex(B.Light)) LampLights[B.Light].W = bBroken ? -FMath::Abs(LampLights[B.Light].W) : FMath::Abs(LampLights[B.Light].W);   // a broken lamp goes out
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
	if (k == TEXT("plant") || P.Set == TEXT("wild"))   // garden planting (CITY-017, WLD-040) and the open ground's own things (WLD-056)
	{
		if (bNoPlants) return;
		FBrixGround G;
		G.Here = P.Set == TEXT("wild") ? BlockSlabHeight + WildUp : g;
		G.Wild = BlockSlabHeight + WildUp;
		TArray<FBrixShape> S;
		if (k == TEXT("plant")) BrixPlantShapes(P, G, *Today, S);
		else if (!BrixWildShapes(P, G, S)) S.Reset();
		if (S.Num() || k == TEXT("plant")) { Shapes(S); return; }
	}
	if (P.Set == TEXT("dress"))   // the street dressing drawn as dressing.js draws it (WLD-028, WLD-031, WLD-032, WLD-034)
	{
		TArray<FBrixShape> S;
		const TOptional<FBrixBoardWall> Wall = BrixBoardWallOf(Blk, P, StoneBaseDistricts);
		if (BrixDressShapes(P, Blk, g, *Today, S) || BrixMoreDressShapes(P, Blk, g, *Today, Wall.GetPtrOrNull(), S)) { Shapes(S); return; }
	}
	if (k == TEXT("arch"))   // a stone arch across an alley (play.js prop()), and crates by some
	{
		TArray<FBrixShape> S;
		BrixArchShapes(P, Blk, g, S);
		Shapes(S);
		return;
	}
	const bool ax = P.Along != TEXT("z");
	const float Oct = UE_PI / 8;
	auto glass = [&](float x0, float y0, float z0, float x1, float y1, float z1)   // lit by night (ABrixLamps)
	{
		Add(GlassGroup(), FTransform(FQuat::Identity, ToWorld(FVector2D((x0 + x1) / 2, (z0 + z1) / 2), (y0 + y1) / 2),
			FVector(FMath::Abs(x1 - x0), FMath::Abs(z1 - z0), FMath::Abs(y1 - y0))), Lantern);
	};
	if (k == TEXT("lamp"))   // a lantern on a post; its light is real by night (the slice's lights: power 1.3, 3.5 m up)
	{
		box(x - .17f, g, z - .17f, x + .17f, g + .55f, z + .17f, StoneDark); box(x - .08f, g + .55f, z - .08f, x + .08f, g + 3.3f, z + .08f, Metal);
		glass(x - .2f, g + 3.3f, z - .2f, x + .2f, g + 3.78f, z + .2f);
		LampLights.Add(FVector4f(x, g + 3.5f, z, 1.3f)); box(x - .27f, g + 3.78f, z - .27f, x + .27f, g + 3.88f, z + .27f, Metal, true);
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
		glass(lx - .11f, h - .12f, lz - .11f, lx + .11f, h + .26f, lz + .11f);
		LampLights.Add(FVector4f(x + P.Out.X * .5f, h, z + P.Out.Y * .5f, P.Power / 120.f));   // (the slice: power / 120) box(lx - .15f, h + .26f, lz - .15f, lx + .15f, h + .31f, lz + .15f, Metal, true);
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
		TArray<FBrixShape> S;
		BrixCratesShapes(x + 1.9, tz, g, TEXT("st") + BrixSlice::Js(P.Pos.X) + BrixSlice::Js(P.Pos.Y), S);
		Shapes(S);
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

// The slice's drawing calls (BrixWorld BrixStreets) as the town's own shapes. Boxes are instances of the box (no collision:
// what is in the way comes as the calls' solids), the small ones fading out with the garden's small things; a crown is an
// instance of one of the dome meshes, turned by its seed; single faces go into the block's facet mesh.
void ABrixTownActor::Shapes(const TArray<FBrixShape>& S)
{
	UStaticMesh* BoxMesh = Kit(TEXT("SM_Kit_Box"), Cube);
	UInstancedStaticMeshComponent* Open = Group(BoxMesh, false), * Fine = Group(BoxMesh, false, GardenCullDistance);
	for (UInstancedStaticMeshComponent* G : { Open, Fine }) if (G->bAffectDistanceFieldLighting) { G->bAffectDistanceFieldLighting = false; G->SetAffectDynamicIndirectLighting(false); }
	auto Lc = [](const FVector3d& C) { return FLinearColor((float)C.X, (float)C.Y, (float)C.Z); };
	auto P3 = [](double x, double y, double z) { return FVector3d(x, y, z); };
	for (int32 s = 0; s < S.Num(); ++s)
	{
		const FBrixShape& Sh = S[s];
		if (!Sh.WrapKind.IsEmpty() && !Wrapping)   // crates by an arch: a thing a car can break on its own (smash.js wrap())
		{
			int32 e = s;
			while (e < S.Num() && S[e].WrapKind == Sh.WrapKind && S[e].WrapAt == Sh.WrapAt) ++e;
			TArray<FBrixShape> Run(S.GetData() + s, e - s);
			for (FBrixShape& R : Run) R.WrapKind.Reset();
			Wrap(Sh.WrapKind, Sh.WrapAt, BlockSlabHeight, [&]() { Shapes(Run); });
			s = e - 1;
			continue;
		}
		const TArray<double, TInlineAllocator<16>>& A = Sh.A;
		switch (Sh.Kind)
		{
		case FBrixShape::Box:
		{
			const FVector Size(FMath::Abs(A[3] - A[0]), FMath::Abs(A[5] - A[2]), FMath::Abs(A[4] - A[1]));
			Add(Size.GetMax() <= FineSize ? Fine : Open, FTransform(FQuat::Identity, ToWorld(FVector2D((A[0] + A[3]) / 2, (A[2] + A[5]) / 2), (float)((A[1] + A[4]) / 2)), Size), Lc(Sh.Col));
			break;
		}
		case FBrixShape::YBox:
		{
			const FVector Size(A[3], A[5], A[4]);
			Add(Size.GetMax() <= FineSize ? Fine : Open, FTransform(FQuat(FVector::UpVector, -A[6]), ToWorld(FVector2D(A[0], A[2]), (float)(A[1] + A[4] / 2)), Size), Lc(Sh.Col));
			break;
		}
		case FBrixShape::Flat:   // a level rectangle: two faces of the block's facet mesh (plain grass, earth and water, not
			// plaster), or a thin box whose top is at its height when it is part of something that breaks
			if (!Wrapping)
			{
				const FVector3d q0(A[0], A[4], A[1]), q1(A[0], A[4], A[3]), q2(A[2], A[4], A[3]), q3(A[2], A[4], A[1]);
				Facet(q0, q1, q2, FVector3d(0, 1, 0), Sh.Col, Sh.bLawn); Facet(q0, q2, q3, FVector3d(0, 1, 0), Sh.Col, Sh.bLawn);
				break;
			}
			Add(Open, FTransform(FQuat::Identity, ToWorld(FVector2D((A[0] + A[2]) / 2, (A[1] + A[3]) / 2), (float)(A[4] - .01)), FVector(FMath::Abs(A[2] - A[0]), FMath::Abs(A[3] - A[1]), .02)), Lc(Sh.Col));
			break;
		case FBrixShape::Dome:   // (the mesh's shades are kept under 1, so its colour is raised to match)
		{
			const int32 V = FMath::Abs((int32)FMath::FloorToDouble(A[5])) % FMath::Max(1, DomeVariants);
			Add(DomeGroup(V), FTransform(FQuat(FVector::UpVector, A[5]), ToWorld(FVector2D(A[0], A[2]), (float)A[1]), FVector(A[3], A[3], A[4])), Lc(Sh.Col * 1.4));
			break;
		}
		case FBrixShape::Quad:
		{
			const FVector3d p0 = P3(A[0], A[1], A[2]), p1 = P3(A[3], A[4], A[5]), p2 = P3(A[6], A[7], A[8]), p3 = P3(A[9], A[10], A[11]), h = P3(A[12], A[13], A[14]);
			Facet(p0, p1, p2, h, Sh.Col);
			if (!p2.Equals(p3, 1e-9) && !p3.Equals(p0, 1e-9)) Facet(p0, p2, p3, h, Sh.Col);   // (a wedge ends where it starts: one face)
			break;
		}
		case FBrixShape::Ribbon:   // play.js ribbon(): a strip along the line, each piece overlapping the next a little
		{
			const double hw = A[0] / 2, y = A[1];
			for (int32 k = 0; k + 1 < Sh.Pts.Num(); ++k)
			{
				const FVector2D a = Sh.Pts[k], b = Sh.Pts[k + 1];
				const double L = FVector2D::Distance(a, b);
				if (L < 1e-3) continue;
				const double nx = -(b.Y - a.Y) / L * hw, nz = (b.X - a.X) / L * hw, e = FMath::Min(.4, L / 2), ux = (b.X - a.X) / L * e, uz = (b.Y - a.Y) / L * e;
				const FVector3d q0(a.X - nx - ux, y, a.Y - nz - uz), q1(a.X + nx - ux, y, a.Y + nz - uz), q2(b.X + nx + ux, y, b.Y + nz + uz), q3(b.X - nx + ux, y, b.Y - nz + uz);
				Facet(q0, q1, q2, FVector3d(0, 1, 0), Sh.Col); Facet(q0, q2, q3, FVector3d(0, 1, 0), Sh.Col);
			}
			break;
		}
		case FBrixShape::Disc:   // play.js disc(): 18 wedges
			for (int32 k = 0; k < 18; ++k)
			{
				const double a = 2 * UE_DOUBLE_PI * k / 18, b = 2 * UE_DOUBLE_PI * (k + 1) / 18;
				Facet(P3(A[0], A[3], A[1]), P3(A[0] + FMath::Cos(a) * A[2], A[3], A[1] + FMath::Sin(a) * A[2]), P3(A[0] + FMath::Cos(b) * A[2], A[3], A[1] + FMath::Sin(b) * A[2]), FVector3d(0, 1, 0), Sh.Col);
			}
			break;
		case FBrixShape::Label:   // painted letters facing the way the slice's lettering faces
			Name(Sh.Text, ToWorld(FVector2D(A[0], A[2]), (float)A[1]), FMath::Atan2(A[4], A[3]), (float)A[5] * 1.6f, 1.5f, true);
			break;
		case FBrixShape::OBox:   // Mesh.obox(): a box placed by the slice's matrix (its axes swapped to Unreal's: x, z, y)
		{
			const double* m = A.GetData();
			const FVector Ax(m[0], m[2], m[1]), Ay(m[8], m[10], m[9]), Az(m[4], m[6], m[5]);   // the box's x, its z and its y
			FBrixM4 M;
			for (int32 j = 0; j < 16; ++j) M.M[j] = m[j];
			const FVector3d C = M.Pt(A[16], A[17], A[18]);
			FTransform Tr(FMatrix(Ax, Ay, Az, FVector::ZeroVector));
			Tr.SetLocation(ToWorld(FVector2D(C.X, C.Z), (float)C.Y));
			Tr.SetScale3D(FVector(A[19], A[21], A[20]));
			Add(FVector(A[19], A[20], A[21]).GetMax() <= FineSize ? Fine : Open, Tr, Lc(Sh.Col));
			break;
		}
		case FBrixShape::Solid:
			Add(BlockerGroup(), FTransform(FQuat::Identity, ToWorld(FVector2D((A[0] + A[3]) / 2, (A[2] + A[5]) / 2), (float)((A[1] + A[4]) / 2)),
				FVector(FMath::Abs(A[3] - A[0]), FMath::Abs(A[5] - A[2]), FMath::Abs(A[4] - A[1]))), FLinearColor::White);
			break;
		}
	}
}

// A block square's facet mesh made again without the faces of the things in it that are broken (a tree's crown)
void ABrixTownActor::RemakeFacets(int32 Key)
{
	FKeptFacets* K = KeptFacets.Find(Key);
	if (!K || !K->C) return;
	TArray<FVector3f> P, N;
	TArray<FLinearColor> Col;
	TArray<uint8> Lawn;
	for (int32 t = 0; t < K->F.N.Num(); ++t)
	{
		const int32 o = K->F.Owner[t];
		if (Breakables.IsValidIndex(o) && Breakables[o].bBroken) continue;
		P.Append({ K->F.P[3 * t], K->F.P[3 * t + 1], K->F.P[3 * t + 2] });
		N.Add(K->F.N[t]);
		Col.Add(K->F.C[t]);
		Lawn.Add(K->F.Lawn.IsValidIndex(t) ? K->F.Lawn[t] : 0);
	}
	UStaticMesh* Old = K->C->GetStaticMesh();
	K->C->SetStaticMesh(FacetMesh(FString::Printf(TEXT("Facets_%d_%d"), Key, ++K->Made), P, N, Col, &Lawn));
	if (Old) Made.Remove(Old);
}

// The trees (play.js tree(): WLD-056) and the road network (roads.js: CITY-043, CITY-045, CITY-091), each block square's
// faces in a mesh of its own; a tree out of the woods a thing of its own a car can break (a fallen one: a log)
void ABrixTownActor::BuildTreesAndRoads(const FBrixCity& City)
{
	const double P = City.Pitch, Wild = BlockSlabHeight + WildUp;
	auto BlockAt = [&](double x, double z) -> const FBrixBlock*
	{
		for (const FBrixBlock& B : City.Blocks) if (x > B.Origin.X && z > B.Origin.Y && x < B.Origin.X + B.Extent.X && z < B.Origin.Y + B.Extent.Y) return &B;
		return nullptr;
	};
	auto Ground = [&](double x, double z)   // the town's ground: the open ground, a garden's lawn, the paving, the street
	{
		const FBrixBlock* B = BlockAt(x, z);
		if (!B) return 0.0;
		if (BrixIsWildTemplate(B->Template)) return Wild;
		for (const FBrixLot& L : B->Lots) if (L.Kind == TEXT("garden") && x >= L.Min.X && x <= L.Max.X && z >= L.Min.Y && z <= L.Max.Y) return (double)LawnUp;
		return (double)BlockSlabHeight;
	};
	TArray<FBrixLawn> Lawns = BrixLawnsOf(City.Blocks, P, Wild, BlockSlabHeight, LawnUp);
	{   // (a terrace's back gardens are its lots' own yards here, not a lawn: no ring there)
		TArray<FBrixLawn> Rows;
		for (const FBrixBlock& B : City.Blocks) if (B.Template == TEXT("rows")) Rows.Add({ B.Origin.X + 2, B.Origin.Y + 16.5, B.Origin.X + B.Extent.X - 2, B.Origin.Y + B.Extent.X - 16.5, (double)BlockSlabHeight });
		Lawns.RemoveAll([&](const FBrixLawn& L) { return Rows.ContainsByPredicate([&](const FBrixLawn& R) { return R.X0 == L.X0 && R.Z0 == L.Z0 && R.X1 == L.X1 && R.Z1 == L.Z1 && R.Y == L.Y; }); });
	}
	const int32 Was = BlockIndex;
	int32 Trees = 0;
	for (const FBrixTreeToday& T : Today->Trees)
	{
		const int32 i = (int32)FMath::FloorToDouble(T.X / P), j = (int32)FMath::FloorToDouble(T.Z / P);
		const FBrixBlock* B = nullptr;   // play.js blockOf(): by the squares a block spans
		for (const FBrixBlock& Bk : City.Blocks)
			if (i >= (int32)FMath::FloorToDouble(Bk.Origin.X / P) && i <= (int32)FMath::FloorToDouble((Bk.Origin.X + Bk.Extent.X) / P)
				&& j >= (int32)FMath::FloorToDouble(Bk.Origin.Y / P) && j <= (int32)FMath::FloorToDouble((Bk.Origin.Y + Bk.Extent.Y) / P)) { B = &Bk; break; }
		const bool bLite = B && B->Template == TEXT("woods");   // thousands, standing close: fewer facets, and no car gets in among them
		const double g = Ground(T.X, T.Z);
		BlockIndex = TreeKey + i * 1000 + j;
		TArray<FBrixShape> S;
		BrixTreeShapes(T, g, bLite, Lawns, Wild, BlockSlabHeight, *Today, S);
		if (bLite) Shapes(S);
		else Wrap(T.bFallen ? TEXT("log") : TEXT("tree"), FVector2D(T.X, T.Z), (float)g, [&]() { Shapes(S); });
		++Trees;
	}
	TArray<FBrixShape> S;
	for (const FIntPoint& K : BrixRoadCells(Today->Roads, P))   // zebras, lay-bys, bus stops, the traffic lights' poles
	{
		S.Reset();
		BlockIndex = RoadKey + K.X * 1000 + K.Y;
		BrixRoadShapes(Today->Roads, P, City.StreetWidth, K, BlockSlabHeight, Ground, S, SignalHeads);
		Shapes(S);
	}
	BlockIndex = Was;
	if (SignalHeads.Num())   // each head's lit lamp, glowing in its own colour (roads.js draw(): material 23)
	{
		SignalLamps = NewObject<UInstancedStaticMeshComponent>(this, TEXT("SignalLamps"));
		SignalLamps->SetupAttachment(RootComponent);
		SignalLamps->SetStaticMesh(Cube);
		SignalLamps->NumCustomDataFloats = 3;
		SignalLamps->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		SignalLamps->SetCastShadow(false);
		if (UMaterialInterface* Glow = LoadObject<UMaterialInterface>(nullptr, TEXT("/Game/Greybox/M_Glow.M_Glow")))
		{
			SignalMat = UMaterialInstanceDynamic::Create(Glow, this);
			SignalMat->SetScalarParameterValue(TEXT("Glow"), SignalGlow);
			SignalLamps->SetMaterial(0, SignalMat);
		}
		else SignalLamps->SetMaterial(0, Own());
		SignalLamps->RegisterComponent();
		AddInstanceComponent(SignalLamps);
		for (const FBrixSignalHead& Hd : SignalHeads) SignalLamps->AddInstance(FTransform(ToWorld(FVector2D(Hd.X, Hd.Z), -50.f)), false);
		SignalPhase.Init(-1, SignalHeads.Num());
	}
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d trees as the slice draws them, %d signal heads, %d zebras, %d lay-bys, %d bus stops"), Trees, SignalHeads.Num(),
		Today->Roads.Crossings.Num(), Today->Roads.Bays.Num(), Today->Roads.Stops.Num());
}

void ABrixTownActor::SetSignals(int32 Day, double Minutes)
{
	if (!SignalLamps || !Today.IsValid()) return;
	bool bAny = false;
	for (int32 h = 0; h < SignalHeads.Num(); ++h)
	{
		const FBrixSignalHead& Hd = SignalHeads[h];
		const int32 Ph = BrixSignalPhase(Today->Roads, Hd.Signal, Hd.bEw, Day, Minutes, Today->Pace);
		if (Ph == SignalPhase[h]) continue;
		SignalPhase[h] = Ph;
		const FBrixShape L = BrixSignalLamp(Hd, Ph);
		const TArray<double, TInlineAllocator<16>>& A = L.A;
		SignalLamps->UpdateInstanceTransform(h, FTransform(FQuat::Identity, ToWorld(FVector2D((A[0] + A[3]) / 2, (A[2] + A[5]) / 2), (float)((A[1] + A[4]) / 2)),
			FVector(A[3] - A[0], A[5] - A[2], A[4] - A[1])), false, false, true);
		SignalLamps->SetCustomData(h, { (float)L.Col.X, (float)L.Col.Y, (float)L.Col.Z }, false);
		bAny = true;
	}
	if (bAny) SignalLamps->MarkRenderStateDirty();
}

// One face of a block's facet mesh, in the slice's frame, lit on the side its hint is on (the material is two-sided)
void ABrixTownActor::Facet(const FVector3d& A, const FVector3d& B, const FVector3d& C, const FVector3d& Hint, const FVector3d& Col, bool bLawn)
{
	FFacets& F = Facets.FindOrAdd(BlockIndex);
	auto U = [](const FVector3d& Q) { return FVector3f((float)Q.X, (float)Q.Z, (float)Q.Y) * CmPerM; };
	FVector3d N = FVector3d::CrossProduct(B - A, C - A).GetSafeNormal();
	if (N.Dot(Hint) < 0) N = -N;
	F.P.Append({ U(A), U(B), U(C) });
	F.N.Add(FVector3f((float)N.X, (float)N.Z, (float)N.Y));
	F.C.Add(FLinearColor((float)Col.X, (float)Col.Y, (float)Col.Z));
	F.Owner.Add(Wrapping ? Breakables.Num() : INDEX_NONE);   // (Wrap() adds it next)
	F.Lawn.Add(bLawn ? 1 : 0);
	if (Wrapping)
	{
		Wrapping->FacetKey = BlockIndex;
		Wrapping->FaceP.Append({ U(A), U(B), U(C) });
		Wrapping->FaceN.Add(F.N.Last());
		Wrapping->FaceC.Add(F.C.Last());
	}
}

// A static mesh made now from triangles (three points each, a normal and a colour each), in M_Facets; a lawn's faces
// (Lawn 1) in a section of their own in M_Lawn, the lawn's own pattern (material 24)
UStaticMesh* ABrixTownActor::FacetMesh(const FString& MeshName, const TArray<FVector3f>& P, const TArray<FVector3f>& Nrm, const TArray<FLinearColor>& Col, const TArray<uint8>* Lawn)
{
	FMeshDescription MD;
	FStaticMeshAttributes Attr(MD);
	Attr.Register();
	TVertexAttributesRef<FVector3f> Pos = Attr.GetVertexPositions();
	TVertexInstanceAttributesRef<FVector3f> N = Attr.GetVertexInstanceNormals(), T = Attr.GetVertexInstanceTangents();
	TVertexInstanceAttributesRef<float> Bs = Attr.GetVertexInstanceBinormalSigns();
	TVertexInstanceAttributesRef<FVector4f> C = Attr.GetVertexInstanceColors();
	TVertexInstanceAttributesRef<FVector2f> UV = Attr.GetVertexInstanceUVs();
	const FPolygonGroupID G = MD.CreatePolygonGroup();
	Attr.GetPolygonGroupMaterialSlotNames()[G] = TEXT("facets");
	const bool bLawns = LawnMat && Lawn && Lawn->Num() * 3 == P.Num() && Lawn->Contains(1);
	FPolygonGroupID GL = G;
	if (bLawns) { GL = MD.CreatePolygonGroup(); Attr.GetPolygonGroupMaterialSlotNames()[GL] = TEXT("lawn"); }
	MD.ReserveNewVertices(P.Num()); MD.ReserveNewVertexInstances(P.Num()); MD.ReserveNewTriangles(P.Num() / 3);
	for (int32 t = 0; t + 2 < P.Num(); t += 3)
	{
		const FVector3f n = Nrm[t / 3];
		// the build stores a vertex colour as sRGB bytes and the material reads the bytes as they are: so it is given the
		// colour whose sRGB bytes are the linear colour wanted
		const FLinearColor Lin(FLinearColor(Col[t / 3].R, Col[t / 3].G, Col[t / 3].B).ToFColor(false));
		const FVector3f tg = (FMath::Abs(n.Z) < .9f ? FVector3f::CrossProduct(n, FVector3f::UpVector) : FVector3f::CrossProduct(n, FVector3f::ForwardVector)).GetSafeNormal();
		FVertexInstanceID Vi[3];
		for (int32 k = 0; k < 3; ++k)
		{
			const FVertexID V = MD.CreateVertex();
			Pos[V] = P[t + k];
			Vi[k] = MD.CreateVertexInstance(V);
			N[Vi[k]] = n; T[Vi[k]] = tg; Bs[Vi[k]] = 1.f;
			C[Vi[k]] = FVector4f(Lin.R, Lin.G, Lin.B, 1.f);
			UV.Set(Vi[k], 0, FVector2f::ZeroVector);
		}
		MD.CreateTriangle(bLawns && (*Lawn)[t / 3] ? GL : G, Vi);
	}
	UStaticMesh* M = NewObject<UStaticMesh>(this, *MeshName);
	M->GetStaticMaterials().Add(FStaticMaterial(FacetsMat, TEXT("facets"), TEXT("facets")));
	if (bLawns) M->GetStaticMaterials().Add(FStaticMaterial(LawnMat, TEXT("lawn"), TEXT("lawn")));
	UStaticMesh::FBuildMeshDescriptionsParams Params;
	Params.bBuildSimpleCollision = false;
	Params.bFastBuild = true;
	M->BuildFromMeshDescriptions({ &MD }, Params);
	Made.Add(M);
	return M;
}

// A group of one of the bush crowns (play.js dome() at R 1, H 1, its jitter from the variant), each instance in its colour
UInstancedStaticMeshComponent* ABrixTownActor::DomeGroup(int32 Variant)
{
	const FString Key = FString::Printf(TEXT("Dome_%d"), Variant);
	if (TObjectPtr<UInstancedStaticMeshComponent>* Found = Parts.Find(Key)) return *Found;
	TArray<FVector3d> Tris;
	TArray<double> Shade;
	BrixDomeMesh(Variant, Tris, Shade);
	TArray<FVector3f> P, N;
	TArray<FLinearColor> C;
	for (int32 t = 0; t < Shade.Num(); ++t)
	{
		const FVector3d a = Tris[t * 3], b = Tris[t * 3 + 1], c = Tris[t * 3 + 2];
		FVector3d n = FVector3d::CrossProduct(b - a, c - a).GetSafeNormal();
		if (n.Dot((a + b + c) / 3 - FVector3d(0, .5, 0)) < 0) n = -n;   // out of the crown
		for (const FVector3d& q : { a, b, c }) P.Add(FVector3f((float)q.X, (float)q.Z, (float)q.Y) * CmPerM);   // a 1 m crown: a scale is a size
		N.Add(FVector3f((float)n.X, (float)n.Z, (float)n.Y));
		const float sh = (float)(Shade[t] / 1.4);
		C.Add(FLinearColor(sh, sh, sh));
	}
	UInstancedStaticMeshComponent* G = NewObject<UInstancedStaticMeshComponent>(this, *Key);
	G->SetupAttachment(RootComponent);
	G->SetStaticMesh(FacetMesh(TEXT("SM_") + Key, P, N, C));
	G->NumCustomDataFloats = 3;
	G->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	G->bAffectDistanceFieldLighting = false;
	G->SetAffectDynamicIndirectLighting(false);
	G->SetCullDistances(0, (int32)(PlantCullDistance * CmPerM));
	G->RegisterComponent();
	AddInstanceComponent(G);
	Parts.Add(Key, G);
	return G;
}
