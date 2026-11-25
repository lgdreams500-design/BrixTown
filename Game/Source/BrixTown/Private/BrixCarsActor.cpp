#include "BrixCarsActor.h"

#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixPeople.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "WorldCollision.h"
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

void ABrixCars::Start(TSharedPtr<FBrixTraffic> InTraffic, const FBrixTram& InTram, const TArray<FBox2D>& InPaving)
{
	Traffic = InTraffic;
	Paving = InPaving;
	Tram = InTram;
	// the tram as cars.js tram() draws it, around its middle (a along the line, w across it): a terracotta body, open
	// windows between cream posts, a cream band and the roof, benches inside, a lamp at the front
	FBrixBodyMesh Body;
	const FVector3f Seat(.24f, .17f, .12f), Metal(.17f, .18f, .21f), Lamp(1.f, .55f, .22f);
	Box(Body, -6, 6, -1.2f, 1.2f, .35f, 1.5f, Terra);
	Box(Body, -6, 6, -1.2f, 1.2f, 2.55f, 2.75f, Cream);
	Box(Body, -6.1f, 6.1f, -1.25f, 1.25f, 2.75f, 2.95f, Metal);
	for (const float e : { -1.f, 1.f }) Box(Body, e * 6 - (e > 0 ? .25f : 0), e * 6 + (e < 0 ? .25f : 0), -1.2f, 1.2f, 1.5f, 2.55f, Cream);   // the ends
	for (int32 k = 1; k <= 6; ++k)   // posts between the six windows, both sides
	{
		const float a = -5.525f + k * 1.85f;
		Box(Body, a - .225f, a + .225f, -1.2f, -1.f, 1.5f, 2.55f, Cream);
		Box(Body, a - .225f, a + .225f, 1.f, 1.2f, 1.5f, 2.55f, Cream);
	}
	for (const float s : { -1.f, 1.f }) Box(Body, -5.75f, -5.3f, s * 1.1f - .1f, s * 1.1f + .1f, 1.5f, 2.55f, Cream);
	for (const float s : { -1.f, 1.f }) Box(Body, -5.3f + .4f, 5.4f, s * .55f - .3f, s * .55f + .3f, 1.1f, 1.15f, Seat);   // bench seats
	Box(Body, 6, 6.06f, -.25f, .25f, 1.1f, 1.4f, Lamp);
	// the rails along Tram Street, end to end (play.js: two iron strips 0.72 m either side of the line)
	{
		FBrixBodyMesh Rails;
		for (const float s : { -.72f, .72f }) Box(Rails, (float)Tram.X0, (float)Tram.X1, s - .05f, s + .05f, 0.f, .04f, Metal);
		FBrixMeshArrays R;
		R.From(Rails, false);
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this, TEXT("Rails"));
		M->SetupAttachment(RootComponent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->RegisterComponent();
		AddInstanceComponent(M);
		R.Upload(M, -1, Material);
		M->SetWorldLocation(ToWorld(FVector2D(0, Tram.Z), RoadUp));
	}
	FBrixMeshArrays A;
	A.From(Body, false);
	for (int32 k = 0; k < 4; ++k)
	{
		UProceduralMeshComponent* M = NewObject<UProceduralMeshComponent>(this);
		M->SetupAttachment(RootComponent);
		M->SetCollisionEnabled(ECollisionEnabled::NoCollision);
		M->RegisterComponent();
		AddInstanceComponent(M);
		A.Upload(M, -1, Material);
		M->SetVisibility(false);
		Trams.Add(M);
	}
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
	const TArray<FBrixTram::FAt> Now = Tram.At(Minute);
	for (int32 k = 0; k < Trams.Num(); ++k)
	{
		if (k >= Now.Num()) { Trams[k]->SetVisibility(false); continue; }
		Trams[k]->SetWorldLocationAndRotation(ToWorld(FVector2D(Now[k].X, Tram.Z), RoadUp), FRotator::ZeroRotator);   // eastward along Tram Street
		Trams[k]->SetVisibility(true);
	}
}

// ---- the player's car (cars.js target(), enter(), free(), drive(), leave())

