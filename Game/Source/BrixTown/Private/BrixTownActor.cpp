#include "BrixTownActor.h"

#include "BrixCity.h"
#include "BrixUnits.h"
#include "Components/InstancedStaticMeshComponent.h"
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
	bool IsOwnSlot(const FName S) { return S == TEXT("frame") || S == TEXT("shutter") || S == TEXT("door") || S == TEXT("roof"); }
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
	G->NumCustomDataFloats = 3;
	const bool bPlain = !Mesh->GetName().StartsWith(TEXT("SM_Kit_")) || Mesh->GetName() == TEXT("SM_Kit_Box")
		|| Mesh->GetName() == TEXT("SM_Kit_Cylinder") || Mesh->GetName() == TEXT("SM_Kit_Sphere") || Mesh->GetName() == TEXT("SM_Kit_RoofWedge");
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

void ABrixTownActor::Add(UInstancedStaticMeshComponent* G, const FTransform& At, const FLinearColor& C)
{
	FPending& P = Pending.FindOrAdd(G);
	P.At.Add(At);
	P.Colour.Add(C);
}

void ABrixTownActor::BuildLot(const FBrixLot& L, const FBrixBlock& B)
{
	FRandomStream Rng((int32)GetTypeHash(L.Id));
	const float t = WallThickness;
	const int32 Floors = FMath::Max(1, L.Floors);
	const float H = Floors * StoreyHeight;
	const bool bOld = B.District != TEXT("Outskirts");
	const bool bCivic = L.Kind == TEXT("health") || L.Kind == TEXT("work");
	const bool bFood = L.Kind == TEXT("food"), bShop = L.Kind == TEXT("shop");
	const bool bShutters = L.Kind == TEXT("home") || bFood || bShop;
	const float StoneTo = bCivic ? H : (StoneBaseDistricts.Contains(B.District) ? FMath::Min(StoneBaseHeight, H) : 0.f);
	const FLinearColor Plaster = PlasterColours.Num() ? PlasterColours[Rng.RandHelper(PlasterColours.Num())] : FLinearColor(0.57f, 0.53f, 0.475f);
	const FLinearColor Frame = FrameColours.Num() ? FrameColours[Rng.RandHelper(FrameColours.Num())] : FLinearColor::White;
	const FLinearColor Shutter = ShutterColours.Num() ? ShutterColours[Rng.RandHelper(ShutterColours.Num())] : FLinearColor::White;
	const FLinearColor DoorPaint = DoorColours.Num() ? DoorColours[Rng.RandHelper(DoorColours.Num())] : FLinearColor::White;
	FLinearColor Dark = StoneColour * 0.7f;
	for (const FBrixSlotColour& F : SlotColours) if (F.Slot == TEXT("stone_dark")) Dark = F.Colour;

	UStaticMesh* BoxMesh = Kit(TEXT("SM_Kit_Box"), Cube);
	UInstancedStaticMeshComponent* Boxes = Group(BoxMesh, true);
	const FVector2D C = L.Centre(), S = L.Size();
	const double Ang = FMath::DegreesToRadians(L.AngleDeg), Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
	const float Base = BlockSlabHeight;
	auto At = [&](const FVector2D& Local, float Up) { return ToWorld(FVector2D(C.X + Local.X * Ca - Local.Y * Sa, C.Y + Local.X * Sa + Local.Y * Ca), Base + Up); };
	auto Box = [&](const FLinearColor& Col, const FVector2D& Local, float Z0, float Z1, double YawRad, float Len, float Dep)
	{
		Add(Boxes, FTransform(FQuat(FVector::UpVector, YawRad + Ang), At(Local, (Z0 + Z1) * 0.5f), FVector(Len, Dep, Z1 - Z0)), Col);
	};

	// the core, set back by the wall's depth all round (the kit's "core": what the reveals look into)
	Box(Plaster, FVector2D::ZeroVector, 0.f, H, 0.0, S.X - 2 * t, S.Y - 2 * t);

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
		if (F.Len >= MinFacadeForOpenings)
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
				// ground storey, as the kit's building() picks it by kind and district
				if (i == DoorBay)
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
					UStaticMesh* W = bTop ? Kit(TEXT("SM_Kit_WinTop")) : (bOld ? Kit(TEXT("SM_Kit_WinUp")) : Kit(TEXT("SM_Kit_WinPlain")));
					UStaticMesh* Sh = bShutters && Rng.FRand() < ShutterShare ? Kit(bTop ? TEXT("SM_Kit_Shutters145") : TEXT("SM_Kit_Shutters160")) : nullptr;
					Ops.Add({ Si, f * StoreyHeight + UpperSill, 1.1f, bTop ? 1.45f : 1.6f, W, Sh, &Frame });
				}
			}
		}

		// the pieces
		for (const FOpening& O : Ops)
		{
			if (!O.Mesh) continue;
			const FTransform Tr(FQuat(FVector::UpVector, Yaw + Ang), At(OnWall(O.S, 0.f), O.Z0));
			Add(Group(O.Mesh, false, OpeningCullDistance), Tr, *O.Paint);
			if (O.Shutters) Add(Group(O.Shutters, false, ShutterCullDistance), Tr, Shutter);
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
		auto Close = [&](const FRun& R) { Box(R.bStone ? StoneColour : Plaster, OnWall((R.Span.X + R.Span.Y) / 2, -t / 2), R.Z0, R.Z1, Yaw, R.Span.Y - R.Span.X, t); };
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
		if (StoneTo > 0.f && StoneTo < H - 0.5f) Box(Dark, OnWall(0.f, BandDepth / 2), StoneTo, StoneTo + BandHeight, Yaw, F.Len + 2 * BandDepth, BandDepth);
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
			Add(Boxes, FTransform(Q, At(Mid, Zc), FVector(Lr + 2 * RoofSideOverhang, Ls, RoofThickness)), RoofColour);
		}
	}
	else
	{
		const FLinearColor Top = bCivic ? StoneColour : Plaster;
		Box(Top, FVector2D(0, -S.Y / 2 + t / 2), H, H + ParapetHeight, 0.0, S.X, t);
		Box(Top, FVector2D(0, S.Y / 2 - t / 2), H, H + ParapetHeight, 0.0, S.X, t);
		Box(Top, FVector2D(-S.X / 2 + t / 2, 0), H, H + ParapetHeight, 0.0, t, S.Y - 2 * t);
		Box(Top, FVector2D(S.X / 2 - t / 2, 0), H, H + ParapetHeight, 0.0, t, S.Y - 2 * t);
		// the coping is a full slab: the roof inside the parapet is not seen from the street
		Box(StoneColour * 0.8f, FVector2D::ZeroVector, H + ParapetHeight, H + ParapetHeight + CopingHeight, 0.0, S.X + 0.12f, S.Y + 0.12f);
	}
	++Buildings;
}

