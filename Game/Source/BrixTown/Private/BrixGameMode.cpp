#include "BrixGameMode.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixInside.h"
#include "BrixCars.h"
#include "BrixCarsActor.h"
#include "BrixCrashActor.h"
#include "BrixWeatherActor.h"
#include "BrixLamps.h"
#include "BrixLifeActor.h"
#include "BrixMall.h"
#include "BrixMallActor.h"
#include "BrixDesignerActor.h"
#include "BrixMapActor.h"
#include "BrixPeople.h"
#include "BrixRooms.h"
#include "BrixSoundActor.h"
#include "BrixTalkActor.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/ExponentialHeightFogComponent.h"
#include "Components/SkyAtmosphereComponent.h"
#include "Components/SkyLightComponent.h"
#include "Engine/DirectionalLight.h"
#include "Engine/ExponentialHeightFog.h"
#include "Engine/SkyLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerStart.h"
#include "HAL/PlatformMisc.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/CommandLine.h"
#include "Misc/Paths.h"
#include "TimerManager.h"
#include "RHI.h"
#include "UnrealClient.h"

using BrixUnits::ToWorld;

ABrixGameMode::ABrixGameMode()
{
	DefaultPawnClass = ABrixCharacter::StaticClass();
	PrimaryActorTick.bCanEverTick = true;
}

void ABrixGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
	Super::InitGame(MapName, Options, ErrorMessage);
	City = MakeShared<FBrixCity>();
	FString Error;
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), CityDir));
	if (!LoadBrixCity(Dir, *City, Error))
	{
		UE_LOG(LogTemp, Error, TEXT("Brix Town: the city did not load from %s: %s"), *Dir, *Error);
		City.Reset();
		return;
	}
	for (const FString& Problem : CheckBrixCity(*City)) UE_LOG(LogTemp, Warning, TEXT("Brix Town data: %s"), *Problem);
	// the clock: the city's spawn time on StartDay, or what the command line asks for
	Day = StartDay;
	FParse::Value(FCommandLine::Get(), TEXT("BrixDay="), Day);
	FString Time = City->SpawnTime;
	FParse::Value(FCommandLine::Get(), TEXT("BrixTime="), Time);
	FString Hh, Mm;
	if (Time.Split(TEXT(":"), &Hh, &Mm)) StartMinute = FCString::Atoi(*Hh) * 60.0 + FCString::Atoi(*Mm);
	// the residents' day: the same day the slice is built for
	Today = MakeShared<FBrixDay>();
	FString DayError;
	if (!LoadBrixDay(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), DayFile)), *Today, DayError))
	{
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: no residents (%s): run Tools/worldgen/build_play.py"), *DayError);
		Today.Reset();
	}
	else if (Today->Day != Day)
		UE_LOG(LogTemp, Warning, TEXT("Brix Town: the residents' routines are for day %d, the clock is on day %d"), Today->Day, Day);
}

AActor* ABrixGameMode::ChoosePlayerStart_Implementation(AController* Player)
{
	if (!City) return Super::ChoosePlayerStart_Implementation(Player);
	if (!Start)
	{
		const FVector At = ToWorld(City->SpawnPos, 1.2f);
		const FRotator Facing(0.f, FMath::RadiansToDegrees(FMath::Atan2(City->SpawnFacing.Y, City->SpawnFacing.X)), 0.f);
		FActorSpawnParameters P;
		P.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		Start = GetWorld()->SpawnActor<APlayerStart>(APlayerStart::StaticClass(), At, Facing, P);
	}
	return Start;
}

