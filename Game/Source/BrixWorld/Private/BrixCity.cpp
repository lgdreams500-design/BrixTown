#include "BrixCity.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, BrixWorld);

namespace
{
	bool ReadJson(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path))
		{
			OutError = FString::Printf(TEXT("Cannot read %s"), *Path);
			return false;
		}
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid())
		{
			OutError = FString::Printf(TEXT("%s is not valid JSON: %s"), *Path, *Reader->GetErrorMessage());
			return false;
		}
		return true;
	}

	// A JSON array of numbers [a, b, ...] as a vector of its first two (0 where missing).
	FVector2D Vec2(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, int32 First = 0)
	{
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (!Obj->TryGetArrayField(Field, Arr) || Arr->Num() < First + 2) return FVector2D::ZeroVector;
		return FVector2D((*Arr)[First]->AsNumber(), (*Arr)[First + 1]->AsNumber());
	}

	FString Str(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		FString S;
		Obj->TryGetStringField(Field, S);
		return S;
	}

	double Num(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field, double Default = 0.0)
	{
		double V = Default;
		Obj->TryGetNumberField(Field, V);
		return V;
	}
}

int32 FBrixCity::NumLots() const
{
	int32 N = 0;
	for (const FBrixBlock& B : Blocks) N += B.Lots.Num();
	return N;
}

int32 FBrixCity::NumTrees() const
{
	int32 N = 0;
	for (const FBrixBlock& B : Blocks) N += B.Trees.Num();
	return N;
}

bool LoadBrixCity(const FString& CityDir, FBrixCity& OutCity, FString& OutError)
{
	OutCity = FBrixCity();
	TSharedPtr<FJsonObject> Root;
	if (!ReadJson(FPaths::Combine(CityDir, TEXT("city.json")), Root, OutError)) return false;

	OutCity.Id = Str(Root, TEXT("id"));
	OutCity.Name = Str(Root, TEXT("name"));
	const TSharedPtr<FJsonObject>* Grid = nullptr;
	if (Root->TryGetObjectField(TEXT("grid"), Grid))
	{
		OutCity.Columns = (int32)Num(*Grid, TEXT("columns"));
		OutCity.Rows = (int32)Num(*Grid, TEXT("rows"));
		OutCity.Pitch = (float)Num(*Grid, TEXT("pitch"));
		OutCity.StreetWidth = (float)Num(*Grid, TEXT("street_width"));
		OutCity.BlockSize = (float)Num(*Grid, TEXT("block_size"));
	}
	const TSharedPtr<FJsonObject>* Spawn = nullptr;
	if (Root->TryGetObjectField(TEXT("spawn"), Spawn))
	{
		OutCity.SpawnPos = Vec2(*Spawn, TEXT("pos"));
		OutCity.SpawnFacing = Vec2(*Spawn, TEXT("facing"));
		OutCity.SpawnTime = Str(*Spawn, TEXT("time"));
	}

	const TArray<TSharedPtr<FJsonValue>>* BlockList = nullptr;
	if (!Root->TryGetArrayField(TEXT("blocks"), BlockList))
	{
		OutError = TEXT("city.json lists no blocks");
		return false;
	}
	for (const TSharedPtr<FJsonValue>& Entry : *BlockList)
	{
		const TSharedPtr<FJsonObject> E = Entry->AsObject();
		TSharedPtr<FJsonObject> B;
		if (!E.IsValid() || !ReadJson(FPaths::Combine(CityDir, Str(E, TEXT("file"))), B, OutError)) return false;

		FBrixBlock& Block = OutCity.Blocks.AddDefaulted_GetRef();
		Block.Id = Str(B, TEXT("id"));
		Block.Template = Str(B, TEXT("template"));
		Block.District = Str(B, TEXT("district"));
		Block.Origin = Vec2(B, TEXT("origin"));
		const double Size = Num(B, TEXT("size"), OutCity.BlockSize);
		FVector2D Span = Vec2(B, TEXT("span"));
		if (Span.IsZero()) Span = FVector2D(1, 1);
		Block.Extent = FVector2D(Size + (Span.X - 1) * OutCity.Pitch, Size + (Span.Y - 1) * OutCity.Pitch);

		const TArray<TSharedPtr<FJsonValue>>* Lots = nullptr;
		if (B->TryGetArrayField(TEXT("lots"), Lots))
		{
			for (const TSharedPtr<FJsonValue>& LV : *Lots)
			{
				const TSharedPtr<FJsonObject> L = LV->AsObject();
				if (!L.IsValid()) continue;
				FBrixLot& Lot = Block.Lots.AddDefaulted_GetRef();
				Lot.Id = Str(L, TEXT("id"));
				Lot.Kind = Str(L, TEXT("kind"));
				Lot.Name = Str(L, TEXT("name"));
				Lot.Min = Vec2(L, TEXT("rect"), 0);
				Lot.Max = Vec2(L, TEXT("rect"), 2);
				Lot.Door = Vec2(L, TEXT("door"));
				Lot.Floors = (int32)Num(L, TEXT("floors"));
				Lot.AngleDeg = (float)Num(L, TEXT("angle"));
				L->TryGetBoolField(TEXT("enterable"), Lot.bEnterable);
			}
		}
		const TArray<TSharedPtr<FJsonValue>>* Trees = nullptr;
		if (B->TryGetArrayField(TEXT("trees"), Trees))
		{
			for (const TSharedPtr<FJsonValue>& TV : *Trees)
			{
				const TSharedPtr<FJsonObject> T = TV->AsObject();
				if (!T.IsValid()) continue;
				FBrixTree& Tree = Block.Trees.AddDefaulted_GetRef();
				Tree.Id = Str(T, TEXT("id"));
				Tree.Species = Str(T, TEXT("species"));
				Tree.Pos = Vec2(T, TEXT("pos"));
			}
		}
	}
	return true;
}

TArray<FString> CheckBrixCity(const FBrixCity& City)
{
	TArray<FString> Problems;
	TSet<FString> Seen;
	if (City.Blocks.IsEmpty()) Problems.Add(TEXT("the city has no blocks"));
	for (const FBrixBlock& B : City.Blocks)
	{
		for (const FBrixLot& L : B.Lots)
		{
			const FString Where = FString::Printf(TEXT("%s lot %s"), *B.Id, *L.Id);
			if (L.Id.IsEmpty()) Problems.Add(Where + TEXT(": no id"));
			else if (Seen.Contains(L.Id)) Problems.Add(Where + TEXT(": id used twice"));
			Seen.Add(L.Id);
			if (L.Kind.IsEmpty()) Problems.Add(Where + TEXT(": no kind"));
			if (L.Name.IsEmpty()) Problems.Add(Where + TEXT(": no name"));
			if (L.Size().X <= 0 || L.Size().Y <= 0) Problems.Add(Where + TEXT(": footprint has no area"));
			if (L.Door.IsZero()) Problems.Add(Where + TEXT(": no door"));
			if (!L.IsOpen() && L.Floors < 1) Problems.Add(Where + TEXT(": a building with no floors"));
		}
	}
	return Problems;
}
