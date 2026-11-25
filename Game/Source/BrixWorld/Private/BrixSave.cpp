#include "BrixSave.h"

#include "BrixEconomy.h"
#include "BrixQuests.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	using FVals = TArray<TSharedPtr<FJsonValue>>;
	TSharedPtr<FJsonValue> Num(double V) { return MakeShared<FJsonValueNumber>(V); }
	TSharedPtr<FJsonValue> Str(const FString& V) { return MakeShared<FJsonValueString>(V); }
	TSharedPtr<FJsonValue> Arr(const FVals& V) { return MakeShared<FJsonValueArray>(V); }
	TSharedPtr<FJsonValue> Obj(const TSharedRef<FJsonObject>& V) { return MakeShared<FJsonValueObject>(V); }
	TSharedRef<FJsonObject> Counts(const TMap<FString, int32>& M)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		for (const auto& KV : M) O->SetNumberField(KV.Key, KV.Value);
		return O;
	}
	void ReadCounts(const FJsonObject& S, const TCHAR* K, TMap<FString, int32>& Out)
	{
		Out.Reset();
		const TSharedPtr<FJsonObject>* O;
		if (S.TryGetObjectField(K, O)) for (const auto& KV : (*O)->Values) Out.Add(FString(KV.Key), (int32)KV.Value->AsNumber());
	}
	const FVals& Array(const FJsonObject& S, const TCHAR* K) { static const FVals None; const FVals* A; return S.TryGetArrayField(K, A) ? *A : None; }
	int64 I64(const TSharedPtr<FJsonValue>& V) { return (int64)FMath::RoundToDouble(V->AsNumber()); }

	TSharedRef<FJsonObject> JobJson(const FBrixJob& J)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("kind"), J.Kind); O->SetStringField(TEXT("item"), J.Item); O->SetStringField(TEXT("step"), J.Step);
		O->SetNumberField(TEXT("giver"), J.Giver); O->SetNumberField(TEXT("day"), J.Day); O->SetNumberField(TEXT("hours"), J.Hours);
		O->SetNumberField(TEXT("minutes"), J.Minutes); O->SetNumberField(TEXT("frm"), J.Frm); O->SetNumberField(TEXT("to"), J.To);
		O->SetNumberField(TEXT("pay"), (double)J.Pay); O->SetNumberField(TEXT("deadline"), J.Deadline); O->SetNumberField(TEXT("worked"), J.Worked);
		if (J.Last) O->SetNumberField(TEXT("last"), *J.Last); else O->SetField(TEXT("last"), MakeShared<FJsonValueNull>());
		return O;
	}
	FBrixJob JobOf(const FJsonObject& O)
	{
		FBrixJob J;
		O.TryGetStringField(TEXT("kind"), J.Kind); O.TryGetStringField(TEXT("item"), J.Item); O.TryGetStringField(TEXT("step"), J.Step);
		J.Giver = (int32)O.GetNumberField(TEXT("giver")); J.Day = (int32)O.GetNumberField(TEXT("day")); J.Hours = (int32)O.GetNumberField(TEXT("hours"));
		J.Minutes = (int32)O.GetNumberField(TEXT("minutes")); J.Frm = (int32)O.GetNumberField(TEXT("frm")); J.To = (int32)O.GetNumberField(TEXT("to"));
		J.Pay = (int64)O.GetNumberField(TEXT("pay")); J.Deadline = O.GetNumberField(TEXT("deadline")); J.Worked = O.GetNumberField(TEXT("worked"));
		double Last = 0;
		if (O.TryGetNumberField(TEXT("last"), Last)) J.Last = Last;
		return J;
	}
}

