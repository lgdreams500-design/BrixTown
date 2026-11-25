#include "BrixGoods.h"
#include "BrixDay.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	// goods.js ACCENTS: terracotta, teal, ochre, plum, blue, olive
	const FVector3f ACCENTS[6] = { FVector3f(.5f, .25f, .19f), FVector3f(.2f, .37f, .38f), FVector3f(.6f, .46f, .2f), FVector3f(.36f, .26f, .38f), FVector3f(.28f, .38f, .52f), FVector3f(.33f, .37f, .2f) };

	void Ints(const TSharedPtr<FJsonObject>& O, const TCHAR* F, TArray<int32>& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!O->TryGetArrayField(F, A)) return;
		Out.Reserve(A->Num());
		for (const TSharedPtr<FJsonValue>& V : *A) Out.Add((int32)V->AsNumber());
	}
}

TArray<FVector3f> FBrixGoodsModels::Colours(const FString& Seed) const
{
	TArray<FVector3f> Out = Palette;
	const int32 k0 = (int32)FMath::FloorToDouble(BrixSliceHash({ Seed, TEXT("a0") }) * 6) % 6;
	const int32 k1 = (k0 + 1 + (int32)FMath::FloorToDouble(BrixSliceHash({ Seed, TEXT("a1") }) * 5) % 5) % 6;
	if (Out.IsValidIndex(A0)) Out[A0] = ACCENTS[k0];
	if (Out.IsValidIndex(A1)) Out[A1] = ACCENTS[k1];
	return Out;
}

bool LoadBrixGoodsModels(const FString& Path, FBrixGoodsModels& Out, FString& OutError)
{
	FString Text;
	TSharedPtr<FJsonObject> J;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = Path + TEXT(" is not there (run Tools/worldgen/build_play.py)"); return false; }
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) || !J) { OutError = Path + TEXT(" does not read"); return false; }
	Out = FBrixGoodsModels();
	const TArray<TSharedPtr<FJsonValue>>* Pal = nullptr;
	const TSharedPtr<FJsonObject>* Models = nullptr;
	if (!J->TryGetArrayField(TEXT("palette"), Pal) || !J->TryGetObjectField(TEXT("models"), Models)) { OutError = Path + TEXT(" has no palette or models"); return false; }
	for (const TSharedPtr<FJsonValue>& V : *Pal)
	{
		const TArray<TSharedPtr<FJsonValue>>& E = V->AsArray();
		const TArray<TSharedPtr<FJsonValue>>& C = E[1]->AsArray();
		const FString Name = E[0]->AsString();
		if (Name == TEXT("accent0")) Out.A0 = Out.Palette.Num();
		if (Name == TEXT("accent1")) Out.A1 = Out.Palette.Num();
		Out.Palette.Add(FVector3f((float)C[0]->AsNumber(), (float)C[1]->AsNumber(), (float)C[2]->AsNumber()));
	}
	for (const auto& K : (*Models)->Values)
	{
		const TSharedPtr<FJsonObject> O = K.Value->AsObject();
		FBrixGoodsModel M;
		M.Name = O->GetStringField(TEXT("name"));
		const TArray<TSharedPtr<FJsonValue>>& S = O->GetArrayField(TEXT("size"));
		if (S.Num() == 3) M.Size = FVector3d(S[0]->AsNumber(), S[1]->AsNumber(), S[2]->AsNumber());
		const TArray<TSharedPtr<FJsonValue>>& Lods = O->GetArrayField(TEXT("lods"));
		for (int32 l = 0; l < 2 && l < Lods.Num(); ++l)
		{
			const TSharedPtr<FJsonObject> L = Lods[l]->AsObject();
			Ints(L, TEXT("p"), M.Lods[l].P); Ints(L, TEXT("n"), M.Lods[l].N); Ints(L, TEXT("c"), M.Lods[l].C); Ints(L, TEXT("i"), M.Lods[l].I);
			const FBrixGoodsModel::FLod& D = M.Lods[l];
			if (D.P.Num() != D.N.Num() || D.P.Num() != D.C.Num() * 3 || D.I.Num() % 3) { OutError = FString(K.Key.ToView()) + TEXT(": its points, normals and colours do not match"); return false; }
			for (const int32 q : D.I) if (q < 0 || q >= D.C.Num()) { OutError = FString(K.Key.ToView()) + TEXT(": a triangle names a point it does not have"); return false; }
			for (const int32 c : D.C) if (!Out.Palette.IsValidIndex(c)) { OutError = FString(K.Key.ToView()) + TEXT(": a colour not in the palette"); return false; }
		}
		if (!M.Lods[1].I.Num()) M.Lods[1] = M.Lods[0];
		Out.Models.Add(FString(K.Key.ToView()), MoveTemp(M));
	}
	if (!Out.Models.Num()) { OutError = Path + TEXT(" has no models"); return false; }
	return true;
}

void BrixGoodPut(const FBrixGoodsModels& Models, const FBrixGoodsModel& M, double X, double Y, double Z, double Yaw, const FString& Seed, int32 Lod,
	TArray<FVector3d>& P, TArray<FVector3f>& N, TArray<FVector3f>& C)
{
	const FBrixGoodsModel::FLod& L = M.Lods[Lod ? 1 : 0];
	const TArray<FVector3f> Col = Models.Colours(Seed);
	const double cs = FMath::Cos(Yaw), sn = FMath::Sin(Yaw);
	P.Reserve(P.Num() + L.I.Num()); N.Reserve(N.Num() + L.I.Num()); C.Reserve(C.Num() + L.I.Num());
	for (const int32 q : L.I)
	{
		const int32 i = q * 3;
		const double u = L.P[i] * .001, v = L.P[i + 2] * .001, nu = L.N[i] * .01, nv = L.N[i + 2] * .01;
		P.Add(FVector3d(X + cs * u + sn * v, Y + L.P[i + 1] * .001, Z - sn * u + cs * v));
		N.Add(FVector3f((float)(cs * nu + sn * nv), (float)(L.N[i + 1] * .01), (float)(-sn * nu + cs * nv)));
		C.Add(Col[L.C[q]]);
	}
}
