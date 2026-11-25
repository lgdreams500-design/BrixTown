#include "BrixQuests.h"

#include "BrixDay.h"
#include "Dom/JsonObject.h"

namespace
{
	TArray<FString> Strs(const TSharedPtr<FJsonObject>& O, const TCHAR* F)
	{
		TArray<FString> Out;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O && O->TryGetArrayField(F, A)) for (const auto& V : *A) Out.Add(V->AsString());
		return Out;
	}
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { FString S; if (O) O->TryGetStringField(F, S); return S; }
	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* F, double D = 0) { double V = D; if (O) O->TryGetNumberField(F, V); return V; }
	TMap<FString, FBrixDialogue::FNode> Nodes(const TSharedPtr<FJsonObject>& O, const FString& Qid)
	{
		TMap<FString, FBrixDialogue::FNode> Out;
		const TSharedPtr<FJsonObject>* N = nullptr;
		if (!O || !O->TryGetObjectField(TEXT("nodes"), N)) return Out;
		for (const auto& K : (*N)->Values)
		{
			const TSharedPtr<FJsonObject> Nd = K.Value->AsObject();
			FBrixDialogue::FNode& Node = Out.Add(FString(K.Key.ToView()));
			Node.Say = Str(Nd, TEXT("say"));
			if (Nd->HasField(TEXT("mood"))) Node.Mood = Str(Nd, TEXT("mood"));
			for (const auto& Ov : Nd->GetArrayField(TEXT("options")))
			{
				const TSharedPtr<FJsonObject> Op = Ov->AsObject();
				FBrixDialogue::FOption& X = Node.Options.AddDefaulted_GetRef();
				X.Text = Str(Op, TEXT("text"));
				X.Next = Str(Op, TEXT("next"));   // null: the talk ends
				X.Need = Strs(Op, TEXT("need"));
				X.Act = Strs(Op, TEXT("act"));
				X.Quest = Qid;
			}
		}
		return Out;
	}
	FString S(int32 V) { return FString::FromInt(V); }
	bool InWindow(double M, double A, double B) { return A <= B ? A <= M && M < B : M >= A || M < B; }
}

