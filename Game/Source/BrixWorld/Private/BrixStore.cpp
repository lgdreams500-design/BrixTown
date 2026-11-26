#include "BrixStore.h"
#include "BrixEconomy.h"

#include "Dom/JsonObject.h"

namespace
{
	double NumOf(const TSharedPtr<FJsonObject>& O, const TCHAR* K, double D = 0) { double V = D; if (O) O->TryGetNumberField(K, V); return V; }
	TSharedPtr<FJsonObject> PieceJson(const FBrixStorePiece& P)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), P.Id); O->SetStringField(TEXT("kind"), P.Kind); O->SetStringField(TEXT("line"), P.Line); O->SetStringField(TEXT("name"), P.Name);
		O->SetNumberField(TEXT("w"), P.W); O->SetNumberField(TEXT("d"), P.D); O->SetNumberField(TEXT("h"), P.H);
		O->SetBoolField(TEXT("rug"), P.bRug); O->SetBoolField(TEXT("boxed"), P.bBoxed);
		if (P.Ready) O->SetNumberField(TEXT("ready"), *P.Ready); else O->SetField(TEXT("ready"), MakeShared<FJsonValueNull>());
		if (P.bFitted) O->SetBoolField(TEXT("fitted"), true);
		if (P.At) O->SetArrayField(TEXT("at"), { MakeShared<FJsonValueNumber>(P.At->X), MakeShared<FJsonValueNumber>(P.At->Y), MakeShared<FJsonValueNumber>(P.At->Z) });
		if (!P.StoodLot.IsEmpty()) O->SetArrayField(TEXT("stood"), { MakeShared<FJsonValueString>(P.StoodLot), MakeShared<FJsonValueNumber>(P.StoodFloor) });
		return O;
	}
	FBrixStorePiece PieceFrom(const TSharedPtr<FJsonObject>& O)
	{
		FBrixStorePiece P;
		O->TryGetStringField(TEXT("id"), P.Id); O->TryGetStringField(TEXT("kind"), P.Kind); O->TryGetStringField(TEXT("line"), P.Line); O->TryGetStringField(TEXT("name"), P.Name);
		O->TryGetNumberField(TEXT("w"), P.W); O->TryGetNumberField(TEXT("d"), P.D); O->TryGetNumberField(TEXT("h"), P.H);
		O->TryGetBoolField(TEXT("rug"), P.bRug); O->TryGetBoolField(TEXT("boxed"), P.bBoxed); O->TryGetBoolField(TEXT("fitted"), P.bFitted);
		double Rd = 0;
		if (O->TryGetNumberField(TEXT("ready"), Rd)) P.Ready = Rd;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (O->TryGetArrayField(TEXT("at"), A) && A->Num() >= 3) P.At = FVector((*A)[0]->AsNumber(), (*A)[1]->AsNumber(), (*A)[2]->AsNumber());
		if (O->TryGetArrayField(TEXT("stood"), A) && A->Num() >= 2) { P.StoodLot = (*A)[0]->AsString(); P.StoodFloor = (int32)(*A)[1]->AsNumber(); }
		return P;
	}
}