void ABrixGameMode::StartPlay()
{
	if (City)
	{
		ABrixTownActor* Town = GetWorld()->SpawnActor<ABrixTownActor>();
		Insides = MakeShared<FBrixInsides>();
		FString InsideError;
		if (FParse::Param(FCommandLine::Get(), TEXT("BrixShut"))) { InsideError = TEXT("-BrixShut"); Insides.Reset(); }   // to compare: every building solid, as before
		else if (LoadBrixInside(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/inside.json"))), *Insides, InsideError))
			Town->Insides = Insides.Get();
		if (!Town->Insides) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: the buildings are shut (%s)"), *InsideError); Insides.Reset(); }
		// Eastgate Mall, the one mall whose inside is built (WLD-058): drawn by its own actor, not as the town's box
		TSharedPtr<FBrixMall> MallData = MakeShared<FBrixMall>();
		FString MallError;
		if (Insides && !FParse::Param(FCommandLine::Get(), TEXT("BrixNoMall")) && LoadBrixMall(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), CityDir)), *MallData, MallError))
			Town->OwnBuilt.Add(MallData->LotId);
		else { if (Insides) UE_LOG(LogTemp, Warning, TEXT("Brix Town: no mall inside (%s)"), *MallError); MallData.Reset(); }
		Town->Breakable = ABrixCrash::BreakableKinds();   // street things and trees a car can break (CITY-072)
		Town->Build(*City);
		Crash = GetWorld()->SpawnActor<ABrixCrash>();
		Crash->Start(Town, *City, StartDay);
		Weather = GetWorld()->SpawnActor<ABrixWeather>();
		Weather->Start(*City, Town);
		TownActor = Town;
		if (Insides)   // the homes near the camera, furnished from their plans (FURN-060 to FURN-063)
		{
			Furnisher = GetWorld()->SpawnActor<ABrixRooms>();
			Furnisher->Start(*City, *Insides, Town, Day);
		}
		if (MallData)
		{
			Mall = GetWorld()->SpawnActor<ABrixMall>();
			Mall->Start(MallData, Town);
		}
		// the inside of every hollow building, to know when the camera is indoors (CITY-013)
		int32 Lk = 0;
		for (const FBrixBlock& B : City->Blocks)
			for (const FBrixLot& L : B.Lots)
			{
				const FBrixInside* In = Insides && Insides->Lots.IsValidIndex(Lk) && Insides->Lots[Lk].IsSet() ? &Insides->Lots[Lk].GetValue() : nullptr;
				if (MallData && L.Id == MallData->LotId)   // the mall: its hall, under its roof
				{
					FRoom R;
					R.Lot = Lk;
					R.Centre = FVector2D((MallData->X0 + MallData->X1) / 2, (MallData->Z0 + MallData->Z1) / 2);
					R.Half = FVector2D((MallData->X1 - MallData->X0) / 2, (MallData->Z1 - MallData->Z0) / 2);
					R.Top = (float)MallData->H;
					Rooms.Add(R);
				}
				else if (In && BrixIsHollow(L, In))
				{
					FRoom R;
					R.Lot = Lk;
					R.Centre = L.Centre();
					const double A = FMath::DegreesToRadians(L.AngleDeg);
					R.Cos = FMath::Cos(A); R.Sin = FMath::Sin(A);
					const FVector2D Size = L.House.IsSet() ? FVector2D(12, 9) : L.Size();   // a designed house: its plan (house_12x9)
					R.Half = Size / 2 - FVector2D(0.3, 0.3);
					R.Top = Town->BlockSlabHeight + Town->StoreyHeight * FMath::Max(1, L.Floors);
					Rooms.Add(R);
				}
				++Lk;
			}
		GetWorld()->SpawnActor<ABrixLamps>()->Start(Town);   // the lamps light at night (WLD-028)
		Traffic = MakeShared<FBrixTraffic>();
		FString CarError;
		const FString Data = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data")));
		if (LoadBrixTraffic(FPaths::Combine(Data, TEXT("cars.json")), FPaths::Combine(Data, TEXT("car_models.json")), *Traffic, CarError))
		{
			Cars = GetWorld()->SpawnActor<ABrixCars>();
			TArray<FBox2D> Paving;
			for (const FBrixBlock& B : City->Blocks) Paving.Add(FBox2D(B.Origin, B.Origin + B.Extent));
			Cars->Start(Traffic, City->Tram, Paving);
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %d vehicles today"), Traffic->Num());
		}
		else UE_LOG(LogTemp, Warning, TEXT("Brix Town: no vehicles (%s); run Tools/worldgen/build_play.py"), *CarError);
		if (Today)
		{
			if (Insides)   // at home inside: in bed, on the sofa (CITY-013, WLD-053)
				UE_LOG(LogTemp, Display, TEXT("Brix Town: %d of the day's stays at home are in a bed or on a seat"), SeatBrixDayIndoors(*Today, *City, *Insides, FBrixDoorRules::FromConfig()));
			if (Mall && Mall->Data())   // Eastgate's keepers, guards, janitors and visitors at their places (CITY-105, CITY-106)
			{
				int32 Mi = 0, k = 0;
				for (const FBrixBlock& B : City->Blocks) for (const FBrixLot& L : B.Lots) { if (L.Id == Mall->Data()->LotId) Mi = k; ++k; }
				UE_LOG(LogTemp, Display, TEXT("Brix Town: %d of the day's stays in %s are at its places"), SeatBrixDayInMall(*Today, *Mall->Data(), Mi), *Mall->Data()->Name);
			}
			People = GetWorld()->SpawnActor<ABrixPeople>();
			People->Start(*City, Today, Cars ? Traffic : nullptr);
			UE_LOG(LogTemp, Display, TEXT("Brix Town: %d residents, their day %d (%s, %s)"), Today->Num(), Today->Day, *Today->Weekday, *Today->Season);
			Talk = GetWorld()->SpawnActor<ABrixTalk>();
			Talk->Start(*City, Today, People);
			GetWorld()->SpawnActor<ABrixSound>()->Start(*City, Today, People, Cars ? Traffic : nullptr);   // the town's sound, recorded from the slice (SND-002 to SND-004, SND-017)
		}
	}
	if (City)
	{
		Life = GetWorld()->SpawnActor<ABrixLife>();   // money and the bank (ADR-0012)
		Life->Start(*City, Today);
		Designer = GetWorld()->SpawnActor<ABrixDesigner>();   // a new game opens the character designer (CHR-001)
		Designer->Start(City->Name);
		Map = GetWorld()->SpawnActor<ABrixMap>();   // the maps (UI-012, UI-018)
		Map->Start(*City, Today, FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), CityDir)));
	}
	SpawnLight();
	Super::StartPlay();

	FString Cam;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixCam="), Cam, false))   // false: the value has commas
	{
		TArray<FString> P;
		Cam.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 5)
		{
			const FVector At = ToWorld(FVector2D(FCString::Atof(*P[0]), FCString::Atof(*P[1])), FCString::Atof(*P[2]));
			Eye = GetWorld()->SpawnActor<ACameraActor>(At, FRotator(FCString::Atof(*P[4]), FCString::Atof(*P[3]), 0.f));
			// The player is possessed after this and would take the view back: look through the camera a moment later.
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, [this]()
			{
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController())
				{
					PC->bAutoManageActiveCameraTarget = false;
					PC->SetViewTarget(Eye);
				}
			}, 1.f, false);
		}
	}
	// -BrixTalk=<resident> (with -BrixAnswers=1,2,...): to see a talk without anyone at the keys, the player is put beside
	// that resident a third of the way into a test shot's wait, the talk opened, and the answers given a second apart
	int32 TalkTo = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixTalk="), TalkTo) && TalkTo >= 0 && Today)
	{
		FString Answers;
		FParse::Value(FCommandLine::Get(), TEXT("BrixAnswers="), Answers, false);
		float Gap = 1.5f;   // -BrixAnswerGap=<s>: seconds between the answers (a shot of the player answering wants one late)
		FParse::Value(FCommandLine::Get(), TEXT("BrixAnswerGap="), Gap);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, TalkTo, Answers, Gap]()
		{
			const TOptional<FBrixWhere> W = Today->Where(TalkTo, ClockMinute());
			APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
			if (!W || !Me) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: resident %d is not out to talk to"), TalkTo); return; }
			const FVector2D P = W->Pos + W->Heading * 1.4;   // in front of them
			Me->SetActorLocation(ToWorld(P, 1.f), false, nullptr, ETeleportType::TeleportPhysics);
			FTimerHandle O;
			GetWorldTimerManager().SetTimer(O, [this, Answers, Gap]()
			{
				if (!Talk) return;
				Talk->Open();
				UE_LOG(LogTemp, Display, TEXT("Brix Town: talking: %s"), Talk->IsTalking() ? TEXT("yes") : TEXT("nobody near enough"));
				TArray<FString> Picks;
				Answers.ParseIntoArray(Picks, TEXT(","));
				for (int32 k = 0; k < Picks.Num(); ++k)
				{
					FTimerHandle A;
					const int32 n = FCString::Atoi(*Picks[k]) - 1;
					GetWorldTimerManager().SetTimer(A, [this, n]() { if (Talk) Talk->Choose(n); }, Gap * (k + 1), false);
				}
			}, 0.3f, false);
		}, ShotDelaySeconds / 3, false);
	}
	// -BrixDrive=<vehicle> (with -BrixHold=steer,throttle): a third of the way into a test shot's wait the player is put by
	// that parked car's door, gets in as with E, and holds the keys from then on
	int32 DriveK = -1;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixDrive="), DriveK) && DriveK >= 0 && Traffic)
	{
		FString Hold;
		FParse::Value(FCommandLine::Get(), TEXT("BrixHold="), Hold, false);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, DriveK, Hold]()
		{
			const TOptional<FBrixCarAt> C = Traffic->At(DriveK, ClockMinute());
			ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			if (!C || !Me) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: vehicle %d is not in the town to drive"), DriveK); return; }
			const FVector3f D = Traffic->Dims(DriveK);
			const FVector2D Door = C->Pos - FVector2D(-C->Heading.Y, C->Heading.X) * (D.Y / 2 + .6);
			Me->SetActorLocation(ToWorld(Door, 1.f), false, nullptr, ETeleportType::TeleportPhysics);
			FTimerHandle O;
			GetWorldTimerManager().SetTimer(O, [this, Me, Hold]()
			{
				Me->TestPress();
				UE_LOG(LogTemp, Display, TEXT("Brix Town: driving: %s"), Cars && Cars->Driving().K >= 0 ? TEXT("yes") : TEXT("no car near enough"));
				TArray<FString> V;
				Hold.ParseIntoArray(V, TEXT(","));
				if (V.Num() == 2) Me->TestHold = FVector2D(FCString::Atof(*V[0]), FCString::Atof(*V[1]));
			}, 0.3f, false);
		}, ShotDelaySeconds / 3, false);
	}
	// -BrixCounter=bank (with -BrixLife=open,current;loan,5000,3...): a third of the way into a test shot's wait the ops are
	// done as the player would at the counter, then the player is put at its door and the counter opened; -BrixLife= alone
	// does the ops (-BrixLifeLead=<s>: that many seconds before the shot, to catch the coins of a reward in flight)
	FString CounterAt, Ops;
	FParse::Value(FCommandLine::Get(), TEXT("BrixCounter="), CounterAt);
	FParse::Value(FCommandLine::Get(), TEXT("BrixLife="), Ops, false);
	if ((!CounterAt.IsEmpty() || !Ops.IsEmpty()) && Life)
	{
		float Lead = 0.f;
		FParse::Value(FCommandLine::Get(), TEXT("BrixLifeLead="), Lead);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, CounterAt, Ops]()
		{
			if (!Life) return;
			if (!Ops.IsEmpty()) Life->RunOps(Ops);
			if (!CounterAt.IsEmpty()) UE_LOG(LogTemp, Display, TEXT("Brix Town: the counter at %s: %s"), *CounterAt, Life->OpenCounterAt(CounterAt) ? TEXT("open") : TEXT("not there"));
		}, Lead > 0 ? FMath::Max(.5f, ShotDelaySeconds - Lead) : ShotDelaySeconds / 3, false);
	}
	// -BrixInside=<lot> / -BrixWalkIn=<lot>: to see inside a building without anyone at the keys (CITY-013)
	FString InLot;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixInside="), InLot)) PutInside(InLot, false);
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixWalkIn="), InLot)) PutInside(InLot, true);
	FString MenuPage;   // -BrixMenu=<page>: the game menu open at that page for the test shot (ABrixLife::TestMenu)
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixMenu="), MenuPage) && Life)
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, MenuPage]() { if (Life) Life->TestMenu(MenuPage); }, FMath::Max(1.f, ShotDelaySeconds - 3.f), false);
	}
	FString LiftLot;   // -BrixLift=<lot>: at the tower's ground-floor lift call, E pressed to go to the top floor (WLD-051)
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixLift="), LiftLot) && City && Insides)
	{
		int32 Lk = 0;
		for (const FBrixBlock& B : City->Blocks)
			for (const FBrixLot& L : B.Lots)
			{
				if (L.Id == LiftLot && Insides->Lots.IsValidIndex(Lk) && Insides->Lots[Lk].IsSet() && TownActor)
					for (const FBrixLiftCall& C : BrixLiftCalls(L, Insides->Lots[Lk].GetValue(), FBrixDoorRules::FromConfig(), 0.f))
						if (C.Floor == 0 && C.Text.Contains(TEXT("top floor")))
						{
							const FVector At = FVector(C.At.X, C.At.Y, C.At.Z + 0.95) * BrixUnits::CmPerM;
							FTimerHandle H;
							GetWorldTimerManager().SetTimer(H, [this, At]()
							{
								ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
								if (!Me) return;
								Me->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
								FTimerHandle P;
								GetWorldTimerManager().SetTimer(P, [Me]() { Me->TestPress(); }, 0.5f, false);
							}, 0.6f, false);
						}
				++Lk;
			}
		if (Mall && Mall->Data() && Mall->Data()->LotId == LiftLot)   // Eastgate: the ground floor's call up to the top floor
			for (const FBrixMallCall& C : Mall->Data()->Calls)
				if (C.Kind == FBrixMallCall::Use && C.A[1] < 1 && C.Text.Contains(FString::Printf(TEXT("up to floor %d"), Mall->Data()->Levels - 1)))
				{
					const FVector At = FVector(C.A[0], C.A[2], C.A[1] + 0.95) * BrixUnits::CmPerM;
					FTimerHandle H;
					GetWorldTimerManager().SetTimer(H, [this, At]()
					{
						ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
						if (!Me) return;
						Me->SetActorLocation(At, false, nullptr, ETeleportType::TeleportPhysics);
						FTimerHandle P;
						GetWorldTimerManager().SetTimer(P, [Me]() { Me->TestPress(); }, 0.5f, false);
					}, 0.6f, false);
					break;
				}
	}
	FString Way;   // -BrixWalk=x,z;x,z;...: after the other switches have placed the player, walk them through these points
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixWalk="), Way, false))
	{
		TArray<FString> Pts;
		Way.ParseIntoArray(Pts, TEXT(";"));
		for (const FString& Pt : Pts) { FString A, B; if (Pt.Split(TEXT(","), &A, &B)) WalkPoints.Add(FVector2D(FCString::Atof(*A), FCString::Atof(*B))); }
	}
	FString Stand;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixStand="), Stand, false))   // x,z,yaw: the player stands there, facing that way
	{
		TArray<FString> P;
		Stand.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 3)
		{
			const FVector At = ToWorld(FVector2D(FCString::Atof(*P[0]), FCString::Atof(*P[1])), 1.2f);
			const float Yaw = FCString::Atof(*P[2]);
			FTimerHandle H;
			GetWorldTimerManager().SetTimer(H, [this, At, Yaw]()
			{
				if (APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0)) Me->SetActorLocationAndRotation(At, FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
				if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-15.f, Yaw, 0.f));
			}, 0.6f, false);
		}
	}
	// -BrixNear=<resident>[,<metres>]: the player stands 3.5 m (or that far) in front of them, facing them, a third into a
	// test shot's wait (the names and the markers over them, ADR-0051)
	int32 NearTo = -1;
	FString NearArg;
	double NearM = 3.5;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixNear="), NearArg, false))
	{
		FString A, B;
		if (NearArg.Split(TEXT(","), &A, &B)) { NearTo = FCString::Atoi(*A); NearM = FCString::Atod(*B); }
		else NearTo = FCString::Atoi(*NearArg);
	}
	if (NearTo >= 0 && Today)
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, NearTo, NearM]()
		{
			const TOptional<FBrixWhere> W = Today->Where(NearTo, ClockMinute());
			APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
			if (!W || !Me) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: resident %d is not out"), NearTo); return; }
			const FVector2D P = W->Pos + W->Heading * NearM;
			Me->SetActorLocation(ToWorld(P, 1.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0)) PC->SetControlRotation(FRotator(-8.0, FMath::RadiansToDegrees(FMath::Atan2(-W->Heading.Y, -W->Heading.X)), 0));
		}, ShotDelaySeconds / 3, false);
	}
	// -BrixBoard=<kind>: on that skateboard a third into a test shot's wait (CITY-099; with -BrixHold=steer,throttle)
	FString BoardKind;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixBoard="), BoardKind))
	{
		FString Hold;
		FParse::Value(FCommandLine::Get(), TEXT("BrixHold="), Hold, false);
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, BoardKind, Hold]()
		{
			ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0));
			if (!Me) return;
			Me->TestBoard(BoardKind);
			TArray<FString> P;
			Hold.ParseIntoArray(P, TEXT(","));
			if (P.Num() == 2) Me->TestHold = FVector2D(FCString::Atof(*P[0]), FCString::Atof(*P[1]));
			UE_LOG(LogTemp, Display, TEXT("Brix Town: on the %s board: %s"), *BoardKind, Me->Boards.Riding() ? TEXT("yes") : TEXT("no"));
		}, ShotDelaySeconds / 3, false);
	}
	// -BrixMark=x,z sets the mark; -BrixMap opens the big map (and -BrixMapZoom=z,x,z zooms it on a place) for a test shot
	FString MarkAt, Zoom;
	if (Map && FParse::Value(FCommandLine::Get(), TEXT("BrixMark="), MarkAt, false))
	{
		TArray<FString> P;
		MarkAt.ParseIntoArray(P, TEXT(","));
		if (P.Num() == 2) Map->Mark.MarkAt(FVector2D(FCString::Atod(*P[0]), FCString::Atod(*P[1])), FString(), false);
	}
	if (Map && (FParse::Param(FCommandLine::Get(), TEXT("BrixMap")) || FParse::Value(FCommandLine::Get(), TEXT("BrixMapZoom="), Zoom, false)))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, [this, Zoom]()
		{
			Map->Open();
			TArray<FString> P;
			Zoom.ParseIntoArray(P, TEXT(","));
			if (P.Num() == 3)
			{
				const double K9 = 900.0 * Map->Image.K / Map->Image.N;
				const FVector2D At = (FVector2D(FCString::Atod(*P[1]), FCString::Atod(*P[2])) + FVector2D(Map->Image.O, Map->Image.O)) * K9;
				Map->View.Z = FMath::Clamp(FCString::Atod(*P[0]), 1.0, 6.0);
				Map->View.Cx = At.X;
				Map->View.Cy = At.Y;
				Map->View.Clamp();
			}
		}, ShotDelaySeconds / 3, false);
	}
	FString Shot;
	if (FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot))
	{
		FTimerHandle H;
		GetWorldTimerManager().SetTimer(H, this, &ABrixGameMode::ShootAndQuit, ShotDelaySeconds, false);
		// a test shot is not played: the mouse and keys of whoever is at this machine do not move it
		FTimerHandle K;
		GetWorldTimerManager().SetTimer(K, [this]()
		{
			if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) { PC->SetIgnoreLookInput(true); PC->SetIgnoreMoveInput(true); }
		}, 0.2f, false);
		FTimerHandle M;   // the frame rate is counted over the second half of the wait, after start-up work has settled
		GetWorldTimerManager().SetTimer(M, [this]() { MarkFrame = GFrameCounter; MarkSeconds = FPlatformTime::Seconds(); if (People) People->ResetTickTime(); }, ShotDelaySeconds * 0.5f, false);
	}
}

