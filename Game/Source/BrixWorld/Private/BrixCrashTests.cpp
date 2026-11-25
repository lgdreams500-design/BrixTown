// CITY-072 to CITY-076: crash damage, as the slice's smash.js and Tools/citysim/repairs.py have it.
#include "BrixCrash.h"
#include "BrixCity.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool Rules(FAutomationTestBase& T, FBrixCrashRules& R)
	{
		FString Error;
		const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/crash.json")));
		return T.TestTrue(TEXT("the crash rules load: ") + Error, LoadBrixCrashRules(Path, R, Error));
	}
}

// CITY-072, CITY-073: every street thing the Unreal town draws that crash.json names breaks (lamps, benches, bins,
// bollards, post boxes, planters, bike racks, telephone boxes, flowerbeds, market stalls, trees and building corners),
// at any speed, each the way the rules say and costing the car its share of speed; fountains, shelters, water towers
// and wells do not. A toppled thing eases over, lies a little short of flat and settles; pieces fly, bounce and come to
// rest on the ground; rubble from a save is the same every time.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY072_StreetThingsBreak, "BrixTown.World.CITY072_street_things_break_as_the_rules_say",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY072_StreetThingsBreak::RunTest(const FString&)
{
	FBrixCrashRules R;
	if (!Rules(*this, R)) return false;
	TestEqual(TEXT("things break at any speed (the owner, 2026-11-22)"), R.BreakKmh, 0.0);
	const TMap<FString, FString> Want = { { TEXT("lamp"), TEXT("topple") }, { TEXT("tree"), TEXT("topple") }, { TEXT("phonebox"), TEXT("topple") },
		{ TEXT("postbox"), TEXT("topple") }, { TEXT("bollard"), TEXT("topple") }, { TEXT("bench"), TEXT("shatter") }, { TEXT("bin"), TEXT("shatter") },
		{ TEXT("planter"), TEXT("shatter") }, { TEXT("bikerack"), TEXT("shatter") }, { TEXT("flowerbed"), TEXT("shatter") }, { TEXT("stall"), TEXT("shatter") },
		{ TEXT("corner"), TEXT("chip") } };
	for (const TPair<FString, FString>& W : Want)
	{
		const FString K = R.KindOf(W.Key);
		const FBrixCrashKind* C = R.Kinds.Find(K);
		if (!TestTrue(W.Key + TEXT(" breaks"), C != nullptr)) continue;
		TestEqual(W.Key + TEXT(": how"), C->How, W.Value);
		TestTrue(W.Key + TEXT(": the car loses some speed"), C->Slow > 0 && C->Slow < 1);
	}
	for (const TCHAR* K : { TEXT("fountain"), TEXT("shelter"), TEXT("watertower"), TEXT("well"), TEXT("lantern") }) TestTrue(FString(K) + TEXT(" does not break"), R.KindOf(K).IsEmpty());
	// toppling
	TestEqual(TEXT("upright when hit"), BrixToppleAngle(R, 0), 0.0);
	TestTrue(TEXT("falling faster as it goes"), BrixToppleAngle(R, R.ToppleS * .5) < BrixToppleAngle(R, R.ToppleS) * .3);
	TestTrue(TEXT("down a little short of flat"), FMath::IsNearlyEqual(BrixToppleAngle(R, R.ToppleS), UE_DOUBLE_PI / 2 - .06, 1e-9));
	TestTrue(TEXT("a bounce, then still"), BrixToppleAngle(R, R.ToppleS + .15) < UE_DOUBLE_PI / 2 - .07 && FMath::IsNearlyEqual(BrixToppleAngle(R, R.ToppleS + 4), UE_DOUBLE_PI / 2 - .06, 1e-6));
	TestTrue(TEXT("fallen long ago: lying"), FMath::IsNearlyEqual(BrixToppleAngle(R, -1), UE_DOUBLE_PI / 2 - .06, 1e-9));
	// pieces
	auto Ground = [](double, double) { return .12; };
	TArray<FBrixPiece> P;
	BrixBurst(P, FVector(0, .32, 0), 8, 0, .3, { FVector3f(.5f, .3f, .2f), FVector3f(.2f) }, R.Pieces, 7);
	TestEqual(TEXT("as many pieces as the rules say"), P.Num(), R.Pieces);
	double Forward = 0;
	for (const FBrixPiece& Q : P) Forward += Q.V.X;
	TestTrue(TEXT("thrown the way the car went"), Forward > 0);
	for (int32 n = 0; n < 600; ++n) BrixStepPieces(P, R, 1.0 / 60, Ground);
	bool bAllDown = true;
	for (const FBrixPiece& Q : P) bAllDown &= Q.bRest && FMath::Abs(Q.P.Y - (.12 + Q.H / 2)) < 1e-6;
	TestTrue(TEXT("every piece at rest on the ground after ten seconds"), bAllDown);
	TArray<FBrixPiece> A, B;
	BrixRubble(A, R, 10, 20, .25, { FVector3f(.4f) }, 33, Ground);
	BrixRubble(B, R, 10, 20, .25, { FVector3f(.4f) }, 33, Ground);
	bool bSame = A.Num() == R.RestPieces && A.Num() == B.Num();
	for (int32 k = 0; bSame && k < A.Num(); ++k) bSame = A[k].P == B[k].P && A[k].S == B[k].S && A[k].bRest && FVector2D::Distance(FVector2D(A[k].P.X, A[k].P.Z), FVector2D(10, 20)) < 1.71;
	TestTrue(TEXT("rubble from a save: the same each time, at rest, about where it broke"), bSame);
	return true;
}

