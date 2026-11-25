#include "BrixWeatherTry.h"

FString BrixNextWeatherTry(const FString& Trying, const TArray<FString>& Kinds)
{
	// play.js: const order = [null, "clear", "cloud", "rain", "fog"]; wxTry = order[(order.indexOf(wxTry) + 1) % order.length]
	TArray<FString> Order = { FString() };
	for (const TCHAR* K : { TEXT("clear"), TEXT("cloud"), TEXT("rain"), TEXT("fog"), TEXT("snow") }) if (Kinds.Contains(K)) Order.Add(K);
	return Order[(Order.IndexOfByKey(Trying) + 1) % Order.Num()];   // (a kind not in the order: indexOf -1, so the day's own)
}
