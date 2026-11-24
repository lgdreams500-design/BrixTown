#include "BrixDay.h"

#include "BrixCity.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

double BrixSliceHash(std::initializer_list<FString> Parts)
{
	uint32 h = 2166136261u;
	for (const FString& S : Parts)
	{
		for (TCHAR Ch : S) { h ^= (uint32)Ch; h *= 16777619u; }
		h ^= 124u; h *= 16777619u;
	}
	h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
	return (double)h / 4294967296.0;
}

bool LoadBrixDay(const FString& Path, FBrixDay& Out, FString& OutError)
{
	Out = FBrixDay();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
	Out.Day = (int32)Root->GetNumberField(TEXT("day"));
	Root->TryGetStringField(TEXT("season"), Out.Season);
	Root->TryGetStringField(TEXT("weekday"), Out.Weekday);
	Root->TryGetStringArrayField(TEXT("acts"), Out.Acts);
	Root->TryGetStringArrayField(TEXT("roles"), Out.Roles);
	for (const TSharedPtr<FJsonValue>& R : Root->GetArrayField(TEXT("residents")))
	{
		const TSharedPtr<FJsonObject> O = R->AsObject();
		Out.Ids.Add(O->GetStringField(TEXT("id")));
		Out.Names.Add(O->GetStringField(TEXT("n")));
		Out.Role.Add((int32)O->GetNumberField(TEXT("r")));
		FString U;
		O->TryGetStringField(TEXT("u"), U);
		Out.Wear.Add(U);
	}
	const TSharedPtr<FJsonObject>* Sigs;
	if (Root->TryGetObjectField(TEXT("role_sig"), Sigs))
		for (const auto& KV : (*Sigs)->Values) Out.RoleSig.Add(FString(KV.Key), KV.Value->AsString());
	// the pieces, as build_play.py routines() writes them: [t0, 0, x, z, act, use, inside, lot(, floor)] a stay,
	// [t0, 1, act, x, z, x, z, ...] a walk, [t0, 2] away, [t0, 3, vehicle] in a car or on a bus
	for (const TSharedPtr<FJsonValue>& RowV : Root->GetArrayField(TEXT("plan")))
	{
		TArray<FBrixDay::FPiece>& Row = Out.Plan.AddDefaulted_GetRef();
		for (const TSharedPtr<FJsonValue>& PV : RowV->AsArray())
		{
			const TArray<TSharedPtr<FJsonValue>>& A = PV->AsArray();
			FBrixDay::FPiece& P = Row.AddDefaulted_GetRef();
			P.T0 = (float)A[0]->AsNumber();
			P.Kind = (uint8)A[1]->AsNumber();
			if (P.Kind == 0)
			{
				P.Pos = FVector2D(A[2]->AsNumber(), A[3]->AsNumber());
				P.Act = (int32)A[4]->AsNumber();
				P.Use = (int32)A[5]->AsNumber();
				P.bInside = A[6]->AsNumber() != 0;
				P.Lot = (int32)A[7]->AsNumber();
				P.Floor = A.Num() > 8 ? (int32)A[8]->AsNumber() : 0;
				const double Turn = BrixSliceHash({ FString::FromInt(Out.Plan.Num() - 1), FString::FromInt(Row.Num() - 1) }) * 6.28;   // the slice: hash(i, k) * 6.28
				P.Facing = FVector2D(FMath::Cos(Turn), FMath::Sin(Turn));
			}
			else if (P.Kind == 3 && A.Num() > 2) P.Vehicle = (int32)A[2]->AsNumber();
			else if (P.Kind == 1)
			{
				P.Act = (int32)A[2]->AsNumber();
				for (int32 j = 3; j + 1 < A.Num(); j += 2) P.Route.Add(FVector2D(A[j]->AsNumber(), A[j + 1]->AsNumber()));
				P.Cum.Add(0.f);
				for (int32 j = 1; j < P.Route.Num(); ++j) P.Cum.Add(P.Cum.Last() + (float)FVector2D::Distance(P.Route[j], P.Route[j - 1]));
			}
		}
	}
	return Out.Plan.Num() > 0;
}

namespace
{
	FVector2D AlongRoute(const FBrixDay::FPiece& P, float D)   // the slice's along()
	{
		int32 k = 1;
		while (k < P.Cum.Num() - 1 && P.Cum[k] < D) ++k;
		const float F = P.Cum[k] > P.Cum[k - 1] ? (D - P.Cum[k - 1]) / (P.Cum[k] - P.Cum[k - 1]) : 0.f;
		return FMath::Lerp(P.Route[k - 1], P.Route[k], FMath::Clamp(F, 0.f, 1.f));
	}
}

int32 FBrixDay::VehicleAt(int32 Resident, double Minute) const
{
	if (!Plan.IsValidIndex(Resident) || Plan[Resident].Num() == 0) return -1;
	const TArray<FPiece>& Row = Plan[Resident];
	int32 Lo = 0, Hi = Row.Num() - 1;
	while (Lo < Hi) { const int32 Mid = (Lo + Hi + 1) >> 1; if (Row[Mid].T0 <= Minute) Lo = Mid; else Hi = Mid - 1; }
	return Row[Lo].Kind == 3 ? Row[Lo].Vehicle : -1;
}

