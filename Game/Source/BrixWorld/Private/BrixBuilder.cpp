#include "BrixBuilder.h"

#include "BrixCity.h"

#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"

namespace
{
	using FObj = TSharedPtr<FJsonObject>;
	using FVal = TSharedPtr<FJsonValue>;
	const double OldBase = 3.55, Gf = 3.2;                // builder.js OLD_BASE, GF

	FObj Ob(const FObj& O, const FString& K) { const FObj* P = nullptr; return O.IsValid() && O->TryGetObjectField(K, P) ? *P : FObj(); }
	FVal Fv(const FObj& O, const FString& K) { return O.IsValid() ? O->TryGetField(K) : FVal(); }
	bool IsNull(const FVal& V) { return !V.IsValid() || V->IsNull(); }
	FString Str(const FObj& O, const FString& K) { FString S; if (O.IsValid()) O->TryGetStringField(K, S); return S; }
	bool Bool(const FObj& O, const FString& K) { bool B = false; if (O.IsValid()) O->TryGetBoolField(K, B); return B; }
	double Num(const FObj& O, const FString& K, double Or = 0) { double D = Or; if (O.IsValid()) O->TryGetNumberField(K, D); return D; }
	TArray<FString> Strs(const FObj& O, const FString& K)
	{
		TArray<FString> Out;
		const TArray<FVal>* A = nullptr;
		if (O.IsValid() && O->TryGetArrayField(K, A)) for (const FVal& V : *A) if (V->Type == EJson::String) Out.Add(V->AsString());
		return Out;
	}
	TArray<FVal> Arr(const FObj& O, const FString& K) { const TArray<FVal>* A = nullptr; return O.IsValid() && O->TryGetArrayField(K, A) ? *A : TArray<FVal>(); }
	FVector3f Rgb(const FVal& V)
	{
		if (IsNull(V) || V->Type != EJson::Array) return FVector3f(0.5f);
		const TArray<FVal>& A = V->AsArray();
		return A.Num() >= 3 ? FVector3f((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber()) : FVector3f(0.5f);
	}
	FVal Copy(const FVal& V)
	{
		if (!V.IsValid()) return MakeShared<FJsonValueNull>();
		switch (V->Type)
		{
		case EJson::Object: return MakeShared<FJsonValueObject>(BrixBuilder::Clone(V->AsObject()));
		case EJson::Array: { TArray<FVal> A; for (const FVal& X : V->AsArray()) A.Add(Copy(X)); return MakeShared<FJsonValueArray>(A); }
		default: return V;                               // numbers, strings, booleans and null do not change in place
		}
	}
	void Paths(const FObj& D, const FString& Prefix, TMap<FString, FVal>& Out)
	{
		for (const auto& KV : D->Values)
		{
			const FString P = Prefix + FString(KV.Key);
			if (KV.Value.IsValid() && KV.Value->Type == EJson::Object) Paths(KV.Value->AsObject(), P + TEXT("."), Out);
			else Out.Add(P, KV.Value);
		}
	}
	bool Match(const FString& Path, const FString& Pattern)
	{
		TArray<FString> P, Q;
		Path.ParseIntoArray(P, TEXT("."), false);
		Pattern.ParseIntoArray(Q, TEXT("."), false);
		if (P.Num() < Q.Num()) return false;
		for (int32 i = 0; i < Q.Num(); ++i) if (Q[i] != TEXT("*") && Q[i] != P[i]) return false;
		return true;
	}
	bool Part(const FString& Path, const TArray<FString>& Allowed) { for (const FString& A : Allowed) if (Match(Path, A)) return true; return false; }
	bool StrIn(const FVal& V, const TArray<FString>& L) { return V.IsValid() && V->Type == EJson::String && L.Contains(V->AsString()); }
	FString JsStr(const FVal& V) { return IsNull(V) ? FString(TEXT("null")) : V->Type == EJson::String ? V->AsString() : BrixBuilder::Json(V); }
	double Round2(double X) { return FCString::Atod(*FString::Printf(TEXT("%.2f"), X)); }   // Number(x.toFixed(2))
	int64 RoundEven(double X)                             // Python's round()
	{
		const double F = FMath::FloorToDouble(X), D = X - F;
		return (int64)(D > .5 ? F + 1 : D < .5 ? F : (FMath::Fmod(F, 2.0) == 0 ? F : F + 1));
	}
	FString PyNum(double X)                               // how Python prints a float (50.0, 320.72)
	{
		if (X == FMath::FloorToDouble(X) && FMath::Abs(X) < 1e15) return FString::Printf(TEXT("%lld.0"), (int64)X);
		for (int32 P = 1; P <= 17; ++P)
		{
			const FString S = FString::Printf(TEXT("%.*g"), P, X);
			if (FCString::Atod(*S) == X) return S;
		}
		return FString::Printf(TEXT("%.17g"), X);
	}
	FString Cap(const FString& S) { FString T = S.Replace(TEXT("_"), TEXT(" ")).ToLower(); if (T.Len()) T[0] = FChar::ToUpper(T[0]); return T; }
	FString Lower(const FString& S) { return S.ToLower(); }
}

const TCHAR* const BrixBuilder::Facades[4] = { TEXT("n"), TEXT("s"), TEXT("e"), TEXT("w") };

int32 FBrixBuiltLook::FacadeIndex(const FString& F) { return F == TEXT("n") ? 0 : F == TEXT("s") ? 1 : F == TEXT("e") ? 2 : 3; }

bool LoadBrixBuilderRules(const TSharedPtr<FJsonObject>& B, FBrixBuilderRules& Out, FString& OutError)
{
	Out = FBrixBuilderRules();
	if (!B.IsValid()) { OutError = TEXT("no building designer rules (run Tools/worldgen/build_play.py)"); return false; }
	Out.Kit = Ob(B, TEXT("kit")); Out.Planning = Ob(B, TEXT("planning")); Out.Patterns = Ob(B, TEXT("patterns")); Out.Checks = Ob(B, TEXT("checks")); Out.RoomPlan = Ob(B, TEXT("room_plan"));
	if (!Out.IsSet()) { OutError = TEXT("the building designer's kit, planning or patterns are missing"); return false; }
	return true;
}

namespace BrixBuilder
{
	FString FacadeName(const FString& F) { return F == TEXT("n") ? TEXT("north") : F == TEXT("s") ? TEXT("south") : F == TEXT("e") ? TEXT("east") : TEXT("west"); }

