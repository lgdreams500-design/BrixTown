#include "BrixStreets.h"

#include "BrixCity.h"
#include "BrixDay.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace BrixSlice
{
	const FVector3d Stone(.52, .48, .43), StoneDark(.37, .34, .32), Paving(.46, .42, .37), Path(.48, .44, .38), Grass(.3, .35, .2),
		Water(.2, .32, .38), Brown(.24, .16, .11), Olive(.30, .33, .20), Plum(.30, .24, .34), Charcoal(.09, .09, .11), Cream(.66, .61, .50),
		Ochre(.52, .40, .20), Terracotta(.44, .24, .18), Teal(.20, .36, .37), Blue(.30, .38, .52), Lantern(1, .55, .22), Metal(.17, .18, .21),
		Soil(.2, .15, .11), Wood(.34, .23, .15);

	// Number::toString: the shortest digits that read back as the same double, laid out as ECMAScript lays them out
	FString Js(double V)
	{
		if (V != V) return TEXT("NaN");
		if (V == 0) return TEXT("0");
		if (!FMath::IsFinite(V)) return V > 0 ? TEXT("Infinity") : TEXT("-Infinity");
		char Buf[64];
		for (int32 P = 1; P <= 17; ++P)
		{
			std::snprintf(Buf, sizeof(Buf), "%.*e", P - 1, V);
			if (std::strtod(Buf, nullptr) == V) break;
		}
		// Buf: [-]d[.ddd]e[+-]xx
		FString S(ANSI_TO_TCHAR(Buf)), Sign;
		if (S.StartsWith(TEXT("-"))) { Sign = TEXT("-"); S.RightChopInline(1); }
		int32 E = S.Find(TEXT("e"));
		const int32 Exp = FCString::Atoi(*S.Mid(E + 1));
		FString Digits = S.Left(E).Replace(TEXT("."), TEXT(""));
		while (Digits.Len() > 1 && Digits.EndsWith(TEXT("0"))) Digits.LeftChopInline(1);
		const int32 k = Digits.Len(), n = Exp + 1;
		FString Out;
		if (k <= n && n <= 21) { Out = Digits; for (int32 i = 0; i < n - k; ++i) Out += TEXT("0"); }
		else if (0 < n && n <= 21) Out = Digits.Left(n) + TEXT(".") + Digits.Mid(n);
		else if (-6 < n && n <= 0) { Out = TEXT("0."); for (int32 i = 0; i < -n; ++i) Out += TEXT("0"); Out += Digits; }
		else
		{
			Out = Digits.Left(1);
			if (k > 1) Out += TEXT(".") + Digits.Mid(1);
			Out += FString::Printf(TEXT("e%s%d"), n - 1 >= 0 ? TEXT("+") : TEXT("-"), FMath::Abs(n - 1));
		}
		return Sign + Out;
	}

	double Hash(std::initializer_list<FString> Parts) { return BrixSliceHash(Parts); }
}

using namespace BrixSlice;

namespace
{
	FString N(double V) { return Js(V); }
	FString N(int32 V) { return FString::FromInt(V); }
	const double QPi = 0.7853981633974483;   // Math.PI / 4

	struct FOut
	{
		TArray<FBrixShape>& S;
		void Box(double x0, double y0, double z0, double x1, double y1, double z1, const FVector3d& C)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Box; B.Col = C; B.A = { x0, y0, z0, x1, y1, z1 };
		}
		void YBox(double cx, double y0, double cz, double w, double h, double d, double yaw, const FVector3d& C)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::YBox; B.Col = C; B.A = { cx, y0, cz, w, h, d, yaw };
		}
		void Flat(double x0, double z0, double x1, double z1, double y, const FVector3d& C)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Flat; B.Col = C; B.A = { x0, z0, x1, z1, y };
		}
		void Solid(double x0, double y0, double z0, double x1, double y1, double z1)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Solid; B.A = { x0, y0, z0, x1, y1, z1 };
		}
		void Quad(const FVector3d& P0, const FVector3d& P1, const FVector3d& P2, const FVector3d& P3, const FVector3d& C, const FVector3d& Hint)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Quad; B.Col = C;
			B.A = { P0.X, P0.Y, P0.Z, P1.X, P1.Y, P1.Z, P2.X, P2.Y, P2.Z, P3.X, P3.Y, P3.Z, Hint.X, Hint.Y, Hint.Z };
		}
		double Dome(double x, double y0, double z, double R, double H, const FVector3d& C, double Seed)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Dome; B.Col = C; B.A = { x, y0, z, R, H, Seed };
			return y0 + H * .5;
		}
		void Disc(double x, double z, double r, double y, const FVector3d& C)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Disc; B.Col = C; B.A = { x, z, r, y };
		}
		void Ribbon(const TArray<FVector2D>& Pts, double w, double y, const FVector3d& C)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Ribbon; B.Col = C; B.A = { w, y }; B.Pts = Pts;
		}
		void Text(const FString& T, double x, double y, double z, double nx, double nz, double h)
		{
			FBrixShape& B = S.AddDefaulted_GetRef(); B.Kind = FBrixShape::Label; B.Text = T; B.A = { x, y, z, nx, nz, h };
		}
	};

	FVector3d Mix3(const FVector3d& A, const FVector3d& B, double T) { return A + (B - A) * T; }

	double Numf(const TSharedPtr<FJsonObject>& J, const TCHAR* F, double Def = 0)
	{
		double V = Def;
		if (J.IsValid()) J->TryGetNumberField(F, V);
		return V;
	}
	FString Strf(const TSharedPtr<FJsonObject>& J, const TCHAR* F)
	{
		FString V;
		if (J.IsValid()) J->TryGetStringField(F, V);
		return V;
	}
	FVector3d Col3(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 3 ? FVector3d(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber()) : FVector3d::ZeroVector;
	}
	FVector3d Col3f(const TSharedPtr<FJsonObject>& J, const TCHAR* F, const FVector3d& Def)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		return J.IsValid() && J->TryGetArrayField(F, A) && A->Num() >= 3 ? Col3(*A) : Def;
	}

	// play.js plantClump(): one plant in a border or drift: leaves, and flowers or dry heads on top
	void Clump(FOut& O, double px, double y, double pz, const FBrixPlantLook* st, double sc, double seed)
	{
		if (!st || st->Size <= 0) return;
		const double h = st->Height * (.75 + .5 * Hash({ N(seed), TEXT("h") })) * st->Size * sc, w = (.14 + .5 * st->Height) * sc * (.8 + .4 * Hash({ N(seed), TEXT("w") }));
		if (st->Leaf > .05)
		{
			const FVector3d lc = st->Color * (.85 + .3 * Hash({ N(seed), TEXT("c") }));
			const double yaw = Hash({ N(seed) }) * 3, hl = h * (st->bEvergreen ? .9 : .7);
			O.YBox(px, y, pz, w, hl * .6, w * .9, yaw, lc * .9);
			O.YBox(px, y, pz, w * .7, hl, w * .65, yaw + QPi, lc);
		}
		else O.YBox(px, y, pz, .05, h * .6, .05, 0, FVector3d(.3, .26, .2));
		const FVector3d* fl = st->Bloom > .05 ? &st->BloomColor : st->Spent > .05 ? &st->SpentColor : nullptr;
		if (!fl) return;
		const double k2 = st->Bloom > .05 ? st->Bloom : st->Spent * .8;
		const int32 n = st->Height > .42 ? 3 : 1;
		for (int32 i = 0; i < n; ++i)
		{
			const double s = (.08 + .1 * st->Height) * sc * (.6 + .4 * k2);
			const double ox = n > 1 ? (Hash({ N(seed), N(i), TEXT("x") }) - .5) * w * .8 : 0, oz = n > 1 ? (Hash({ N(seed), N(i), TEXT("z") }) - .5) * w * .8 : 0;
			O.YBox(px + ox, y + h * (.7 + .25 * Hash({ N(seed), N(i) })), pz + oz, s, s * (st->Height < .42 ? 1.2 : .9), s, QPi + i, *fl * (.85 + .3 * Hash({ N(seed), N(i), TEXT("f") })));
		}
	}

	// play.js heads(): flower heads (or dry ones) dotted over the top of a crown
	void Heads(FOut& O, double x, double cy, double z, double R, double H, int32 n, double size, const FVector3d& col, double seed)
	{
		for (int32 k = 0; k < n; ++k)
		{
			const double a = Hash({ N(seed), N(k), TEXT("a") }) * 6.283, e = .2 + 1.1 * Hash({ N(seed), N(k), TEXT("e") }), s = size * (.75 + .5 * Hash({ N(k), N(seed) }));
			O.YBox(x + FMath::Cos(a) * FMath::Cos(e) * R * .92, cy + FMath::Sin(e) * H * .52 - s * .4, z + FMath::Sin(a) * FMath::Cos(e) * R * .92, s, s * .8, s, QPi + a,
				col * (.85 + .3 * Hash({ N(seed), N(k), TEXT("c") })));
		}
	}

	double SegD(double px, double pz, const FVector2D& A, const FVector2D& Q)
	{
		const double dx = Q.X - A.X, dz = Q.Y - A.Y, L0 = dx * dx + dz * dz, L = L0 != 0 ? L0 : 1;
		const double t = FMath::Clamp(((px - A.X) * dx + (pz - A.Y) * dz) / L, 0.0, 1.0);
		return FMath::Sqrt(FMath::Square(px - A.X - t * dx) + FMath::Square(pz - A.Y - t * dz));
	}
}