void ABrixGameMode::PutInside(const FString& LotId, bool bWalk)
{
	if (!City) return;
	const FBrixLot* Found = nullptr;
	const FBrixBlock* In = nullptr;
	for (const FBrixBlock& B : City->Blocks) for (const FBrixLot& L : B.Lots) if (L.Id == LotId) { Found = &L; In = &B; }
	if (!Found) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: no lot %s to go into"), *LotId); return; }
	const FBrixDoorway Dw = BrixDoorwayOf(*City, *Found, In->District, FBrixDoorRules::FromConfig());
	const FVector2D Inward = -Dw.Out;
	const float Deep = (float)FMath::Min(Found->Size().X, Found->Size().Y);
	const bool bMall = Mall && Mall->Data() && Mall->Data()->LotId == LotId;   // Eastgate: in along its south concourse to the court
	const FVector2D Inside = Dw.Centre + Inward * (bMall ? MallInside : FMath::Clamp(Deep * 0.45f, 1.5f, 5.f));
	const FVector2D From = bWalk ? Dw.Centre - Inward * 3.0 : Inside;
	const FRotator Facing(0.f, FMath::RadiansToDegrees(FMath::Atan2(Inward.Y, Inward.X)), 0.f);
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, [this, From, Inside, Facing, bWalk, LotId]()
	{
		APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!Me) return;
		Me->SetActorLocationAndRotation(ToWorld(From, 1.f), Facing, false, nullptr, ETeleportType::TeleportPhysics);
		if (APlayerController* PC = GetWorld()->GetFirstPlayerController()) PC->SetControlRotation(FRotator(-12.f, Facing.Yaw, 0.f));
		if (bWalk) { WalkTo = Inside; WalkFrom = From; WalkLot = LotId; }
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the player is %s %s"), bWalk ? TEXT("at the door of") : TEXT("inside"), *LotId);
	}, 0.5f, false);
}

