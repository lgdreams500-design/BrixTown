#include "BrixPeople.h"

#include "BrixCars.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixGameMode.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "GameFramework/Character.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Async/ParallelFor.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "BrixCarsActor.h"
#include "BrixCrashActor.h"
#include "BrixWeatherActor.h"
#include "Components/TextRenderComponent.h"
#include "WorldCollision.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

void FBrixMeshArrays::From(const FBrixBodyMesh& Body, bool bFlip)
{
	const int32 Num = Body.Pos.Num();
	V.SetNumUninitialized(Num, EAllowShrinking::No);
	N.SetNumUninitialized(Num, EAllowShrinking::No);
	C.SetNumUninitialized(Num, EAllowShrinking::No);
	for (int32 k = 0; k < Num; ++k)   // the slice's frame (x east, y up, z south, metres) to Unreal's
	{
		const FVector3f& Q = Body.Pos[k];
		const FVector3f& Nn = Body.Normal[k];
		const FVector3f& Cc = Body.Colour[k];
		V[k] = FVector(Q.X, Q.Z, Q.Y) * CmPerM;
		N[k] = FVector(Nn.X, Nn.Z, Nn.Y);
		C[k] = FLinearColor(Cc.X, Cc.Y, Cc.Z, 1.f);
	}
	Tri = Body.Index;
	if (bFlip) for (int32 k = 0; k + 2 < Tri.Num(); k += 3) Swap(Tri[k + 1], Tri[k + 2]);
}

int32 FBrixMeshArrays::Upload(UProceduralMeshComponent* M, int32 Had, UMaterialInterface* Material) const
{
	static const TArray<FVector2D> NoUV;
	static const TArray<FProcMeshTangent> NoTangents;
	// the same shape at the same detail always comes out in the same order, so the same count of indices is the same
	// triangles: the section is updated in place; a new count (another detail, a blink) makes it anew
	if (Tri.Num() != Had)
	{
		M->CreateMeshSection_LinearColor(0, V, Tri, N, NoUV, C, NoTangents, false, false);
		M->SetMaterial(0, Material);
	}
	else M->UpdateMeshSection_LinearColor(0, V, N, NoUV, C, NoTangents, false);
	return Tri.Num();
}

