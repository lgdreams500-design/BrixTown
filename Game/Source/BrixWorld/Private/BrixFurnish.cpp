#include "BrixFurnish.h"

#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixInside.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace
{
	// play.js C (the slice's palette) and ROOM_WOOD
	const FVector3f Cream(.66f, .61f, .50f), Ochre(.52f, .40f, .20f), Terracotta(.44f, .24f, .18f), Teal(.20f, .36f, .37f), Blue(.30f, .38f, .52f),
		Olive(.30f, .33f, .20f), Plum(.30f, .24f, .34f), Charcoal(.09f, .09f, .11f), Metal(.17f, .18f, .21f), Wood(.36f, .24f, .15f);

	// play.js ROOM_STYLE: what each kind of room's chairs, counters and lamps are, what stands on its counters
	enum class ETool : uint8 { None, Espresso, Till, Vise, Ledger };
	// Tall, Low: pieces for the walls (tall ones keep off windows and the door wall); Mid: things for open floor; Pictures: what
	// hangs on the walls; bDim: no lamps hang from the beams (a cinema)
	struct FStyle
	{
		TArray<FVector3f> Fabric; TOptional<FVector3f> Chair, Counter, Top; ETool Tool = ETool::None; TOptional<FVector3f> Lamp;
		TArray<FString> Tall, Low, Mid; FString Pictures; bool bDim = false;
	};
	const FStyle& StyleOf(const FString& Sp, const FString& Kind)
	{
		static const TMap<FString, FStyle> Styles = {
			{ TEXT("home"), { { Olive, Plum, FVector3f(.55f, .35f, .24f), Blue }, {}, {}, {}, ETool::None, Cream, { TEXT("wardrobe"), TEXT("bookcase"), TEXT("dresser"), TEXT("fireplace") }, { TEXT("sideboard"), TEXT("plant"), TEXT("dresser") }, { TEXT("dining"), TEXT("rug") }, TEXT("art") } },
			{ TEXT("food"), { { Cream }, FVector3f(.42f, .3f, .2f), FVector3f(.3f, .2f, .13f), FVector3f(.62f, .58f, .52f), ETool::Espresso, Ochre, {}, { TEXT("plant") }, {}, TEXT("menu") } },
			{ TEXT("shop"), { { Cream }, {}, FVector3f(.32f, .22f, .14f), Teal, ETool::Till, Cream, {}, { TEXT("plant") }, {}, TEXT("price") } },
			{ TEXT("work"), { { Blue, Olive }, {}, {}, {}, ETool::Ledger, Teal, { TEXT("bookcase"), TEXT("filing") }, { TEXT("plant"), TEXT("sideboard") }, { TEXT("rug") }, TEXT("art") } },
			{ TEXT("office"), { { Blue, Charcoal }, {}, {}, {}, ETool::Ledger, Teal, { TEXT("bookcase"), TEXT("filing") }, { TEXT("plant"), TEXT("sofa") }, { TEXT("reception"), TEXT("rug") }, TEXT("art") } },
			{ TEXT("guild"), { { Terracotta }, {}, {}, {}, ETool::Ledger, Ochre, { TEXT("bookcase"), TEXT("fireplace") }, { TEXT("sideboard"), TEXT("plant") }, { TEXT("long"), TEXT("rug") }, TEXT("art") } },
			{ TEXT("hall"), { { Terracotta }, {}, {}, {}, ETool::Ledger, Cream, { TEXT("bookcase"), TEXT("filing") }, { TEXT("bench"), TEXT("plant") }, { TEXT("reception"), TEXT("benches") }, TEXT("art") } },
			{ TEXT("works"), { { Charcoal }, {}, FVector3f(.36f, .26f, .18f), FVector3f(.42f, .3f, .2f), ETool::Vise, Metal, { TEXT("lockers") }, { TEXT("workbench"), TEXT("barrels"), TEXT("crates") }, { TEXT("crates") }, TEXT("tools") } },
			{ TEXT("health"), { { Teal }, FVector3f(.6f, .62f, .62f), FVector3f(.8f, .8f, .76f), Teal, ETool::Ledger, Cream, { TEXT("medcab") }, { TEXT("sink"), TEXT("bench"), TEXT("plant") }, { TEXT("reception"), TEXT("screens") }, TEXT("notice") } },
			{ TEXT("hospital"), { { Teal }, FVector3f(.6f, .62f, .62f), FVector3f(.8f, .8f, .76f), Teal, ETool::Ledger, Cream, { TEXT("medcab") }, { TEXT("sink"), TEXT("bench"), TEXT("plant") }, { TEXT("reception"), TEXT("screens"), TEXT("beds") }, TEXT("notice") } },
			{ TEXT("utility"), { { Charcoal }, {}, {}, {}, ETool::Ledger, Metal, { TEXT("lockers") }, { TEXT("workbench"), TEXT("crates"), TEXT("barrels") }, { TEXT("crates") }, TEXT("tools") } },
			{ TEXT("mall"), { { Cream }, {}, FVector3f(.32f, .22f, .14f), Ochre, ETool::Till, Ochre, {}, { TEXT("plant"), TEXT("bench") }, { TEXT("kiosk"), TEXT("plant") }, TEXT("price") } },
			{ TEXT("flats"), { { Olive }, {}, {}, {}, ETool::Ledger, Cream, {}, { TEXT("sofa"), TEXT("plant") }, { TEXT("reception"), TEXT("rug") }, TEXT("art") } },
			{ TEXT("bank"), { { Teal }, FVector3f(.42f, .3f, .2f), FVector3f(.3f, .2f, .13f), FVector3f(.72f, .7f, .64f), ETool::Ledger, Cream, { TEXT("filing") }, { TEXT("plant"), TEXT("bench") }, {}, TEXT("art") } },
			{ TEXT("library"), { { Olive }, FVector3f(.36f, .24f, .15f), FVector3f(.3f, .2f, .13f), FVector3f(.24f, .34f, .26f), ETool::Ledger, Olive, { TEXT("bookcase"), TEXT("bookcase"), TEXT("filing") }, { TEXT("plant"), TEXT("bench") }, {}, TEXT("art") } },
			{ TEXT("museum"), { { Terracotta }, {}, FVector3f(.3f, .2f, .13f), FVector3f(.62f, .58f, .52f), ETool::Ledger, Cream, {}, { TEXT("bench"), TEXT("plant") }, {}, TEXT("art") } },
			{ TEXT("cinema"), { { FVector3f(.42f, .1f, .1f) }, {}, FVector3f(.42f, .1f, .1f), FVector3f(.62f, .5f, .3f), ETool::Till, FVector3f(.42f, .1f, .1f), {}, {}, {}, TEXT("art"), true } },
			{ TEXT("dealer"), { { Charcoal }, {}, FVector3f(.32f, .34f, .36f), FVector3f(.8f, .8f, .78f), ETool::Ledger, Cream, {}, { TEXT("plant"), TEXT("sofa") }, {}, TEXT("price") } },
		};
		if (const FStyle* S = Styles.Find(Sp)) return *S;
		if (const FStyle* S = Styles.Find(Kind)) return *S;
		return Styles[TEXT("work")];
	}
	template <typename T> const T& Pick(const TArray<T>& List, double R) { return List[FMath::Min(List.Num() - 1, (int32)(R * List.Num()))]; }
	// As JS writes a number in a hash: the fewest digits that read back as the same double (13.649999999999999, not 13.65),
	// never in exponent form for the sizes a room has, minus zero as 0
	FString JsNum(double V)
	{
		if (V == 0) return TEXT("0");
		char Buf[64];
		int32 P = 1;
		for (; P <= 17; ++P) { std::snprintf(Buf, sizeof(Buf), "%.*g", (int)P, V); if (std::strtod(Buf, nullptr) == V) break; }
		if (P > 17) P = 17;
		if (const char* E = std::strchr(Buf, 'e')) std::snprintf(Buf, sizeof(Buf), "%.*f", FMath::Max(0, (int)P - 1 - std::atoi(E + 1)), V);
		return FString(ANSI_TO_TCHAR(Buf));
	}
	double YawOf(double Dx, double Dz) { return FMath::Atan2(Dx, -Dz); }   // play.js yawOf
	FVector2D Facing(double Yaw) { return FVector2D(FMath::Sin(Yaw), -FMath::Cos(Yaw)); }   // the way a yaw faces

	// ------------------------------------------------------------ the rooms' dressing: play.js room() and layout()
	// play.js C.stone, C.stoneDark, C.water; ROOM_GOODS; the colour of lit glass (the slice's lit-window quads, material 1)
	const FVector3f Stone(.52f, .48f, .43f), StoneDark(.37f, .34f, .32f), Water(.2f, .32f, .38f), LitGlass(.80f, .88f, .92f);
	const TArray<FVector3f> RoomGoods = { Ochre, Terracotta, Olive, Cream, FVector3f(.6f, .2f, .16f), FVector3f(.42f, .5f, .24f), FVector3f(.3f, .38f, .52f), FVector3f(.55f, .45f, .3f) };
	FVector3f Mul3(const FVector3f& C, double K) { return C * (float)K; }   // play.js mul3
	double JsRound(double V) { return FMath::FloorToDouble(V + .5); }      // Math.round

	// play.js hash() of (l.id, tag, ...): numbers as JS writes them
	FString HStr(const TCHAR* S) { return FString(S); }
	FString HStr(const FString& S) { return S; }
	FString HStr(double V) { return JsNum(V); }
	FString HStr(int32 V) { return FString::FromInt(V); }
	template <typename... T> double HashOf(const FString& Id, const TCHAR* Tag, const T&... K) { return BrixSliceHash({ Id, FString(Tag), HStr(K)... }); }

	// A box straight into the room, as dm.ybox() draws it (no keep-out test: the slice's dm.box, dm.ybox, fbox and quads have none)
	void Put(FBrixFurnished& Out, double cx, double y0, double cz, double sx, double sy, double sz, double yaw, const FVector3f& c, bool bGlow)
	{
		if (!(sx > 0 && sy > 0 && sz > 0)) return;   // (the slice would draw it inside out)
		Out.Boxes.Add({ FVector2D(cx, cz), (float)y0, FVector3f((float)sx, (float)sy, (float)sz), (float)yaw, c, bGlow });
	}
	void AABox(FBrixFurnished& Out, double x0, double y0, double z0, double x1, double y1, double z1, const FVector3f& c, bool bGlow = false)   // dm.box
	{
		Put(Out, (x0 + x1) / 2, FMath::Min(y0, y1), (z0 + z1) / 2, FMath::Abs(x1 - x0), FMath::Abs(y1 - y0), FMath::Abs(z1 - z0), 0, c, bGlow);
	}
	void Flat(FBrixFurnished& Out, double x0, double z0, double x1, double z1, double y, const FVector3f& c)   // dm.flat: a quad on the floor, a very thin box
	{
		AABox(Out, x0, y - .002, z0, x1, y, z1, c);
	}

	// room()'s walls table: is the wall along x, where its inner face is, which way is into the room
	struct FWall { bool ns = true; double fix = 0, out = 1; };
	FVector3d SPAt(const FWall& S, double a, double y, double d) { return S.ns ? FVector3d(a, y, S.fix + S.out * d) : FVector3d(S.fix + S.out * d, y, a); }   // SP.at
	// play.js fbox(): a box on a wall face, a along the wall, d out from it (the faces it leaves out do not matter here)
	void FBox(FBrixFurnished& Out, const FWall& S, double a0, double a1, double y0, double y1, double d0, double d1, const FVector3f& c, bool bGlow = false)
	{
		const double p0 = S.fix + S.out * d0, p1 = S.fix + S.out * d1, lo = FMath::Min(p0, p1), hi = FMath::Max(p0, p1);
		if (S.ns) AABox(Out, a0, y0, lo, a1, y1, hi, c, bGlow); else AABox(Out, lo, y0, a0, hi, y1, a1, c, bGlow);
	}
	// SP.disc(): a flat round face on a wall, looking along n; here a thin square of the same area on that face
	void Disc(FBrixFurnished& Out, const FVector3d& Ctr, const FVector2D& N, double r, const FVector3f& c)
	{
		const double s = r * 1.7725, th = .005;   // sqrt(pi) r
		if (FMath::Abs(N.Y) > FMath::Abs(N.X)) Put(Out, Ctr.X, Ctr.Y - s / 2, Ctr.Z, s, s, th, 0, c, false);
		else Put(Out, Ctr.X, Ctr.Y - s / 2, Ctr.Z, th, s, s, 0, c, false);
	}

	// What room() and layout() are handed by furnish(): the lot as Storeys.proxy() makes it for the floor, and furnish()'s B,
	// hard, chair and table. Heights are above the floor (the slice adds F, the floor's height; the port leaves it out).
	struct FDress
	{
		FBrixFurnished* Out = nullptr;
		TArray<FVector3d>* Occ = nullptr;        // play.js occ: [x, z, r] kept clear
		TFunction<void(double, double, double, double, double, double, const FVector3f&, double, bool)> BFn;
		TFunction<void(double, double, double, double, double, double)> HardFn;
		TFunction<void(double, double, double, const FVector3f&, bool)> ChairFn;
		TFunction<void(double, double, double, double, bool)> TableFn;
		TFunction<TArray<FVector2D>(const TCHAR*)> PlacesFn;   // the floor's places of a kind (l.objects)
		const FStyle* Rs = nullptr;              // R
		FString Id, Kind, Sp, Side;              // l.id ("id/f" above the ground, as proxy() names it), l.kind, l.sp, side
		FVector3f Accent = FVector3f(.5f), Fab = FVector3f(.5f);
		FVector2D Door = FVector2D::ZeroVector, Dir = FVector2D::ZeroVector, RMin = FVector2D::ZeroVector, RMax = FVector2D::ZeroVector;   // l.door, DOOR_DIR[side], l.rect
		double Dw = 1.7, FloorY = .2;            // l.dw; F (Storeys.floorY), only to work out heights under the ceiling
		int32 Level = 0;
		TOptional<FBrixStairGeo> SG;             // Storeys.stairOf(l)
		bool bTower = false;
		void B(double cx, double y0, double cz, double sx, double sy, double sz, const FVector3f& c, double yaw = 0, bool bGlow = false) const { BFn(cx, y0, cz, sx, sy, sz, c, yaw, bGlow); }
		void Hard(double ax0, double ay0, double az0, double ax1, double ay1, double az1) const { HardFn(ax0, ay0, az0, ax1, ay1, az1); }
		void Chair(double cx, double cz, double yaw, const FVector3f& c, bool bArm = false) const { ChairFn(cx, cz, yaw, c, bArm); }
		void Table(double cx, double cz, double size, double ht, bool bRound) const { TableFn(cx, cz, size, ht, bRound); }
		void OccPush(double ox, double oz, double r) const { Occ->Add(FVector3d(ox, oz, r)); }
		bool Clear(double px, double pz, double r) const   // room()'s clear(): nothing placed or used there
		{
			for (const FVector3d& O : *Occ) if (!(FMath::Sqrt(FMath::Square(px - O.X) + FMath::Square(pz - O.Y)) > r + O.Z)) return false;
			return true;
		}
		double CeilY() const { return 3.0 * Level + 3.2; }   // Storeys.ceilY (above the street, as the slice has it)
	};

	// play.js layout(): the arrangement of each kind of building, laid out from the door: `a` runs along the door wall, `i`
	// inward from it. Everything is placed round the data's own places (staff behind counters at the back, seats and customers
	// in front).
	void Layout(const FDress& D)
	{
		FBrixFurnished& Out = *D.Out;
		const double F = D.FloorY, CEIL = D.CeilY();         // this floor's ceiling (storeys.js); CEIL - F: its height over the floor
		const double x0 = D.RMin.X, z0 = D.RMin.Y, x1 = D.RMax.X, z1 = D.RMax.Y, t = .35;
		const FVector2D o = D.Dir;
		const double dx = D.Door.X, dz = D.Door.Y, dw = D.Dw, FR = FMath::Atan2(-o.X, -o.Y);
		auto h = [&D](const auto&... k) { return HashOf(D.Id, TEXT("lay"), k...); };
		const FVector3f WD = Wood, DARK(.3f, .2f, .13f), TOP(.62f, .58f, .52f), WHITE(.84f, .85f, .83f);
		auto wp = [&](double pa, double pi) { return FVector2D(dx - o.X * pi - o.Y * pa, dz - o.Y * pi + o.X * pa); };
		auto rel = [&](const FVector2D& q) { return FVector2D(-(q.X - dx) * o.Y + (q.Y - dz) * o.X, -((q.X - dx) * o.X + (q.Y - dz) * o.Y)); };
		double aMin = 1e30, aMax = -1e30, iMax = -1e30;
		for (const FVector2D& Cn : { FVector2D(x0, z0), FVector2D(x1, z0), FVector2D(x0, z1), FVector2D(x1, z1) }) { const FVector2D q = rel(Cn); aMin = FMath::Min(aMin, q.X); aMax = FMath::Max(aMax, q.X); iMax = FMath::Max(iMax, q.Y); }
		aMin += t; aMax -= t; iMax -= t;
		const double iMin = t;
		auto FB = [&](double fa, double fi, double y0, double sa, double sy, double si, const FVector3f& c, bool bGlow = false, double yaw = 0) { const FVector2D p = wp(fa, fi); D.B(p.X, y0, p.Y, sa, sy, si, c, FR + yaw, bGlow); };
		auto FH = [&](double ha0, double ha1, double hi0, double hi1, double ht) { const FVector2D p = wp(ha0, hi0), q = wp(ha1, hi1); D.Hard(p.X, 0, p.Y, q.X, ht, q.Y); };
		auto FO = [&](double fa, double fi, double r) { const FVector2D p = wp(fa, fi); D.OccPush(p.X, p.Y, r); };
		auto FF = [&](double fa, double fi, double r) { const FVector2D p = wp(fa, fi); return D.Clear(p.X, p.Y, r) && fa - r > aMin - .35 && fa + r < aMax + .35 && fi - r > iMin - .35 && fi + r < iMax + .35; };   // (pieces may stand against a wall)
		auto FFrun = [&](double fa, double ri0, double ri1, double r) { for (double ri = ri0; ri <= ri1; ri += .8) if (!FF(fa, ri, r)) return false; return true; };
		auto ob = [&](const TCHAR* K) { TArray<FVector2D> P = D.PlacesFn(K); for (FVector2D& q : P) q = rel(q); return P; };
		const FVector2D pv(-o.Y, o.X);
		auto lamp = [&](double la, double li, double drop, const FVector3f& c)
		{
			const FVector2D p = wp(la, li);
			AABox(Out, p.X - .01, CEIL - drop - F, p.Y - .01, p.X + .01, CEIL - F, p.Y + .01, Charcoal);
			Put(Out, p.X, CEIL - drop - .2 - F, p.Y, .32, .2, .32, 0, c, false);
			AABox(Out, p.X - .07, CEIL - drop - .27 - F, p.Y - .07, p.X + .07, CEIL - drop - .19 - F, p.Y + .07, FVector3f(1, .78f, .5f), true);
		};
		// glass(): a pane of lit glass at a, from i0 to i1, y0 to y1 (a quad in the slice: a very thin box here)
		auto glass = [&](double ga, double gi0, double gi1, double y0, double y1) { const FVector2D p = wp(ga, (gi0 + gi1) / 2); Put(Out, p.X, y0, p.Y, .01, y1 - y0, FMath::Abs(gi1 - gi0), FR, LitGlass, true); };
		const FString& Kd = D.Kind;
		const FString key = (Kd == TEXT("bank") || Kd == TEXT("dealer") || Kd == TEXT("library") || Kd == TEXT("museum") || Kd == TEXT("cinema")) ? Kd : D.Sp == TEXT("mall") ? TEXT("mall") : Kd == TEXT("food") ? TEXT("cafe") : Kd == TEXT("shop") ? TEXT("grocer") : Kd == TEXT("home") ? TEXT("home") : D.Sp == TEXT("works") ? TEXT("works")
			: Kd == TEXT("work") ? (D.Sp == TEXT("hall") ? TEXT("hall") : D.Sp == TEXT("guild") ? TEXT("guild") : TEXT("office")) : Kd == TEXT("health") ? TEXT("clinic") : Kd == TEXT("utility") ? TEXT("depot") : TEXT("");
		const TArray<FVector2D> Wk = ob(TEXT("work")), Cu = ob(TEXT("customer")), Ch = ob(TEXT("chair"));
		auto MaxI = [](const TArray<FVector2D>& P) { double m = -1e30; for (const FVector2D& q : P) m = FMath::Max(m, q.Y); return m; };
		auto MinI = [](const TArray<FVector2D>& P) { double m = 1e30; for (const FVector2D& q : P) m = FMath::Min(m, q.Y); return m; };
		auto MaxA = [](const TArray<FVector2D>& P) { double m = -1e30; for (const FVector2D& q : P) m = FMath::Max(m, q.X); return m; };
		auto MinA = [](const TArray<FVector2D>& P) { double m = 1e30; for (const FVector2D& q : P) m = FMath::Min(m, q.X); return m; };
		const FVector2D Corners[] = { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) };

		if (key == TEXT("cafe"))
		{
			// one coffee bar across the back joining the baristas' places: espresso machines, grinders, cups, a lit cake case
			if (Wk.Num())
			{
				const double wi = MaxI(Wk), ci = wi - .8, a0 = FMath::Max(aMin + .4, MinA(Wk) - 2.6), a1 = FMath::Min(aMax - .4, MaxA(Wk) + 2.6), am = (a0 + a1) / 2, L = a1 - a0;
				FB(am, ci, 0, L, 1.0, .62, DARK); FB(am, ci, 1.0, L + .1, .06, .8, TOP);
				for (int32 k = 0; k < (int32)FMath::FloorToDouble(L / .9); k++) FB(a0 + .45 + k * .9, ci - .32, .15, .78, .72, .02, FVector3f(.24f, .16f, .1f));
				FB(am, ci - .45, .18, L, .04, .04, Metal); FH(a0, a1, ci - .4, ci + .4, 1.06);
				for (const FVector2D& W : Wk)
				{
					const double wa = W.X;
					FB(wa - .25, ci + .1, 1.06, .56, .46, .42, Metal); FB(wa - .25, ci + .1, 1.52, .4, .06, .32, FVector3f(.6f, .58f, .55f)); FB(wa - .32, ci - .14, 1.18, .05, .12, .05, FVector3f(.2f, .2f, .22f)); FB(wa - .18, ci - .14, 1.18, .05, .12, .05, FVector3f(.2f, .2f, .22f));
					FB(wa + .4, ci + .15, 1.06, .18, .4, .18, FVector3f(.2f, .2f, .22f)); FB(wa + .4, ci + .15, 1.46, .2, .12, .2, FVector3f(.5f, .3f, .18f));
					for (int32 k = 0; k < 4; k++) FB(wa + .75 + (k % 2) * .11, ci - .05, 1.06 + (k / 2) * .09, .08, .085, .08, FVector3f(.9f, .88f, .84f));
				}
				FB(a1 - .75, ci, 1.06, 1.2, .48, .58, FVector3f(.95f, .86f, .64f), true); FB(a1 - .75, ci, 1.54, 1.24, .03, .62, Metal);
				for (int32 k = 0; k < 4; k++) FB(a1 - 1.2 + k * .3, ci, 1.08, .22, .11, .22, Pick(TArray<FVector3f>{ Ochre, FVector3f(.55f, .3f, .18f), Cream, FVector3f(.6f, .2f, .16f) }, h(k, TEXT("cake"))));
				FB(a0 + .35, ci, 1.06, .16, .22, .16, FVector3f(.75f, .8f, .82f));
				const double bi = iMax - .32;                     // the back counter, shelves of cups and bottles, chalk menus over the baristas
				if (bi - ci > 1.2)
				{
					FB(am, bi, 0, L, .9, .6, DARK); FB(am, bi, .9, L + .05, .04, .64, TOP); FH(a0, a1, bi - .3, iMax, .95);
					for (int32 s = 0; s < 2; s++)
					{
						FB(am, iMax - .14, 1.45 + s * .42, L, .04, .28, WD);
						for (int32 k = 0; k < (int32)FMath::FloorToDouble(L / .24); k++) FB(a0 + .15 + k * .24, iMax - .14, 1.49 + s * .42, .1, s ? .27 : .11, .1, s ? Pick(TArray<FVector3f>{ FVector3f(.16f, .3f, .18f), FVector3f(.38f, .2f, .1f), FVector3f(.6f, .52f, .3f), FVector3f(.5f, .14f, .12f) }, h(k, s)) : FVector3f(.9f, .88f, .84f));
					}
					for (int32 k = 0; k < 3; k++)
					{
						const double ma = am + (k - 1) * L / 3.2;
						FB(ma, iMax - .03, 2.25, 1.1, .72, .03, FVector3f(.1f, .1f, .1f)); FB(ma, iMax - .05, 2.86, .7, .05, .02, FVector3f(.9f, .86f, .74f));
						for (int32 q = 0; q < 4; q++) FB(ma - .3 + .12 * h(k, q), iMax - .05, 2.72 - q * .14, .4 + .3 * h(q, k), .03, .02, FVector3f(.86f, .82f, .72f));
					}
					for (double a = a0 + .7; a < a1; a += 1.5) FO(a, bi, .8);
				}
				const int32 NL = (int32)FMath::Max(2.0, JsRound(L / 2.4));
				for (int32 k = 0; k < NL; k++) lamp(a0 + (k + .5) * L / NL, ci, .85, Ochre);
				for (double a = a0 + .5; a < a1; a += 1.2) FO(a, ci, .7);
			}
			// a window bar with stools under the front windows, either side of the door
			for (const int32 sg : { -1, 1 })
			{
				const double b0 = sg < 0 ? aMin + .5 : dw / 2 + 1.6, b1 = sg < 0 ? -dw / 2 - 1.6 : aMax - .5;
				if (b1 - b0 < 1.8 || !FF((b0 + b1) / 2, .9, .2)) continue;
				bool ok = true; for (double a = b0 + .3; a < b1; a += .8) if (!FF(a, .95, .3)) { ok = false; break; }
				if (!ok) continue;
				FB((b0 + b1) / 2, .6, 1.02, b1 - b0, .05, .44, TOP); for (double a = b0 + .3; a < b1; a += 1.5) FB(a, .55, 0, .06, 1.02, .06, Metal); FH(b0, b1, .38, .82, 1.05);
				for (double a = b0 + .45; a < b1 - .2; a += .9) { FB(a, 1.1, .74, .36, .05, .36, DARK); FB(a, 1.1, 0, .05, .74, .05, Metal); FB(a, 1.1, .25, .3, .03, .3, Metal, false, PI / 4); }
				for (double a = b0; a <= b1; a += 1) FO(a, .95, .55);
			}
			const double ka = dw / 2 + .9;                       // a coat stand by the door
			if (FF(ka, 1.0, .3)) { FB(ka, 1.0, 0, .05, 1.8, .05, WD); FB(ka, 1.0, 0, .46, .04, .46, WD, false, PI / 4); FB(ka, 1.0, 1.62, .5, .05, .05, WD); FB(ka, 1.0, 1.62, .05, .05, .5, WD); FB(ka + .18, 1.0, 1.2, .2, .45, .12, Olive); FO(ka, 1.0, .35); }
		}

		if (key == TEXT("grocer"))
		{
			// checkouts at the staff places: a belt, a sweets rack in front, a stack of baskets by the door
			const double wiMin = Wk.Num() ? MinI(Wk) : iMax - 2, ci = wiMin - .8;
			for (const FVector2D& W : Wk)
			{
				const double wa = W.X, wiK = W.Y;
				FB(wa, wiK - .8, .98, 1.5, .02, .32, FVector3f(.12f, .12f, .13f)); FB(wa - .55, wiK - .8, .98, .3, .2, .3, FVector3f(.2f, .2f, .2f));
				FB(wa + .35, wiK - 1.3, 0, .9, 1.1, .26, WD); for (int32 s = 0; s < 3; s++) for (int32 k = 0; k < 4; k++) FB(wa + .02 + k * .22, wiK - 1.38, .18 + s * .3, .16, .13, .1, Pick(RoomGoods, h(wa, s, k)));
				FH(wa - .1, wa + .8, wiK - 1.45, wiK - 1.15, 1.1); FO(wa + .35, wiK - 1.3, .5);
				const FVector2D p = wp(wa, wiK - .8); AABox(Out, p.X - .01, CEIL - .55 - F, p.Y - .01, p.X + .01, CEIL - F, p.Y + .01, Charcoal); FB(wa, wiK - .8, CEIL - F - .8, .9, .25, .04, Teal); FB(wa, wiK - .82, CEIL - F - .74, .5, .12, .02, Cream);
			}
			const double ba = -(dw / 2 + 1.1);
			if (FF(ba, 1.0, .3)) { for (int32 k = 0; k < 5; k++) FB(ba, 1.0, k * .2, .5, .18, .36, FVector3f(.62f, .16f, .12f)); FO(ba, 1.0, .4); }
			// fridges with lit glass doors down one side wall; produce stands down the other
			auto sideRun = [&](double aw, int32 sg, const TFunction<void(double, double, int32, double)>& build) { for (double ri = 2.0; ri + 1.3 < iMax - .3; ri += 1.4) build(aw + sg * .45, ri + .65, sg, ri); };
			sideRun(aMin, 1, [&](double a, double i, int32 sg, double i0)
			{
				if (!FF(a, i, .5)) return;
				FB(a, i, 0, .86, 2.05, 1.34, WHITE); FB(a, i, 2.05, .92, .28, 1.36, Teal); FB(a + sg * .44, i, .0, .02, .15, 1.3, FVector3f(.2f, .2f, .22f));
				glass(a + sg * .44, i0 + .05, i0 + .64, .2, 1.95); glass(a + sg * .44, i0 + .68, i0 + 1.27, .2, 1.95);
				for (int32 s = 0; s < 4; s++) for (int32 k = 0; k < 4; k++) FB(a + sg * .2, i0 + .15 + k * .32, .3 + s * .42, .2, .2, .16, Pick(TArray<FVector3f>{ FVector3f(.85f, .85f, .8f), FVector3f(.3f, .45f, .6f), FVector3f(.7f, .2f, .15f), FVector3f(.85f, .7f, .3f) }, h(i0, s, k)));
				FH(a - .45, a + .45, i0, i0 + 1.34, 2.3); FO(a, i, .8);
			});
			sideRun(aMax, -1, [&](double a, double i, int32 sg, double i0)
			{
				if (!FF(a, i, .5)) return;
				FB(a, i, 0, .86, .62, 1.3, WD); FB(a - sg * .1, i, .62, .66, .2, 1.26, WD); FB(a + sg * .2, i, .62, .4, .45, 1.26, WD);
				for (int32 k = 0; k < 8; k++) FB(a + (k % 2 ? sg * .2 : -sg * .1), i0 + .2 + (k / 2) * .3, (k % 2 ? 1.07 : .82), .26, .1, .24, Pick(TArray<FVector3f>{ FVector3f(.6f, .2f, .16f), FVector3f(.42f, .5f, .24f), Ochre, FVector3f(.55f, .3f, .45f), FVector3f(.3f, .45f, .2f) }, h(i0, k)));
				FH(a - .45, a + .45, i0, i0 + 1.3, 1.1); FO(a, i, .8);
			});
			// aisles of stocked shelving between the customers' places, running in from the door; signs hang over them
			TArray<double> cas; for (const FVector2D& c : Cu) cas.Add(c.X);
			cas.StableSort();
			TArray<double> ais;
			for (int32 k = 0; k + 1 < cas.Num(); k++) ais.Add((cas[k] + cas[k + 1]) / 2);
			if (cas.Num()) { ais.Add(cas[0] - 2.6); ais.Add(cas.Last() + 2.6); }
			const double ai0 = 2.2, ai1 = FMath::Min(ci - 2.1, iMax - 1.5);
			for (const double a : ais)
			{
				if (FMath::Abs(a) < dw / 2 + 1.3 || a < aMin + 2.0 || a > aMax - 2.0 || ai1 - ai0 < 1.6 || !FFrun(a, ai0, ai1, .55)) continue;
				const double im = (ai0 + ai1) / 2, Lg = ai1 - ai0;
				FB(a, im, 0, 1.0, .12, Lg, FVector3f(.3f, .3f, .32f)); FB(a, im, .12, .06, 1.5, Lg, WHITE);
				for (int32 s = 0; s < 4; s++)
				{
					const double y = .22 + s * .38; FB(a, im, y, 1.0, .03, Lg, WHITE);
					for (const int32 sd : { -1, 1 }) for (int32 g = 0; g < (int32)FMath::FloorToDouble(Lg / .3); g++) FB(a + sd * .28, ai0 + .15 + g * .3, y + .03, .34, .12 + .16 * h(a, s, g, sd), .25, Pick(RoomGoods, h(a, g, s, sd)));
				}
				FB(a, ai0, 0, 1.04, 1.62, .06, Teal); FB(a, ai0 - .04, 1.2, .8, .3, .02, Cream);
				const FVector2D p = wp(a, im); AABox(Out, p.X - .01, CEIL - .5 - F, p.Y - .01, p.X + .01, CEIL - F, p.Y + .01, Charcoal); FB(a, im, CEIL - F - .78, .05, .28, 1.3, Teal); FB(a, im, CEIL - F - .72, .07, .14, 1.0, Cream);
				FH(a - .5, a + .5, ai0, ai1, 1.65); for (double i = ai0; i <= ai1; i += .9) FO(a, i, .6);
			}
			// stocked shelving along the back wall behind the checkouts
			if (iMax - ci > 1.6) for (double a = aMin + 1.2; a < aMax - 1.0; a += 2.0)
			{
				if (!FF(a, iMax - .3, .4)) continue;
				FB(a, iMax - .25, 0, 1.9, 1.9, .45, WD); for (int32 s = 0; s < 4; s++) for (int32 k = 0; k < 6; k++) FB(a - .8 + k * .32, iMax - .45, .2 + s * .44, .24, .12 + .14 * h(a, s, k), .1, Pick(RoomGoods, h(k, s, a)));
				FH(a - .95, a + .95, iMax - .5, iMax, 1.9); FO(a, iMax - .3, .7);
			}
		}

		if (key == TEXT("home"))
		{
			// one home round each bed: a wardrobe at the back wall, a rug and lamp by the armchair, a kitchenette and a small table
			const TArray<FVector2D> beds = ob(TEXT("bed")), rooms = ob(TEXT("room"));
			for (const FVector2D& Bd : beds)
			{
				const double ba = Bd.X, bi = Bd.Y, wa = ba + 1.5;
				if (bi > iMax - 2.8 && FF(wa, iMax - .35, .55)) { FB(wa, iMax - .32, 0, 1.1, 2.0, .58, WD); FB(wa, iMax - .62, .1, .02, 1.85, .02, Mul3(WD, .6)); FB(wa, iMax - .32, 2.0, 1.16, .07, .62, Mul3(WD, .8)); FB(wa - .1, iMax - .64, 1.0, .04, .15, .04, Ochre); FB(wa + .1, iMax - .64, 1.0, .04, .15, .04, Ochre); FH(wa - .55, wa + .55, iMax - .62, iMax, 2.0); FO(wa, iMax - .35, .7); }
			}
			for (int32 k = 0; k < rooms.Num(); k++)
			{
				const double ra = rooms[k].X, ri = rooms[k].Y;
				const FVector2D p = wp(ra, ri);
				Flat(Out, p.X - 1.25, p.Y - 1.0, p.X + 1.25, p.Y + 1.0, .004, Mul3(D.Accent, .7)); Flat(Out, p.X - 1.1, p.Y - .85, p.X + 1.1, p.Y + .85, .006, Mul3(Pick(RoomGoods, h(k, TEXT("rug"))), .85));   // the rug
				const double ta = ra + 1.0;
				if (FF(ta, ri, .35))   // the coffee table
				{
					FB(ta, ri, .38, .72, .05, .5, WD); for (const FVector2D& uv : Corners) FB(ta + uv.X * .3, ri + uv.Y * .2, 0, .04, .38, .04, WD);
					FB(ta, ri, .43, .26, .04, .18, Pick(RoomGoods, h(k, TEXT("bk")))); FB(ta + .2, ri + .1, .43, .08, .1, .08, FVector3f(.9f, .88f, .84f)); FH(ta - .36, ta + .36, ri - .25, ri + .25, .43); FO(ta, ri, .45);
				}
				const double la = ra - .8;
				if (FF(la, ri + .4, .2)) { FB(la, ri + .4, 0, .28, .03, .28, Metal); FB(la, ri + .4, 0, .03, 1.45, .03, Metal); FB(la, ri + .4, 1.4, .38, .32, .38, Cream); FB(la, ri + .4, 1.46, .14, .12, .14, FVector3f(1, .8f, .55f), true); FO(la, ri + .4, .3); }   // the floor lamp
			}
			for (const FVector2D& Bd : beds)   // the kitchenette
			{
				double ka = Bd.X; if (FMath::Abs(ka) < dw / 2 + 1.4) ka += ka >= 0 ? 2.4 : -2.4;
				if (!FF(ka, .65, .85)) continue;
				FB(ka, .62, 0, 1.8, .88, .58, FVector3f(.8f, .78f, .72f)); FB(ka, .62, .88, 1.84, .05, .62, FVector3f(.4f, .36f, .32f)); FB(ka - .45, .62, .93, .5, .02, .36, Metal); FB(ka - .45, .45, .93, .04, .3, .04, Metal);
				FB(ka + .45, .62, .93, .56, .03, .48, FVector3f(.15f, .15f, .16f)); for (const double u : { -.13, .13 }) FB(ka + .45 + u, .62, .96, .16, .02, .16, FVector3f(.3f, .3f, .3f));
				for (const double u : { -.6, 0.0, .6 }) FB(ka + u, .34, .15, .02, .65, .02, Mul3(FVector3f(.8f, .78f, .72f), .7));
				FB(ka, .5, 1.5, 1.8, .6, .32, WD); FH(ka - .9, ka + .9, .33, .92, .93); FO(ka, .65, .95);
				if (FF(ka, 2.0, .9))
				{
					const FVector2D q = wp(ka, 2.0); D.Table(q.X, q.Y, .8, .74, false); D.Hard(q.X - .4, 0, q.Y - .4, q.X + .4, .74, q.Y + .4);
					for (const int32 sg : { -1, 1 }) { const FVector2D c = wp(ka + sg * .62, 2.0); D.Chair(c.X, c.Y, YawOf(-sg * pv.X, -sg * pv.Y), D.Fab); }
					FO(ka, 2.0, 1.0);
				}
			}
			TArray<double> bs; for (const FVector2D& Bd : beds) bs.Add(Bd.X);
			bs.StableSort();
			for (int32 k = 0; k + 1 < bs.Num(); k++) if (bs[k + 1] - bs[k] > 5.5)   // a divider between two homes
			{
				const double ma = (bs[k] + bs[k + 1]) / 2, di0 = FMath::Max(iMin + 2.5, iMax - 6.5), dim = (di0 + iMax) / 2;
				if (!FFrun(ma, di0, iMax - .2, .1)) continue;
				FB(ma, dim, 0, .1, 1.2, iMax - di0, Mul3(WD, .9)); FB(ma, dim, 1.2, .16, .05, iMax - di0, WD); FH(ma - .06, ma + .06, di0, iMax, 1.25);
			}
		}

		if (key == TEXT("works"))
		{
			// a hoist rail under the beams; timber racks down one side wall, bar stock and sheets down the other; machines between the benches
			const double GF = 3.2;   // play.js GF
			FB((aMin + aMax) / 2, (iMin + iMax) / 2, GF - .5, aMax - aMin - 1, .16, .14, Metal);
			FB((aMin + aMax) / 2, (iMin + iMax) / 2, GF - 1.4, .03, .9, .03, Charcoal); FB((aMin + aMax) / 2, (iMin + iMax) / 2, GF - 1.5, .1, .12, .06, Metal);
			for (const TPair<double, int32>& AwSg : { TPair<double, int32>(aMin, 1), TPair<double, int32>(aMax, -1) })
			{
				const double aw = AwSg.Key; const int32 sg = AwSg.Value;
				for (double i = 2.0; i < iMax - 1.6; i += 2.4)
				{
					const double a = aw + sg * .45; if (!FF(a, i + 1.1, .5)) continue;
					for (const double u : { 0.0, 2.2 }) FB(a, i + u, 0, .1, 2.0, .1, Metal);
					for (int32 s = 0; s < 3; s++) { FB(a, i + 1.1, .4 + s * .6, .8, .04, 2.3, Metal); for (int32 k = 0; k < 4; k++) FB(a + (k - 1.5) * .18, i + 1.1, .44 + s * .6, .14, .1, 2.2, sg > 0 ? Mul3(WD, .9 + .2 * h(i, s, k)) : FVector3f(.4f, .42f, .44f)); }
					FH(a - .45, a + .45, i, i + 2.2, 2.0); FO(a, i + 1.1, 1.1);
				}
			}
			// rows: the staff places grouped by i written to one decimal (wi.toFixed(1)), in the order the rows are met
			auto ToFixed1 = [](double v) -> FString
			{
				char Buf[64];
				const double q = v * 4;
				if (q == FMath::FloorToDouble(q) && FMath::Abs(FMath::Fmod(q, 2.0)) == 1.0) { const double n = FMath::FloorToDouble(FMath::Abs(v) * 10 + .5); std::snprintf(Buf, sizeof(Buf), "%s%.1f", v < 0 ? "-" : "", n / 10); }   // a tie: JS rounds it away from zero
				else std::snprintf(Buf, sizeof(Buf), "%.1f", v);
				return FString(ANSI_TO_TCHAR(Buf));
			};
			TArray<TPair<FString, TArray<double>>> rows;
			for (const FVector2D& W : Wk)
			{
				const FString K = ToFixed1(W.Y);
				TPair<FString, TArray<double>>* Row = rows.FindByPredicate([&K](const TPair<FString, TArray<double>>& Rw) { return Rw.Key == K; });
				if (!Row) { rows.Add(TPair<FString, TArray<double>>(K, TArray<double>())); Row = &rows.Last(); }
				Row->Value.Add(W.X);
			}
			for (TPair<FString, TArray<double>>& Rw : rows)
			{
				TArray<double>& as = Rw.Value; as.StableSort();
				for (int32 k = 0; k + 1 < as.Num(); k++)
				{
					const double ma = (as[k] + as[k + 1]) / 2, mi = FCString::Atod(*Rw.Key) - .4; if (!FF(ma, mi, .9)) continue;
					FB(ma, mi, 0, 1.4, .95, .8, FVector3f(.3f, .34f, .32f)); FB(ma - .3, mi, .95, .5, .55, .6, FVector3f(.3f, .34f, .32f)); FB(ma + .4, mi, .95, .5, .2, .5, Metal); FB(ma + .55, mi - .3, 1.0, .06, .35, .2, Ochre); FH(ma - .7, ma + .7, mi - .4, mi + .4, 1.5); FO(ma, mi, 1.0);
				}
			}
		}

		if (key == TEXT("office") || key == TEXT("hall") || key == TEXT("guild"))
		{
			// a typewriter and papers on every desk; the town hall gets a public counter and benches, the guild banners, the offices a water cooler
			for (const FVector2D& W : Wk) { const double wa = W.X, wi = W.Y; FB(wa - .15, wi - .8, .76, .42, .1, .3, FVector3f(.15f, .15f, .16f)); FB(wa - .15, wi - .68, .86, .38, .14, .03, FVector3f(.9f, .9f, .86f)); FB(wa + .35, wi - .85, .76, .2, .08, .26, FVector3f(.86f, .84f, .78f)); }
			if (key == TEXT("hall") && Wk.Num())
			{
				const double ci = MinI(Wk) - 2.6;
				for (const FVector2D& S01 : { FVector2D(aMin + 1.5, -dw / 2 - 1.2), FVector2D(dw / 2 + 1.2, aMax - 1.5) })
				{
					const double s0 = S01.X, s1 = S01.Y;
					if (s1 - s0 > 2 && FFrun((s0 + s1) / 2, ci - .3, ci + .3, .2))
					{
						FB((s0 + s1) / 2, ci, 0, s1 - s0, 1.05, .6, DARK); FB((s0 + s1) / 2, ci, 1.05, s1 - s0 + .1, .05, .72, TOP);
						for (double a = s0 + .8; a < s1; a += 2.2) { FB(a, ci, 1.1, .05, .5, .05, Metal); FB(a, ci + .05, 1.4, .3, .22, .03, Cream); }
						FH(s0, s1, ci - .3, ci + .3, 1.1); for (double a = s0; a < s1; a += 1) FO(a, ci, .6);
					}
				}
				for (const int32 sg : { -1, 1 }) for (int32 r = 0; r < 2; r++)
				{
					const double a = sg * (dw / 2 + 3.5), i = 1.8 + r * 1.6; if (!FF(a, i, 1.3)) continue;
					FB(a, i, .42, 2.4, .06, .45, WD); FB(a, i + .2, .48, 2.4, .4, .06, WD); for (const double u : { -1.1, 1.1 }) FB(a + u, i, 0, .08, .42, .4, WD); FH(a - 1.2, a + 1.2, i - .25, i + .25, .5); FO(a, i, 1.3);
				}
				for (const int32 sg : { -1, 1 })
				{
					const double a = sg * 3.2, i = iMax - .5; if (!FF(a, i, .3)) continue;
					FB(a, i, 0, .3, .1, .3, StoneDark); FB(a, i, 0, .04, 2.6, .04, Metal); FB(a + .3, i, 1.6, .6, .9, .02, sg < 0 ? Terracotta : Teal); FO(a, i, .5);
				}
				FB(0, iMax - .03, 1.5, 1.3, 1.1, .05, FVector3f(.3f, .2f, .12f)); FB(0, iMax - .06, 1.58, 1.1, .94, .02, FVector3f(.42f, .34f, .3f));
			}
			if (key == TEXT("guild"))
				for (const TPair<double, int32>& AwSg : { TPair<double, int32>(aMin, 1), TPair<double, int32>(aMax, -1) })
					for (double i = 2.5; i < iMax - 1; i += 3.0)
					{
						const double aw = AwSg.Key; const int32 sg = AwSg.Value;
						FB(aw + sg * .02, i, 1.0, .03, 1.6, .7, Pick(TArray<FVector3f>{ Terracotta, Teal, Ochre }, h(i, sg))); FB(aw + sg * .04, i, 2.55, .05, .06, .85, Metal); FB(aw + sg * .04, i, 1.5, .02, .3, .3, Ochre);
					}
			if (key == TEXT("office")) { const double a = aMax - .6, i = 1.6; if (FF(a, i, .35)) { FB(a, i, 0, .4, .95, .4, WHITE); FB(a, i, .95, .3, .45, .3, FVector3f(.55f, .7f, .82f)); FB(a, i - .21, .8, .06, .06, .04, Metal); FO(a, i, .4); } }
		}

		if (key == TEXT("clinic"))
		{
			// a low table with magazines before the waiting chairs; an examination bay with a couch, screens, a sink and a cabinet at the back
			if (Ch.Num())
			{
				double sum = 0; for (const FVector2D& c : Ch) sum += c.X;
				const double ma = sum / Ch.Num(), mi = MinI(Ch) - 1.0;
				if (FF(ma, mi, .6))
				{
					FB(ma, mi, .36, 1.2, .05, .55, WD); for (const FVector2D& uv : Corners) FB(ma + uv.X * .55, mi + uv.Y * .22, 0, .04, .36, .04, WD);
					for (int32 k = 0; k < 3; k++) FB(ma - .35 + k * .3, mi, .41, .22, .02, .3, Pick(TArray<FVector3f>{ Teal, Terracotta, Cream }, h(k, TEXT("mag"))));
					FH(ma - .6, ma + .6, mi - .28, mi + .28, .41); FO(ma, mi, .8);
				}
			}
			const double recA = Wk.Num() ? Wk[0].X : 0, ea = recA > 0 ? aMin + 2.0 : aMax - 2.0, ei = iMax - 1.4;
			if (FF(ea, ei, 1.2))
			{
				FB(ea, ei, 0, 1.95, .6, .7, WHITE); FB(ea, ei, .6, 1.9, .12, .66, FVector3f(.92f, .92f, .9f)); FB(ea + .7, ei, .72, .45, .1, .32, FVector3f(.95f, .95f, .93f));
				for (int32 k = 0; k < 3; k++) FB(ea - .9 + k * .9, ei - 1.0, 0, .86, 1.75, .04, FVector3f(.82f, .8f, .72f), false, k == 1 ? 0 : (k - 1) * .3);
				FB(ea - 1.6, iMax - .3, 0, .7, .85, .5, WHITE); FB(ea - 1.6, iMax - .3, .85, .45, .04, .35, Metal); FB(ea + 1.5, iMax - .25, 0, .9, 1.85, .4, WHITE); FB(ea + 1.5, iMax - .46, 1.45, .08, .3, .02, Teal); FB(ea + 1.5, iMax - .46, 1.56, .3, .08, .02, Teal);
				FH(ea - 1.0, ea + 1.0, ei - .35, ei + .35, .75); FO(ea, ei, 1.6);
			}
		}

		if (key == TEXT("depot"))
		{
			// a tram bay down one side: rails, a pit, a tram standing over it
			const double ta = aMin + 2.0, ti0 = 1.6, ti1 = iMax - 1.2;
			if (ti1 - ti0 > 6 && FFrun(ta, ti0 + 1, ti1 - 1, 1.2))
			{
				for (const double u : { -.72, .72 }) FB(ta + u, (ti0 + ti1) / 2, 0, .08, .05, ti1 - ti0, Metal);
				const double tl = FMath::Min(11.0, ti1 - ti0 - 1), tm = (ti0 + ti1) / 2;
				FB(ta, tm, .35, 2.4, 1.0, tl, Terracotta); FB(ta, tm, 1.35, 2.4, 1.4, tl, Cream); FB(ta, tm, 2.75, 2.5, .2, tl + .1, Metal);
				for (int32 k = 0; k < (int32)FMath::FloorToDouble(tl / 1.9); k++) for (const int32 sg : { -1, 1 }) glass(ta + sg * 1.21, tm - tl / 2 + .4 + k * 1.9, tm - tl / 2 + 1.8 + k * 1.9, 1.5, 2.55);
				FH(ta - 1.25, ta + 1.25, tm - tl / 2, tm + tl / 2, 2.95); for (double i = ti0; i < ti1; i += 1.5) FO(ta, i, 1.6);
			}
		}

		if (key == TEXT("mall"))
		{
			// a shop booth at each trader's place: shelves behind, a canopy and a name board over the counter; a fountain in the court
			for (const FVector2D& W : Wk)
			{
				const double wa = W.X, wi = W.Y;
				const FVector3f col = Pick(TArray<FVector3f>{ Teal, Terracotta, Ochre, Blue }, h(wa, TEXT("bo")));
				FB(wa, wi + .6, 0, 3.2, 2.2, .45, WD); for (int32 s = 0; s < 4; s++) for (int32 k = 0; k < 9; k++) FB(wa - 1.4 + k * .35, wi + .38, .25 + s * .5, .28, .14 + .14 * h(wa, s, k), .12, Pick(RoomGoods, h(k, s, wa)));
				for (const double u : { -1.6, 1.6 }) for (const double v : { -1.2, .8 }) FB(wa + u, wi + v, 0, .08, 2.6, .08, Metal);
				FB(wa, wi - .2, 2.6, 3.5, .12, 2.2, col); FB(wa, wi - 1.32, 2.2, 3.4, .4, .06, col); FB(wa, wi - 1.36, 2.28, 2.2, .22, .02, Cream);
				FH(wa - 1.6, wa + 1.6, wi + .35, wi + .85, 2.2); FO(wa, wi + .6, 1.6);
			}
			const double fa = 0, fi = ((Cu.Num() ? MinI(Cu) : iMax / 2) + 2.0) / 2 + .5;
			if (FF(fa, fi, 1.8)) { FB(fa, fi, 0, 3.0, .45, 3.0, Stone, false, PI / 8); FB(fa, fi, 0, 3.0, .45, 3.0, Stone, false, -PI / 8 + PI / 4); FB(fa, fi, .38, 2.5, .04, 2.5, Water); FB(fa, fi, 0, .3, 1.4, .3, Stone); FB(fa, fi, 1.4, .9, .12, .9, StoneDark, false, PI / 4); FH(fa - 1.5, fa + 1.5, fi - 1.5, fi + 1.5, .45); FO(fa, fi, 2.0); }
		}

		const FVector2D backN = o;   // [o[0], 0, o[1]]
		auto onBack = [&](double ba, double y, double d) { const FVector2D p = wp(ba, iMax - d); return FVector3d(p.X, y, p.Y); };   // (heights above the floor)
		auto counterRow = [&](const FVector3f& bodyC, const FVector3f& topC, double ht, double& Ra0, double& Ra1, double& Rci)   // one long counter across the back joining the staff places
		{
			const double wi = MaxI(Wk), ci = wi - .8, a0 = FMath::Max(aMin + .4, MinA(Wk) - 1.6), a1 = FMath::Min(aMax - .4, MaxA(Wk) + 1.6), am = (a0 + a1) / 2, L = a1 - a0;
			FB(am, ci, 0, L, ht, .6, bodyC); FB(am, ci, ht, L + .08, .05, .74, topC);
			for (int32 k = 0; k < (int32)FMath::FloorToDouble(L / 1.0); k++) FB(a0 + .5 + k, ci - .31, .12, .86, ht - .25, .02, Mul3(bodyC, .8));
			FH(a0, a1, ci - .35, ci + .35, ht + .05); for (double a = a0 + .5; a < a1; a += 1.2) FO(a, ci, .7);
			Ra0 = a0; Ra1 = a1; Rci = ci;
		};

		if (key == TEXT("bank") && Wk.Num())
		{
			// the tellers' counter with glass screens and a brass tray at each window; the vault door and a clock on the back wall;
			// queue posts with ropes before the counter; writing desks by the door
			double a0 = 0, a1 = 0, ci = 0; counterRow(DARK, FVector3f(.74f, .72f, .66f), 1.05, a0, a1, ci);
			const FVector3f Brass(.55f, .45f, .2f);
			FB((a0 + a1) / 2, ci, 1.1, a1 - a0, .85, .03, FVector3f(.6f, .7f, .76f)); FB((a0 + a1) / 2, ci, 1.95, a1 - a0 + .1, .25, .14, DARK);
			for (const FVector2D& W : Wk) { const double wa = W.X; FB(wa - .6, ci, 1.1, .06, .85, .08, Brass); FB(wa, ci - .05, 1.1, .45, .05, .14, Brass); FB(wa, ci - .04, 1.12, .3, .12, .02, FVector3f(.12f, .12f, .12f)); }
			const double va = (a0 + a1) / 2;
			if (iMax - ci > 1.1) { Disc(Out, onBack(va, 1.25, .04), backN, 1.15, FVector3f(.32f, .34f, .36f)); Disc(Out, onBack(va, 1.25, .07), backN, .9, FVector3f(.42f, .44f, .46f)); Disc(Out, onBack(va, 1.25, .1), backN, .16, Brass); for (int32 k = 0; k < 4; k++) FB(va, iMax - .12, 1.21, .06, .06, .02, Brass, false, k * PI / 4); }
			Disc(Out, onBack(va + 3.5, 2.5, .04), backN, .42, DARK); Disc(Out, onBack(va + 3.5, 2.5, .06), backN, .34, FVector3f(.9f, .88f, .82f));
			const double qi = ci - 3.0;
			if (qi > 2) for (double a = -6; a <= 6; a += 1.5)
			{
				if (FMath::Abs(a) < dw / 2 + .4 || !FF(a, qi, .15)) continue;
				FB(a, qi, 0, .22, .04, .22, Brass); FB(a, qi, 0, .05, .95, .05, Brass); if (a + 1.5 <= 6 && FMath::Abs(a + .75) >= dw / 2 + .4) FB(a + .75, qi, .82, 1.4, .05, .05, FVector3f(.5f, .1f, .1f)); FO(a, qi, .3);
			}
			for (const int32 sg : { -1, 1 })
			{
				const double a = sg * (dw / 2 + 3.0), i = 2.2; if (!FF(a, i, .7)) continue;
				for (const double u : { -.4, .4 }) FB(a + u, i, 0, .06, 1.0, .06, Metal);
				FB(a, i, 1.0, 1.1, .05, .55, FVector3f(.3f, .2f, .13f)); for (int32 k = 0; k < 3; k++) FB(a - .3 + k * .3, i, 1.05, .2, .01, .28, FVector3f(.9f, .88f, .82f)); FH(a - .55, a + .55, i - .28, i + .28, 1.05); FO(a, i, .8);
			}
		}

		if (key == TEXT("library"))
		{
			// stacks of double-sided bookcases between the readers' tables and the browsers; green lamps on every table;
			// a card catalogue and a globe by the door (the issue desk is the counter at the librarians' places)
			const TArray<FVector2D> Se = ob(TEXT("seat"));
			const double si = Se.Num() ? MaxI(Se) : 6, cuI = Cu.Num() ? MinI(Cu) : iMax - 4, sti = (si + cuI) / 2;
			if (cuI - si > 2.4) for (double a = aMin + 1.7; a + 2.6 < aMax - 1.0; a += 3.6)
			{
				const double m = a + 1.3; if (FMath::Abs(m) < dw / 2 + 1.6 || !FF(m, sti, .45) || !FF(a + .2, sti, .3) || !FF(a + 2.4, sti, .3)) continue;
				FB(m, sti, 0, 2.6, 2.1, .7, WD); FB(m, sti, 2.1, 2.7, .06, .74, Mul3(WD, .8));
				for (int32 s = 0; s < 5; s++) for (const int32 sd : { -1, 1 })
				{
					FB(m, sti + sd * .31, .1 + s * .42, 2.5, .03, .1, Mul3(WD, .9));
					for (int32 k = 0; k < 14; k++) { const double bh = .24 + .1 * h(m, s, k, sd); FB(a + .15 + k * .17, sti + sd * .3, .13 + s * .42, .13, bh, .12, Pick(RoomGoods, h(k, s, m, sd))); }
				}
				FH(a, a + 2.6, sti - .35, sti + .35, 2.1); FO(m, sti, 1.5);
			}
			const TArray<FVector2D> Seats = D.PlacesFn(TEXT("seat"));
			for (int32 k = 0; k < Seats.Num(); k++)
			{
				const FVector2D q = rel(FVector2D(Seats[k].X + (k % 2 ? -1 : 1) * .7, Seats[k].Y)); const double ta = q.X, ti = q.Y;
				FB(ta, ti, .74, .14, .03, .14, FVector3f(.55f, .45f, .2f)); FB(ta, ti, .74, .02, .3, .02, FVector3f(.55f, .45f, .2f)); FB(ta, ti, 1.02, .28, .1, .14, FVector3f(.16f, .36f, .22f)); FB(ta, ti, .98, .1, .05, .06, FVector3f(1, .8f, .55f), true);
			}
			const double ca = dw / 2 + 2.2;
			if (FF(ca, 1.2, .7)) { FB(ca, 1.2, 0, 1.2, 1.15, .5, WD); for (int32 r = 0; r < 4; r++) for (int32 q = 0; q < 4; q++) FB(ca - .42 + q * .28, 1.2 - .26, .15 + r * .25, .22, .18, .02, Mul3(WD, .75)); FH(ca - .6, ca + .6, .95, 1.45, 1.15); FO(ca, 1.2, .8); }
			const double ga = -(dw / 2 + 2.2);
			if (FF(ga, 1.4, .5)) { FB(ga, 1.4, 0, .5, .04, .5, WD, false, PI / 4); FB(ga, 1.4, 0, .05, .85, .05, WD); FB(ga, 1.4, .85, .5, .5, .5, Blue, false, PI / 4); FB(ga, 1.4, .92, .36, .36, .36, Olive, false, PI / 8); FO(ga, 1.4, .45); }
		}

		if (key == TEXT("museum"))
		{
			// an exhibit before each visitor's place (a statue on a plinth, an urn, a glass case of finds) behind rope posts; a great
			// painting across the back wall
			const FVector3f stone(.72f, .7f, .66f), Brass(.55f, .45f, .2f);
			for (int32 k = 0; k < Cu.Num(); k++)
			{
				const double ca = Cu[k].X, ci = Cu[k].Y, ei = ci + 1.4; if (!FF(ca, ei, .32)) continue;
				const int32 tk = k % 3;
				FB(ca, ei, 0, .8, 1.0, .8, stone);
				if (tk == 0) { FB(ca, ei, 1.0, .5, .1, .4, stone); FB(ca - .1, ei, 1.1, .14, .7, .16, stone); FB(ca + .1, ei, 1.1, .14, .7, .16, stone); FB(ca, ei, 1.8, .42, .62, .26, stone); FB(ca, ei, 2.44, .2, .26, .22, stone); FB(ca + .3, ei, 1.9, .1, .5, .12, stone, false, .4); }
				else if (tk == 1) { FB(ca, ei, 1.0, .3, .1, .3, Terracotta, false, PI / 4); FB(ca, ei, 1.1, .5, .38, .5, Terracotta, false, PI / 4); FB(ca, ei, 1.48, .3, .22, .3, Terracotta, false, PI / 4); FB(ca, ei, 1.7, .4, .06, .4, Terracotta, false, PI / 4); FB(ca, ei, 1.3, .54, .06, .54, Charcoal, false, PI / 4); }
				else { FB(ca, ei, 1.0, .74, .55, .74, FVector3f(.62f, .72f, .76f)); for (int32 q = 0; q < 3; q++) FB(ca - .2 + q * .2, ei, 1.02, .12, .05 + .06 * q, .16, Pick(TArray<FVector3f>{ Ochre, Brass, Teal }, h(k, q))); }
				for (const double u : { -.75, .75 }) { FB(ca + u, ei - .8, 0, .2, .04, .2, Brass); FB(ca + u, ei - .8, 0, .05, .9, .05, Brass); }
				FB(ca, ei - .8, .8, 1.5, .05, .05, FVector3f(.5f, .1f, .1f));
				FH(ca - .45, ca + .45, ei - .45, ei + .45, 1.6); FO(ca, ei, .9);
			}
			const double pw = FMath::Min(6.0, aMax - aMin - 6);
			FB(0, iMax - .05, .9, pw + .3, 2.1, .06, FVector3f(.42f, .3f, .14f)); FB(0, iMax - .1, 1.05, pw, 1.8, .03, FVector3f(.36f, .42f, .5f)); FB(-pw * .2, iMax - .12, 1.2, pw * .4, .7, .02, FVector3f(.5f, .44f, .3f)); FB(pw * .15, iMax - .12, 1.6, pw * .35, .9, .02, FVector3f(.3f, .36f, .24f));
		}

		if (key == TEXT("cinema"))
		{
			// the screen on the back wall between red curtains, a stage before it, a red runner up the aisle, wall lights low and
			// warm; ticket and sweets counters by the door (the counters at the staff places), a popcorn machine on one
			const double sw = FMath::Min(aMax - aMin - 4, 14.0);
			const FVector3f red(.5f, .1f, .1f);
			{ const FVector2D p = wp(0, iMax - .06); Put(Out, p.X, .95, p.Y, sw, 2.0, .01, FR, FVector3f(.85f, .87f, .9f), true); }   // the screen (a quad in the slice)
			for (const double s : { -1.0, 1.0 }) FB(s * (sw / 2 + .55), iMax - .2, 0, 1.1, 3.0, .26, red);
			FB(0, iMax - .2, 2.95, sw + 2.2, .25, .26, red);
			FB(0, iMax - 1.0, 0, sw + 1, .42, 1.5, DARK); FH(-(sw + 1) / 2, (sw + 1) / 2, iMax - 1.75, iMax - .25, .42); FO(0, iMax - 1.0, 2.5);
			{ const FVector2D p0 = wp(-.55, 3.0), p1 = wp(.55, iMax - 1.8); Flat(Out, FMath::Min(p0.X, p1.X), FMath::Min(p0.Y, p1.Y), FMath::Max(p0.X, p1.X), FMath::Max(p0.Y, p1.Y), .005, FVector3f(.36f, .08f, .08f)); }
			for (const TPair<double, int32>& AwSg : { TPair<double, int32>(aMin, 1), TPair<double, int32>(aMax, -1) })
				for (double i = 3; i < iMax - 1; i += 3.0) { const double aw = AwSg.Key; const int32 sg = AwSg.Value; FB(aw + sg * .08, i, 2.1, .14, .24, .24, FVector3f(1, .72f, .42f), true); FB(aw + sg * .05, i, 2.0, .08, .1, .3, FVector3f(.55f, .45f, .2f)); }
			if (Wk.Num()) { const double wa = Wk[0].X, wi = Wk[0].Y; FB(wa + .45, wi - .8, .98, .5, .62, .42, FVector3f(.95f, .82f, .5f), true); FB(wa + .45, wi - .8, 1.6, .54, .06, .46, red); }
		}

		if (key == TEXT("dealer"))
		{
			// cars on show between the buyers' places, each with a price board on a stand; tyres and oil by the back wall; a brochure rack
			const TArray<FVector3f> colors = { Teal, Terracotta, Cream, Blue, Olive, FVector3f(.52f, .16f, .14f) };
			auto car = [&](double ca, double ci, const FVector3f& col)
			{
				FB(ca, ci, .32, 1.8, .5, 4.2, col); FB(ca, ci - .15, .82, 1.62, .48, 2.2, col); FB(ca, ci - .15, .86, 1.64, .36, 2.0, FVector3f(.32f, .42f, .48f));
				FB(ca, ci + 1.8, .6, 1.7, .14, .5, col); for (const double u : { -1.0, 1.0 }) for (const double v : { -1.35, 1.35 }) FB(ca + u * .82, ci + v, 0, .26, .64, .64, Charcoal);
				for (const double v : { -2.12, 2.12 }) FB(ca, ci + v, .34, 1.74, .14, .1, Metal);
				for (const double u : { -.6, .6 }) FB(ca + u, ci - 2.16, .55, .26, .14, .04, FVector3f(1, .9f, .7f), true);
				FB(ca + 1.4, ci - 1.6, 0, .04, 1.0, .04, Metal); FB(ca + 1.4, ci - 1.6, 1.0, .5, .36, .04, Cream);
				FH(ca - .9, ca + .9, ci - 2.1, ci + 2.1, 1.3); FO(ca, ci, 2.3);
			};
			TArray<double> cas; for (const FVector2D& c : Cu) cas.Add(c.X);
			cas.StableSort();
			const double ci = Cu.Num() ? Cu[0].Y : 6;
			int32 n = 0;
			for (int32 k = 0; k + 1 < cas.Num(); k++) { const double m = (cas[k] + cas[k + 1]) / 2; if (FMath::Abs(m) < dw / 2 + 1.4 || !FF(m, ci + .6, 1.0) || !FF(m, ci - 1.2, .8) || !FF(m, ci + 2.4, .8)) continue; car(m, ci + .6, colors[n++ % colors.Num()]); }
			for (double a = aMin + 2.5; a < aMax - 2; a += 5.5) { if (FMath::Abs(a) > dw / 2 + 1.4 && FF(a, ci + 5.2, 1.0) && FF(a, ci + 3.4, .8) && FF(a, ci + 7.0, .8)) car(a, ci + 5.2, colors[n++ % colors.Num()]); }
			for (double a = aMin + 1.0; a < aMax - 1; a += 4.0) { if (!FF(a, iMax - .5, .5)) continue; for (int32 k = 0; k < 4; k++) FB(a, iMax - .5, k * .24, .62, .22, .62, Charcoal, false, k * .3); FB(a + .7, iMax - .4, 0, .3, .45, .3, Ochre); FO(a, iMax - .5, .7); }
			const double ra = -(dw / 2 + 1.8);
			if (FF(ra, 1.2, .5)) { FB(ra, 1.2, 0, .7, 1.4, .3, WD); for (int32 r = 0; r < 3; r++) for (int32 q = 0; q < 3; q++) FB(ra - .22 + q * .22, 1.04, .3 + r * .38, .18, .26, .02, Pick(colors, h(r, q, TEXT("br")))); FO(ra, 1.2, .5); }
		}
	}

	// play.js room(): the room dressed for what the building is: the way in kept clear, layout(), then along each wall tall
	// pieces where there is no window and it is not the door wall, low ones elsewhere, pictures above the low ones; what this
	// kind of building has standing on the open floor; lamps hanging from the beams.
	void Room(const FDress& D)
	{
		FBrixFurnished& Out = *D.Out;
		const FStyle& R = *D.Rs;
		const double x0 = D.RMin.X, z0 = D.RMin.Y, x1 = D.RMax.X, z1 = D.RMax.Y, t = .35, rx0 = x0 + t, rx1 = x1 - t, rz0 = z0 + t, rz1 = z1 - t;
		const FVector2D d = D.Dir;
		const double dx = D.Door.X, dz = D.Door.Y, dw = D.Dw;
		for (int32 s = 0; s < 5; s++) D.OccPush(dx - d.X * s * 1.2, dz - d.Y * s * 1.2, dw / 2 + .7);   // keep the way in clear
		if (D.SG)                                                 // and the stairwell (storeys.js)
		{
			const FBrixStairGeo& SG = *D.SG;
			for (double u = SG.U0 + .5; u < SG.U1; u += .8) for (const double v : { ((double)SG.Lane0.X + SG.Lane0.Y) / 2, ((double)SG.Lane1.X + SG.Lane1.Y) / 2 }) { const FVector2D q = SG.P((float)u, (float)v); D.OccPush(q.X, q.Y, .9); }
			if (!(D.Level > 0))                                   // and the way from the door to its foot
			{
				const FVector2D ft = SG.FootAt(0), p1(dx - d.X * 1.6, dz - d.Y * 1.6);
				const double L = FMath::Sqrt(FMath::Square(ft.X - p1.X) + FMath::Square(ft.Y - p1.Y));
				for (double s = 0; s <= L; s += .7) D.OccPush(p1.X + (ft.X - p1.X) * s / L, p1.Y + (ft.Y - p1.Y) * s / L, .8);
			}
		}
		if (D.bTower)
		{
			const double bx = D.Side == TEXT("e") ? x0 + 1.5 : D.Side == TEXT("w") ? x1 - 1.5 : x0 + 1.5, bz = D.Side == TEXT("s") ? z0 + 1.5 : D.Side == TEXT("n") ? z1 - 1.5 : z0 + 1.5;
			D.OccPush(bx, bz, 3.2);
		}
		auto freeAt = [&](double px, double pz, double r) { return px > rx0 + r * .7 && px < rx1 - r * .7 && pz > rz0 + r * .7 && pz < rz1 - r * .7 && D.Clear(px, pz, r); };
		const TMap<FString, FWall> walls = { { TEXT("n"), { true, rz0, 1 } }, { TEXT("s"), { true, rz1, -1 } }, { TEXT("w"), { false, rx0, 1 } }, { TEXT("e"), { false, rx1, -1 } } };
		const FVector3f W = Wood;
		auto h = [&D](const auto&... k) { return HashOf(D.Id, TEXT("room"), k...); };
		bool menuDone = false;
		auto box = [&](const FWall& S, double ba, double y0, double y1, double d0, double d1, double w, const FVector3f& c, bool bGlow = false) { FBox(Out, S, ba - w / 2, ba + w / 2, y0, y1, d0, d1, c, bGlow); };
		auto solidOn = [&](const FWall& S, double sa, double w, double dep, double ht) { const FVector3d p = SPAt(S, sa - w / 2, 0, 0), q = SPAt(S, sa + w / 2, 0, dep); D.Hard(p.X, 0, p.Z, q.X, ht, q.Z); };
		// wall pieces: [width, depth, height] and how each is built (P)
		static const TMap<FString, FVector3d> PSize = {
			{ TEXT("wardrobe"), FVector3d(1.2, .6, 2.1) }, { TEXT("bookcase"), FVector3d(1.3, .34, 2.0) }, { TEXT("dresser"), FVector3d(1.0, .5, 1.0) }, { TEXT("fireplace"), FVector3d(1.8, .45, 1.25) },
			{ TEXT("sideboard"), FVector3d(1.6, .45, .85) }, { TEXT("plant"), FVector3d(.6, .6, 1.3) }, { TEXT("bottles"), FVector3d(1.6, .3, 1.9) }, { TEXT("pastry"), FVector3d(1.4, .6, 1.05) },
			{ TEXT("crates"), FVector3d(1.2, .8, 1.1) }, { TEXT("workbench"), FVector3d(2.2, .7, .9) }, { TEXT("machine"), FVector3d(1.3, .9, 1.4) }, { TEXT("barrels"), FVector3d(1.3, .65, .95) },
			{ TEXT("filing"), FVector3d(1.2, .6, 1.35) }, { TEXT("medcab"), FVector3d(1.0, .45, 1.9) }, { TEXT("sink"), FVector3d(1.0, .55, .9) }, { TEXT("bench"), FVector3d(1.8, .5, .9) },
			{ TEXT("lockers"), FVector3d(1.4, .5, 1.85) }, { TEXT("sofa"), FVector3d(2.0, .85, .85) } };
		auto build = [&](const FString& name, const FWall& S, double a)
		{
			if (name == TEXT("wardrobe")) { box(S, a, 0, 2.1, 0, .6, 1.2, W); box(S, a, .1, 2.0, .6, .61, .02, Mul3(W, .6)); for (const double u : { -.1, .1 }) box(S, a + u, 1.0, 1.15, .6, .64, .04, Ochre); box(S, a, 2.1, 2.18, -.02, .64, 1.26, Mul3(W, .8)); }
			else if (name == TEXT("bookcase"))
			{
				box(S, a, 0, 2.0, 0, .05, 1.3, W); for (const double u : { -.63, .63 }) box(S, a + u, 0, 2.0, 0, .34, .04, W);
				for (int32 k = 0; k < 5; k++)
				{
					box(S, a, .05 + k * .46, .09 + k * .46, 0, .34, 1.22, W);
					if (k < 4) { int32 b = 0; for (double u = -.56; u < .52; b++) { const double bw = .05 + .05 * h(a, k, b), bh = .24 + .12 * h(b, k, a); box(S, a + u + bw / 2, .09 + k * .46, .09 + k * .46 + bh, .04, .28, bw, Pick(RoomGoods, h(a, b, k))); u += bw + .01; } }
				}
			}
			else if (name == TEXT("dresser")) { box(S, a, 0, .95, 0, .5, 1.0, W); for (int32 k = 0; k < 3; k++) box(S, a, .12 + k * .28, .14 + k * .28, .5, .51, .92, Mul3(W, .6)); box(S, a, 1.3, 2.0, 0, .04, .7, Mul3(W, .7)); box(S, a, 1.36, 1.94, .04, .05, .58, FVector3f(.55f, .62f, .66f)); }
			else if (name == TEXT("fireplace")) { box(S, a, 0, 1.15, 0, .4, 1.8, Stone); box(S, a, .1, .8, .4, .41, .9, FVector3f(.08f, .06f, .05f)); box(S, a, .12, .3, .2, .38, .5, FVector3f(1, .5f, .2f), true); box(S, a, 1.15, 1.25, -.02, .5, 2.0, StoneDark); box(S, a, 1.6, 2.2, 0, .04, .9, Mul3(W, .7)); box(S, a, 1.66, 2.14, .04, .05, .78, Pick(RoomGoods, h(a, TEXT("f")))); }
			else if (name == TEXT("sideboard")) { box(S, a, 0, .85, 0, .45, 1.6, W); for (const double u : { -.4, .4 }) box(S, a + u, .1, .75, .45, .46, .7, Mul3(W, .7)); box(S, a - .4, .85, 1.15, .15, .3, .16, Pick(TArray<FVector3f>{ Teal, Ochre, Terracotta }, h(a, TEXT("v")))); box(S, a + .3, .85, .9, .1, .35, .4, FVector3f(.86f, .84f, .78f)); }
			else if (name == TEXT("plant")) { box(S, a, 0, .45, .1, .5, .4, Terracotta); for (int32 k = 0; k < 5; k++) box(S, a + (h(a, k) - .5) * .3, .45 + k * .16, .75 + k * .16, .08 + h(k, a) * .2, .32 + h(k, a) * .2, .32, Pick(TArray<FVector3f>{ FVector3f(.26f, .38f, .2f), FVector3f(.36f, .46f, .24f) }, h(a, k, TEXT("p")))); }
			else if (name == TEXT("bottles")) { box(S, a, 0, 1.9, 0, .04, 1.6, W); for (int32 k = 0; k < 4; k++) { box(S, a, .3 + k * .42, .33 + k * .42, 0, .3, 1.6, W); for (int32 b = 0; b < 9; b++) box(S, a - .7 + b * .17, .33 + k * .42, .33 + k * .42 + .24 + .06 * h(a, k, b), .1, .18, .06, Pick(TArray<FVector3f>{ FVector3f(.16f, .3f, .18f), FVector3f(.38f, .2f, .1f), FVector3f(.6f, .52f, .3f), FVector3f(.5f, .14f, .12f) }, h(b, k, a))); } }
			else if (name == TEXT("pastry")) { box(S, a, 0, .8, 0, .6, 1.4, W); box(S, a, .8, 1.05, .05, .55, 1.3, FVector3f(.95f, .85f, .62f), true); for (int32 b = 0; b < 5; b++) box(S, a - .5 + b * .25, .82, .9, .15, .45, .16, Pick(TArray<FVector3f>{ Ochre, FVector3f(.55f, .3f, .18f), Cream }, h(a, b))); }
			else if (name == TEXT("crates")) { for (int32 k = 0; k < 2; k++) { box(S, a - .3 + k * .6, 0, .5, .05, .7, .56, W); box(S, a - .3 + k * .6, .5, .62, .1, .65, .46, Pick(RoomGoods, h(a, k))); } box(S, a, .62, 1.05, .1, .6, .55, Mul3(W, .85)); }
			else if (name == TEXT("workbench")) { box(S, a, .82, .9, 0, .7, 2.2, W); for (const double u : { -1.0, 1.0 }) box(S, a + u * 1.0, 0, .82, .05, .65, .08, W); box(S, a, .25, .3, .05, .65, 2.0, W); box(S, a + .8, .9, 1.05, .45, .65, .2, Metal); box(S, a, 1.2, 2.2, 0, .03, 2.0, FVector3f(.3f, .22f, .16f)); for (int32 k = 0; k < 7; k++) box(S, a - .8 + k * .27, 1.4 + .3 * h(a, k), 1.75 + .3 * h(a, k), .03, .06, .05, Metal); }
			else if (name == TEXT("machine")) { box(S, a, 0, .9, .05, .85, 1.3, FVector3f(.3f, .34f, .32f)); box(S, a - .3, .9, 1.4, .2, .7, .5, FVector3f(.3f, .34f, .32f)); box(S, a + .35, .95, 1.1, .3, .6, .5, Metal); box(S, a + .55, 1.0, 1.35, .2, .3, .06, Ochre); }
			else if (name == TEXT("barrels")) { for (const double u : { -.32, .32 }) { box(S, a + u, 0, .95, .05, .62, .56, W); for (const double y : { .2, .75 }) box(S, a + u, y, y + .05, .04, .63, .58, Metal); } }
			else if (name == TEXT("filing")) { for (const double u : { -.3, .3 }) { box(S, a + u, 0, 1.35, 0, .6, .56, FVector3f(.42f, .44f, .44f)); for (int32 k = 0; k < 4; k++) box(S, a + u, .15 + k * .32, .18 + k * .32, .6, .61, .3, Metal); } }
			else if (name == TEXT("medcab")) { box(S, a, 0, 1.9, 0, .45, 1.0, FVector3f(.86f, .86f, .82f)); box(S, a, .9, 1.8, .45, .46, .86, FVector3f(.55f, .62f, .66f)); box(S, a, 1.45, 1.75, .46, .47, .08, Teal); box(S, a, 1.56, 1.64, .46, .47, .3, Teal); }
			else if (name == TEXT("sink")) { box(S, a, 0, .85, 0, .55, 1.0, FVector3f(.86f, .86f, .82f)); box(S, a, .85, .9, .05, .5, .6, Metal); box(S, a, .9, 1.15, .05, .1, .05, Metal); box(S, a, 1.2, 1.9, 0, .03, .7, FVector3f(.55f, .62f, .66f)); }
			else if (name == TEXT("bench")) { box(S, a, .42, .48, 0, .45, 1.8, R.Chair.Get(W)); box(S, a, .48, .9, 0, .06, 1.8, R.Chair.Get(W)); for (const double u : { -.8, .8 }) box(S, a + u, 0, .42, .05, .42, .06, Metal); }
			else if (name == TEXT("lockers")) { box(S, a, 0, 1.85, 0, .5, 1.4, FVector3f(.32f, .4f, .4f)); for (int32 k = 1; k < 4; k++) box(S, a - .7 + k * .35, .05, 1.8, .5, .51, .02, FVector3f(.2f, .26f, .26f)); for (int32 k = 0; k < 4; k++) box(S, a - .52 + k * .35, 1.5, 1.6, .5, .52, .18, FVector3f(.2f, .26f, .26f)); }
			else if (name == TEXT("sofa")) { const FVector3f fab = D.Fab; box(S, a, 0, .42, .05, .85, 2.0, fab); box(S, a, .42, .85, .05, .3, 2.0, fab); for (const double u : { -.92, .92 }) box(S, a + u, .42, .62, .05, .85, .16, fab); for (const double u : { -.5, .5 }) box(S, a + u, .42, .52, .3, .8, .9, Mul3(fab, 1.12)); }
		};
		auto picture = [&](const FWall& S, double a, int32 k)   // something on the wall: a painting, the menu, price boards, a notice, tools
		{
			const FString& kind = R.Pictures; const double y0 = 1.45;
			if (kind == TEXT("menu") && !menuDone) { menuDone = true; FBox(Out, S, a - .6, a + .6, y0, y0 + .85, 0, .04, FVector3f(.1f, .1f, .1f)); for (int32 q = 0; q < 5; q++) FBox(Out, S, a - .45, a - .45 + .5 + .35 * h(a, q), y0 + .65 - q * .13, y0 + .69 - q * .13, .04, .05, FVector3f(.85f, .82f, .74f)); return; }
			if (kind == TEXT("tools")) { FBox(Out, S, a - .5, a + .5, y0, y0 + .8, 0, .03, FVector3f(.3f, .22f, .16f)); for (int32 q = 0; q < 4; q++) FBox(Out, S, a - .38 + q * .25, a - .33 + q * .25, y0 + .15, y0 + .55 + .1 * h(a, q), .03, .07, Metal); return; }
			const double w = kind == TEXT("notice") || kind == TEXT("price") ? .55 : .6 + .4 * h(a, k, TEXT("w")), ht = kind == TEXT("price") ? .4 : .5 + .3 * h(k, a, TEXT("h"));
			FBox(Out, S, a - w / 2, a + w / 2, y0, y0 + ht, 0, .04, kind == TEXT("art") ? FVector3f(.3f, .2f, .12f) : FVector3f(.2f, .16f, .12f));
			FBox(Out, S, a - w / 2 + .05, a + w / 2 - .05, y0 + .05, y0 + ht - .05, .04, .045, kind == TEXT("notice") ? FVector3f(.86f, .84f, .78f) : kind == TEXT("price") ? FVector3f(.12f, .12f, .12f) : Pick(RoomGoods, h(a, k, TEXT("c"))));
		};

		Layout(D);
		// along each wall: tall pieces where there is no window and it is not the door wall, low ones elsewhere; pictures above the low ones
		// winAt(): the building's windows from inside (l.inWin) are not in the Unreal data, so none is taken to be there
		auto winAt = [](const TCHAR*, double, double) { return false; };
		int32 placed = 0;
		for (const TCHAR* s : { TEXT("n"), TEXT("s"), TEXT("w"), TEXT("e") })
		{
			const FWall& S = walls.FindChecked(s);
			const double A0 = S.ns ? rx0 : rz0, A1 = S.ns ? rx1 : rz1, doorA = S.ns ? dx : dz;
			const bool onDoor = D.Side == s;
			int32 k = 0;
			for (double a = A0 + 1.3; a < A1 - 1.0 && placed < 60; a += 2.7, k++)
			{
				if (onDoor && FMath::Abs(a - doorA) < dw / 2 + 1.6) continue;
				const bool tall = !onDoor && !winAt(s, a, 1.5) && R.Tall.Num() && h(s, k) < .55;
				const TArray<FString>& list = tall ? R.Tall : R.Low;
				if (!list.Num() || h(s, k, TEXT("skip")) < .18) continue;
				const FString& name = Pick(list, h(s, k, TEXT("pick")));
				const FVector3d& Sz = PSize.FindChecked(name);
				const double w = Sz.X, dep = Sz.Y, ht = Sz.Z;
				const FVector3d c = SPAt(S, a, 0, dep / 2 + .05);
				if (a - w / 2 < A0 + .2 || a + w / 2 > A1 - .2 || !D.Clear(c.X, c.Z, FMath::Max(w, dep) / 2)) continue;
				build(name, S, a); if (name != TEXT("plant")) solidOn(S, a, w, dep, ht);
				D.OccPush(c.X, c.Z, FMath::Max(w, dep) / 2 + .3); placed++;
				if (ht < 1.3 && name != TEXT("fireplace") && !winAt(s, a, 1.0) && !R.Pictures.IsEmpty()) picture(S, a, k);
			}
		}
		// open floor: what this kind of building has standing in the room
		const double cx = (rx0 + rx1) / 2, cz = (rz0 + rz1) / 2;
		TArray<FVector3d> spots;
		for (double gx = rx0 + 2; gx < rx1 - 1.5; gx += 2.5) for (double gz = rz0 + 2; gz < rz1 - 1.5; gz += 2.5) spots.Add(FVector3d(gx, gz, FMath::Sqrt(FMath::Square(gx - cx) + FMath::Square(gz - cz)) + 3 * h(gx, gz)));
		spots.StableSort([](const FVector3d& p, const FVector3d& q) { return p.Z < q.Z; });
		static const TMap<FString, double> want = { { TEXT("dining"), 1 }, { TEXT("rug"), 1 }, { TEXT("produce"), 4 }, { TEXT("kiosk"), 6 }, { TEXT("crates"), 2 }, { TEXT("machine"), 2 }, { TEXT("reception"), 1 }, { TEXT("screens"), 2 }, { TEXT("beds"), 6 }, { TEXT("benches"), 3 }, { TEXT("long"), 1 }, { TEXT("plant"), 2 } };
		static const TMap<FString, double> rOf = { { TEXT("dining"), 1.9 }, { TEXT("produce"), 1.4 }, { TEXT("kiosk"), 2.2 }, { TEXT("crates"), .9 }, { TEXT("machine"), 1.0 }, { TEXT("reception"), 1.6 }, { TEXT("screens"), 1.2 }, { TEXT("beds"), 1.4 }, { TEXT("benches"), 1.6 }, { TEXT("long"), 2.4 }, { TEXT("plant"), .5 } };
		for (const FString& m : R.Mid)
		{
			const double* Wn = want.Find(m);
			int32 n = (int32)JsRound((Wn ? *Wn : 1) * FMath::Max(1.0, (rx1 - rx0) * (rz1 - rz0) / 160));   // bigger rooms hold more
			for (const FVector3d& Gs : spots)
			{
				if (n <= 0) break;
				const double px = Gs.X, pz = Gs.Y;
				if (m == TEXT("rug"))
				{
					if (!freeAt(px, pz, .3)) continue;
					Flat(Out, px - 1.4, pz - 1.0, px + 1.4, pz + 1.0, .004, Mul3(D.Accent, .8)); Flat(Out, px - 1.25, pz - .85, px + 1.25, pz + .85, .006, Mul3(Pick(RoomGoods, h(px, pz)), .9));
					n--; continue;
				}
				const double* Rp = rOf.Find(m);
				if (!Rp) continue;
				const double r = *Rp;
				if (!freeAt(px, pz, r)) continue;
				if (m == TEXT("dining")) { D.Table(px, pz, 1.1, .76, false); D.Hard(px - .55, 0, pz - .55, px + .55, .76, pz + .55); for (const FVector2D& uv : { FVector2D(0, -1), FVector2D(0, 1), FVector2D(-1, 0), FVector2D(1, 0) }) D.Chair(px + uv.X * .8, pz + uv.Y * .8, YawOf(-uv.X, -uv.Y), D.Fab); }
				else if (m == TEXT("produce")) { D.B(px, 0, pz, 2.2, .7, 1.0, Wood); for (int32 k = 0; k < 6; k++) { const double u = -.75 + (k % 3) * .75, v = k < 3 ? -.25 : .25; D.B(px + u, .7, pz + v, .66, .12, .44, Wood); D.B(px + u, .82, pz + v, .58, .1, .38, Pick(RoomGoods, h(px, k))); } D.Hard(px - 1.1, 0, pz - .5, px + 1.1, .9, pz + .5); }
				else if (m == TEXT("kiosk")) { for (const FVector2D& uv : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) }) D.B(px + uv.X * .95, 0, pz + uv.Y * .95, .08, 2.3, .08, Metal); D.B(px, 2.3, pz, 2.2, .12, 2.2, Pick(TArray<FVector3f>{ Teal, Terracotta, Ochre }, h(px, pz))); D.B(px, 0, pz, 1.7, 1.0, 1.7, Wood); D.B(px, 1.0, pz, 1.8, .05, 1.8, FVector3f(.62f, .58f, .52f)); for (int32 k = 0; k < 4; k++) D.B(px - .5 + k * .33, 1.05, pz, .22, .2, .3, Pick(RoomGoods, h(px, k))); D.Hard(px - .9, 0, pz - .9, px + .9, 1.05, pz + .9); }
				else if (m == TEXT("crates")) { for (int32 k = 0; k < 3; k++) D.B(px + (k - 1) * .62, 0, pz, .58, .55 + (k == 1 ? .55 : 0), .58, Mul3(Wood, .85 + .2 * h(px, k))); D.Hard(px - .9, 0, pz - .3, px + .9, 1.1, pz + .3); }
				else if (m == TEXT("machine")) { D.B(px, 0, pz, 1.4, .95, .8, FVector3f(.3f, .34f, .32f)); D.B(px - .3, .95, pz, .5, .55, .6, FVector3f(.3f, .34f, .32f)); D.B(px + .4, .95, pz, .5, .2, .5, Metal); D.Hard(px - .7, 0, pz - .4, px + .7, 1.5, pz + .4); }
				else if (m == TEXT("reception")) { D.B(px, 0, pz, 2.4, 1.05, .7, R.Counter.Get(Wood)); D.B(px, 1.05, pz, 2.5, .05, .8, R.Top.Get(Teal)); D.B(px + .7, 1.1, pz, .14, .35, .14, Metal); D.B(px + .7, 1.42, pz, .26, .1, .26, R.Lamp.Get(Cream)); D.B(px - .5, 1.1, pz, .4, .03, .3, FVector3f(.86f, .84f, .78f)); D.Hard(px - 1.2, 0, pz - .35, px + 1.2, 1.1, pz + .35); }
				else if (m == TEXT("screens")) { for (int32 k = 0; k < 3; k++) { const double yaw = (k - 1) * .5; D.B(px + FMath::Cos(yaw) * (k - 1) * .62, 0, pz + FMath::Sin(yaw) * (k - 1) * .62, .62, 1.75, .04, FVector3f(.82f, .8f, .72f), -yaw); } D.B(px, 0, pz + .9, .9, .55, 1.9, FVector3f(.86f, .86f, .82f)); D.B(px, .55, pz + .9, .86, .1, 1.8, FVector3f(.92f, .92f, .9f)); D.Hard(px - .45, 0, pz - .05, px + .45, .6, pz + 1.85); }
				else if (m == TEXT("beds")) { D.B(px, 0, pz, .95, .55, 2.0, FVector3f(.86f, .86f, .82f)); D.B(px, .55, pz, .9, .12, 1.9, FVector3f(.92f, .92f, .9f)); D.B(px, .67, pz + .65, .6, .1, .3, FVector3f(.95f, .95f, .93f)); D.B(px, 0, pz - 1.05, .95, .95, .06, Metal); D.Hard(px - .48, 0, pz - 1, px + .48, .67, pz + 1); }
				else if (m == TEXT("benches") || m == TEXT("long"))
				{
					const bool bLong = m == TEXT("long"); const double L2 = bLong ? 3.4 : 2.4;
					D.B(px, .42, pz, L2, .06, .45, Wood); D.B(px, .48, pz + .2, L2, .4, .06, Wood); for (const double u : { -1.0, 1.0 }) D.B(px + u * (L2 / 2 - .1), 0, pz, .08, .42, .4, Wood);
					if (bLong) { D.B(px, .74, pz - 1.1, L2, .05, 1.0, Wood); for (const double u : { -1.0, 1.0 }) D.B(px + u * (L2 / 2 - .2), 0, pz - 1.1, .1, .74, .9, Wood); D.Hard(px - L2 / 2, 0, pz - 1.6, px + L2 / 2, .78, pz - .6); }
					D.Hard(px - L2 / 2, 0, pz - .25, px + L2 / 2, .5, pz + .25);
				}
				else if (m == TEXT("plant")) { D.B(px, 0, pz, .45, .45, .45, Terracotta); for (int32 k = 0; k < 4; k++) D.B(px + (h(px, k) - .5) * .3, .45 + k * .2, pz + (h(k, pz) - .5) * .3, .5 - k * .07, .32, .5 - k * .07, FVector3f(.28f, .4f, .21f)); }
				D.OccPush(px, pz, r + .3); n--;
			}
		}
		// lamps hanging from the beams
		const int32 nx = FMath::Max(1, (int32)JsRound((rx1 - rx0) / 4.4)), nz = FMath::Max(1, (int32)JsRound((rz1 - rz0) / 4.4));
		if (!R.bDim) for (int32 i = 0; i < nx; i++) for (int32 j = 0; j < nz; j++)   // (a cinema stays dark)
		{
			const double lx = rx0 + (i + .5) * (rx1 - rx0) / nx, lz = rz0 + (j + .5) * (rz1 - rz0) / nz, top = D.CeilY() - .02 - D.FloorY;
			if (D.SG)   // no lamp over the stairwell
			{
				const FBrixStairGeo& SG = *D.SG;
				const FVector2D pa = SG.P(SG.U0 - .3f, FMath::Min(SG.V0, SG.V1) - .3f), pb = SG.P(SG.U1 + .3f, FMath::Max(SG.V0, SG.V1) + .3f);
				if (lx > FMath::Min(pa.X, pb.X) && lx < FMath::Max(pa.X, pb.X) && lz > FMath::Min(pa.Y, pb.Y) && lz < FMath::Max(pa.Y, pb.Y)) continue;
			}
			AABox(Out, lx - .012, top - .55, lz - .012, lx + .012, top, lz + .012, Charcoal);
			Put(Out, lx, top - .78, lz, .42, .24, .42, PI / 4, R.Lamp.Get(Cream), false); AABox(Out, lx - .09, top - .86, lz - .09, lx + .09, top - .76, lz + .09, FVector3f(1, .78f, .5f), true);
		}
	}
}