void ABrixGameMode::SpawnLight()
{
	// Lights are set up before they are first registered (deferred spawn): a sky light made movable and real-time after
	// registering keeps its first, empty capture and leaves the shade black. The sun is then turned by the clock (UpdateSun).
	UWorld* W = GetWorld();
	const FTransform SunAt(FRotator(-30.f, 180.f, 0.f));
	ADirectionalLight* Sun = W->SpawnActorDeferred<ADirectionalLight>(ADirectionalLight::StaticClass(), SunAt);
	UDirectionalLightComponent* SunLight = CastChecked<UDirectionalLightComponent>(Sun->GetLightComponent());
	SunLight->Mobility = EComponentMobility::Movable;
	SunLight->Intensity = SunLux;
	SunLight->LightColor = SunColour.ToFColor(true);
	SunLight->bAtmosphereSunLight = true;
	Sun->FinishSpawning(SunAt);
	SunActor = Sun;
	StartSeconds = W->GetTimeSeconds();
	UpdateSun();
	if (City)
	{
		const FBrixSky& S = City->Sky;
		UE_LOG(LogTemp, Display, TEXT("Brix Town: day %d (%s), %02d:%02d"), Day, *S.Seasons[S.SeasonOf(Day)], (int32)StartMinute / 60, (int32)StartMinute % 60);
	}

	ASkyAtmosphere* Atmosphere = W->SpawnActor<ASkyAtmosphere>();
	USkyAtmosphereComponent* Air = Atmosphere->GetComponent();
	Air->SetMieScatteringScale(Air->MieScatteringScale * SkyHaziness);
	ASkyLight* Sky = W->SpawnActorDeferred<ASkyLight>(ASkyLight::StaticClass(), FTransform::Identity);
	USkyLightComponent* SkyLight = Sky->GetLightComponent();
	SkyLight->Mobility = EComponentMobility::Movable;
	SkyLight->bRealTimeCapture = true;
	SkyLight->Intensity = SkyLightIntensity;
	Sky->FinishSpawning(FTransform::Identity);
	SkyFill = SkyLight;
	SkyLight->RecaptureSky();
	FTimerHandle H;   // once the sky has been drawn: capture it again, and say what the sky light holds
	GetWorldTimerManager().SetTimer(H, [SkyLight]()
	{
		SkyLight->RecaptureSky();
		UE_LOG(LogTemp, Display, TEXT("Brix Town: sky light movable %d, real-time %d, intensity %.2f, visible %d"),
			SkyLight->Mobility == EComponentMobility::Movable, SkyLight->bRealTimeCapture, SkyLight->Intensity, SkyLight->IsVisible());
	}, 2.f, false);

	AExponentialHeightFog* Haze = W->SpawnActor<AExponentialHeightFog>();
	Haze->GetComponent()->SetFogDensity(HazeDensity);
	Haze->GetComponent()->SetFogHeightFalloff(HazeFalloff);
	Haze->GetComponent()->SetFogInscatteringColor(HazeColour);
	HazeFog = Haze->GetComponent();
	UpdateSun();
}

