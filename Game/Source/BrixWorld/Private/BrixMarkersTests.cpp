// QST-005, ADR-0051: the markers over heads and places are World/rules/markers.json as world.marker_problems() checks it,
// each step of a quest gets the marker for what it asks (a new line offered, someone to talk to, going back to hand it in,
// a place by what it is), and a marker is a spark far off, a token nearer, larger near enough to act.
#include "BrixCity.h"
#include "BrixEconomy.h"
#include "BrixMap.h"
#include "BrixMarkers.h"
#include "BrixQuests.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString MarkersFile() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/markers.json"))); }
	FString MarkersCityDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))); }
	TSet<FString> MarkerLotKinds(const FBrixCity& City) { TSet<FString> S; for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) S.Add(L.Kind); return S; }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST005_TheMarkersAreSound, "BrixTown.World.QST005_the_markers_are_sound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST005_TheMarkersAreSound::RunTest(const FString&)
{
	FBrixCity City;
	FBrixMarkerRules R;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(MarkersCityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("markers.json loads: ") + Error, LoadBrixMarkers(MarkersFile(), R, Error))) return false;
	const TSet<FString> Kinds = MarkerLotKinds(City);
	const TArray<FString> P = BrixMarkerProblems(R.Json, Kinds);
	TestEqual(TEXT("no problems: ") + FString::Join(P, TEXT("; ")), P.Num(), 0);
	for (const auto& K : R.Kinds)
	{
		const FBrixMarkerSign* S = R.SignOf(K.Key);
		TestTrue(TEXT("a sign with strokes for ") + K.Key, S && S->Lines.Num() > 0 && S->Lines.Num() == S->Closed.Num());
	}
	TestEqual(TEXT("the caption's words"), R.Words(TEXT("talk"), TEXT("Mira")), FString(TEXT("Talk to Mira")));
	// a broken copy is caught as world.py catches it (test_QST_005_a_bad_marker_is_caught)
	FBrixMarkerRules B;
	LoadBrixMarkers(MarkersFile(), B, Error);
	const TSharedPtr<FJsonObject> J = B.Json, K = J->GetObjectField(TEXT("kinds"));
	K->GetObjectField(TEXT("cafe"))->SetStringField(TEXT("sign"), TEXT("basket"));
	K->GetObjectField(TEXT("bank"))->SetStringField(TEXT("colour"), TEXT("olive"));
	K->RemoveField(TEXT("handin"));
	J->GetObjectField(TEXT("behaviour"))->SetNumberField(TEXT("token_m"), 500);
	{
		TArray<TSharedPtr<FJsonValue>> Line = J->GetObjectField(TEXT("signs"))->GetObjectField(TEXT("tag"))->GetArrayField(TEXT("lines"))[0]->AsArray();
		Line[0] = MakeShared<FJsonValueNumber>(30);
		TArray<TSharedPtr<FJsonValue>> Lines = { MakeShared<FJsonValueArray>(Line) };
		J->GetObjectField(TEXT("signs"))->GetObjectField(TEXT("tag"))->SetArrayField(TEXT("lines"), Lines);
	}
	const FString Bad = FString::Join(BrixMarkerProblems(J, Kinds), TEXT("; "));
	for (const TCHAR* Bit : { TEXT("share the sign basket"), TEXT("tag goes outside"), TEXT("bank's colour"), TEXT("near_m < token_m < spark_m"), TEXT("no kind handin") })
		TestTrue(FString(TEXT("caught: ")) + Bit, Bad.Contains(Bit));
	// how strong a marker is with distance
	TestEqual(TEXT("nothing beyond the spark's reach"), R.At(R.SparkM + 1, false).Alpha, 0.f);
	TestTrue(TEXT("a spark between"), R.At((R.TokenM + R.SparkM) / 2, false).bSpark);
	const FBrixMarkerRules::FLook Mid = R.At(R.TokenM - R.FadeM - 1, false), Near = R.At(R.NearM / 2, false);
	TestTrue(TEXT("a token, at full strength, within its reach"), !Mid.bSpark && FMath::IsNearlyEqual(Mid.Alpha, 1.f));
	TestTrue(TEXT("larger near enough to act"), Near.Size > Mid.Size && FMath::IsNearlyEqual(Near.Size, R.TokenNearPx));
	TestTrue(TEXT("a place's token shows from nearer than a person's"), R.At((R.PlaceM + R.TokenM) / 2, true).bSpark && !R.At((R.PlaceM + R.TokenM) / 2, false).bSpark);
	TestTrue(TEXT("the pulse beats between 0 and 1"), FMath::IsNearlyEqual(R.Pulse(0), 0.f) && FMath::IsNearlyEqual(R.Pulse(R.PulseS / 2), 1.f));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST005_EachStepGetsItsMarker, "BrixTown.World.QST005_each_quest_step_gets_its_marker",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST005_EachStepGetsItsMarker::RunTest(const FString&)
{
	FBrixCity City;
	FBrixLifeData Life;
	TArray<FBrixEconLot> Lots;
	FBrixQuestRules QR;
	TArray<FBrixQuestResident> Res;
	FBrixMarkerRules R;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(MarkersCityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json"))), Life, Error))) return false;
	if (!TestTrue(TEXT("the lots: ") + Error, BrixEconLots(City, Life, Lots, Error))) return false;
	if (!TestTrue(TEXT("the quests: ") + Error, LoadBrixQuests(Life.Json, QR, Res, Error))) return false;
	if (!TestTrue(TEXT("markers.json loads: ") + Error, LoadBrixMarkers(MarkersFile(), R, Error))) return false;
	TArray<FString> LotKind;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) LotKind.Add(L.Kind);
	FBrixEconomy E(Life, Lots);
	FBrixQuests Q(QR, E, Res);
	auto KindOf = [&](int32 Resident, int32 Lot = -1) -> FString
	{
		for (const FBrixMapTarget& T : BrixQuestTargets(Q, QR, E))
			if ((Resident >= 0 && T.Resident == Resident) || (Lot >= 0 && T.Lot == Lot))
				return BrixMarkerKindFor(T, R, LotKind.IsValidIndex(T.Lot) ? LotKind[T.Lot] : FString());
		return FString();
	};
	auto Goto = [&](const FString& Line, const FString& Stage)   // put a line at a stage, as a talk or an event would
	{
		for (auto& A : Q.Active) if (A.Key == Line) { A.Value.Stage = Stage; return; }
		FBrixQuests::FActive A;
		A.Stage = Stage;
		Q.Active.Add({ Line, A });
	};
	TestTrue(TEXT("a new game starts the first line"), Q.NewGame());
	if (!TestTrue(TEXT("a line is under way"), Q.Active.Num() > 0)) return false;
	const FString Qid = Q.Active[0].Key;
	const FBrixQuestLine* L = QR.Quests.FindByPredicate([&](const FBrixQuestLine& X) { return X.Id == Qid; });
	if (!TestNotNull(TEXT("its line"), L)) return false;
	const FBrixQuestStage* First = L->Stages.Find(L->Start);
	if (!TestTrue(TEXT("it starts by sending you to someone"), First && First->Goal.Kind == TEXT("talk"))) return false;
	const int32 Who = Q.ResidentIndex(First->Goal.Value);
	TestEqual(TEXT("the first person to see: talk"), KindOf(Who), FString(TEXT("talk")));
	for (const auto& S : L->Stages)   // a later stage that sends you back to them: hand it in
		if (S.Key != L->Start && S.Value.Goal.Kind == TEXT("talk") && S.Value.Goal.Value == First->Goal.Value)
		{
			Goto(Qid, S.Key);
			TestEqual(TEXT("going back to them (") + S.Key + TEXT("): hand it in"), KindOf(Who), FString(TEXT("handin")));
		}
		else if (S.Value.Goal.Kind == TEXT("bank"))
		{
			Goto(Qid, S.Key);
			TestEqual(TEXT("the bank: its own sign"), KindOf(-1, E.BankLot), FString(TEXT("bank")));
		}
	// another line's step that sends you to a place, shown by what the place is
	for (const FBrixQuestLine& X : QR.Quests)
		for (const auto& S : X.Stages)
			if (S.Value.Goal.Kind == TEXT("go"))
			{
				Goto(X.Id, S.Key);
				const int32 N = E.LotIndex.FindRef(S.Value.Goal.Value, -1);
				const FString* P = LotKind.IsValidIndex(N) ? R.PlaceKinds.Find(LotKind[N]) : nullptr;
				TestEqual(TEXT("a place to go to (") + S.Value.Goal.Value + TEXT(")"), KindOf(-1, N), P ? *P : FString(TEXT("go")));
			}
	// a step that needs something: every shop with it in stock today, by what the shop is; none once it is carried
	auto Shops = [&](const FString& It)
	{
		TArray<int32> N;
		for (const FBrixMapTarget& T : BrixQuestTargets(Q, QR, E)) if (T.Item == It) N.Add(T.Lot);
		return N;
	};
	for (const auto& S : L->Stages)
		if (S.Value.Goal.Kind == TEXT("have"))
		{
			const FString It = S.Value.Goal.Value;
			Goto(Qid, S.Key);
			const TArray<int32> N = Shops(It);
			TestTrue(TEXT("shops with ") + It + TEXT(" are marked"), N.Num() > 0);
			int32 Want = 0;
			for (int32 k = 0; k < E.Kinds.Num(); ++k) Want += !E.Kinds[k].IsEmpty() && !E.Outlets.Contains(k) && E.Stock(k, It) > 0;
			TestEqual(TEXT("every shop with it in stock, and only those"), N.Num(), Want);
			for (int32 n : N)
			{
				const FString* P = LotKind.IsValidIndex(n) ? R.PlaceKinds.Find(LotKind[n]) : nullptr;
				TestEqual(TEXT("shown by what the shop is"), KindOf(-1, n), P ? *P : FString(TEXT("go")));
			}
			{   // only the nearest few are shown
				TArray<FBrixMapTarget> T = BrixQuestTargets(Q, QR, E);
				const int32 Others = T.FilterByPredicate([](const FBrixMapTarget& X) { return X.Item.IsEmpty(); }).Num();
				TArray<FVector2D> Mid;
				for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& Lt : B.Lots) Mid.Add(Lt.Centre());
				const FVector2D At = Mid.IsValidIndex(N.Last()) ? Mid[N.Last()] : FVector2D::ZeroVector;   // stand at the last of them
				BrixKeepNearestShops(T, R, At, [&](int32 k) { return Mid.IsValidIndex(k) ? Mid[k] : FVector2D::ZeroVector; });
				const TArray<FBrixMapTarget> Kept = T.FilterByPredicate([&](const FBrixMapTarget& X) { return X.Item == It; });
				TestEqual(TEXT("the nearest few"), Kept.Num(), FMath::Min(R.HaveShops, N.Num()));
				TestTrue(TEXT("the one you stand at first"), Kept.Num() && Kept[0].Lot == N.Last());
				TestEqual(TEXT("the rest kept"), T.FilterByPredicate([](const FBrixMapTarget& X) { return X.Item.IsEmpty(); }).Num(), Others);
			}
			E.GiveItem(It);
			TestEqual(TEXT("none once it is carried"), Shops(It).Num(), 0);
			E.TakeItem(It);
		}
	// the elder's shopping: the shops first, then back to the elder
	if (const FBrixJobKind* Sk = QR.Job(TEXT("shopping")); Sk && Sk->Items.Num())
	{
		FBrixJob J;
		J.Kind = TEXT("shopping");
		J.Item = Sk->Items[0];
		J.Giver = Who;
		Q.Job = J;
		TestTrue(TEXT("a shopping job marks the shops with ") + J.Item, Shops(J.Item).Num() > 0);
		E.GiveItem(J.Item);
		TestTrue(TEXT("with it bought, back to the one who asked"), BrixQuestTargets(Q, QR, E).ContainsByPredicate([&](const FBrixMapTarget& T) { return T.Resident == Who && T.Kind == TEXT("job"); }) && Shops(J.Item).Num() == 0);
		E.TakeItem(J.Item);
		Q.Job.Reset();
	}
	// one with a line to give
	bool bOffer = false;
	for (const FBrixMapTarget& T : BrixQuestTargets(Q, QR, E)) if (T.bOffer) { bOffer = true; TestEqual(TEXT("a line offered"), BrixMarkerKindFor(T, R, FString()), FString(TEXT("offer"))); }
	AddInfo(bOffer ? TEXT("a line is offered at the start") : TEXT("no line is offered at the start"));
	return true;
}

#endif
