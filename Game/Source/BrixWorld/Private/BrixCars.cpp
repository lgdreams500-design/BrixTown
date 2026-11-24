#include "BrixCars.h"

#include "BrixDay.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadJsonFile(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = TEXT("Cannot read ") + Path; return false; }
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Out) || !Out.IsValid()) { OutError = Path + TEXT(" is not valid JSON"); return false; }
		return true;
	}
	TOptional<FVector4f> Bay(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 4) return {};
		return FVector4f((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber(), (float)(*A)[3]->AsNumber());
	}
}

bool LoadBrixTraffic(const FString& CarsPath, const FString& ModelsPath, FBrixTraffic& Out, FString& OutError)
{
	Out = FBrixTraffic();
	TSharedPtr<FJsonObject> Root, Models;
	if (!ReadJsonFile(CarsPath, Root, OutError) || !ReadJsonFile(ModelsPath, Models, OutError)) return false;
	Out.Step = (float)Root->GetNumberField(TEXT("step"));
	double Lay = 0;
	Root->TryGetNumberField(TEXT("bus_layover_min"), Lay);
	Out.BusLayover = (float)Lay;
	Root->TryGetStringArrayField(TEXT("lines"), Out.Lines);
	for (const TSharedPtr<FJsonValue>& CV : Root->GetArrayField(TEXT("cars")))
	{
		const TSharedPtr<FJsonObject> C = CV->AsObject();
		FBrixVehicle& V = Out.Vehicles.AddDefaulted_GetRef();
		V.Home = Bay(C->TryGetField(TEXT("home")));
		V.Kind = (int32)C->GetNumberField(TEXT("k"));
		double N = -1;
		if (C->TryGetNumberField(TEXT("l"), N)) V.Line = (int32)N;
		if (C->TryGetNumberField(TEXT("o"), N)) V.Owner = (int32)N;
		// a trip: [t0, t1, bay1 or null, [x, z, hx, hz, ...], (stowed [from, to])]
		for (const TSharedPtr<FJsonValue>& TV : C->GetArrayField(TEXT("trips")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = TV->AsArray();
			FBrixVehicleTrip& T = V.Trips.AddDefaulted_GetRef();
			T.T0 = (float)A[0]->AsNumber();
			T.T1 = (float)A[1]->AsNumber();
			T.Bay1 = Bay(A[2]);
			const TArray<TSharedPtr<FJsonValue>>& P = A[3]->AsArray();
			for (int32 j = 0; j + 3 < P.Num(); j += 4)
				T.Pts.Add(FVector4f((float)P[j]->AsNumber(), (float)P[j + 1]->AsNumber(), (float)P[j + 2]->AsNumber(), (float)P[j + 3]->AsNumber()));
			if (A.Num() > 4) { const TArray<TSharedPtr<FJsonValue>>& S = A[4]->AsArray(); T.Stowed = FVector2f((float)S[0]->AsNumber(), (float)S[1]->AsNumber()); }
		}
	}
	Models->TryGetStringArrayField(TEXT("parts"), Out.Parts);
	for (const auto& KV : Models->GetObjectField(TEXT("models"))->Values)
	{
		const TSharedPtr<FJsonObject> M = KV.Value->AsObject();
		FBrixCarModel& Md = Out.Models.Add(FString(KV.Key));
		Md.Name = FString(KV.Key);
		const TArray<TSharedPtr<FJsonValue>>& Seat = M->GetArrayField(TEXT("seat"));
		Md.Seat = FVector2f((float)Seat[0]->AsNumber(), (float)Seat[1]->AsNumber());
		Md.SeatY = (float)M->GetNumberField(TEXT("seat_y"));
		double B = 0;
		if (M->TryGetNumberField(TEXT("back"), B)) Md.Back = (float)B;
		const TArray<TSharedPtr<FJsonValue>>& Bx = M->GetArrayField(TEXT("box"));
		Md.Box = FVector4f((float)Bx[0]->AsNumber(), (float)Bx[1]->AsNumber(), (float)Bx[2]->AsNumber(), (float)Bx[3]->AsNumber());
		for (const TSharedPtr<FJsonValue>& LV : M->GetArrayField(TEXT("lods")))
		{
			const TSharedPtr<FJsonObject> L = LV->AsObject();
			FBrixCarModel::FLod& Lod = Md.Lods.AddDefaulted_GetRef();
			const TArray<TSharedPtr<FJsonValue>>& P = L->GetArrayField(TEXT("p"));
			const TArray<TSharedPtr<FJsonValue>>& Nn = L->GetArrayField(TEXT("n"));
			for (int32 j = 0; j + 2 < P.Num(); j += 3)
			{
				Lod.P.Add(FVector3f((float)P[j]->AsNumber(), (float)P[j + 1]->AsNumber(), (float)P[j + 2]->AsNumber()));
				Lod.N.Add(FVector3f((float)Nn[j]->AsNumber(), (float)Nn[j + 1]->AsNumber(), (float)Nn[j + 2]->AsNumber()) / 100.f);   // stored in hundredths
			}
			for (const TSharedPtr<FJsonValue>& T : L->GetArrayField(TEXT("t"))) Lod.Part.Add((uint8)T->AsNumber());
		}
	}
	if (!Out.Vehicles.Num() || !Out.Models.Num()) { OutError = TEXT("no vehicles or no car models"); return false; }
	return true;
}

TOptional<FBrixCarAt> FBrixTraffic::At(int32 k, double m) const
{
	if (!Vehicles.IsValidIndex(k)) return {};
	const FBrixVehicle& C = Vehicles[k];
	auto Parked = [](const FVector4f& B) { FBrixCarAt A; A.Pos = FVector2D(B.X, B.Y); A.Heading = FVector2D(B.Z, B.W); return A; };
	const bool bBus = C.Kind == 2;
	// a bus waits at its first stop before its first run, and leaves service after its last (traffic.py car_at())
	if (bBus && C.Trips.Num() && m < C.Trips[0].T0)
	{
		if (m >= C.Trips[0].T0 - BusLayover && C.Trips[0].Pts.Num()) return Parked(C.Trips[0].Pts[0]);
		if (!C.Home) return {};
	}
	TOptional<FVector4f> BayNow = C.Home;
	for (int32 i = 0; i < C.Trips.Num(); ++i)   // the slice's at()
	{
		const FBrixVehicleTrip& T = C.Trips[i];
		if (m < T.T0) break;
		if (m < T.T1)
		{
			const TArray<FVector4f>& P = T.Pts;
			const int32 n = P.Num();
			if (n == 0) return {};
			if (n < 2) { FBrixCarAt A = Parked(P[0]); A.bMoving = true; return A; }
			const double f = FMath::Min((double)(n - 1), FMath::Max(0.0, (m - T.T0) / Step));
			const int32 j = FMath::Min(n - 2, (int32)FMath::FloorToDouble(f));
			const float g = (float)(f - j);
			const FVector4f& a = P[j]; const FVector4f& b = P[j + 1];
			FBrixCarAt A;
			A.Pos = FVector2D(FMath::Lerp(a.X, b.X, g), FMath::Lerp(a.Y, b.Y, g));
			const FVector2D H(FMath::Lerp(a.Z, b.Z, g), FMath::Lerp(a.W, b.W, g));
			A.Heading = H.IsNearlyZero() ? FVector2D(1, 0) : H.GetSafeNormal();
			A.bMoving = FVector2D(b.X - a.X, b.Y - a.Y).Size() > .05;
			return A;
		}
		BayNow = T.Bay1;
		if (T.Stowed.X >= 0 && m >= T.Stowed.X && m < T.Stowed.Y) return {};   // parked off the street (CITY-093)
		if (bBus && i == C.Trips.Num() - 1 && m >= T.T1 + BusLayover) return {};   // the day's last run done
	}
	if (!BayNow) return {};
	return Parked(*BayNow);
}

const FBrixCarModel* FBrixTraffic::ModelOf(int32 k) const
{
	if (!Vehicles.IsValidIndex(k) || Vehicles[k].Kind == 2) return nullptr;
	if (Vehicles[k].Kind == 1) return Models.Find(TEXT("van"));
	const double h = BrixSliceHash({ FString::FromInt(k), TEXT("model") });
	return Models.Find(h < .4 ? TEXT("small") : h < .72 ? TEXT("estate") : TEXT("pickup"));
}
