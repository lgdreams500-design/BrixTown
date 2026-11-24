#include "BrixPeople.h"

#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

int32 BrixBodyToMesh(UProceduralMeshComponent* M, const FBrixBodyMesh& Body, int32 Had, bool bFlip, UMaterialInterface* Material)
{
	const int32 Num = Body.Pos.Num();
	TArray<FVector> V, N;
	TArray<FLinearColor> C;
	V.SetNumUninitialized(Num);
	N.SetNumUninitialized(Num);
	C.SetNumUninitialized(Num);
	for (int32 k = 0; k < Num; ++k)   // the slice's frame (x east, y up, z south, metres) to Unreal's
	{
		const FVector3f& Q = Body.Pos[k];
		const FVector3f& Nn = Body.Normal[k];
		const FVector3f& Cc = Body.Colour[k];
		V[k] = FVector(Q.X, Q.Z, Q.Y) * CmPerM;
		N[k] = FVector(Nn.X, Nn.Z, Nn.Y);
		C[k] = FLinearColor(Cc.X, Cc.Y, Cc.Z, 1.f);
	}
	static const TArray<FVector2D> NoUV;
	static const TArray<FProcMeshTangent> NoTangents;
	if (Num != Had)   // a new count (another detail, a blink): the section is made anew, otherwise updated in place
	{
		TArray<int32> Tri;
		Tri.SetNumUninitialized(Num);
		for (int32 k = 0; k + 2 < Num; k += 3) { Tri[k] = k; Tri[k + 1] = bFlip ? k + 2 : k + 1; Tri[k + 2] = bFlip ? k + 1 : k + 2; }
		M->CreateMeshSection_LinearColor(0, V, Tri, N, NoUV, C, NoTangents, false, false);
		M->SetMaterial(0, Material);
	}
	else M->UpdateMeshSection_LinearColor(0, V, N, NoUV, C, NoTangents, false);
	return Num;
}

ABrixPeople::ABrixPeople()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Greybox(TEXT("/Game/Greybox/M_Greybox.M_Greybox"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> People(TEXT("/Game/People/M_People.M_People"));
	Figures = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Figures"));
	Figures->SetupAttachment(RootComponent);
	Figures->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	BodyMaterial = People.Object ? People.Object : Greybox.Object;
	if (!People.Object) UE_LOG(LogTemp, Warning, TEXT("Brix Town: /Game/People/M_People is missing (run Tools/unreal/make_people_material.py); the people are grey"));
}

void ABrixPeople::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay)
{
	Day = InDay;
	for (const FBrixBlock& B : City.Blocks) Paving.Add(FBox2D(B.Origin, B.Origin + B.Extent));
	FString Error;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/people.json")));
	if (!LoadBrixPeopleRules(Path, Rules, Error)) UE_LOG(LogTemp, Error, TEXT("Brix Town: %s"), *Error);
	for (int32 i = 0; i < Day->Num(); ++i)
	{
		Lane.Add((float)(BrixSliceHash({ FString::FromInt(i), TEXT("lane") }) * 2 - 1));
		const FString RoleName = Day->Roles.IsValidIndex(Day->Role[i]) ? Day->Roles[Day->Role[i]] : FString();
		const FString* Sig = Day->RoleSig.Find(RoleName);
		// the slice: look(i, the job's own clothes or the role, the role's walk)
		Looks.Add(BrixLookOf(Rules, i, Day->Wear.IsValidIndex(i) && !Day->Wear[i].IsEmpty() ? Day->Wear[i] : RoleName, Sig ? *Sig : FString(TEXT("Easy"))));
	}
	while (RebuildEvery.Num() < 3) RebuildEvery.Add(RebuildEvery.Num() + 1);
	// a pool of mesh bodies, all out of sight until the clock places someone in them
	for (int32 s = 0; s < MaxMeshBodies; ++s)
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this, *FString::Printf(TEXT("Body%d"), s));
		M->SetupAttachment(RootComponent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->bUseAsyncCooking = true;
		M->SetVisibility(false);
		M->RegisterComponent();
		AddInstanceComponent(M);
		Meshes.Add(M);
	}
	Slots.SetNum(MaxMeshBodies);
}

float ABrixPeople::GroundAt(const FVector2D& P) const
{
	for (const FBox2D& B : Paving) if (B.IsInside(P)) return Slab;
	return 0.f;
}