	TSharedPtr<FJsonObject> Clone(const TSharedPtr<FJsonObject>& D)
	{
		TSharedPtr<FJsonObject> O = MakeShared<FJsonObject>();
		if (D.IsValid()) for (const auto& KV : D->Values) O->SetField(FString(KV.Key), Copy(KV.Value));
		return O;
	}

	FString Json(const TSharedPtr<FJsonValue>& V)
	{
		if (!V.IsValid()) return TEXT("null");
		FString S;
		const TSharedRef<TJsonWriter<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>> W = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&S);
		FJsonSerializer::Serialize(V, FString(), W);
		return S;
	}

	bool Same(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B) { return Json(IsNull(A) ? FVal() : A) == Json(IsNull(B) ? FVal() : B); }

	TSharedPtr<FJsonValue> Get(const TSharedPtr<FJsonObject>& D, const FString& Path)
	{
		TArray<FString> K;
		Path.ParseIntoArray(K, TEXT("."), false);
		FObj O = D;
		for (int32 i = 0; i < K.Num(); ++i)
		{
			const FVal V = Fv(O, K[i]);
			if (i == K.Num() - 1) return V;
			if (IsNull(V) || V->Type != EJson::Object) return FVal();
			O = V->AsObject();
		}
		return FVal();
	}

	TArray<FString> Changed(const TSharedPtr<FJsonObject>& A0, const TSharedPtr<FJsonObject>& B0)
	{
		TMap<FString, FVal> A, B;
		Paths(A0, FString(), A);
		Paths(B0, FString(), B);
		TArray<FString> Ks;
		A.GetKeys(Ks);
		for (const auto& KV : B) Ks.AddUnique(KV.Key);
		TArray<FString> Out;
		for (const FString& K : Ks) if (!Same(A.FindRef(K), B.FindRef(K))) Out.Add(K);
		Out.Sort([](const FString& X, const FString& Y) { return FCString::Strcmp(*X, *Y) < 0; });
		return Out;
	}

	TSharedPtr<FJsonObject> Remodel(const TSharedPtr<FJsonObject>& Old, const FBrixDesignChanges& Changes)
	{
		const FObj New = Clone(Old);
		for (const auto& C : Changes)
		{
			TArray<FString> Ks;
			C.Key.ParseIntoArray(Ks, TEXT("."), false);
			const FString Last = Ks.Pop();
			TArray<FObj> Nodes = { New };
			for (const FString& K : Ks)
			{
				TArray<FObj> Next;
				for (const FObj& N : Nodes)
					if (K == TEXT("*")) { for (const TCHAR* F : Facades) if (const FObj X = Ob(N, F)) Next.Add(X); }
					else if (const FObj X = Ob(N, K)) Next.Add(X);
				Nodes = Next;
			}
			for (const FObj& N : Nodes) N->SetField(Last, Copy(C.Value));
		}
		return New;
	}