// CITY-074: damage is stored state, a list of what broke, where and when; it is kept and read back unchanged.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY074_DamageIsKept, "BrixTown.World.CITY074_damage_is_kept",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY074_DamageIsKept::RunTest(const FString&)
{
	TArray<FBrixDamage> D = { { TEXT("lamp@601.5,410.2"), TEXT("lamp"), 23432.25, FVector2D(601.5, 410.2), FVector2D(.6, .8), FVector2D::ZeroVector },
		{ TEXT("corner@b_6_4_l0:2"), TEXT("corner"), 23440.5, FVector2D(601, 407), FVector2D(-1, 0), FVector2D(1, 1) } };
	TArray<FBrixDamage> Back;
	if (!TestTrue(TEXT("read back"), BrixDamageFromJson(BrixDamageToJson(D), Back))) return false;
	if (!TestEqual(TEXT("as many"), Back.Num(), D.Num())) return false;
	for (int32 k = 0; k < D.Num(); ++k)
		TestTrue(D[k].Id + TEXT(" unchanged"), Back[k].Id == D[k].Id && Back[k].Kind == D[k].Kind && Back[k].At == D[k].At && Back[k].Pos == D[k].Pos && Back[k].Dir == D[k].Dir && Back[k].Out == D[k].Out);
	TestFalse(TEXT("a broken save is caught"), BrixDamageFromJson(TEXT("[{\"id\": 3"), Back));
	return true;
}