bool FBrixStoreRules::Load(const TSharedPtr<FJsonObject>& J)
{
	if (!J) return false;
	const TSharedPtr<FJsonObject>* C = nullptr;
	if (!J->TryGetObjectField(TEXT("company"), C)) return false;
	(*C)->TryGetStringField(TEXT("id"), CompanyId); (*C)->TryGetStringField(TEXT("name"), CompanyName); (*C)->TryGetStringField(TEXT("account"), Account);
	Float = (int64)NumOf(*C, TEXT("float")); WagesBp = (int32)NumOf(*C, TEXT("wages_bp"));
	PriceLevelBp = (int32)NumOf(J, TEXT("price_level_bp"), 10000);
	InstantFee = (int64)NumOf(J, TEXT("instant_fee"));
	const TSharedPtr<FJsonObject> P = J->GetObjectField(TEXT("flat_pack")), D = J->GetObjectField(TEXT("delivery")), A = J->GetObjectField(TEXT("assembly"));
	PackFromM3 = NumOf(P, TEXT("from_m3"), .3); PacksTo = NumOf(P, TEXT("packs_to"), 6); MostBoxes = (int32)NumOf(P, TEXT("most_boxes"), 6);
	const TArray<TSharedPtr<FJsonValue>>& Bx = P->GetArrayField(TEXT("box"));
	if (Bx.Num() == 3) Box = FVector3d(Bx[0]->AsNumber(), Bx[1]->AsNumber(), Bx[2]->AsNumber());
	FeeByM3.Reset();
	for (const TSharedPtr<FJsonValue>& V : D->GetArrayField(TEXT("fee_by_m3")))
	{
		const TArray<TSharedPtr<FJsonValue>>& B = V->AsArray();
		FeeByM3.Add({ B[0]->IsNull() ? TOptional<double>() : TOptional<double>(B[0]->AsNumber()), (int64)B[1]->AsNumber() });
	}
	Slots.Reset();
	for (const TSharedPtr<FJsonValue>& V : D->GetArrayField(TEXT("slots"))) Slots.Add(FIntPoint((int32)V->AsArray()[0]->AsNumber(), (int32)V->AsArray()[1]->AsNumber()));
	FirstDayAfter = (int32)NumOf(D, TEXT("first_day_after"), 1); DaysAhead = (int32)NumOf(D, TEXT("days_ahead"), 7);
	VanMPerMin = NumOf(D, TEXT("van_m_per_min"), 400); CarryMin = NumOf(D, TEXT("carry_min"), 4);
	FeeBp = (int32)NumOf(A, TEXT("fee_bp")); LeastFee = (int64)NumOf(A, TEXT("least_fee")); MinPerM3 = NumOf(A, TEXT("min_per_m3"), 30); LeastMin = (int32)NumOf(A, TEXT("least_min"), 10);
	return !Account.IsEmpty() && Slots.Num() && FeeByM3.Num() && VanMPerMin > 0;
}

double FBrixStore::Vol(const FString& Kind) const { const FBrixFurnishRulesD::FPiece* P = F.Pieces.Find(Kind); return P ? (double)P->Size.X * (double)P->Size.Y * (double)P->Size.Z : 0; }

bool FBrixStore::Sells(const FString& Kind) const { const FBrixFurnishRulesD::FPiece* P = F.Pieces.Find(Kind); return P && P->From.IsEmpty(); }

int64 FBrixStore::Price(const FString& Kind) const { const FBrixFurnishRulesD::FPiece* P = F.Pieces.Find(Kind); return P ? FMath::Max<int64>(1, (P->Price * R.PriceLevelBp + 5000) / 10000) : 0; }

bool FBrixStore::FlatPacked(const FString& Kind) const { const FBrixFurnishRulesD::FPiece* P = F.Pieces.Find(Kind); return P && !P->bRug && Vol(Kind) >= R.PackFromM3; }

int32 FBrixStore::Boxes(const FString& Kind, FVector3d* OutStack) const
{
	if (!FlatPacked(Kind)) return 0;
	const int32 n = FMath::Min(R.MostBoxes, FMath::Max(1, (int32)FMath::CeilToDouble(Vol(Kind) / R.PacksTo / (R.Box.X * R.Box.Y * R.Box.Z) - 1e-9)));
	if (OutStack) *OutStack = FVector3d(R.Box.X, R.Box.Y, FMath::RoundToDouble(R.Box.Z * n * 100) / 100);
	return n;
}

bool FBrixStore::Tag(const FString& Kind, const FString& Line, FBrixStoreTag& Out)
{
	if (!Sells(Kind)) return No(TEXT("the outlet does not sell that"));
	if (!F.Lines.Contains(Line)) return No(TEXT("there is no such line"));
	const FBrixFurnishRulesD::FPiece& P = F.Pieces[Kind];
	Out = FBrixStoreTag();
	Out.Name = P.Name; Out.Price = Price(Kind); Out.Size = P.Size; Out.Line = Line;
	for (const FString& X : F.Lines) if (X != Line) Out.OtherLines.Add(X);
	Out.bFlatPacked = FlatPacked(Kind); Out.Boxes = Boxes(Kind);
	Out.AssemblyFee = Out.bFlatPacked ? AssemblyFee(Kind) : 0; Out.AssemblyMin = Out.bFlatPacked ? AssemblyMin(Kind) : 0;
	return true;
}

