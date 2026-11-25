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
