#include "BrixWeather.h"

#include "BrixCars.h"
#include "BrixDay.h"

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
		O->TryGetNumberField(TEXT("snow"), K.Snow);
	}
	const TSharedPtr<FJsonObject> C = S->GetObjectField(TEXT("clouds"));
	Out.BlendMinutes = C->GetNumberField(TEXT("blend_minutes"));
	Out.bFollow = C->GetBoolField(TEXT("follow_weather"));
	for (const auto& KV : C->GetObjectField(TEXT("cover"))->Values) Out.Cover.Add(FString(KV.Key), KV.Value->AsNumber());
	Out.Scale = C->GetNumberField(TEXT("scale")); Out.Soft = C->GetNumberField(TEXT("soft")); Out.CloudSpeed = C->GetNumberField(TEXT("speed"));
	Out.Change = C->GetNumberField(TEXT("change")); Out.Lit = C->GetNumberField(TEXT("lit")); Out.Shade = C->GetNumberField(TEXT("shade"));
	const TArray<TSharedPtr<FJsonValue>>& Wd = C->GetArrayField(TEXT("wind"));
	Out.Wind = FVector2D(Wd[0]->AsNumber(), Wd[1]->AsNumber());
	S->TryGetNumberField(TEXT("overcast_fill"), Out.OvercastFill);
	const TSharedPtr<FJsonObject> R = S->GetObjectField(TEXT("rain"));
	Out.Alpha = R->GetNumberField(TEXT("alpha")); Out.Slant = R->GetNumberField(TEXT("slant")); Out.Speed = R->GetNumberField(TEXT("speed"));
	Out.Length = R->GetNumberField(TEXT("length")); Out.WetDark = R->GetNumberField(TEXT("wet_dark")); Out.WetSheen = R->GetNumberField(TEXT("wet_sheen"));
	Out.Puddles = R->GetNumberField(TEXT("puddles")); Out.Ripples = R->GetNumberField(TEXT("ripples")); Out.DryMinutes = R->GetNumberField(TEXT("dry_minutes"));
	const TSharedPtr<FJsonObject>* Sn = nullptr;
	if (S->TryGetObjectField(TEXT("snow"), Sn))
	{
		const TSharedPtr<FJsonObject>& N = *Sn;
		Out.SnowAlpha = N->GetNumberField(TEXT("alpha")); Out.SnowSpeed = N->GetNumberField(TEXT("speed")); Out.SnowSway = N->GetNumberField(TEXT("sway"));
		Out.FlakeSize = N->GetNumberField(TEXT("size")); Out.SettleMinutes = N->GetNumberField(TEXT("settle_minutes"));
		Out.MeltMinutes = N->GetNumberField(TEXT("melt_minutes")); Out.SnowCover = N->GetNumberField(TEXT("cover"));
		const TSharedPtr<FJsonObject>* Tr = nullptr;
		if (N->TryGetObjectField(TEXT("tracks"), Tr))
		{
			Out.TrailM = (*Tr)->GetNumberField(TEXT("trail_m")); Out.CarTrailM = (*Tr)->GetNumberField(TEXT("car_trail_m"));
			Out.FillMinutes = (*Tr)->GetNumberField(TEXT("fill_minutes")); Out.FadeMinutes = (*Tr)->GetNumberField(TEXT("fade_minutes"));
			Out.MinLying = (*Tr)->GetNumberField(TEXT("min_lying"));
		}
	}
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
	W.Grey = Blend(P.Grey, C.Grey, N.Grey); W.Rain = Blend(P.Rain, C.Rain, N.Rain); W.Snow = Blend(P.Snow, C.Snow, N.Snow);
	const double Dry = P.Rain > 0 ? FMath::Clamp(1 - into / R.DryMinutes, 0.0, 1.0) : 0.0;   // drying after a wet slot
	W.Wet = FMath::Max(FMath::Min(1.0, W.Rain * 1.5), Dry);
	return W;
}