int64 FBrixStore::DeliveryFee(const TArray<FString>& Kinds) const
{
	double v = 0;
	for (const FString& K : Kinds) v += Vol(K);
	for (const TPair<TOptional<double>, int64>& B : R.FeeByM3) if (!B.Key || v <= *B.Key + 1e-9) return B.Value;
	return R.FeeByM3.Num() ? R.FeeByM3.Last().Value : 0;
}

int64 FBrixStore::AssemblyFee(const FString& Kind) const { return FMath::Max<int64>(R.LeastFee, (Price(Kind) * R.FeeBp + 5000) / 10000); }

int32 FBrixStore::AssemblyMin(const FString& Kind) const { return FMath::Max(R.LeastMin, (int32)FMath::FloorToDouble(Vol(Kind) * R.MinPerM3 + .5)); }

TArray<FIntPoint> FBrixStore::SlotsNow() const
{
	TArray<FIntPoint> Out;
	for (int32 d = E.Day + R.FirstDayAfter; d <= E.Day + R.DaysAhead; ++d) for (int32 k = 0; k < R.Slots.Num(); ++k) Out.Add(FIntPoint(d, k));
	return Out;
}

namespace
{
	struct FRun { FString Unit; int32 Day = 0, Slot = 0; TArray<int32> Items; };
	bool QuoteRuns(FBrixStore& S, const TArray<FBrixBasketItem>& Basket, bool bAtHome, int64& Total, TArray<FBrixFurnLine>& Lines, TArray<FRun>& Runs)
	{
		Total = 0; Lines.Reset(); Runs.Reset();
		auto No = [&S](const TCHAR* W) { S.Why = W; return false; };
		if (!Basket.Num()) return No(TEXT("the basket is empty"));
		if (bAtHome && !S.bCatalogue) return No(TEXT("you have no catalogue to order from"));
		const TArray<FIntPoint> Ok = S.SlotsNow();
		for (int32 i = 0; i < Basket.Num(); ++i)
		{
			const FBrixBasketItem& It = Basket[i];
			if (!S.Sells(It.Kind)) return No(TEXT("the outlet does not sell that"));
			if (!S.F.Lines.Contains(It.Line)) return No(TEXT("there is no such line"));
			const FString Name = S.F.Pieces[It.Kind].Name;
			Lines.Add({ Name, 1, S.Price(It.Kind) });
			if (It.How == TEXT("instant"))
			{
				if (bAtHome) return No(TEXT("ordering from home books delivery only"));
				if (S.R.InstantFee) Lines.Add({ TEXT("Take it now: ") + Name, 1, S.R.InstantFee });
			}
			else if (It.How == TEXT("delivery"))
			{
				if (It.Unit.IsEmpty() || !S.Holds || !S.Holds(It.Unit)) return No(TEXT("that is not a home of yours to deliver to"));
				if (!Ok.Contains(FIntPoint(It.Day, It.Slot))) return No(TEXT("that delivery slot cannot be booked"));
				if (It.bAssembled && S.FlatPacked(It.Kind)) Lines.Add({ TEXT("Put together: ") + Name, 1, S.AssemblyFee(It.Kind) });
				FRun* Rn = Runs.FindByPredicate([&](const FRun& X) { return X.Unit == It.Unit && X.Day == It.Day && X.Slot == It.Slot; });
				if (!Rn) { Rn = &Runs.AddDefaulted_GetRef(); Rn->Unit = It.Unit; Rn->Day = It.Day; Rn->Slot = It.Slot; }
				Rn->Items.Add(i);
			}
			else return No(TEXT("each piece is taken now or delivered"));
		}
		Runs.StableSort([](const FRun& A, const FRun& B) { return A.Unit != B.Unit ? A.Unit < B.Unit : A.Day != B.Day ? A.Day < B.Day : A.Slot < B.Slot; });
		for (const FRun& Rn : Runs)
		{
			TArray<FString> Kinds;
			for (const int32 i : Rn.Items) Kinds.Add(Basket[i].Kind);
			Lines.Add({ FString::Printf(TEXT("Delivery, day %d"), Rn.Day + 1), (double)Rn.Items.Num(), S.DeliveryFee(Kinds) });
		}
		for (const FBrixFurnLine& L : Lines) Total += L.Chips;
		return true;
	}
}

