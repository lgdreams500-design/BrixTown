// CITY-040: the day's vehicles in the Unreal build are where Tools/citysim/traffic.py puts them.
#include "BrixCars.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// Every 7th vehicle of the day at eight minutes (Tests/values/cars_city_01.json, from traffic.py car_at()): in the town
// when traffic.py has it there and not when it is away, out of service or parked off the street; parked within 0.1 m of
// its bay (the data's rounding); on the move within 4 m: build_play.py eases a vehicle sideways between its bay and the
// lane as it pulls in or out, where traffic.py keeps it on the lane, and a bus stop's lay-by lies some 3.5 m off the lane
// (the trip is otherwise sampled every second of the day). A wrong trip or bay is tens of metres out.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY040_VehiclesFollowTheTraffic, "BrixTown.World.CITY040_vehicles_are_where_the_traffic_puts_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY040_VehiclesFollowTheTraffic::RunTest(const FString&)
{
	FBrixTraffic Traffic;
	FString Error, Text;
	const FString Data = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data")));
	if (!TestTrue(TEXT("the vehicles load: ") + Error, LoadBrixTraffic(FPaths::Combine(Data, TEXT("cars.json")), FPaths::Combine(Data, TEXT("car_models.json")), Traffic, Error))) return false;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/cars_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	int32 Checked = 0, Wrong = 0;
	double Worst = 0;
	for (const auto& MV : Want->GetObjectField(TEXT("at"))->Values)
	{
		const double Minute = FCString::Atod(*FString(MV.Key.ToView()));
		for (const auto& CV : MV.Value->AsObject()->Values)
		{
			const int32 k = FCString::Atoi(*FString(CV.Key.ToView()));
			const TSharedPtr<FJsonObject> W = CV.Value->AsObject();
			const FString State = W->GetStringField(TEXT("state"));
			const TArray<TSharedPtr<FJsonValue>>* Pos = nullptr;
			const bool bThere = W->TryGetArrayField(TEXT("pos"), Pos) && Pos->Num() == 2;
			const TOptional<FBrixCarAt> Got = Traffic.At(k, Minute);
			++Checked;
			if (bThere != Got.IsSet())
			{
				AddError(FString::Printf(TEXT("vehicle %d at minute %.1f: %s, traffic.py says %s"), k, Minute, Got.IsSet() ? TEXT("in the town") : TEXT("not in the town"), *State));
				++Wrong;
				continue;
			}
			if (!bThere) continue;
			const double Off = FVector2D::Distance(Got->Pos, FVector2D((*Pos)[0]->AsNumber(), (*Pos)[1]->AsNumber()));
			const bool bParked = State == TEXT("parked");
			Worst = FMath::Max(Worst, bParked ? 0.0 : Off);
			if (Off > (bParked ? 0.1 : 4.0))
			{
				AddError(FString::Printf(TEXT("vehicle %d at minute %.1f (%s): %.2f m from where traffic.py puts it"), k, Minute, *State, Off));
				++Wrong;
			}
		}
	}
	AddInfo(FString::Printf(TEXT("%d checked; on the move at most %.2f m out"), Checked, Worst));
	TestTrue(TEXT("vehicles were checked"), Checked > 300);
	return Wrong == 0;
}

