#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixLamps.generated.h"

class ABrixTownActor;
class UPointLightComponent;

// The street lamps and wall lanterns by night (WLD-028), as the slice lights them: every lamp's glass glows as the
// daylight goes, and the nearest MaxLights of them to the camera give real light (the slice's twelve uLamp), warm, falling
// off within Reach. By day they are off. The town's lamps and their strengths come from ABrixTownActor::LampLights.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixLamps : public AActor
{
	GENERATED_BODY()

public:
	ABrixLamps();
	void Start(ABrixTownActor* InTown);
	virtual void Tick(float DeltaSeconds) override;

	UPROPERTY(Config) int32 MaxLights = 12;
	UPROPERTY(Config) float Candela = 6.f;         // a lamp of strength 1 at full night
	UPROPERTY(Config) float Reach = 10.f;          // m: its light's radius
	UPROPERTY(Config) float GlassGlow = 6.f;       // the glass's glow at full night
	UPROPERTY(Config) FLinearColor Warm = FLinearColor(1.f, .74f, .48f);   // the slice's lamplight
	UPROPERTY(Config) float NightBelow = .35f;     // daylight under which the lamps come on, fully by 0

	float Night() const { return LastNight; }

private:
	UPROPERTY() TObjectPtr<ABrixTownActor> Town;
	UPROPERTY() TArray<TObjectPtr<UPointLightComponent>> Lights;
	float LastNight = -1.f;
};