void ABrixGameMode::SetClock(double Minutes)
{
	StartMinute = Minutes;
	StartSeconds = GetWorld()->GetTimeSeconds();
}

double ABrixGameMode::ClockMinutes() const
{
	const float Pace = City ? City->Sky.RealSecondsPerMinute : 10.f;
	return StartMinute + (GetWorld()->GetTimeSeconds() - StartSeconds) / FMath::Max(0.01, (double)Pace) * ClockRate;
}

void ABrixGameMode::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	UpdateSun();
	if (TownActor) TownActor->SetSignals(Day, ClockMinutes());   // the traffic lights on traffic.py's cycle (CITY-045)
	if (!WalkTo && WalkPoints.Num() && GetWorld()->GetTimeSeconds() > 1.0)   // -BrixWalk: the next point
	{
		WalkTo = WalkPoints[0];
		WalkPoints.RemoveAt(0);
		WalkSeconds = 0.f;
		if (const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0)) WalkFrom = FVector2D(Me->GetActorLocation().X / BrixUnits::CmPerM, Me->GetActorLocation().Y / BrixUnits::CmPerM);
		WalkLot = FString::Printf(TEXT("the point (%.1f, %.1f)"), WalkTo->X, WalkTo->Y);
	}
	if (WalkTo)   // -BrixWalkIn: walking the player in through the doorway, as if W were held toward the room
	{
		APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0);
		if (!Me) return;
		const FVector2D Here(Me->GetActorLocation().X / BrixUnits::CmPerM, Me->GetActorLocation().Y / BrixUnits::CmPerM);
		const FVector2D To = *WalkTo - Here;
		WalkSeconds += DeltaSeconds;
		if (To.Size() < 0.4)
		{
			UE_LOG(LogTemp, Display, TEXT("Brix Town: walked %s %s in %.1f s, %.2f m up"), WalkLot.StartsWith(TEXT("the point")) ? TEXT("to") : TEXT("in through the doorway of"), *WalkLot, WalkSeconds, Me->GetActorLocation().Z / BrixUnits::CmPerM - 0.85);
			WalkTo.Reset();
		}
		else if (WalkSeconds > 12.f)
		{
			UE_LOG(LogTemp, Warning, TEXT("Brix Town: could not walk to %s: stopped %.2f m short, %.2f m from the start"), *WalkLot, To.Size(), (Here - WalkFrom).Size());
			WalkTo.Reset();
		}
		else Me->AddMovementInput(FVector(To.GetSafeNormal(), 0.0), 1.f, true);   // forced: a test shot ignores the keys
	}
}