bool LoadBrixStreetsToday(const FString& File, FBrixStreetsToday& Out, FString& OutError)
{
	Out = FBrixStreetsToday();
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *File)) { OutError = TEXT("Cannot read ") + File; return false; }
	TSharedPtr<FJsonObject> Root;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Root) || !Root.IsValid()) { OutError = File + TEXT(" is not valid JSON"); return false; }
	const TSharedPtr<FJsonObject>* Plants = nullptr;
	if (Root->TryGetObjectField(TEXT("plants"), Plants))
		for (const auto& KV : (*Plants)->Values)
		{
			const TSharedPtr<FJsonObject> J = KV.Value->AsObject();
			FBrixPlantLook& L = Out.Plants.Add(FString(KV.Key), FBrixPlantLook());
			L.Size = Numf(J, TEXT("size"), 1); L.Leaf = Numf(J, TEXT("leaf"), 1); L.Bloom = Numf(J, TEXT("bloom")); L.Spent = Numf(J, TEXT("spent"));
			L.Height = Numf(J, TEXT("height"), .5);
			L.Color = Col3f(J, TEXT("color"), L.Color); L.BloomColor = Col3f(J, TEXT("bloom_color"), L.BloomColor); L.SpentColor = Col3f(J, TEXT("spent_color"), L.SpentColor);
			J->TryGetBoolField(TEXT("evergreen"), L.bEvergreen);
		}
	const TSharedPtr<FJsonObject>* Fl = nullptr;
	if (Root->TryGetObjectField(TEXT("flowers"), Fl))
	{
		Out.FlowerSize = Numf(*Fl, TEXT("size")); Out.FlowerBloom = Numf(*Fl, TEXT("bloom"));
		const TArray<TSharedPtr<FJsonValue>>* Cs = nullptr;
		if ((*Fl)->TryGetArrayField(TEXT("colors"), Cs)) for (const TSharedPtr<FJsonValue>& C : *Cs) Out.FlowerColors.Add(Col3(C->AsArray()));
	}
	const TSharedPtr<FJsonObject>* Lw = nullptr;
	if (Root->TryGetObjectField(TEXT("lawn"), Lw))
	{
		const TArray<TSharedPtr<FJsonValue>>* Ts = nullptr;
		if ((*Lw)->TryGetArrayField(TEXT("tones"), Ts)) for (const TSharedPtr<FJsonValue>& C : *Ts) Out.LawnTones.Add(Col3(C->AsArray()));
		Out.EdgeM = Numf(*Lw, TEXT("edge_m"), Out.EdgeM); Out.TuftsPer100m2 = Numf(*Lw, TEXT("tufts_per_100m2"), Out.TuftsPer100m2); Out.TreeRingM = Numf(*Lw, TEXT("tree_ring_m"), Out.TreeRingM);
		Out.EdgeColor = Col3f(*Lw, TEXT("edge_color"), Out.EdgeColor); Out.TuftColor = Col3f(*Lw, TEXT("tuft_color"), Out.TuftColor); Out.MulchColor = Col3f(*Lw, TEXT("mulch_color"), Out.MulchColor);
	}
	if (Out.LawnTones.Num() < 3) Out.LawnTones = { FVector3d(.18, .27, .13), FVector3d(.29, .34, .19), FVector3d(.42, .39, .21) };
	const TSharedPtr<FJsonObject>* Dr = nullptr;
	if (Root->TryGetObjectField(TEXT("dressing"), Dr)) { Out.Dressing = *Dr; (*Dr)->TryGetBoolField(TEXT("bunting"), Out.bBunting); }
	return true;
}

void BrixPlantShapes(const FBrixProp& P, const FBrixGround& G, const FBrixStreetsToday& Today, TArray<FBrixShape>& Out)
{
	FOut O{ Out };
	const TSharedPtr<FJsonObject>& J = P.Json;
	const double x = P.Pos.X, z = P.Pos.Y;
	const double y = (P.Set == TEXT("wild") ? G.Wild : FMath::Max(G.Here, G.Grass)) + .03;
	const double Seed = Numf(J, TEXT("seed"));
	const double sd = Seed != 0 ? Seed : Hash({ N(x), N(z) }) * 9999;
	const FString Shape = Strf(J, TEXT("shape"));
	const FString Species = Strf(J, TEXT("species"));
	const FBrixPlantLook* st = Today.Plants.Find(Species);
	const bool ax = Strf(J, TEXT("along")) == TEXT("x");
	if (Shape == TEXT("hedge"))   // clipped box: sections a little uneven in height and tone, the top paler where it catches the sun
	{
		if (!st) return;
		const double L = Numf(J, TEXT("len")), dep = Numf(J, TEXT("depth")), ph = Numf(J, TEXT("h"));
		const int32 n = FMath::Max(1, (int32)JsRound(L / 1.3));
		for (int32 i = 0; i < n; ++i)
		{
			const double a0 = -L / 2 + L * i / n, a1 = -L / 2 + L * (i + 1) / n, hh = ph * (.95 + .08 * Hash({ N(sd), N(i) }));
			const FVector3d col = st->Color * (.88 + .22 * Hash({ N(sd), N(i), TEXT("c") })), top = Mix3(col, st->Color * 1.35, .5);
			const double b0 = ax ? x + a0 : x - dep / 2, b1 = ax ? z - dep / 2 : z + a0, b2 = ax ? x + a1 : x + dep / 2, b3 = ax ? z + dep / 2 : z + a1;
			O.Box(b0, y, b1, b2, y + hh - .07, b3, col);
			O.Box(b0 + .05, y + hh - .07, b1 + .05, b2 - .05, y + hh, b3 - .05, top);   // (its sides in col; the cap is mostly top)
		}
		if (ax) O.Solid(x - L / 2, y, z - dep / 2, x + L / 2, y + ph, z + dep / 2);
		else O.Solid(x - dep / 2, y, z - L / 2, x + dep / 2, y + ph, z + L / 2);
	}
	else if (Shape == TEXT("bush"))
	{
		if (!st) return;
		const double r = Numf(J, TEXT("r")), H = FMath::Min(st->Height, r * 1.7) * (.85 + .3 * Hash({ N(sd), TEXT("H") })), R = r * (.62 + .3 * st->Leaf);
		double cy = y + H * .42;
		if (st->Leaf > .05)   // a big dome with two smaller ones leaning off it, so it reads as a shrub, not a stone
		{
			for (int32 k = 0; k < 2; ++k)
			{
				const double a = sd + k * 2.6 + Hash({ N(sd), N(k) }) * 1.2, o = R * .55;
				O.Dome(x + FMath::Cos(a) * o, y, z + FMath::Sin(a) * o, R * .62, H * .72, st->Color * (.9 + .15 * k), std::fmod(sd + k * 11, 97.0));
			}
			cy = O.Dome(x, y + H * .08, z, R * .85, H * .95, st->Color, std::fmod(sd, 97.0));
		}
		else for (int32 k = 0; k < 7; ++k)   // bare twigs in winter
		{
			const double a = k / 7.0 * 6.283 + sd, l = H * (.7 + .3 * Hash({ N(sd), N(k) }));
			O.YBox(x + FMath::Cos(a) * r * .25, y, z + FMath::Sin(a) * r * .25, .06, l, .06, a, FVector3d(.3, .25, .2));
		}
		if (st->Bloom > .05) Heads(O, x, cy, z, R, H, (int32)JsRound((10 + 10 * r) * st->Bloom), .2 + .08 * r, st->BloomColor, sd);
		else if (st->Spent > .05) Heads(O, x, cy, z, R, H, (int32)JsRound(8 + 6 * r), .18 + .06 * r, st->SpentColor, sd + 7);
		O.Solid(x - r * .65, y, z - r * .65, x + r * .65, y + H * .8, z + r * .65);
	}
	else if (Shape == TEXT("border"))   // a bed of dark earth, the two kinds planted in drifts along it
	{
		const double L = Numf(J, TEXT("len")), dep = Numf(J, TEXT("depth"));
		TArray<FString> sp;
		J->TryGetStringArrayField(TEXT("species"), sp);
		if (ax) O.Flat(x - L / 2, z - dep / 2, x + L / 2, z + dep / 2, y - .005, Soil);
		else O.Flat(x - dep / 2, z - L / 2, x + dep / 2, z + L / 2, y - .005, Soil);
		if (!sp.Num()) return;
		const int32 n = (int32)JsRound(L / .5);
		for (int32 i = 0; i < n; ++i) for (const double row : { -.22, .22 })
		{
			const double t = -L / 2 + (i + .5 + (Hash({ N(sd), N(i), N(row) }) - .5) * .6) * L / n, v = row * dep + (Hash({ N(sd), N(row), N(i) }) - .5) * .12;
			const int32 which = (int32)FMath::FloorToDouble(std::fmod((t + L / 2) / 2.4 + Hash({ N(sd), TEXT("d") }), (double)sp.Num()));
			const FBrixPlantLook* s2 = Today.Plants.Find(sp[(which + (row > 0 && Hash({ N(sd), N(i) }) < .3 ? 1 : 0)) % sp.Num()]);
			Clump(O, ax ? x + t : x + v, y, ax ? z + v : z + t, s2, 1, sd * 31 + i * 7 + (row > 0 ? 3 : 0));
		}
	}
	else if (Shape == TEXT("drift"))   // flowers coming up in the grass, thinning out at the edge
	{
		if (!st || st->Size <= 0) return;
		const double r = Numf(J, TEXT("r")), hole = Numf(J, TEXT("hole"));
		const int32 n = FMath::Min(40, (int32)JsRound((r * r - hole * hole) * 2.6));
		for (int32 k = 0; k < n; ++k)
		{
			const double a = Hash({ N(sd), N(k), TEXT("a") }) * 6.283, d = hole + (r - hole) * FMath::Sqrt(Hash({ N(sd), N(k), TEXT("d") })) * (.7 + .3 * Hash({ N(k), N(sd) }));
			Clump(O, x + FMath::Cos(a) * d, y, z + FMath::Sin(a) * d, st, .8, sd + k * 13);
		}
	}
}

