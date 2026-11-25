#include "BrixInside.h"

#include "BrixCity.h"
#include "Dom/JsonObject.h"
#include "Misc/ConfigCacheIni.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	FVector2D V2(const TArray<TSharedPtr<FJsonValue>>& A, int32 At = 0)
	{
		return A.Num() >= At + 2 ? FVector2D(A[At]->AsNumber(), A[At + 1]->AsNumber()) : FVector2D::ZeroVector;
	}
	FVector3f Rgb(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 3 ? FVector3f((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber()) : FVector3f(0.75f);
	}
	TOptional<FBrixPlanFrame> FrameOf(const TSharedPtr<FJsonObject>& O)
	{
		const TSharedPtr<FJsonObject>* F = nullptr;
		if (!O->TryGetObjectField(TEXT("interior_frame"), F)) return {};
		FBrixPlanFrame Fr;
		Fr.O = V2((*F)->GetArrayField(TEXT("o")));
		Fr.X = V2((*F)->GetArrayField(TEXT("x")));
		Fr.Y = V2((*F)->GetArrayField(TEXT("y")));
		for (const TCHAR* K : { TEXT("u"), TEXT("v") })
			for (const TSharedPtr<FJsonValue>& Row : (*F)->GetArrayField(K))
			{
				TArray<float>& Out = (FCString::Strcmp(K, TEXT("u")) == 0 ? Fr.U : Fr.V).AddDefaulted_GetRef();
				for (const TSharedPtr<FJsonValue>& X : Row->AsArray()) Out.Add((float)X->AsNumber());
			}
		return Fr;
	}
}