bool LoadBrixQuests(const TSharedPtr<FJsonObject>& J, FBrixQuestRules& Out, TArray<FBrixQuestResident>& OutRes, FString& OutError)
{
	Out = FBrixQuestRules();
	OutRes.Reset();
	const TSharedPtr<FJsonObject>* Qp = nullptr;
	if (!J || !J->TryGetObjectField(TEXT("quests"), Qp)) { OutError = TEXT("no quests in life.json"); return false; }
	const TSharedPtr<FJsonObject> Q = *Qp;
	Out.First = Str(Q, TEXT("first"));
	for (const auto& K : Q->GetObjectField(TEXT("quests"))->Values)
	{
		const TSharedPtr<FJsonObject> O = K.Value->AsObject();
		FBrixQuestLine& L = Out.Quests.AddDefaulted_GetRef();
		L.Id = FString(K.Key.ToView());
		L.Title = Str(O, TEXT("title")); L.Summary = Str(O, TEXT("summary")); L.Giver = Str(O, TEXT("giver")); L.Start = Str(O, TEXT("start"));
		O->TryGetBoolField(TEXT("starter"), L.bStarter);
		L.Needs = Strs(O, TEXT("needs"));
		for (const auto& Sk : O->GetObjectField(TEXT("stages"))->Values)
		{
			const TSharedPtr<FJsonObject> So = Sk.Value->AsObject();
			FBrixQuestStage& St = L.Stages.Add(FString(Sk.Key.ToView()));
			St.Log = Str(So, TEXT("log")); St.Start = Str(So, TEXT("start")); St.Then = Str(So, TEXT("then"));
			St.Nodes = Nodes(So, L.Id);
			const TSharedPtr<FJsonObject>* Od = nullptr;
			if (So->TryGetObjectField(TEXT("on_done"), Od)) { St.OnDoneGive = Strs(*Od, TEXT("give")); St.OnDoneTake = Strs(*Od, TEXT("take")); }
			const TSharedPtr<FJsonObject> G = So->GetObjectField(TEXT("goal"));
			FBrixGoal& Gl = St.Goal;
			for (const TCHAR* Kind : { TEXT("go"), TEXT("near"), TEXT("have"), TEXT("bank"), TEXT("money"), TEXT("stall"), TEXT("stall_sold"), TEXT("talk") })
				if (G->HasField(Kind)) { Gl.Kind = Kind; break; }   // goal_met()'s order
			if (Gl.Kind == TEXT("near")) { const auto& A = G->GetArrayField(TEXT("near")); Gl.Near = FVector2D(A[0]->AsNumber(), A[1]->AsNumber()); Gl.R = Num(G, TEXT("r")); }
			else if (Gl.Kind == TEXT("money") || Gl.Kind == TEXT("stall_sold")) Gl.Number = Num(G, *Gl.Kind);
			else if (!Gl.Kind.IsEmpty()) G->TryGetStringField(*Gl.Kind, Gl.Value);
			Gl.From = Num(G, TEXT("from"), 0);
			Gl.To = Num(G, TEXT("to"), 1440);
		}
		for (const auto& Ek : O->GetObjectField(TEXT("endings"))->Values)
		{
			const TSharedPtr<FJsonObject> Eo = Ek.Value->AsObject();
			FBrixQuestEnding& En = L.Endings.Add(FString(Ek.Key.ToView()));
			En.Log = Str(Eo, TEXT("log")); En.Pay = (int64)Num(Eo, TEXT("pay")); En.Charge = (int64)Num(Eo, TEXT("charge"));
			En.Give = Strs(Eo, TEXT("give")); En.Take = Strs(Eo, TEXT("take"));
			const TSharedPtr<FJsonObject>* R = nullptr;
			if (Eo->TryGetObjectField(TEXT("rep"), R)) for (const auto& Rv : (*R)->Values) En.Rep.Add({ FString(Rv.Key.ToView()), (int32)Rv.Value->AsNumber() });
		}
		const TSharedPtr<FJsonObject>* Of = nullptr;
		if (O->TryGetObjectField(TEXT("offer"), Of)) { L.OfferLine = Str(*Of, TEXT("line")); L.OfferStart = Str(*Of, TEXT("start")); L.OfferNodes = Nodes(*Of, L.Id); }
	}
	const TSharedPtr<FJsonObject> Jb = Q->GetObjectField(TEXT("jobs"));
	Out.MinRep = (int32)Num(Jb, TEXT("min_rep"), -20);
	for (const auto& K : Jb->GetObjectField(TEXT("kinds"))->Values)
	{
		const TSharedPtr<FJsonObject> O = K.Value->AsObject();
		FBrixJobKind& Kd = Out.Jobs.AddDefaulted_GetRef();
		Kd.Id = FString(K.Key.ToView());
		Kd.Title = Str(O, TEXT("title")); Kd.Offer = Str(O, TEXT("offer")); Kd.Log = Str(O, TEXT("log"));
		Kd.Roles = Strs(O, TEXT("roles")); Kd.Needs = Strs(O, TEXT("needs")); Kd.Items = Strs(O, TEXT("items"));
		const TSharedPtr<FJsonObject>* Rn = nullptr;
		if (O->TryGetObjectField(TEXT("role_needs"), Rn)) for (const auto& V : (*Rn)->Values) Kd.RoleNeeds.Add(FString(V.Key.ToView()), V.Value->AsString());
		Kd.ChanceBp = (int64)Num(O, TEXT("chance_bp")); Kd.Pay = (int64)Num(O, TEXT("pay")); Kd.PayPer100m = (int64)Num(O, TEXT("pay_per_100m"));
		Kd.Hours = (int32)Num(O, TEXT("hours"), 1); Kd.Minutes = (int32)Num(O, TEXT("minutes")); Kd.Rep = (int32)Num(O, TEXT("rep")); Kd.FailRep = (int32)Num(O, TEXT("fail_rep"));
	}
	for (const auto& V : Jb->GetObjectField(TEXT("lines"))->Values) Out.Lines.Add(FString(V.Key.ToView()), V.Value->AsString());
	for (const auto& V : J->GetArrayField(TEXT("residents")))
	{
		const TSharedPtr<FJsonObject> O = V->AsObject();
		OutRes.Add({ Str(O, TEXT("id")), Str(O, TEXT("role")), Str(O, TEXT("home")), Str(O, TEXT("work")) });
	}
	// QST-001: every stage and ending a quest line names is there
	for (const FBrixQuestLine& L : Out.Quests)
	{
		if (!L.Stages.Contains(L.Start)) { OutError = L.Id + TEXT(" starts at a stage it does not have"); return false; }
		for (const auto& St : L.Stages)
			if (!St.Value.Then.IsEmpty() && !St.Value.Then.StartsWith(TEXT("end:")) && !L.Stages.Contains(St.Value.Then.Mid(6))) { OutError = L.Id + TEXT(": no stage ") + St.Value.Then; return false; }
	}
	if (!Out.Find(Out.First)) { OutError = TEXT("the first quest line is not there"); return false; }
	return true;
}