TSharedRef<FJsonObject> BrixEconomySave(const FBrixEconomy& E)
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	FVals Tx;
	for (const FBrixTx& T : E.L.Tx)
	{
		FVals Lines;
		for (const TPair<FString, int64>& L : T.Lines) Lines.Add(Arr({ Str(L.Key), Num((double)L.Value) }));
		Tx.Add(Arr({ Num(T.No), Num(T.Day), Num(T.Minute), Str(T.Memo), Arr(Lines) }));
	}
	S->SetArrayField(TEXT("tx"), Tx);
	S->SetNumberField(TEXT("day"), E.Day);
	S->SetNumberField(TEXT("minute"), E.Minute);
	S->SetNumberField(TEXT("settled"), E.Settled);
	S->SetObjectField(TEXT("inv"), Counts(E.Inv));
	FVals Acc;
	for (const FString& A : E.Accounts) Acc.Add(Str(A));
	S->SetArrayField(TEXT("accounts"), Acc);
	S->SetBoolField(TEXT("welcomed"), E.bWelcomed);
	S->SetBoolField(TEXT("barred"), E.bBarred);
	FVals Loans;
	for (const FBrixLoan& L : E.Loans)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("id"), L.Id); O->SetNumberField(TEXT("day"), L.Day);
		O->SetNumberField(TEXT("principal"), (double)L.Principal); O->SetNumberField(TEXT("total"), (double)L.Total); O->SetNumberField(TEXT("paid"), (double)L.Paid);
		O->SetNumberField(TEXT("fees"), (double)L.Fees); O->SetNumberField(TEXT("fees_paid"), (double)L.FeesPaid); O->SetNumberField(TEXT("taken"), (double)L.Taken);
		FVals Plan;
		for (int64 P : L.Plan) Plan.Add(Num((double)P));
		O->SetArrayField(TEXT("plan"), Plan);
		O->SetNumberField(TEXT("missed"), L.Missed);
		O->SetStringField(TEXT("status"), L.Status);
		Loans.Add(Obj(O));
	}
	S->SetArrayField(TEXT("loans"), Loans);
	TSharedRef<FJsonObject> Trades = MakeShared<FJsonObject>();
	for (const auto& T : E.Trades)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		for (const auto& It : T.Value) O->SetArrayField(It.Key, { Num(It.Value.Key), Num(It.Value.Value) });
		Trades->SetObjectField(T.Key, O);
	}
	S->SetObjectField(TEXT("trades"), Trades);
	S->SetObjectField(TEXT("rep"), Counts(E.Rep));
	if (E.Stall)
	{
		TSharedRef<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetNumberField(TEXT("day"), E.Stall->Day);
		O->SetNumberField(TEXT("done"), E.Stall->Done);
		FVals Slots;
		for (const FBrixStall::FSlot& Sl : E.Stall->Slots) Slots.Add(Arr({ Str(Sl.Item), Num((double)Sl.Price), MakeShared<FJsonValueBoolean>(Sl.bSold) }));
		O->SetArrayField(TEXT("slots"), Slots);
		S->SetObjectField(TEXT("stall"), O);
	}
	else S->SetField(TEXT("stall"), MakeShared<FJsonValueNull>());
	S->SetNumberField(TEXT("stall_sold_total"), E.StallSoldTotal);
	S->SetObjectField(TEXT("clothes"), Counts(E.Clothes));
	S->SetObjectField(TEXT("homeware"), Counts(E.Homeware));
	return S;
}