bool LoadBrixInside(const FString& Path, FBrixInsides& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("cannot read ") + Path + TEXT(" (run Tools/worldgen/build_play.py)"); return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root) { OutError = Path + TEXT(" is not JSON"); return false; }

	const TSharedPtr<FJsonObject>* St = nullptr;
	if (Root->TryGetObjectField(TEXT("storeys"), St))
	{
		FBrixStoreyRules& R = Out.Rules;
		R.Storey = (float)(*St)->GetNumberField(TEXT("storey"));
		R.Slab = (float)(*St)->GetNumberField(TEXT("slab"));
		R.Wall = (float)(*St)->GetNumberField(TEXT("wall"));
		R.LiftFromFloors = (int32)(*St)->GetNumberField(TEXT("lift_from_floors"));
		const TSharedPtr<FJsonObject>& S = (*St)->GetObjectField(TEXT("stair"));
		R.StairWidth = (float)S->GetNumberField(TEXT("width"));
		R.Spine = (float)S->GetNumberField(TEXT("spine"));
		R.Landing = (float)S->GetNumberField(TEXT("landing"));
		R.Run = (float)S->GetNumberField(TEXT("run"));
		R.KeepClear = (float)S->GetNumberField(TEXT("keep_clear"));
	}

	LoadBrixHomeData(Root, Out);
	Out.Lots.Reset();
	for (const TSharedPtr<FJsonValue>& V : Root->GetArrayField(TEXT("lots")))
	{
		TOptional<FBrixInside>& Slot = Out.Lots.AddDefaulted_GetRef();
		if (!V.IsValid() || V->IsNull()) continue;
		const TSharedPtr<FJsonObject> O = V->AsObject();
		FBrixInside In;
		In.Id = O->GetStringField(TEXT("id"));
		O->TryGetStringField(TEXT("special"), In.Special);
		In.DoorWidth = (float)O->GetNumberField(TEXT("door_width"));
		for (const TSharedPtr<FJsonValue>& F : O->GetArrayField(TEXT("floors")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = F->AsArray();
			In.FloorFinish.Add(A.Num() ? (int32)A[0]->AsNumber() : 0);
			In.FloorColour.Add(A.Num() > 1 ? Rgb(A[1]->AsArray()) : FVector3f(0.5f));
		}
		for (const TSharedPtr<FJsonValue>& W : O->GetArrayField(TEXT("walls"))) In.WallColour.Add(Rgb(W->AsArray()));
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (O->TryGetArrayField(TEXT("storeys"), Arr)) for (const TSharedPtr<FJsonValue>& U : *Arr) In.Uses.Add(U->AsString());
		const TSharedPtr<FJsonObject>* S = nullptr;
		if (O->TryGetObjectField(TEXT("stair"), S))
		{
			FBrixStair G;
			const TArray<TSharedPtr<FJsonValue>>& R = (*S)->GetArrayField(TEXT("rect"));
			G.Min = V2(R, 0); G.Max = V2(R, 2);
			G.bAlongX = (*S)->GetStringField(TEXT("axis")) == TEXT("x");
			G.Wall = (*S)->GetStringField(TEXT("wall"));
			G.Up0 = (int32)(*S)->GetNumberField(TEXT("up0"));
			FString Kind;
			G.bStraight = (*S)->TryGetStringField(TEXT("kind"), Kind) && Kind == TEXT("straight");
			double W = 0; if ((*S)->TryGetNumberField(TEXT("width"), W)) G.Width = (float)W;
			In.Stair = G;
		}
		if (O->TryGetArrayField(TEXT("lift"), Arr)) In.Lift = V2(*Arr);
		O->TryGetBoolField(TEXT("tower"), In.bTower);
		const TSharedPtr<FJsonObject>* Lv = nullptr;
		if (O->TryGetObjectField(TEXT("levels"), Lv))
			for (const auto& KV : (*Lv)->Values)
			{
				TArray<int32>& L = In.Levels.Add(FString(KV.Key));
				for (const TSharedPtr<FJsonValue>& X : KV.Value->AsArray()) L.Add((int32)X->AsNumber());
			}
		if (O->TryGetArrayField(TEXT("flats"), Arr))
			for (const TSharedPtr<FJsonValue>& Fv : *Arr)
			{
				const TSharedPtr<FJsonObject> F = Fv->AsObject();
				FBrixFlat Fl;
				Fl.Id = F->GetStringField(TEXT("id"));
				Fl.Name = F->GetStringField(TEXT("name"));
				Fl.Floor = (int32)F->GetNumberField(TEXT("floor"));
				const TArray<TSharedPtr<FJsonValue>>& R = F->GetArrayField(TEXT("rect"));
				Fl.Min = V2(R, 0); Fl.Max = V2(R, 2);
				const TArray<TSharedPtr<FJsonValue>>* D = nullptr;
				if (F->TryGetArrayField(TEXT("door"), D)) Fl.Door = V2(*D);
				if (F->TryGetArrayField(TEXT("household"), D)) for (const TSharedPtr<FJsonValue>& H : *D) Fl.Household.Add(H->AsString());
				F->TryGetStringField(TEXT("interior"), Fl.Interior);
				F->TryGetStringField(TEXT("furniture"), Fl.Line);
				Fl.Frame = FrameOf(F);
				In.Flats.Add(MoveTemp(Fl));
			}
		O->TryGetStringField(TEXT("interior"), In.Interior);
		O->TryGetStringField(TEXT("furniture"), In.Line);
		In.Frame = FrameOf(O);
		double Res = 0; if (O->TryGetNumberField(TEXT("residents"), Res)) In.Residents = (int32)Res;
		double Wage = 0; if (O->TryGetNumberField(TEXT("wage"), Wage)) In.Wage = (int64)Wage;
		O->TryGetStringField(TEXT("house_line"), In.HouseLine);
		const TSharedPtr<FJsonObject>* Ob = nullptr;
		if (O->TryGetObjectField(TEXT("objects"), Ob))
			for (const auto& KV : (*Ob)->Values)
			{
				TArray<FVector2D>& Pts = In.Objects.Add(FString(KV.Key));
				for (const TSharedPtr<FJsonValue>& P : KV.Value->AsArray()) Pts.Add(V2(P->AsArray()));
			}
		Slot = MoveTemp(In);
	}
	return true;
}

