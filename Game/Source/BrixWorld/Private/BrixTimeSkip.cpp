#include "BrixTimeSkip.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool LoadBrixTimeSkipRules(const FString& Path, FBrixTimeSkipRules& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("cannot read ") + Path; return false; }
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) { OutError = TEXT("not JSON: ") + Path; return false; }
	Out = FBrixTimeSkipRules();
	const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
	if (!J->TryGetArrayField(TEXT("steps"), Steps)) { OutError = TEXT("no steps in ") + Path; return false; }
	for (const TSharedPtr<FJsonValue>& V : *Steps)
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FBrixSkipStep S;
		S.Id = O->GetStringField(TEXT("id"));
		O->TryGetStringField(TEXT("label"), S.Label);
		O->TryGetNumberField(TEXT("minutes"), S.Minutes);
		FString To;
		S.bNextWeather = O->TryGetStringField(TEXT("to"), To) && To == TEXT("next_weather");
		if (S.Minutes <= 0 && !S.bNextWeather) { OutError = TEXT("step ") + S.Id + TEXT(" moves the clock by nothing"); return false; }
		Out.Steps.Add(S);
	}
	const TSharedPtr<FJsonObject>* NW = nullptr;
	if (J->TryGetObjectField(TEXT("next_weather"), NW))
	{
		(*NW)->TryGetNumberField(TEXT("slot_minutes"), Out.SlotMinutes);
		(*NW)->TryGetNumberField(TEXT("max_days"), Out.MaxDays);
	}
	J->TryGetStringField(TEXT("who"), Out.Who);
	const TArray<TSharedPtr<FJsonValue>>* Not = nullptr;
	if (J->TryGetArrayField(TEXT("not_during"), Not)) for (const TSharedPtr<FJsonValue>& V : *Not) Out.NotDuring.Add(V->AsString());
	return true;
}

namespace
{
	FString SlotWeather(const TArray<FString>& DayWeather, double SlotMinutes, double Minutes)
	{
		if (DayWeather.Num() == 0) return TEXT("clear");
		const int64 Slot = (int64)FMath::FloorToDouble(Minutes / SlotMinutes);
		const int32 n = DayWeather.Num();
		return DayWeather[(int32)(((Slot % n) + n) % n)];
	}
}

double BrixNextWeather(const FBrixTimeSkipRules& R, const TArray<FString>& DayWeather, double Minutes)
{
	const FString Now = SlotWeather(DayWeather, R.SlotMinutes, Minutes);
	const double Limit = Minutes + R.MaxDays * 1440.0;
	for (double B = (FMath::FloorToDouble(Minutes / R.SlotMinutes) + 1) * R.SlotMinutes; B < Limit; B += R.SlotMinutes)
	{
		if (SlotWeather(DayWeather, R.SlotMinutes, B) != Now) return B;
	}
	return Limit;
}

double BrixSkipTarget(const FBrixTimeSkipRules& R, const TArray<FString>& DayWeather, double Minutes, const FString& Step)
{
	for (const FBrixSkipStep& S : R.Steps)
	{
		if (S.Id != Step) continue;
		return S.bNextWeather ? BrixNextWeather(R, DayWeather, Minutes) : Minutes + S.Minutes;
	}
	return -1.0;
}

FString BrixAskSkip(const FBrixTimeSkipRules& R, const TArray<FString>& DayWeather, double Minutes, const FString& Step,
	bool bAskerIsHost, const TArray<FString>& Busy, double& OutMinutes)
{
	OutMinutes = Minutes;
	if (R.Who == TEXT("host") && !bAskerIsHost) return TEXT("not_host");
	for (const FString& B : Busy) if (R.NotDuring.Contains(B)) return TEXT("busy:") + B;
	const double T = BrixSkipTarget(R, DayWeather, Minutes, Step);
	if (T < 0) return TEXT("unknown_step");
	OutMinutes = T;
	return FString();
}