bool BrixWildShapes(const FBrixProp& P, const FBrixGround& G, TArray<FBrixShape>& Out)
{
	FOut O{ Out };
	const TSharedPtr<FJsonObject>& J = P.Json;
	const double x = P.Pos.X, z = P.Pos.Y, g = G.Here;
	const FString& k = P.Kind;
	const FVector3d Board(.12, .2, .16), Fence(.4, .3, .2), Straw(.62, .52, .3);
	if (k == TEXT("fence"))   // post and two rails
	{
		const bool ax = P.Along == TEXT("x");
		const double L = Numf(J, TEXT("len"));
		const int32 n = FMath::Max(1, (int32)JsRound(L / 3));
		for (int32 i = 0; i <= n; ++i) { const double t = -L / 2 + L * i / n, px = ax ? x + t : x, pz = ax ? z : z + t; O.Box(px - .07, g, pz - .07, px + .07, g + 1.15, pz + .07, Fence); }
		for (const double h : { .55, 1.0 })
		{
			if (ax) O.Box(x - L / 2, g + h, z - .03, x + L / 2, g + h + .1, z + .03, Fence * 1.1);
			else O.Box(x - .03, g + h, z - L / 2, x + .03, g + h + .1, z + L / 2, Fence * 1.1);
		}
		if (ax) O.Solid(x - L / 2, g, z - .1, x + L / 2, g + 1.1, z + .1);
		else O.Solid(x - .1, g, z - L / 2, x + .1, g + 1.1, z + L / 2);
	}
	else if (k == TEXT("board"))   // a "For sale" board on two posts, facing the track
	{
		double fx = 0, fz = 0;
		const TArray<TSharedPtr<FJsonValue>>* Fa = nullptr;
		if (J.IsValid() && J->TryGetArrayField(TEXT("face"), Fa) && Fa->Num() >= 2) { fx = (*Fa)[0]->AsNumber(); fz = (*Fa)[1]->AsNumber(); }
		const bool ax = fz != 0;
		const double w = 1.7;
		for (const double s : { -.75, .75 }) { const double px = x + (ax ? s : 0), pz = z + (ax ? 0 : s); O.Box(px - .06, g, pz - .06, px + .06, g + 2.1, pz + .06, Fence); }
		if (ax) O.Box(x - w / 2, g + 1.1, z - .04, x + w / 2, g + 2.0, z + .04, Board); else O.Box(x - .04, g + 1.1, z - w / 2, x + .04, g + 2.0, z + w / 2, Board);
		if (ax) O.Box(x - w / 2, g + 1.0, z - .05, x + w / 2, g + 1.1, z + .05, Terracotta); else O.Box(x - .05, g + 1.0, z - w / 2, x + .05, g + 1.1, z + w / 2, Terracotta);
		const FString T = Strf(J, TEXT("text"));
		O.Text(T.IsEmpty() ? TEXT("FOR SALE") : T, x + fx * .06, g + 1.55, z + fz * .06, fx, fz, .2);
		O.Solid(x - .8, g, z - .8, x + .8, g + 2, z + .8);
	}
	else if (k == TEXT("peg"))   // a survey peg with a painted top at each corner of a plot
	{
		O.Box(x - .05, g, z - .05, x + .05, g + .5, z + .05, Fence); O.Box(x - .055, g + .4, z - .055, x + .055, g + .52, z + .055, Ochre);
	}
	else if (k == TEXT("log"))   // a fallen trunk, lying where it fell, moss along its top
	{
		const double d = .45 + .1 * Hash({ P.Id, TEXT("d") }), len = Numf(J, TEXT("len")) != 0 ? Numf(J, TEXT("len")) : 3, yaw = Numf(J, TEXT("yaw"));
		O.YBox(x, g, z, len, d, d, yaw, FVector3d(.36, .29, .22)); O.YBox(x, g + d, z, len * .9, .05, d * .7, yaw, FVector3d(.3, .34, .18));
	}
	else if (k == TEXT("rock"))
	{
		const double r = Numf(J, TEXT("r")) != 0 ? Numf(J, TEXT("r")) : .8, sd = Numf(J, TEXT("seed")) != 0 ? Numf(J, TEXT("seed")) : 1;
		O.YBox(x, g - .05, z, r * 1.6, r * .7, r * 1.2, sd, Stone * (.85 + .2 * Hash({ N(sd), TEXT("c") }))); O.YBox(x, g - .05, z, r * 1.1, r, r * .9, sd + .8, Stone * .95);
		O.Solid(x - r * .7, g, z - r * .7, x + r * .7, g + r * .9, z + r * .7);
	}
	else if (k == TEXT("picnic"))   // a timber table with a bench along each side
	{
		const double yaw = Numf(J, TEXT("yaw")), c = FMath::Cos(yaw), s = FMath::Sin(yaw);
		const FVector3d W(.5, .34, .18);
		O.YBox(x, g + .72, z, 2.0, .06, .8, yaw, W); O.YBox(x, g, z, .12, .72, .7, yaw, W * .8);
		for (const double o : { -.75, .75 }) O.YBox(x - s * o, g + .42, z + c * o, 2.0, .05, .3, yaw, W);
		O.Solid(x - 1.1, g, z - 1.1, x + 1.1, g + .78, z + 1.1);
	}
	else if (k == TEXT("haybale"))   // bales in twos, one on top of the other now and then
	{
		const double yaw = Numf(J, TEXT("yaw"));
		const bool two = Hash({ P.Id, TEXT("up") }) < .3;
		O.YBox(x, g, z, 1.2, .8, .9, yaw, Straw); if (two) O.YBox(x, g + .8, z, 1.2, .8, .9, yaw + .2, Straw * 1.08);
		O.Solid(x - .7, g, z - .7, x + .7, g + (two ? 1.6 : .8), z + .7);
	}
	else if (k == TEXT("crop"))   // a field: what grows in it, in rows along it
	{
		const FVector2D Sz = [&]() { const TArray<TSharedPtr<FJsonValue>>* A = nullptr; return J->TryGetArrayField(TEXT("size"), A) && A->Num() >= 2 ? FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) : FVector2D::ZeroVector; }();
		const double w = Sz.X, d = Sz.Y, x0 = x - w / 2, z0 = z - d / 2, y = G.Wild + .012;
		const FString crop = Strf(J, TEXT("crop"));
		const FVector3d Furrow(.24, .18, .13), Cabbage(.19, .29, .17);
		const FVector3d col = crop == TEXT("wheat") ? FVector3d(.52, .45, .22) : crop == TEXT("barley") ? FVector3d(.48, .45, .27) : crop == TEXT("furrow") ? Furrow
			: crop == TEXT("pasture") ? FVector3d(.27, .34, .17) : crop == TEXT("cabbage") ? Cabbage : FVector3d(.33, .36, .2);
		O.Flat(x0, z0, x0 + w, z0 + d, y, crop == TEXT("furrow") || crop == TEXT("cabbage") ? Furrow : col);
		if (crop == TEXT("wheat") || crop == TEXT("barley"))   // the standing crop: a level top just under waist height, a little uneven
		{
			for (int32 i = 0; i < w / 6; ++i) { const double a = x0 + i * 6, c = FMath::Min(x0 + w, a + 6); O.Box(a, y, z0, c, y + .75 + .08 * Hash({ P.Id, N(i) }), z0 + d, col * (.92 + .16 * Hash({ N(i), P.Id }))); }
		}
		else if (crop == TEXT("furrow") || crop == TEXT("cabbage"))   // ridges of earth, or rows of cabbages along them
		{
			for (double a = x0 + .6; a < x0 + w - .3; a += 1.3)
				O.Box(a - .3, y, z0, a + .3, y + (crop == TEXT("cabbage") ? .32 : .14), z0 + d, crop == TEXT("cabbage") ? Cabbage * (.9 + .2 * Hash({ N(a), P.Id })) : Furrow * 1.15);
		}
		else if (crop == TEXT("rough")) O.Flat(x0 + 1, z0 + 1, x0 + w - 1, z0 + d - 1, y + .003, col * 1.08);   // mown inside its fence
	}
	else if (k == TEXT("pond"))   // still water in a ring of reeds
	{
		const double r = Numf(J, TEXT("r")), y = G.Wild + .01;
		O.Disc(x, z, r + 1.2, y, Soil * 1.2); O.Disc(x, z, r, y + .01, Water);
		for (int32 i = 0; i < r * 2.2; ++i)
		{
			const double a = i / (r * 2.2) * 6.283 + Hash({ P.Id, N(i) }) * .3, rr = r + .2 + .6 * Hash({ N(i), P.Id });
			if (Hash({ P.Id, N(i), TEXT("gap") }) < .3) continue;
			for (int32 q = 0; q < 4; ++q)
				O.YBox(x + FMath::Cos(a) * rr + (Hash({ N(i), N(q), TEXT("rx") }) - .5) * .5, y, z + FMath::Sin(a) * rr + (Hash({ N(q), N(i), TEXT("rz") }) - .5) * .5, .06,
					.8 + .7 * Hash({ N(i), N(q), TEXT("h") }), .06, a + q, FVector3d(.3, .38, .18) * (.85 + .3 * Hash({ N(q), N(i) })));
		}
		O.Solid(x - r * .7, 0, z - r * .7, x + r * .7, 1, z + r * .7);
	}
	else return false;
	return true;
}

void BrixLawnShapes(double x0, double z0, double x1, double z1, double y, const FVector3d& c, const FBrixBlock& B, bool bTufts, const FBrixStreetsToday& T, TArray<FBrixShape>& Out)
{
	FOut O{ Out };
	O.Flat(x0, z0, x1, z1, y, c);
	const double e = T.EdgeM, ye = y + .006;
	const FVector3d ec = T.EdgeColor * (c.Y / T.LawnTones[1].Y);
	O.Flat(x0, z0, x1, z0 + e, ye, ec); O.Flat(x0, z1 - e, x1, z1, ye, ec);
	O.Flat(x0, z0 + e, x0 + e, z1 - e, ye, ec); O.Flat(x1 - e, z0 + e, x1, z1 - e, ye, ec);
	if (!bTufts) return;
	auto Clear = [&](double px, double pz)
	{
		for (const TPair<FVector2D, FVector2D>& Pa : B.Paths) if (SegD(px, pz, Pa.Key, Pa.Value) < 1.4) return false;
		for (const FBrixProp& p : B.Props)
		{
			const double len = Numf(p.Json, TEXT("len"));
			if (len != 0)   // a hedge or border, as a line (one with no depth, a fence, never stands in the way: NaN in the slice)
			{
				double dep = 0;
				if (!p.Json.IsValid() || !p.Json->TryGetNumberField(TEXT("depth"), dep)) continue;
				const double h = len / 2;
				const bool ax = p.Along == TEXT("x");
				const FVector2D a = ax ? FVector2D(p.Pos.X - h, p.Pos.Y) : FVector2D(p.Pos.X, p.Pos.Y - h), q = ax ? FVector2D(p.Pos.X + h, p.Pos.Y) : FVector2D(p.Pos.X, p.Pos.Y + h);
				if (SegD(px, pz, a, q) < dep / 2 + .5) return false;
				continue;
			}
			const double r = Numf(p.Json, TEXT("r"));
			const double lim = p.Kind == TEXT("fountain") ? 3.6 : p.Kind == TEXT("playground") ? 9 : Strf(p.Json, TEXT("shape")) == TEXT("drift") ? r : r != 0 ? r + .4 : 1.7;
			if (FMath::Sqrt(FMath::Square(p.Pos.X - px) + FMath::Square(p.Pos.Y - pz)) < lim) return false;
		}
		return true;
	};
	const int32 n = (int32)JsRound((x1 - x0) * (z1 - z0) / 100 * T.TuftsPer100m2);
	const double W = x1 - x0, H = z1 - z0;
	for (int32 i = 0; i < n; ++i)
	{
		const double r1 = Hash({ N(x0), N(z0), TEXT("tu"), N(i) }), r2 = Hash({ N(z0), N(x0), TEXT("tv"), N(i) });
		const int32 side = (int32)FMath::FloorToDouble(Hash({ N(i), N(x0), TEXT("ts") }) * 5);
		double px, pz;
		if (side < 4)
		{
			const double off = .25 + 1.1 * Hash({ N(i), N(z0), TEXT("to") });
			px = side < 2 ? x0 + r1 * W : (side == 2 ? x0 + off : x1 - off);
			pz = side >= 2 ? z0 + r2 * H : (side == 0 ? z0 + off : z1 - off);
		}
		else { px = x0 + .5 + r1 * (W - 1); pz = z0 + .5 + r2 * (H - 1); }
		if (!Clear(px, pz)) continue;
		const double s = .7 + .6 * Hash({ N(pz), N(px), TEXT("sz") });
		const FVector3d tc = T.TuftColor * (.88 + .24 * Hash({ N(i), TEXT("tc") }));
		for (int32 k = 0; k < 6; ++k)   // a clump of faceted spikes leaning out from one root, like the faceted tree crowns
		{
			const double a = (k / 6.0 + .15 * Hash({ N(px), N(pz), N(k) })) * 6.283, ca = FMath::Cos(a), sa = FMath::Sin(a), rx = px + ca * .06, rz = pz + sa * .06, w = .05 * s;
			const FVector3d tip(px + ca * (.14 + .1 * Hash({ N(k), N(px) })) * s, y + (.2 + .2 * Hash({ N(pz), N(k) })) * s, pz + sa * (.14 + .1 * Hash({ N(k), N(pz) })) * s);
			const FVector3d p0(rx - sa * w, y, rz + ca * w), p1(rx + sa * w, y, rz - ca * w), p2(rx - ca * w, y, rz - sa * w);
			const FVector3d col = tc * (.85 + .3 * Hash({ N(k), N(i) }));
			O.Quad(p0, p1, tip, tip, col, FVector3d(ca, .5, sa));
			O.Quad(p1, p2, tip, tip, col * .85, FVector3d(sa - ca, .5, -ca - sa));
			O.Quad(p2, p0, tip, tip, col * .9, FVector3d(-sa - ca, .5, ca - sa));
		}
	}
}