// ---------------------------------------------------------------- the quests

FBrixQuests::FBrixQuests(const FBrixQuestRules& InQ, FBrixEconomy& InE, const TArray<FBrixQuestResident>& InRes)
	: Q(InQ), E(InE), Residents(InRes), Why(InE.Why)
{
	for (int32 n = 0; n < E.Lots.Num(); ++n)
	{
		if (E.Lots[n].Kind == TEXT("shop")) Shops.Add(n);
		if (E.Lots[n].Kind == TEXT("home")) Homes.Add(n);
	}
	for (int32 i = 0; i < Residents.Num(); ++i) RidIndex.Add(Residents[i].Id, i);
}

void FBrixQuests::Note(const FString& Text)
{
	Log.Add({ E.Day, E.Minute, Text });
	if (OnNote) OnNote(Text);
}

void FBrixQuests::RepAdd(const FString& Rid, int32 N)
{
	E.Rep.Add(Rid, FMath::Clamp(Rep(Rid) + N, E.R.RepMin, E.R.RepMax));
}

const FString* FBrixQuests::DoneWith(const FString& Qid) const
{
	for (const auto& D : Done) if (D.Key == Qid) return &D.Value;
	return nullptr;
}

const FBrixQuests::FActive* FBrixQuests::ActiveOf(const FString& Qid) const
{
	for (const auto& A : Active) if (A.Key == Qid) return &A.Value;
	return nullptr;
}

bool FBrixQuests::Met(const FString& Need) const
{
	FString Qid, En;
	if (!Need.Split(TEXT(":"), &Qid, &En)) Qid = Need;
	const FString* D = DoneWith(Qid);
	return D && (En.IsEmpty() || *D == En);
}

bool FBrixQuests::NewGame()
{
	E.NewCharacter();
	return Start(Q.First);
}

bool FBrixQuests::Start(const FString& Qid)
{
	const FBrixQuestLine* L = Q.Find(Qid);
	if (!L) return No(TEXT("no such quest"));
	if (ActiveOf(Qid) || DoneWith(Qid)) return No(TEXT("already taken"));
	Active.Add({ Qid, FActive() });
	Note(TEXT("Started: ") + L->Title);
	return Goto(Qid, TEXT("stage:") + L->Start);
}

bool FBrixQuests::Goto(const FString& Qid, const FString& Target)
{
	FString K, V;
	Target.Split(TEXT(":"), &K, &V);
	if (K == TEXT("end")) return End(Qid, V);
	for (auto& A : Active) if (A.Key == Qid) { A.Value.Stage = V; A.Value.Since = E.StallSoldTotal; return true; }
	FActive A;
	A.Stage = V;
	A.Since = E.StallSoldTotal;
	Active.Add({ Qid, A });
	return true;
}

const FBrixQuestStage* FBrixQuests::Stage(const FString& Qid) const
{
	const FActive* A = ActiveOf(Qid);
	const FBrixQuestLine* L = A ? Q.Find(Qid) : nullptr;
	return L ? L->Stages.Find(A->Stage) : nullptr;
}

TArray<FString> FBrixQuests::Offers(const FString& Rid) const
{
	TArray<FString> Out;
	for (const FBrixQuestLine& L : Q.Quests)
	{
		if (L.Giver != Rid || L.Giver.IsEmpty() || ActiveOf(L.Id) || DoneWith(L.Id)) continue;
		bool bOk = true;
		for (const FString& N : L.Needs) bOk &= Met(N);
		if (bOk) Out.Add(L.Id);
	}
	return Out;
}

TArray<FString> FBrixQuests::TalkingTo(const FString& Rid) const
{
	TArray<FString> Out;
	for (const auto& A : Active) { const FBrixQuestStage* St = Stage(A.Key); if (St && St->Goal.Kind == TEXT("talk") && St->Goal.Value == Rid) Out.Add(A.Key); }
	return Out;
}

bool FBrixQuests::NeedOk(const TArray<FString>& Need) const
{
	for (FString X : Need)
	{
		const bool bNeg = X.StartsWith(TEXT("!"));
		if (bNeg) X.RightChopInline(1);
		FString K, V;
		X.Split(TEXT(":"), &K, &V);
		const bool bOk = K == TEXT("have") ? E.Inv.FindRef(V) > 0 : E.TotalMoney() >= FCString::Atoi64(*V);
		if (bOk == bNeg) return false;
	}
	return true;
}