// The sun where the calendar puts it for this minute of this day (WLD-043, ADR-0024; play.js lighting()): worked out from the
// clock each frame, never stored. Its light comes from no lower than sun_lift; it reddens as it sinks; it fades through the
// calendar's hour of dawn and dusk.
void ABrixGameMode::UpdateSun()
{
	if (!SunActor || !City) return;
	const FBrixSky& S = City->Sky;
	const double Minutes = ClockMinutes();
	const int32 DayNow = Day + (int32)FMath::Floor(Minutes / 1440.0);
	const double Minute = FMath::Fmod(Minutes, 1440.0);
	const int32 Season = S.SeasonOf(DayNow);
	FVector3d P = S.SunPlace(Season, S.HourAngle(Season, Minute));      // east +x, up +y, south +z
	P.Y = FMath::Max(P.Y, (double)S.SunLift);
	P.Normalize();
	const FVector ToSun(P.X, P.Z, P.Y);                                  // in Unreal: x east, y south, z up
	UDirectionalLightComponent* Light = CastChecked<UDirectionalLightComponent>(SunActor->GetLightComponent());
	SunActor->SetActorRotation((-ToSun).Rotation());
	const float Low = FMath::SmoothStep(0.05f, 0.5f, (float)P.Y);
	Light->SetLightColor(FMath::Lerp(SunDawnColour, SunColour, Low));
	const float Light01 = (float)S.Daylight(Season, Minute);
	Daylight = Light01;
	// the weather (WLD-037, ADR-0016): cloud, rain and fog keep sunlight off the street, and part of it comes down as soft
	// grey light from the whole sky; the air thickens
	const bool bWx = Weather && Weather->bOn;
	const float SunLet = bWx ? (float)Weather->Now.Sun : 1.f;
	Light->SetIntensity(SunLux * Light01 * SunLet);
	// the sky's fill and the haze's glow go down with the day, and the haze takes the low sun's colour (lighting(): fog and sky
	// mix towards the night's as the day goes)
	const float Fill = FMath::Lerp(NightFill, 1.f, Light01);
	if (SkyFill) SkyFill->SetIntensity(SkyLightIntensity * Fill * (1.f + (bWx ? Weather->OvercastSkyGain * (float)Weather->Rules.OvercastFill * (1.f - SunLet) * Light01 : 0.f)));
	// under cloud, rain or snow the haze is lit by the grey sky, not the sun: its colour turns from the sun's to a cool grey by
	// how much sun the weather stops (ADR-0016: a covered day is flat and cool)
	const FLinearColor SunTint = FMath::Lerp(SunDawnColour, FLinearColor::White, Low);
	if (HazeFog) HazeFog->SetFogInscatteringColor(HazeColour * Fill * FMath::Lerp(SunTint, OvercastHaze, bWx ? 1.f - SunLet : 0.f));
	// little haze indoors (play.js: the fog is cut by 0.85 inside): the height fog is not shut out by walls
	if (HazeFog)
	{
		const APlayerCameraManager* Cam = UGameplayStatics::GetPlayerCameraManager(this, 0);
		const bool bIndoors = Cam && RoomAt(Cam->GetCameraLocation()) != INDEX_NONE;
		HazeFog->SetFogDensity(HazeDensity * (bIndoors ? IndoorHaze : 1.f) * (bWx ? (float)Weather->Now.Haze : 1.f));
		if (People) People->CameraLot = Cam ? RoomAt(Cam->GetCameraLocation()) : INDEX_NONE;   // its residents inside are drawn (CITY-013)
	}
}

