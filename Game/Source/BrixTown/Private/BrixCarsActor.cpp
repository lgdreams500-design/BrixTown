#include "BrixCarsActor.h"

#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixPeople.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

namespace
{
	// the slice's colours (play.js C) and the cars' paints (cars.js meshCar())
	const FVector3f Teal(.20f, .36f, .37f), Terra(.44f, .24f, .18f), Ochre(.52f, .40f, .20f), Blue(.30f, .38f, .52f), Olive(.30f, .33f, .20f),
		Cream(.66f, .61f, .50f), Plum(.30f, .24f, .34f), Wood(.34f, .23f, .15f), Charcoal(.09f, .09f, .11f);
	const FVector3f Paint[9] = { Teal, Terra, Ochre, Blue, Olive, Cream, Plum, FVector3f(.55f, .40f, .34f), FVector3f(.42f, .47f, .40f) };
	const FVector3f Roofs[2] = { Cream, FVector3f(.74f, .70f, .62f) };
	const FVector3f VanPaint[4] = { Cream, Teal, Ochre, FVector3f(.55f, .40f, .34f) };
	FVector3f Named(const FString& N)
	{
		static const TMap<FString, FVector3f> C = { { TEXT("teal"), Teal }, { TEXT("terracotta"), Terra }, { TEXT("ochre"), Ochre }, { TEXT("blue"), Blue },
			{ TEXT("olive"), Olive }, { TEXT("cream"), Cream }, { TEXT("plum"), Plum }, { TEXT("charcoal"), Charcoal } };
		const FVector3f* F = C.Find(N);
		return F ? *F : Teal;
	}
	double H(int32 k, const TCHAR* K) { return BrixSliceHash({ FString::FromInt(k), K }); }

	// a box in the vehicle's frame (a along, front +; w across; y up), into a body mesh in the slice's frame (x = a, y up,
	// z = w), its triangles turned as the people's are
	void Box(FBrixBodyMesh& M, float a0, float a1, float w0, float w1, float y0, float y1, const FVector3f& Col)
	{
		auto P = [&](int i) { return FVector3f(i & 1 ? a1 : a0, i & 4 ? y1 : y0, i & 2 ? w1 : w0); };
		static const int32 Faces[6][4] = { { 0, 2, 6, 4 }, { 1, 5, 7, 3 }, { 0, 4, 5, 1 }, { 2, 3, 7, 6 }, { 0, 1, 3, 2 }, { 4, 6, 7, 5 } };
		static const FVector3f Out[6] = { FVector3f(-1, 0, 0), FVector3f(1, 0, 0), FVector3f(0, -1, 0), FVector3f(0, 1, 0), FVector3f(0, 0, -1), FVector3f(0, 0, 1) };
		for (int32 f = 0; f < 6; ++f)
		{
			FVector3f Q[4] = { P(Faces[f][0]), P(Faces[f][1]), P(Faces[f][2]), P(Faces[f][3]) };
			// Faces[] index corners as (a, w, y) bits 1, 2, 4; here (a, y, w): the outward normal in this frame
			const FVector3f N(Out[f].X, Out[f].Z, Out[f].Y);
			if (FVector3f::DotProduct(FVector3f::CrossProduct(Q[1] - Q[0], Q[2] - Q[0]), N) < 0) Swap(Q[1], Q[3]);
			for (const int32 k : { 0, 1, 2, 0, 2, 3 }) { M.Index.Add(M.Pos.Num()); M.Pos.Add(Q[k]); M.Normal.Add(N); M.Colour.Add(Col); }
		}
	}
}

ABrixCars::ABrixCars()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/People/M_People.M_People"));   // the vertex colour, matte
	Material = Mat.Object;
}

void ABrixCars::Start(TSharedPtr<FBrixTraffic> InTraffic)
{
	Traffic = InTraffic;
}