bool FBrixQuests::Act(const FString& Qid, const TArray<FString>& Acts, const FString& Rid)
{
	for (const FString& A : Acts)
	{
		FString K, V;
		if (!A.Split(TEXT(":"), &K, &V)) K = A;
		bool bOk = true;
		if (K == TEXT("start")) bOk = Start(Qid);
		else if (K == TEXT("stage") || K == TEXT("end")) bOk = Goto(Qid, A);
		else if (K == TEXT("give")) bOk = E.GiveItem(V);
		else if (K == TEXT("take")) bOk = E.TakeItem(V);
		else if (K == TEXT("rep")) RepAdd(Rid, FCString::Atoi(*V));
		if (!bOk) return false;   // refused: the acts after it are not done (the slice's throw)
	}
	return true;
}

bool FBrixQuests::End(const FString& Qid, const FString& Eid)
{
	const FBrixQuestLine* L = Q.Find(Qid);
	const FBrixQuestEnding* En = L ? L->Endings.Find(Eid) : nullptr;
	if (!En) return No(TEXT("no such ending"));
	if (En->Charge && !E.Spend(En->Charge, TEXT("sys:residents"), L->Title)) return false;
	if (En->Pay) E.L.Post(E.Day, E.Minute, TEXT("Reward: ") + L->Title, { { TEXT("sys:rewards"), -En->Pay }, { TEXT("p:cash"), En->Pay } }, Why);   // QST-004
	for (const FString& It : En->Give) E.GiveItem(It);
	for (const FString& It : En->Take) if (E.Inv.FindRef(It)) E.TakeItem(It);
	for (const auto& R : En->Rep) RepAdd(R.Key, R.Value);
	Active.RemoveAll([&](const TPair<FString, FActive>& A) { return A.Key == Qid; });
	Done.Add({ Qid, Eid });
	Note(TEXT("Finished: ") + L->Title + TEXT(". ") + En->Log);
	return true;
}

bool FBrixQuests::GoalMet(const FString& Qid, const FCtx& C) const
{
	const FBrixQuestStage* St = Stage(Qid);
	const FActive* A = ActiveOf(Qid);
	if (!St || !A) return false;
	const FBrixGoal& G = St->Goal;
	if (G.Kind == TEXT("go")) return C.Lot == G.Value && InWindow(C.Minute, G.From, G.To);
	if (G.Kind == TEXT("near")) return C.Pos.IsSet() && FVector2D::Distance(*C.Pos, G.Near) <= G.R && InWindow(C.Minute, G.From, G.To);
	if (G.Kind == TEXT("have")) return E.Inv.FindRef(G.Value) > 0;
	if (G.Kind == TEXT("bank")) return E.HasAccount(TEXT("current"));
	if (G.Kind == TEXT("money")) return E.TotalMoney() >= (int64)G.Number;
	if (G.Kind == TEXT("stall")) return E.Stall.IsSet();
	if (G.Kind == TEXT("stall_sold")) return E.StallSoldTotal - A->Since >= (int32)G.Number;
	return false;
}

void FBrixQuests::Check(const FCtx& C)
{
	bool bMoved = true;
	while (bMoved)
	{
		bMoved = false;
		TArray<FString> Ids;
		for (const auto& A : Active) Ids.Add(A.Key);
		for (const FString& Qid : Ids)
		{
			const FBrixQuestStage* St = Stage(Qid);
			if (!St || St->Goal.Kind == TEXT("talk") || !GoalMet(Qid, C)) continue;
			for (const FString& It : St->OnDoneGive) E.GiveItem(It);
			for (const FString& It : St->OnDoneTake) E.TakeItem(It);
			Goto(Qid, St->Then);
			bMoved = true;
		}
	}
	CheckJob(C);
}

// ---- daily jobs (QST-006)