FBrixCloudsNow BrixCloudsNow(const FBrixWeatherRules& R, const TArray<FString>& Day, int32 DayIndex, double m, const FString& Trying)
{
	auto Cov = [&](int32 i)
	{
		FString K = Trying;
		if (K.IsEmpty() && R.bFollow && Day.Num() == 4) K = Day[((i % 4) + 4) % 4];
		if (K.IsEmpty()) K = TEXT("clear");
		return R.Cover.FindRef(K);
	};
	const int32 s = FMath::FloorToInt(m / 360);
	const double into = m - s * 360, b = R.BlendMinutes;
	FBrixCloudsNow C;
	C.Cover = into < b ? Cov(s - 1) + (Cov(s) - Cov(s - 1)) * (.5 + .5 * into / b) : into > 360 - b ? Cov(s) + (Cov(s + 1) - Cov(s)) * (.5 * (into - (360 - b)) / b) : Cov(s);
	const double t = DayIndex * 1440.0 + m;
	auto Wrap = [](double x) { return x - 512.0 * FMath::FloorToDouble(x / 512.0); };   // Python's % for these
	C.Offset = FVector2D(Wrap(R.Wind.X * R.CloudSpeed * t), Wrap(R.Wind.Y * R.CloudSpeed * t));
	C.Phase = Wrap(R.Change * t);
	return C;
}

FBrixSnowNow BrixSnowNow(const FBrixWeatherRules& R, const TArray<FString>& Day, double m, const FString& Trying)
{
	auto Snows = [&](int32 i)
	{
		FString K = Trying;
		if (K.IsEmpty() && R.bFollow && Day.Num() == 4) K = Day[((i % 4) + 4) % 4];
		if (K.IsEmpty()) K = TEXT("clear");
		const FBrixWeatherKind* Kd = R.Kinds.Find(K);
		return Kd && Kd->Snow > 0;
	};
	const int32 s = FMath::FloorToInt(m / 360);
	double Lying = 0;
	for (int32 i = s - 2; i <= s; ++i)   // from twelve hours back: building while it snows, melting while it does not
	{
		const double dt = i == s ? m - s * 360.0 : 360.0;
		Lying = Snows(i) ? FMath::Min(1.0, Lying + dt / R.SettleMinutes) : FMath::Max(0.0, Lying - dt / R.MeltMinutes);
	}
	FBrixSnowNow Out;
	Out.Falling = BrixWeatherNow(R, Day, m, Trying).Snow;
	Out.Lying = Lying * R.SnowCover;
	return Out;
}

double BrixPrintStrength(const FBrixWeatherRules& R, double Age, double Falling, double Lying)
{
	if (Lying < R.MinLying || Age < 0) return 0;
	return FMath::Clamp(1 - Age * Falling / R.FillMinutes, 0.0, 1.0) * FMath::Clamp(1 - Age / R.FadeMinutes, 0.0, 1.0);
}

void BrixFootprints(const FBrixWeatherRules& R, const FBrixDay& Day, int32 i, double M, double Stride, double Side, double Falling, double Lying, TArray<FBrixPrint>& Out)
{
	if (Lying < R.MinLying || Stride <= 0) return;
	const int32 K = Day.PieceAt(i, M);
	if (K < 0) return;
	const TArray<FBrixDay::FPiece>& Row = Day.Plan[i];
	const FBrixDay::FPiece& P = Row[K];
	if (P.Kind != 1 || P.Route.Num() < 2 || P.Cum.Num() != P.Route.Num()) return;
	const double T1 = K + 1 < Row.Num() ? Row[K + 1].T0 : 1440.0, Len = P.Cum.Last(), Dur = FMath::Max(T1 - P.T0, 1e-6);
	const double Now = FMath::Clamp((M - P.T0) / Dur, 0.0, 1.0) * Len;
	int32 Seg = 0;
	for (int32 n = FMath::FloorToInt(Now / Stride); n >= 0; --n)   // each footstep behind them, the nearest first
	{
		const double d = n * Stride;
		if (Now - d > R.TrailM) break;
		if (d >= Now) continue;
		while (Seg + 1 < P.Cum.Num() - 1 && P.Cum[Seg + 1] < d) ++Seg;
		while (Seg > 0 && P.Cum[Seg] > d) --Seg;
		const double SegLen = FMath::Max(P.Cum[Seg + 1] - P.Cum[Seg], 1e-6), f = FMath::Clamp((d - P.Cum[Seg]) / SegLen, 0.0, 1.0);
		const FVector2D Dir = (P.Route[Seg + 1] - P.Route[Seg]).GetSafeNormal();
		const double Age = (Now - d) / Len * Dur, Str = BrixPrintStrength(R, Age, Falling, Lying);
		if (Str <= 0) continue;
		const FVector2D Across(-Dir.Y, Dir.X);
		Out.Add({ FMath::Lerp(P.Route[Seg], P.Route[Seg + 1], f) + Across * (n % 2 ? Side : -Side), Dir, Age, Str });
	}
}