FBrixStairGeo BrixStairOf(const FBrixStair& S, const FBrixStoreyRules& R)
{
	FBrixStairGeo G;
	G.bAlongX = S.bAlongX;
	G.bStraight = S.bStraight;
	G.Up0 = S.Up0;
	G.U0 = S.bAlongX ? S.Min.X : S.Min.Y; G.U1 = S.bAlongX ? S.Max.X : S.Max.Y;
	G.V0 = S.bAlongX ? S.Min.Y : S.Min.X; G.V1 = S.bAlongX ? S.Max.Y : S.Max.X;
	const bool bWallLow = S.bAlongX ? S.Wall == TEXT("n") : S.Wall == TEXT("w");   // lane 0 (against the wall) at the low v end
	const float W = S.Width > 0.f ? S.Width : R.StairWidth, Sp = R.Spine;
	G.Lane0 = bWallLow ? FVector2f(G.V0, G.V0 + W) : FVector2f(G.V1 - W, G.V1);
	G.Lane1 = bWallLow ? FVector2f(G.V0 + W + Sp, G.V1) : FVector2f(G.V0, G.V1 - W - Sp);
	G.Spine = bWallLow ? FVector2f(G.V0 + W, G.V0 + W + Sp) : FVector2f(G.V1 - W - Sp, G.V1 - W);
	G.Open = bWallLow ? G.Lane0.Y : G.Lane0.X;
	G.ULo = G.U0 + R.Landing;
	G.UHi = G.U1 - R.Landing;
	return G;
}

FBrixStairGeo::FFlight FBrixStairGeo::Flight(int32 Floor) const
{
	FFlight F;
	const bool bOdd = !bStraight && (Floor % 2) != 0;
	F.Lane = bOdd ? Lane1 : Lane0;
	F.Dir = (bOdd ? -1 : 1) * Up0;
	F.Ub = F.Dir > 0 ? ULo : UHi;
	F.Ut = F.Dir > 0 ? UHi : ULo;
	return F;
}

FVector2D FBrixStairGeo::FootAt(int32 Floor) const
{
	const FFlight F = Flight(Floor);
	return P(F.Ub - F.Dir * 0.6f, (F.Lane.X + F.Lane.Y) / 2);
}

TArray<FVector4f> FBrixStairGeo::GroundSolids() const
{
	const FFlight F = Flight(0);
	TArray<FVector4f> Out = { FVector4f(ULo, UHi, F.Lane.X, F.Lane.Y) };
	if (!bStraight) Out.Add(FVector4f(ULo, UHi, Spine.X, Spine.Y));
	return Out;
}

FBrixDoorRules FBrixDoorRules::FromConfig()
{
	FBrixDoorRules R;
	if (!GConfig) return R;
	const TCHAR* Sec = TEXT("/Script/BrixTown.BrixTownActor");
	auto F = [&](const TCHAR* Key, float& V) { GConfig->GetFloat(Sec, Key, V, GGameIni); };
	F(TEXT("WallThickness"), R.Wall);
	F(TEXT("Bay"), R.Bay);
	F(TEXT("MinFacadeForOpenings"), R.MinFacade);
	F(TEXT("ShopPane"), R.ShopPane);
	F(TEXT("ShopPier"), R.ShopPier);
	F(TEXT("HousePlinth"), R.HousePlinth);
	return R;
}

bool BrixIsHollow(const FBrixLot& Lot, const FBrixInside* Inside)
{
	if (!Lot.bEnterable || Lot.IsOpen() || Lot.Kind == TEXT("furniture")) return false;
	return !(Inside && Inside->Special == TEXT("mall"));
}