bool BrixIsWildTemplate(const FString& T)
{
	return T == TEXT("woods") || T == TEXT("common") || T == TEXT("orchard") || T == TEXT("fields") || T == TEXT("plots") || T == TEXT("meadow");
}

void BrixGreenGroundShapes(const FBrixBlock& B, const FBrixGround& G, const FBrixStreetsToday& T, TArray<FBrixShape>& Out)
{
	FOut O{ Out };
	const double bx = B.Origin.X, bz = B.Origin.Y, SX = B.Extent.X, SZ = B.Extent.Y;
	const FVector3d WoodFloor(.19, .2, .12), Gravel(.44, .4, .33);
	if (B.Template == TEXT("woods"))   // leaf litter under the trees, in patches of a few tones
	{
		O.Flat(bx, bz, bx + SX, bz + SZ, G.Wild, WoodFloor);
		for (int32 i = 0; i < SX / 16; ++i) for (int32 j = 0; j < SZ / 16; ++j) if (Hash({ B.Id, N(i), N(j), TEXT("lit") }) < .45)
		{
			const double x = bx + i * 16 + 4 * Hash({ N(i), N(j), B.Id }), z = bz + j * 16 + 4 * Hash({ N(j), N(i), B.Id }), w = 6 + 8 * Hash({ B.Id, N(j), N(i) });
			O.Flat(x, z, FMath::Min(bx + SX, x + w), FMath::Min(bz + SZ, z + w * (.6 + .6 * Hash({ N(i), B.Id, N(j) }))), G.Wild + .004, WoodFloor * (Hash({ N(i), N(j), TEXT("tn") }) < .5 ? 1.25 : .82));
		}
	}
	else BrixLawnShapes(bx, bz, bx + SX, bz + SZ, G.Wild, B.Template == TEXT("fields") ? Grass * .92 : Grass, B, B.Template == TEXT("meadow"), T, Out);
	for (const FBrixTrack& Tr : B.Tracks) O.Ribbon(Tr.Points, Tr.Width, G.Wild + .015, Gravel);
}

void BrixDomeMesh(double seed, TArray<FVector3d>& OutTris, TArray<double>& OutShade)
{
	// play.js dome(x 0, y0 0, z 0, R 1, H 1, col, seed) without the seed in the angle (a turn of the whole)
	const int32 Nn = 7, M = 3;
	const double cy = .5;
	TArray<TArray<FVector3d>> Ring;
	for (int32 k = 0; k <= M; ++k)
	{
		const double f = -.55 + (UE_DOUBLE_PI / 2 + .55) * k / M;
		TArray<FVector3d>& Row = Ring.AddDefaulted_GetRef();
		for (int32 i = 0; i < Nn; ++i)
		{
			const double a = i / (double)Nn * 6.2832 + k * .5, rr = (.82 + .36 * Hash({ N(seed), N(k), N(i) })) * (k == M ? 0 : FMath::Cos(f));
			Row.Add(FVector3d(FMath::Cos(a) * rr, FMath::Max(0.0, cy + FMath::Sin(f) * .58 * (k == M ? 1 : .85 + .3 * Hash({ N(i), N(seed), N(k) }))), FMath::Sin(a) * rr));
		}
	}
	for (int32 k = 0; k < M; ++k) for (int32 i = 0; i < Nn; ++i)
	{
		const int32 j = (i + 1) % Nn;
		const FVector3d a = Ring[k][i], b = Ring[k][j], c = Ring[k + 1][j], d = Ring[k + 1][i];
		const double cc = .74 + .5 * Hash({ N(seed), N(k), N(i) }) + .07 * k;
		OutTris.Append({ a, b, c }); OutShade.Add(cc);
		if (k < M - 1) { OutTris.Append({ a, c, d }); OutShade.Add(cc); }
	}
}

double BrixShapesChecksum(const TArray<FBrixShape>& Shapes)
{
	// the same sum as Tools/unreal/streets_values.py checksum()
	double S = 0;
	for (const FBrixShape& Sh : Shapes)
	{
		S += (Sh.Kind + 1) * 1000.0;
		for (int32 j = 0; j < Sh.A.Num(); ++j) S += Sh.A[j] * (1 + (j % 5) * .1);
		S += Sh.Col.X * 3 + Sh.Col.Y * 5 + Sh.Col.Z * 7;
		for (const FVector2D& P : Sh.Pts) S += P.X * .3 + P.Y * .7;
	}
	return S;
}

FBrixM4 FBrixM4::T(double X, double Y, double Z) { FBrixM4 R; R.M[12] = X; R.M[13] = Y; R.M[14] = Z; return R; }
FBrixM4 FBrixM4::Rx(double A) { const double c = FMath::Cos(A), s = FMath::Sin(A); FBrixM4 R; R.M[5] = c; R.M[6] = s; R.M[9] = -s; R.M[10] = c; return R; }
FBrixM4 FBrixM4::Ry(double A) { const double c = FMath::Cos(A), s = FMath::Sin(A); FBrixM4 R; R.M[0] = c; R.M[2] = -s; R.M[8] = s; R.M[10] = c; return R; }
FBrixM4 FBrixM4::Rz(double A) { const double c = FMath::Cos(A), s = FMath::Sin(A); FBrixM4 R; R.M[0] = c; R.M[1] = s; R.M[4] = -s; R.M[5] = c; return R; }
FBrixM4 FBrixM4::operator*(const FBrixM4& B) const
{
	FBrixM4 O;
	for (int32 c = 0; c < 4; ++c) for (int32 r = 0; r < 4; ++r)
		O.M[c * 4 + r] = M[r] * B.M[c * 4] + M[4 + r] * B.M[c * 4 + 1] + M[8 + r] * B.M[c * 4 + 2] + M[12 + r] * B.M[c * 4 + 3];
	return O;
}
FVector3d FBrixM4::Pt(double X, double Y, double Z) const
{
	return FVector3d(M[0] * X + M[4] * Y + M[8] * Z + M[12], M[1] * X + M[5] * Y + M[9] * Z + M[13], M[2] * X + M[6] * Y + M[10] * Z + M[14]);
}

void BrixOBox(TArray<FBrixShape>& Out, const FBrixM4& M, double Cx, double Cy, double Cz, double Sx, double Sy, double Sz, const FVector3d& Col)
{
	FBrixShape& B = Out.AddDefaulted_GetRef();
	B.Kind = FBrixShape::OBox; B.Col = Col;
	for (int32 i = 0; i < 16; ++i) B.A.Add(M.M[i]);
	B.A.Append({ Cx, Cy, Cz, Sx, Sy, Sz });
}

namespace
{
	// dressing.js's colours and PAINT
	const FVector3d Rubber(.5, .28, .22), Sand(.72, .62, .44), DWood(.5, .32, .16), Iron(.17, .18, .21), Red(.58, .12, .1), DGreen(.16, .3, .22), Bronze(.3, .42, .36);
	const FVector3d& Paint(double r)
	{
		static const FVector3d P[4] = { Teal, Ochre, Terracotta, Blue };
		return P[FMath::Min(3, (int32)FMath::FloorToDouble(r * 4))];
	}
}