void BrixTyreTracks(const FBrixWeatherRules& R, const FBrixTraffic& Traffic, int32 k, double M, double Gauge, double Falling, double Lying, TArray<FBrixPrint>& Out)
{
	if (Lying < R.MinLying) return;
	const double Dt = .005;   // game minutes between the points looked back at
	TOptional<FBrixCarAt> Prev = Traffic.At(k, M);
	if (!Prev) return;
	double Run = 0;
	for (double t = M - Dt; t > M - 30 && Run < R.CarTrailM; t -= Dt)
	{
		const TOptional<FBrixCarAt> C = Traffic.At(k, FMath::Max(0.0, t));
		if (!C) break;
		const double Step = FVector2D::Distance(C->Pos, Prev->Pos);
		if (Step > 25) break;   // came on the map there, or left it
		if (Step > .01)
		{
			const double Age = M - t, Str = BrixPrintStrength(R, Age, Falling, Lying);
			if (Str > 0)
				for (const double s : { -.5, .5 })
				{
					const FVector2D AcrossA(-Prev->Heading.Y, Prev->Heading.X), AcrossB(-C->Heading.Y, C->Heading.X);
					Out.Add({ Prev->Pos + AcrossA * (s * Gauge), Prev->Heading, Age - Dt, Str });
					Out.Add({ C->Pos + AcrossB * (s * Gauge), C->Heading, Age, Str });
				}
			Run += Step;
		}
		Prev = C;
	}
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
	if ((*W)->HasField(TEXT("snow"))) Rows(TEXT("snow"), Out.Snowy);
	const TSharedPtr<FJsonObject> P = (*W)->GetObjectField(TEXT("pace"));
	Out.Run = P->GetNumberField(TEXT("run")); Out.Hurry = P->GetNumberField(TEXT("hurry")); Out.Umbrella = P->GetNumberField(TEXT("umbrella"));
	P->TryGetNumberField(TEXT("snow"), Out.SnowPace);
	for (const auto& KV : S->GetObjectField(TEXT("signatures"))->Values) Out.SigSpeed.Add(FString(KV.Key), KV.Value->AsNumber());
	return true;
}

FBrixWet BrixWetOf(const FBrixPeopleRules& People, const FBrixWear& W, int32 i, double Rain, double Snow, int32 Wardrobe, bool bCovered,
	bool bWalking, bool bOtherWalk, const FString& SigName, double S, double M, double T0, double T1, double Len)
{
	FBrixWet Out;
	// a kind of weather tried on a day without it: what they would have worn on a day of it
	const TArray<FBrixWear::FRow>& Rows = Wardrobe == 2 && W.Snowy.Num() ? W.Snowy : Wardrobe == 1 ? W.Wet : W.Today;
	if (!Rows.IsValidIndex(i)) return Out;
	const FBrixWear::FRow& w = Rows[i];
	auto Colour = [&](int32 c) -> TOptional<FVector3f> { if (!W.Colours.IsValidIndex(c)) return {}; const FVector3f* C = People.Palette.Find(W.Colours[c]); return C ? TOptional<FVector3f>(*C) : TOptional<FVector3f>(); };
	if (w.Coat >= 0) Out.Coat = Colour(w.Coat);
	if (Snow >= .25 && !bCovered)   // out in the snow (CITY-107): the hood up, else bare-headed, never an umbrella; a careful walk
	{
		if (w.bHood) Out.Hood = Out.Coat;
		if (bWalking && !bOtherWalk)
		{
			Out.Sig = TEXT("Careful");
			const double dur = FMath::Max(T1 - T0, 1e-6), d = FMath::Clamp((M - T0) / dur, 0.0, 1.0) * Len;
			Out.Phase = FMath::Fmod(d / BrixStepLength(People.Sig(Out.Sig), S), 1.0);
		}
		return Out;
	}
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