bool FBrixStore::Quote(const TArray<FBrixBasketItem>& Basket, bool bAtHome, int64& OutTotal, TArray<FBrixFurnLine>& OutLines)
{
	TArray<FRun> Runs;
	return QuoteRuns(*this, Basket, bAtHome, OutTotal, OutLines, Runs);
}

bool FBrixStore::Checkout(const TArray<FBrixBasketItem>& Basket, bool bAtHome, int64& OutTotal, TArray<FString>& OutIds)
{
	TArray<FBrixFurnLine> Lines;
	TArray<FRun> Runs;
	OutIds.Reset();
	if (!QuoteRuns(*this, Basket, bAtHome, OutTotal, Lines, Runs)) return false;
	if (!E.Spend(OutTotal, R.Account, FString::Printf(TEXT("%s: %d piece%s"), *R.CompanyName, Basket.Num(), Basket.Num() == 1 ? TEXT("") : TEXT("s")))) { Why = E.Why; return false; }
	TArray<FBrixStorePiece> Made;
	for (const FBrixBasketItem& It : Basket)
	{
		const FBrixFurnishRulesD::FPiece& P = F.Pieces[It.Kind];
		FBrixStorePiece Q;
		Q.Id = FString::Printf(TEXT("f%d"), ++N); Q.Kind = It.Kind; Q.Line = It.Line; Q.Name = P.Name; Q.W = P.Size.X; Q.D = P.Size.Y; Q.H = P.Size.Z; Q.bRug = P.bRug;
		Made.Add(Q);
		OutIds.Add(Q.Id);
		if (It.How == TEXT("instant")) Owned.Add(Q);
	}
	for (const FRun& Rn : Runs)
	{
		FBrixStoreOrder O;
		O.Id = FString::Printf(TEXT("o%d"), Orders.Num() + 1); O.Unit = Rn.Unit; O.Day = Rn.Day; O.Slot = Rn.Slot;
		for (const int32 i : Rn.Items)
		{
			FBrixStorePiece Q = Made[i];
			Q.bBoxed = FlatPacked(Q.Kind);
			Q.bFitted = Basket[i].bAssembled && Q.bBoxed;   // the fitter comes with the van and puts it together in place
			Q.At = Basket[i].At;
			O.Pieces.Add(Q);
		}
		Orders.Add(O);
	}
	return true;
}

FBrixStore::FLegs FBrixStore::Legs(const FBrixStoreOrder& O) const
{
	FLegs L;
	L.Drive = (RouteM ? RouteM(O.Unit) : 600.0) / R.VanMPerMin;
	double t = L.Drive;
	for (const FBrixStorePiece& P : O.Pieces)
	{
		t += R.CarryMin;
		if (P.bFitted) t += AssemblyMin(P.Kind);
		L.Down.Add(t);
	}
	L.Leave = t;
	L.Back = t + L.Drive;
	return L;
}

FBrixVan FBrixStore::Van(const FBrixStoreOrder& O, int32 Day, double Minute) const
{
	// a piece already set down was carried in at its own time: its fitter's time no longer shows in the pieces, so the run's
	// times are worked out from the pieces as they were booked (the first Landed pieces count as carried)
	const double t = (Day - O.Day) * 1440.0 + Minute - (R.Slots.IsValidIndex(O.Slot) ? R.Slots[O.Slot].X : 0);
	const FLegs L = Legs(O);
	FBrixVan V;
	for (const double x : L.Down) V.Landed += t >= x - 1e-9;
	if (t < 0) { V.Phase = TEXT("booked"); V.F = 0; V.Landed = 0; }
	else if (t < L.Drive) { V.Phase = TEXT("out"); V.F = L.Drive > 0 ? t / L.Drive : 1; V.Landed = 0; }
	else if (t < L.Leave) { V.Phase = TEXT("at the door"); V.F = L.Leave > L.Drive ? (t - L.Drive) / (L.Leave - L.Drive) : 1; }
	else if (t < L.Back) { V.Phase = TEXT("back"); V.F = L.Back > L.Leave ? (t - L.Leave) / (L.Back - L.Leave) : 1; }
	else { V.Phase = TEXT("done"); V.F = 1; }
	return V;
}

