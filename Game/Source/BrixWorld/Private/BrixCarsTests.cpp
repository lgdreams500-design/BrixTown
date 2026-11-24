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

#endif