	FBrixBuiltLook Built(const FBrixBuilderRules& R, const TSharedPtr<FJsonObject>& D)
	{
		FBrixBuiltLook B;
		const FObj Cols = Ob(R.Kit, TEXT("colours"));
		auto Colour = [&](const FString& Id) { return Rgb(Fv(Ob(Cols, Id), TEXT("rgb"))); };
		const FObj W = Ob(D, TEXT("walls"));
		for (int32 k = 0; k < 4; ++k)
		{
			const FObj Fa = Ob(W, Facades[k]);
			FBrixBuiltLook::FFacade& F = B.Walls[k];
			F.Plaster = Colour(Str(Fa, TEXT("plaster")));
			const FObj St = Ob(Fa, TEXT("stone"));
			F.Pattern = Str(St, TEXT("pattern"));
			const FVal C = Fv(St, TEXT("color"));
			if (!IsNull(C) && !F.Pattern.IsEmpty())
			{
				const TArray<FVal> P = Arr(R.Patterns, F.Pattern);
				const int32 i = (int32)C->AsNumber();
				if (P.IsValidIndex(i) && P[i]->Type == EJson::Array) F.Stone = Rgb(P[i]);
			}
			F.bStoneToEaves = Bool(Fa, TEXT("stone_to_eaves"));
			F.bBrickToEaves = Bool(Fa, TEXT("brick_to_eaves"));
		}
		B.Side = Str(Ob(D, TEXT("shell")), TEXT("side"));
		B.Roof = Colour(Str(Ob(D, TEXT("roof")), TEXT("tiles")));
		const FObj Op = Ob(D, TEXT("openings"));
		B.Shutter = Colour(Str(Op, TEXT("shutter_color")));
		B.Frame = Colour(Str(Op, TEXT("frame")));
		B.Awning = Colour(Str(Op, TEXT("awning")));
		B.bShutters = Bool(Op, TEXT("shutters"));
		const FObj Win = Ob(Ob(R.Kit, TEXT("windows")), Str(Op, TEXT("class")));
		B.WinWidth = (float)Num(Win, TEXT("width"), 1.1); B.WinGap = (float)Num(Win, TEXT("gap"), 3.2); B.WinTall = (float)Num(Win, TEXT("tall"), 1.6);
		const FObj In = Ob(D, TEXT("inside")), FF = Ob(R.Kit, TEXT("floor_finishes")), Mix = Ob(R.Kit, TEXT("inner_mix"));
		for (const FString& X : Strs(In, TEXT("floors"))) { const FObj Fo = Ob(FF, X); B.FloorFinish.Add((int32)Num(Fo, TEXT("mat"))); B.FloorColour.Add(Rgb(Fv(Fo, TEXT("rgb")))); }
		const FVector3f With = Rgb(Fv(Mix, TEXT("with")));
		const float Share = (float)Num(Mix, TEXT("share"));
		for (const FString& X : Strs(In, TEXT("walls"))) { const FVector3f P = Colour(X); B.WallColour.Add(P + (With - P) * Share); }
		return B;
	}

	TSharedPtr<FJsonObject> BuiltFacadeJson(const FBrixBuilderRules& R, const TSharedPtr<FJsonObject>& D, const FString& F)   // world.built()'s walls[f]
	{
		const FObj Fa = Ob(Ob(D, TEXT("walls")), F), Cols = Ob(R.Kit, TEXT("colours"));
		const FObj Out = Clone(Fa);
		Out->SetField(TEXT("plaster"), Copy(Fv(Ob(Cols, Str(Fa, TEXT("plaster"))), TEXT("rgb"))));
		const FObj St = Clone(Ob(Fa, TEXT("stone")));
		const FVal C = Fv(St, TEXT("color"));
		if (!IsNull(C)) { const TArray<FVal> P = Arr(R.Patterns, Str(St, TEXT("pattern"))); const int32 i = (int32)C->AsNumber(); St->SetField(TEXT("color"), P.IsValidIndex(i) ? Copy(P[i]) : MakeShared<FJsonValueNull>()); }
		Out->SetObjectField(TEXT("stone"), St);
		return Out;
	}

	bool Allowed(const TSharedPtr<FJsonObject>& Item, const TSharedPtr<FJsonObject>& D)
	{
		if (!Item.IsValid() || Str(Item, TEXT("status")) != TEXT("approved")) return false;
		const FObj Wh = Ob(Item, TEXT("when")), Sh = Ob(D, TEXT("shell"));
		if (!Wh.IsValid()) return true;
		for (const TCHAR* K : { TEXT("big"), TEXT("old"), TEXT("tower") }) if (Wh->HasField(K) && Bool(Wh, K) != Bool(Sh, K)) return false;
		if (Wh->HasField(TEXT("kinds")) && !Strs(Wh, TEXT("kinds")).Contains(Str(Sh, TEXT("kind")))) return false;
		if (Wh->HasField(TEXT("not_kinds")) && Strs(Wh, TEXT("not_kinds")).Contains(Str(Sh, TEXT("kind")))) return false;
		if (Wh->HasField(TEXT("specials")) && !StrIn(Fv(D, TEXT("special")), Strs(Wh, TEXT("specials")))) return false;
		return true;
	}