bool BrixDressShapes(const FBrixProp& P, const FBrixBlock& Blk, double g, const FBrixStreetsToday& Today, TArray<FBrixShape>& Out)
{
	FOut O{ Out };
	const TSharedPtr<FJsonObject>& J = P.Json;
	const double x = P.Pos.X, z = P.Pos.Y;
	const FString& k = P.Kind;
	if (k == TEXT("playground"))   // soft red ground with a pale edge
	{
		const double hw = Numf(J, TEXT("w")) / 2, hd = Numf(J, TEXT("d")) / 2;
		O.Flat(x - hw - .15, z - hd - .15, x + hw + .15, z + hd + .15, g + .015, Cream); O.Flat(x - hw, z - hd, x + hw, z + hd, g + .025, Rubber);
	}
	else if (k == TEXT("swing"))   // an A-frame at each end and a beam; the seats hang from it (BrixPlay)
	{
		const double n = Numf(J, TEXT("seats")), half = n * 1.1 / 2 + .3;
		const FVector3d col = Paint(Hash({ P.Id }));
		for (const double s : { -1.0, 1.0 }) for (const double v : { -.75, .75 }) O.YBox(x + s * half, g, z + v * .5, .1, 2.45, .1, 0, col);
		for (const double s : { -1.0, 1.0 }) O.Box(x + s * half - .06, g + .9, z - .45, x + s * half + .06, g + .98, z + .45, col);
		O.Box(x - half - .05, g + 2.32, z - .06, x + half + .05, g + 2.44, z + .06, col);
		for (const double s : { -1.0, 1.0 }) O.Solid(x + s * half - .1, g, z - .5, x + s * half + .1, g + 2.4, z + .5);
	}
	else if (k == TEXT("slide"))   // ladder up to a railed top, a shiny chute down to the ground
	{
		const FVector3d col = Paint(Hash({ P.Id, TEXT("c") })), Shine(.72, .72, .74);
		for (const double v : { -.3, .3 }) { O.Box(x - 1.95, g, z + v - .04, x - 1.85, g + 2.3, z + v + .04, Iron); O.Box(x - 1.25, g, z + v - .04, x - 1.15, g + 2.3, z + v + .04, Iron); }
		for (double r = .3; r < 1.6; r += .3) O.Box(x - 1.95, g + r, z - .3, x - 1.85, g + r + .04, z + .3, Iron);
		O.Box(x - 1.95, g + 1.5, z - .36, x - 1.15, g + 1.58, z + .36, col);
		for (const double v : { -.3, .3 }) O.Box(x - 1.95, g + 2.2, z + v - .03, x - 1.15, g + 2.26, z + v + .03, Iron);   // a rail each side: open above
		const double t0 = x - 1.15, t1 = g + 1.55, b0 = x + 2.1, b1 = g + .32, L = FMath::Sqrt(FMath::Square(b0 - t0) + FMath::Square(b1 - t1)), ang = FMath::Atan2(t1 - b1, b0 - t0);
		const FBrixM4 m = (FBrixM4::T((t0 + b0) / 2, (t1 + b1) / 2, z) * FBrixM4::Ry(0)) * FBrixM4::Rz(-ang);
		BrixOBox(Out, m, 0, 0, 0, L, .05, .5, Shine);
		for (const double v : { -.27, .27 }) BrixOBox(Out, m, 0, .1, v, L, .2, .04, col);
		O.Box(b0, g, z - .3, b0 + .45, g + .3, z + .3, Shine);
		O.Solid(x - 1.95, g, z - .36, x - 1.15, g + 2.3, z + .36);
	}
	else if (k == TEXT("seesaw"))   // the plank rocks (BrixPlay); here the pivot
	{
		O.Box(x - .2, g, z - .25, x + .2, g + .42, z + .25, Iron); O.Box(x - .05, g + .42, z - .3, x + .05, g + .52, z + .3, Iron);
	}
	else if (k == TEXT("sandpit"))   // a wooden kerb round the sand, a bucket and a spade
	{
		const double w = Numf(J, TEXT("w")), h = (w != 0 ? w : 3.4) / 2;
		for (const double s : { -1.0, 1.0 }) { O.Box(x - h, g, z + s * h - .12, x + h, g + .28, z + s * h + .12, DWood); O.Box(x + s * h - .12, g, z - h, x + s * h + .12, g + .28, z + h, DWood); }
		O.Flat(x - h + .12, z - h + .12, x + h - .12, z + h - .12, g + .2, Sand);
		O.Box(x + .5, g + .2, z - .3, x + .72, g + .42, z - .08, Blue); O.YBox(x - .6, g + .2, z + .4, .08, .04, .5, .6, Ochre);
		for (int32 q = 0; q < 3; ++q) O.YBox(x - .9 + q * .7, g + .2, z - .8 + .5 * Hash({ P.Id, N(q) }), .4 + .2 * Hash({ N(q), P.Id }), .12, .3, Hash({ P.Id, N(q), TEXT("3") }) * 3, Sand * .92);
	}
	else if (k == TEXT("climber"))   // a frame of painted bars with a deck halfway up
	{
		const FVector3d col = Paint(Hash({ P.Id, TEXT("cl") }));
		const double h = 1.0;
		for (const double sx : { -h, h }) for (const double sz : { -h, h }) O.Box(x + sx - .04, g, z + sz - .04, x + sx + .04, g + 1.9, z + sz + .04, col);
		for (const double y : { .5, 1.1, 1.85 }) for (const double s : { -h, h }) { O.Box(x - h, g + y, z + s - .03, x + h, g + y + .06, z + s + .03, col); O.Box(x + s - .03, g + y, z - h, x + s + .03, g + y + .06, z + h, col); }
		O.Box(x - h, g + .84, z - h, x + h, g + .9, z + h, DWood);
		O.Solid(x - h, g, z - h, x + h, g + .9, z + h);
	}
	else return false;
	(void)Blk; (void)Today;
	return true;
}

namespace
{
	// the letters' share of a name cell: the slice measures the name on a canvas (Georgia, 46 px, in a 512 px cell); here it is
	// estimated from its length (streets_values.py stubs nameCell() with the same estimate)
	double CellTw(const FString& T) { return FMath::Min(.99, (T.Len() * 24 + 8) / 512.0); }

	// play.js fbox(): a box on a wall face (a along the wall, d out from it)
	struct FFace { bool bNs = true; double Fix = 0, Out = 1; };
	void WallBox(FOut& O, const FFace& S, double a0, double a1, double y0, double y1, double d0, double d1, const FVector3d& c)
	{
		const double p0 = S.Fix + S.Out * d0, p1 = S.Fix + S.Out * d1, lo = FMath::Min(p0, p1), hi = FMath::Max(p0, p1);
		if (S.bNs) O.Box(a0, y0, lo, a1, y1, hi, c); else O.Box(lo, y0, a0, hi, y1, a1, c);
	}
	FVector3d FaceAt(const FFace& S, double a, double y, double d) { return S.bNs ? FVector3d(a, y, S.Fix + S.Out * d) : FVector3d(S.Fix + S.Out * d, y, a); }

	// dressing.js advertOf(): today's advert on a board, k picks the next ones along a long hoarding
	const TSharedPtr<FJsonObject> AdvertOf(const FBrixStreetsToday& T, const FString& Id, int32 k)
	{
		if (!T.Dressing.IsValid()) return nullptr;
		const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
		if (!T.Dressing->TryGetArrayField(TEXT("advert_list"), List) || !List->Num()) return nullptr;
		double Own = 0;
		const TSharedPtr<FJsonObject>* Ads = nullptr;
		if (T.Dressing->TryGetObjectField(TEXT("adverts"), Ads)) (*Ads)->TryGetNumberField(Id, Own);
		return (*List)[((int32)Own + k * 3) % List->Num()]->AsObject();
	}
	void PaintBoard(FOut& O, const FFace& S, double a0, double a1, double y0, double y1, const TSharedPtr<FJsonObject>& Ad, double d0)
	{
		const FVector3d Ground = Col3f(Ad, TEXT("ground"), Cream), Ink = Col3f(Ad, TEXT("ink"), Charcoal);
		WallBox(O, S, a0, a1, y0, y1, d0, d0 + .04, Ground);
		WallBox(O, S, a0 + .12, a1 - .12, y0 + .12, y0 + .2, d0 + .04, d0 + .05, Ink);
		WallBox(O, S, a0 + .12, a1 - .12, y1 - .2, y1 - .12, d0 + .04, d0 + .05, Ink);
		const FString Text = Strf(Ad, TEXT("text"));
		const double fit = FMath::Min((y1 - y0) * .42, (a1 - a0 - .6) / (8 * CellTw(Text) * (64.0 / 46) * .82));
		const FVector3d C = FaceAt(S, (a0 + a1) / 2, (y0 + y1) / 2, d0 + .06);
		O.Text(Text, C.X, C.Y, C.Z, S.bNs ? 0 : S.Out, S.bNs ? S.Out : 0, fit);
	}
	void Post(FOut& O, double x, double g, double z, double h, double w, const FVector3d& c) { O.Box(x - w, g, z - w, x + w, g + h, z + w, c); }
}

// play.js crates(): goods stacked by a door, in the accent colours; each lot of them a thing a car can break ("crates")
void BrixCratesShapes(double x, double z, double g, const FString& Seed, TArray<FBrixShape>& Out)
{
	const int32 First = Out.Num();
	FOut O{ Out };
	const double a = .38 + .1 * Hash({ Seed, TEXT("c1") });
	auto col = [&](int32 k)
	{
		static const FVector3d Cs[4] = { Ochre, Terracotta, FVector3d(.58, .5, .38), Ochre };
		return Cs[FMath::Min(3, (int32)FMath::FloorToDouble(Hash({ Seed, TEXT("cc"), N(k) }) * 4))];
	};
	O.Box(x - a, g, z - a, x + a, g + 2 * a, z + a, col(0)); O.Solid(x - a, g, z - a, x + a, g + 2 * a, z + a);
	if (Hash({ Seed, TEXT("c2") }) < .7) { const double b = a * .72, ox = (Hash({ Seed, TEXT("c3") }) - .5) * .2; O.Box(x + ox - b, g + 2 * a, z - b, x + ox + b, g + 2 * a + 2 * b, z + b, col(1)); }
	if (Hash({ Seed, TEXT("c4") }) < .5) { const double b = .3, ox = a + .42; O.Box(x + ox - b, g, z - b, x + ox + b, g + 2 * b, z + b, col(2)); O.Solid(x + ox - b, g, z - b, x + ox + b, g + 2 * b, z + b); }
	for (int32 i = First; i < Out.Num(); ++i) { Out[i].WrapKind = TEXT("crates"); Out[i].WrapAt = FVector2D(x, z); }
}

