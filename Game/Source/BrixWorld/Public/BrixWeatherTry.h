#pragma once

#include "CoreMinimal.h"

// The weather key (WLD-037; play.js: K steps through the day's own weather, clear, cloud, rain and fog; World/rules/
// settings.json "weather": K). After fog comes snow, where sky.json has it (WLD-069), then the day's own again. Trying: the
// kind tried now, empty for the day's own; Kinds: the kinds sky.json knows. Returns the next one to try (empty: the day's own).
BRIXWORLD_API FString BrixNextWeatherTry(const FString& Trying, const TArray<FString>& Kinds);