	TArray<FString> Problems(const FBrixBuilderRules& R, const TSharedPtr<FJsonObject>& D)   // world.design_problems() for one design
	{
		const FObj K = R.Kit, Cols = Ob(K, TEXT("colours"));
		TArray<FString> Out;
		for (const TCHAR* P : { TEXT("shell"), TEXT("walls"), TEXT("roof"), TEXT("openings"), TEXT("ornament"), TEXT("inside"), TEXT("circulation") })
			if (!D->HasField(P)) Out.Add(FString(TEXT("design has no ")) + P);
		if (Out.Num()) return Out;
		const TArray<FString> Plaster = Strs(K, TEXT("plaster"));
		for (const TCHAR* F : Facades)
		{
			const FObj Fa = Ob(Ob(D, TEXT("walls")), F);
			if (!Fa.IsValid()) { Out.Add(FString(TEXT("no ")) + F + TEXT(" facade")); continue; }
			if (!StrIn(Fv(Fa, TEXT("plaster")), Plaster)) Out.Add(FString(F) + TEXT(" facade's plaster colour not in the kit"));
			const FObj St = Ob(Fa, TEXT("stone"));
			const FVal Pt = Fv(St, TEXT("pattern"));
			if (!IsNull(Pt))
			{
				const FString P = JsStr(Pt);
				if (!R.Patterns->HasField(P)) Out.Add(FString(F) + TEXT(" facade's stone pattern not in surfaces.json"));
				else
				{
					const FVal C = Fv(St, TEXT("color"));
					const int32 N = Arr(R.Patterns, P).Num();
					if (!IsNull(C) && !(C->Type == EJson::Number && C->AsNumber() == FMath::FloorToDouble(C->AsNumber()) && C->AsNumber() >= 0 && C->AsNumber() < N))
						Out.Add(FString(F) + TEXT(" facade's stone colour is not one ") + P + TEXT(" comes in"));
				}
			}
		}
		const FObj Roof = Ob(D, TEXT("roof")), Op = Ob(D, TEXT("openings"));
		if (!StrIn(Fv(Roof, TEXT("tiles")), Strs(K, TEXT("roof_tiles")))) Out.Add(TEXT("roof tiles not in the kit"));
		const FObj Roofs = Ob(K, TEXT("roofs")), Wins = Ob(K, TEXT("windows"));
		const FString Rt = Str(Roof, TEXT("type")), Wc = Str(Op, TEXT("class"));
		if (!Roofs->HasField(Rt)) Out.Add(TEXT("roof type ") + Rt + TEXT(" not in the kit"));
		else if (!Allowed(Ob(Roofs, Rt), D)) Out.Add(TEXT("a ") + Lower(Str(Ob(Roofs, Rt), TEXT("name"))) + TEXT(" is not for this building"));
		if (!Wins->HasField(Wc)) Out.Add(TEXT("window type not in the kit"));
		else if (!Allowed(Ob(Wins, Wc), D)) Out.Add(Lower(Str(Ob(Wins, Wc), TEXT("name"))) + TEXT(" are not for this building"));
		if (!StrIn(Fv(Op, TEXT("awning")), Strs(K, TEXT("accents")))) Out.Add(TEXT("awning colour not in the kit"));
		if (!StrIn(Fv(Op, TEXT("frame")), Strs(K, TEXT("frames")))) Out.Add(TEXT("frame colour not in the kit"));
		if (!Cols->HasField(Str(Op, TEXT("shutter_color")))) Out.Add(TEXT("shutter colour not in the kit"));
		for (const TPair<const TCHAR*, const TCHAR*>& PC : { TPair<const TCHAR*, const TCHAR*>(TEXT("openings"), TEXT("openings")), TPair<const TCHAR*, const TCHAR*>(TEXT("ornament"), TEXT("ornaments")) })
		{
			const FObj Part0 = Ob(D, PC.Key), Cat = Ob(K, PC.Value);
			for (const auto& KV : Part0->Values)
			{
				bool b = false;
				if (KV.Value.IsValid() && KV.Value->TryGetBool(b) && b && KV.Value->Type == EJson::Boolean && Cat->HasField(FString(KV.Key)) && Str(Ob(Cat, FString(KV.Key)), TEXT("status")) != TEXT("approved"))
					Out.Add(Lower(Str(Ob(Cat, FString(KV.Key)), TEXT("name"))) + TEXT(" is not approved"));
			}
		}
		const FObj Sh = Ob(D, TEXT("shell")), I = Ob(D, TEXT("inside"));
		const int32 n = (int32)Num(Sh, TEXT("floors"));
		const TArray<FVal> Fl = Arr(I, TEXT("floors")), Us = Arr(I, TEXT("uses")), Wl = Arr(I, TEXT("walls"));
		if (!(Fl.Num() == Us.Num() && Us.Num() == Wl.Num() && Wl.Num() == n)) Out.Add(FString::Printf(TEXT("%d floors but %d finishes"), n, Fl.Num()));
		const FObj FF = Ob(K, TEXT("floor_finishes"));
		for (int32 f = 0; f < Fl.Num(); ++f) { const FString X = JsStr(Fl[f]); if (!FF->HasField(X) || X == TEXT("note")) Out.Add(FString::Printf(TEXT("floor %d's finish not in the kit"), f)); }
		for (int32 f = 0; f < Wl.Num(); ++f) if (!StrIn(Wl[f], Plaster)) Out.Add(FString::Printf(TEXT("floor %d's inside walls not a kit colour"), f));
		if (n >= 2 && IsNull(Fv(Ob(D, TEXT("circulation")), TEXT("stair"))) && IsNull(Fv(Sh, TEXT("plan")))) Out.Add(TEXT("no stair in the design"));
		return Out;
	}