int32 ABrixCars::TargetCar(const FVector2D& P, double& OutD) const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Traffic || !Mode || Drive.K >= 0) return -1;
	const double Minute = Mode->ClockMinute();
	int32 Best = -1;
	OutD = 1e9;
	for (int32 k = 0; k < Traffic->Num(); ++k)
	{
		const double Far = Traffic->Vehicles[k].Kind == 2 ? 8 : 5;
		const TOptional<FBrixCarAt> C = Traffic->At(k, Minute);
		if (!C || C->bMoving || FMath::Abs(C->Pos.X - P.X) > Far || FMath::Abs(C->Pos.Y - P.Y) > Far) continue;
		if (!Traffic->Hits(*C, k, P, 1.5)) continue;   // within arm's reach of the door
		const double D = FVector2D::Distance(C->Pos, P);
		if (D < OutD) { OutD = D; Best = k; }
	}
	return Best;
}

bool ABrixCars::Enter(int32 k)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const TOptional<FBrixCarAt> C = Traffic && Mode ? Traffic->At(k, Mode->ClockMinute()) : TOptional<FBrixCarAt>();
	if (!C) return false;
	if (IsBus(k))   // on the bus: a passenger, carried where it goes (CITY-092)
	{
		Drive = { k, C->Pos.X, C->Pos.Y, FMath::Atan2(C->Heading.Y, C->Heading.X), 0, true, false };
		return true;
	}
	Drive = { k, C->Pos.X, C->Pos.Y, FMath::Atan2(C->Heading.Y, C->Heading.X), 0 };
	// out of the bay onto the road first, as residents' cars do: a sideways step off the paving, if there is room
	auto OnPaving = [&](const FVector2D& P) { for (const FBox2D& B : Paving) if (B.IsInside(P)) return true; return false; };
	const FVector2D Side(-C->Heading.Y, C->Heading.X);
	bool bOut = false;
	for (const double d : { 2.85, 2.2, 1.6 })
	{
		for (const double s : { -1.0, 1.0 })
		{
			const FVector2D P = C->Pos + Side * (s * d);
			if (!OnPaving(P) && Free(P.X, P.Y, Drive.A)) { Drive.X = P.X; Drive.Z = P.Y; bOut = true; break; }
		}
		if (bOut) break;
	}
	FBrixCarAt M = *C;
	M.bMoving = false;
	Traffic->Moved.Add(k, M);   // it no longer follows its owner's day
	return true;
}

bool ABrixCars::Free(double X, double Z, double A) const
{
	const FVector3f D = Traffic->Dims(Drive.K);
	const FVector2D H(FMath::Cos(A), FMath::Sin(A));
	// a wall, a building, a lamp or a bench: the car's box a little above the street (clear of the paving's kerb)
	const FVector Mid = ToWorld(FVector2D(X, Z) + H * D.Z, RoadUp + .75f);
	FCollisionQueryParams Q(TEXT("BrixCar"), false);
	if (const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0)) Q.AddIgnoredActor(Me);
	if (GetWorld()->OverlapAnyTestByChannel(Mid, FQuat(FVector::UpVector, A), ECC_WorldStatic, FCollisionShape::MakeBox(FVector(D.X / 2, D.Y / 2, .5f) * CmPerM), Q)) return false;
	// another car: any of the outline's eight points inside it (cars.js free())
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const double Minute = Mode ? Mode->ClockMinute() : 0;
	const FVector2D Side(-H.Y, H.X);
	const double L = D.X / 2, W = D.Y / 2;
	const FVector2D Pts[8] = { { L, -W }, { L, W }, { -L, -W }, { -L, W }, { L, 0 }, { -L, 0 }, { 0, -W }, { 0, W } };
	for (int32 k = 0; k < Traffic->Num(); ++k)
	{
		if (k == Drive.K) continue;
		const TOptional<FBrixCarAt> C = Traffic->At(k, Minute);
		if (!C || FMath::Abs(C->Pos.X - X) > 16 || FMath::Abs(C->Pos.Y - Z) > 16) continue;
		for (const FVector2D& Pt : Pts)
			if (Traffic->Hits(*C, k, FVector2D(X, Z) + H * (Pt.X + D.Z) + Side * Pt.Y, .05)) return false;
	}
	return true;
}