TOptional<FBrixJob> FBrixQuests::JobOffer(int32 I, int32 Day) const
{
	if (!Residents.IsValidIndex(I)) return {};
	const FBrixQuestResident& R = Residents[I];
	const FBrixJobKind* K = Q.Jobs.FindByPredicate([&](const FBrixJobKind& J) { return J.Roles.Contains(R.Role); });
	if (!K) return {};
	if (BrixSliceHash({ S(I), S(Day), TEXT("job") }) * 10000 >= K->ChanceBp) return {};
	for (const FString& N : K->Needs) if (!Met(N)) return {};
	if (const FString* Rn = K->RoleNeeds.Find(R.Role)) if (!Met(*Rn)) return {};
	if (Rep(R.Id) < Q.MinRep || JobsDone.Contains(S(I) + TEXT("|") + S(Day))) return {};
	FBrixJob O;
	O.Kind = K->Id; O.Giver = I; O.Day = Day; O.Pay = K->Pay; O.Hours = K->Hours;
	if (K->Id == TEXT("parcel"))
	{
		O.Frm = Shops[(int32)FMath::Floor(BrixSliceHash({ S(I), S(Day), TEXT("from") }) * Shops.Num())];
		O.To = Homes[(int32)FMath::Floor(BrixSliceHash({ S(I), S(Day), TEXT("to") }) * Homes.Num())];
		const int64 D = (int64)FMath::TruncToDouble(FVector2D::Distance(E.Lots[O.Frm].Centre(), E.Lots[O.To].Centre()));
		O.Pay = K->Pay + BrixMoney::FloorDiv(K->PayPer100m * D, 100);
	}
	else if (K->Id == TEXT("shopping")) O.Item = K->Items[(int32)FMath::Floor(BrixSliceHash({ S(I), S(Day), TEXT("item") }) * K->Items.Num())];
	else if (K->Id == TEXT("shift"))
	{
		if (R.Work.IsEmpty()) return {};
		O.To = E.LotIndex.FindRef(R.Work, -1);
		O.Minutes = K->Minutes;
	}
	return O;
}

bool FBrixQuests::TakeJob(const FBrixJob& O)
{
	if (Job) return No(TEXT("one job at a time"));
	FBrixJob J = O;
	J.Deadline = E.Day * 1440.0 + E.Minute + O.Hours * 60.0;
	J.Step = O.Kind == TEXT("parcel") ? TEXT("pickup") : TEXT("work");
	J.Worked = 0;
	J.Last.Reset();
	Job = J;
	JobsDone.Add(S(O.Giver) + TEXT("|") + S(O.Day));
	Note(TEXT("Job taken: ") + Q.Job(O.Kind)->Title);
	return true;
}

void FBrixQuests::FinishJob(int64 Extra)
{
	const FBrixJob J = *Job;
	const FBrixJobKind* K = Q.Job(J.Kind);
	if (Extra) E.L.Post(E.Day, E.Minute, TEXT("Paid back"), { { TEXT("sys:residents"), -Extra }, { TEXT("p:cash"), Extra } }, Why);
	E.L.Post(E.Day, E.Minute, TEXT("Job: ") + K->Title, { { TEXT("sys:rewards"), -J.Pay }, { TEXT("p:cash"), J.Pay } }, Why);
	RepAdd(Residents[J.Giver].Id, K->Rep);
	Job.Reset();
	Note(TEXT("Job done: ") + K->Title);
}

void FBrixQuests::FailJob()
{
	const FBrixJob J = *Job;
	const FBrixJobKind* K = Q.Job(J.Kind);
	if (J.Kind == TEXT("parcel") && J.Step == TEXT("deliver") && E.Inv.FindRef(TEXT("parcel"))) E.TakeItem(TEXT("parcel"));
	RepAdd(Residents[J.Giver].Id, K->FailRep);
	Job.Reset();
	Note(TEXT("Job failed: ") + K->Title);
}

bool FBrixQuests::HandOver()
{
	if (!Job || Job->Kind != TEXT("shopping") || !E.Inv.FindRef(Job->Item)) return No(TEXT("nothing to hand over"));
	E.TakeItem(Job->Item);
	FinishJob(E.Item(Job->Item)->Price);
	return true;
}

void FBrixQuests::CheckJob(const FCtx& C)
{
	if (!Job) return;
	FBrixJob& J = *Job;
	const double Now = E.Day * 1440.0 + E.Minute;
	if (Now > J.Deadline) { FailJob(); return; }
	if (J.Kind == TEXT("parcel"))
	{
		if (J.Step == TEXT("pickup") && C.Lot == E.Lots[J.Frm].Id) { E.GiveItem(TEXT("parcel")); J.Step = TEXT("deliver"); }
		else if (J.Step == TEXT("deliver") && C.Lot == E.Lots[J.To].Id && E.Inv.FindRef(TEXT("parcel"))) { E.TakeItem(TEXT("parcel")); FinishJob(); }
	}
	else if (J.Kind == TEXT("shift"))
	{
		const bool bInside = J.To >= 0 && C.Lot == E.Lots[J.To].Id;
		if (bInside && J.Last) J.Worked += Now - *J.Last;
		if (bInside) J.Last = Now; else J.Last.Reset();
		if (J.Worked >= J.Minutes) FinishJob();
	}
}
