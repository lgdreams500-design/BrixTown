#include "BrixFurnishPlan.h"

#include "BrixFurnish.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

namespace
{
	const double Tol = 1e-3;
	FString The(const FBrixLayoutPiece& P) { return TEXT("the ") + P.Name.ToLower(); }
	bool Sold(const FBrixFurnishRulesD& F, const FBrixLayoutPiece& P) { return P.Kind == TEXT("town") || F.Pieces.Contains(P.Kind); }
	TArray<FVector2D> RectPts(const FVector4f& R) { return { FVector2D(R.X, R.Y), FVector2D(R.Z, R.Y), FVector2D(R.Z, R.W), FVector2D(R.X, R.W) }; }
	bool Apart(const TArray<FVector2D>& A, const TArray<FVector2D>& B)   // kept apart by a line along one of their edges
	{
		for (const TArray<FVector2D>* P : { &A, &B })
			for (int32 k = 0; k < P->Num(); ++k)
			{
				const FVector2D E = (*P)[(k + 1) % P->Num()] - (*P)[k];
				const double n = E.Size();
				if (n < 1e-9) continue;
				const FVector2D N(-E.Y / n, E.X / n);
				double A0 = 1e18, A1 = -1e18, B0 = 1e18, B1 = -1e18;
				for (const FVector2D& Q : A) { const double v = Q.X * N.X + Q.Y * N.Y; A0 = FMath::Min(A0, v); A1 = FMath::Max(A1, v); }
				for (const FVector2D& Q : B) { const double v = Q.X * N.X + Q.Y * N.Y; B0 = FMath::Min(B0, v); B1 = FMath::Max(B1, v); }
				if (FMath::Min(A1, B1) - FMath::Max(A0, B0) <= Tol) return true;
			}
		return false;
	}
	bool OffStep(double V, double Step) { return FMath::Abs(V / Step - FMath::RoundHalfFromZero(V / Step)) > 1e-6; }
	double D3(double V) { return FCString::Atod(*FString::Printf(TEXT("%.3f"), V)); }
}

bool FBrixFurnishRulesD::Load(const TSharedPtr<FJsonObject>& J)
{
	if (!J.IsValid()) return false;
	J->TryGetNumberField(TEXT("grid_m"), Grid); J->TryGetNumberField(TEXT("turn_deg"), Turn); J->TryGetNumberField(TEXT("rug_h"), RugH);
	Lines.Reset(); Rooms.Reset(); Pieces.Reset();
	for (const auto& V : J->GetArrayField(TEXT("lines"))) Lines.Add(V->AsString());
	for (const auto& R : J->GetObjectField(TEXT("rooms"))->Values)
	{
		TArray<FString> Ks;
		for (const auto& V : R.Value->AsArray()) Ks.Add(V->AsString());
		Rooms.Add({ FString(R.Key), Ks });
	}
	for (const auto& P : J->GetObjectField(TEXT("pieces"))->Values)
	{
		const TSharedPtr<FJsonObject> O = P.Value->AsObject();
		FPiece& Pc = Pieces.Add(FString(P.Key));
		O->TryGetStringField(TEXT("name"), Pc.Name);
		O->TryGetStringField(TEXT("from"), Pc.From);
		const TArray<TSharedPtr<FJsonValue>>& S = O->GetArrayField(TEXT("size"));
		Pc.Size = FVector3f(S[0]->AsNumber(), S[1]->AsNumber(), S[2]->AsNumber());
		double Pr = 0; O->TryGetNumberField(TEXT("price"), Pr); Pc.Price = (int64)Pr;
		O->TryGetBoolField(TEXT("rug"), Pc.bRug);
	}
	return true;
}