void ABrixPeople::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Day || !Mode || !PC || !Rules.Roles.Num()) return;
	const double T0 = FPlatformTime::Seconds();
	++Frame;
	// where the player is looking from (the camera when a test camera is in use)
	FVector Eye = FVector::ZeroVector;
	if (PC->PlayerCameraManager) Eye = PC->PlayerCameraManager->GetCameraLocation();
	else if (const APawn* Me = PC->GetPawn()) Eye = Me->GetActorLocation();
	const FVector2D Here(Eye.X / CmPerM, Eye.Y / CmPerM);
	const double Minute = Mode->ClockMinute(), Seconds = GetWorld()->GetTimeSeconds();

	// everyone out of doors within reach, nearest first
	struct FNear { int32 i; double D; FVector2D P; float Up; double Yaw; int32 Pose; double Phase; };
	TArray<FNear> Near;
	const double Reach2 = (double)ShowDistance * ShowDistance;
	for (int32 i = 0; i < Day->Num(); ++i)
	{
		const TOptional<FBrixWhere> W = Day->Where(i, Minute);
		if (!W || W->bInside) continue;
		// a little to one side of the pavement's line, never at a door (the slice's lane, CITY-043)
		FVector2D P = W->Pos;
		if (W->bWalking) P += FVector2D(-W->Heading.Y, W->Heading.X) * Lane[i] * LaneWidth * FMath::Clamp(FMath::Min(W->WalkDone, W->WalkLeft) / 5.f, 0.f, 1.f);
		const double D2 = FVector2D::DistSquared(P, Here);
		if (D2 >= Reach2) continue;
		const FBrixLook& L = Looks[i];
		const double Phase = W->bWalking ? FMath::Fmod(W->Along / BrixStepLength(Rules.Sig(L.SigName), L.S), 1.0) : 0.0;
		// the slice's yaw: a body faces (sin yaw, -cos yaw)
		Near.Add({ i, FMath::Sqrt(D2), P, GroundAt(P) + (W->Floor > 0 ? W->Floor * 3.f : 0.f), FMath::Atan2(W->Heading.X, -W->Heading.Y), W->bWalking ? 1 : 0, Phase });
	}
	Near.Sort([](const FNear& A, const FNear& B) { return A.D < B.D; });
	if (Near.Num() > MaxBodies) Near.SetNum(MaxBodies);

	// how much of each to draw: meshes for the nearest within NearDistance, blocks beyond
	TArray<int32> LodOf;
	LodOf.SetNum(Near.Num());
	int32 Full = 0, MeshCount = 0;
	for (int32 n = 0; n < Near.Num(); ++n)
	{
		const double D = Near[n].D;
		int32 Lod = D < FullDistance && Full < FullCount ? 0 : D < MidDistance ? 1 : D < NearDistance ? 2 : 3;
		if (Lod == 0) ++Full;
		if (Lod < 3 && MeshCount >= MaxMeshBodies) Lod = 3;
		if (Lod < 3) ++MeshCount;
		LodOf[n] = Lod;
	}

	// mesh bodies keep their slot while they stay near, so a body is moved, not rebuilt, between its rebuilds
	TMap<int32, int32> SlotOfResident;
	for (int32 s = 0; s < Slots.Num(); ++s) if (Slots[s].Resident >= 0) SlotOfResident.Add(Slots[s].Resident, s);
	TArray<bool> Kept;
	Kept.Init(false, Slots.Num());
	TArray<int32> Want;   // index into Near, for those needing a new slot
	TArray<int32> SlotFor;
	SlotFor.Init(-1, Near.Num());
	for (int32 n = 0; n < Near.Num(); ++n)
	{
		if (LodOf[n] >= 3) continue;
		if (const int32* S = SlotOfResident.Find(Near[n].i)) { SlotFor[n] = *S; Kept[*S] = true; }
		else Want.Add(n);
	}
	int32 Free = 0;
	for (const int32 n : Want)
	{
		while (Free < Slots.Num() && Kept[Free]) ++Free;
		if (Free >= Slots.Num()) { LodOf[n] = 3; continue; }
		Kept[Free] = true;
		Slots[Free] = FSlot();
		Slots[Free].Resident = Near[n].i;
		SlotFor[n] = Free;
	}
	for (int32 s = 0; s < Slots.Num(); ++s)
		if (!Kept[s] && Slots[s].Resident >= 0) { Slots[s] = FSlot(); Meshes[s]->SetVisibility(false); }

	TArray<FVector> V, N;
	TArray<FLinearColor> C;
	TArray<int32> Tri;
	const TArray<FVector2D> NoUV;
	const TArray<FProcMeshTangent> NoTangents;
	BoxScratch.Reset();
	for (int32 n = 0; n < Near.Num(); ++n)
	{
		const FNear& P = Near[n];
		const FBrixLook& L = Looks[P.i];
		FBrixDrawOptions O;
		O.Pose = P.Pose;
		O.Phase = P.Phase;
		O.Seconds = Seconds;
		O.Lod = LodOf[n];
		O.FaceFade = FMath::Clamp((FaceOutDistance - P.D) / (FaceOutDistance - MidDistance), 0.0, 1.0);
		if (O.Lod >= 3)
		{
			Scratch.Reset();
			DrawBrixPerson(Rules, L, FVector(P.P.X, P.Up, P.P.Y), P.Yaw, O, Scratch, &BoxScratch);
			continue;
		}
		const int32 s = SlotFor[n];
		if (s < 0) continue;
		FSlot& Slot = Slots[s];
		UProceduralMeshComponent* M = Meshes[s];
		const bool bRebuild = Slot.Lod != O.Lod || ++Slot.Age >= RebuildEvery[FMath::Clamp(O.Lod, 0, 2)] || Slot.Verts == 0;
		if (bRebuild)
		{
			// the body as people.js draws it facing yaw 0 at the origin, in Unreal's centimetres; the component turns and places it
			Scratch.Reset();
			DrawBrixPerson(Rules, L, FVector::ZeroVector, 0.0, O, Scratch);
			Slot.Verts = BrixBodyToMesh(M, Scratch, Slot.Verts, bFlipWinding, BodyMaterial);
			Slot.Lod = O.Lod;
			Slot.Age = 0;
		}
		M->SetWorldLocationAndRotation(ToWorld(P.P, P.Up), FRotator(0.0, FMath::RadiansToDegrees(P.Yaw), 0.0));
		M->SetVisibility(true);
	}
	// the far figures: six faces a box, in the room the mesh has (the rest folded to a point), so the mesh is updated in place
	// and made anew only when the room grows or shrinks by a step of 32 figures
	const int32 Step = 32 * 5, Cap = FMath::Max(Step, (BoxScratch.Num() + Step - 1) / Step * Step);
	const int32 NumV = Cap * 36;
	V.SetNumUninitialized(NumV, EAllowShrinking::No);
	N.SetNumUninitialized(NumV, EAllowShrinking::No);
	C.SetNumUninitialized(NumV, EAllowShrinking::No);
	const FVector Rest = BoxScratch.Num() ? FVector(BoxScratch[0].Min.X, BoxScratch[0].Min.Z, BoxScratch[0].Min.Y) * CmPerM : Eye;
	int32 q = 0;
	for (int32 b = 0; b < Cap; ++b)
	{
		if (b >= BoxScratch.Num())
		{
			for (int32 k = 0; k < 36; ++k) { V[q] = Rest; N[q] = FVector::UpVector; C[q] = FLinearColor::Black; ++q; }
			continue;
		}
		const FBrixFarBox& B = BoxScratch[b];
		const FLinearColor Col(B.Colour.X, B.Colour.Y, B.Colour.Z, 1.f);
		// corners in Unreal's frame (x east, y south, z up), in centimetres
		const FVector Lo = FVector(B.Min.X, B.Min.Z, B.Min.Y) * CmPerM, Hi = FVector(B.Max.X, B.Max.Z, B.Max.Y) * CmPerM;
		auto Corner = [&](int32 i) { return FVector(i & 1 ? Hi.X : Lo.X, i & 2 ? Hi.Y : Lo.Y, i & 4 ? Hi.Z : Lo.Z); };
		static const int32 Faces[6][4] = { { 0, 2, 6, 4 }, { 1, 5, 7, 3 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 1, 3, 2 }, { 4, 6, 7, 5 } };
		static const FVector Out[6] = { FVector(-1, 0, 0), FVector(1, 0, 0), FVector(0, -1, 0), FVector(0, 1, 0), FVector(0, 0, -1), FVector(0, 0, 1) };
		for (int32 f = 0; f < 6; ++f)
		{
			FVector Q[4] = { Corner(Faces[f][0]), Corner(Faces[f][1]), Corner(Faces[f][2]), Corner(Faces[f][3]) };
			// the bodies' turn: their triangles come out of the slice's frame with the cross of their edges pointing in
			if (FVector::DotProduct(FVector::CrossProduct(Q[1] - Q[0], Q[2] - Q[0]), Out[f]) > 0) Swap(Q[1], Q[3]);
			for (const int32 k : { 0, 1, 2, 0, 2, 3 }) { V[q] = Q[k]; N[q] = Out[f]; C[q] = Col; ++q; }
		}
	}
	if (Cap != FigureCap)
	{
		Tri.SetNumUninitialized(NumV, EAllowShrinking::No);
		for (int32 k = 0; k + 2 < NumV; k += 3) { Tri[k] = k; Tri[k + 1] = bFlipWinding ? k + 2 : k + 1; Tri[k + 2] = bFlipWinding ? k + 1 : k + 2; }
		Figures->CreateMeshSection_LinearColor(0, V, Tri, N, NoUV, C, NoTangents, false, false);
		Figures->SetMaterial(0, BodyMaterial);
		FigureCap = Cap;
	}
	else Figures->UpdateMeshSection_LinearColor(0, V, N, NoUV, C, NoTangents, false);
	LastShown = Near.Num();
	TickSeconds += FPlatformTime::Seconds() - T0;
	++TickFrames;
}
