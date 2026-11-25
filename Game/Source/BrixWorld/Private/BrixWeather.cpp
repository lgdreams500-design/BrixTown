#include "BrixWeather.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) || !Out.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
		return true;
	}
}

bool LoadBrixWeatherRules(const FString& SkyPath, FBrixWeatherRules& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> S;
	if (!ReadJson(SkyPath, S, OutError)) return false;
	Out = FBrixWeatherRules();
	for (const auto& KV : S->GetObjectField(TEXT("weather"))->Values)
	{
		const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
		FBrixWeatherKind& K = Out.Kinds.Add(FString(KV.Key));
		K.Sun = O->GetNumberField(TEXT("sun")); K.Haze = O->GetNumberField(TEXT("haze")); K.Veil = O->GetNumberField(TEXT("veil"));
		K.Grey = O->GetNumberField(TEXT("grey")); K.Rain = O->GetNumberField(TEXT("rain"));
	}
	const TSharedPtr<FJsonObject> C = S->GetObjectField(TEXT("clouds"));
	Out.BlendMinutes = C->GetNumberField(TEXT("blend_minutes"));
	Out.bFollow = C->GetBoolField(TEXT("follow_weather"));
	for (const auto& KV : C->GetObjectField(TEXT("cover"))->Values) Out.Cover.Add(FString(KV.Key), KV.Value->AsNumber());
	S->TryGetNumberField(TEXT("overcast_fill"), Out.OvercastFill);
	const TSharedPtr<FJsonObject> R = S->GetObjectField(TEXT("rain"));
	Out.Alpha = R->GetNumberField(TEXT("alpha")); Out.Slant = R->GetNumberField(TEXT("slant")); Out.Speed = R->GetNumberField(TEXT("speed"));
	Out.Length = R->GetNumberField(TEXT("length")); Out.WetDark = R->GetNumberField(TEXT("wet_dark")); Out.WetSheen = R->GetNumberField(TEXT("wet_sheen"));
	Out.Puddles = R->GetNumberField(TEXT("puddles")); Out.Ripples = R->GetNumberField(TEXT("ripples")); Out.DryMinutes = R->GetNumberField(TEXT("dry_minutes"));
	if (!Out.Kinds.Contains(TEXT("clear")) || !Out.Kinds.Contains(TEXT("rain"))) { OutError = SkyPath + TEXT(" has no clear or rain weather"); return false; }
	return true;
}

FBrixWeatherNow BrixWeatherNow(const FBrixWeatherRules& R, const TArray<FString>& Day, double m, const FString& Trying)
{
	auto KindOf = [&](int32 i) -> FString
	{
		if (!Trying.IsEmpty()) return Trying;
		if (R.bFollow && Day.Num() == 4) { const FString& K = Day[((i % 4) + 4) % 4]; if (!K.IsEmpty()) return K; }
		return TEXT("clear");
	};
	auto Of = [&](int32 i) { const FBrixWeatherKind* K = R.Kinds.Find(KindOf(i)); return K ? *K : FBrixWeatherKind(); };
	const int32 s = FMath::FloorToInt(m / 360);
	const double into = m - s * 360, b = R.BlendMinutes;
	const FBrixWeatherKind P = Of(s - 1), C = Of(s), N = Of(s + 1);
	auto Blend = [&](double p, double c, double n) { return into < b ? p + (c - p) * (.5 + .5 * into / b) : into > 360 - b ? c + (n - c) * (.5 * (into - (360 - b)) / b) : c; };
	FBrixWeatherNow W;
	W.Kind = KindOf(s);
	W.Sun = Blend(P.Sun, C.Sun, N.Sun); W.Haze = Blend(P.Haze, C.Haze, N.Haze); W.Veil = Blend(P.Veil, C.Veil, N.Veil);
	W.Grey = Blend(P.Grey, C.Grey, N.Grey); W.Rain = Blend(P.Rain, C.Rain, N.Rain);
	const double Dry = P.Rain > 0 ? FMath::Clamp(1 - into / R.DryMinutes, 0.0, 1.0) : 0.0;   // drying after a wet slot
	W.Wet = FMath::Max(FMath::Min(1.0, W.Rain * 1.5), Dry);
	return W;
}

