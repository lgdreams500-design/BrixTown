#include "BrixCity.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
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

	TArray<FString> Strings(const TSharedPtr<FJsonObject>& Obj, const TCHAR* Field)
	{
		TArray<FString> Out;
		Obj->TryGetStringArrayField(Field, Out);
		return Out;
	}

	// A 'surface' field: {"wall": "rubble", ...}
	TMap<FString, FString> OwnSurface(const TSharedPtr<FJsonObject>& Obj)
	{
		TMap<FString, FString> Out;
		const TSharedPtr<FJsonObject>* S = nullptr;
		if (Obj->TryGetObjectField(TEXT("surface"), S))
			for (const auto& KV : (*S)->Values) Out.Add(FString(KV.Key.ToView()), KV.Value->AsString());
		return Out;
	}

	// World/rules/surfaces.json into the city's patterns and rules (ADR-0017)
	bool ReadSurfaces(const FString& Path, FBrixCity& City, FString& OutError)
	{
		TSharedPtr<FJsonObject> S;
		if (!ReadJson(Path, S, OutError)) return false;
		const TSharedPtr<FJsonObject>* Pats = nullptr;
		if (S->TryGetObjectField(TEXT("patterns"), Pats))
		{
			for (const auto& KV : (*Pats)->Values)
			{
				const TSharedPtr<FJsonObject> P = KV.Value->AsObject();
				FBrixPattern& Pt = City.Patterns.AddDefaulted_GetRef();
				Pt.Name = FString(KV.Key.ToView());
				Pt.For = Str(P, TEXT("for"));
				Pt.Type = Str(P, TEXT("type"));
				Pt.Unit = Vec2(P, TEXT("unit"));
				Pt.Joint = (float)Num(P, TEXT("joint"), 0.02);
				Pt.Vary = (float)Num(P, TEXT("vary"), 0.8);
				Pt.Bevel = (float)Num(P, TEXT("bevel"));
				Pt.Dome = (float)Num(P, TEXT("dome"));
				Pt.Mortar = (float)Num(P, TEXT("mortar"));
				Pt.Upper = (float)Num(P, TEXT("upper"));
				const TArray<TSharedPtr<FJsonValue>>* Cols = nullptr;
				if (P->TryGetArrayField(TEXT("colors"), Cols))
					for (const TSharedPtr<FJsonValue>& C : *Cols)
					{
						if (C->Type == EJson::String) { Pt.Colors.Add(TOptional<FVector3f>()); continue; }
						const TArray<TSharedPtr<FJsonValue>>& A = C->AsArray();
						Pt.Colors.Add(FVector3f((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber()));
					}
			}
		}
		const TSharedPtr<FJsonObject>* Rules = nullptr;
		if (S->TryGetObjectField(TEXT("rules"), Rules))
		{
			for (const auto& KV : (*Rules)->Values)
			{
				TArray<FBrixSurfaceRule>& List = City.SurfaceRules.Add(FString(KV.Key.ToView()));
				for (const TSharedPtr<FJsonValue>& RV : KV.Value->AsArray())
				{
					const TSharedPtr<FJsonObject> R = RV->AsObject();
					FBrixSurfaceRule& Rule = List.AddDefaulted_GetRef();
					const TSharedPtr<FJsonObject>* If = nullptr;
					if (R->TryGetObjectField(TEXT("if"), If))
						for (const auto& C : (*If)->Values)
						{
							TArray<FString> Want;
							for (const TSharedPtr<FJsonValue>& W : C.Value->AsArray()) Want.Add(W->AsString());
							Rule.If.Add(FString(C.Key.ToView()), Want);
						}
					const TSharedPtr<FJsonValue> Use = R->TryGetField(TEXT("use"));
					if (Use.IsValid() && Use->Type == EJson::String) Rule.Use = Use->AsString();
					else if (Use.IsValid())
					{
						for (const auto& W : Use->AsObject()->Values) Rule.Weights.Add(TPair<FString, double>(FString(W.Key.ToView()), W.Value->AsNumber()));
						Rule.Weights.Sort([](const TPair<FString, double>& A, const TPair<FString, double>& B) { return A.Key.Compare(B.Key, ESearchCase::CaseSensitive) < 0; });
					}
				}
			}
		}
		return true;
	}

	// world.surfaces(): the colour a surface takes from its pattern's list, and for a wall whether it runs to the eaves
	FBrixSurface Choose(const FBrixCity& City, int32 Pattern, const FString& Id, bool bWall);

	FVector3f Rgb(const TSharedPtr<FJsonValue>& V)
	{
		if (!V.IsValid() || V->Type != EJson::Array) return FVector3f::ZeroVector;
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		return A.Num() >= 3 ? FVector3f((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber()) : FVector3f::ZeroVector;
	}

	// houses.js mapA(): a point of the plan (inner faces of its 0.25 m walls) moved into the built house (0.32 m walls):
	// within each room it keeps its distance to the nearer of the room's two walls
	float MapA(float X, const TArray<float>& O, const TArray<float>& N)
	{
		if (X <= O[0]) return N[0] + X - O[0];
		for (int32 i = 0; i + 1 < O.Num(); ++i)
			if (X <= O[i + 1]) return X - O[i] <= O[i + 1] - X ? N[i] + X - O[i] : N[i + 1] - (O[i + 1] - X);
		return N.Last() + X - O.Last();
	}

	// home_plans.json: each plan's outside, as houses.js builds it (walls WallT deep; only house_12x9 has a front door)
	bool ReadHousePlans(const FString& Path, FBrixCity& City, FString& OutError)
	{
		TSharedPtr<FJsonObject> Root;
		if (!ReadJson(Path, Root, OutError)) return false;
		const TSharedPtr<FJsonObject>* Plans = nullptr;
		if (!Root->TryGetObjectField(TEXT("plans"), Plans)) return true;
		const float WallT = 0.32f;
		for (const auto& KV : (*Plans)->Values)
		{
			const TSharedPtr<FJsonObject>* House = nullptr;
			if (!KV.Value->AsObject()->TryGetObjectField(TEXT("house"), House)) continue;
			const TSharedPtr<FJsonObject>& Hs = *House;
			if (!Hs->HasField(TEXT("front_door")) || !Hs->HasField(TEXT("windows"))) continue;
			FBrixHousePlan P;
			P.Name = FString(KV.Key.ToView());
			P.HalfU = (float)Num(Hs, TEXT("W1"), 6.0);
			P.HalfV = (float)Num(Hs, TEXT("D1"), 4.5);
			const float T = (float)Num(Hs, TEXT("T"), 0.25), XA = (float)Num(Hs, TEXT("XA")), XB = (float)Num(Hs, TEXT("XB")), YM = (float)Num(Hs, TEXT("YM"));
			const float PI0 = P.HalfU - T, PJ0 = P.HalfV - T, SI = P.HalfU - WallT, SJ = P.HalfV - WallT;
			auto Mu = [&](float U) { return MapA(U, { -PI0, XA, XB, PI0 }, { -SI, XA, XB, SI }); };
			auto Mv = [&](float V) { return MapA(V, { -PJ0, YM, PJ0 }, { -SJ, YM, SJ }); };
			const FVector2D Door = Vec2(Hs, TEXT("front_door"));
			P.DoorAt = Mu((float)(Door.X + Door.Y) / 2);
			P.DoorWidth = (float)(Door.Y - Door.X);
			// a one-storey plan lists its windows; a plan of storeys or a flat lists them by floor ({"ground": [...],
			// "upstairs": [...]}): the outside takes the first floor's (reading those as a list gave none and a warning each)
			const TArray<TSharedPtr<FJsonValue>>* Wins = nullptr;
			const TSharedPtr<FJsonObject>* ByFloor = nullptr;
			if (!Hs->TryGetArrayField(TEXT("windows"), Wins) && Hs->TryGetObjectField(TEXT("windows"), ByFloor))
			{
				const TArray<TSharedPtr<FJsonValue>>* Floors = nullptr;
				FString First = TEXT("ground");
				if (Hs->TryGetArrayField(TEXT("floors"), Floors) && Floors->Num()) First = (*Floors)[0]->AsString();
				if (!(*ByFloor)->TryGetArrayField(First, Wins) && (*ByFloor)->Values.Num()) (*ByFloor)->Values.CreateConstIterator().Value()->TryGetArray(Wins);
			}
			static const TArray<TSharedPtr<FJsonValue>> NoWindows;
			for (const TSharedPtr<FJsonValue>& W : Wins ? *Wins : NoWindows)
			{
				const TArray<TSharedPtr<FJsonValue>>& A = W->AsArray();
				FBrixPlanWindow Win;
				Win.Side = A[0]->AsString();
				const float C = (float)A[1]->AsNumber();
				Win.At = (Win.Side == TEXT("front") || Win.Side == TEXT("back")) ? Mu(C) : Mv(C);
				Win.Width = (float)A[2]->AsNumber();
				P.Windows.Add(Win);
			}
			City.HousePlans.Add(P.Name, P);
		}
		return true;
	}

	// houses.js lookS(): a house's line and colourway (house_styles.json) to its colours and trim
	TOptional<FBrixHouseLook> HouseLook(const TSharedPtr<FJsonObject>& Styles, const TSharedPtr<FJsonObject>& House)
	{
		FBrixHouseLook K;
		K.Plan = Str(House, TEXT("plan"));
		K.Line = Str(House, TEXT("furniture"));
		K.Style = Str(House, TEXT("style"));
		const TSharedPtr<FJsonObject> Lines = Styles->GetObjectField(TEXT("lines")), Ways = Styles->GetObjectField(TEXT("colourways"));
		if (!Lines->HasField(K.Line)) K.Line = TEXT("oldtown");
		const TSharedPtr<FJsonObject> Line = Lines->GetObjectField(K.Line), LineWays = Ways->GetObjectField(K.Line);
		const TSharedPtr<FJsonObject> W = LineWays->HasField(K.Style) ? LineWays->GetObjectField(K.Style) : LineWays->GetObjectField(TEXT("a"));
		Line->TryGetBoolField(TEXT("stone"), K.bStone);
		Line->TryGetBoolField(TEXT("quoins"), K.bQuoins);
		Line->TryGetBoolField(TEXT("boxes"), K.bBoxes);
		Line->TryGetBoolField(TEXT("arches"), K.bArches);
		Line->TryGetBoolField(TEXT("hood"), K.bHood);
		const TSharedPtr<FJsonObject> Pal = Styles->GetObjectField(TEXT("palette"));
		const FString Wall = Str(W, TEXT("wall"));
		if (Wall != TEXT("stone")) K.Wall = Rgb(Styles->GetObjectField(TEXT("plasters"))->GetArrayField(Wall)[1]);
		K.Shutter = Rgb(Pal->TryGetField(Str(W, TEXT("shutter"))));
		K.Door = Rgb(Pal->TryGetField(Str(W, TEXT("door"))));
		K.Frame = Rgb(Pal->TryGetField(Str(W, TEXT("frame"))));
		const TArray<TSharedPtr<FJsonValue>>& R = Styles->GetObjectField(TEXT("roofs"))->GetArrayField(Str(W, TEXT("roof")));
		K.Roof = (Rgb(R[0]) + Rgb(R[1])) * 0.5f;
		const TArray<TSharedPtr<FJsonValue>>* Garden = nullptr;
		if (House->TryGetArrayField(TEXT("garden"), Garden) && Garden->Num() == 4)
			K.Garden = FVector4f((float)(*Garden)[0]->AsNumber(), (float)(*Garden)[1]->AsNumber(), (float)(*Garden)[2]->AsNumber(), (float)(*Garden)[3]->AsNumber());
		const TSharedPtr<FJsonObject>* Trim = nullptr;
		bool bBoxes = true;
		if (House->TryGetObjectField(TEXT("trim"), Trim) && (*Trim)->TryGetBoolField(TEXT("window_boxes"), bBoxes) && !bBoxes) K.bBoxes = false;
		return K;
	}
}

int32 FBrixSky::SeasonOf(int32 Day) const
{
	const int32 Year = FMath::Max(1, Seasons.Num() * DaysPerSeason);
	return ((Day % Year + Year) % Year) / FMath::Max(1, DaysPerSeason);
}

double FBrixSky::Declination(int32 Season) const
{
	const double Half = (Sunset[Season] - Sunrise[Season]) / 24.0 * UE_DOUBLE_PI, La = FMath::DegreesToRadians((double)LatitudeDeg);
	return FMath::Atan(-FMath::Cos(Half) / FMath::Tan(La));
}

double FBrixSky::HourAngle(int32 Season, double Minute) const
{
	return (Minute - (Sunrise[Season] + Sunset[Season]) * 30.0) / 1440.0 * UE_DOUBLE_TWO_PI;
}

FVector3d FBrixSky::SunPlace(int32 Season, double H) const
{
	const double La = FMath::DegreesToRadians((double)LatitudeDeg), Dec = Declination(Season);
	const double Cd = FMath::Cos(Dec), Sd = FMath::Sin(Dec), Ch = FMath::Cos(H);
	return FVector3d(-Cd * FMath::Sin(H), Cd * Ch * FMath::Cos(La) + Sd * FMath::Sin(La), Cd * Ch * FMath::Sin(La) - Sd * FMath::Cos(La));
}

double FBrixSky::Daylight(int32 Season, double Minute) const
{
	const double Rise = Sunrise[Season] * 60.0, Set = Sunset[Season] * 60.0;
	const double X = FMath::Clamp(FMath::Min((Minute - (Rise - 30.0)) / 60.0, ((Set + 30.0) - Minute) / 60.0), 0.0, 1.0);
	return X * X * (3.0 - 2.0 * X);                       // smoothstep, as the slice's smooth(0, 1, .)
}

int32 FBrixPattern::TypeIndex() const
{
	static const TCHAR* Types[] = { TEXT("bond"), TEXT("flemish"), TEXT("rubble"), TEXT("flags"), TEXT("herringbone"), TEXT("basket") };
	for (int32 i = 0; i < 6; ++i) if (Type == Types[i]) return i;
	return 0;
}

int32 FBrixCity::FindPattern(const FString& PatternName) const
{
	return Patterns.IndexOfByPredicate([&](const FBrixPattern& P) { return P.Name == PatternName; });
}

double BrixSurfaceHash(const TArray<FString>& Parts)
{
	const FString Hex = FMD5::HashAnsiString(*FString::Join(Parts, TEXT("|")));
	return (double)FParse::HexNumber64(*Hex.Left(8)) / (double)0xFFFFFFFFull;
}

int32 PickBrixSurface(const FBrixCity& City, const FString& Slot, const FBrixBlock& Block, const FBrixLot* Lot)
{
	if (const FString* Own = (Lot ? Lot->OwnSurface : Block.OwnSurface).Find(Slot)) return City.FindPattern(*Own);
	const TArray<FBrixSurfaceRule>* Rules = City.SurfaceRules.Find(Slot);
	if (!Rules) return INDEX_NONE;
	for (const FBrixSurfaceRule& R : *Rules)
	{
		bool bFits = true;
		for (const auto& C : R.If)
		{
			TArray<FString> Got;
			if (C.Key == TEXT("template")) Got.Add(Block.Template);
			else if (C.Key == TEXT("district")) Got.Add(Block.District);
			else if (!Lot) { bFits = false; break; }
			else if (C.Key == TEXT("tags")) Got = Lot->Tags;
			else if (C.Key == TEXT("kind")) Got.Add(Lot->Kind);
			else if (C.Key == TEXT("name")) Got.Add(Lot->Name);
			else if (C.Key == TEXT("id")) Got.Add(Lot->Id);
			if (!Got.ContainsByPredicate([&](const FString& G) { return C.Value.Contains(G); })) { bFits = false; break; }
		}
		if (!bFits) continue;
		if (!R.Use.IsEmpty()) return City.FindPattern(R.Use);
		double Total = 0, Sum = 0;
		for (const auto& W : R.Weights) Sum += W.Value;
		const double Roll = BrixSurfaceHash({ Lot ? Lot->Id : Block.Id, Slot }) * Sum;
		for (const auto& W : R.Weights) { Total += W.Value; if (Roll < Total) return City.FindPattern(W.Key); }
		return R.Weights.Num() ? City.FindPattern(R.Weights.Last().Key) : INDEX_NONE;
	}
	return INDEX_NONE;
}

namespace
{
	FBrixSurface Choose(const FBrixCity& City, int32 Pattern, const FString& Id, bool bWall)
	{
		FBrixSurface S;
		S.Pattern = Pattern;
		if (!City.Patterns.IsValidIndex(Pattern)) return S;
		const FBrixPattern& P = City.Patterns[Pattern];
		if (P.Colors.Num()) S.Color = P.Colors[(int32)(BrixSurfaceHash({ Id, TEXT("colour") }) * P.Colors.Num()) % P.Colors.Num()];
		S.bUpper = bWall && P.Upper > 0.f && BrixSurfaceHash({ Id, TEXT("upper") }) < P.Upper;
		return S;
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

	// the rules sit beside the city, where city.json's "rules" points
	const FString RulesDir = FPaths::Combine(CityDir, Root->HasField(TEXT("rules")) ? Str(Root, TEXT("rules")) : FString(TEXT("../rules")));
	TSharedPtr<FJsonObject> HouseStyles;
	if (!ReadJson(FPaths::Combine(RulesDir, TEXT("house_styles.json")), HouseStyles, OutError)) return false;
	if (!ReadHousePlans(FPaths::Combine(RulesDir, TEXT("home_plans.json")), OutCity, OutError)) return false;
	{
		// the calendar and the sky (ADR-0024)
		TSharedPtr<FJsonObject> Cal, Sky;
		if (!ReadJson(FPaths::Combine(RulesDir, TEXT("calendar.json")), Cal, OutError)) return false;
		if (!ReadJson(FPaths::Combine(RulesDir, TEXT("sky.json")), Sky, OutError)) return false;
		const TSharedPtr<FJsonObject> C = Cal->GetObjectField(TEXT("calendar"));
		FBrixSky& S = OutCity.Sky;
		S.Seasons = Strings(C, TEXT("seasons"));
		for (const TSharedPtr<FJsonValue>& V : C->GetArrayField(TEXT("sunrise"))) S.Sunrise.Add((float)V->AsNumber());
		for (const TSharedPtr<FJsonValue>& V : C->GetArrayField(TEXT("sunset"))) S.Sunset.Add((float)V->AsNumber());
		S.DaysPerSeason = (int32)Num(C, TEXT("days_per_season"), 12);
		S.RealSecondsPerMinute = (float)Num(C, TEXT("real_seconds_per_minute"), 10);
		// the tram: its timetable, and its line as the slice lays it (play.js TRAM_Z, tramAt(): Tram Street, stops a half,
		// three and a half and seven and a half pitches in from the line the town wall stood on, on 60 m beyond the map)
		const TSharedPtr<FJsonObject>* Tr = nullptr;
		if (Cal->TryGetObjectField(TEXT("tram"), Tr))
		{
			FBrixTram& T = OutCity.Tram;
			auto Hm = [](const FString& V) { FString A, B; return V.Split(TEXT(":"), &A, &B) ? FCString::Atod(*A) * 60 + FCString::Atod(*B) : 0.0; };
			T.First = Hm(Str(*Tr, TEXT("first"))); T.Last = Hm(Str(*Tr, TEXT("last")));
			T.Every = Num(*Tr, TEXT("every"), 20); T.Speed = Num(*Tr, TEXT("speed"), 8); T.Dwell = Num(*Tr, TEXT("dwell"), 1);
			T.Pace = S.RealSecondsPerMinute;
			const TSharedPtr<FJsonObject>* Wall = nullptr;
			const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
			double Wx0 = 0, Wx1 = OutCity.Columns;
			if (Root->TryGetObjectField(TEXT("wall"), Wall) && (*Wall)->TryGetArrayField(TEXT("cells"), Cells) && Cells->Num() >= 4) { Wx0 = (*Cells)[0]->AsNumber(); Wx1 = (*Cells)[2]->AsNumber(); }
			const double P = OutCity.Pitch, OX = Wx0 * P, OX1 = Wx1 * P;
			T.Stops = { OX + P / 2, OX + 3 * P + P / 2, OX + 7 * P + P / 2 };
			T.X0 = OX - 60;
			T.X1 = OX1 + 60;
			const TSharedPtr<FJsonObject>* Streets = nullptr;
			TArray<FString> EW;
			if (Root->TryGetObjectField(TEXT("streets"), Streets)) (*Streets)->TryGetStringArrayField(TEXT("east_west"), EW);
			T.Z = FMath::Max(0, EW.IndexOfByKey(FString(TEXT("Tram")))) * P - 1.8;
		}
		const TSharedPtr<FJsonObject> N = Sky->GetObjectField(TEXT("night"));
		S.LatitudeDeg = (float)Num(N, TEXT("latitude"), 40);
		const TSharedPtr<FJsonObject>* Paths = nullptr;
		if (N->TryGetObjectField(TEXT("paths"), Paths)) S.SunLift = (float)Num(*Paths, TEXT("sun_lift"), 0.08);
		if (S.Seasons.Num() == 0 || S.Sunrise.Num() != S.Seasons.Num() || S.Sunset.Num() != S.Seasons.Num())
		{
			OutError = TEXT("calendar.json: seasons, sunrise and sunset do not match");
			return false;
		}
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
		Block.OwnSurface = OwnSurface(B);

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
				Lot.Tags = Strings(L, TEXT("tags"));
				Lot.OwnSurface = OwnSurface(L);
				const TSharedPtr<FJsonObject>* Objs = nullptr;
				if (L->TryGetObjectField(TEXT("objects"), Objs))
					for (const auto& KV : (*Objs)->Values)
					{
						TArray<FVector2D>& At = Lot.Objects.Add(FString(KV.Key));
						const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
						if (KV.Value->TryGetArray(Pts))
							for (const TSharedPtr<FJsonValue>& PV : *Pts)
							{
								const TArray<TSharedPtr<FJsonValue>>* XY = nullptr;
								if (PV->TryGetArray(XY) && XY->Num() >= 2) At.Add(FVector2D((*XY)[0]->AsNumber(), (*XY)[1]->AsNumber()));
							}
					}
				const TSharedPtr<FJsonObject>* House = nullptr;
				if (L->TryGetObjectField(TEXT("house"), House) && HouseStyles.IsValid()) Lot.House = HouseLook(HouseStyles, *House);
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
		const TArray<TSharedPtr<FJsonValue>>* Props = nullptr;
		if (B->TryGetArrayField(TEXT("props"), Props))
		{
			for (const TSharedPtr<FJsonValue>& PV : *Props)
			{
				const TSharedPtr<FJsonObject> P = PV->AsObject();
				if (!P.IsValid()) continue;
				FBrixProp& Prop = Block.Props.AddDefaulted_GetRef();
				Prop.Id = Str(P, TEXT("id"));
				Prop.Kind = Str(P, TEXT("kind"));
				Prop.Set = Str(P, TEXT("set"));
				Prop.Pos = Vec2(P, TEXT("pos"));
				Prop.Out = Vec2(P, TEXT("out"));
				P->TryGetStringField(TEXT("along"), Prop.Along);
				Prop.R = (float)Num(P, TEXT("r"));
				Prop.Height = (float)Num(P, TEXT("height"));
				Prop.Power = (float)Num(P, TEXT("power"));
				double F = 0;
				if (P->TryGetNumberField(TEXT("face"), F)) Prop.Face = (float)F;
				P->TryGetBoolField(TEXT("grand"), Prop.bGrand);
				P->TryGetBoolField(TEXT("big"), Prop.bBig);
				Prop.Color = (int32)Num(P, TEXT("color"));
				Prop.Json = P;
			}
		}
		auto Pt = [](const TSharedPtr<FJsonValue>& V)
		{
			const TArray<TSharedPtr<FJsonValue>>* XY = nullptr;
			return V.IsValid() && V->TryGetArray(XY) && XY->Num() >= 2 ? FVector2D((*XY)[0]->AsNumber(), (*XY)[1]->AsNumber()) : FVector2D::ZeroVector;
		};
		const TArray<TSharedPtr<FJsonValue>>* Paths = nullptr;
		if (B->TryGetArrayField(TEXT("paths"), Paths))
			for (const TSharedPtr<FJsonValue>& PV : *Paths)
			{
				const TArray<TSharedPtr<FJsonValue>>* AB = nullptr;
				if (PV->TryGetArray(AB) && AB->Num() >= 2) Block.Paths.Add({ Pt((*AB)[0]), Pt((*AB)[1]) });
			}
		const TArray<TSharedPtr<FJsonValue>>* Tracks = nullptr;
		if (B->TryGetArrayField(TEXT("tracks"), Tracks))
			for (const TSharedPtr<FJsonValue>& TV : *Tracks)
			{
				const TSharedPtr<FJsonObject> T = TV->AsObject();
				if (!T.IsValid()) continue;
				FBrixTrack& Tr = Block.Tracks.AddDefaulted_GetRef();
				T->TryGetNumberField(TEXT("width"), Tr.Width);
				const TArray<TSharedPtr<FJsonValue>>* Pts = nullptr;
				if (T->TryGetArrayField(TEXT("points"), Pts)) for (const TSharedPtr<FJsonValue>& V : *Pts) Tr.Points.Add(Pt(V));
			}
	}
	// stone, brick and paving (ADR-0017)
	if (!ReadSurfaces(FPaths::Combine(RulesDir, TEXT("surfaces.json")), OutCity, OutError)) return false;
	for (FBrixBlock& B : OutCity.Blocks)
	{
		B.Pavement = Choose(OutCity, PickBrixSurface(OutCity, TEXT("pavement"), B, nullptr), B.Id, false);
		for (FBrixLot& L : B.Lots)
		{
			if (L.Kind == TEXT("garden")) continue;
			const bool bOpen = L.IsOpen();
			L.Surface = Choose(OutCity, PickBrixSurface(OutCity, bOpen ? TEXT("ground") : TEXT("wall"), B, &L), L.Id, !bOpen);
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
			if (L.House.IsSet() && !City.HousePlans.Contains(L.House->Plan == TEXT("samples_19") ? FString(TEXT("house_12x9")) : L.House->Plan))
				Problems.Add(Where + TEXT(": a designed house with a plan the game does not know (") + L.House->Plan + TEXT(")"));
		}
	}
	return Problems;
}

int32 FBrixTram::Standing(double Minute, const FVector2D& P, double Reach, double& OutD) const
{
	const TArray<FAt> Now = At(Minute);
	int32 Best = -1;
	OutD = 1e9;
	for (int32 k = 0; k < Now.Num(); ++k)
	{
		if (!Now[k].bStopped || (Stops.Num() && Now[k].X >= Stops.Last() - .01)) continue;   // at the last stop it goes on out of town
		const double dx = FMath::Max(0.0, FMath::Abs(P.X - Now[k].X) - HalfLength), dz = FMath::Max(0.0, FMath::Abs(P.Y - Z) - HalfWidth), D = FMath::Sqrt(dx * dx + dz * dz);
		if (D <= Reach && D < OutD) { OutD = D; Best = k; }
	}
	return Best;
}

TOptional<FBrixTram::FAt> FBrixTram::Find(double Minute, double Run) const
{
	for (const FAt& A : At(Minute)) if (A.Run == Run) return A;
	return {};
}

FVector2D FBrixTram::Seat(double X, int32 Slot) const
{
	return FVector2D(X - 4.7 + (Slot % 6) * 1.85, Z + (Slot < 6 ? -.55 : .55));
}

FVector2D FBrixTram::Alight(double X) const
{
	return FVector2D(X + HalfLength - 2.4, Z - HalfWidth - .9);
}

bool FBrixTram::Rideable(double Minute, double Run) const
{
	const TOptional<FAt> A = Find(Minute, Run);
	return A && Stops.Num() && (A->X <= Stops.Last() + .01);
}

TArray<FBrixTram::FAt> FBrixTram::At(double m) const
{
	TArray<FAt> Out;
	if (Stops.Num() < 2) return Out;
	const double v = Speed * Pace, ref = Stops[1];    // metres a game minute; the middle stop keeps the timetable
	const int32 K = (int32)FMath::FloorToDouble((m - First) / Every);
	for (int32 k = K - 2; k <= K + 2; ++k)
	{
		const double T = First + k * Every;
		if (T < First || T > Last) continue;
		double t = m - (T - (ref - Stops[0]) / v - Dwell - (Stops[0] - X0) / v), x = X0;   // minutes since this tram came on the map
		for (const double s : Stops)
		{
			const double run = (s - x) / v;
			if (t < run) { x += t * v; t = -1; break; }
			t -= run;
			x = s;
			if (t < Dwell) { t = -2; break; }
			t -= Dwell;
		}
		if (t >= 0) x += t * v;
		if (m - T > -40 && x > X0 && x < X1) Out.Add({ x, t == -2, T });
	}
	return Out;
}