void ABrixTownActor::Build(const FBrixCity& City)
{
	UInstancedStaticMeshComponent* Boxes = Group(Kit(TEXT("SM_Kit_Box"), Cube), true);
	UInstancedStaticMeshComponent* Trunks = Group(Kit(TEXT("SM_Kit_Cylinder"), Cylinder), true);
	UInstancedStaticMeshComponent* Crowns = Group(Kit(TEXT("SM_Kit_Sphere"), Sphere), false);
	const FVector2D MapSize(City.Columns * City.Pitch, City.Rows * City.Pitch);
	Add(Boxes, FTransform(FQuat::Identity, ToWorld(MapSize * 0.5, -0.5f), FVector(MapSize.X, MapSize.Y, 1.f)), GroundColour);
	for (const FBrixBlock& B : City.Blocks)
	{
		const bool bGreen = GreenTemplates.Contains(B.Template);
		Add(Boxes, FTransform(FQuat::Identity, ToWorld(B.Origin + B.Extent * 0.5, BlockSlabHeight * 0.5f), FVector(B.Extent.X, B.Extent.Y, BlockSlabHeight)), bGreen ? GreenColour : BlockColour);
		for (const FBrixLot& L : B.Lots)
		{
			if (L.IsOpen())
			{
				if (L.Kind == TEXT("garden"))
					Add(Boxes, FTransform(FQuat(FVector::UpVector, FMath::DegreesToRadians(L.AngleDeg)), ToWorld(L.Centre(), BlockSlabHeight + 0.02f), FVector(L.Size().X, L.Size().Y, 0.04f)), GreenColour);
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
		for (int32 i = 0; i < P.Value.Colour.Num(); ++i)
		{
			const FLinearColor& Col = P.Value.Colour[i];
			G->SetCustomData(First + i, { Col.R, Col.G, Col.B }, false);
		}
		G->MarkRenderStateDirty();
		Instances += P.Value.At.Num();
	}
	Pending.Empty();
	UE_LOG(LogTemp, Display, TEXT("Brix Town: built %s from data: %d blocks, %d buildings, %d openings, %d instances in %d groups"),
		*City.Name, City.Blocks.Num(), Buildings, Openings, Instances, Parts.Num());
}