void ABrixCars::Build(UProceduralMeshComponent* M, int32 k, int32 Lod)
{
	FBrixBodyMesh Body;
	const FBrixVehicle& V = Traffic->Vehicles[k];
	if (V.Kind == 2)   // a bus: a long soft box in its line's colour, open windows, a cream roof (cars.js busBody())
	{
		const float L = 11.f, W = 2.5f, h = L / 2, w = W / 2;
		const FVector3f Col = V.Line >= 0 && Traffic->Lines.IsValidIndex(V.Line) ? Named(Traffic->Lines[V.Line]) : Teal, Inside(.24f, .17f, .12f), Trim(.30f, .29f, .29f), Tyre(.07f, .065f, .065f);
		Box(Body, -h, h, -w, w, .36f, 1.28f, Col);
		Box(Body, -h + .05f, h - .05f, -w - .012f, w + .012f, 1.02f, 1.14f, Cream);
		for (float a = -h + .25f; a <= h - .2f; a += 1.55f) for (const float s : { -1.f, 1.f }) Box(Body, a - .07f, a + .07f, s * (w - .1f) - .07f, s * (w - .1f) + .07f, 1.28f, 2.36f, Cream);
		Box(Body, -h, -h + .22f, -w + .1f, w - .1f, 1.28f, 2.36f, Col);
		Box(Body, h - .2f, h, -w + .1f, -w + .45f, 1.28f, 2.36f, Cream); Box(Body, h - .2f, h, w - .45f, w - .1f, 1.28f, 2.36f, Cream);
		Box(Body, -h - .02f, h + .02f, -w - .02f, w + .02f, 2.34f, 2.72f, Cream);
		Box(Body, h - .06f, h + .05f, -w + .45f, w - .45f, 2.06f, 2.3f, FVector3f(.86f, .80f, .64f));
		Box(Body, h - 1.f, h - .7f, -w + .2f, w - .2f, 1.28f, 1.42f, Inside);
		for (float a = -h + .9f; a < h - 1.6f; a += 1.1f) for (const float s : { -1.f, 1.f }) Box(Body, a - .25f, a + .25f, s * (w - .55f) - .3f, s * (w - .55f) + .3f, .95f, 1.05f, Inside);
		Box(Body, h - .08f, h + .08f, -w + .1f, w - .1f, .36f, .5f, Trim); Box(Body, -h - .08f, -h + .08f, -w + .1f, w - .1f, .36f, .5f, Trim);
		for (const float a : { h - 2.1f, -h + 2.5f }) for (const float s : { -1.f, 1.f }) Box(Body, a - .5f, a + .5f, s * w - .15f, s * w + .15f, 0.f, 1.f, Tyre);
	}
	else if (const FBrixCarModel* Md = Traffic->ModelOf(k))   // a Soft box car in this car's colours (cars.js meshCar())
	{
		const bool bVan = V.Kind == 1;
		const FVector3f BodyC = bVan ? VanPaint[k % 4] : Paint[(int32)(H(k, TEXT("col")) * 9) % 9];
		const FVector3f RoofC = H(k, TEXT("two")) < .75 ? Roofs[(int32)(H(k, TEXT("roof")) * 2) % 2] : BodyC;
		const bool bRack = H(k, TEXT("rack")) < .65, bSpare = H(k, TEXT("spare")) < .5;
		TArray<TOptional<FVector3f>> PartC;
		for (const FString& N : Traffic->Parts)
		{
			if ((N == TEXT("luggage") || N == TEXT("bag") || N == TEXT("rail")) && !bRack) { PartC.Add({}); continue; }   // a roof load comes as one
			if (N == TEXT("spare") && !bSpare) { PartC.Add({}); continue; }
			FVector3f C = BodyC;
			if (N == TEXT("roof")) C = RoofC;
			else if (N == TEXT("band")) C = BodyC == Cream ? Teal : Cream;
			else if (N == TEXT("tyre") || N == TEXT("spare")) C = FVector3f(.07f, .065f, .065f);
			else if (N == TEXT("hub")) C = FVector3f(.58f, .56f, .52f);
			else if (N == TEXT("leather")) C = FVector3f(.30f, .19f, .12f);
			else if (N == TEXT("lamp")) C = FVector3f(.86f, .80f, .64f);
			else if (N == TEXT("cream") || N == TEXT("bag")) C = Cream;
			else if (N == TEXT("iron") || N == TEXT("rail")) C = FVector3f(.10f, .10f, .11f);
			else if (N == TEXT("wood") || N == TEXT("cargo")) C = Wood;
			else if (N == TEXT("luggage")) C = Terra;
			PartC.Add(C);
		}
		const FBrixCarModel::FLod& D = Md->Lods[FMath::Clamp(Lod, 0, Md->Lods.Num() - 1)];
		for (int32 t = 0; t < D.Part.Num(); ++t)
		{
			const TOptional<FVector3f>& C = PartC.IsValidIndex(D.Part[t]) ? PartC[D.Part[t]] : TOptional<FVector3f>(BodyC);
			if (!C) continue;
			for (int32 v = 0; v < 3; ++v)
			{
				const FVector3f& Q = D.P[t * 3 + v];
				const FVector3f& Nn = D.N[t * 3 + v];
				Body.Index.Add(Body.Pos.Num());
				Body.Pos.Add(FVector3f(Q.X, Q.Z, Q.Y));   // (along, across, up) to the slice's (x, up, z)
				Body.Normal.Add(FVector3f(Nn.X, Nn.Z, Nn.Y));
				Body.Colour.Add(*C);
			}
		}
	}
	FBrixMeshArrays A;
	A.From(Body, false);
	A.Upload(M, -1, Material);
}

void ABrixCars::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Traffic || !Mode || !PC || !PC->PlayerCameraManager) return;
	++Frame;
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const FVector2D Here(Eye.X / CmPerM, Eye.Y / CmPerM);
	const double Minute = Mode->ClockMinute();
	int32 Shown = 0;
	for (int32 k = 0; k < Traffic->Num(); ++k)
	{
		const TOptional<FBrixCarAt> C = Traffic->At(k, Minute);
		const double D = C ? FVector2D::Distance(C->Pos, Here) : 1e9;
		FShown* S = Live.Find(k);
		if (D > ShowDistance)
		{
			if (S) { S->M->SetVisibility(false); Pool.Add(S->M); Live.Remove(k); }
			continue;
		}
		if (!S)
		{
			UProceduralMeshComponent* M = nullptr;
			if (Pool.Num()) M = Pool.Pop();
			else
			{
				M = NewObject<UProceduralMeshComponent>(this);
				M->SetupAttachment(RootComponent);
				M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				M->RegisterComponent();
				AddInstanceComponent(M);
			}
			S = &Live.Add(k, FShown{ M, -1, 0 });
		}
		const int32 Lod = D < NearDistance ? 0 : 1;
		if (S->Lod != Lod) { Build(S->M, k, Lod); S->Lod = Lod; }
		// the vehicle's frame: along its heading (Unreal yaw from x towards y), across to its right
		S->M->SetWorldLocationAndRotation(ToWorld(C->Pos, RoadUp), FRotator(0.0, FMath::RadiansToDegrees(FMath::Atan2(C->Heading.Y, C->Heading.X)), 0.0));
		S->M->SetVisibility(true);
		++Shown;
	}
	LastShown = Shown;
}