bool BrixEconomyLoad(FBrixEconomy& E, const FJsonObject& S, FString& OutWhy)
{
	E.L = FBrixLedger();
	for (const TSharedPtr<FJsonValue>& TV : Array(S, TEXT("tx")))   // balances worked out again from the ledger itself
	{
		const FVals& T = TV->AsArray();
		if (T.Num() < 5) { OutWhy = TEXT("a transaction of the save is broken"); return false; }
		FBrixLines Lines;
		for (const TSharedPtr<FJsonValue>& LV : T[4]->AsArray()) { const FVals& L = LV->AsArray(); Lines.Emplace(L[0]->AsString(), I64(L[1])); }
		FString Why;
		if (!E.L.Post((int32)T[1]->AsNumber(), T[2]->AsNumber(), T[3]->AsString(), Lines, Why)) { OutWhy = TEXT("the save's ledger does not balance: ") + Why; return false; }
	}
	E.Day = (int32)S.GetNumberField(TEXT("day"));
	E.Minute = S.GetNumberField(TEXT("minute"));
	E.Settled = (int32)S.GetNumberField(TEXT("settled"));
	ReadCounts(S, TEXT("inv"), E.Inv);
	E.Accounts.Reset();
	for (const TSharedPtr<FJsonValue>& A : Array(S, TEXT("accounts"))) E.Accounts.Add(A->AsString());
	S.TryGetBoolField(TEXT("welcomed"), E.bWelcomed);
	S.TryGetBoolField(TEXT("barred"), E.bBarred);
	E.Loans.Reset();
	for (const TSharedPtr<FJsonValue>& LV : Array(S, TEXT("loans")))
	{
		const TSharedPtr<FJsonObject> O = LV->AsObject();
		FBrixLoan L;
		L.Id = (int32)O->GetNumberField(TEXT("id")); L.Day = (int32)O->GetNumberField(TEXT("day"));
		L.Principal = (int64)O->GetNumberField(TEXT("principal")); L.Total = (int64)O->GetNumberField(TEXT("total")); L.Paid = (int64)O->GetNumberField(TEXT("paid"));
		L.Fees = (int64)O->GetNumberField(TEXT("fees")); L.FeesPaid = (int64)O->GetNumberField(TEXT("fees_paid")); L.Taken = (int64)O->GetNumberField(TEXT("taken"));
		for (const TSharedPtr<FJsonValue>& P : O->GetArrayField(TEXT("plan"))) L.Plan.Add(I64(P));
		L.Missed = (int32)O->GetNumberField(TEXT("missed"));
		O->TryGetStringField(TEXT("status"), L.Status);
		E.Loans.Add(L);
	}
	E.Trades.Reset();
	const TSharedPtr<FJsonObject>* Trades;
	if (S.TryGetObjectField(TEXT("trades"), Trades))
		for (const auto& T : (*Trades)->Values)
		{
			TMap<FString, TPair<int32, int32>>& M = E.Trades.Add(FString(T.Key));
			for (const auto& It : T.Value->AsObject()->Values) { const FVals& BS = It.Value->AsArray(); M.Add(FString(It.Key), TPair<int32, int32>((int32)BS[0]->AsNumber(), (int32)BS[1]->AsNumber())); }
		}
	ReadCounts(S, TEXT("rep"), E.Rep);
	E.Stall.Reset();
	const TSharedPtr<FJsonObject>* St;
	if (S.TryGetObjectField(TEXT("stall"), St))
	{
		FBrixStall X;
		X.Day = (int32)(*St)->GetNumberField(TEXT("day"));
		X.Done = (*St)->GetNumberField(TEXT("done"));
		for (const TSharedPtr<FJsonValue>& SV : (*St)->GetArrayField(TEXT("slots"))) { const FVals& A = SV->AsArray(); FBrixStall::FSlot Sl; Sl.Item = A[0]->AsString(); Sl.Price = I64(A[1]); Sl.bSold = A[2]->AsBool(); X.Slots.Add(Sl); }
		E.Stall = X;
	}
	E.StallSoldTotal = (int32)S.GetNumberField(TEXT("stall_sold_total"));
	ReadCounts(S, TEXT("clothes"), E.Clothes);
	ReadCounts(S, TEXT("homeware"), E.Homeware);
	return true;
}

TSharedRef<FJsonObject> BrixQuestsSave(const FBrixQuests& Q)
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	FVals Active, Done, Log, Jobs;
	for (const auto& A : Q.Active) Active.Add(Arr({ Str(A.Key), Str(A.Value.Stage), Num(A.Value.Since) }));   // in the order taken
	for (const auto& D : Q.Done) Done.Add(Arr({ Str(D.Key), Str(D.Value) }));
	for (const FBrixQuests::FNote& N : Q.Log) Log.Add(Arr({ Num(N.Day), Num(N.Minute), Str(N.Text) }));
	for (const FString& J : Q.JobsDone) Jobs.Add(Str(J));
	S->SetArrayField(TEXT("active"), Active);
	S->SetArrayField(TEXT("done"), Done);
	S->SetArrayField(TEXT("log"), Log);
	if (Q.Job) S->SetObjectField(TEXT("job"), JobJson(*Q.Job)); else S->SetField(TEXT("job"), MakeShared<FJsonValueNull>());
	S->SetArrayField(TEXT("jobs_done"), Jobs);
	return S;
}

bool BrixQuestsLoad(FBrixQuests& Q, const FJsonObject& S, FString& OutWhy)
{
	Q.Active.Reset(); Q.Done.Reset(); Q.Log.Reset(); Q.Job.Reset(); Q.JobsDone.Reset();
	for (const TSharedPtr<FJsonValue>& V : Array(S, TEXT("active")))
	{
		const FVals& A = V->AsArray();
		if (A.Num() < 3) { OutWhy = TEXT("a quest of the save is broken"); return false; }
		FBrixQuests::FActive X;
		X.Stage = A[1]->AsString();
		X.Since = (int32)A[2]->AsNumber();
		Q.Active.Emplace(A[0]->AsString(), X);
	}
	for (const TSharedPtr<FJsonValue>& V : Array(S, TEXT("done"))) { const FVals& A = V->AsArray(); Q.Done.Emplace(A[0]->AsString(), A[1]->AsString()); }
	for (const TSharedPtr<FJsonValue>& V : Array(S, TEXT("log"))) { const FVals& A = V->AsArray(); FBrixQuests::FNote N; N.Day = (int32)A[0]->AsNumber(); N.Minute = A[1]->AsNumber(); N.Text = A[2]->AsString(); Q.Log.Add(N); }
	const TSharedPtr<FJsonObject>* J;
	if (S.TryGetObjectField(TEXT("job"), J)) Q.Job = JobOf(**J);
	for (const TSharedPtr<FJsonValue>& V : Array(S, TEXT("jobs_done"))) Q.JobsDone.Add(V->AsString());
	return true;
}