	namespace
	{
		struct FMeasures { double Stone[4] = { 0 }, Plaster[4] = { 0 }, Upper[4] = { 0 }; double Roof = 0, FloorM2 = 0; int32 Windows = 0, Floors = 0; };
		FMeasures Measures(const FBrixBuilderRules& R, const FBrixDesignLot& L, const FObj& D)
		{
			FMeasures M;
			const double W = L.Max.X - L.Min.X, Dp = L.Max.Y - L.Min.Y;
			const FObj Sh = Ob(D, TEXT("shell"));
			const double H = Num(Sh, TEXT("height"));
			const int32 n = (int32)Num(Sh, TEXT("floors"));
			const double Band = Bool(Sh, TEXT("old")) ? OldBase : Gf, Gap = Num(Ob(Ob(R.Kit, TEXT("windows")), Str(Ob(D, TEXT("openings")), TEXT("class"))), TEXT("gap"), 3.2);
			const double Len[4] = { W, W, Dp, Dp };
			int32 Win = 0;
			for (int32 k = 0; k < 4; ++k)
			{
				const FObj Fa = Ob(Ob(D, TEXT("walls")), Facades[k]);
				const bool bTo = Bool(Fa, TEXT("stone_to_eaves")) || Bool(Fa, TEXT("brick_to_eaves"));
				M.Stone[k] = Len[k] * (bTo ? H : Band);
				M.Plaster[k] = bTo ? 0 : Len[k] * FMath::Max(0.0, H - Band);
				M.Upper[k] = Len[k] * FMath::Max(0.0, H - Band);
				Win += FMath::Max(1, (int32)FMath::FloorToDouble((Len[k] - 1.6) / Gap));
			}
			M.Windows = Win * n;
			M.Roof = W * Dp * (Str(Ob(D, TEXT("roof")), TEXT("type")) == TEXT("pitched") ? 1.25 : 1.0);
			M.FloorM2 = FMath::Max(0.0, (W - .7) * (Dp - .7));
			M.Floors = n;
			return M;
		}
	}

	int64 Cost(const FBrixBuilderRules& R, const FBrixDesignLot& L, const TSharedPtr<FJsonObject>& Old, const TSharedPtr<FJsonObject>& New, TArray<FBrixCostLine>& Lines)   // BUILD-005
	{
		const FObj P = Ob(R.Kit, TEXT("prices"));
		const FMeasures M = Measures(R, L, New);
		const TArray<FString> Ch = Changed(Old, New);
		auto Has = [&](const FString& Pat) { for (const FString& C : Ch) if (Match(C, Pat)) return true; return false; };
		Lines.Reset();
		auto Add = [&](const FString& What, double Qty, double Unit) { if (Qty) Lines.Add({ What, Round2(Qty), RoundEven(Qty * Unit) }); };
		double Plaster = 0, Stone = 0, Brick = 0;
		for (int32 k = 0; k < 4; ++k)
		{
			const FString F = Facades[k];
			const FObj Fa = Ob(Ob(New, TEXT("walls")), F);
			if (Has(TEXT("walls.") + F + TEXT(".plaster"))) Plaster += M.Plaster[k];
			if (Has(TEXT("walls.") + F + TEXT(".stone"))) Stone += !Bool(Fa, TEXT("brick_to_eaves")) ? M.Stone[k] : M.Stone[k] - M.Upper[k];
			if (Has(TEXT("walls.") + F + TEXT(".brick_to_eaves")) && Bool(Fa, TEXT("brick_to_eaves"))) Brick += M.Upper[k];
		}
		Add(TEXT("Repaint the plaster"), Plaster, Num(P, TEXT("plaster_m2")));
		Add(TEXT("New stone or brick"), Stone, Num(P, TEXT("stone_m2")));
		Add(TEXT("Brick to the eaves"), Brick, Num(P, TEXT("brick_to_eaves_m2")));
		if (Has(TEXT("roof.tiles"))) Add(TEXT("Re-tile the roof"), M.Roof, Num(P, TEXT("roof_tiles_m2")));
		const FObj Op = Ob(New, TEXT("openings")), Or = Ob(New, TEXT("ornament"));
		if (Has(TEXT("openings.shutters")) && Bool(Op, TEXT("shutters"))) Add(TEXT("Shutters"), M.Windows, Num(P, TEXT("shutters_each")));
		else if (Has(TEXT("openings.shutter_color")) && Bool(Op, TEXT("shutters"))) Add(TEXT("Paint the shutters"), M.Windows, Num(P, TEXT("shutter_paint_each")));
		if (Has(TEXT("openings.frame"))) Add(TEXT("Paint the window frames"), M.Windows, Num(P, TEXT("frame_each")));
		if (Has(TEXT("openings.balcony")) && Bool(Op, TEXT("balcony"))) Add(TEXT("A balcony"), 1, Num(P, TEXT("balcony")));
		if (Has(TEXT("openings.keystones")) && Bool(Op, TEXT("keystones"))) Add(TEXT("Keystones over the windows"), M.Windows, Num(P, TEXT("keystones_each")));
		if (Has(TEXT("ornament.quoins")) && Bool(Or, TEXT("quoins"))) Add(TEXT("Corner stones"), 1, Num(P, TEXT("quoins")));
		if (Has(TEXT("ornament.lantern")) && Bool(Or, TEXT("lantern"))) Add(TEXT("A wall lantern"), 1, Num(P, TEXT("lantern")));
		const TArray<FVal> Of = Arr(Ob(Old, TEXT("inside")), TEXT("floors")), Nf = Arr(Ob(New, TEXT("inside")), TEXT("floors"));
		const TArray<FVal> Ow = Arr(Ob(Old, TEXT("inside")), TEXT("walls")), Nw = Arr(Ob(New, TEXT("inside")), TEXT("walls"));
		const int32 N = FMath::Min(Of.Num(), Nf.Num());
		int32 Floors = 0, Walls = 0;
		for (int32 f = 0; f < N; ++f) { Floors += !Same(Of[f], Nf[f]); Walls += Ow.IsValidIndex(f) && Nw.IsValidIndex(f) && !Same(Ow[f], Nw[f]); }
		auto Plural = [](int32 k) { return k > 1 ? TEXT("s") : TEXT(""); };
		if (Floors) Add(FString::Printf(TEXT("New floors (%d storey%s)"), Floors, Plural(Floors)), M.FloorM2 * Floors, Num(P, TEXT("floor_m2")));
		if (Walls) Add(FString::Printf(TEXT("Repaint inside (%d storey%s)"), Walls, Plural(Walls)), M.FloorM2 * Walls, Num(P, TEXT("inner_m2")));
		if (Lines.Num()) Lines.Add({ TEXT("Permit"), 1, (int64)Num(P, TEXT("permit")) });
		int64 Total = 0;
		for (const FBrixCostLine& Ln : Lines) Total += Ln.Chips;
		return Total;
	}

