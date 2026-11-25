#include "BrixDesigner.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	const TCHAR* const SwatchKeys[3] = { TEXT("skin"), TEXT("hair_color"), TEXT("eyes") };
	const TCHAR* const ExtraKeys[3] = { TEXT("scarf"), TEXT("cap"), TEXT("apron") };

	FVector3f V3(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 3 ? FVector3f((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber()) : FVector3f::ZeroVector;
	}
	TArray<FVector3f> V3List(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		TArray<FVector3f> Out;
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (O->TryGetArrayField(K, A)) for (const TSharedPtr<FJsonValue>& V : *A) Out.Add(V3(V->AsArray()));
		return Out;
	}
	TArray<TPair<FString, FString>> Labels(const TSharedPtr<FJsonObject>& O, const TCHAR* K)
	{
		TArray<TPair<FString, FString>> Out;
		const TSharedPtr<FJsonObject>* M;
		if (O->TryGetObjectField(K, M)) for (const auto& KV : (*M)->Values) Out.Emplace(FString(KV.Key), KV.Value->AsString());
		return Out;
	}
	int32& Swatch(FBrixDesign& V, int32 k) { return k == 0 ? V.Skin : k == 1 ? V.HairColor : V.Eyes; }
	int32 Swatch(const FBrixDesign& V, int32 k) { return k == 0 ? V.Skin : k == 1 ? V.HairColor : V.Eyes; }
	FString& Extra(FBrixDesign& V, int32 k) { return k == 0 ? V.Scarf : k == 1 ? V.Cap : V.Apron; }
	const FString& Extra(const FBrixDesign& V, int32 k) { return k == 0 ? V.Scarf : k == 1 ? V.Cap : V.Apron; }
	const TArray<FVector3f>& SwatchList(const FBrixDesignerRules& D, int32 k) { return k == 0 ? D.Skin : k == 1 ? D.HairColor : D.Eyes; }
	struct FNamed { const TCHAR* Key; FString FBrixDesign::* Field; };
	const FNamed Named[] = { { TEXT("hair"), &FBrixDesign::Hair }, { TEXT("garment"), &FBrixDesign::Garment }, { TEXT("bottom_type"), &FBrixDesign::BottomType },
		{ TEXT("walk"), &FBrixDesign::Walk }, { TEXT("top"), &FBrixDesign::Top }, { TEXT("bottom"), &FBrixDesign::Bottom }, { TEXT("shoes"), &FBrixDesign::Shoes } };
}

bool FBrixDesign::operator==(const FBrixDesign& O) const
{
	if (Sliders.Num() != O.Sliders.Num()) return false;
	for (const auto& KV : Sliders) { const double* X = O.Sliders.Find(KV.Key); if (!X || *X != KV.Value) return false; }
	return Name == O.Name && Skin == O.Skin && HairColor == O.HairColor && Eyes == O.Eyes && Hair == O.Hair && Garment == O.Garment && Top == O.Top
		&& BottomType == O.BottomType && Bottom == O.Bottom && Shoes == O.Shoes && Walk == O.Walk && Scarf == O.Scarf && Cap == O.Cap && Apron == O.Apron
		&& bBag == O.bBag && bTash == O.bTash;
}

const FVector3f* FBrixDesignerRules::Colour(const FString& Name) const
{
	for (const auto& P : Palette) if (P.Key == Name) return &P.Value;
	return nullptr;
}

const FBrixDesignerSlider* FBrixDesignerRules::SliderRule(const FString& Key) const { return Sliders.FindByPredicate([&](const FBrixDesignerSlider& S) { return S.Key == Key; }); }

bool FBrixDesignerRules::Has(const TArray<TPair<FString, FString>>& List, const FString& Key) { return List.ContainsByPredicate([&](const TPair<FString, FString>& P) { return P.Key == Key; }); }