FString FBrixSaveGame::ToString() const
{
	TSharedRef<FJsonObject> S = MakeShared<FJsonObject>();
	S->SetNumberField(TEXT("v"), Version);
	S->SetObjectField(TEXT("look"), BrixDesignToJson(Look));
	TSharedRef<FJsonObject> Pl = MakeShared<FJsonObject>();
	Pl->SetNumberField(TEXT("x"), Pos.X); Pl->SetNumberField(TEXT("y"), Pos.Y); Pl->SetNumberField(TEXT("z"), Pos.Z); Pl->SetNumberField(TEXT("yaw"), Yaw);
	S->SetObjectField(TEXT("pl"), Pl);
	TSharedRef<FJsonObject> Clock = MakeShared<FJsonObject>();
	Clock->SetNumberField(TEXT("day"), Day); Clock->SetNumberField(TEXT("m"), Minute);
	S->SetObjectField(TEXT("clock"), Clock);
	if (Econ) S->SetObjectField(TEXT("econ"), Econ);
	if (Quests) S->SetObjectField(TEXT("quests"), Quests);
	if (Estate) S->SetObjectField(TEXT("estate"), Estate);
	if (Remodels) S->SetObjectField(TEXT("remodels"), Remodels);
	if (Plans) S->SetObjectField(TEXT("plans"), Plans);
	S->SetArrayField(TEXT("outfits"), Outfits);
	S->SetObjectField(TEXT("feats"), Counts(Feats));
	S->SetStringField(TEXT("at"), At.ToIso8601());
	FString Out;
	const TSharedRef<TJsonWriter<>> W = TJsonWriterFactory<>::Create(&Out);
	FJsonSerializer::Serialize(S, W);
	return Out;
}

bool FBrixSaveGame::FromString(const FString& Text, const FBrixDesignerRules& D, FString& OutWhy)
{
	TSharedPtr<FJsonObject> S;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), S) || !S.IsValid()) { OutWhy = TEXT("the save is not JSON"); return false; }
	double V = 0;
	if (!S->TryGetNumberField(TEXT("v"), V) || (int32)V != 1) { OutWhy = TEXT("the save is of another version"); return false; }
	Version = (int32)V;
	const TSharedPtr<FJsonObject>* O;
	Look = FBrixDesign();
	if (!S->TryGetObjectField(TEXT("look"), O) || !BrixDesignFromJson(**O, Look) || BrixLookProblems(D, Look).Num()) { OutWhy = TEXT("the save's character cannot be played"); return false; }
	if (S->TryGetObjectField(TEXT("pl"), O)) { Pos = FVector((*O)->GetNumberField(TEXT("x")), (*O)->GetNumberField(TEXT("y")), (*O)->GetNumberField(TEXT("z"))); Yaw = (*O)->GetNumberField(TEXT("yaw")); }
	if (S->TryGetObjectField(TEXT("clock"), O)) { Day = (int32)(*O)->GetNumberField(TEXT("day")); Minute = (*O)->GetNumberField(TEXT("m")); }
	Econ = S->TryGetObjectField(TEXT("econ"), O) ? *O : nullptr;
	Quests = S->TryGetObjectField(TEXT("quests"), O) ? *O : nullptr;
	Estate = S->TryGetObjectField(TEXT("estate"), O) ? *O : nullptr;
	Remodels = S->TryGetObjectField(TEXT("remodels"), O) ? *O : nullptr;
	Plans = S->TryGetObjectField(TEXT("plans"), O) ? *O : nullptr;
	if (!Econ || !Quests) { OutWhy = TEXT("the save has no money or quests"); return false; }
	Outfits = Array(*S, TEXT("outfits"));
	ReadCounts(*S, TEXT("feats"), Feats);
	FString When;
	if (!S->TryGetStringField(TEXT("at"), When) || !FDateTime::ParseIso8601(*When, At)) At = FDateTime();
	return true;
}
