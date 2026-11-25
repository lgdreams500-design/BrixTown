#include "BrixMarkers.h"

#include "BrixMap.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FString MkKey(const auto& Key) { return FString(Key.ToView()); }
	bool MkHex(const FString& S)
	{
		if (S.Len() != 7 || S[0] != '#') return false;
		for (int32 i = 1; i < 7; ++i) if (!FChar::IsHexDigit(S[i])) return false;
		return true;
	}
	const TCHAR* const MkKinds[] = { TEXT("offer"), TEXT("talk"), TEXT("handin"), TEXT("job"), TEXT("go"), TEXT("shop"), TEXT("cafe"), TEXT("bank"), TEXT("stall"), TEXT("estate"), TEXT("prices"), TEXT("mark") };
	double MkNum(const TSharedPtr<FJsonObject>& O, const TCHAR* F, double Or) { double V = Or; if (O) O->TryGetNumberField(F, V); return V; }
	FColor MkColour(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { FString S; return O && O->TryGetStringField(F, S) && MkHex(S) ? FColor::FromHex(S) : FColor::Magenta; }
}

FString FBrixMarkerRules::Words(const FString& K, const FString& Name) const
{
	const FBrixMarkerKind* M = Kinds.Find(K);
	return M ? M->Words.Replace(TEXT("{name}"), *Name) : FString();
}

FBrixMarkerRules::FLook FBrixMarkerRules::At(double Metres, bool bPlace) const
{
	FLook L;
	const double Reach = bPlace ? PlaceM : TokenM;
	if (Metres > SparkM) return L;
	if (Metres > Reach)
	{
		L.bSpark = true;
		L.Size = SparkPx;
		L.Alpha = (float)FMath::Clamp((SparkM - Metres) / (2.0 * FadeM), 0.0, 1.0);
		return L;
	}
	L.Alpha = (float)(.35 + .65 * FMath::Clamp((Reach - Metres) / FadeM, 0.0, 1.0));
	L.Size = TokenPx + (TokenNearPx - TokenPx) * (float)FMath::Clamp((2.0 * NearM - Metres) / NearM, 0.0, 1.0);
	return L;
}

float FBrixMarkerRules::Pulse(double Seconds) const
{
	return .5f - .5f * (float)FMath::Cos(UE_TWO_PI * FMath::Fmod(Seconds, (double)PulseS) / PulseS);
}

float FBrixMarkerRules::Bob(double Seconds, int32 Seed) const
{
	return BobM * (float)FMath::Sin(UE_TWO_PI * Seconds / BobS + Seed * 1.7);
}