// CITY-075: repair crews from the town's depots (its enterable utility lots, read from the city here) mend forty pieces
// of damage over two days exactly when Tools/citysim/repairs.py says (Tests/values/repairs_city_01.json, from
// Tools/worldgen/unreal_test_values.py): the same crew, leaving, arriving and done within a millionth of a minute.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY075_RepairsAsPython, "BrixTown.World.CITY075_repair_crews_mend_as_repairs_py_does",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY075_RepairsAsPython::RunTest(const FString&)
{
	FBrixCrashRules R;
	if (!Rules(*this, R)) return false;
	FBrixCity City;
	FString Error, Text;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))), City, Error))) return false;
	TArray<TPair<FString, FVector2D>> Depots;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) if (L.Kind == TEXT("utility") && L.bEnterable) Depots.Add({ L.Id, L.Door });
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/repairs_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	TestEqual(TEXT("the same depots as repairs.py"), Depots.Num(), Want->GetArrayField(TEXT("depots")).Num());
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("depots")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		const TPair<FString, FVector2D>* D = Depots.FindByPredicate([&](const TPair<FString, FVector2D>& P) { return P.Key == A[0]->AsString(); });
		TestTrue(A[0]->AsString() + TEXT(" is a depot with its door"), D && FVector2D::Distance(D->Value, FVector2D(A[1]->AsArray()[0]->AsNumber(), A[1]->AsArray()[1]->AsNumber())) < 1e-6);
	}
	TArray<FBrixDamage> Damage;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("damage")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& P = O->GetArrayField(TEXT("pos"));
		Damage.Add({ O->GetStringField(TEXT("id")), O->GetStringField(TEXT("kind")), O->GetNumberField(TEXT("at")), FVector2D(P[0]->AsNumber(), P[1]->AsNumber()) });
	}
	const TMap<FString, FBrixRepair> Plan = BrixRepairs(Damage, Depots, R, Want->GetNumberField(TEXT("pace")));
	int32 Wrong = 0;
	for (const auto& KV : Want->GetObjectField(TEXT("plan"))->Values)
	{
		const TSharedPtr<FJsonObject> E = KV.Value->AsObject();
		const FBrixRepair* G = Plan.Find(FString(KV.Key));
		if (!G || G->Crew != E->GetStringField(TEXT("crew")) || FMath::Abs(G->Leave - E->GetNumberField(TEXT("leave"))) > 1e-6
			|| FMath::Abs(G->Arrive - E->GetNumberField(TEXT("arrive"))) > 1e-6 || FMath::Abs(G->Done - E->GetNumberField(TEXT("done"))) > 1e-6)
		{
			if (Wrong++ < 8) AddError(FString::Printf(TEXT("%s: %s, done %.3f; repairs.py: %s, done %.3f"), *FString(KV.Key), G ? *G->Crew : TEXT("no crew"), G ? G->Done : 0.0,
				*E->GetStringField(TEXT("crew")), E->GetNumberField(TEXT("done"))));
		}
	}
	TestEqual(TEXT("every piece of damage planned"), Plan.Num(), Damage.Num());
	return Wrong == 0 && Damage.Num() >= 40;
}

// CITY-076: a resident within react_m of a smash turns to it, throws up their arms and shouts for react_s; further off
// or later they carry on.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY076_ResidentsShoutAtASmash, "BrixTown.World.CITY076_residents_shout_at_a_smash",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY076_ResidentsShoutAtASmash::RunTest(const FString&)
{
	FBrixCrashRules R;
	if (!Rules(*this, R)) return false;
	FBrixDodged Q;
	TestTrue(TEXT("near and soon after: reacts"), BrixCrashReact(R, 5, FVector2D(10, 0), 0, 0, R.ReactS * .4, 100, 100 + R.ReactS * .4, Q));
	TestTrue(TEXT("turned to the smash"), FMath::Abs(FMath::FindDeltaAngleRadians(Q.Yaw, FMath::Atan2(-10.0, 0.0))) < 1e-9);
	TestTrue(TEXT("both arms up"), Q.Arms[0] && Q.Arms[1] && Q.Arms[0]->X > .99 && (*Q.Arms[0])[1] > 120 && (*Q.Arms[1])[1] > 120);
	TestTrue(TEXT("a shout from the rules"), R.Shouts.Contains(Q.Shout));
	TestFalse(TEXT("too far: carries on"), BrixCrashReact(R, 5, FVector2D(R.ReactM + .1, 0), 0, 0, 1, 100, 101, Q));
	TestFalse(TEXT("too long ago: carries on"), BrixCrashReact(R, 5, FVector2D(3, 0), 0, 0, R.ReactS, 100, 100 + R.ReactS, Q));
	return true;
}

#endif
