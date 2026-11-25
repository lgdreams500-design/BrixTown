#pragma once

#include "CoreMinimal.h"

// The weather key (WLD-037; play.js: K steps through the day's own weather, clear, cloud, rain and fog; World/rules/
// settings.json "weather": K). After fog comes snow, where sky.json has it (WLD-069), then the day's own again. Trying: the
// kind tried now, empty for the day's own; Kinds: the kinds sky.json knows. Returns the next one to try (empty: the day's own).
BRIXWORLD_API FString BrixNextWeatherTry(const FString& Trying, const TArray<FString>& Kinds);

// Wet clothes (WLD-037): how much darker someone out of doors is in the rain, as the slice darkens everything outdoors
// (play.js: c *= 1 - uWet.x * .3, uWet.x = wet * rain.wet_dark): Wet how wet it is (1 in the rain, drying after it, as the
// paving), WetDark sky.json rain.wet_dark, Share the slice's .3; 0 indoors. The material multiplies by 1 minus it.
BRIXWORLD_API float BrixWetClothes(double Wet, double WetDark, double Share, bool bIndoors);