TOptional<FBrixWhere> FBrixDay::Where(int32 Resident, double Minute) const
{
	if (!Plan.IsValidIndex(Resident) || Plan[Resident].Num() == 0) return {};
	const TArray<FPiece>& Row = Plan[Resident];
	int32 Lo = 0, Hi = Row.Num() - 1;                        // the slice's pieceAt(): the last piece begun by this minute
	while (Lo < Hi) { const int32 Mid = (Lo + Hi + 1) >> 1; if (Row[Mid].T0 <= Minute) Lo = Mid; else Hi = Mid - 1; }
	const FPiece& P = Row[Lo];
	if (P.Kind == 2 || P.Kind == 3) return {};             // away, or in their car or on a bus (drawn in the vehicle)
	FBrixWhere W;
	W.Act = P.Act;
	if (P.Kind == 0)
	{
		W.Pos = P.Pos;
		W.bInside = P.bInside;
		W.Floor = P.Floor;
		W.Heading = P.Facing;
		W.Pose = P.Pose;
		W.SeatUp = P.SeatUp;
		return W;
	}
	if (P.Route.Num() < 2) { W.Pos = P.Route.Num() ? P.Route[0] : FVector2D::ZeroVector; return W; }
	const double T1 = Lo + 1 < Row.Num() ? Row[Lo + 1].T0 : 1440.0, L = P.Cum.Last();
	const float D = (float)(FMath::Clamp((Minute - P.T0) / FMath::Max(T1 - P.T0, 1e-6), 0.0, 1.0) * L);
	W.Pos = AlongRoute(P, D);
	const FVector2D Ahead = AlongRoute(P, FMath::Min((float)L, D + 2.f)), Behind = AlongRoute(P, FMath::Max(0.f, D - 2.f));
	const FVector2D Way = Ahead - Behind;
	W.Heading = Way.IsNearlyZero() ? FVector2D(1, 0) : Way.GetSafeNormal();
	W.bWalking = true;
	W.Pose = 1;
	W.Along = D;
	W.WalkDone = D - (P.Cum.Num() > 1 ? P.Cum[1] : 0.f);
	W.WalkLeft = (P.Cum.Num() > 1 ? P.Cum[P.Cum.Num() - 2] : (float)L) - D;
	return W;
}

int32 SeatBrixDay(FBrixDay& Day, const FBrixCity& City)
{
	// the slice's heights (play.js Y_YARD) and its facing: yaw 0 faces north, (sin yaw, -cos yaw)
	constexpr float YardUp = 0.16f;
	TArray<const FBrixLot*> Lots;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) Lots.Add(&L);
	struct FSpot { FVector2D At; uint8 Pose; FVector2D Face; float Up; };
	TArray<TArray<FSpot>> Spots;
	Spots.SetNum(Lots.Num());
	for (int32 i = 0; i < Lots.Num(); ++i)
	{
		const FBrixLot& L = *Lots[i];
		const FVector2D C = L.Centre();
		auto Of = [&](const TCHAR* K) { const TArray<FVector2D>* A = L.Objects.Find(K); return A ? *A : TArray<FVector2D>(); };
		if (L.Kind == TEXT("garden"))
		{
			for (const FVector2D& P : Of(TEXT("bench")))   // facing the garden's middle along the way the bench is further out
				Spots[i].Add({ P, 2, FMath::Abs(P.Y - C.Y) > FMath::Abs(P.X - C.X) ? FVector2D(0, FMath::Sign(C.Y - P.Y)) : FVector2D(FMath::Sign(C.X - P.X), 0), YardUp + .46f });
			for (const FVector2D& P : Of(TEXT("rim")))     // on a fountain's rim, facing out
				Spots[i].Add({ P, 2, (P - C).GetSafeNormal(), YardUp + .5f });
			for (const FVector2D& P : Of(TEXT("flowerbed"))) Spots[i].Add({ P, 0, FVector2D(0, -1), -1.f });
		}
		else if (L.Kind == TEXT("market"))
			for (const FVector2D& P : Of(TEXT("stall"))) Spots[i].Add({ P, 0, FVector2D(0, P.Y < C.Y ? 1 : -1), -1.f });
	}
	int32 Seated = 0;
	for (TArray<FBrixDay::FPiece>& Row : Day.Plan)
		for (FBrixDay::FPiece& P : Row)
		{
			if (P.Kind != 0 || P.bInside || P.Use < 0 || !Spots.IsValidIndex(P.Lot)) continue;
			for (const FSpot& S : Spots[P.Lot])
				if (FMath::Abs(S.At.X - P.Pos.X) < .2 && FMath::Abs(S.At.Y - P.Pos.Y) < .2)
				{
					P.Pose = S.Pose;
					P.SeatUp = S.Up;
					if (!S.Face.IsNearlyZero()) P.Facing = S.Face;
					if (S.Pose == 2) ++Seated;
					break;
				}
		}
	return Seated;
}