	FBrixDesignLot LotOf(const FBrixLot& L, const FBrixBlock& B)
	{
		FBrixDesignLot D;
		D.Id = L.Id; D.Kind = L.Kind; D.Name = L.Name; D.District = B.District; D.Template = B.Template;
		D.Min = L.Min; D.Max = L.Max; D.Floors = L.Floors; D.Tags = L.Tags;
		return D;
	}

	bool Civic(const FBrixBuilderRules& R, const FBrixDesignLot& L)
	{
		const FObj C = Ob(R.Planning, TEXT("civic_only"));
		if (Strs(C, TEXT("kinds")).Contains(L.Kind) || Strs(C, TEXT("names")).Contains(L.Name) || Strs(C, TEXT("templates")).Contains(L.Template)) return true;
		const TArray<FString> T = Strs(C, TEXT("tags"));
		for (const FString& X : L.Tags) if (T.Contains(X)) return true;
		return false;
	}

	TArray<TPair<FString, TArray<FBrixDesignOption>>> Options(const FBrixBuilderRules& R, const FBrixDesignLot& L, const TSharedPtr<FJsonObject>& D)   // BUILD-017
	{
		const FObj K = R.Kit, PL = R.Planning, Cols = Ob(K, TEXT("colours"));
		const FObj Pal = Ob(Ob(PL, TEXT("districts")), L.District);
		auto Col = [&](const TArray<FString>& Ids)
		{
			TArray<FBrixDesignOption> O;
			for (const FString& I : Ids) { FBrixDesignOption X; X.Id = MakeShared<FJsonValueString>(I); X.Name = Str(Ob(Cols, I), TEXT("name")); X.Rgb = Rgb(Fv(Ob(Cols, I), TEXT("rgb"))); O.Add(X); }
			return O;
		};
		auto Filter = [](const TArray<FString>& A, const TArray<FString>& In) { TArray<FString> O; for (const FString& x : A) if (In.Contains(x)) O.Add(x); return O; };
		TArray<FString> Stones = Strs(Pal, TEXT("stone"));
		if (Civic(R, L)) Stones.Append(Strs(Ob(PL, TEXT("civic_only")), TEXT("stone")));
		TArray<TPair<FString, TArray<FBrixDesignOption>>> Menu;
		Menu.Add({ TEXT("walls.*.plaster"), Col(Filter(Strs(K, TEXT("plaster")), Strs(Pal, TEXT("plaster")))) });
		TArray<FBrixDesignOption> St;
		const TArray<FString> Brick = Strs(PL, TEXT("brick_to_eaves"));
		for (const FString& S : Stones)
		{
			FBrixDesignOption X;
			X.Id = MakeShared<FJsonValueString>(S);
			X.Name = Cap(S);
			const TArray<FVal> P = Arr(R.Patterns, S);
			for (int32 k = 0; k < P.Num(); ++k) X.Colors.Add(P[k]->Type == EJson::String ? StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueNull>()) : StaticCastSharedRef<FJsonValue>(MakeShared<FJsonValueNumber>(k)));
			X.bBrickToEaves = Brick.Contains(S);
			St.Add(X);
		}
		Menu.Add({ TEXT("walls.*.stone"), St });
		Menu.Add({ TEXT("roof.tiles"), Col(Filter(Strs(K, TEXT("roof_tiles")), Strs(Pal, TEXT("roof_tiles")))) });
		Menu.Add({ TEXT("openings.frame"), Col(Strs(K, TEXT("frames"))) });
		Menu.Add({ TEXT("inside.walls"), Col(Strs(K, TEXT("plaster"))) });
		TArray<FBrixDesignOption> Fl;
		for (const auto& KV : Ob(K, TEXT("floor_finishes"))->Values)
		{
			if (FString(KV.Key) == TEXT("note") || KV.Value->Type != EJson::Object) continue;
			FBrixDesignOption X;
			X.Id = MakeShared<FJsonValueString>(FString(KV.Key));
			X.Name = Str(KV.Value->AsObject(), TEXT("name"));
			X.Rgb = Rgb(Fv(KV.Value->AsObject(), TEXT("rgb")));
			X.Mat = (int32)Num(KV.Value->AsObject(), TEXT("mat"));
			Fl.Add(X);
		}
		Menu.Add({ TEXT("inside.floors"), Fl });
		auto YesNo = [](const FString& Yes, const FString& NoName)
		{
			FBrixDesignOption A, B;
			A.Id = MakeShared<FJsonValueBoolean>(true); A.Name = Yes;
			B.Id = MakeShared<FJsonValueBoolean>(false); B.Name = NoName;
			return TArray<FBrixDesignOption>{ A, B };
		};
		const FObj Ops = Ob(K, TEXT("openings")), Orn = Ob(K, TEXT("ornaments"));
		if (Allowed(Ob(Ops, TEXT("shutters")), D))
		{
			Menu.Add({ TEXT("openings.shutters"), YesNo(TEXT("With shutters"), TEXT("No shutters")) });
			const FString List = Str(Ob(PL, TEXT("shutters")), Str(Ob(D, TEXT("openings")), TEXT("class")) == TEXT("old") ? TEXT("old") : TEXT("default"));
			Menu.Add({ TEXT("openings.shutter_color"), Col(Strs(K, List)) });
		}
		const TCHAR* Extra[4][3] = { { TEXT("openings.balcony"), TEXT("openings"), TEXT("balcony") }, { TEXT("openings.keystones"), TEXT("openings"), TEXT("keystones") },
			{ TEXT("ornament.quoins"), TEXT("ornaments"), TEXT("quoins") }, { TEXT("ornament.lantern"), TEXT("ornaments"), TEXT("lantern") } };
		for (const auto& E : Extra)
		{
			const FObj It = Ob(Ob(K, E[1]), E[2]);
			if (!Allowed(It, D)) continue;
			const FString Nm = Str(It, TEXT("name"));
			FString NoName = Nm;
			if (NoName.Len()) NoName[0] = FChar::ToLower(NoName[0]);
			Menu.Add({ E[0], YesNo(Nm, TEXT("No ") + NoName) });
		}
		return Menu;
	}

	TArray<FString> Check(const FBrixBuilderRules& R, const FBrixDesignLot& L, const TSharedPtr<FJsonObject>& Old, const TSharedPtr<FJsonObject>& New,
		bool bWhole, const TArray<int32>& Held, TOptional<int64> Funds)
	{
		const FObj K = R.Kit, PL = R.Planning, Rm = Ob(PL, TEXT("remodel"));
		TArray<FString> Out;
		const TArray<FString> Ch = Changed(Old, New);
		if (!Ch.Num()) return { TEXT("nothing has changed") };
		const TArray<FString> Outside = Strs(Rm, TEXT("outside")), Inside = Strs(Rm, TEXT("inside"));
		bool bOut = false;
		for (const FString& P : Ch) { if (!(Part(P, Outside) || Part(P, Inside))) Out.Add(TEXT("a remodel cannot change ") + P); bOut |= Part(P, Outside); }
		if (bOut && !bWhole) Out.Add(TEXT("changing the outside needs the whole building"));
		const TArray<FVal> Of = Arr(Ob(Old, TEXT("inside")), TEXT("floors")), Nf = Arr(Ob(New, TEXT("inside")), TEXT("floors"));
		const TArray<FVal> Ow = Arr(Ob(Old, TEXT("inside")), TEXT("walls")), Nw = Arr(Ob(New, TEXT("inside")), TEXT("walls"));
		auto At = [](const TArray<FVal>& A, int32 f) { return A.IsValidIndex(f) ? A[f] : FVal(); };
		for (int32 f = 0; f < L.Floors; ++f)
			if ((!Same(At(Of, f), At(Nf, f)) || !Same(At(Ow, f), At(Nw, f))) && !(bWhole || Held.Contains(f))) Out.Add(FString::Printf(TEXT("floor %d is not yours to change"), f));
		const TArray<FString> Before = Problems(R, Old);
		for (const FString& P : Problems(R, New)) if (!Before.Contains(P)) Out.Add(P);
		const FObj Pal = Ob(Ob(PL, TEXT("districts")), L.District);
		if (!Pal.IsValid()) { Out.Add(TEXT("no planning palette for ") + L.District); return Out; }
		const TArray<FString> PalPlaster = Strs(Pal, TEXT("plaster")), PalStone = Strs(Pal, TEXT("stone")), CivicStone = Strs(Ob(PL, TEXT("civic_only")), TEXT("stone")), Brick = Strs(PL, TEXT("brick_to_eaves"));
		for (const TCHAR* F : Facades)
		{
			const FObj Fa = Ob(Ob(New, TEXT("walls")), F);
			const FString Where = FacadeName(F);
			const FVal StV = Fv(Ob(Fa, TEXT("stone")), TEXT("pattern"));
			const FString St = JsStr(StV);
			if (Ch.Contains(FString(TEXT("walls.")) + F + TEXT(".plaster")) && !StrIn(Fv(Fa, TEXT("plaster")), PalPlaster)) Out.Add(TEXT("that plaster colour is not one of ") + L.District + TEXT("'s"));
			bool bStone = false;
			for (const FString& P : Ch) bStone |= Match(P, FString(TEXT("walls.")) + F + TEXT(".stone"));
			if (bStone)
			{
				if (StrIn(StV, CivicStone)) { if (!Civic(R, L)) Out.Add(St + TEXT(" stone is kept for the town's civic buildings")); }
				else if (!StrIn(StV, PalStone)) Out.Add(St + TEXT(" is not one of ") + L.District + TEXT("'s stones or bricks"));
				const TArray<FVal> Cs = IsNull(StV) ? TArray<FVal>() : Arr(R.Patterns, St);
				const FVal C = Fv(Ob(Fa, TEXT("stone")), TEXT("color"));
				if (!IsNull(C))
				{
					const bool bInt = C->Type == EJson::Number && C->AsNumber() == FMath::FloorToDouble(C->AsNumber());
					const int32 c = bInt ? (int32)C->AsNumber() : -1;
					if (!bInt || c < 0 || c >= Cs.Num() || Cs[c]->Type == EJson::String) Out.Add(TEXT("that colour is not one ") + St + TEXT(" comes in"));
				}
			}
			if (Bool(Fa, TEXT("brick_to_eaves")) && !StrIn(StV, Brick)) Out.Add(TEXT("only brick can run to the eaves (the ") + Where + TEXT(" facade)"));
		}
		if (Ch.Contains(TEXT("roof.tiles")) && !StrIn(Fv(Ob(New, TEXT("roof")), TEXT("tiles")), Strs(Pal, TEXT("roof_tiles")))) Out.Add(TEXT("those roof tiles are not ") + L.District + TEXT("'s"));
		const FObj Op = Ob(New, TEXT("openings"));
		if (Ch.Contains(TEXT("openings.shutter_color")))
		{
			const FString List = Str(Ob(PL, TEXT("shutters")), Str(Op, TEXT("class")) == TEXT("old") ? TEXT("old") : TEXT("default"));
			if (!StrIn(Fv(Op, TEXT("shutter_color")), Strs(K, List))) Out.Add(TEXT("that shutter colour is not for these windows"));
		}
		const TCHAR* Extra[5][3] = { { TEXT("openings.shutters"), TEXT("openings"), TEXT("shutters") }, { TEXT("openings.balcony"), TEXT("openings"), TEXT("balcony") },
			{ TEXT("openings.keystones"), TEXT("openings"), TEXT("keystones") }, { TEXT("ornament.quoins"), TEXT("ornaments"), TEXT("quoins") }, { TEXT("ornament.lantern"), TEXT("ornaments"), TEXT("lantern") } };
		for (const auto& E : Extra)
		{
			const FVal V = Get(New, E[0]);
			bool b = false;
			if (Ch.Contains(E[0]) && V.IsValid() && V->TryGetBool(b) && b && !Allowed(Ob(Ob(K, E[1]), E[2]), New))
				Out.Add(Lower(Str(Ob(Ob(K, E[1]), E[2]), TEXT("name"))) + TEXT(" cannot go on this building"));
		}
		if (Funds.IsSet())
		{
			TArray<FBrixCostLine> Ln;
			const int64 Total = Cost(R, L, Old, New, Ln);
			if (Total > *Funds) Out.Add(TEXT("it costs ") + PyNum(Total / 100.0) + TEXT(" brix and you have ") + PyNum(*Funds / 100.0));
		}
		TArray<FString> Uniq;
		for (const FString& S : Out) Uniq.AddUnique(S);
		return Uniq;
	}
}
