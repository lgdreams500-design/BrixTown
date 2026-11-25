#include "BrixWardrobe.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

bool FBrixWardrobe::Save(const FBrixDesignerRules& D, const FBrixDesign& V, const FString& Name, FString& OutWhy)
{
	const FString N = Name.TrimStartAndEnd();
	const TSharedRef<FJsonObject> O = BrixOutfitOf(D, V, N);
	if (BrixOutfitProblems(D, *O).Num()) { OutWhy = FString::Printf(TEXT("an outfit needs a name of 1 to %d letters"), D.NameMax); return false; }
	const int32 K = Outfits.IndexOfByPredicate([&](const TSharedRef<FJsonObject>& X) { return X->GetStringField(TEXT("name")) == N; });
	if (K < 0 && Outfits.Num() >= D.MaxOutfits) { OutWhy = FString::Printf(TEXT("the wardrobe holds %d outfits"), D.MaxOutfits); return false; }
	if (K >= 0) Outfits[K] = O;
	else Outfits.Add(O);
	return true;
}

bool FBrixWardrobe::Forget(const FString& Name)
{
	return Outfits.RemoveAll([&](const TSharedRef<FJsonObject>& X) { return X->GetStringField(TEXT("name")) == Name; }) > 0;
}

const FJsonObject* FBrixWardrobe::Find(const FString& Name) const
{
	for (const TSharedRef<FJsonObject>& X : Outfits) if (X->GetStringField(TEXT("name")) == Name) return &X.Get();
	return nullptr;
}

TArray<FString> FBrixWardrobe::Names() const
{
	TArray<FString> Out;
	for (const TSharedRef<FJsonObject>& X : Outfits) Out.Add(X->GetStringField(TEXT("name")));
	return Out;
}

TArray<TSharedPtr<FJsonValue>> FBrixWardrobe::ToJson() const
{
	TArray<TSharedPtr<FJsonValue>> Out;
	for (const TSharedRef<FJsonObject>& X : Outfits) Out.Add(MakeShared<FJsonValueObject>(X));
	return Out;
}

void FBrixWardrobe::Load(const FBrixDesignerRules& D, const TArray<TSharedPtr<FJsonValue>>& List)
{
	Outfits.Reset();
	for (const TSharedPtr<FJsonValue>& V : List)
	{
		const TSharedPtr<FJsonObject>* O = nullptr;
		if (V.IsValid() && V->TryGetObject(O) && !BrixOutfitProblems(D, **O).Num() && Outfits.Num() < D.MaxOutfits) Outfits.Add(O->ToSharedRef());
	}
}

bool LoadBrixGarments(const FString& LifePath, TMap<FString, FBrixGarment>& Out, FString& OutError)
{
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *LifePath)) { OutError = TEXT("Cannot read ") + LifePath; return false; }
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J.IsValid()) { OutError = LifePath + TEXT(" is not valid JSON"); return false; }
	const TSharedPtr<FJsonObject>* Goods;
	const TSharedPtr<FJsonObject>* List;
	if (!J->TryGetObjectField(TEXT("goods"), Goods) || !(*Goods)->TryGetObjectField(TEXT("goods"), List)) { OutError = LifePath + TEXT(" has no goods"); return false; }
	Out.Reset();
	for (const auto& KV : (*List)->Values)
	{
		const TSharedPtr<FJsonObject> G = KV.Value->AsObject();
		FString Goes;
		const TSharedPtr<FJsonObject>* Parts;
		if (!G || !G->TryGetStringField(TEXT("goes"), Goes) || Goes != TEXT("wear") || !G->TryGetObjectField(TEXT("parts"), Parts)) continue;
		FBrixGarment W;
		W.Id = FString(KV.Key);
		G->TryGetStringField(TEXT("name"), W.Name);
		W.Parts = *Parts;
		Out.Add(W.Id, W);
	}
	return true;
}

TArray<FString> BrixGarmentProblems(const FBrixDesignerRules& D, const FBrixGarment& G)
{
	TArray<FString> Out;
	if (!G.Parts.IsValid() || !G.Parts->Values.Num()) { Out.Add(G.Id + TEXT(" sets no part of the look")); return Out; }
	for (const auto& KV : G.Parts->Values)
		if (!D.WardrobeParts.Contains(FString(KV.Key))) Out.Add(G.Id + TEXT(" cannot change ") + FString(KV.Key));
	FBrixDesign V = D.Default;
	if (!BrixDesignFromJson(*G.Parts, V)) Out.Add(G.Id + TEXT(" has a part of the wrong kind"));
	for (const FString& P : BrixLookProblems(D, V)) Out.Add(G.Id + TEXT(": ") + P);
	return Out;
}

bool BrixIsWearing(const FBrixDesign& V, const FBrixGarment& G)
{
	if (!G.Parts.IsValid()) return false;
	FBrixDesign On = V;
	BrixDesignFromJson(*G.Parts, On);
	return On == V;
}

TArray<FString> BrixGarmentWords(const FBrixDesignerRules& D, const FBrixGarment& G)
{
	TArray<FString> Words;
	if (!G.Parts.IsValid()) return Words;
	for (const auto& KV : G.Parts->Values)
	{
		const FString K(KV.Key), V = KV.Value->Type == EJson::String ? KV.Value->AsString() : FString();
		if (K == TEXT("garment")) for (const auto& X : D.Garment) { if (X.Key == V) Words.Add(X.Value); }
		if (K == TEXT("bottom_type")) for (const auto& X : D.BottomType) { if (X.Key == V) Words.Add(X.Value); }
		if (K == TEXT("bag")) for (const auto& X : D.Extras) { if (X.Key == TEXT("bag")) Words.Add(X.Value); }
	}
	return Words;
}

void BrixPutOn(const FBrixGarment& G, FBrixDesign& V)
{
	if (G.Parts.IsValid()) BrixDesignFromJson(*G.Parts, V);
}