namespace BrixFurnishPlan
{
	TArray<FVector2D> Corners(const FBrixLayoutPiece& P)
	{
		const double A = FMath::DegreesToRadians(P.Yaw), c = FMath::Cos(A), s = FMath::Sin(A);
		TArray<FVector2D> Out;
		for (const FVector2D& Uv : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(1, 1), FVector2D(-1, 1) })
		{
			const double u = Uv.X * P.W / 2, v = Uv.Y * P.D / 2;
			Out.Add(FVector2D(P.X + c * u + s * v, P.Z - s * u + c * v));
		}
		return Out;
	}

	double Snap(const FBrixFurnishRulesD& F, double V) { return FCString::Atod(*FString::Printf(TEXT("%.2f"), FMath::RoundHalfFromZero(V / F.Grid) * F.Grid)); }

	TArray<TPair<FString, FString>> Problems(const FBrixFurnishRulesD& F, const FVector4f& Room, const TArray<FVector4f>& Keep, double Ceiling, const FBrixLayout& Pieces)
	{
		TArray<TPair<FString, FString>> Out;
		for (const FBrixLayoutPiece& P : Pieces)
		{
			if (!Sold(F, P)) { Out.Add({ P.Id + TEXT("|sold"), TEXT("the ") + P.Kind + TEXT(" is not sold here") }); continue; }
			if (P.Kind != TEXT("town") && !(P.Line && F.Lines.Contains(*P.Line))) Out.Add({ P.Id + TEXT("|line"), The(P) + TEXT(" has no line ") + (P.Line ? *P.Line : FString(TEXT("undefined"))) });
			if (OffStep(P.Yaw, F.Turn)) Out.Add({ P.Id + TEXT("|turn"), The(P) + FString::Printf(TEXT(" is not turned in %s degree steps"), *FString::SanitizeFloat(F.Turn, 0)) });
			if (P.Kind != TEXT("town") && (OffStep(P.X, F.Grid) || OffStep(P.Z, F.Grid))) Out.Add({ P.Id + TEXT("|grid"), The(P) + FString::Printf(TEXT(" is off the %d cm grid"), (int32)FMath::RoundHalfFromZero(F.Grid * 100)) });
			const TArray<FVector2D> C = Corners(P);
			double X0 = 1e18, X1 = -1e18, Z0 = 1e18, Z1 = -1e18;
			for (const FVector2D& Q : C) { X0 = FMath::Min(X0, Q.X); X1 = FMath::Max(X1, Q.X); Z0 = FMath::Min(Z0, Q.Y); Z1 = FMath::Max(Z1, Q.Y); }
			if (X0 < Room.X - Tol || X1 > Room.Z + Tol || Z0 < Room.Y - Tol || Z1 > Room.W + Tol) Out.Add({ P.Id + TEXT("|room"), The(P) + TEXT(" sticks out of the room") });
			if (P.H > Ceiling + Tol) Out.Add({ P.Id + TEXT("|ceiling"), The(P) + TEXT(" does not fit under the ceiling") });
			if (!P.bRug && Keep.ContainsByPredicate([&](const FVector4f& R) { return !Apart(C, RectPts(R)); })) Out.Add({ P.Id + TEXT("|keep"), The(P) + TEXT(" is in the way of the door or the stairs") });
		}
		FBrixLayout Stand;
		for (const FBrixLayoutPiece& P : Pieces) if (!P.bRug && Sold(F, P)) Stand.Add(P);
		for (int32 i = 0; i < Stand.Num(); ++i)
			for (int32 j = i + 1; j < Stand.Num(); ++j)
				if (!Apart(Corners(Stand[i]), Corners(Stand[j])))
				{
					const bool bLess = FCString::Strcmp(*Stand[i].Id, *Stand[j].Id) < 0;
					Out.Add({ (bLess ? Stand[i].Id : Stand[j].Id) + TEXT("|") + (bLess ? Stand[j].Id : Stand[i].Id) + TEXT("|clash"), The(Stand[i]) + TEXT(" and ") + The(Stand[j]) + TEXT(" stand in each other") });
				}
		return Out;
	}

	int64 Cost(const FBrixFurnishRulesD& F, const FBrixLayout& Before, const FBrixLayout& After, const FBrixLayout& Belongings, TArray<FBrixFurnLine>& Lines)
	{
		Lines.Reset();
		auto Mine = [&](const FString& Id) { return Belongings.ContainsByPredicate([&](const FBrixLayoutPiece& B) { return B.Id == Id; }); };
		for (const FBrixLayoutPiece& P : After)
		{
			const FBrixLayoutPiece* O = Before.FindByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == P.Id; });
			if (!O && Mine(P.Id)) Lines.Add({ TEXT("Place: ") + P.Name, 1, 0 });
			else if (!O) { const FBrixFurnishRulesD::FPiece* K = F.Pieces.Find(P.Kind); Lines.Add({ TEXT("Buy: ") + P.Name, 1, K ? K->Price : 0 }); }
			else if (O->X != P.X || O->Z != P.Z || O->Yaw != P.Yaw) Lines.Add({ TEXT("Move: ") + P.Name, 1, 0 });
		}
		for (const FBrixLayoutPiece& P : Before) if (!After.ContainsByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == P.Id; })) Lines.Add({ TEXT("To your belongings: ") + P.Name, 1, 0 });
		int64 T = 0;
		for (const FBrixFurnLine& L : Lines) T += L.Chips;
		return T;
	}

	TArray<FString> Check(const FBrixFurnishRulesD& F, const FVector4f& Room, const TArray<FVector4f>& Keep, double Ceiling, const FBrixLayout& Before, const FBrixLayout& After, TOptional<int64> Funds, const FBrixLayout& Belongings)
	{
		TSet<FString> Old;
		for (const auto& P : Problems(F, Room, Keep, Ceiling, Before)) Old.Add(P.Key);
		TArray<FString> Out;
		for (const auto& P : Problems(F, Room, Keep, Ceiling, After)) if (!Old.Contains(P.Key)) Out.AddUnique(P.Value);
		TArray<FBrixFurnLine> L;
		const int64 Total = Cost(F, Before, After, Belongings, L);
		for (const FBrixLayoutPiece& P : After)
		{
			const bool bWas = Before.ContainsByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == P.Id; });
			const bool bMine = Belongings.ContainsByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == P.Id; });
			const FBrixFurnishRulesD::FPiece* K = F.Pieces.Find(P.Kind);
			if (!bWas && P.Id.StartsWith(TEXT("b")) && !bMine) Out.Add(The(P) + TEXT(" is not among your belongings"));
			else if (!bWas && !P.Id.StartsWith(TEXT("b")) && K && !K->From.IsEmpty()) Out.Add(The(P) + TEXT(" is sold at the ") + K->From + TEXT(", not here"));   // FURN-064
		}
		if (Funds && Total > *Funds) Out.Add(FString::Printf(TEXT("you cannot pay %lld.%02lld brix"), Total / 100, Total % 100));
		return Out;
	}

	FBrixLayoutPiece KitPiece(const FBrixFurnishRulesD& F, const FString& Kind, const FString& Id, double X, double Z, double Yaw, const FString& Line)
	{
		FBrixLayoutPiece P;
		const FBrixFurnishRulesD::FPiece* K = F.Pieces.Find(Kind);
		P.Id = Id; P.Kind = Kind; P.Name = K ? K->Name : Kind;
		P.W = K ? K->Size.X : 1; P.D = K ? K->Size.Y : 1; P.H = K ? K->Size.Z : 1;
		P.X = X; P.Z = Z; P.Yaw = Yaw; P.bRug = K && K->bRug; P.Line = Line;
		return P;
	}

	FBrixLayout Stow(const FBrixLayout& Before, const FBrixLayout& After, const FBrixLayout& Belongings, const FString& Lot, int32 Level)
	{
		auto Now = [&](const FString& Id) { return After.ContainsByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == Id; }); };
		FBrixLayout Out;
		for (const FBrixLayoutPiece& B : Belongings) if (!Now(B.Id)) Out.Add(B);
		for (const FBrixLayoutPiece& P : Before)
		{
			if (Now(P.Id)) continue;
			FBrixLayoutPiece Q = P;
			Q.X = Q.Z = Q.Yaw = 0;
			if (!P.Id.StartsWith(TEXT("b")))
			{
				Q.Id = FString::Printf(TEXT("b:%s:%d:%s"), *Lot, Level, *P.Id);
				if (P.Kind == TEXT("town") && !P.FromLot) { Q.FromLot = Lot; Q.FromLevel = Level; Q.FromId = P.Id; }
			}
			Out.Add(Q);
		}
		return Out;
	}

	FBrixLayout Apply(const FBrixFurnishRulesD& F, const FBrixLayout& Before, const TSharedPtr<FJsonObject>& Edit, const FBrixLayout& Belongings)
	{
		TArray<FString> Remove;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (Edit->TryGetArrayField(TEXT("remove"), A)) for (const auto& V : *A) Remove.Add(V->AsString());
		const TSharedPtr<FJsonObject>* Move = nullptr;
		Edit->TryGetObjectField(TEXT("move"), Move);
		FBrixLayout Out;
		for (const FBrixLayoutPiece& P : Before)
		{
			if (Remove.Contains(P.Id)) continue;
			FBrixLayoutPiece Q = P;
			const TArray<TSharedPtr<FJsonValue>>* M = nullptr;
			if (Move && (*Move)->TryGetArrayField(P.Id, M)) { Q.X = (*M)[0]->AsNumber(); Q.Z = (*M)[1]->AsNumber(); Q.Yaw = (*M)[2]->AsNumber(); }
			Out.Add(Q);
		}
		if (Edit->TryGetArrayField(TEXT("add"), A))
			for (const auto& V : *A) { const TArray<TSharedPtr<FJsonValue>>& X = V->AsArray(); Out.Add(KitPiece(F, X[1]->AsString(), X[0]->AsString(), X[2]->AsNumber(), X[3]->AsNumber(), X[4]->AsNumber(), X[5]->AsString())); }
		if (Edit->TryGetArrayField(TEXT("place"), A))
			for (const auto& V : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>& X = V->AsArray();
				const FString Bid = X[0]->AsString();
				const FBrixLayoutPiece* B = Belongings.FindByPredicate([&](const FBrixLayoutPiece& Y) { return Y.Id == Bid; });
				FBrixLayoutPiece Q;
				if (B) Q = *B; else { Q.Id = Bid; Q.Kind = TEXT("town"); Q.Name = TEXT("Piece"); }
				Q.X = X[1]->AsNumber(); Q.Z = X[2]->AsNumber(); Q.Yaw = X[3]->AsNumber();
				Out.Add(Q);
			}
		return Out;
	}

	TSharedPtr<FJsonObject> ToJson(const FBrixLayoutPiece& P, bool bWhere)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		O->SetStringField(TEXT("id"), P.Id); O->SetStringField(TEXT("kind"), P.Kind); O->SetStringField(TEXT("name"), P.Name);
		O->SetNumberField(TEXT("w"), P.W); O->SetNumberField(TEXT("d"), P.D); O->SetNumberField(TEXT("h"), P.H);
		if (bWhere) { O->SetNumberField(TEXT("x"), P.X); O->SetNumberField(TEXT("z"), P.Z); O->SetNumberField(TEXT("yaw"), P.Yaw); }
		O->SetBoolField(TEXT("rug"), P.bRug);
		if (P.Line) O->SetStringField(TEXT("line"), *P.Line);
		if (P.FromLot)
		{
			TSharedPtr<FJsonObject> Fr = MakeShared<FJsonObject>();
			Fr->SetStringField(TEXT("lot"), *P.FromLot); Fr->SetNumberField(TEXT("level"), P.FromLevel); Fr->SetStringField(TEXT("id"), P.FromId);
			O->SetObjectField(TEXT("from"), Fr);
		}
		return O;
	}

	FBrixLayoutPiece FromJson(const TSharedPtr<FJsonObject>& O)
	{
		FBrixLayoutPiece P;
		O->TryGetStringField(TEXT("id"), P.Id); O->TryGetStringField(TEXT("kind"), P.Kind); O->TryGetStringField(TEXT("name"), P.Name);
		O->TryGetNumberField(TEXT("w"), P.W); O->TryGetNumberField(TEXT("d"), P.D); O->TryGetNumberField(TEXT("h"), P.H);
		O->TryGetNumberField(TEXT("x"), P.X); O->TryGetNumberField(TEXT("z"), P.Z); O->TryGetNumberField(TEXT("yaw"), P.Yaw);
		O->TryGetBoolField(TEXT("rug"), P.bRug);
		FString Ln;
		if (O->TryGetStringField(TEXT("line"), Ln)) P.Line = Ln;
		const TSharedPtr<FJsonObject>* Fr = nullptr;
		if (O->TryGetObjectField(TEXT("from"), Fr))
		{
			FString L; (*Fr)->TryGetStringField(TEXT("lot"), L); P.FromLot = L;
			double Lv = 0; (*Fr)->TryGetNumberField(TEXT("level"), Lv); P.FromLevel = (int32)Lv;
			(*Fr)->TryGetStringField(TEXT("id"), P.FromId);
		}
		return P;
	}

	FTown TownPieces(const FBrixFurnished& Furn)   // play.js townPieces(), on the boxes BrixFurnishFloor laid (heights from the floor)
	{
		FTown T;
		const double E = .006;
		struct FB { double X0, Y0, Z0, X1, Y1, Z1; };
		TArray<FB> Bb;
		for (const FBrixFurnBox& B : Furn.Boxes)
		{
			const double c = FMath::Abs(FMath::Cos(B.Yaw)), s = FMath::Abs(FMath::Sin(B.Yaw)), hx = B.Size.X / 2, hz = B.Size.Z / 2;
			const double Ex = c * hx + s * hz, Ez = s * hx + c * hz;
			Bb.Add({ B.At.X - Ex, B.Y0, B.At.Y - Ez, B.At.X + Ex, B.Y0 + B.Size.Y, B.At.Y + Ez });
		}
		auto Flat = [](const FB& b) { return b.Y1 - b.Y0 < .02 && b.Y0 < .05; };
		TArray<int32> Par;
		for (int32 i = 0; i < Bb.Num(); ++i) Par.Add(i);
		TFunction<int32(int32)> Find = [&](int32 i) { while (Par[i] != i) { Par[i] = Par[Par[i]]; i = Par[i]; } return i; };
		TArray<int32> Order = Par;
		Order.StableSort([&](int32 i, int32 j) { return Bb[i].X0 < Bb[j].X0; });
		for (int32 k = 0; k < Order.Num(); ++k)
		{
			const FB& A = Bb[Order[k]];
			for (int32 q = k + 1; q < Order.Num(); ++q)
			{
				const FB& B = Bb[Order[q]];
				if (B.X0 > A.X1 + E) break;
				if (Flat(A) != Flat(B) || B.Z0 > A.Z1 + E || A.Z0 > B.Z1 + E || (!Flat(A) && (B.Y0 > A.Y1 + E || A.Y0 > B.Y1 + E))) continue;
				Par[Find(Order[k])] = Find(Order[q]);
			}
		}
		TArray<int32> Roots;
		TMap<int32, TArray<int32>> Groups;
		for (int32 i = 0; i < Bb.Num(); ++i) { const int32 r = Find(i); if (!Groups.Contains(r)) Roots.Add(r); Groups.FindOrAdd(r).Add(i); }
		Roots.StableSort([&](int32 a, int32 b) { return Groups[a][0] < Groups[b][0]; });
		for (const int32 r : Roots)
		{
			const TArray<int32>& G = Groups[r];
			double X0 = 1e18, X1 = -1e18, Y0 = 1e18, Y1 = -1e18, Z0 = 1e18, Z1 = -1e18;
			for (const int32 i : G) { X0 = FMath::Min(X0, Bb[i].X0); X1 = FMath::Max(X1, Bb[i].X1); Y0 = FMath::Min(Y0, Bb[i].Y0); Y1 = FMath::Max(Y1, Bb[i].Y1); Z0 = FMath::Min(Z0, Bb[i].Z0); Z1 = FMath::Max(Z1, Bb[i].Z1); }
			const bool bRug = Flat(Bb[G[0]]);
			if (!bRug && Y0 > 1.0) { T.Fixed.Append(G); continue; }      // on a wall or from the ceiling: it stays
			const double w = X1 - X0, d = Z1 - Z0, h = Y1;
			FBrixLayoutPiece P;
			P.Id = FString::Printf(TEXT("t%d"), T.Pieces.Num());
			P.Name = bRug ? TEXT("Rug") : h > 1.6 ? TEXT("Cupboard") : w * d < .3 && h > 1.1 ? TEXT("Lamp") : h < .6 ? TEXT("Low table") : w * d < .5 ? TEXT("Chair") : TEXT("Cabinet");
			P.W = D3(w); P.D = D3(d); P.H = D3(h); P.X = D3((X0 + X1) / 2); P.Z = D3((Z0 + Z1) / 2); P.bRug = bRug;
			T.Pieces.Add(P);
			T.Boxes.Add(G);
			T.Centre.Add(FVector2D((X0 + X1) / 2, (Z0 + Z1) / 2));
		}
		return T;
	}

	void LayoutBoxes(const FBrixFurnished& Furn, const FTown& Town, const FBrixLayout& Layout, TArray<FBrixFurnBox>& Out)
	{
		Out.Reset();
		for (const int32 i : Town.Fixed) if (Furn.Boxes.IsValidIndex(i)) Out.Add(Furn.Boxes[i]);
		for (const FBrixLayoutPiece& P : Layout)
		{
			if (P.Kind != TEXT("town")) continue;
			const int32 k = Town.Pieces.IndexOfByPredicate([&](const FBrixLayoutPiece& X) { return X.Id == P.Id; });
			if (k == INDEX_NONE) continue;   // a piece from elsewhere: drawn as its own box by the rooms
			const double Y = FMath::DegreesToRadians(P.Yaw), c = FMath::Cos(Y), s = FMath::Sin(Y);
			for (const int32 i : Town.Boxes[k])
			{
				FBrixFurnBox B = Furn.Boxes[i];
				const FVector2D D = B.At - Town.Centre[k];
				B.At = FVector2D(P.X + c * D.X + s * D.Y, P.Z - s * D.X + c * D.Y);
				B.Yaw += (float)Y;
				Out.Add(B);
			}
		}
	}
}