bool LoadBrixDesigner(const FString& DesignerPath, const FString& SettingsPath, FBrixDesignerRules& Out, FString& OutError)
{
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *DesignerPath)) { OutError = TEXT("Cannot read ") + DesignerPath; return false; }
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) { OutError = DesignerPath + TEXT(" is not valid JSON"); return false; }
	Out = FBrixDesignerRules();
	const TSharedPtr<FJsonObject>* S;
	if (J->TryGetObjectField(TEXT("sliders"), S))
		for (const auto& KV : (*S)->Values)
		{
			const TSharedPtr<FJsonObject> O = KV.Value->AsObject();
			FBrixDesignerSlider R;
			R.Key = FString(KV.Key);
			O->TryGetStringField(TEXT("label"), R.Label);
			O->TryGetStringField(TEXT("unit"), R.Unit);
			R.Min = O->GetNumberField(TEXT("min")); R.Max = O->GetNumberField(TEXT("max"));
			R.Step = O->GetNumberField(TEXT("step")); R.Default = O->GetNumberField(TEXT("default"));
			Out.Sliders.Add(R);
		}
	Out.Skin = V3List(J, TEXT("skin"));
	Out.HairColor = V3List(J, TEXT("hair_color"));
	Out.Eyes = V3List(J, TEXT("eyes"));
	const TArray<TSharedPtr<FJsonValue>>* H;
	if (J->TryGetArrayField(TEXT("hair"), H)) for (const TSharedPtr<FJsonValue>& V : *H) Out.Hair.Add(V->AsString());
	Out.Garment = Labels(J, TEXT("garment"));
	Out.BottomType = Labels(J, TEXT("bottom_type"));
	Out.Extras = Labels(J, TEXT("extras"));
	Out.Walk = Labels(J, TEXT("walk"));
	const TSharedPtr<FJsonObject>* P;
	if (J->TryGetObjectField(TEXT("palette"), P)) for (const auto& KV : (*P)->Values) Out.Palette.Emplace(FString(KV.Key), V3(KV.Value->AsArray()));
	const TSharedPtr<FJsonObject>* N;
	if (J->TryGetObjectField(TEXT("name"), N)) Out.NameMax = (int32)(*N)->GetNumberField(TEXT("max"));
	const TSharedPtr<FJsonObject>* Def;
	if (!J->TryGetObjectField(TEXT("default"), Def) || !BrixDesignFromJson(**Def, Out.Default)) { OutError = DesignerPath + TEXT(" has no sound default"); return false; }
	if (!Out.Sliders.Num() || !Out.Skin.Num() || !Out.Palette.Num() || !Out.Hair.Num()) { OutError = DesignerPath + TEXT(" has no sliders, skins, hair or palette"); return false; }

	TSharedPtr<FJsonObject> St;
	if (FFileHelper::LoadFileToString(Text, *SettingsPath) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), St) && St.IsValid())
	{
		const TSharedPtr<FJsonObject>* Sv;
		double Slots = 0;
		if (St->TryGetObjectField(TEXT("save"), Sv) && (*Sv)->TryGetNumberField(TEXT("slots"), Slots)) Out.SaveSlots = (int32)Slots;
		const TSharedPtr<FJsonObject>* W;
		if (St->TryGetObjectField(TEXT("wardrobe"), W))
		{
			const TArray<TSharedPtr<FJsonValue>>* Parts;
			if ((*W)->TryGetArrayField(TEXT("parts"), Parts)) for (const TSharedPtr<FJsonValue>& V : *Parts) Out.WardrobeParts.Add(V->AsString());
			double M = 0;
			if ((*W)->TryGetNumberField(TEXT("max_outfits"), M)) Out.MaxOutfits = (int32)M;
		}
	}
	if (!Out.WardrobeParts.Num()) { OutError = SettingsPath + TEXT(" has no wardrobe parts"); return false; }
	return true;
}