bool BrixFurnishFloor(const FBrixInsides& Ins, const FBrixLot& L, const FBrixInside& In, int32 Floor, FBrixFurnished& Out, bool bDress)
{
	Out = FBrixFurnished();
	TArray<FVector3d> occ;   // play.js occ: [x, z, r] kept clear, for room() to place round
	if (!BrixIsHollow(L, &In) || L.House.IsSet() || !In.Interior.IsEmpty()) return false;   // planned homes: BrixHomes
	if (L.Fitout.IsValid() && L.AngleDeg == 0.f && Floor == L.Fitout->Floor)
	{
		// a room fitted out by its design (World/rules/fitouts.json): in the door's frame, as play.js layout()'s wp() has it
		const FBrixFitout& Fo = *L.Fitout;
		const FVector2D Dr = L.Door;
		const FVector2D o = FMath::Abs(Dr.Y - L.Min.Y) < .01 ? FVector2D(0, -1) : FMath::Abs(Dr.Y - L.Max.Y) < .01 ? FVector2D(0, 1) : FMath::Abs(Dr.X - L.Min.X) < .01 ? FVector2D(-1, 0) : FVector2D(1, 0);
		const FVector2D AlongV(-o.Y, o.X), InV(-o.X, -o.Y);
		auto At = [&](double a, double i) { return Dr + AlongV * a + InV * i; };
		const double FR = FMath::Atan2(-o.X, -o.Y);   // a box turned as the layout turns its boxes
		for (const FBrixFitoutPiece& Pc : Fo.Pieces)
		{
			FBrixFurniturePiece P;
			P.Kind = Pc.Piece; P.Line = Fo.Line; P.Floor = Floor; P.At = At(Pc.Along, Pc.In); P.Size = Pc.bHung ? FVector3f::ZeroVector : Pc.Size;   // a hung piece: no footprint in the way
			P.B = Pc.Faces == TEXT("back") ? -InV : Pc.Faces == TEXT("+along") ? -AlongV : Pc.Faces == TEXT("-along") ? AlongV : InV;   // B: toward its back
			P.A = FVector2D(P.B.Y, -P.B.X);                                                                                           // as BrixHomes turns A to B
			Out.Pieces.Add(P);
		}
		for (const FBrixFitoutGlow& G : Fo.Glow)
		{
			const FVector2D p = At(G.Along, G.In);
			Put(Out, p.X, G.Y0, p.Y, G.Size.X, G.Size.Y, G.Size.Z, FR, G.Colour, true);
		}
		for (const FBrixFitoutWall& Wl : Fo.Walls)   // the partitions, each cut at its doorways
		{
			const FVector2D P0 = At(Wl.From.X, Wl.From.Y), P1 = At(Wl.To.X, Wl.To.Y);
			const double Len = FVector2D::Distance(P0, P1);
			if (Len < .05) continue;
			const FVector2D U = (P1 - P0) / Len;
			auto Seg = [&](double s0, double s1, bool bDoor)
			{
				if (s1 - s0 < .02) return;
				FBrixWallSeg S;
				S.Floor = Floor; S.bDoorway = bDoor; S.bPartition = true;
				S.R.Centre = P0 + U * ((s0 + s1) / 2); S.R.Along = U; S.R.Length = (float)(s1 - s0); S.R.Width = .12f;
				Out.Walls.Add(S);
			};
			double s = -.06;   // the ends run on by half a wall, so corners close
			TArray<FVector2D> Ds = Wl.Doors;
			Ds.Sort([](const FVector2D& A, const FVector2D& B) { return A.X < B.X; });
			for (const FVector2D& Dd : Ds) { Seg(s, Dd.X, false); Seg(Dd.X, Dd.X + Dd.Y, true); s = Dd.X + Dd.Y; }
			Seg(s, Len + .06, false);
		}
		const double Top = 3.0;   // the ground floor's ceiling over its floor (Storeys: ceilY 3.2, floorY 0.2)
		for (const FVector2D& Lp : Fo.Lamps)   // a pendant as the layout's lamp(): its rod, its shade, the lit glass under it
		{
			const FVector2D p = At(Lp.X, Lp.Y);
			const double drop = Fo.LampDrop;
			AABox(Out, p.X - .01, Top - drop, p.Y - .01, p.X + .01, Top, p.Y + .01, FVector3f(.12f, .11f, .1f));
			Put(Out, p.X, Top - drop - .2, p.Y, .32, .2, .32, 0, FVector3f(.42f, .36f, .22f), false);
			AABox(Out, p.X - .07, Top - drop - .27, p.Y - .07, p.X + .07, Top - drop - .19, p.Y + .07, FVector3f(1, .78f, .5f), true);
		}
		return Out.Pieces.Num() > 0 || Out.Boxes.Num() > 0 || Out.Walls.Num() > 0;
	}
	// storeys.js proxy(): what floor f is for, and its own places (lot.levels)
	FString Kind = L.Kind, Sp = In.Special;
	if (Floor > 0)
	{
		static const TMap<FString, TPair<FString, FString>> UseAs = {
			{ TEXT("bedrooms"), { TEXT("home"), TEXT("") } }, { TEXT("flat"), { TEXT("home"), TEXT("flats") } }, { TEXT("store"), { TEXT("shop"), TEXT("") } },
			{ TEXT("rooms"), { TEXT("home"), TEXT("") } }, { TEXT("office"), { TEXT("work"), TEXT("office") } }, { TEXT("wards"), { TEXT("health"), TEXT("") } },
			{ TEXT("stacks"), { TEXT("library"), TEXT("") } }, { TEXT("gallery"), { TEXT("museum"), TEXT("") } }, { TEXT("plant"), { TEXT("utility"), TEXT("depot") } } };
		const TPair<FString, FString>* As = In.Uses.IsValidIndex(Floor) ? UseAs.Find(In.Uses[Floor]) : nullptr;
		if (!As) As = UseAs.Find(TEXT("rooms"));
		Kind = As->Key; Sp = As->Value;
	}
	auto Places = [&](const TCHAR* K)
	{
		TArray<FVector2D> P;
		const TArray<FVector2D>* All = In.Objects.Find(K);
		if (!All) return P;
		const TArray<int32>* Lv = In.Levels.Find(K);
		TSet<int32> Planned;   // homes.js others(): a planned flat's places are furnished from its plan (BrixFlatHomeOf)
		for (const FBrixFlat& Q : In.Flats)
			if (!Q.Interior.IsEmpty()) for (const int32 i : FCString::Strcmp(K, TEXT("bed")) == 0 ? Q.Beds : FCString::Strcmp(K, TEXT("room")) == 0 ? Q.Rooms : TArray<int32>()) Planned.Add(i);
		for (int32 i = 0; i < All->Num(); ++i) if (!Planned.Contains(i) && (Lv ? (Lv->IsValidIndex(i) && (*Lv)[i] == Floor) : Floor == 0)) P.Add((*All)[i]);
		return P;
	};
	// the door's side and the ways to and from it (play.js building(): the edge of the rect the door lies on)
	const FVector2D D0 = L.Door;
	const TCHAR* Side = FMath::Abs(D0.Y - L.Min.Y) < .01 ? TEXT("n") : FMath::Abs(D0.Y - L.Max.Y) < .01 ? TEXT("s") : FMath::Abs(D0.X - L.Min.X) < .01 ? TEXT("w") : TEXT("e");
	const FVector2D d = FCString::Strcmp(Side, TEXT("n")) == 0 ? FVector2D(0, -1) : FCString::Strcmp(Side, TEXT("s")) == 0 ? FVector2D(0, 1) : FCString::Strcmp(Side, TEXT("w")) == 0 ? FVector2D(-1, 0) : FVector2D(1, 0);
	const bool ns = d.X == 0;
	const double toDoor = YawOf(d.X, d.Y), fromDoor = YawOf(-d.X, -d.Y);
	// storeys.js keepOut(): the stairwell and 0.6 m round it, and on the ground floor the way from the door to the stair's foot
	TOptional<FBox2D> Keep;
	TArray<TPair<FVector2D, FVector2D>> Way;
	if (In.Stair)
	{
		const FBrixStairGeo G = BrixStairOf(*In.Stair, Ins.Rules);
		const FVector2D A = G.P(G.U0, G.V0), B = G.P(G.U1, G.V1);
		Keep = FBox2D(FVector2D(FMath::Min(A.X, B.X), FMath::Min(A.Y, B.Y)), FVector2D(FMath::Max(A.X, B.X), FMath::Max(A.Y, B.Y))).ExpandBy(0.6);
		if (Floor == 0)
		{
			const FVector2D P0 = D0 - d * .4, P1 = D0 - d * 1.6;
			Way.Add({ P0, P1 }); Way.Add({ P1, G.FootAt(0) });
		}
	}
	auto Kept = [&](double x0, double z0, double x1, double z1)
	{
		if (Keep && x1 > Keep->Min.X && x0 < Keep->Max.X && z1 > Keep->Min.Y && z0 < Keep->Max.Y) return true;
		const FVector2D Cn((x0 + x1) / 2, (z0 + z1) / 2);
		const double R = FMath::Sqrt(FMath::Square(x1 - x0) + FMath::Square(z1 - z0)) / 2 + .55;
		for (const TPair<FVector2D, FVector2D>& S : Way)
		{
			const FVector2D V = S.Value - S.Key;
			const double L2 = FMath::Max(V.SizeSquared(), 1e-6), T = FMath::Clamp(FVector2D::DotProduct(Cn - S.Key, V) / L2, 0.0, 1.0);
			if (FVector2D::Distance(Cn, S.Key + V * T) < R) return true;
		}
		return false;
	};
	auto B = [&](double cx, double y0, double cz, double sx, double sy, double sz, const FVector3f& c, double yaw = 0, bool bGlow = false)
	{
		const double cy = FMath::Abs(FMath::Cos(yaw)), sn = FMath::Abs(FMath::Sin(yaw)), hx = (cy * sx + sn * sz) / 2, hz = (sn * sx + cy * sz) / 2;
		if (y0 < 2 && Kept(cx - hx, cz - hz, cx + hx, cz + hz)) return;
		Out.Boxes.Add({ FVector2D(cx, cz), (float)y0, FVector3f((float)sx, (float)sy, (float)sz), (float)yaw, c, bGlow });
	};
	auto Hard = [&](double x0, double y0, double z0, double x1, double y1, double z1)
	{
		if (y0 < 2 && Kept(FMath::Min(x0, x1), FMath::Min(z0, z1), FMath::Max(x0, x1), FMath::Max(z0, z1))) return;
		Out.Solids.Add({ FVector2D(FMath::Min(x0, x1), FMath::Min(z0, z1)), FVector2D(FMath::Max(x0, x1), FMath::Max(z0, z1)), (float)y0, (float)y1 });
	};
	auto Chair = [&](double x, double z, double yaw, const FVector3f& c, bool arm)   // a chair facing along yaw; the back behind the sitter
	{
		const double fx2 = FMath::Sin(yaw), fz2 = -FMath::Cos(yaw), w = arm ? .72 : .44;
		B(x, .4, z, w, .07, w, c, -yaw);
		for (const FVector2D uv : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) })
		{
			const double px = FMath::Cos(yaw) * uv.X * (w / 2 - .04) + fx2 * uv.Y * (w / 2 - .04), pz = FMath::Sin(yaw) * uv.X * (w / 2 - .04) + fz2 * uv.Y * (w / 2 - .04);
			B(x + px, 0, z + pz, .045, .4, .045, Wood, -yaw);
		}
		B(x - fx2 * (w / 2 - .03), .47, z - fz2 * (w / 2 - .03), w, arm ? .5 : .48, .06, c, -yaw);
		if (arm) for (const double u : { -1.0, 1.0 }) B(x + FMath::Cos(yaw) * u * (w / 2 - .05), .47, z + FMath::Sin(yaw) * u * (w / 2 - .05), .1, .2, w, c, -yaw);
	};
	auto Table = [&](double x, double z, double size, double h, bool round)   // a round top on a pedestal, or a square one on four legs
	{
		B(x, h - .04, z, size, .045, size, Wood, 0);
		if (round) { B(x, h - .04, z, size, .045, size, Wood, PI / 4); B(x, 0, z, .09, h - .04, .09, Metal); B(x, 0, z, .45, .04, .45, Metal, PI / 4); }
		else for (const FVector2D uv : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) }) B(x + uv.X * (size / 2 - .06), 0, z + uv.Y * (size / 2 - .06), .06, h - .04, .06, Wood);
	};
	auto Spot = [&](double x, double z, int32 Pose, double Yaw, double Up)
	{
		FBrixSpot S;
		S.At = FVector2D(x, z); S.Floor = Floor; S.Up = (float)Up; S.bLie = Pose == 3; S.Facing = Pose == 3 ? FVector2D(0, 1) : Facing(Yaw);   // lying head to +z, the headboard
		Out.Spots.Add(S);
	};
	const FStyle& R = StyleOf(Sp, Kind);
	const FString Pid = Floor > 0 ? L.Id + TEXT("/") + FString::FromInt(Floor) : L.Id;   // storeys.js proxy(): the floor's own id above the ground
	const FVector3f Fab = Pick(R.Fabric, BrixSliceHash({ Pid, TEXT("fab") }));
	const int32 N0 = Out.Boxes.Num();

	for (const FVector2D& P : Places(TEXT("bed")))   // a framed bed with a headboard, blanket and pillow, a nightstand and its lamp
	{
		const double x = P.X, z = P.Y;
		const FVector3f Bl = Pick(TArray<FVector3f>{ Cream, Blue, Olive, Plum, Terracotta }, BrixSliceHash({ Pid, JsNum(x), JsNum(z) }));
		B(x, 0, z, 1.0, .32, 2.0, Wood); B(x, .32, z, .94, .2, 1.92, FVector3f(.78f, .74f, .66f)); B(x, .5, z - .3, .98, .1, 1.36, Bl); B(x, .52, z + .68, .62, .14, .34, FVector3f(.85f, .82f, .76f));
		B(x, 0, z + 1.02, 1.06, .98, .08, Wood); B(x, 0, z - 1.02, 1.06, .5, .07, Wood);
		if (BrixSliceHash({ Pid, JsNum(x), JsNum(z), TEXT("ns") }) < .7)
		{
			const double sx = x + (BrixSliceHash({ Pid, JsNum(z), TEXT("s") }) < .5 ? -.82 : .82);
			B(sx, 0, z + .75, .42, .5, .38, Wood); B(sx, .5, z + .75, .12, .22, .12, FVector3f(.9f, .8f, .6f), 0, true);
		}
		Hard(x - .5, 0, z - 1, x + .5, .5, z + 1); occ.Add(FVector3d(x, z, 1.5));
		Spot(x, z, 3, 0, .62);
	}
	{
		const TArray<FVector2D> Rooms = Places(TEXT("room"));
		for (int32 k = 0; k < Rooms.Num(); ++k)
		{
			const double Yaw = FMath::FloorToDouble(BrixSliceHash({ Pid, JsNum(k), TEXT("r") }) * 4) * PI / 2;
			Chair(Rooms[k].X, Rooms[k].Y, Yaw, Fab, Kind == TEXT("home")); occ.Add(FVector3d(Rooms[k].X, Rooms[k].Y, 1.0));
			Spot(Rooms[k].X, Rooms[k].Y, 2, Yaw, .45);
		}
	}
	for (const FVector2D& P : Places(TEXT("chair"))) { Chair(P.X, P.Y, toDoor, R.Chair.Get(Cream), false); occ.Add(FVector3d(P.X, P.Y, .9)); Spot(P.X, P.Y, 2, toDoor, .45); }
	{
		const TArray<FVector2D> Seats = Places(TEXT("seat"));
		for (int32 k = 0; k < Seats.Num(); ++k)
		{
			const double x = Seats[k].X, z = Seats[k].Y;
			if (Kind == TEXT("cinema") || Kind == TEXT("museum"))   // a cinema seat facing the screen at the back; a museum bench facing the exhibits
			{
				const FVector3f Red(.42f, .1f, .1f);
				if (Kind == TEXT("cinema")) { B(x, 0, z, .5, .42, .5, Charcoal, -fromDoor); B(x, .42, z, .5, .1, .48, Red, -fromDoor); B(x - FMath::Sin(fromDoor) * .24, .42, z + FMath::Cos(fromDoor) * .24, .52, .55, .1, Red, -fromDoor); }
				else { B(x, .4, z, 1.5, .07, .45, Wood, -fromDoor); for (const double u : { -.65, .65 }) B(x + FMath::Cos(fromDoor) * u, 0, z + FMath::Sin(fromDoor) * u, .07, .4, .4, Wood, -fromDoor); }
				occ.Add(FVector3d(x, z, Kind == TEXT("cinema") ? .45 : .9)); Spot(x, z, 2, fromDoor, .45);
				continue;
			}
			const double sgn = k % 2 ? -1 : 1;
			Chair(x, z, YawOf(sgn, 0), R.Chair.Get(Wood), false);
			Table(x + sgn * .7, z, .64, .74, Kind == TEXT("food"));
			if (Kind == TEXT("food") || Sp == TEXT("mall")) Chair(x + sgn * 1.4, z, YawOf(-sgn, 0), R.Chair.Get(Wood), false);   // a table for two
			Hard(x + sgn * .7 - .31, 0, z - .31, x + sgn * .7 + .31, .74, z + .31); occ.Add(FVector3d(x + sgn * .35, z, 1.2));
			Spot(x, z, 2, YawOf(sgn, 0), .45);
		}
	}
	for (const FVector2D& P : Places(TEXT("work")))   // a desk with a lamp and papers, or a counter with what that trade keeps on it
	{
		const double x = P.X, z = P.Y;
		const bool desk = Kind == TEXT("work") && Sp != TEXT("works");
		const double cx = x + d.X * .8, cz = z + d.Y * .8, w = desk ? 1.3 : 1.7, dep = desk ? .65 : .55, h = desk ? .76 : .98;
		const double sx = ns ? w : dep, sz = ns ? dep : w;
		if (desk)
		{
			B(cx, h - .04, cz, sx, .04, sz, Wood);
			B(cx + (ns ? w / 2 - .22 : 0), 0, cz + (ns ? 0 : w / 2 - .22), ns ? .4 : dep - .04, h - .04, ns ? dep - .04 : .4, Wood);
			B(cx - (ns ? w / 2 - .04 : 0), 0, cz - (ns ? 0 : w / 2 - .04), ns ? .05 : dep - .06, h - .04, ns ? dep - .06 : .05, Wood);
			B(cx - (ns ? .2 : 0), h, cz - (ns ? 0 : .2), .32, .02, .24, FVector3f(.88f, .86f, .8f), .2);
			B(cx + (ns ? .4 : .1), h, cz + (ns ? .1 : .4), .1, .38, .1, Metal); B(cx + (ns ? .4 : .1), h + .34, cz + (ns ? .1 : .4), .2, .08, .2, R.Lamp.Get(Teal));
			Chair(x, z, toDoor, R.Chair.Get(Wood), false);
		}
		else if (Kind != TEXT("food"))   // (a cafe's staff stand behind one long coffee bar, built in room())
		{
			const FVector3f Ctr = R.Counter.Get(Wood);
			B(cx, 0, cz, sx, h - .05, sz, Ctr); B(cx, h - .05, cz, sx + .06, .05, sz + .06, R.Top.Get(In.Accent));
			for (int32 q = 0; q < 3; ++q) { const double u = (q - 1) * w / 3; B(cx + (ns ? u : d.X * -.29), .12, cz + (ns ? d.Y * -.29 : u), ns ? w / 3 - .12 : .02, h - .3, ns ? .02 : w / 3 - .12, Ctr * .8f); }
			switch (R.Tool)   // play.js espresso, till, vise, ledger
			{
			case ETool::Espresso:
				B(cx - (ns ? .45 : 0), h, cz - (ns ? 0 : .45), .5, .42, .36, Metal); B(cx - (ns ? .45 : 0), h + .42, cz - (ns ? 0 : .45), .3, .08, .3, FVector3f(.6f, .58f, .55f));
				for (int32 k = 0; k < 4; ++k) B(cx + (ns ? .1 + k * .12 : 0), h, cz + (ns ? 0 : .1 + k * .12), .07, .08, .07, FVector3f(.86f, .84f, .8f));
				B(cx + (ns ? .62 : 0), h, cz + (ns ? 0 : .62), .3, .26, .3, FVector3f(.95f, .82f, .55f), 0, true);
				break;
			case ETool::Till:
				B(cx - (ns ? .5 : 0), h, cz - (ns ? 0 : .5), .36, .2, .3, Charcoal); B(cx - (ns ? .5 : 0), h + .2, cz - (ns ? 0 : .5), .26, .14, .04, FVector3f(.2f, .3f, .3f));
				B(cx + (ns ? .35 : 0), h, cz + (ns ? 0 : .35), .5, .14, .34, Ochre);
				break;
			case ETool::Vise:
				B(cx - (ns ? .5 : 0), h, cz - (ns ? 0 : .5), .22, .18, .2, Metal); B(cx - (ns ? .5 : 0), h + .18, cz - (ns ? 0 : .5), .04, .04, .34, Metal);
				B(cx + (ns ? .2 : 0), h, cz + (ns ? 0 : .2), .5, .03, .08, FVector3f(.5f, .32f, .18f)); B(cx + (ns ? .4 : 0), h, cz + (ns ? 0 : .4), .06, .05, .3, Metal);
				break;
			case ETool::Ledger:
				B(cx, h, cz, .3, .04, .22, FVector3f(.86f, .84f, .78f)); B(cx + (ns ? .5 : 0), h, cz + (ns ? 0 : .5), .1, .3, .1, Metal);
				break;
			default: break;
			}
		}
		Hard(cx - sx / 2, 0, cz - sz / 2, cx + sx / 2, h, cz + sz / 2); occ.Add(FVector3d(cx, cz, 1.2)); occ.Add(FVector3d(x, z, .8));
		Spot(x, z, desk ? 2 : 0, toDoor, desk ? .45 : 0);
	}
	for (const FVector2D& P : Places(TEXT("customer"))) { Spot(P.X, P.Y, 0, fromDoor, 0); occ.Add(FVector3d(P.X, P.Y, .7)); }
	// play.js: if (!l.flats) room(...): a building of flats keeps each flat's own beds and chairs (walled by storeys.js)
	if (bDress && In.Flats.Num() == 0)
	{
		FDress Dr;
		Dr.Out = &Out; Dr.Occ = &occ;
		Dr.BFn = [&B](double cx, double y0, double cz, double sx, double sy, double sz, const FVector3f& c, double yaw, bool bGlow) { B(cx, y0, cz, sx, sy, sz, c, yaw, bGlow); };
		Dr.HardFn = Hard; Dr.ChairFn = Chair; Dr.TableFn = Table;
		Dr.PlacesFn = [&Places](const TCHAR* K) { return Places(K); };
		Dr.Rs = &R; Dr.Kind = Kind; Dr.Sp = Sp; Dr.Side = Side; Dr.Accent = In.Accent; Dr.Fab = Fab;
		Dr.Id = Floor > 0 ? L.Id + TEXT("/") + FString::FromInt(Floor) : L.Id;   // proxy(): p.id = l.id + "/" + f above the ground
		Dr.Dir = d; Dr.RMin = L.Min; Dr.RMax = L.Max;
		Dr.Dw = FMath::RoundToDouble(In.DoorWidth * 1e4) / 1e4;   // l.dw, as the double the slice has (1.7, not 1.7f)
		Dr.Level = Floor; Dr.FloorY = Floor == 0 ? .2 : 3.0 * Floor + .4;   // Storeys.floorY
		if (In.Stair) Dr.SG = BrixStairOf(*In.Stair, Ins.Rules);
		Dr.Door = Floor > 0 && Dr.SG ? Dr.SG->FootAt(Floor) : D0;   // proxy(): above the ground the way in is the stair's foot
		Dr.bTower = In.bTower;
		const int32 NB = Out.Boxes.Num(), NS = Out.Solids.Num();
		Room(Dr);
		// Not in the slice: what the dressing would stand through the walls is cut back to their inner faces (the slice's works
		// racks run on through the back wall, i < iMax - 1.6 for a rack 2.2 long; above the ground a museum's great painting is
		// centred on the stair's foot, the floor's way in, and runs off the side). Square-on boxes only: the turned ones keep inside.
		const FBox2D In2(L.Min + FVector2D(.35, .35), L.Max - FVector2D(.35, .35));
		auto Cut = [](double& Lo, double& Hi, double A, double B2) { Lo = FMath::Max(Lo, A); Hi = FMath::Min(Hi, B2); return Hi > Lo; };
		for (int32 i = Out.Boxes.Num() - 1; i >= NB; --i)
		{
			FBrixFurnBox& X = Out.Boxes[i];
			const double c = FMath::Abs(FMath::Cos(X.Yaw)), s = FMath::Abs(FMath::Sin(X.Yaw));
			if (FMath::Min(c, s) > 1e-3) continue;
			const bool bAlong = c > s;   // its x along the world's x
			const double hx = (bAlong ? X.Size.X : X.Size.Z) / 2, hz = (bAlong ? X.Size.Z : X.Size.X) / 2;
			double xa = X.At.X - hx, xb = X.At.X + hx, za = X.At.Y - hz, zb = X.At.Y + hz;
			if (xa >= In2.Min.X && xb <= In2.Max.X && za >= In2.Min.Y && zb <= In2.Max.Y) continue;
			if (!Cut(xa, xb, In2.Min.X, In2.Max.X) || !Cut(za, zb, In2.Min.Y, In2.Max.Y)) { Out.Boxes.RemoveAt(i); continue; }
			X.At = FVector2D((xa + xb) / 2, (za + zb) / 2);
			(bAlong ? X.Size.X : X.Size.Z) = (float)(xb - xa); (bAlong ? X.Size.Z : X.Size.X) = (float)(zb - za);
		}
		for (int32 i = Out.Solids.Num() - 1; i >= NS; --i)
		{
			FBrixFurnSolid& So = Out.Solids[i];
			if (!Cut(So.Min.X, So.Max.X, In2.Min.X, In2.Max.X) || !Cut(So.Min.Y, So.Max.Y, In2.Min.Y, In2.Max.Y)) Out.Solids.RemoveAt(i);
		}
	}
	return Out.Boxes.Num() > N0;
}