FBrixDoorway BrixDoorwayOf(const FBrixCity& City, const FBrixLot& L, const FString& District, const FBrixDoorRules& R)
{
	FBrixDoorway Dw;
	const float t = R.Wall;
	const FBrixHouseLook* Hs = L.House.IsSet() ? &L.House.GetValue() : nullptr;
	const FBrixHousePlan* Plan = Hs ? City.HousePlans.Find(Hs->Plan == TEXT("samples_19") ? FString(TEXT("house_12x9")) : Hs->Plan) : nullptr;
	const FVector2D C = L.Centre(), S = Plan ? FVector2D(2 * Plan->HalfU, 2 * Plan->HalfV) : L.Size();
	const double Ang = FMath::DegreesToRadians(L.AngleDeg), Ca = FMath::Cos(Ang), Sa = FMath::Sin(Ang);
	auto World = [&](const FVector2D& P) { return FVector2D(C.X + P.X * Ca - P.Y * Sa, C.Y + P.X * Sa + P.Y * Ca); };
	const FVector2D DL((L.Door.X - C.X) * Ca + (L.Door.Y - C.Y) * Sa, -(L.Door.X - C.X) * Sa + (L.Door.Y - C.Y) * Ca);
	struct FFacade { FVector2D Mid, Normal; float Len; };
	const FFacade Facades[4] = {
		{ FVector2D(0, -S.Y / 2), FVector2D(0, -1), (float)S.X }, { FVector2D(0, S.Y / 2), FVector2D(0, 1), (float)S.X },
		{ FVector2D(-S.X / 2, 0), FVector2D(-1, 0), (float)S.Y - 2 * t }, { FVector2D(S.X / 2, 0), FVector2D(1, 0), (float)S.Y - 2 * t } };
	int32 Side = 0;
	if (!Plan)
	{
		const double Gap[4] = { FMath::Abs(DL.Y + S.Y / 2), FMath::Abs(DL.Y - S.Y / 2), FMath::Abs(DL.X + S.X / 2), FMath::Abs(DL.X - S.X / 2) };
		for (int32 k = 1; k < 4; ++k) if (Gap[k] < Gap[Side]) Side = k;
	}
	const FFacade& F = Facades[Side];
	const double Yaw = FMath::Atan2(-F.Normal.X, F.Normal.Y);
	const FVector2D Along(FMath::Cos(Yaw), FMath::Sin(Yaw));
	Dw.Side = Side;
	if (Plan)
	{
		Dw.Kind = FBrixDoorway::EKind::House;
		Dw.bArchHouse = Hs->bArches;
		Dw.Along = (float)FVector2D::DotProduct(FVector2D(Plan->DoorAt, F.Mid.Y) - F.Mid, Along);
		Dw.Width = Plan->DoorWidth;
		Dw.Height = 2.4f;
	}
	else
	{
		const bool bOld = !R.OutskirtDistricts.Contains(District);
		const bool bCivic = L.Kind == TEXT("health") || L.Kind == TEXT("work");
		const float Sd = (float)FVector2D::DotProduct(DL - F.Mid, Along);
		if (L.Kind == TEXT("shop") && L.Floors >= 2 && F.Len >= 4.f)
		{
			// the shopfront's pane nearest the door (BrixTownActor: piers every ShopPane metres across the front)
			const float Wd = F.Len, X0 = -F.Len / 2;
			const int32 N = FMath::Max(3, FMath::RoundToInt((Wd - 0.8f) / R.ShopPane));
			float Best = 1e9f;
			for (int32 i = 0; i < N; ++i)
			{
				const float A = X0 + 0.4f + i * (Wd - 0.8f) / N + R.ShopPier / 2, B2 = X0 + 0.4f + (i + 1) * (Wd - 0.8f) / N - R.ShopPier / 2;
				if (FMath::Abs((A + B2) / 2 - Sd) < Best) { Best = FMath::Abs((A + B2) / 2 - Sd); Dw.Along = (A + B2) / 2; Dw.Width = B2 - A; }
			}
			Dw.Kind = FBrixDoorway::EKind::Shop;
			Dw.Height = R.ShopDoorHeight;
		}
		else
		{
			Dw.Kind = bCivic ? FBrixDoorway::EKind::Civic : bOld ? FBrixDoorway::EKind::Arch : FBrixDoorway::EKind::Plain;
			Dw.Width = bCivic ? R.DoorCivic : R.DoorPlain;
			Dw.Height = bCivic ? R.CivicHeight : bOld ? R.ArchHeight : R.PlainHeight;
			const float Room = FMath::Max(0.f, F.Len / 2 - Dw.Width / 2 - 0.25f);
			Dw.Along = FMath::Clamp(Sd, -Room, Room);
		}
	}
	Dw.Centre = World(F.Mid + Along * Dw.Along);
	Dw.Out = FVector2D(F.Normal.X * Ca - F.Normal.Y * Sa, F.Normal.X * Sa + F.Normal.Y * Ca);
	return Dw;
}