FBrixLook BrixDesignLook(const FBrixDesign& V, const FBrixDesignerRules& D)
{
	FBrixLook L;
	auto C = [&](const FString& K) { const FVector3f* X = D.Colour(K); return X ? *X : FVector3f::ZeroVector; };
	auto Opt = [&](const FString& K) { TOptional<FVector3f> O; if (const FVector3f* X = K.IsEmpty() ? nullptr : D.Colour(K)) O = *X; return O; };
	L.S = V.Slider(TEXT("height")) / 1.72;
	L.W = V.Slider(TEXT("build")) * 1.04;
	L.Skin = D.Skin.IsValidIndex(V.Skin) ? D.Skin[V.Skin] : FVector3f::ZeroVector;
	L.Hair = V.Hair;
	L.HairC = D.HairColor.IsValidIndex(V.HairColor) ? D.HairColor[V.HairColor] : FVector3f::ZeroVector;
	L.Eyes = D.Eyes.IsValidIndex(V.Eyes) ? D.Eyes[V.Eyes] : FVector3f::ZeroVector;
	L.Garment = V.Garment;
	L.Top = C(V.Top);
	L.Bottom = C(V.Bottom);
	L.Legs = V.BottomType;
	L.Shoes = C(V.Shoes);
	L.Scarf = Opt(V.Scarf);
	L.Cap = Opt(V.Cap);
	L.Apron = Opt(V.Apron);
	L.bBag = V.bBag;
	L.bTash = V.bTash;
	L.SigName = FBrixDesignerRules::Has(D.Walk, V.Walk) ? V.Walk : TEXT("Springy");   // SIG[v.walk] || SIG.Springy
	L.Seed = 7;
	L.FaceW = V.Slider(TEXT("face_w")); L.EyeSize = V.Slider(TEXT("eye_size")); L.EyeOpen = V.Slider(TEXT("eye_open"));
	L.Brow = V.Slider(TEXT("brow")); L.Nose = V.Slider(TEXT("nose")); L.Lips = V.Slider(TEXT("lips"));
	return L;
}

TArray<FString> BrixLookProblems(const FBrixDesignerRules& D, const FBrixDesign& V)
{
	TArray<FString> Out;
	for (const FBrixDesignerSlider& S : D.Sliders)
	{
		const double* X = V.Sliders.Find(S.Key);
		if (!X || !(*X >= S.Min - 1e-9 && *X <= S.Max + 1e-9)) Out.Add(S.Key + TEXT(" out of range"));
	}
	for (int32 k = 0; k < 3; ++k)
		if (!SwatchList(D, k).IsValidIndex(Swatch(V, k))) Out.Add(FString(SwatchKeys[k]) + TEXT(" is not a swatch"));
	if (!D.Hair.Contains(V.Hair)) Out.Add(TEXT("hair ") + V.Hair + TEXT(" is not offered"));
	if (!FBrixDesignerRules::Has(D.Garment, V.Garment)) Out.Add(TEXT("garment ") + V.Garment + TEXT(" is not offered"));
	if (!FBrixDesignerRules::Has(D.BottomType, V.BottomType)) Out.Add(TEXT("bottom_type ") + V.BottomType + TEXT(" is not offered"));
	if (!FBrixDesignerRules::Has(D.Walk, V.Walk)) Out.Add(TEXT("walk ") + V.Walk + TEXT(" is not offered"));
	const TPair<const TCHAR*, const FString*> Colours[3] = { { TEXT("top"), &V.Top }, { TEXT("bottom"), &V.Bottom }, { TEXT("shoes"), &V.Shoes } };
	for (const auto& Cl : Colours) if (!D.Colour(*Cl.Value)) Out.Add(FString(Cl.Key) + TEXT(" colour ") + *Cl.Value + TEXT(" is not in the palette"));
	for (int32 k = 0; k < 3; ++k)
		if (!Extra(V, k).IsEmpty() && !D.Colour(Extra(V, k))) Out.Add(FString(ExtraKeys[k]) + TEXT(" colour ") + Extra(V, k) + TEXT(" is not in the palette"));
	const int32 Len = V.Name.TrimStartAndEnd().Len();
	if (!(Len > 0 && Len <= D.NameMax)) Out.Add(FString::Printf(TEXT("the name must be 1 to %d letters"), D.NameMax));
	return Out;
}

TSharedRef<FJsonObject> BrixDesignToJson(const FBrixDesign& V)
{
	TSharedRef<FJsonObject> J = MakeShared<FJsonObject>();
	J->SetStringField(TEXT("name"), V.Name);
	for (const auto& KV : V.Sliders) J->SetNumberField(KV.Key, KV.Value);
	for (int32 k = 0; k < 3; ++k) J->SetNumberField(SwatchKeys[k], Swatch(V, k));
	for (const FNamed& F : Named) J->SetStringField(F.Key, V.*F.Field);
	for (int32 k = 0; k < 3; ++k)
		if (Extra(V, k).IsEmpty()) J->SetField(ExtraKeys[k], MakeShared<FJsonValueNull>());
		else J->SetStringField(ExtraKeys[k], Extra(V, k));
	J->SetBoolField(TEXT("bag"), V.bBag);
	J->SetBoolField(TEXT("tash"), V.bTash);
	return J;
}