int32 BrixBodyToMesh(UProceduralMeshComponent* M, const FBrixBodyMesh& Body, int32 Had, bool bFlip, UMaterialInterface* Material)
{
	FBrixMeshArrays A;
	A.From(Body, bFlip);
	return A.Upload(M, Had, Material);
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

void ABrixPeople::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, TSharedPtr<FBrixTraffic> InTraffic)
{
	Day = InDay;
	Traffic = InTraffic;
	for (const FBrixBlock& B : City.Blocks) Paving.Add(FBox2D(B.Origin, B.Origin + B.Extent));
	const int32 Seated = SeatBrixDay(*Day, City);   // benches and fountain rims (CITY-017)
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %d of the day's stays are on a seat"), Seated);
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
	{   // the repair crews (smash.js crewLook): two people of the utility role, walking as it walks
		const FString* Sig = Day->RoleSig.Find(TEXT("utility"));
		for (int32 c = 0; c < 2; ++c) CrewLooks.Add(BrixLookOf(Rules, 310000 + c * 17, TEXT("utility"), Sig ? *Sig : FString(TEXT("Easy"))));
	}
	{   // out of the way of the player's car (CITY-070, CITY-071)
		FString DodgeError;
		const FString DodgeRules = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/dodge.json")));
		Dodge.bOn = LoadBrixDodgeRules(DodgeRules, Dodge.R, DodgeError);
		if (!Dodge.bOn) UE_LOG(LogTemp, Warning, TEXT("Brix Town: %s; nobody gets out of the way of the player's car"), *DodgeError);
	}
	// who rides the tram (CITY-041): someone away whose day takes them from a tram stop, or brings them back to one, rides
	// the run that is at that stop when they leave or arrive (cars.js tramRiders())
	Tram = City.Tram;
	{
		TArray<const FBrixLot*> AllLots;
		for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) AllLots.Add(&L);
		auto AtStop = [&](const FBrixDay::FPiece& P) { return P.Kind == 0 && AllLots.IsValidIndex(P.Lot) && AllLots[P.Lot]->Kind == TEXT("stop"); };
		auto RunAt = [&](double M, double X)
		{
			TOptional<double> Best;
			double Bd = 25;
			for (const FBrixTram::FAt& T : Tram.At(M)) if (FMath::Abs(T.X - X) < Bd) { Bd = FMath::Abs(T.X - X); Best = T.Run; }
			return Best;
		};
		for (int32 i = 0; i < Day->Num(); ++i)
		{
			const TArray<FBrixDay::FPiece>& Row = Day->Plan[i];
			for (int32 j = 1; j < Row.Num(); ++j)
			{
				if (Row[j].Kind != 2) continue;
				const FBrixDay::FPiece& Before = Row[j - 1];
				const FBrixDay::FPiece* After = j + 1 < Row.Num() ? &Row[j + 1] : nullptr;
				FTramRide R{ i, Row[j].T0, After ? After->T0 : 1440.0, {}, {} };
				if (AtStop(Before)) R.Out = RunAt(Row[j].T0, Before.Pos.X);
				if (After && AtStop(*After)) R.Back = RunAt(R.T1, After->Pos.X);
				if (R.Out || R.Back) TramRides.Add(R);
			}
		}
	}
	// people from out of town in the vehicles: the same person every time (C7), by the vehicle (the slice numbers them as it
	// meets them; here by the vehicle, so the same van always has the same driver)
	if (Traffic)
		for (int32 k = 0; k < Traffic->Num(); ++k)
		{
			const FBrixVehicle& V = Traffic->Vehicles[k];
			auto Who = [&](const FString& Key, const TCHAR* RoleName)
			{
				const FString* Sig = Day->RoleSig.Find(RoleName);
				return BrixLookOf(Rules, 200000 + k * 7 + (int32)(BrixSliceHash({ Key, TEXT("who") }) * 5), RoleName, Sig ? *Sig : FString(TEXT("Easy")));
			};
			Drivers.Add(Who((V.Kind == 2 ? TEXT("bus") : TEXT("drv")) + FString::FromInt(k), V.Kind == 2 ? TEXT("utility") : V.Kind == 1 ? TEXT("courier") : TEXT("commuter")));
			Passengers.Add(Who(TEXT("pas") + FString::FromInt(k), TEXT("elder")));
		}
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
	// those who were held in a talk run late until they next stand still where they would be anyway (conversations.js where())
	TMap<int32, double> MinuteOf;
	for (auto It = Late.CreateIterator(); It; ++It)
	{
		const double M2 = FMath::Fmod(Minute - It->Value + 1440.0, 1440.0);
		const int32 K2 = Day->PieceAt(It->Key, M2);
		if (Day->Plan.IsValidIndex(It->Key) && K2 >= 0 && Day->Plan[It->Key][K2].Kind == 0 && Day->PieceAt(It->Key, Minute) == K2) { It.RemoveCurrent(); continue; }
		MinuteOf.Add(It->Key, M2);
	}

	// everyone out of doors within reach, nearest first
	// worked out for everyone at once on the task threads (CITY-022: from the day and the minute alone)
	// i: who (a resident, or 1,000,000 + 2 a vehicle + 0 or 1 for someone from out of town in it), the slot's key
	struct FNear { int32 i; double D; FVector2D P; float Up; double Yaw; int32 Pose; double Phase; const FBrixLook* L; TOptional<double> Floor; bool bDriving; int32 Extra = -1; int32 Wet = -1; };
	const double Reach2 = (double)ShowDistance * ShowDistance;
	TArray<FNear> All;
	All.SetNumUninitialized(Day->Num());
	TArray<int32> InVehicle;
	InVehicle.Init(-1, Day->Num());
	ParallelFor(Day->Num(), [&](int32 i)
	{
		All[i].i = -1;
		if (i == Held.Resident) return;   // added below, where the talk holds them
		const double* M2 = MinuteOf.Find(i);
		const double Mi = M2 ? *M2 : Minute;
		const TOptional<FBrixWhere> W = Day->Where(i, Mi);
		if (!W) { InVehicle[i] = Day->VehicleAt(i, Mi); return; }
		if (W->bInside && W->Lot != CameraLot) return;   // inside: seen only from inside their building (play.js nearPeople)
		// a little to one side of the pavement's line, never at a door (the slice's lane, CITY-043)
		FVector2D P = W->Pos;
		if (W->bWalking) P += FVector2D(-W->Heading.Y, W->Heading.X) * Lane[i] * LaneWidth * FMath::Clamp(FMath::Min(W->WalkDone, W->WalkLeft) / 5.f, 0.f, 1.f);
		const double D2 = FVector2D::DistSquared(P, Here);
		if (D2 >= Reach2) return;
		const FBrixLook& L = Looks[i];
		const double Phase = W->bWalking ? FMath::Fmod(W->Along / BrixStepLength(Rules.Sig(L.SigName), L.S), 1.0) : 0.0;
		// the slice's yaw: a body faces (sin yaw, -cos yaw)
		All[i] = { i, FMath::Sqrt(D2), P, W->SeatUp >= 0.f ? W->SeatUp : GroundAt(P) + (W->Floor > 0 ? W->Floor * 3.f : 0.f), FMath::Atan2(W->Heading.X, -W->Heading.Y), W->Pose, Phase, &L, {}, false };
	});
	TArray<FNear> Near;
	for (const FNear& A : All) if (A.i >= 0) Near.Add(A);
	// out of the way of the player's car, then telling them off (CITY-070, CITY-071; dodge.js): laid over where the day has them
	TArray<FBrixDodged> Extras;
	if (Dodge.bOn)
	{
		const ABrixCars* CarsActor = Mode->Cars;
		const bool bDriving = CarsActor && CarsActor->Driving().K >= 0 && !CarsActor->Driving().bRide;
		const ABrixCars::FDrive D = CarsActor ? CarsActor->Driving() : ABrixCars::FDrive();
		Dodge.Frame(Seconds, bDriving, FVector2D(D.X, D.Z), FVector2D(FMath::Cos(D.A), FMath::Sin(D.A)), 0.0, D.V);
		if (bDriving || Dodge.Live())
		{
			FCollisionQueryParams Q(TEXT("BrixDodge"), false);
			if (const APawn* Me = PC->GetPawn()) Q.AddIgnoredActor(Me);
			for (FNear& A : Near)
			{
				const float Ground = GroundAt(A.P);
				// somewhere to stand: not a wall, not a step up (dodge.js clear())
				auto Clear = [&](const FVector2D& P) { return FMath::Abs(GroundAt(P) - Ground) < .5f && !GetWorld()->OverlapAnyTestByChannel(ToWorld(P, Ground + .9f), FQuat::Identity, ECC_WorldStatic, FCollisionShape::MakeSphere(15.f), Q); };
				FBrixDodged Out;
				Out.Pos = A.P; Out.Yaw = A.Yaw; Out.Pose = A.Pose; Out.bWalking = A.Pose == 1; Out.Phase = A.Phase;
				if (!Dodge.Apply(A.i, A.P, A.Up, A.Yaw, A.Pose, false, Clear, Out)) continue;
				A.P = Out.Pos;
				A.Up = GroundAt(Out.Pos) + (float)Out.Hop;
				A.Yaw = Out.Yaw;
				A.Pose = Out.Pose;
				A.Phase = Out.Phase;
				A.D = FVector2D::Distance(A.P, Here);
				A.Extra = Extras.Add(Out);
			}
		}
		if (Dodge.Live() > DodgeLogged) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d residents getting out of the way of the player's car"), Dodge.Live());
		DodgeLogged = Dodge.Live();
	}
	// residents near something the player's car smashed turn to it and shout (CITY-076, smash.js apply()); the repair crews
	// at work, crouched over what they mend (smash.js worker())
	if (const ABrixCrash* Crash = Mode->Crash.Get())
	{
		if (Crash->Recent.Num())
			for (FNear& A : Near)
			{
				if (A.Extra >= 0 || A.Pose == 2) continue;   // dodging already, or sitting
				const ABrixCrash::FRecent* S = nullptr;
				for (const ABrixCrash::FRecent& E : Crash->Recent) if (FVector2D::Distance(E.P, A.P) < Crash->Rules().ReactM && Seconds - E.T < Crash->Rules().ReactS) S = &E;
				FBrixDodged Out;
				if (!S || !BrixCrashReact(Crash->Rules(), A.i, A.P, S->P.X, S->P.Y, Seconds - S->T, S->T, Seconds, Out)) continue;
				A.Yaw = Out.Yaw; A.Pose = 0; A.Phase = 0;
				A.Extra = Extras.Add(Out);
			}
		for (int32 k = 0; k < Crash->Workers.Num() && CrewLooks.Num() == 2; ++k)
		{
			const ABrixCrash::FWorker& W = Crash->Workers[k];
			const double D = FVector2D::Distance(W.P, Here);
			if (D > ShowDistance) continue;
			FBrixDodged Out;
			const double w = FMath::Sin(Seconds * 5 + W.K) * 20;
			Out.Arms[0] = FVector4d(1, 60 + w, 20, 70);
			Out.Arms[1] = FVector4d(1, 60 - w, 20, 70);
			Out.Chest = FVector2D(24, 0);
			const FVector2D To = W.Toward - W.P;
			FNear N{ 2000000 + k, D, W.P, GroundAt(W.P), FMath::Atan2(To.X, -To.Y), 0, 0.0, &CrewLooks[W.K % 2], {}, false };
			N.Extra = Extras.Add(Out);
			Near.Add(N);
		}
	}
	// the one to talk to: the nearest to the player (not the camera) within reach, on their level
	Cand = FCandidate();
	if (const APawn* Me = PC->GetPawn())
	{
		const FVector At = Me->GetActorLocation();
		const ACharacter* Ch = Cast<ACharacter>(Me);
		const float Feet = (At.Z - (Ch && Ch->GetRootComponent() ? Ch->GetSimpleCollisionHalfHeight() : 85.f)) / CmPerM;
		const FVector2D Mine(At.X / CmPerM, At.Y / CmPerM);
		for (const FNear& A : Near)
		{
			const double D = FVector2D::Distance(A.P, Mine);
			if (Held.Resident >= 0 || D > TalkReach || FMath::Abs(A.Up - Feet) > 1.2f || (Cand.Resident >= 0 && D >= Cand.D)) continue;
			const double* M2 = MinuteOf.Find(A.i);
			const TOptional<FBrixWhere> W = Day->Where(A.i, M2 ? *M2 : Minute);
			if (!W) continue;
			Cand = { A.i, A.P, A.Up, A.Yaw, A.Pose, W->Act, W->Lot, W->bWalking, D };
		}
	}
	// the weather (CITY-077 to CITY-080; ADR-0020): today's coat; out in the rain an umbrella up, else a hood, else a hand over
	// the head while walking, and a quicker walk; nothing but the coat indoors, sheltering or under a roof
	TArray<FBrixWet> Wets;
	if (const ABrixWeather* Wx = Mode->Weather.Get(); Wx && Wx->bOn)
	{
		const bool bTrying = Wx->IsTrying();
		const int32 Shelter = Day->Acts.IndexOfByKey(FString(TEXT("shelter")));
		if (CoatLooks.Num() != Day->Num()) { CoatLooks.SetNum(Day->Num()); CoatMade.Init(0, Day->Num()); }
		for (FNear& A : Near)
		{
			if (A.i < 0 || A.i >= Day->Num()) continue;
			const double* M2 = MinuteOf.Find(A.i);
			const double Mi = M2 ? *M2 : Minute;
			const int32 K = Day->PieceAt(A.i, Mi);
			const FBrixDay::FPiece* P = K >= 0 ? &Day->Plan[A.i][K] : nullptr;
			const bool bWalk = A.Pose == 1 && P && P->Kind == 1 && P->Cum.Num();
			const double T1 = K >= 0 && K + 1 < Day->Plan[A.i].Num() ? Day->Plan[A.i][K + 1].T0 : 1440.0;
			const bool bCovered = (P && P->Act == Shelter) || (A.Up < 6.f && Wx->Roofed(A.P));
			const bool bOther = Extras.IsValidIndex(A.Extra) && (!Extras[A.Extra].Sig.IsEmpty() || A.Pose != 1);   // running from a car, or held by a smash
			const FBrixLook& L0 = Looks[A.i];
			FBrixWet W = BrixWetOf(Rules, Wx->Wear, A.i, Wx->Now.Rain, bTrying, bCovered, bWalk, bOther, L0.SigName, L0.S, Mi, P ? P->T0 : 0, T1, bWalk ? P->Cum.Last() : 0);
			if (W.Coat && !(L0.Garment == TEXT("coat") && L0.Top == *W.Coat))
			{
				const int8 Want = bTrying ? 2 : 1;
				if (CoatMade[A.i] != Want) { CoatLooks[A.i] = L0; CoatLooks[A.i].Garment = TEXT("coat"); CoatLooks[A.i].Top = *W.Coat; CoatMade[A.i] = Want; }
				A.L = &CoatLooks[A.i];
			}
			if (!W.Sig.IsEmpty() && !Extras.IsValidIndex(A.Extra)) A.Phase = W.Phase;
			if (W.Hood || W.Umbrella || W.Arms[0] || W.Arms[1] || !W.Sig.IsEmpty()) A.Wet = Wets.Add(MoveTemp(W));
		}
	}
	// the trams' riders on their benches, facing the way the tram goes (cars.js tram(): up to twelve)
	for (const FBrixTram::FAt& T : Tram.At(Minute))
	{
		if (FMath::Abs(T.X - Here.X) > RideDistance + 6 || FMath::Abs(Tram.Z - Here.Y) > RideDistance) continue;
		int32 Slot = 0;
		for (const FTramRide& R : TramRides)
		{
			const bool bGoing = R.Out && *R.Out == T.Run && Minute >= R.T0 && Minute < R.T0 + 40, bComing = R.Back && *R.Back == T.Run && Minute < R.T1 && Minute > R.T1 - 40;
			if (!bGoing && !bComing) continue;
			if (Slot >= 12) break;
			const int32 Row = Slot % 6;
			const double Side = Slot < 6 ? -1 : 1;
			++Slot;
			const FVector2D P(T.X - 4.7 + Row * 1.85, Tram.Z + Side * .55);
			Near.Add({ R.Resident, FVector2D::Distance(P, Here), P, 1.15f, FMath::Atan2(1.0, 0.0), 2, 0.0, &Looks[R.Resident], {}, false });
		}
	}
	if (Held.Resident >= 0 && Looks.IsValidIndex(Held.Resident))   // held in a talk: where the talk keeps them
		Near.Add({ Held.Resident, FVector2D::Distance(Held.Pos, Here), Held.Pos, Held.Up, Held.Yaw, Held.Pose, 0.0, &Looks[Held.Resident], {}, false });
	Heard.Reset();   // for the sound: the residents near, where they are and how they move (SND-017)
	for (const FNear& A : Near) if (A.i >= 0 && A.i < Day->Num())
		Heard.Add({ A.i, A.P, A.Pose, A.Phase, Extras.IsValidIndex(A.Extra) ? Extras[A.Extra].Shout : FString(), Extras.IsValidIndex(A.Extra) && Extras[A.Extra].Sig == TEXT("Run") });
	// whoever rides in a vehicle near enough (cars.js occupants() and seat()): its owner at the wheel and neighbours sharing
	// the ride beside and behind (CITY-094), a bus's driver and riders on its benches, someone from out of town in a van or a
	// car passing through; seated as low as their head needs under the roof, feet along the floor, hands on the wheel
	if (Traffic)
	{
		TMap<int32, TArray<int32>> ByCar;
		for (int32 i = 0; i < InVehicle.Num(); ++i) if (InVehicle[i] >= 0) ByCar.FindOrAdd(InVehicle[i]).Add(i);
		const double Ride2 = (double)RideDistance * RideDistance;
		for (int32 k = 0; k < Traffic->Num(); ++k)
		{
			if (Traffic->Moved.Contains(k)) continue;   // the player has driven it: they sit in it themselves (a bus they ride is not moved)
			const TOptional<FBrixCarAt> C = Traffic->At(k, Minute);
			if (!C || FVector2D::DistSquared(C->Pos, Here) > Ride2) continue;
			const FBrixVehicle& V = Traffic->Vehicles[k];
			const bool bBus = V.Kind == 2;
			const FBrixCarModel* Md = Traffic->ModelOf(k);
			if (!bBus && !Md) continue;
			struct FOcc { const FBrixLook* L; int32 Key; float A, Wd; };
			TArray<FOcc> Occ;
			const TArray<int32>* Ins = ByCar.Find(k);
			if (bBus)
			{
				const float L = 11.f, W = 2.5f;
				Occ.Add({ &Drivers[k], 1000000 + 2 * k, 4.5f, -.72f });
				if (Ins) for (int32 n = 0; n < FMath::Min(14, Ins->Num()); ++n)
					Occ.Add({ &Looks[(*Ins)[n]], (*Ins)[n], L / 2 - 2.3f - (n / 2) * 1.1f, (n % 2 ? 1.f : -1.f) * (W / 2 - .55f) });
			}
			else if (Ins && Ins->Num())
			{
				const int32 Wheel = Ins->Contains(V.Owner) ? V.Owner : (*Ins)[0];
				Occ.Add({ &Looks[Wheel], Wheel, Md->Seat.X, Md->Seat.Y });
				TArray<FVector2f> Seats = { FVector2f(Md->Seat.X, -Md->Seat.Y) };
				if (Md->Back) { Seats.Add(FVector2f(*Md->Back, Md->Seat.Y)); Seats.Add(FVector2f(*Md->Back, -Md->Seat.Y)); }
				int32 n = 0;
				for (const int32 r : *Ins) if (r != Wheel && n < Seats.Num()) { Occ.Add({ &Looks[r], r, Seats[n].X, Seats[n].Y }); ++n; }
			}
			else if (!(V.Home && V.Kind != 1) && C->bTrip)   // a van out delivering or a car passing through: its own driver
			{
				Occ.Add({ &Drivers[k], 1000000 + 2 * k, Md->Seat.X, Md->Seat.Y });
				if (V.Kind != 1 && BrixSliceHash({ FString::FromInt(k), TEXT("pass") }) < .45) Occ.Add({ &Passengers[k], 1000000 + 2 * k + 1, Md->Seat.X, -Md->Seat.Y });
			}
			const float SeatY0 = bBus ? .95f : Md->SeatY;
			const double Yaw = FMath::Atan2(C->Heading.X, -C->Heading.Y);
			for (int32 n = 0; n < Occ.Num(); ++n)
			{
				const FOcc& O = Occ[n];
				const FVector2D P = C->Pos + C->Heading * O.A + FVector2D(-C->Heading.Y, C->Heading.X) * O.Wd;
				FNear R{ O.Key, FVector2D::Distance(P, Here), P, 0.f, Yaw, 2, 0.0, O.L, {}, false };
				if (bBus) R.Up = SeatY0;
				else
				{
					const float Roof = Md->Roof ? *Md->Roof : 1.56f, Head = (O.L->Cap ? 1.06f : O.L->Hair == TEXT("afro") ? 1.12f : O.L->Hair == TEXT("bun") ? 1.06f : 1.04f) * (float)O.L->S;
					const float Sy = FMath::Min(SeatY0, Roof - .05f - Head), Floor = Md->Floor ? *Md->Floor + .03f : .3f;
					R.Up = Sy;
					R.Floor = Floor - Sy;
					R.bDriving = n == 0;
				}
				Near.Add(R);
			}
		}
	}
	const double T1 = FPlatformTime::Seconds();
	Near.Sort([](const FNear& A, const FNear& B) { return A.D < B.D; });
	if (Near.Num() > MaxBodies) Near.SetNum(MaxBodies);

	// how much of each to draw: meshes for the nearest within NearDistance, blocks beyond
	TArray<int32> LodOf;
	LodOf.SetNum(Near.Num());
	int32 Full = 0, MeshCount = 0;
	for (int32 n = 0; n < Near.Num(); ++n)
	{
		const double D = Near[n].D;
		int32 Lod = Near[n].i == Held.Resident ? 0 : D < FullDistance && Full < FullCount ? 0 : D < MidDistance ? 1 : D < NearDistance ? 2 : 3;   // the one talking: their whole face
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

	BoxScratch.Reset();
	struct FJob { int32 Slot; const FBrixLook* L; FBrixDrawOptions O; };
	TArray<FJob> Jobs;
	for (int32 n = 0; n < Near.Num(); ++n)
	{
		const FNear& P = Near[n];
		const FBrixLook& L = *P.L;
		FBrixDrawOptions O;
		O.Floor = P.Floor;
		O.bDriving = P.bDriving;
		if (P.i == Held.Resident) O.Face = HeldFace;
		if (Wets.IsValidIndex(P.Wet))   // dressed for the rain (a dodge's run and gestures below come first)
		{
			const FBrixWet& X = Wets[P.Wet];
			O.Hood = X.Hood; O.Umbrella = X.Umbrella;
			O.Sig = X.Sig;
			O.Arms[0] = X.Arms[0]; O.Arms[1] = X.Arms[1];
			O.Chest = X.Chest;
		}
		if (Extras.IsValidIndex(P.Extra))   // a dodge's run and gestures
		{
			const FBrixDodged& X = Extras[P.Extra];
			O.Sig = X.Sig;
			O.Arms[0] = X.Arms[0]; O.Arms[1] = X.Arms[1];
			O.Chest = X.Chest;
		}
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
		if (Slot.Lod != O.Lod || ++Slot.Age >= RebuildEvery[FMath::Clamp(O.Lod, 0, 2)] || Slot.Verts == 0 || O.Face) Jobs.Add({ s, &L, O });   // a talking face every frame
		UProceduralMeshComponent* M = Meshes[s];
		M->SetWorldLocationAndRotation(ToWorld(P.P, P.Up), FRotator(0.0, FMath::RadiansToDegrees(P.Yaw), 0.0));
		M->SetVisibility(true);
	}
	const double T2 = FPlatformTime::Seconds();
	// the bodies due a rebuild, built at once on the task threads (the body as people.js draws it facing yaw 0 at the origin,
	// in Unreal's centimetres; its component turns and places it), then handed to their meshes here
	if (Built.Num() < Slots.Num()) Built.SetNum(Slots.Num());
	ParallelFor(Jobs.Num(), [&](int32 j)
	{
		FBuilt& B = Built[Jobs[j].Slot];
		B.Body.Reset();
		DrawBrixPerson(Rules, *Jobs[j].L, FVector::ZeroVector, 0.0, Jobs[j].O, B.Body);
		B.Arrays.From(B.Body, bFlipWinding);
	});
	const double T3 = FPlatformTime::Seconds();
	for (const FJob& J : Jobs)
	{
		FSlot& Slot = Slots[J.Slot];
		Slot.Verts = Built[J.Slot].Arrays.Upload(Meshes[J.Slot], Slot.Verts, BodyMaterial);
		Slot.Lod = J.O.Lod;
		Slot.Age = 0;
	}
	const double T4 = FPlatformTime::Seconds();
	TArray<FVector> V, N;
	TArray<FLinearColor> C;
	TArray<int32> Tri;
	const TArray<FVector2D> NoUV;
	const TArray<FProcMeshTangent> NoTangents;
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
	// the shouts, over the residents shouting them, turned to the camera (CITY-071: a tag, no words spoken)
	{
		int32 t = 0;
		TArray<FVector2D> Shouted;
		for (const FNear& P : Near)
		{
			if (!Extras.IsValidIndex(P.Extra) || Extras[P.Extra].Shout.IsEmpty() || P.D > Dodge.R.TagM) continue;
			bool bCrowded = false;   // in a crowd, one shout of those close together (the nearest), so they can be read
			for (const FVector2D& O : Shouted) bCrowded |= FVector2D::Distance(O, P.P) < ShoutGap;
			if (bCrowded) continue;
			Shouted.Add(P.P);
			if (t >= ShoutTags.Num())
			{
				UTextRenderComponent* T = NewObject<UTextRenderComponent>(this);
				T->SetupAttachment(RootComponent);
				T->SetHorizontalAlignment(EHTA_Center);
				T->SetVerticalAlignment(EVRTA_TextBottom);
				T->SetWorldSize(ShoutSize * CmPerM);
				T->SetTextRenderColor(ShoutColour.ToFColor(true));
				T->SetCollisionEnabled(ECollisionEnabled::NoCollision);
				T->CastShadow = false;
				T->RegisterComponent();
				AddInstanceComponent(T);
				ShoutTags.Add(T);
			}
			UTextRenderComponent* T = ShoutTags[t++];
			const FVector At = ToWorld(P.P, P.Up + 2.05f * (float)P.L->S);
			T->SetText(FText::FromString(Extras[P.Extra].Shout));
			T->SetWorldLocationAndRotation(At, (Eye - At).Rotation());
			T->SetVisibility(true);
		}
		for (; t < ShoutTags.Num(); ++t) ShoutTags[t]->SetVisibility(false);
	}
	const double T5 = FPlatformTime::Seconds();
	Parts[0] += T1 - T0; Parts[1] += T2 - T1; Parts[2] += T3 - T2; Parts[3] += T4 - T3; Parts[4] += T5 - T4;
	TickSeconds += T5 - T0;
	++TickFrames;
}