bool ParseBrixMarkers(const TSharedPtr<FJsonObject>& J, FBrixMarkerRules& Out, FString& OutError)
{
	Out = FBrixMarkerRules();
	Out.Json = J;
	const TSharedPtr<FJsonObject>* Kinds = nullptr, *Signs = nullptr, *Places = nullptr, *Token = nullptr, *Beh = nullptr;
	if (!J || !J->TryGetObjectField(TEXT("kinds"), Kinds) || !J->TryGetObjectField(TEXT("signs"), Signs) || !J->TryGetObjectField(TEXT("place_kinds"), Places)
		|| !J->TryGetObjectField(TEXT("token"), Token) || !J->TryGetObjectField(TEXT("behaviour"), Beh))
	{
		OutError = TEXT("markers.json needs kinds, signs, place_kinds, token and behaviour");
		return false;
	}
	for (const auto& K : (*Kinds)->Values)
	{
		const TSharedPtr<FJsonObject> O = K.Value->AsObject();
		FBrixMarkerKind M;
		O->TryGetStringField(TEXT("sign"), M.Sign);
		O->TryGetStringField(TEXT("words"), M.Words);
		O->TryGetBoolField(TEXT("new"), M.bNew);
		M.Colour = MkColour(O, TEXT("colour"));
		Out.Kinds.Add(MkKey(K.Key), M);
	}
	for (const auto& P : (*Places)->Values) Out.PlaceKinds.Add(MkKey(P.Key), P.Value->AsString());
	for (const auto& S : (*Signs)->Values)
	{
		const TSharedPtr<FJsonObject> O = S.Value->AsObject();
		FBrixMarkerSign G;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(TEXT("lines"), A))
			for (const TSharedPtr<FJsonValue>& L : *A)
			{
				TArray<FVector2f> Pts;
				const TArray<TSharedPtr<FJsonValue>>& V = L->AsArray();
				for (int32 i = 0; i + 1 < V.Num(); i += 2) Pts.Add(FVector2f((float)V[i]->AsNumber(), (float)V[i + 1]->AsNumber()));
				G.Lines.Add(Pts);
			}
		if (O->TryGetArrayField(TEXT("closed"), A)) for (const TSharedPtr<FJsonValue>& C : *A) G.Closed.Add(C->AsBool());
		if (O->TryGetArrayField(TEXT("dots"), A))
			for (const TSharedPtr<FJsonValue>& D : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>& V = D->AsArray();
				if (V.Num() == 3) G.Dots.Add(FVector3f((float)V[0]->AsNumber(), (float)V[1]->AsNumber(), (float)V[2]->AsNumber()));
			}
		while (G.Closed.Num() < G.Lines.Num()) G.Closed.Add(false);
		Out.Signs.Add(MkKey(S.Key), G);
	}
	const TSharedPtr<FJsonObject> T = *Token, B = *Beh;
	Out.Face = MkColour(T, TEXT("face")); Out.FaceLight = MkColour(T, TEXT("face_light")); Out.FaceDark = MkColour(T, TEXT("face_dark"));
	Out.Edge = MkColour(T, TEXT("edge")); Out.Rim = MkColour(T, TEXT("rim")); Out.Stamp = MkColour(T, TEXT("stamp")); Out.Halo = MkColour(T, TEXT("halo"));
	Out.HaloAlpha = MkNum(T, TEXT("halo_alpha"), Out.HaloAlpha); Out.HaloNewAlpha = MkNum(T, TEXT("halo_new_alpha"), Out.HaloNewAlpha);
	Out.HaloSize = MkNum(T, TEXT("halo_size"), Out.HaloSize); Out.HaloNewSize = MkNum(T, TEXT("halo_new_size"), Out.HaloNewSize);
	Out.RingWidth = MkNum(T, TEXT("ring_width"), Out.RingWidth); Out.EdgeDepth = MkNum(T, TEXT("edge_depth"), Out.EdgeDepth);
	Out.SignSize = MkNum(T, TEXT("sign_size"), Out.SignSize); Out.SignStroke = MkNum(T, TEXT("sign_stroke"), Out.SignStroke);
	Out.TokenPx = MkNum(B, TEXT("token_px"), Out.TokenPx); Out.TokenNearPx = MkNum(B, TEXT("token_near_px"), Out.TokenNearPx);
	Out.NearM = MkNum(B, TEXT("near_m"), Out.NearM); Out.SparkPx = MkNum(B, TEXT("spark_px"), Out.SparkPx);
	Out.TokenM = MkNum(B, TEXT("token_m"), Out.TokenM); Out.SparkM = MkNum(B, TEXT("spark_m"), Out.SparkM); Out.FadeM = MkNum(B, TEXT("fade_m"), Out.FadeM);
	Out.PlaceM = MkNum(B, TEXT("place_m"), Out.PlaceM); Out.PlaceUpM = MkNum(B, TEXT("place_up_m"), Out.PlaceUpM); Out.HeadUpM = MkNum(B, TEXT("head_up_m"), Out.HeadUpM);
	Out.BobM = MkNum(B, TEXT("bob_m"), Out.BobM); Out.BobS = MkNum(B, TEXT("bob_s"), Out.BobS);
	Out.PulseS = MkNum(B, TEXT("pulse_s"), Out.PulseS); Out.PulseRing = MkNum(B, TEXT("pulse_ring"), Out.PulseRing);
	Out.BehindAlpha = MkNum(B, TEXT("behind_alpha"), Out.BehindAlpha); Out.EdgePx = MkNum(B, TEXT("edge_px"), Out.EdgePx);
	Out.MapTokenPx = MkNum(B, TEXT("map_token_px"), Out.MapTokenPx); Out.MiniTokenPx = MkNum(B, TEXT("mini_token_px"), Out.MiniTokenPx);
	B->TryGetBoolField(TEXT("edge_arrows"), Out.bEdgeArrows);
	return true;
}