TArray<FBrixStorePiece>* FBrixStore::ArrivedAt(const FString& Unit, bool bAdd)
{
	for (TPair<FString, TArray<FBrixStorePiece>>& A : Arrived) if (A.Key == Unit) return &A.Value;
	if (!bAdd) return nullptr;
	Arrived.Add(TPair<FString, TArray<FBrixStorePiece>>(Unit, TArray<FBrixStorePiece>()));
	return &Arrived.Last().Value;
}

TArray<FString> FBrixStore::Advance(int32 Day, double Minute)
{
	TArray<FString> New;
	for (FBrixStoreOrder& O : Orders)
	{
		const int32 n = Van(O, Day, Minute).Landed;
		while (O.Landed < n)
		{
			FBrixStorePiece P = O.Pieces[O.Landed];
			if (P.bFitted) P.bBoxed = false;
			// (the order keeps the piece as it was booked, so the van's times stay what they were)
			++O.Landed;
			FBrixStorePiece Q = P;
			Q.bFitted = false;
			if (Holds && Holds(O.Unit)) ArrivedAt(O.Unit, true)->Add(Q);
			else
			{
				Owned.Add(Q);
				Notes.Add({ Day, Minute, FString::Printf(TEXT("Your %s could not be delivered: %s is no longer yours. It is with your belongings."), *Q.Name.ToLower(), *O.Unit) });
			}
			New.Add(Q.Id);
		}
	}
	return New;
}

TArray<FString> FBrixStore::OnItsWay() const
{
	TArray<FString> Out;
	for (const FBrixStoreOrder& O : Orders) for (int32 k = O.Landed; k < O.Pieces.Num(); ++k) Out.Add(O.Pieces[k].Id);
	return Out;
}

void FBrixStore::MoveOut(const FString& Unit)
{
	if (TArray<FBrixStorePiece>* A = ArrivedAt(Unit)) Owned.Append(*A);
	Arrived.RemoveAll([&](const TPair<FString, TArray<FBrixStorePiece>>& X) { return X.Key == Unit; });
}

void FBrixStore::RemoveArrived(const FString& Unit, const FString& Id)
{
	if (TArray<FBrixStorePiece>* A = ArrivedAt(Unit)) A->RemoveAll([&](const FBrixStorePiece& P) { return P.Id == Id; });
	Arrived.RemoveAll([](const TPair<FString, TArray<FBrixStorePiece>>& X) { return !X.Value.Num(); });
}

FBrixStorePiece* FBrixStore::Find(const FString& Id, FString* OutWhere)
{
	for (FBrixStorePiece& P : Owned) if (P.Id == Id) { if (OutWhere) *OutWhere = TEXT("belongings"); return &P; }
	TArray<FString> Units;
	for (const TPair<FString, TArray<FBrixStorePiece>>& A : Arrived) Units.Add(A.Key);
	Units.Sort();
	for (const FString& U : Units) for (FBrixStorePiece& P : *ArrivedAt(U)) if (P.Id == Id) { if (OutWhere) *OutWhere = U; return &P; }
	Why = TEXT("you have no such piece here");
	return nullptr;
}

bool FBrixStore::IsBoxed(const FString& Id, int32 Day, double Minute)
{
	const FBrixStorePiece* P = Find(Id);
	return P && P->bBoxed && !(P->Ready && Day * 1440.0 + Minute >= *P->Ready - 1e-9);
}

bool FBrixStore::AsPlaced(const FString& Id, int32 Day, double Minute, FBrixLayoutPiece& Out, bool& bOutBoxed)
{
	const FBrixStorePiece* P = Find(Id);
	if (!P) return false;
	Out = FBrixLayoutPiece();
	Out.Id = P->Id; Out.Kind = P->Kind; Out.Line = P->Line; Out.Name = P->Name; Out.W = P->W; Out.D = P->D; Out.H = P->H; Out.bRug = P->bRug;
	bOutBoxed = IsBoxed(Id, Day, Minute);
	if (bOutBoxed)
	{
		FVector3d St;
		Boxes(P->Kind, &St);
		Out.Name = P->Name + TEXT(" (in its boxes)"); Out.W = St.X; Out.D = St.Y; Out.H = St.Z; Out.bBoxed = true;
	}
	return true;
}

