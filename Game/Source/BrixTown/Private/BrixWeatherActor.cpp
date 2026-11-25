#include "BrixWeatherActor.h"

#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::CmPerM;

namespace
{
	const double Cell = 16;
	FIntPoint CellOf(const FVector2D& P) { return FIntPoint(FMath::FloorToInt(P.X / Cell), FMath::FloorToInt(P.Y / Cell)); }
	FString Data(const TCHAR* P) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), P)); }
}

ABrixWeather::ABrixWeather()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
	Rain = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Rain"));
	Rain->SetupAttachment(RootComponent);
	Rain->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Rain->SetCastShadow(false);
	RootComponent->SetMobility(EComponentMobility::Movable);
	Rain->SetMobility(EComponentMobility::Movable);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> Mat(TEXT("/Game/Weather/M_Rain.M_Rain"));   // Tools/unreal/make_rain_material.py
	RainMaterial = Mat.Object;
}

void ABrixWeather::Start(const FBrixCity& City, ABrixTownActor* InTown)
{
	Town = InTown;
	FString Error;
	bOn = LoadBrixWeatherRules(Data(TEXT("../World/rules/sky.json")), Rules, Error)
		&& LoadBrixWear(Data(TEXT("Data/day.json")), Data(TEXT("../World/rules/signatures.json")), Wear, Error);
	if (!bOn) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: %s; the weather stays clear"), *Error); return; }
	Wear.Pace = City.Tram.Pace;
	FParse::Value(FCommandLine::Get(), TEXT("BrixWeather="), Trying);
	if (!Trying.IsEmpty() && !Rules.Kinds.Contains(Trying)) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: no weather called %s"), *Trying); Trying.Reset(); }
	// the roofs: every building's footprint (not the open lots), turned as its lot is
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			if (L.IsOpen()) continue;
			const double A = FMath::DegreesToRadians(L.AngleDeg);
			const int32 k = Roofs.Add({ L.Centre(), L.Size() / 2, FMath::Cos(A), FMath::Sin(A) });
			const double Reach = L.Size().Size() / 2;
			const FIntPoint Lo = CellOf(L.Centre() - FVector2D(Reach)), Hi = CellOf(L.Centre() + FVector2D(Reach));
			for (int32 x = Lo.X; x <= Hi.X; ++x) for (int32 y = Lo.Y; y <= Hi.Y; ++y) RoofGrid.Add(FIntPoint(x, y), k);
		}
	// the drops: spread evenly over the rings' area, each with its own place in the fall and its own speed
	FRandomStream Rn(16);
	for (int32 k = 0; k < MaxDrops; ++k)
	{
		const float R = FMath::Sqrt(FMath::Lerp(NearDrops * NearDrops, FarDrops * FarDrops, Rn.FRand()));
		Drops.Add({ Rn.FRand() * 2 * PI, R, Rn.FRand(), .85f + .3f * Rn.FRand() });
	}
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the day's weather %s%s"), *FString::Join(Wear.Weather, TEXT(", ")), Trying.IsEmpty() ? TEXT("") : *(TEXT("; trying ") + Trying));
}

bool ABrixWeather::Roofed(const FVector2D& P) const
{
	TArray<int32> In;
	RoofGrid.MultiFind(CellOf(P), In);
	for (const int32 k : In)
	{
		const FRoof& R = Roofs[k];
		const FVector2D D = P - R.C, L(D.X * R.Cos + D.Y * R.Sin, -D.X * R.Sin + D.Y * R.Cos);   // into the lot's own frame
		if (FMath::Abs(L.X) <= R.Half.X && FMath::Abs(L.Y) <= R.Half.Y) return true;
	}
	return false;
}

void ABrixWeather::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!bOn || !Mode || !PC || !PC->PlayerCameraManager) return;
	Now = BrixWeatherNow(Rules, Wear.Weather, Mode->ClockMinute(), Trying);
	// the paving wet in the rain, drying after it (WLD-037)
	const float Wet = (float)Now.Wet;
	if (Town && FMath::Abs(Wet - LastWet) > .004f)
	{
		Town->SetWet(Wet * Rules.WetDark, Wet * Rules.WetSheen, Wet * Rules.Puddles);
		LastWet = Wet;
	}
	// the rain: drops round the camera, not under a roof, none while the camera is indoors
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation();
	const int32 Count = FMath::Clamp(FMath::RoundToInt(Drops.Num() * Now.Rain), 0, Drops.Num());
	if (Count == 0 || Mode->RoomAt(Eye) != INDEX_NONE) { Rain->SetVisibility(false); return; }
	const FVector2D Here(Eye.X / CmPerM, Eye.Y / CmPerM);
	const double Up = Eye.Z / CmPerM, T = GetWorld()->GetTimeSeconds(), H = FallHeight, Len = Rules.Length;
	const float Day = FMath::Clamp(Mode->Daylight, 0.f, 1.f);
	const FLinearColor Col(DropColour.R * (.3f + .7f * Day), DropColour.G * (.3f + .7f * Day), DropColour.B * (.32f + .68f * Day), (float)(Rules.Alpha * Now.Rain * (2.f - Day)));   // by night the drops catch the lamplight and stand out more
	TArray<FVector> V, N;
	TArray<FLinearColor> C;
	TArray<int32> Tri;
	V.Reserve(Count * 4); N.Reserve(Count * 4); C.Reserve(Count * 4); Tri.Reserve(Count * 6);
	const FVector Slant = FVector(Rules.Slant, 0, 0);   // the wind leans the drops a little (east)
	for (int32 k = 0; k < Count; ++k)
	{
		const FDrop& D = Drops[k];
		const FVector2D P = Here + FVector2D(FMath::Cos(D.A), FMath::Sin(D.A)) * D.R;
		const double Bottom = Up + H * .6 - FMath::Fmod(D.Phase * H + T * Rules.Speed * D.Speed, H);
		const bool bDry = Roofed(P) || Bottom < 0;
		const FVector Lo(P.X, P.Y, FMath::Max(Bottom, 0.0)), Hi = FVector(P.X, P.Y, FMath::Max(Bottom, 0.0) + (bDry ? 0 : Len)) + Slant * (bDry ? 0 : Len);
		const FVector To = (Eye / CmPerM - Lo).GetSafeNormal2D(), Side = FVector(-To.Y, To.X, 0) * (DropWidth / 2);
		const int32 b = V.Num();
		for (const FVector& Q : { Lo - Side, Lo + Side, Hi + Side, Hi - Side }) { V.Add(Q * CmPerM); N.Add(To); C.Add(Col); }
		for (const int32 i : { 0, 1, 2, 0, 2, 3 }) Tri.Add(b + i);
	}
	static const TArray<FVector2D> NoUV;
	static const TArray<FProcMeshTangent> NoT;
	if (Tri.Num() != RainTris) { Rain->CreateMeshSection_LinearColor(0, V, Tri, N, NoUV, C, NoT, false, false); Rain->SetMaterial(0, RainMaterial); RainTris = Tri.Num(); }
	else Rain->UpdateMeshSection_LinearColor(0, V, N, NoUV, C, NoT, false);
	Rain->SetVisibility(true);
}
