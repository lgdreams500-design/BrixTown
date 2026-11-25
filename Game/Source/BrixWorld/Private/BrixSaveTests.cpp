// QST-008: a save brings the game back. The fixed walk through the quests (Game/Data/life.json quest_checks, as QST001 runs
// it) is stopped halfway, saved, read back into a new economy and new quests, and both games are then played on to the end:
// the two must stay the same in every part a save keeps. A loan, the market stall and clothes from an outlet are in it too.
#include "BrixCity.h"
#include "BrixEconomy.h"
#include "BrixQuests.h"
#include "BrixSave.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString SProj(const TCHAR* P) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), P)); }
	FString Text(const TSharedRef<FJsonObject>& O)
	{
		FString Out;
		const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
		FJsonSerializer::Serialize(O, W);
		return Out;
	}
	// one op of build_play.py quest_script() (QST001's runner, without the checks)
	void RunOp(const TArray<TSharedPtr<FJsonValue>>& Op, FBrixEconomy& E, FBrixQuests& Q, const TArray<FBrixEconLot>& Lots, const TArray<FBrixQuestResident>& Res, const TSharedPtr<FJsonObject>& Shops)
	{
		const FString Name = Op[0]->AsString();
		auto Str = [&](int32 k) { return Op.IsValidIndex(k) && !Op[k]->IsNull() ? Op[k]->AsString() : FString(); };
		if (Name == TEXT("advance")) E.Advance((int32)Op[1]->AsNumber(), Op[2]->AsNumber());
		else if (Name == TEXT("newgame")) Q.NewGame();
		else if (Name == TEXT("open")) E.OpenAccount(Str(1));
		else if (Name == TEXT("buy")) E.Buy((int32)Shops->GetNumberField(Str(1)), Str(2));
		else if (Name == TEXT("act")) { TArray<FString> Acts; for (const auto& A : Op[2]->AsArray()) Acts.Add(A->AsString()); Q.Act(Str(1), Acts, Str(3)); }
		else if (Name == TEXT("check")) { FBrixQuests::FCtx Ctx; Ctx.Lot = Str(1); Ctx.Pos = FVector2D(Op[2]->AsNumber(), Op[3]->AsNumber()); Ctx.Minute = E.Minute; Q.Check(Ctx); }
		else if (Name == TEXT("take"))
		{
			const int32 Day = (int32)Op[2]->AsNumber();
			TOptional<FBrixJob> O;
			for (int32 i = 0; i < Res.Num() && !O; ++i) { const TOptional<FBrixJob> X = Q.JobOffer(i, Day); if (X && X->Kind == Str(1)) O = X; }
			if (O) Q.TakeJob(*O);
		}
		else if (Name == TEXT("buyfor"))
		{
			for (int32 k = 0; k < Lots.Num(); ++k) { const FString K = E.ShopKind(k); if (!K.IsEmpty() && !E.Outlets.Contains(k) && Q.Job && E.R.Shop(K)->Sells.Contains(Q.Job->Item)) { E.Buy(k, Q.Job->Item); break; } }
		}
		else if (Name == TEXT("handover")) Q.HandOver();
		else if (Name == TEXT("at")) { FBrixQuests::FCtx Ctx; if (Q.Job) Ctx.Lot = Lots[Str(1) == TEXT("frm") ? Q.Job->Frm : Q.Job->To].Id; Ctx.Minute = E.Minute; Q.Check(Ctx); }
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST008_ASaveBringsTheGameBack, "BrixTown.World.QST008_a_save_brings_the_game_back",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST008_ASaveBringsTheGameBack::RunTest(const FString&)
{
	FBrixCity City;
	FBrixLifeData Life;
	TArray<FBrixEconLot> Lots;
	FBrixQuestRules QR;
	TArray<FBrixQuestResident> Res;
	FBrixDesignerRules D;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(SProj(TEXT("../World/city_01")), City, Error))) return false;
	if (!TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(SProj(TEXT("Data/life.json")), Life, Error))) return false;
	if (!TestTrue(TEXT("the lots are in order: ") + Error, BrixEconLots(City, Life, Lots, Error))) return false;
	if (!TestTrue(TEXT("the quests load: ") + Error, LoadBrixQuests(Life.Json, QR, Res, Error))) return false;
	if (!TestTrue(TEXT("the designer loads: ") + Error, LoadBrixDesigner(SProj(TEXT("../World/rules/designer.json")), SProj(TEXT("../World/rules/settings.json")), D, Error))) return false;
	const TSharedPtr<FJsonObject> C = Life.Json->GetObjectField(TEXT("quest_checks")), Shops = C->GetObjectField(TEXT("shops"));
	const TArray<TSharedPtr<FJsonValue>>& Script = C->GetArrayField(TEXT("script"));

	// a new game starts clean: nothing but the starting cash
	FBrixEconomy A(Life, Lots);
	FBrixQuests QA(QR, A, Res);
	A.NewCharacter();
	TestTrue(TEXT("a new game holds only the starting cash"), A.L.Statement({ TEXT("p:cash") }).Num() == 1 && A.TotalMoney() == A.R.StartCash && !A.Loans.Num() && !A.Inv.Num() && !QA.Active.Num() && !QA.Done.Num());

	const int32 Half = Script.Num() / 2;
	for (int32 k = 0; k < Half; ++k) RunOp(Script[k]->AsArray(), A, QA, Lots, Res, Shops);
	A.TakeLoan(A.R.LoanMin, A.R.LoanTerms.Num() ? A.R.LoanTerms[0] : 3);   // a loan under way, a stall rented, clothes from an outlet
	A.RentStall();
	bool bOutlet = false;
	for (const auto& M : A.Outlets)
	{
		for (const FBrixOutlet& U : M.Value) for (const FString& G : A.OutletSells(M.Key, U.Id)) if (!bOutlet && A.Good(G).Goes == TEXT("wear")) bOutlet = A.BuyAt(M.Key, U.Id, G);
		if (bOutlet) break;
	}
	TestTrue(TEXT("clothes were bought at an outlet"), bOutlet);

	FBrixSaveGame S;   // saved, and the text read back as a new game would read it
	S.Look = D.Default;
	S.Look.Name = TEXT("Ada");
	S.Pos = FVector(12.5, .12, -40.25);
	S.Yaw = 1.25;
	S.Day = A.Day;
	S.Minute = A.Minute;
	S.Econ = BrixEconomySave(A);
	S.Quests = BrixQuestsSave(QA);
	S.Feats.Add(TEXT("first_quest"), 0);
	S.At = FDateTime(2026, 11, 25, 10, 30);
	const FString Saved = S.ToString();
	FBrixSaveGame R;
	if (!TestTrue(TEXT("the save reads back: ") + Error, R.FromString(Saved, D, Error))) return false;
	TestEqual(TEXT("the save reads back as it was written"), R.ToString(), Saved);
	TestTrue(TEXT("the character comes back"), R.Look == S.Look && R.Pos.Equals(S.Pos) && R.Yaw == S.Yaw && R.Feats.FindRef(TEXT("first_quest")) == 0 && R.At == S.At);

	FBrixEconomy B(Life, Lots);
	FBrixQuests QB(QR, B, Res);
	if (!TestTrue(TEXT("the economy loads: ") + Error, BrixEconomyLoad(B, *R.Econ, Error))) return false;
	if (!TestTrue(TEXT("the quests load: ") + Error, BrixQuestsLoad(QB, *R.Quests, Error))) return false;
	TestEqual(TEXT("the money comes back"), Text(BrixEconomySave(B)), Text(BrixEconomySave(A)));
	TestEqual(TEXT("the quests come back"), Text(BrixQuestsSave(QB)), Text(BrixQuestsSave(QA)));
	for (const auto& Bal : A.L.Bal) TestEqual(TEXT("the balance of ") + Bal.Key + TEXT(", from the ledger"), B.L.Balance(Bal.Key), Bal.Value);

	for (int32 k = Half; k < Script.Num(); ++k)   // both played on: the loaded game goes the same way
	{
		RunOp(Script[k]->AsArray(), A, QA, Lots, Res, Shops);
		RunOp(Script[k]->AsArray(), B, QB, Lots, Res, Shops);
	}
	A.Advance(A.Day + 3, 600);   // three midnights: the loan's instalments, the stall's day
	B.Advance(B.Day + 3, 600);
	TestEqual(TEXT("the loaded game plays on as the saved one"), Text(BrixEconomySave(B)), Text(BrixEconomySave(A)));
	TestEqual(TEXT("and its quests too"), Text(BrixQuestsSave(QB)), Text(BrixQuestsSave(QA)));

	FBrixSaveGame Bad;
	TestFalse(TEXT("text that is not a save is refused"), Bad.FromString(TEXT("{\"v\":1}"), D, Error));
	FBrixSaveGame Odd = S;
	Odd.Look.Sliders.Add(TEXT("height"), 2.4);
	TestFalse(TEXT("a save whose character cannot be played is refused"), Bad.FromString(Odd.ToString(), D, Error));
	return true;
}

#endif