bool FBrixStore::Assemble(const FString& Id, int32 Day, double Minute, int32& OutMinutes)
{
	FString Where;
	FBrixStorePiece* P = Find(Id, &Where);
	if (!P) return false;
	if (!IsBoxed(Id, Day, Minute)) return No(TEXT("that piece is not in its boxes"));
	if (Where == TEXT("belongings")) return No(TEXT("put it down in a home of yours first"));
	if (P->Ready) return No(TEXT("that piece is being put together"));
	OutMinutes = AssemblyMin(P->Kind);
	P->Ready = Day * 1440.0 + Minute + OutMinutes;
	return true;
}

bool FBrixStore::Fitter(const FString& Id, int32 Day, int32 Slot, int64& OutFee)
{
	FString Where;
	FBrixStorePiece* P = Find(Id, &Where);
	if (!P) return false;
	if (!IsBoxed(Id, E.Day, E.Minute)) return No(TEXT("that piece is not in its boxes"));
	if (Where == TEXT("belongings")) return No(TEXT("put it down in a home of yours first"));
	if (P->Ready) return No(TEXT("that piece is being put together"));
	if (!SlotsNow().Contains(FIntPoint(Day, Slot))) return No(TEXT("that slot cannot be booked"));
	OutFee = AssemblyFee(P->Kind);
	if (!E.Spend(OutFee, R.Account, FString::Printf(TEXT("%s: a fitter for the %s"), *R.CompanyName, *P->Name.ToLower()))) { Why = E.Why; return false; }
	P->Ready = Day * 1440.0 + R.Slots[Slot].X + AssemblyMin(P->Kind);
	return true;
}

void FBrixStore::Found(int32 Day, double Minute)
{
	FString W;
	if (R.Float && !E.L.Balance(R.Account)) E.L.Post(Day, Minute, R.CompanyName + TEXT(" opens"), { { TEXT("sys:founding"), -R.Float }, { R.Account, R.Float } }, W);
}

void FBrixStore::Settle(int32 Day)
{
	for (int32 d = Settled + 1; d <= Day; ++d)
	{
		const int64 Over = E.L.Balance(R.Account) - R.Float;
		if (Over > 0)
		{
			const int64 Wages = Over * R.WagesBp / 10000;
			FString W;
			E.L.Post(d + 1, 0, R.CompanyName + TEXT(": wages and stock"), { { R.Account, -Over }, { TEXT("sys:wages"), Wages }, { TEXT("sys:spend"), Over - Wages } }, W);
		}
		Settled = d;
	}
}

TSharedPtr<FJsonObject> FBrixStore::Save() const
{
	TSharedPtr<FJsonObject> S = MakeShared<FJsonObject>();
	auto List = [](const TArray<FBrixStorePiece>& Ps) { TArray<TSharedPtr<FJsonValue>> A; for (const FBrixStorePiece& P : Ps) A.Add(MakeShared<FJsonValueObject>(PieceJson(P))); return A; };
	S->SetNumberField(TEXT("n"), N);
	S->SetArrayField(TEXT("owned"), List(Owned));
	TArray<TSharedPtr<FJsonValue>> Os;
	for (const FBrixStoreOrder& O : Orders)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("id"), O.Id); J->SetStringField(TEXT("unit"), O.Unit); J->SetNumberField(TEXT("day"), O.Day); J->SetNumberField(TEXT("slot"), O.Slot);
		J->SetNumberField(TEXT("landed"), O.Landed); J->SetArrayField(TEXT("pieces"), List(O.Pieces));
		Os.Add(MakeShared<FJsonValueObject>(J));
	}
	S->SetArrayField(TEXT("orders"), Os);
	TArray<TSharedPtr<FJsonValue>> Ar;   // (a list, so the units keep their order)
	for (const TPair<FString, TArray<FBrixStorePiece>>& A : Arrived)
	{
		TSharedPtr<FJsonObject> J = MakeShared<FJsonObject>();
		J->SetStringField(TEXT("unit"), A.Key); J->SetArrayField(TEXT("pieces"), List(A.Value));
		Ar.Add(MakeShared<FJsonValueObject>(J));
	}
	S->SetArrayField(TEXT("arrived"), Ar);
	TArray<TSharedPtr<FJsonValue>> Ns;
	for (const FNote& Nt : Notes) Ns.Add(MakeShared<FJsonValueArray>(TArray<TSharedPtr<FJsonValue>>{ MakeShared<FJsonValueNumber>(Nt.Day), MakeShared<FJsonValueNumber>(Nt.Minute), MakeShared<FJsonValueString>(Nt.Words) }));
	S->SetArrayField(TEXT("notes"), Ns);
	S->SetBoolField(TEXT("has_catalogue"), bCatalogue);
	S->SetNumberField(TEXT("settled"), Settled);
	return S;
}