bool LoadBrixWear(const FString& DayPath, const FString& SignaturesPath, FBrixWear& Out, FString& OutError)
{
	TSharedPtr<FJsonObject> D, S;
	if (!ReadJson(DayPath, D, OutError) || !ReadJson(SignaturesPath, S, OutError)) return false;
	Out = FBrixWear();
	D->TryGetStringArrayField(TEXT("weather"), Out.Weather);
	const TSharedPtr<FJsonObject>* W = nullptr;
	if (!D->TryGetObjectField(TEXT("wear"), W)) { OutError = DayPath + TEXT(" has no wear: run Tools/worldgen/build_play.py"); return false; }
	(*W)->TryGetStringArrayField(TEXT("colours"), Out.Colours);
	auto Rows = [&](const TCHAR* K, TArray<FBrixWear::FRow>& R)
	{
		for (const TSharedPtr<FJsonValue>& V : (*W)->GetArrayField(K))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			R.Add({ (int32)A[0]->AsNumber(), A[1]->AsNumber() != 0, (int32)A[2]->AsNumber() });
		}
	};
	Rows(TEXT("today"), Out.Today);
	Rows(TEXT("wet"), Out.Wet);
	const TSharedPtr<FJsonObject> P = (*W)->GetObjectField(TEXT("pace"));
	Out.Run = P->GetNumberField(TEXT("run")); Out.Hurry = P->GetNumberField(TEXT("hurry")); Out.Umbrella = P->GetNumberField(TEXT("umbrella"));
	for (const auto& KV : S->GetObjectField(TEXT("signatures"))->Values) Out.SigSpeed.Add(FString(KV.Key), KV.Value->AsNumber());
	return true;
}

FBrixWet BrixWetOf(const FBrixPeopleRules& People, const FBrixWear& W, int32 i, double Rain, bool bTrying, bool bCovered,
	bool bWalking, bool bOtherWalk, const FString& SigName, double S, double M, double T0, double T1, double Len)
{
	FBrixWet Out;
	const TArray<FBrixWear::FRow>& Rows = bTrying ? W.Wet : W.Today;   // trying rain on a dry day: what they would have worn on a wet one
	if (!Rows.IsValidIndex(i)) return Out;
	const FBrixWear::FRow& w = Rows[i];
	auto Colour = [&](int32 c) -> TOptional<FVector3f> { if (!W.Colours.IsValidIndex(c)) return {}; const FVector3f* C = People.Palette.Find(W.Colours[c]); return C ? TOptional<FVector3f>(*C) : TOptional<FVector3f>(); };
	if (w.Coat >= 0) Out.Coat = Colour(w.Coat);
	if (Rain < .25 || bCovered) return Out;   // dry, indoors, sheltering, under a roof
	if (w.Umbrella >= 0) { Out.Umbrella = Colour(w.Umbrella); Out.Arms[1] = FVector4d(1, 52, 12, 84); }
	else if (w.bHood) Out.Hood = Out.Coat;
	if (!bWalking || bOtherWalk) return Out;   // standing or sitting out in the rain; or already running from a car
	const double* Own0 = W.SigSpeed.Find(SigName);
	const double own = (Own0 ? *Own0 : 1.14) * W.Pace, dur = FMath::Max(T1 - T0, 1e-6);
	const double ratio = Len / dur / own, d = FMath::Clamp((M - T0) / dur, 0.0, 1.0) * Len;
	const FString Sig = ratio > (W.Hurry + W.Run) / 2 ? TEXT("Run") : Out.Umbrella ? TEXT("") : TEXT("Hurried");   // running for cover, or hurrying without an umbrella
	if (!Sig.IsEmpty()) { Out.Sig = Sig; const double Step = BrixStepLength(People.Sig(Sig), S); Out.Phase = FMath::Fmod(d / Step, 1.0); }
	if (!Out.Umbrella && !Out.Hood) { Out.Arms[0] = FVector4d(1, 150, 30, 118); Out.Chest = FVector2D(6, 0); }   // a hand over the head
	return Out;
}