bool LoadBrixMarkers(const FString& Path, FBrixMarkerRules& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("cannot read ") + Path; return false; }
	TSharedPtr<FJsonObject> J;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) { OutError = TEXT("markers.json is not JSON"); return false; }
	return ParseBrixMarkers(J, Out, OutError);
}

TArray<FString> BrixMarkerProblems(const TSharedPtr<FJsonObject>& J, const TSet<FString>& LotKinds)   // world.marker_problems()
{
	TArray<FString> Out;
	FBrixMarkerRules R;
	FString Error;
	if (!ParseBrixMarkers(J, R, Error)) return { TEXT("markers: ") + Error };
	for (const TCHAR* K : MkKinds) if (!R.Kinds.Contains(K)) Out.Add(FString::Printf(TEXT("markers: no kind %s"), K));
	TMap<FString, TArray<FString>> BySign;
	const TSharedPtr<FJsonObject> Kinds = J->GetObjectField(TEXT("kinds"));
	for (const auto& K : R.Kinds)
	{
		if (!R.Signs.Contains(K.Value.Sign)) Out.Add(TEXT("markers: kind ") + K.Key + TEXT(" has no sign ") + K.Value.Sign);
		FString C;
		if (!Kinds->GetObjectField(K.Key)->TryGetStringField(TEXT("colour"), C) || !MkHex(C)) Out.Add(TEXT("markers: kind ") + K.Key + TEXT("'s colour is not #rrggbb"));
		if (K.Value.Words.IsEmpty()) Out.Add(TEXT("markers: kind ") + K.Key + TEXT(" needs new (yes or no) and words"));
		BySign.FindOrAdd(K.Value.Sign).Add(K.Key);
	}
	for (auto& S : BySign)
	{
		S.Value.Sort();
		if (S.Value.Num() > 1 && S.Value != TArray<FString>({ TEXT("go"), TEXT("mark") })) Out.Add(TEXT("markers: kinds ") + FString::Join(S.Value, TEXT(", ")) + TEXT(" share the sign ") + S.Key + TEXT(" (each kind needs its own shape)"));
	}
	for (const auto& S : R.Signs)
	{
		bool bOut = false;
		for (const TArray<FVector2f>& L : S.Value.Lines) for (const FVector2f& P : L) bOut |= P.X < 0 || P.Y < 0 || P.X > 24 || P.Y > 24;
		if (bOut) Out.Add(TEXT("markers: sign ") + S.Key + TEXT(" goes outside its 24-unit square"));
	}
	for (const auto& P : R.PlaceKinds)
	{
		if (LotKinds.Num() && !LotKinds.Contains(P.Key)) Out.Add(TEXT("markers: place kind ") + P.Key + TEXT(" is not a kind of lot"));
		if (!R.Kinds.Contains(P.Value)) Out.Add(TEXT("markers: place kind ") + P.Key + TEXT(" points at no marker kind ") + P.Value);
	}
	if (!(0 < R.NearM && R.NearM < R.TokenM && R.TokenM < R.SparkM)) Out.Add(TEXT("markers: near_m < token_m < spark_m"));
	if (!(0 < R.FadeM && R.FadeM < R.TokenM && 0 < R.PlaceM && R.PlaceM <= R.TokenM)) Out.Add(TEXT("markers: fade_m and place_m within token_m"));
	if (!(0 < R.SparkPx && R.SparkPx < R.TokenPx && R.TokenPx <= R.TokenNearPx)) Out.Add(TEXT("markers: spark_px < token_px <= token_near_px"));
	return Out;
}

FString BrixMarkerKindFor(const FBrixMapTarget& T, const FBrixMarkerRules& R, const FString& LotKind)
{
	if (T.Kind == TEXT("go") && T.Lot >= 0)
		if (const FString* P = R.PlaceKinds.Find(LotKind)) return *P;
	return T.Kind.IsEmpty() ? FString(TEXT("go")) : T.Kind;
}
