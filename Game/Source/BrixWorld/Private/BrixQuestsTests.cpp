// QST-001 to QST-007: quest lines, jobs and reputation in the Unreal build follow Tools/citysim/quests.py. Game/Data/life.json
// carries a fixed walk through them (build_play.py quest_script()) and what Python gives; the C++ runs it on the same data.
// QST-010 to QST-015: the same for a career's story lines, with a recorded game of the Baker career and the courting
// (build_play.py story_run()): every reply open at every line, every act, and what is left at the end.
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixEconomy.h"
#include "BrixQuests.h"
#include "BrixSave.h"

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

	// Runs a script of ops (build_play.py run_op()) and returns what each gives.
	TArray<FString> RunOps(FAutomationTestBase& T, const TSharedPtr<FJsonObject>& C, FBrixEconomy& E, FBrixQuests& Q, const TArray<FBrixEconLot>& Lots, const TArray<FBrixQuestResident>& Res, bool& bHome)
	{
		const TSharedPtr<FJsonObject> Shops = C->GetObjectField(TEXT("shops"));
		TArray<FString> Results;
		for (const TSharedPtr<FJsonValue>& OpV : C->GetArrayField(TEXT("script")))
		{
			const TArray<TSharedPtr<FJsonValue>>& Op = OpV->AsArray();
			const FString Name = Op[0]->AsString();
			FString R = TEXT("ok");
			bool bOk = true;
			auto Str = [&](int32 k) { return Op.IsValidIndex(k) && !Op[k]->IsNull() ? Op[k]->AsString() : FString(); };
			auto Strs = [&](int32 k) { TArray<FString> A; for (const auto& V : Op[k]->AsArray()) A.Add(V->AsString()); return A; };
			auto Where = [&]() { FBrixQuests::FCtx Ctx; Ctx.Lot = Str(1); if (!Op[2]->IsNull()) Ctx.Pos = FVector2D(Op[2]->AsNumber(), Op[3]->AsNumber()); Ctx.Minute = Op[4]->AsNumber(); return Ctx; };
			if (Name == TEXT("advance")) E.Advance((int32)Op[1]->AsNumber(), Op[2]->AsNumber());
			else if (Name == TEXT("newgame")) bOk = Q.NewGame();
			else if (Name == TEXT("open")) bOk = E.OpenAccount(Str(1));
			else if (Name == TEXT("buy")) bOk = E.Buy((int32)Shops->GetNumberField(Str(1)), Str(2));
			else if (Name == TEXT("act")) bOk = Q.Act(Str(1), Strs(2), Str(3));
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
			// the story lines' ops (QST-010 to QST-015)
			else if (Name == TEXT("start")) bOk = Q.Start(Str(1));
			else if (Name == TEXT("first")) R = Q.FirstLine(Str(1));
			else if (Name == TEXT("replies")) { TArray<FString> A; for (int32 k : Q.Replies(Str(1), Str(2))) A.Add(FString::FromInt(k)); R = Join(A); }
			else if (Name == TEXT("scenes")) R = Join(Q.Scenes(Where()));
			else if (Name == TEXT("checkat")) Q.Check(Where());
			else if (Name == TEXT("home")) bHome = Op[1]->AsNumber() != 0;
			else if (Name == TEXT("earn")) { const int64 N = (int64)Op[1]->AsNumber(); E.L.Post(E.Day, E.Minute, TEXT("Earned"), { { TEXT("sys:residents"), -N }, { TEXT("p:cash"), N } }, E.Why); }
			else if (Name == TEXT("regard")) E.Rep.Add(Str(1), FMath::Max(E.Rep.FindRef(Str(1)), (int32)Op[2]->AsNumber()));
			else if (Name == TEXT("need")) R = Q.NeedOk(Strs(1)) ? TEXT("yes") : TEXT("no");
			else T.AddError(TEXT("an op the port does not know: ") + Name);
			Results.Add(bOk ? R : FString(TEXT("refused")));
		}
		const TArray<TSharedPtr<FJsonValue>>& WR = C->GetObjectField(TEXT("want"))->GetArrayField(TEXT("results"));
		for (int32 k = 0; k < FMath::Max(WR.Num(), Results.Num()); ++k)
		{
			const FString W = WR.IsValidIndex(k) ? WR[k]->AsString() : TEXT("-"), G = Results.IsValidIndex(k) ? Results[k] : TEXT("-");
			if (W != G) T.AddError(FString::Printf(TEXT("op %d gives %s, Python %s (%s)"), k, *G, *W, *E.Why));
		}
		return Results;
	}

	// What is left after a script: the lines under way and finished, the journal, reputation, money and things carried.
	void SameState(FAutomationTestBase& T, const TSharedPtr<FJsonObject>& Want, const FBrixEconomy& E, const FBrixQuests& Q)
	{
		FString GotA, WantA;
		TArray<FString> Keys;
		for (const auto& A : Q.Active) Keys.Add(A.Key + TEXT("=") + A.Value.Stage);
		Keys.Sort();
		GotA = Join(Keys);
		Keys.Reset();
		for (const auto& A : Want->GetObjectField(TEXT("active"))->Values) Keys.Add(FString(A.Key.ToView()) + TEXT("=") + A.Value->AsString());
		Keys.Sort();
		WantA = Join(Keys);
		T.TestEqual(TEXT("the quest lines under way"), GotA, WantA);
		T.TestEqual(TEXT("quest lines finished"), Q.Done.Num(), Want->GetObjectField(TEXT("done"))->Values.Num());
		for (const auto& D : Want->GetObjectField(TEXT("done"))->Values)
		{
			const FString* Got = Q.DoneWith(FString(D.Key.ToView()));
			T.TestEqual(TEXT("the ending of ") + FString(D.Key.ToView()), Got ? *Got : FString(), D.Value->AsString());
		}
		const TArray<TSharedPtr<FJsonValue>>& WLog = Want->GetArrayField(TEXT("log"));
		if (T.TestEqual(TEXT("the journal's notes"), Q.Log.Num(), WLog.Num()))
			for (int32 k = 0; k < WLog.Num(); ++k) T.TestEqual(FString::Printf(TEXT("note %d"), k), Q.Log[k].Text, WLog[k]->AsString());
		for (const auto& R : Want->GetObjectField(TEXT("rep"))->Values) T.TestEqual(TEXT("reputation with ") + FString(R.Key.ToView()), (int32)E.Rep.FindRef(FString(R.Key.ToView())), (int32)R.Value->AsNumber());
		T.TestEqual(TEXT("people with a reputation"), E.Rep.Num(), Want->GetObjectField(TEXT("rep"))->Values.Num());
		for (const auto& B : Want->GetObjectField(TEXT("balances"))->Values) T.TestEqual(TEXT("the balance of ") + FString(B.Key.ToView()), E.L.Balance(FString(B.Key.ToView())), (int64)B.Value->AsNumber());
		T.TestEqual(TEXT("transactions"), (int64)E.L.Tx.Num(), (int64)Want->GetNumberField(TEXT("tx")));
		T.TestEqual(TEXT("things carried"), E.Inv.Num(), Want->GetObjectField(TEXT("inv"))->Values.Num());
		for (const auto& I : Want->GetObjectField(TEXT("inv"))->Values) T.TestEqual(TEXT("carried: ") + FString(I.Key.ToView()), (int32)E.Inv.FindRef(FString(I.Key.ToView())), (int32)I.Value->AsNumber());
	}
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
	const TSharedPtr<FJsonObject> C = Life.Json->GetObjectField(TEXT("quest_checks")), Want = C->GetObjectField(TEXT("want"));
	bool bHome = false;
	RunOps(*this, C, E, Q, Lots, Res, bHome);
	SameState(*this, Want, E, Q);
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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST010_StoryLinesRunAsPython, "BrixTown.World.QST010_story_lines_run_as_the_python_does",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST010_StoryLinesRunAsPython::RunTest(const FString&)
{
	FBrixCity City;
	FBrixLifeData Life;
	TArray<FBrixEconLot> Lots;
	FBrixQuestRules QR;
	TArray<FBrixQuestResident> Res;
	if (!LoadAll(*this, City, Life, Lots, QR, Res)) return false;
	int32 Story = 0, Said = 0, Speakers = 0, Told = 0;   // QST-014, QST-015: the Baker career and the courting are in the data
	for (const FBrixQuestLine& L : QR.Quests)
	{
		if (L.Line.IsEmpty()) continue;
		++Story;
		TestTrue(L.Id + TEXT(" starts when its giver is spoken to (QST-011)"), L.Opens());
		for (const auto& St : L.Stages) for (const auto& N : St.Value.Nodes) { ++Said; Told += N.Value.Who == TEXT("scene"); Speakers += !N.Value.Who.IsEmpty() && N.Value.Who != TEXT("scene"); }
	}
	TestEqual(TEXT("story lines"), Story, 9);
	TestTrue(TEXT("lines nobody says, and lines other people say (QST-012)"), Told > 0 && Speakers > 0 && Said > 300);
	FBrixEconomy E(Life, Lots);
	FBrixQuests Q(QR, E, Res);
	bool bHome = false;
	Q.OwnsHome = [&bHome]() { return bHome; };
	const TSharedPtr<FJsonObject> C = Life.Json->GetObjectField(TEXT("story_checks")), Want = C->GetObjectField(TEXT("want"));
	const TArray<FString> Results = RunOps(*this, C, E, Q, Lots, Res, bHome);
	TestTrue(TEXT("a long play"), Results.Num() > 300);
	SameState(*this, Want, E, Q);
	auto Names = [&](const TCHAR* F) { TArray<FString> A; for (const auto& V : Want->GetArrayField(F)) A.Add(V->AsString()); return Join(A); };
	TestEqual(TEXT("the facts the player left (QST-010)"), Join(Q.Flags), Names(TEXT("flags")));
	TestEqual(TEXT("the lines taken again (QST-013)"), Join(Q.Again), Names(TEXT("again")));
	TestTrue(TEXT("a line was taken again in this play"), Q.Again.Num() > 0);
	int32 Gave = 0;
	for (const auto& A : Q.Awarded) Gave += A.Value.Num();
	TestEqual(TEXT("reputation the replies gave, by line and person"), Gave, Want->GetArrayField(TEXT("awarded")).Num());
	for (const auto& V : Want->GetArrayField(TEXT("awarded")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		const TMap<FString, int32>* M = Q.Awarded.Find(A[0]->AsString());
		TestEqual(TEXT("given in ") + A[0]->AsString() + TEXT(" to ") + A[1]->AsString(), M ? M->FindRef(A[1]->AsString()) : 0, (int32)A[2]->AsNumber());
	}
	const FString* Wed = Q.DoneWith(TEXT("the_wedding"));
	TestTrue(TEXT("the play reaches the wedding (QST-015)"), Wed != nullptr);
	TestTrue(TEXT("and she said yes to a home"), bHome && Q.DoneWith(TEXT("question_mei")) && *Q.DoneWith(TEXT("question_mei")) == TEXT("yes"));
	// QST-008: the facts are part of the save
	FBrixEconomy E2(Life, Lots);
	FBrixQuests Q2(QR, E2, Res);
	FString Why;
	TestTrue(TEXT("the quests' save reads back"), BrixQuestsLoad(Q2, *BrixQuestsSave(Q), Why));
	TestEqual(TEXT("facts kept in the save"), Join(Q2.Flags), Join(Q.Flags));
	TestEqual(TEXT("lines taken again kept in the save"), Join(Q2.Again), Join(Q.Again));
	int32 Kept = 0;   // by line and person: a line whose replies gave nothing has nothing to keep
	for (const auto& A : Q2.Awarded) for (const auto& R : A.Value) Kept += Q.Awarded.FindRef(A.Key).FindRef(R.Key) == R.Value;
	TestEqual(TEXT("reputation given kept in the save"), Kept, Gave);
	return true;
}

#endif