// play.js prop() for an arch: a stone arch across an alley, wall to wall, its ring of arch stones, a room bridging the alley
// over it where it meets the street, and now and then crates by it
void BrixArchShapes(const FBrixProp& P, const FBrixBlock& B, double g, TArray<FBrixShape>& Out)
{
	FOut O{ Out };
	const TSharedPtr<FJsonObject>& J = P.Json;
	const double x = P.Pos.X, z = P.Pos.Y;
	const bool ns = Strf(J, TEXT("span")) == TEXT("x");
	const double r = Numf(J, TEXT("width")) / 2, t = Numf(J, TEXT("thick")) / 2, spring = 2.3, top = spring + r + 1.6;
	const int32 Nq = 12;
	auto P3 = [&](double a, double y, double o) { return ns ? FVector3d(x + a, y, z + o) : FVector3d(x + o, y, z + a); };
	auto Y = [&](double a) { return spring + FMath::Sqrt(FMath::Max(0.0, r * r - a * a)); };
	for (int32 i = 0; i < Nq; ++i)
	{
		const double a0 = -r + 2 * r * i / Nq, a1 = -r + 2 * r * (i + 1) / Nq, y0 = Y(a0), y1 = Y(a1), am = (a0 + a1) / 2;
		for (const double o : { -t, t }) O.Quad(P3(a0, y0, o), P3(a1, y1, o), P3(a1, top, o), P3(a0, top, o), Stone, ns ? FVector3d(0, 0, o) : FVector3d(o, 0, 0));
		O.Quad(P3(a0, y0, -t), P3(a1, y1, -t), P3(a1, y1, t), P3(a0, y0, t), Stone * .92, ns ? FVector3d(-am, spring - (y0 + y1) / 2, 0) : FVector3d(0, spring - (y0 + y1) / 2, -am));
	}
	O.Quad(P3(-r, top, -t), P3(r, top, -t), P3(r, top, t), P3(-r, top, t), StoneDark, FVector3d(0, 1, 0));
	const double e = .42 / r;
	auto V3 = [&](double a, double y, double o) { return P3(a * (1 + e), spring + (y - spring) * (1 + e), o); };
	for (int32 i = 0; i < Nq; ++i)   // the ring of arch stones, standing a little proud of the wall
	{
		const double a0 = -r + 2 * r * i / Nq, a1 = -r + 2 * r * (i + 1) / Nq, y0 = Y(a0), y1 = Y(a1);
		const FVector3d col = Stone * (.78 + .3 * Hash({ N(x), N(z), N(i) }));
		for (const double o : { -t - .08, t + .08 }) O.Quad(P3(a0, y0, o), P3(a1, y1, o), V3(a1, y1, o), V3(a0, y0, o), col, ns ? FVector3d(0, 0, o) : FVector3d(o, 0, 0));
	}
	for (const double sd : { -1.0, 1.0 })
	{
		const FVector3d a = P3(sd * r, 0, -t - .16), b2 = P3(sd * (r - .5), spring, t + .16);
		O.Box(FMath::Min(a.X, b2.X), 0, FMath::Min(a.Z, b2.Z), FMath::Max(a.X, b2.X), spring + .12, FMath::Max(a.Z, b2.Z), Stone);
	}
	const FVector3d q0 = P3(-r, 0, -t), q1 = P3(r, 0, t);
	const double bx[4] = { FMath::Min(q0.X, q1.X), FMath::Min(q0.Z, q1.Z), FMath::Max(q0.X, q1.X), FMath::Max(q0.Z, q1.Z) };
	O.Solid(bx[0], spring + r * .7, bx[1], bx[2], top, bx[3]);
	const double loc = ns ? z - B.Origin.Y : x - B.Origin.X, Size = B.Extent.X;   // (a block's size: its extent over one grid cell)
	if (loc < 2 || loc > Size - 2)   // this arch is on the street: roof the first 7 m behind it, a room bridging the alley over the gateway
	{
		const double dir = loc < 2 ? 1 : -1;
		const FVector3d c0 = P3(-r, 0, dir * t), c1 = P3(r, 0, dir * (t + 7));
		const double cb[4] = { FMath::Min(c0.X, c1.X), FMath::Min(c0.Z, c1.Z), FMath::Max(c0.X, c1.X), FMath::Max(c0.Z, c1.Z) };
		const double ub[4] = { FMath::Min(bx[0], cb[0]), FMath::Min(bx[1], cb[1]), FMath::Max(bx[2], cb[2]), FMath::Max(bx[3], cb[3]) };
		O.Box(cb[0], top - .5, cb[1], cb[2], top, cb[3], StoneDark); O.Solid(cb[0], top - .5, cb[1], cb[2], top, cb[3]);
		O.Box(ub[0], top, ub[1], ub[2], top + 2.5, ub[3], FVector3d(.70, .58, .46));
		O.Box(ub[0] - .15, top + 2.5, ub[1] - .15, ub[2] + .15, top + 2.75, ub[3] + .15, StoneDark);
		for (int32 kb = 0; kb < 5; ++kb)
		{
			const double o = dir * (t + .9 + kb * 1.45);
			const FVector3d e0 = P3(-r, 0, o - .11), e1 = P3(r, 0, o + .11);
			O.Box(FMath::Min(e0.X, e1.X), top - .78, FMath::Min(e0.Z, e1.Z), FMath::Max(e0.X, e1.X), top - .5, FMath::Max(e0.Z, e1.Z), Brown);
		}
	}
	const double hk = Hash({ N(x), N(z), TEXT("al") });
	if (hk > .35) { const FVector3d c = P3(r - .55, 0, (hk > .7 ? 1 : -1) * (t + 1.3)); BrixCratesShapes(c.X, c.Z, g, TEXT("ar") + N(x) + N(z), Out); }
}