void ABrixCars::Step(float Dt, float Throttle, float Steer, bool bHandbrake)
{
	if (Drive.K < 0 || !Traffic) return;
	const double Top = Traffic->PlayerTopKmh / 3.6;
	double v = Drive.V;
	if (Throttle > 0) v += (v < 0 ? 8 : 3.2 + 2.4 * FMath::Min(1.0, Top / 28)) * Dt;   // a faster car pulls harder
	else if (Throttle < 0) v -= (v > 0 ? 7 : 2.5) * Dt;
	else v -= FMath::Sign(v) * FMath::Min(FMath::Abs(v), 1.2 * Dt);
	if (bHandbrake) v -= FMath::Sign(v) * FMath::Min(FMath::Abs(v), 9.0 * Dt);
	v = FMath::Clamp(v, -4.0, Top);
	const double Turn = Steer * .5 * (1 - .5 * FMath::Min(1.0, FMath::Abs(v) / Top)), a = Drive.A + v / 2.6 * FMath::Tan(Turn) * Dt;
	const double x = Drive.X + FMath::Cos(a) * v * Dt, z = Drive.Z + FMath::Sin(a) * v * Dt;
	if (Free(x, z, a)) { Drive.X = x; Drive.Z = z; Drive.A = a; Drive.V = v; }
	else Drive.V = -v * .2;   // a bump: stop, and roll back a little
	FBrixCarAt M;
	M.Pos = FVector2D(Drive.X, Drive.Z);
	M.Heading = FVector2D(FMath::Cos(Drive.A), FMath::Sin(Drive.A));
	M.bMoving = FMath::Abs(Drive.V) > .2;
	Traffic->Moved.Add(Drive.K, M);
}

bool ABrixCars::RideSeat(FVector2D& OutPos, double& OutYaw)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const TOptional<FBrixCarAt> C = Traffic && Mode && Drive.bRide ? Traffic->At(Drive.K, Mode->ClockMinute()) : TOptional<FBrixCarAt>();
	if (!C) { Drive.bMoving = false; return false; }   // out of service
	Drive.X = C->Pos.X; Drive.Z = C->Pos.Y; Drive.A = FMath::Atan2(C->Heading.Y, C->Heading.X); Drive.bMoving = C->bMoving;
	Drive.V = C->bMoving ? 5 : 0;   // for the camera, which swings round behind while it goes
	const double a = 11.0 / 2 - 2.3, w = -(2.5 / 2 - .55);   // a seat near the front (cars.js ride())
	OutPos = C->Pos + C->Heading * a + FVector2D(-C->Heading.Y, C->Heading.X) * w;
	OutYaw = Drive.A;
	return true;
}

void ABrixCars::Leave(FVector2D& OutSpot, double& OutYaw)
{
	if (Drive.K < 0) return;
	if (Drive.bRide)   // off the bus where it stands, on the kerb side by the door
	{
		const FVector2D H(FMath::Cos(Drive.A), FMath::Sin(Drive.A)), Side(-H.Y, H.X);
		OutSpot = FVector2D(Drive.X, Drive.Z) + H * 3.6 - Side * (2.5 / 2 + .9);
		OutYaw = Drive.A;
		Drive = FDrive();
		return;
	}
	const FVector3f D = Traffic->Dims(Drive.K);
	const FVector2D H(FMath::Cos(Drive.A), FMath::Sin(Drive.A)), Side(-H.Y, H.X), C(Drive.X, Drive.Z);
	FBrixCarAt M;
	M.Pos = C;
	M.Heading = H;
	Traffic->Moved.Add(Drive.K, M);   // it stays where it was left
	// out by the driver's door, else the other side, else the back, else the front
	const FVector2D Spots[4] = { C - Side * (D.Y / 2 + .7), C + Side * (D.Y / 2 + .7), C - H * (D.X / 2 + .8), C + H * (D.X / 2 + .8) };
	OutSpot = Spots[0];
	FCollisionQueryParams Q(TEXT("BrixOut"), false);
	if (const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0)) Q.AddIgnoredActor(Me);
	for (const FVector2D& S : Spots)
		if (!GetWorld()->OverlapAnyTestByChannel(ToWorld(S, 1.f), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeCapsule(30.f, 70.f), Q)) { OutSpot = S; break; }
	OutYaw = Drive.A;
	Drive = FDrive();
}

bool ABrixCars::DriverSeat(FVector2D& OutPos, double& OutYaw, float& OutUp, float& OutFloor, float Head) const
{
	if (Drive.K < 0 || !Traffic) return false;
	const FBrixCarModel* Md = Traffic->ModelOf(Drive.K);
	if (!Md) return false;
	const FVector2D H(FMath::Cos(Drive.A), FMath::Sin(Drive.A)), Side(-H.Y, H.X);
	OutPos = FVector2D(Drive.X, Drive.Z) + H * Md->Seat.X + Side * Md->Seat.Y;
	OutYaw = Drive.A;
	const float Roof = Md->Roof ? *Md->Roof : 1.56f;
	OutUp = FMath::Min(Md->SeatY, Roof - .05f - Head);   // as low as the head needs under the roof (cars.js seat())
	OutFloor = (Md->Floor ? *Md->Floor + .03f : .3f) - OutUp;
	return true;
}
