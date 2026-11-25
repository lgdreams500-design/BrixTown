// QST-001 to QST-007: quest lines, jobs and reputation in the Unreal build follow Tools/citysim/quests.py. Game/Data/life.json
// carries a fixed walk through them (build_play.py quest_script()) and what Python gives; the C++ runs it on the same data.
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixEconomy.h"
#include "BrixQuests.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LoadAll(FAutomationTestBase& T, FBrixCity& City, FBrixLifeData& Life, TArray<FBrixEconLot>& Lots, FBrixQuestRules& Q, TArray<FBrixQuestResident>& Res)
	{
		FString Error;
		if (!T.TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))), City, Error))) return false;
		if (!T.TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json"))), Life, Error))) return false;
		if (!T.TestTrue(TEXT("the lots are in order: ") + Error, BrixEconLots(City, Life, Lots, Error))) return false;
		return T.TestTrue(TEXT("the quests load (QST-001): ") + Error, LoadBrixQuests(Life.Json, Q, Res, Error));
	}
	FString Join(const TArray<FString>& A) { return FString::Join(A, TEXT(",")); }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST001_QuestsRunAsPython, "BrixTown.World.QST001_quests_run_as_the_python_does",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST001_QuestsRunAsPython::RunTest(const FString&)
{
	FBrixCity City;
	FBrixLifeData Life;
	TArray<FBrixEconLot> Lots;
	FBrixQuestRules QR;
	TArray<FBrixQuestResident> Res;
	if (!LoadAll(*this, City, Life, Lots, QR, Res)) return false;
	FBrixEconomy E(Life, Lots);
	FBrixQuests Q(QR, E, Res);
	const TSharedPtr<FJsonObject> C = Life.Json->GetObjectField(TEXT("quest_checks")), Shops = C->GetObjectField(TEXT("shops")), Want = C->GetObjectField(TEXT("want"));
	TArray<FString> Results;
	for (const TSharedPtr<FJsonValue>& OpV : C->GetArrayField(TEXT("script")))
	{
		const TArray<TSharedPtr<FJsonValue>>& Op = OpV->AsArray();
		const FString Name = Op[0]->AsString();
		FString R = TEXT("ok");
		bool bOk = true;
		auto Str = [&](int32 k) { return Op.IsValidIndex(k) && !Op[k]->IsNull() ? Op[k]->AsString() : FString(); };
		if (Name == TEXT("advance")) E.Advance((int32)Op[1]->AsNumber(), Op[2]->AsNumber());
		else if (Name == TEXT("newgame")) bOk = Q.NewGame();
		else if (Name == TEXT("open")) bOk = E.OpenAccount(Str(1));
		else if (Name == TEXT("buy")) bOk = E.Buy((int32)Shops->GetNumberField(Str(1)), Str(2));
		else if (Name == TEXT("act"))
		{
			TArray<FString> Acts;
			for (const auto& A : Op[2]->AsArray()) Acts.Add(A->AsString());
			bOk = Q.Act(Str(1), Acts, Str(3));
		}
		else if (Name == TEXT("check")) { FBrixQuests::FCtx Ctx; Ctx.Lot = Str(1); Ctx.Pos = FVector2D(Op[2]->AsNumber(), Op[3]->AsNumber()); Ctx.Minute = E.Minute; Q.Check(Ctx); }
		else if (Name == TEXT("talking")) R = Join(Q.TalkingTo(Str(1)));
		else if (Name == TEXT("offers")) R = Join(Q.Offers(Str(1)));
		else if (Name == TEXT("take"))
		{
			const int32 Day = (int32)Op[2]->AsNumber();
			TOptional<FBrixJob> O;
			for (int32 i = 0; i < Res.Num() && !O; ++i) { const TOptional<FBrixJob> X = Q.JobOffer(i, Day); if (X && X->Kind == Str(1)) O = X; }
			bOk = O.IsSet() && Q.TakeJob(*O);
			if (bOk) R = FString::Printf(TEXT("%d|%lld"), O->Giver, O->Pay);
		}
		else if (Name == TEXT("buyfor"))
		{
			int32 n = -1;
			for (int32 k = 0; k < Lots.Num() && n < 0; ++k) { const FString K = E.ShopKind(k); if (!K.IsEmpty() && !E.Outlets.Contains(k) && Q.Job && E.R.Shop(K)->Sells.Contains(Q.Job->Item)) n = k; }
			bOk = n >= 0 && E.Buy(n, Q.Job->Item);
		}
		else if (Name == TEXT("handover")) bOk = Q.HandOver();
		else if (Name == TEXT("at"))
		{
			FBrixQuests::FCtx Ctx;
			if (Q.Job) Ctx.Lot = Lots[Str(1) == TEXT("frm") ? Q.Job->Frm : Q.Job->To].Id;
			Ctx.Minute = E.Minute;
			Q.Check(Ctx);
		}
		else AddError(TEXT("an op the port does not know: ") + Name);
		Results.Add(bOk ? R : FString(TEXT("refused")));
	}
	const TArray<TSharedPtr<FJsonValue>>& WR = Want->GetArrayField(TEXT("results"));
	for (int32 k = 0; k < FMath::Max(WR.Num(), Results.Num()); ++k)
	{
		const FString W = WR.IsValidIndex(k) ? WR[k]->AsString() : TEXT("-"), G = Results.IsValidIndex(k) ? Results[k] : TEXT("-");
		if (W != G) AddError(FString::Printf(TEXT("op %d gives %s, Python %s (%s)"), k, *G, *W, *E.Why));
	}
	FString GotA, WantA;
	for (const auto& A : Q.Active) GotA += A.Key + TEXT("=") + A.Value.Stage + TEXT(" ");
	for (const auto& A : Want->GetObjectField(TEXT("active"))->Values) WantA += FString(A.Key.ToView()) + TEXT("=") + A.Value->AsString() + TEXT(" ");
	TestEqual(TEXT("the quest lines under way"), GotA, WantA);
	TestEqual(TEXT("quest lines finished"), Q.Done.Num(), Want->GetObjectField(TEXT("done"))->Values.Num());
	for (const auto& D : Want->GetObjectField(TEXT("done"))->Values)
	{
		const FString* Got = Q.DoneWith(FString(D.Key.ToView()));
		TestEqual(TEXT("the ending of ") + FString(D.Key.ToView()), Got ? *Got : FString(), D.Value->AsString());
	}
	const TArray<TSharedPtr<FJsonValue>>& WLog = Want->GetArrayField(TEXT("log"));
	if (TestEqual(TEXT("the journal's notes"), Q.Log.Num(), WLog.Num()))
		for (int32 k = 0; k < WLog.Num(); ++k) TestEqual(FString::Printf(TEXT("note %d"), k), Q.Log[k].Text, WLog[k]->AsString());
	TestEqual(TEXT("a job under way"), Q.Job.IsSet(), (Want->TryGetField(TEXT("job")).IsValid() && !Want->TryGetField(TEXT("job"))->IsNull()));
	TArray<FString> WJ;
	for (const auto& V : Want->GetArrayField(TEXT("jobs_done"))) WJ.Add(V->AsString());
	TestEqual(TEXT("the jobs taken"), Join(Q.JobsDone), Join(WJ));
	for (const auto& R : Want->GetObjectField(TEXT("rep"))->Values) TestEqual(TEXT("reputation with ") + FString(R.Key.ToView()), (int32)E.Rep.FindRef(FString(R.Key.ToView())), (int32)R.Value->AsNumber());
	TestEqual(TEXT("people with a reputation"), E.Rep.Num(), Want->GetObjectField(TEXT("rep"))->Values.Num());
	for (const auto& B : Want->GetObjectField(TEXT("balances"))->Values) TestEqual(TEXT("the balance of ") + FString(B.Key.ToView()), E.L.Balance(FString(B.Key.ToView())), (int64)B.Value->AsNumber());
	TestEqual(TEXT("transactions"), (int64)E.L.Tx.Num(), (int64)Want->GetNumberField(TEXT("tx")));
	TestEqual(TEXT("things carried"), E.Inv.Num(), Want->GetObjectField(TEXT("inv"))->Values.Num());
	// QST-006: who offers which job on days 0 to 2, worked out from (resident, day) alone
	TArray<TArray<FString>> Got;
	for (int32 Day = 0; Day < 3; ++Day)
		for (int32 i = 0; i < Res.Num(); ++i)
			if (const TOptional<FBrixJob> O = Q.JobOffer(i, Day))
				Got.Add({ FString::FromInt(i), FString::FromInt(Day), O->Kind, FString::Printf(TEXT("%lld"), O->Pay), FString::FromInt(O->Frm), FString::FromInt(O->To), O->Item, FString::FromInt(O->Minutes) });
	TestEqual(TEXT("jobs on offer over three days"), Got.Num(), (int32)Want->GetNumberField(TEXT("offer_count")));
	const TArray<TSharedPtr<FJsonValue>>& WO = Want->GetArrayField(TEXT("offers"));
	for (int32 k = 0; k < WO.Num() && k < Got.Num(); ++k)
	{
		FString W;
		for (const auto& X : WO[k]->AsArray()) W += (W.IsEmpty() ? TEXT("") : TEXT(" ")) + (X->Type == EJson::String ? X->AsString() : FString::Printf(TEXT("%lld"), (int64)X->AsNumber()));
		TestEqual(FString::Printf(TEXT("job offer %d"), k), FString::Join(Got[k], TEXT(" ")), W);
	}
	return true;
}

#endif