bool BrixDesignFromJson(const FJsonObject& J, FBrixDesign& V)
{
	bool bOk = true;
	for (const auto& KV : J.Values)
	{
		const FString K(KV.Key);
		const TSharedPtr<FJsonValue>& X = KV.Value;
		if (K == TEXT("name")) { bOk &= X->Type == EJson::String; V.Name = X->AsString(); continue; }
		if (K == TEXT("bag") || K == TEXT("tash")) { bOk &= X->Type == EJson::Boolean; (K == TEXT("bag") ? V.bBag : V.bTash) = X->Type == EJson::Boolean && X->AsBool(); continue; }
		bool bDone = false;
		for (int32 k = 0; k < 3 && !bDone; ++k)
			if (K == SwatchKeys[k])
			{
				const double N = X->Type == EJson::Number ? X->AsNumber() : -1.0;
				bOk &= X->Type == EJson::Number && N == FMath::FloorToDouble(N);
				Swatch(V, k) = (int32)N;
				bDone = true;
			}
			else if (K == ExtraKeys[k])
			{
				bOk &= X->Type == EJson::Null || X->Type == EJson::String;
				Extra(V, k) = X->Type == EJson::String ? X->AsString() : FString();
				bDone = true;
			}
		for (const FNamed& F : Named)
			if (!bDone && K == F.Key) { bOk &= X->Type == EJson::String; V.*F.Field = X->AsString(); bDone = true; }
		if (!bDone)
		{
			bOk &= X->Type == EJson::Number;   // a slider (or a value the designer does not know: kept, and look_problems ignores it)
			if (X->Type == EJson::Number) V.Sliders.Add(K, X->AsNumber());
		}
	}
	return bOk;
}

TSharedRef<FJsonObject> BrixOutfitOf(const FBrixDesignerRules& D, const FBrixDesign& V, const FString& Name)
{
	const TSharedRef<FJsonObject> All = BrixDesignToJson(V);
	TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
	for (const FString& P : D.WardrobeParts) if (const TSharedPtr<FJsonValue> X = All->TryGetField(P)) O->SetField(P, X);
	O->SetStringField(TEXT("name"), Name);
	return O;
}

TArray<FString> BrixOutfitProblems(const FBrixDesignerRules& D, const FJsonObject& Outfit)
{
	TArray<FString> Out;
	for (const auto& KV : Outfit.Values)
		if (KV.Key != TEXT("name") && !D.WardrobeParts.Contains(FString(KV.Key))) Out.Add(TEXT("an outfit cannot change ") + FString(KV.Key));
	for (const FString& P : D.WardrobeParts)
		if (!Outfit.HasField(P)) Out.Add(TEXT("the outfit has no ") + P);
	FBrixDesign Full = D.Default;
	BrixDesignFromJson(Outfit, Full);
	Full.Name = D.Default.Name;
	for (const FString& P : BrixLookProblems(D, Full))
	{
		FString First = P;
		P.Split(TEXT(" "), &First, nullptr);
		if (D.WardrobeParts.Contains(First)) Out.Add(P);
	}
	FString Name;
	const int32 Len = Outfit.TryGetStringField(TEXT("name"), Name) ? Name.TrimStartAndEnd().Len() : 0;
	if (!(Len > 0 && Len <= D.NameMax)) Out.Add(FString::Printf(TEXT("an outfit needs a name of 1 to %d letters"), D.NameMax));
	return Out;
}

void BrixWearOutfit(const FBrixDesignerRules& D, const FJsonObject& Outfit, FBrixDesign& V)
{
	FJsonObject Parts;
	for (const FString& P : D.WardrobeParts) if (const TSharedPtr<FJsonValue> X = Outfit.TryGetField(P)) Parts.SetField(P, X);
	BrixDesignFromJson(Parts, V);
}