int32 ABrixGameMode::RoomAt(const FVector& WorldCm) const
{
	const FVector2D P(WorldCm.X / BrixUnits::CmPerM, WorldCm.Y / BrixUnits::CmPerM);
	const float Up = (float)(WorldCm.Z / BrixUnits::CmPerM);
	for (int32 i = 0; i < Rooms.Num(); ++i)
	{
		const FRoom& R = Rooms[i];
		const FVector2D D = P - R.Centre, L(D.X * R.Cos + D.Y * R.Sin, -D.X * R.Sin + D.Y * R.Cos);
		if (FMath::Abs(L.X) < R.Half.X && FMath::Abs(L.Y) < R.Half.Y && Up < R.Top) return R.Lot;
	}
	return INDEX_NONE;
}

void ABrixGameMode::ShootAndQuit()
{
	FString Shot;
	FParse::Value(FCommandLine::Get(), TEXT("BrixShot="), Shot);
	FScreenshotRequest::RequestScreenshot(Shot, true, false);   // with what is on screen over the view (a talk's box)
	const double Secs = FPlatformTime::Seconds() - MarkSeconds;
	UE_LOG(LogTemp, Display, TEXT("Brix Town: %.1f frames a second over the last %.0f s before the screenshot"), Secs > 0 ? (GFrameCounter - MarkFrame) / Secs : 0.0, Secs);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: at the shot the game thread took %.1f ms, the render thread %.1f ms, the GPU %.1f ms"),
		FPlatformTime::ToMilliseconds(GGameThreadTime), FPlatformTime::ToMilliseconds(GRenderThreadTime), FPlatformTime::ToMilliseconds(RHIGetGPUFrameCycles()));
	if (const APawn* Me = UGameplayStatics::GetPlayerPawn(this, 0))
	{
		const int32 Room = RoomAt(Me->GetActorLocation());
		FString Where = TEXT("out of doors");
		if (Room != INDEX_NONE && Insides && Insides->Lots.IsValidIndex(Room) && Insides->Lots[Room].IsSet()) Where = TEXT("inside ") + Insides->Lots[Room]->Id;
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the player is at (%.1f, %.1f) %.2f m up, %s"), Me->GetActorLocation().X / 100, Me->GetActorLocation().Y / 100, Me->GetActorLocation().Z / 100 - 0.85, *Where);
	}
	if (const ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0)); Me && Me->Boards.Kind())
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the %s %s, %.1f km/h"), *Me->Boards.Kind()->Name, Me->Boards.Riding() ? TEXT("ridden") : TEXT("in the hand"), Me->Boards.V * 3.6);
	if (Map)
	{
		TArray<FString> Said;
		for (const ABrixMap::FTag& T : Map->Tags) Said.Add(T.Text);
		UE_LOG(LogTemp, Display, TEXT("Brix Town: the maps: %d residents on them, %d markers; over the street: %s"), Map->Folk.Num(), Map->Dots.Num(), Said.Num() ? *FString::Join(Said, TEXT(" | ")) : TEXT("nothing"));
	}
	if (Furnisher) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d homes furnished near the camera, %d pieces, %d room lamps on"), Furnisher->Furnished(), Furnisher->PiecesShown(), Furnisher->LampsOn());
	if (Cars) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d vehicles drawn"), Cars->Shown());
	if (Cars && Cars->Driving().K >= 0) UE_LOG(LogTemp, Display, TEXT("Brix Town: the player's car at (%.1f, %.1f) going %.1f m/s"), Cars->Driving().X, Cars->Driving().Z, Cars->Driving().V);
	if (People) UE_LOG(LogTemp, Display, TEXT("Brix Town: %d residents with a body at %02d:%02d, %.2f ms a frame working them out (%s)"), People->Shown(), (int32)ClockMinute() / 60, (int32)ClockMinute() % 60, People->TickMs(), *People->TickParts());
	FTimerHandle H;
	GetWorldTimerManager().SetTimer(H, []() { FPlatformMisc::RequestExit(false); }, 3.f, false);
}