// the rest of dressing.js prop(): the bandstand, notice boards, sundials, wells, pergolas, statues, the news kiosk, bunting on
// market days, planters, bins, bike racks, post and telephone boxes, bollards, the tram stops' timetables, adverts painted on
// blank walls and hoardings pasted with posters
bool BrixMoreDressShapes(const FBrixProp& P, const FBrixBlock& Blk, double g, const FBrixStreetsToday& Today, const FBrixBoardWall* Wall, TArray<FBrixShape>& Out)
{
	FOut O{ Out };
	const TSharedPtr<FJsonObject>& J = P.Json;
	const double x = P.Pos.X, z = P.Pos.Y;
	const FString& k = P.Kind;
	const double Q4 = UE_DOUBLE_PI / 4, Q8 = UE_DOUBLE_PI / 8;
	auto Pt = [&](double q, double rr, double y) { return FVector3d(x + FMath::Cos(q * Q4) * rr, y, z + FMath::Sin(q * Q4) * rr); };
	if (k == TEXT("bandstand"))   // an eight-sided stage with iron posts and a pointed roof
	{
		const double R = Numf(J, TEXT("r"));
		for (int32 q = 0; q < 8; ++q)
		{
			const FVector3d o(FMath::Cos((q + .5) * Q4), 0, FMath::Sin((q + .5) * Q4));
			O.Quad(Pt(q, R, g), Pt(q + 1, R, g), Pt(q + 1, R, g + .7), Pt(q, R, g + .7), Stone, o);
			O.Quad(Pt(q, R, g + .7), Pt(q + 1, R, g + .7), FVector3d(x, g + .7, z), FVector3d(x, g + .7, z), DWood, FVector3d(0, 1, 0));
			const FVector3d c = Pt(q, R - .25, g + .7);
			O.Box(c.X - .07, g + .7, c.Z - .07, c.X + .07, g + 3.4, c.Z + .07, Cream);
			const FVector3d c2 = Pt(q + 1, R - .25, g + 1.6);
			if (q != 2) O.Box(FMath::Min(c.X, c2.X) - .03, g + 1.55, FMath::Min(c.Z, c2.Z) - .03, FMath::Max(c.X, c2.X) + .03, g + 1.62, FMath::Max(c.Z, c2.Z) + .03, Iron);
			O.Quad(Pt(q, R + .4, g + 3.4), Pt(q + 1, R + .4, g + 3.4), FVector3d(x, g + 5.0, z), FVector3d(x, g + 5.0, z), Teal, FVector3d(o.X, .8, o.Z));
			O.Quad(Pt(q + 1, R + .4, g + 3.4), Pt(q, R + .4, g + 3.4), FVector3d(x, g + 3.4, z), FVector3d(x, g + 3.4, z), Cream, FVector3d(0, -1, 0));
		}
		O.Box(x - .1, g + 5.0, z - .1, x + .1, g + 5.5, z + .1, Ochre);
		for (int32 q = 0; q < 3; ++q) { const FVector3d c = Pt(2.3 + q * .2, R + .25 + q * .45, 0); O.Box(c.X - .7, g + .47 * (2 - q) * .5, c.Z - .7, c.X + .7, g + .7 - q * .23, c.Z + .7, Stone); }
		O.Solid(x - R * .9, g, z - R * .9, x + R * .9, g + .7, z + R * .9);
	}
	else if (k == TEXT("noticeboard"))   // two posts, a little roof, papers pinned up
	{
		const bool ax = Strf(J, TEXT("face")) == TEXT("x");
		const double sg = Numf(J, TEXT("sign")) != 0 ? Numf(J, TEXT("sign")) : 1, f[2] = { ax ? sg : 0, ax ? 0 : sg }, r[2] = { ax ? 0.0 : 1.0, ax ? 1.0 : 0.0 };
		for (const double u : { -.75, .75 }) Post(O, x + r[0] * u, g, z + r[1] * u, 2.1, .05, DWood);
		O.Box(x - r[0] * .8 - f[0] * .02 - .02 * r[1], g + .9, z - r[1] * .8 - f[1] * .02 - .02 * r[0], x + r[0] * .8 + f[0] * .04 + .02 * r[1], g + 1.95, z + r[1] * .8 + f[1] * .04 + .02 * r[0], DGreen);
		O.Box(x - r[0] * .95 - .18 * FMath::Abs(f[0]), g + 2.0, z - r[1] * .95 - .18 * FMath::Abs(f[1]), x + r[0] * .95 + .18 * FMath::Abs(f[0]), g + 2.1, z + r[1] * .95 + .18 * FMath::Abs(f[1]), Terracotta);
		static const FVector3d Paper[4] = { Cream, FVector3d(.86, .82, .7), FVector3d(.8, .7, .5), FVector3d(.7, .78, .8) };
		for (int32 q = 0; q < 6; ++q)
		{
			const double u = -.6 + (q % 3) * .55 + .08 * Hash({ P.Id, N(q) }), v = q < 3 ? 1.55 : 1.1, w = .18 + .08 * Hash({ N(q), P.Id });
			const FVector3d& c = Paper[FMath::Min(3, (int32)FMath::FloorToDouble(Hash({ P.Id, N(q), TEXT("2") }) * 4))];
			const double cx = x + r[0] * u + f[0] * .05, cz = z + r[1] * u + f[1] * .05;
			O.Box(cx - w * r[0] - .005, g + v, cz - w * r[1] - .005, cx + w * r[0] + .005, g + v + .3 + .1 * Hash({ N(q) }), cz + w * r[1] + .005, c);
		}
		O.Solid(x - .8 * r[0] - .06, g, z - .8 * r[1] - .06, x + .8 * r[0] + .06, g + 2.1, z + .8 * r[1] + .06);
	}
	else if (k == TEXT("sundial"))
	{
		const FVector3d Brass(.55, .45, .2);
		O.Box(x - .5, g, z - .5, x + .5, g + .2, z + .5, Stone); O.YBox(x, g + .2, z, .4, .8, .4, Q4, Stone);
		O.Box(x - .38, g + 1.0, z - .38, x + .38, g + 1.05, z + .38, Brass); O.Box(x - .02, g + 1.05, z - .25, x + .02, g + 1.3, z + .1, Brass);
		O.Solid(x - .3, g, z - .3, x + .3, g + 1.05, z + .3);
	}
	else if (k == TEXT("well"))   // a round stone well with a little roof, a rope and a bucket
	{
		const double R = .85;
		for (int32 q = 0; q < 8; ++q)
		{
			const FVector3d o(FMath::Cos((q + .5) * Q4), 0, FMath::Sin((q + .5) * Q4));
			O.Quad(Pt(q, R, g), Pt(q + 1, R, g), Pt(q + 1, R, g + .75), Pt(q, R, g + .75), Stone, o);
			O.Quad(Pt(q, R, g + .75), Pt(q + 1, R, g + .75), Pt(q + 1, R - .2, g + .75), Pt(q, R - .2, g + .75), Stone, FVector3d(0, 1, 0));
			O.Quad(Pt(q, R - .2, g + .75), Pt(q + 1, R - .2, g + .75), FVector3d(x, g + .3, z), FVector3d(x, g + .3, z), FVector3d(.05, .06, .07), FVector3d(0, 1, 0));
		}
		for (const double s : { -1.0, 1.0 }) Post(O, x + s * .8, g + .7, z, 1.5, .06, DWood);
		O.Box(x - .85, g + 1.75, z - .03, x + .85, g + 1.81, z + .03, DWood); O.Box(x - .02, g + .95, z - .02, x + .02, g + 1.75, z + .02, Cream); O.Box(x - .14, g + .8, z - .14, x + .14, g + 1.0, z + .14, DWood);
		for (const double s : { -1.0, 1.0 }) O.Quad(FVector3d(x - 1.1, g + 2.2, z), FVector3d(x + 1.1, g + 2.2, z), FVector3d(x + 1.1, g + 1.9, z + s * .75), FVector3d(x - 1.1, g + 1.9, z + s * .75), Terracotta, FVector3d(0, .8, s));
		O.Solid(x - R, g, z - R, x + R, g + .75, z + R);
	}
	else if (k == TEXT("pergola"))   // four posts, beams across, a vine over the top
	{
		for (const double sx : { -1.4, 1.4 }) for (const double sz : { -1.4, 1.4 }) { Post(O, x + sx, g, z + sz, 2.45, .09, Cream); O.Solid(x + sx - .1, g, z + sz - .1, x + sx + .1, g + 2.4, z + sz + .1); }
		for (const double s : { -1.4, 1.4 }) O.Box(x - 1.7, g + 2.4, z + s - .07, x + 1.7, g + 2.56, z + s + .07, Cream);
		for (int32 q = 0; q < 6; ++q) O.Box(x - 1.6 + q * .64 - .04, g + 2.56, z - 1.7, x - 1.6 + q * .64 + .04, g + 2.66, z + 1.7, Cream);
		for (int32 q = 0; q < 9; ++q) O.YBox(x - 1.3 + (q % 3) * 1.3 + .3 * Hash({ P.Id, N(q) }), g + 2.6, z - 1.3 + (q / 3) * 1.3, .9, .35 + .2 * Hash({ N(q) }), .8, Hash({ N(q), P.Id }) * 3, Olive * (.9 + .3 * Hash({ P.Id, N(q), TEXT("1") })));
		for (const double sx : { -1.4, 1.4 }) O.YBox(x + sx, g, z - 1.4, .5, 2.4, .3, 0, Olive * .85);
		O.Box(x - .9, g + .36, z - .27, x + .9, g + .47, z + .27, DWood); for (const double s : { -.6, .6 }) O.Box(x + s - .15, g, z - .22, x + s + .15, g + .36, z + .22, Stone);
	}
	else if (k == TEXT("statue"))   // a figure in green bronze on a stone pedestal, steps round it
	{
		bool bSmall = false;
		if (J.IsValid()) J->TryGetBoolField(TEXT("small"), bSmall);
		const double sm = bSmall ? .55 : 1, base = bSmall ? 0 : .5;
		if (!bSmall) for (int32 q = 0; q < 2; ++q) O.Box(x - 2.2 + q * .45, g + q * .25, z - 2.2 + q * .45, x + 2.2 - q * .45, g + (q + 1) * .25, z + 2.2 - q * .45, Stone);
		const double ph = base + 2.0 * sm;
		O.Box(x - .75 * sm, g + base, z - .75 * sm, x + .75 * sm, g + ph, z + .75 * sm, Stone); O.Box(x - .9 * sm, g + ph, z - .9 * sm, x + .9 * sm, g + ph + .15 * sm, z + .9 * sm, StoneDark);
		const double y = g + ph + .15 * sm, S = 1.25 * sm;
		for (const double s : { -1.0, 1.0 }) O.Box(x + s * .14 * S - .1 * S, y, z - .1 * S, x + s * .14 * S + .1 * S, y + .85 * S, z + .1 * S, Bronze);
		O.Box(x - .3 * S, y + .7 * S, z - .2 * S, x + .3 * S, y + 1.5 * S, z + .2 * S, Bronze); O.Box(x - .36 * S, y + .55 * S, z - .24 * S, x + .36 * S, y + .95 * S, z + .24 * S, Bronze * .9);
		O.YBox(x, y + 1.52 * S, z, .26 * S, .3 * S, .26 * S, Q4, Bronze);
		O.Box(x - .44 * S, y + .85 * S, z - .08 * S, x - .32 * S, y + 1.45 * S, z + .08 * S, Bronze);
		BrixOBox(Out, FBrixM4::T(x + .38 * S, y + 1.42 * S, z) * FBrixM4::Rz(-2.5), 0, .32 * S, 0, .12 * S, .65 * S, .12 * S, Bronze);
		O.Box(x + .55 * S, y + 1.95 * S, z - .06 * S, x + .75 * S, y + 2.0 * S, z + .06 * S, FVector3d(.55, .45, .2));
		const double hs = bSmall ? .5 : 2.2;
		O.Solid(x - hs, g, z - hs, x + hs, g + ph, z + hs);
	}
	else if (k == TEXT("kiosk"))   // a green news kiosk: papers on the counter, its name on the roof
	{
		const double f = Numf(J, TEXT("face")) != 0 ? Numf(J, TEXT("face")) : -1;
		O.Box(x - 1.1, g, z - .95, x + 1.1, g + .95, z + .95, DGreen); O.Box(x - 1.1, g + 2.15, z - .95, x + 1.1, g + 2.4, z + .95, DGreen);
		for (const double sz : { -1.0, 1.0 }) O.Box(x - 1.1, g + .95, z + sz * .95 - .08, x + 1.1, g + 2.15, z + sz * .95 + .08, DGreen);
		O.Box(x - f * 1.1 - .08, g + .95, z - .95, x - f * 1.1 + .08, g + 2.15, z + .95, DGreen);
		O.Box(x - 1.45, g + 2.4, z - 1.3, x + 1.45, g + 2.52, z + 1.3, Terracotta);
		O.Box(x + f * 1.1 - .02, g + .95, z - .9, x + f * 1.1 + f * .3, g + 1.0, z + .9, DWood);
		static const FVector3d Papers[3] = { Cream, FVector3d(.86, .82, .7), FVector3d(.6, .2, .16) }, Mags[4] = { Cream, Ochre, FVector3d(.6, .2, .16), Blue };
		for (int32 q = 0; q < 6; ++q)
			O.Box(x + f * 1.12 + f * (q % 2) * .14 - .06, g + 1.0, z - .75 + (q / 2) * .55, x + f * 1.12 + f * (q % 2) * .14 + .06, g + 1.03 + .02 * q, z - .5 + (q / 2) * .55,
				Papers[FMath::Min(2, (int32)FMath::FloorToDouble(Hash({ P.Id, N(q) }) * 3))]);
		for (int32 q = 0; q < 5; ++q)
			O.Box(x + f * 1.11, g + 1.3 + (q % 2) * .32, z - .8 + q * .36, x + f * 1.13, g + 1.56 + (q % 2) * .32, z - .54 + q * .36, Mags[FMath::Min(3, (int32)FMath::FloorToDouble(Hash({ P.Id, N(q), TEXT("9") }) * 4))]);
		O.Text(TEXT("News"), x + f * 1.46, g + 2.46, z, f, 0, .1);
		WallBox(O, FFace{ false, x + f * 1.1, f }, z - .7, z + .7, 2.17, 2.37, 0, .03, Cream);   // (the slice lays this strip at 2.17 m from the street)
		O.Text(TEXT("Papers"), x + f * 1.14, g + 2.27, z, f, 0, .12);
		O.Solid(x - 1.1, g, z - .95, x + 1.1, g + 2.4, z + .95);
	}
	else if (k == TEXT("bunting"))   // flags on a line strung from a pole to the fountain: up on market days only
	{
		if (!Today.bBunting) return true;
		const FVector2D To = [&]() { const TArray<TSharedPtr<FJsonValue>>* A = nullptr; return J->TryGetArrayField(TEXT("to"), A) && A->Num() >= 2 ? FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) : P.Pos; }();
		const double hh0 = Numf(J, TEXT("h0")), h0 = g + hh0, h1 = g + Numf(J, TEXT("h1")), L = FVector2D::Distance(To, P.Pos);
		const int32 n = (int32)FMath::FloorToDouble(L / .55);
		const double ux = (To.X - x) / L, uz = (To.Y - z) / L;
		static const FVector3d Cols[5] = { Ochre, Terracotta, Teal, Cream, Blue };
		bool bPoles = false;
		if (J.IsValid()) J->TryGetBoolField(TEXT("poles"), bPoles);
		if (bPoles) { Post(O, x, g, z, hh0 + .2, .05, Cream); O.Solid(x - .05, g, z - .05, x + .05, g + hh0, z + .05); }
		auto yAt = [&](double t) { return h0 + (h1 - h0) * t - .7 * 4 * t * (1 - t); };
		for (int32 q = 0; q < n; ++q)
		{
			const double t0 = (double)q / n, t1 = (double)(q + 1) / n;
			const FVector3d a(x + ux * L * t0, yAt(t0), z + uz * L * t0), c(x + ux * L * t1, yAt(t1), z + uz * L * t1);
			O.Box(FMath::Min(a.X, c.X) - .006, FMath::Min(a.Y, c.Y) - .006, FMath::Min(a.Z, c.Z) - .006, FMath::Max(a.X, c.X) + .006, FMath::Max(a.Y, c.Y) + .006, FMath::Max(a.Z, c.Z) + .006, Iron);
			const FVector3d m((a.X + c.X) / 2, (a.Y + c.Y) / 2 - .32, (a.Z + c.Z) / 2), nx(-uz, 0, ux);
			O.Quad(a, c, m, m, Cols[q % 5], nx); O.Quad(c, a, m, m, Cols[q % 5], FVector3d(-nx.X, 0, -nx.Z));
		}
	}
	else if (k == TEXT("planter"))
	{
		bool bBig = P.bBig;
		const double h = bBig ? .75 : .48, s = bBig ? .7 : .45;
		const FVector3d col = bBig ? Stone : DWood;
		O.Box(x - s, g, z - s, x + s, g + h, z + s, col); O.Flat(x - s + .08, z - s + .08, x + s - .08, z + s - .08, g + h + .005, Soil);
		for (int32 q = 0; q < 4; ++q) O.YBox(x + (q % 2 - .5) * s * .9, g + h, z + (q / 2 - .5) * s * .9, s * .8, .3 + .35 * Hash({ P.Id, N(q) }), s * .8, Hash({ N(q), P.Id }) * 3, Olive * (.9 + .3 * Hash({ P.Id, N(q) })));
		if (Today.FlowerSize > .2 && Today.FlowerColors.Num())
			for (int32 q = 0; q < 5; ++q)
			{
				const double fx = x + (Hash({ P.Id, N(q), TEXT("5") }) - .5) * s * 1.4, fz = z + (Hash({ P.Id, N(q), TEXT("6") }) - .5) * s * 1.4;
				O.Box(fx - .06, g + h + .45, fz - .06, fx + .06, g + h + .58, fz + .06, Today.FlowerColors[(q + (int32)FMath::FloorToDouble(Hash({ P.Id }) * 9)) % Today.FlowerColors.Num()]);
			}
		O.Solid(x - s, g, z - s, x + s, g + h, z + s);
	}
	else if (k == TEXT("bin"))
	{
		O.YBox(x, g, z, .42, .82, .42, Q8, DGreen); O.YBox(x, g + .82, z, .5, .08, .5, Q8, DGreen * .8); O.Box(x - .12, g + .9, z - .12, x + .12, g + .95, z + .12, Iron);
		O.Solid(x - .25, g, z - .25, x + .25, g + .9, z + .25);
	}
	else if (k == TEXT("bikerack"))   // three hoops, now and then a bicycle leant on one
	{
		const bool ax = P.Along == TEXT("x");
		static const FVector3d Bikes[4] = { Blue, Red, Teal, Ochre };
		for (int32 q = -1; q <= 1; ++q)
		{
			const double cx = x + (ax ? q * .8 : 0), cz = z + (ax ? 0 : q * .8);
			for (const double s : { -.32, .32 }) O.Box(cx + (ax ? 0 : s) - .03, g, cz + (ax ? s : 0) - .03, cx + (ax ? 0 : s) + .03, g + .8, cz + (ax ? s : 0) + .03, Iron);
			O.Box(cx - (ax ? .03 : .35), g + .8, cz - (ax ? .35 : .03), cx + (ax ? .03 : .35), g + .86, cz + (ax ? .35 : .03), Iron);
			if (Hash({ P.Id, N(q) }) < .45)
			{
				const FVector3d& col = Bikes[FMath::Min(3, (int32)FMath::FloorToDouble(Hash({ P.Id, N(q), TEXT("2") }) * 4))];
				const double yaw = ax ? UE_DOUBLE_PI / 2 : 0;
				for (const double s : { -.55, .55 }) O.YBox(cx + (ax ? .12 : s), g, cz + (ax ? s : .12), ax ? .05 : .66, .66, ax ? .66 : .05, 0, FVector3d(.08, .08, .09));
				O.YBox(cx + (ax ? .12 : 0), g + .5, cz + (ax ? 0 : .12), ax ? .04 : 1.0, .06, ax ? 1.0 : .04, 0, col);
				O.YBox(cx + (ax ? .12 : -.3), g + .8, cz + (ax ? -.3 : .12), .1, .05, .22, yaw, FVector3d(.1, .08, .07));
			}
		}
		O.Solid(x - (ax ? 1.2 : .1), g, z - (ax ? .1 : 1.2), x + (ax ? 1.2 : .1), g + .86, z + (ax ? .1 : 1.2));
	}
	else if (k == TEXT("postbox"))   // a red pillar box
	{
		O.YBox(x, g, z, .46, 1.15, .46, Q8, Red); O.YBox(x, g + 1.15, z, .52, .12, .52, Q8, Red * .85); O.YBox(x, g + 1.27, z, .3, .08, .3, Q8, Red * .85);
		const bool bOut = J.IsValid() && J->HasField(TEXT("out"));
		const FVector2D o = bOut ? P.Out : P.Along == TEXT("z") ? FVector2D(1, 0) : FVector2D(0, 1);
		O.Box(x - o.X * .25 - .12 * FMath::Abs(o.Y) - .01, g + .92, z - o.Y * .25 - .12 * FMath::Abs(o.X) - .01, x - o.X * .23 + .12 * FMath::Abs(o.Y) + .01, g + .96, z - o.Y * .23 + .12 * FMath::Abs(o.X) + .01, FVector3d(.05, .05, .05));
		O.Solid(x - .25, g, z - .25, x + .25, g + 1.3, z + .25);
	}
	else if (k == TEXT("phonebox"))   // a red telephone box with glazed sides and a crown
	{
		O.Box(x - .5, g, z - .5, x + .5, g + .15, z + .5, Red);
		for (const double sx : { -.45, .45 }) for (const double sz : { -.45, .45 }) O.Box(x + sx - .06, g, z + sz - .06, x + sx + .06, g + 2.35, z + sz + .06, Red);
		const int32 Sides[4][2] = { { 0, -1 }, { 0, 1 }, { 1, -1 }, { 1, 1 } };
		for (const auto& Sd : Sides)
		{
			const bool ax = Sd[0] != 0;
			const double s = Sd[1], fx = ax ? x + s * .44 : x, fz = ax ? z : z + s * .44;
			O.Box(fx - (ax ? .015 : .38), g + .85, fz - (ax ? .38 : .015), fx + (ax ? .015 : .38), g + 2.05, fz + (ax ? .38 : .015), FVector3d(.36, .44, .48));
			for (const double y : { 1.25, 1.65 }) O.Box(fx - (ax ? .025 : .38), g + y, fz - (ax ? .38 : .025), fx + (ax ? .025 : .38), g + y + .04, fz + (ax ? .38 : .025), Red);
		}
		O.Box(x - .55, g + 2.35, z - .55, x + .55, g + 2.6, z + .55, Red); O.YBox(x, g + 2.6, z, .9, .14, .9, 0, Red * .9); O.Box(x - .12, g + 2.74, z - .12, x + .12, g + 2.84, z + .12, Red * .9);
		O.Box(x - .2, g + 1.4, z - .2, x + .2, g + 1.6, z + .2, FVector3d(1, .85, .6));
		O.Solid(x - .5, g, z - .5, x + .5, g + 2.6, z + .5);
	}
	else if (k == TEXT("bollard"))
	{
		O.YBox(x, g, z, .2, .78, .2, Q8, Iron); O.YBox(x, g + .78, z, .24, .08, .24, Q8, Iron);
		O.Solid(x - .1, g, z - .1, x + .1, g + .8, z + .1);
	}
	else if (k == TEXT("timetable"))   // a times board on a post at the tram stop
	{
		Post(O, x, g, z, 2.1, .05, Teal); O.Box(x - .38, g + 1.2, z - .04, x + .38, g + 2.0, z + .04, Teal);
		for (int32 q = 0; q < 6; ++q) O.Box(x - .3, g + 1.3 + q * .11, z + .04, x + .3 - .2 * Hash({ P.Id, N(q) }), g + 1.36 + q * .11, z + .05, Cream);
		O.Text(TEXT("Tram"), x, g + 1.9, z + .05, 0, 1, .1);
		O.Solid(x - .06, g, z - .06, x + .06, g + 2.1, z + .06);
	}
	else if (k == TEXT("advert") || k == TEXT("hoarding"))   // today's advert painted on a blank wall, or a timber hoarding pasted with posters
	{
		FBrixBoardSpan Bs;
		if (!Wall || !BrixBoardSpanOf(P, *Wall, Bs)) return true;
		const TCHAR s = Bs.Side;
		const bool ns = s == 'n' || s == 's';
		const double a0 = Bs.A0, a1 = Bs.A1, y0 = Bs.Y0, y1 = Bs.Y1;
		const FFace S{ ns, ns ? (s == 'n' ? Wall->Min.Y : Wall->Max.Y) : (s == 'w' ? Wall->Min.X : Wall->Max.X), s == 'n' || s == 'w' ? -1.0 : 1.0 };
		if (k == TEXT("advert")) { if (const TSharedPtr<FJsonObject> Ad = AdvertOf(Today, P.Id, 0)) PaintBoard(O, S, a0, a1, y0, y1, Ad, .02); }
		else
		{
			WallBox(O, S, a0, a1, y0, y1, 0, .1, FVector3d(.42, .3, .18));
			for (double a = a0; a < a1; a += .2) WallBox(O, S, a, a + .02, y0, y1, .1, .11, FVector3d(.3, .2, .12));
			const int32 n = FMath::Max(1, (int32)FMath::FloorToDouble((a1 - a0) / 3.6));
			const double w = (a1 - a0) / n;
			for (int32 q = 0; q < n; ++q) if (const TSharedPtr<FJsonObject> Ad = AdvertOf(Today, P.Id, q)) PaintBoard(O, S, a0 + q * w + .25, a0 + (q + 1) * w - .25, y0 + .45, y1 - .3, Ad, .11);
			WallBox(O, S, a0 - .1, a1 + .1, y1, y1 + .12, 0, .2, FVector3d(.3, .2, .12));
		}
	}
	else return false;
	(void)Blk;
	return true;
}