void FBrixStore::Load(const TSharedPtr<FJsonObject>& S)
{
	if (!S) return;
	auto List = [](const TArray<TSharedPtr<FJsonValue>>& A) { TArray<FBrixStorePiece> Ps; for (const TSharedPtr<FJsonValue>& V : A) Ps.Add(PieceFrom(V->AsObject())); return Ps; };
	N = (int32)NumOf(S, TEXT("n")); Settled = (int32)NumOf(S, TEXT("settled"), -1);
	S->TryGetBoolField(TEXT("has_catalogue"), bCatalogue);
	Owned = List(S->GetArrayField(TEXT("owned")));
	Orders.Reset();
	for (const TSharedPtr<FJsonValue>& V : S->GetArrayField(TEXT("orders")))
	{
		const TSharedPtr<FJsonObject> J = V->AsObject();
		FBrixStoreOrder O;
		O.Id = J->GetStringField(TEXT("id")); O.Unit = J->GetStringField(TEXT("unit")); O.Day = (int32)NumOf(J, TEXT("day")); O.Slot = (int32)NumOf(J, TEXT("slot")); O.Landed = (int32)NumOf(J, TEXT("landed"));
		O.Pieces = List(J->GetArrayField(TEXT("pieces")));
		Orders.Add(O);
	}
	Arrived.Reset();
	for (const TSharedPtr<FJsonValue>& V : S->GetArrayField(TEXT("arrived"))) Arrived.Add(TPair<FString, TArray<FBrixStorePiece>>(V->AsObject()->GetStringField(TEXT("unit")), List(V->AsObject()->GetArrayField(TEXT("pieces")))));
	Notes.Reset();
	for (const TSharedPtr<FJsonValue>& V : S->GetArrayField(TEXT("notes"))) { const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray(); Notes.Add({ (int32)A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsString() }); }
}

bool BrixStoreSetDown(const FBrixFurnishRulesD& F, const FVector4f& Room, const TArray<FVector4f>& Keep, double Ceiling, const FBrixLayout& Layout,
	const FBrixLayoutPiece& Piece, const TOptional<FVector>& At, const FVector2D& Entry, FBrixLayoutPiece& Out)
{
	TSet<FString> Was;
	for (const TPair<FString, FString>& Q : BrixFurnishPlan::Problems(F, Room, Keep, Ceiling, Layout)) Was.Add(Q.Key);
	auto Fits = [&](double x, double z, double yaw, FBrixLayoutPiece& Q)
	{
		Q = Piece;
		Q.X = x; Q.Z = z; Q.Yaw = yaw;
		FBrixLayout With = Layout;
		With.Add(Q);
		for (const TPair<FString, FString>& Pr : BrixFurnishPlan::Problems(F, Room, Keep, Ceiling, With)) if (!Was.Contains(Pr.Key)) return false;
		return true;
	};
	if (At && Fits(At->X, At->Y, At->Z, Out)) return true;
	const double g = .25;
	double Best = -1;
	for (double x = Room.X + g; x < Room.Z; x += g)
		for (double z = Room.Y + g; z < Room.W; z += g)
		{
			const double Dist = FMath::Sqrt(FMath::Square(x - Entry.X) + FMath::Square(z - Entry.Y));
			if (Best >= 0 && !(Dist < Best - 1e-9)) continue;
			FBrixLayoutPiece Q;
			if (Fits(FMath::RoundToDouble(x * 100) / 100, FMath::RoundToDouble(z * 100) / 100, 0, Q)) { Best = Dist; Out = Q; }
		}
	return Best >= 0;
}
