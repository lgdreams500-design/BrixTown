// CITY-070, CITY-071: residents get out of the way of the player's car and tell the driver off, as the slice's dodge.js does.
#include "BrixDodge.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

// Prototype/play/dodge.js, run by Tools/unreal/people_values.py (Tests/values/dodge_city_01.json): a car driven east at
// 9 m/s past residents standing, walking and sitting at either side of its line, a wall to the north, then stopping. Frame
// by frame the port lays the same on each: who dodges, where to (the side with room), the startle, the scramble's hop and
// run, the turn on the driver with a fist or both arms and the shout (CITY-071), the walk back; within a micrometre and a
// microradian.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY070_DodgeAsTheSlice, "BrixTown.World.CITY070_residents_dodge_and_scold_as_the_slice_does",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY070_DodgeAsTheSlice::RunTest(const FString&)
{
	FBrixDodge Dodge;
	FString Error, Text;
	const FString Rules = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/dodge.json")));
	if (!TestTrue(TEXT("the dodge rules load: ") + Error, LoadBrixDodgeRules(Rules, Dodge.R, Error))) return false;
	Dodge.bOn = true;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/dodge_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	struct FRes { int32 i; FVector2D P; double Yaw; int32 Pose; };
	TArray<FRes> Res;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("residents")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		Res.Add({ (int32)A[0]->AsNumber(), FVector2D(A[1]->AsNumber(), A[2]->AsNumber()), A[3]->AsNumber(), (int32)A[4]->AsNumber() });
	}
	const double CarYaw = Want->GetNumberField(TEXT("yaw"));
	const FVector2D Heading(FMath::Sin(CarYaw), -FMath::Cos(CarYaw));
	auto Clear = [](const FVector2D& P) { return P.Y >= -2.6; };   // the wall north of z -2.6
	int32 Wrong = 0, Laid = 0, Shouts = 0, Fists = 0;
	TSet<int32> Dodged;
	for (const TSharedPtr<FJsonValue>& FV : Want->GetArrayField(TEXT("frames")))
	{
		const TSharedPtr<FJsonObject> F = FV->AsObject();
		const double T = F->GetNumberField(TEXT("t"));
		const TArray<TSharedPtr<FJsonValue>>& Car = F->GetArrayField(TEXT("car"));
		Dodge.Frame(T, true, FVector2D(Car[0]->AsNumber(), Car[1]->AsNumber()), Heading, .12, Car[2]->AsNumber());
		const TArray<TSharedPtr<FJsonValue>>& Rows = F->GetArrayField(TEXT("rows"));
		for (int32 r = 0; r < Res.Num(); ++r)
		{
			FBrixDodged Q;
			Q.Pos = Res[r].P; Q.Yaw = Res[r].Yaw; Q.Pose = Res[r].Pose; Q.bWalking = Res[r].Pose == 1; Q.Phase = .3;
			const bool bGot = Dodge.Apply(Res[r].i, Res[r].P, .12, Res[r].Yaw, Res[r].Pose, false, Clear, Q);
			const TArray<TSharedPtr<FJsonValue>>* W = nullptr;
			const bool bWant = Rows[r]->TryGetArray(W);
			const FString Who = FString::Printf(TEXT("resident %d at %.2f s"), Res[r].i, T - 100);
			if (bGot != bWant) { if (Wrong++ < 12) AddError(Who + (bGot ? TEXT(": dodges, dodge.js leaves them be") : TEXT(": left be, dodge.js has them dodge"))); continue; }
			if (!bGot) continue;
			++Laid;
			Dodged.Add(Res[r].i);
			const TArray<TSharedPtr<FJsonValue>>& E = *W;
			bool bSame = FMath::Abs(Q.Pos.X - E[0]->AsNumber()) < 1e-6 && FMath::Abs(Q.Pos.Y - E[1]->AsNumber()) < 1e-6 && FMath::Abs(Q.Hop - E[2]->AsNumber()) < 1e-6
				&& FMath::Abs(FMath::FindDeltaAngleRadians(Q.Yaw, E[3]->AsNumber())) < 1e-6 && Q.Pose == (int32)E[4]->AsNumber() && Q.bWalking == (E[5]->AsNumber() != 0)
				&& FMath::Abs(Q.Phase - E[6]->AsNumber()) < 1e-6 && Q.Sig == E[7]->AsString() && Q.Shout == E[10]->AsString();
			const TArray<TSharedPtr<FJsonValue>>* Arms = nullptr;
			if (E[8]->TryGetArray(Arms))
				for (int32 a = 0; a < 2; ++a)
				{
					const TArray<TSharedPtr<FJsonValue>>& G = (*Arms)[a]->AsArray();
					for (int32 j = 0; j < 4; ++j) bSame &= Q.Arms[a].IsSet() && FMath::Abs((*Q.Arms[a])[j] - G[j]->AsNumber()) < 1e-6;
				}
			else bSame &= !Q.Arms[0] && !Q.Arms[1];
			const TArray<TSharedPtr<FJsonValue>>* Chest = nullptr;
			if (E[9]->TryGetArray(Chest)) bSame &= FMath::Abs(Q.Chest.X - (*Chest)[0]->AsNumber()) < 1e-6 && FMath::Abs(Q.Chest.Y - (*Chest)[1]->AsNumber()) < 1e-6;
			if (!Q.Shout.IsEmpty()) ++Shouts;
			if (Q.Arms[1] && (*Q.Arms[1])[1] > 160) ++Fists;
			if (!bSame && Wrong++ < 12)
				AddError(FString::Printf(TEXT("%s: at (%.3f, %.3f) pose %d '%s' '%s', dodge.js (%.3f, %.3f) pose %d '%s' '%s'"), *Who, Q.Pos.X, Q.Pos.Y, Q.Pose, *Q.Sig, *Q.Shout,
					E[0]->AsNumber(), E[1]->AsNumber(), (int32)E[4]->AsNumber(), *E[7]->AsString(), *E[10]->AsString()));
		}
	}
	AddInfo(FString::Printf(TEXT("%d residents dodged; %d frames laid on, %d with a shout, %d with a fist"), Dodged.Num(), Laid, Shouts, Fists));
	TestTrue(TEXT("residents dodged, shouted and shook fists"), Dodged.Num() >= 4 && Shouts > 0 && Fists > 0);
	return Wrong == 0;
}

#endif