TOptional<FBrixBoardWall> BrixBoardWallOf(const FBrixBlock& B, const FBrixProp& P, const TArray<FString>& OldDistricts)
{
	const FString Lot = Strf(P.Json, TEXT("lot"));
	for (const FBrixLot& L : B.Lots)
		if (L.Id == Lot)
		{
			FBrixBoardWall W;
			W.Min = L.Min; W.Max = L.Max;
			W.bOld = OldDistricts.Contains(B.District);
			W.H = FMath::Max(L.Floors * 3 + .4, 3.2 + .4);
			return W;
		}
	return {};
}

bool BrixBoardSpanOf(const FBrixProp& P, const FBrixBoardWall& Wall, FBrixBoardSpan& Out)
{
	if (P.Kind != TEXT("advert") && P.Kind != TEXT("hoarding")) return false;
	const FVector2D o = P.Out;
	Out.Side = o.Y < 0 ? 'n' : o.Y > 0 ? 's' : o.X < 0 ? 'w' : 'e';
	const bool ns = Out.Side == 'n' || Out.Side == 's';
	const double c = ns ? P.Pos.X : P.Pos.Y, hw = Numf(P.Json, TEXT("w")) / 2;
	Out.A0 = c - hw; Out.A1 = c + hw;
	if (P.Kind == TEXT("hoarding")) { Out.Y0 = .12 + .1; Out.Y1 = 2.9; }   // (play.js Y_PAVE + 0.1)
	else { Out.Y0 = Wall.bOld ? 3.55 + .55 : 3.85; Out.Y1 = FMath::Min(Out.Y0 + 2.6, (Wall.H != 0 ? Wall.H : 7) - .5); }   // (OLD_BASE 3.55)
	return true;
}