// CITY-049 (the rule proved in Tools/citysim/giveway.py): a resident car on its way with the player standing in its path
// slows and stands short of them, never touching them, and waits; once they are gone it goes on along its own timeline,
// never less late while it moves (so never faster than the timeline, which keeps to the limits), and is back on its
// timeline by the end of its next stop. One far behind with no player near is put back on its timeline.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY049_CarsGiveWay, "BrixTown.World.CITY049_resident_cars_give_way_to_the_player",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY049_CarsGiveWay::RunTest(const FString&)
{
	FBrixTraffic Traffic;
	FString Error;
	const FString Data = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data")));
	if (!TestTrue(TEXT("the vehicles load: ") + Error, LoadBrixTraffic(FPaths::Combine(Data, TEXT("cars.json")), FPaths::Combine(Data, TEXT("car_models.json")), Traffic, Error))) return false;
	const FString Rules = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/traffic.json")));
	if (!TestTrue(TEXT("the give-way rules load: ") + Error, LoadBrixGiveWay(Rules, Traffic.GiveWayRules, Error))) return false;
	int32 Cases = 0, Wrong = 0;
	const double Dt = .01;   // game minutes (a tenth of a second at the calendar's pace)
	for (int32 k = 0; k < Traffic.Num() && Cases < 6; k += 13)
	{
		if (Traffic.Vehicles[k].Kind != 0) continue;
		for (const FBrixVehicleTrip& T : Traffic.Vehicles[k].Trips)
		{
			if (!T.Bay1 || T.T1 - T.T0 < 1.5) continue;   // a trip that ends parked in the town, long enough to meet someone on
			const double m0 = T.T0 + .4;
			const TOptional<FBrixCarAt> Ahead = Traffic.OnTimeline(k, m0 + .15);
			if (!Ahead || !Ahead->bMoving || !Traffic.OnTimeline(k, m0)->bMoving) continue;
			++Cases;
			Traffic.Late.Reset();
			const FVector3d Me(Ahead->Pos.X, Ahead->Pos.Y, Traffic.GiveWayRules.PersonR);   // standing in the road ahead of it
			double m = m0;
			bool bTouched = false;
			FVector2D Was;
			for (int32 n = 0; n < 1200; ++n, m += Dt)   // two minutes of the player standing there
			{
				Traffic.GiveWay(m, Dt, { Me }, { k });
				const TOptional<FBrixCarAt> C = Traffic.At(k, m + Dt);
				if (C && Traffic.Hits(*C, k, FVector2D(Me.X, Me.Y), Me.Z)) bTouched = true;
				if (n == 1100) Was = C->Pos;
			}
			const TOptional<FBrixCarAt> Stood = Traffic.At(k, m);
			if (bTouched || !Stood || FVector2D::Distance(Stood->Pos, Was) > .05 || Traffic.Late.FindRef(k) < .5)
			{
				AddError(FString::Printf(TEXT("car %d from minute %.1f: %s"), k, m0, bTouched ? TEXT("ran into the player") : TEXT("did not wait for them")));
				++Wrong;
				continue;
			}
			// the player has gone: on along its own timeline, caught up at its next stop
			double L = Traffic.Late.FindRef(k);
			bool bFaster = false;
			const double End = T.T1 + 600;
			while (m < End && Traffic.Late.FindRef(k) > 0)
			{
				Traffic.GiveWay(m, Dt, {}, { k });
				m += Dt;
				const TOptional<FBrixCarAt> C = Traffic.At(k, m);
				if (C && C->bMoving && Traffic.Late.FindRef(k) < L - 1e-9) bFaster = true;
				L = Traffic.Late.FindRef(k);
			}
			const TOptional<FBrixCarAt> A = Traffic.At(k, m), B = Traffic.OnTimeline(k, m);
			if (bFaster || Traffic.Late.FindRef(k) > 0 || A.IsSet() != B.IsSet() || (A && FVector2D::Distance(A->Pos, B->Pos) > 1e-6))
			{
				AddError(FString::Printf(TEXT("car %d from minute %.1f: %s"), k, m0, bFaster ? TEXT("caught up on the move") : TEXT("not back on its timeline after its stop")));
				++Wrong;
			}
			break;
		}
	}
	// far behind, nobody near: back on its timeline
	Traffic.Late.Reset();
	int32 Far = -1;
	double mf = 600;
	for (int32 k = 0; k < Traffic.Num() && Far < 0; ++k)
		for (double m = 480; m < 1100 && Far < 0; m += 7)
			if (Traffic.Vehicles[k].Kind == 0 && Traffic.OnTimeline(k, m - 12) && Traffic.OnTimeline(k, m - 12)->bMoving) { Far = k; mf = m; }
	if (TestTrue(TEXT("a car on the move to put back"), Far >= 0))
	{
		Traffic.Late.Add(Far, 12.0);
		Traffic.GiveWay(mf, Dt, { FVector3d(1e5, 1e5, .4) }, { Far });
		TestEqual(TEXT("put back on its timeline out of sight"), Traffic.Late.FindRef(Far), 0.0);
	}
	TestTrue(TEXT("cars were met in the road"), Cases >= 4);
	return Wrong == 0;
}

#endif
