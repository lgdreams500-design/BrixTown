// FURN-022 to FURN-029: buying at the furniture outlet in the Unreal build follows Tools/citysim/furniture.py.
// Game/Data/life.json carries furniture.check_script() and what Python gives for it (build_play.py furniture_data, the
// same fixed run the slice's store.js checks itself against); here the C++ runs it on the same data and must give the
// same answers, op for op, and the same store and money at the end.
#include "BrixCity.h"
#include "BrixEconomy.h"
#include "BrixStore.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	using FVal = TSharedPtr<FJsonValue>;
	FVal JN(double V) { return MakeShared<FJsonValueNumber>(V); }
	FVal JS(const FString& V) { return MakeShared<FJsonValueString>(V); }
	FVal JB(bool V) { return MakeShared<FJsonValueBoolean>(V); }
	FVal JNull() { return MakeShared<FJsonValueNull>(); }
	FVal JA(const TArray<FVal>& A) { return MakeShared<FJsonValueArray>(A); }
	FVal Ids(const TArray<FString>& A) { TArray<FVal> O; for (const FString& S : A) O.Add(JS(S)); return JA(O); }

	bool StoreAlike(const FVal& A, const FVal& B, FString& Where, const FString& At = FString())
	{
		const bool bAn = !A.IsValid() || A->IsNull(), bBn = !B.IsValid() || B->IsNull();
		if (bAn || bBn) { if (bAn != bBn) Where = At + TEXT(" null"); return bAn == bBn; }
		if (A->Type != B->Type) { Where = At + TEXT(" type"); return false; }
		switch (A->Type)
		{
		case EJson::Number: if (FMath::Abs(A->AsNumber() - B->AsNumber()) > 1e-6) { Where = At + FString::Printf(TEXT(" %g vs %g"), A->AsNumber(), B->AsNumber()); return false; } return true;
		case EJson::String: if (A->AsString() != B->AsString()) { Where = At + TEXT(" '") + A->AsString() + TEXT("' vs '") + B->AsString() + TEXT("'"); return false; } return true;
		case EJson::Boolean: if (A->AsBool() != B->AsBool()) { Where = At + TEXT(" bool"); return false; } return true;
		case EJson::Array:
		{
			const TArray<FVal>& X = A->AsArray(), & Y = B->AsArray();
			if (X.Num() != Y.Num()) { Where = At + FString::Printf(TEXT(" length %d vs %d"), X.Num(), Y.Num()); return false; }
			for (int32 i = 0; i < X.Num(); ++i) if (!StoreAlike(X[i], Y[i], Where, At + FString::Printf(TEXT("[%d]"), i))) return false;
			return true;
		}
		case EJson::Object:
		{
			const TSharedPtr<FJsonObject> X = A->AsObject(), Y = B->AsObject();
			if (X->Values.Num() != Y->Values.Num()) { Where = At + TEXT(" keys"); return false; }
			for (const auto& KV : Y->Values) if (!StoreAlike(X->TryGetField(FString(KV.Key)), KV.Value, Where, At + TEXT(".") + FString(KV.Key))) return false;
			return true;
		}
		default: return true;
		}
	}

	TArray<FBrixBasketItem> BasketOf(const FVal& V)
	{
		TArray<FBrixBasketItem> Out;
		for (const FVal& I : V->AsArray())
		{
			const TSharedPtr<FJsonObject> O = I->AsObject();
			FBrixBasketItem It;
			O->TryGetStringField(TEXT("kind"), It.Kind); O->TryGetStringField(TEXT("line"), It.Line); O->TryGetStringField(TEXT("how"), It.How); O->TryGetStringField(TEXT("unit"), It.Unit);
			double D = 0;
			if (O->TryGetNumberField(TEXT("day"), D)) It.Day = (int32)D;
			if (O->TryGetNumberField(TEXT("slot"), D)) It.Slot = (int32)D;
			O->TryGetBoolField(TEXT("assembled"), It.bAssembled);
			const TArray<FVal>* At = nullptr;
			if (O->TryGetArrayField(TEXT("at"), At) && At->Num() >= 3) It.At = FVector((*At)[0]->AsNumber(), (*At)[1]->AsNumber(), (*At)[2]->AsNumber());
			Out.Add(It);
		}
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFURN025StoreMatchesPython, "BrixTown.World.FURN025_the_store_matches_the_python",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FFURN025StoreMatchesPython::RunTest(const FString& Parameters)
{
	FBrixCity City;
	FBrixLifeData Data;
	TArray<FBrixEconLot> Lots;
	FString Error;
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01")));
	const FString Life = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/life.json")));
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(Dir, City, Error))) return false;
	if (!TestTrue(TEXT("life.json loads: ") + Error, LoadBrixLife(Life, Data, Error))) return false;
	if (!TestTrue(TEXT("the lots are in life.json's order: ") + Error, BrixEconLots(City, Data, Lots, Error))) return false;
	const TSharedPtr<FJsonObject>* Fu = nullptr;
	const TSharedPtr<FJsonObject>* Fg = nullptr;
	if (!TestTrue(TEXT("life.json has the furniture store and the list of pieces (run Tools/worldgen/build_play.py)"), Data.Json->TryGetObjectField(TEXT("furniture"), Fu) && Data.Json->TryGetObjectField(TEXT("furnishing"), Fg))) return false;
	FBrixStoreRules R;
	FBrixFurnishRulesD F;
	if (!TestTrue(TEXT("the store's rules read"), R.Load((*Fu)->GetObjectField(TEXT("rules"))) && F.Load(*Fg))) return false;
	const TSharedPtr<FJsonObject> Checks = (*Fu)->GetObjectField(TEXT("checks")), Want = Checks->GetObjectField(TEXT("want"));
	const TArray<FVal>& Script = Checks->GetArrayField(TEXT("script")), & WantR = Want->GetArrayField(TEXT("results"));

	// furniture.script_player(): a new character at 07:20 on day 0 with a current account and 600,000 chips in it
	FBrixEconomy E(Data, Lots);
	E.Advance(0, 440);
	E.NewCharacter();
	E.OpenAccount(TEXT("current"));
	E.L.Post(0, 440, TEXT("Test money"), { { TEXT("sys:start"), -600000 }, { TEXT("p:current"), 600000 } }, E.Why);
	TMap<FString, bool> Held = { { TEXT("u:mine"), true }, { TEXT("u:lost"), true } };
	FBrixStore S(R, F, E);
	S.Holds = [&Held](const FString& U) { return Held.FindRef(U); };
	S.RouteM = [](const FString& U) { return U == TEXT("u:mine") ? 800.0 : 2000.0; };
	S.Found();
	if (!TestEqual(TEXT("as many results as ops"), WantR.Num(), Script.Num())) return false;
	int32 Bad = 0;
	for (int32 k = 0; k < Script.Num(); ++k)
	{
		const TArray<FVal>& Op = Script[k]->AsArray();
		const FString Name = Op[0]->AsString();
		bool bOk = true;
		FVal Res = JNull();
		S.Why.Reset();
		if (Name == TEXT("tag"))
		{
			FBrixStoreTag T;
			bOk = S.Tag(Op[1]->AsString(), Op[2]->AsString(), T);
			if (bOk)
			{
				TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
				O->SetStringField(TEXT("name"), T.Name); O->SetNumberField(TEXT("price"), (double)T.Price);
				O->SetArrayField(TEXT("size"), { JN(T.Size.X), JN(T.Size.Y), JN(T.Size.Z) });
				O->SetStringField(TEXT("line"), T.Line); O->SetField(TEXT("other_lines"), Ids(T.OtherLines));
				O->SetBoolField(TEXT("flat_packed"), T.bFlatPacked); O->SetNumberField(TEXT("boxes"), T.Boxes);
				O->SetNumberField(TEXT("assembly_fee"), (double)T.AssemblyFee); O->SetNumberField(TEXT("assembly_min"), T.AssemblyMin);
				Res = MakeShared<FJsonValueObject>(O);
			}
		}
		else if (Name == TEXT("checkout") || Name == TEXT("home"))
		{
			int64 Total = 0;
			TArray<FString> Got;
			bOk = S.Checkout(BasketOf(Op[1]), Name == TEXT("home"), Total, Got);
			Res = JA({ JN((double)Total), Ids(Got) });
		}
		else if (Name == TEXT("catalogue")) S.bCatalogue = true;
		else if (Name == TEXT("clock"))
		{
			E.Advance((int32)Op[1]->AsNumber(), Op[2]->AsNumber());
			S.Settle((int32)Op[1]->AsNumber() - 1);
			Res = Ids(S.Advance((int32)Op[1]->AsNumber(), Op[2]->AsNumber()));
		}
		else if (Name == TEXT("van"))
		{
			const FBrixStoreOrder* O = S.Orders.FindByPredicate([&](const FBrixStoreOrder& X) { return X.Id == Op[1]->AsString(); });
			if (!TestNotNull(TEXT("the order is there"), O)) return false;
			const FBrixVan V = S.Van(*O, E.Day, E.Minute);
			Res = JA({ JS(V.Phase), JN(FMath::RoundToDouble(V.F * 1e6) / 1e6), JN(V.Landed) });
		}
		else if (Name == TEXT("assemble")) { int32 M = 0; bOk = S.Assemble(Op[1]->AsString(), E.Day, E.Minute, M); Res = JN(M); }
		else if (Name == TEXT("boxed")) { bOk = S.Find(Op[1]->AsString()) != nullptr; if (bOk) Res = JB(S.IsBoxed(Op[1]->AsString(), E.Day, E.Minute)); }
		else if (Name == TEXT("fitter")) { int64 Fee = 0; bOk = S.Fitter(Op[1]->AsString(), (int32)Op[2]->AsNumber(), (int32)Op[3]->AsNumber(), Fee); Res = JN((double)Fee); }
		else if (Name == TEXT("lose")) Held.Add(Op[1]->AsString(), false);
		else { bOk = false; S.Why = TEXT("no such op"); }
		const FVal Got = bOk ? JA({ JS(TEXT("ok")), Res }) : JA({ JS(TEXT("refused")), JS(S.Why) });
		FString Where;
		if (!StoreAlike(Got, WantR[k], Where) && ++Bad <= 8) AddError(FString::Printf(TEXT("op %d (%s): %s"), k, *Name, *Where));
	}
	TestEqual(TEXT("every op gives what Python gives"), Bad, 0);
	// the store and the money at the end
	TArray<FString> Owned;
	for (const FBrixStorePiece& P : S.Owned) Owned.Add(P.Id);
	FString Where;
	TestTrue(TEXT("the belongings: ") + Where, StoreAlike(Ids(Owned), Want->TryGetField(TEXT("owned")), Where));
	TestTrue(TEXT("nothing is on its way that Python has landed: ") + Where, StoreAlike(Ids(S.OnItsWay()), Want->TryGetField(TEXT("on_its_way")), Where));
	TSharedPtr<FJsonObject> Arr = MakeShared<FJsonObject>();
	for (const TPair<FString, TArray<FBrixStorePiece>>& A : S.Arrived) { TArray<FString> I; for (const FBrixStorePiece& P : A.Value) I.Add(P.Id); Arr->SetField(A.Key, Ids(I)); }
	TestTrue(TEXT("what was set down where: ") + Where, StoreAlike(MakeShared<FJsonValueObject>(Arr), Want->TryGetField(TEXT("arrived")), Where));
	TArray<FVal> Notes;
	for (const FBrixStore::FNote& Nt : S.Notes) Notes.Add(JA({ JN(Nt.Day), JN(Nt.Minute), JS(Nt.Words) }));
	TestTrue(TEXT("what the player was told: ") + Where, StoreAlike(JA(Notes), Want->TryGetField(TEXT("notes")), Where));
	TestEqual(TEXT("the company's account"), E.L.Balance(R.Account), (int64)Want->GetNumberField(TEXT("company")));
	TestEqual(TEXT("the player's money"), E.TotalMoney(), (int64)Want->GetNumberField(TEXT("player")));
	TestEqual(TEXT("the money supply"), E.L.Supply(), (int64)Want->GetNumberField(TEXT("supply")));
	TestEqual(TEXT("the company's account in words"), BrixMoney::Format(E.L.Balance(R.Account)), Want->GetStringField(TEXT("words")));

	// the save: the store comes back as it was, and goes on the same (QST-008)
	FBrixEconomy E2(Data, Lots);
	FBrixStore S2(R, F, E2);
	S2.Holds = S.Holds;
	S2.RouteM = S.RouteM;
	S2.Load(S.Save());
	FString W2;
	TestTrue(TEXT("a saved store reads back the same: ") + W2, StoreAlike(MakeShared<FJsonValueObject>(S2.Save()), MakeShared<FJsonValueObject>(S.Save()), W2));

	// FURN-028: a delivered piece stands where it was marked, or at the free spot nearest the way in, or nowhere
	const FVector4f Room(0, 0, 6, 5);
	const TArray<FVector4f> Keep = { FVector4f(2.5f, 0, 3.5f, 1.2f) };
	FBrixLayoutPiece Sofa = BrixFurnishPlan::KitPiece(F, TEXT("k17:sofa"), TEXT("f90"), 0, 0, 0, TEXT("plain")), Table = BrixFurnishPlan::KitPiece(F, TEXT("l_sidetable"), TEXT("f91"), 0, 0, 0, TEXT("plain")), Q1, Q2, Q3;
	TestTrue(TEXT("a piece stands where it was marked"), BrixStoreSetDown(F, Room, Keep, 2.6, {}, Sofa, FVector(3.0, 2.5, 90), FVector2D(3.0, 0.6), Q1) && Q1.X == 3.0 && Q1.Z == 2.5 && Q1.Yaw == 90);
	TestTrue(TEXT("one marked where another stands goes to a free spot"), BrixStoreSetDown(F, Room, Keep, 2.6, { Q1 }, Table, FVector(3.0, 2.5, 0), FVector2D(3.0, 0.6), Q2) && !(Q2.X == 3.0 && Q2.Z == 2.5));
	TestEqual(TEXT("and nothing is wrong with the floor then"), BrixFurnishPlan::Problems(F, Room, Keep, 2.6, { Q1, Q2 }).Num(), 0);
	TestFalse(TEXT("with no room on the floor it waits by the door"), BrixStoreSetDown(F, FVector4f(0, 0, 0.4f, 0.4f), {}, 2.6, {}, Table, {}, FVector2D(0.2, 0.2), Q3));
	return true;
}

#endif
