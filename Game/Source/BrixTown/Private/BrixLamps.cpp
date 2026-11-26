#include "BrixLamps.h"

#include "BrixGameMode.h"
#include "BrixLight.h"
#include "BrixTownActor.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/PointLightComponent.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"

using BrixUnits::ToWorld;
using BrixUnits::CmPerM;

ABrixLamps::ABrixLamps()
{
	PrimaryActorTick.bCanEverTick = true;
	RootComponent = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
}

void ABrixLamps::Start(ABrixTownActor* InTown)
{
	Town = InTown;
	for (int32 k = 0; k < MaxLights; ++k)
	{
		UPointLightComponent* L = NewObject<UPointLightComponent>(this);
		L->SetupAttachment(RootComponent);
		L->SetMobility(EComponentMobility::Movable);
		L->SetIntensityUnits(ELightUnits::Candelas);
		L->SetLightColor(Warm);
		L->SetAttenuationRadius(Reach * CmPerM);
		L->SetSourceRadius(15.f);
		L->SetCastShadows(false);   // twelve moving shadowed lights would cost more than the town
		L->SetVisibility(false);
		L->RegisterComponent();
		AddInstanceComponent(L);
		Lights.Add(L);
	}
	Held.Init(INDEX_NONE, Lights.Num());
}

void ABrixLamps::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = GetWorld()->GetFirstPlayerController();
	if (!Town || !Mode || !PC || !PC->PlayerCameraManager) return;
	// on as the calendar's daylight goes, and already as the sun gets low: the streets are in shade by then (UBrixLight)
	const UBrixLight* Sight = GetWorld()->GetSubsystem<UBrixLight>();
	const float Night = FMath::Max(Sight ? Sight->LowSun() : 0.f, FMath::SmoothStep(0.f, 1.f, FMath::Clamp(1.f - Mode->Daylight / FMath::Max(NightBelow, .01f), 0.f, 1.f)));
	if (Town->GlassMat && !FMath::IsNearlyEqual(Night, LastNight, .002f)) Town->GlassMat->SetScalarParameterValue(TEXT("Glow"), GlassGlow * Night);
	LastNight = Night;
	if (Night <= .01f)
	{
		for (int32 k = 0; k < Lights.Num(); ++k) { Held[k] = INDEX_NONE; if (Lights[k]->IsVisible()) Lights[k]->SetVisibility(false); }
		return;
	}
	// the nearest lamps to the camera, as the slice picks its twelve
	const FVector Eye = PC->PlayerCameraManager->GetCameraLocation() / CmPerM;
	TArray<TPair<float, int32>> Near;
	const TArray<FVector4f>& All = Town->LampLights;
	for (int32 i = 0; i < All.Num(); ++i)
	{
		const float D2 = FMath::Square(All[i].X - (float)Eye.X) + FMath::Square(All[i].Z - (float)Eye.Y);
		if (D2 < 120.f * 120.f && All[i].W > 0) Near.Add({ D2, i });   // a broken lamp is out (CITY-073)
	}
	Near.Sort([](const TPair<float, int32>& A, const TPair<float, int32>& B) { return A.Key < B.Key; });
	const int32 N = FMath::Min(Lights.Num(), Near.Num());
	const float Edge = Near.Num() > Lights.Num() ? FMath::Sqrt(Near[Lights.Num()].Key) : 120.f;   // the nearest lamp left out, or the cut-off
	TSet<int32, DefaultKeyFuncs<int32>, TInlineSetAllocator<16>> Want, Have;
	for (int32 k = 0; k < N; ++k) Want.Add(Near[k].Value);
	for (int32 k = 0; k < Lights.Num(); ++k)
	{
		if (Held[k] != INDEX_NONE && Want.Contains(Held[k])) Have.Add(Held[k]);
		else Held[k] = INDEX_NONE;
	}
	int32 Next = 0;
	for (int32 k = 0; k < Lights.Num(); ++k)
	{
		UPointLightComponent* L = Lights[k];
		if (Held[k] == INDEX_NONE)
		{
			while (Next < N && Have.Contains(Near[Next].Value)) ++Next;
			if (Next < N)
			{
				Held[k] = Near[Next].Value;
				Have.Add(Held[k]);
				const FVector4f& To = All[Held[k]];
				L->SetWorldLocation(ToWorld(FVector2D(To.X, To.Z), To.Y));
			}
		}
		if (Held[k] == INDEX_NONE) { if (L->IsVisible()) L->SetVisibility(false); continue; }
		const FVector4f& P = All[Held[k]];
		const float Far = FMath::Sqrt(FMath::Square(P.X - (float)Eye.X) + FMath::Square(P.Z - (float)Eye.Y));
		const float Level = Candela * P.W * Night * FMath::SmoothStep(0.f, 1.f, FMath::Clamp((Edge - Far) / FMath::Max(Edge * FadeBand, .01f), 0.f, 1.f));
		if (!FMath::IsNearlyEqual(L->Intensity, Level, Candela * .002f)) L->SetIntensity(Level);
		if (L->IsVisible() != (Level > 0.f)) L->SetVisibility(Level > 0.f);
	}
}
