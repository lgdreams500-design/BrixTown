#include "BrixOutlet.h"

#include "BrixStreets.h"

#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace BrixSlice;

namespace
{
	using V2 = FVector2D;
	using V3 = FVector3d;
	const double PI_ = 3.141592653589793;

	double Hyp(double A, double B)   // Math.hypot of two, as V8 sums it
	{
		const double a = std::fabs(A), b = std::fabs(B), mx = FMath::Max(a, b);
		if (mx == 0) return 0;
		double sum = 0, comp = 0;
		for (const double v : { a, b }) { const double n = v / mx, summand = n * n - comp, pre = sum + summand; comp = (pre - sum) - summand; sum = pre; }
		return std::sqrt(sum) * mx;
	}
	V3 Mul(const V3& c, double k) { return V3(c.X * k, c.Y * k, c.Z * k); }
	V3 Mix(const V3& a, const V3& b, double f) { return V3(a.X + (b.X - a.X) * f, a.Y + (b.Y - a.Y) * f, a.Z + (b.Z - a.Z) * f); }
	double Hs(std::initializer_list<FString> P) { return Hash(P); }
	FString J(double v) { return Js(v); }
	FString J(int32 v) { return FString::FromInt(v); }
	FString Fixed1(double v) { char B[64]; std::snprintf(B, sizeof(B), "%.1f", v); return FString(ANSI_TO_TCHAR(B)); }
	double YawOf(double dx, double dz) { return std::atan2(dx, -dz); }

	const V3 IRON(.10, .10, .11), INW(.70, .63, .53), FLOORC(.60, .56, .50), WOOD(.34, .23, .15), CARD(.58, .48, .32);
	const V3 PALE(.6, .5, .34), MID(.32, .2, .12), DARK(.17, .12, .09), HONEY(.51, .38, .2);

	// outlet.js LINES: the same parts, different numbers and colours (ADR-0026)
	struct FLine
	{
		FString Label;
		V3 Wood, WoodD, Knob, Frame, Cloth, ClothD, Accent, Rug;
		FString Leg, Arms, Head, Extra;
		double Top = .045, Arm = .16;
		bool bRound = false, bButtons = false, bPanels = false, bStraps = false;
	};
	const FLine& LineOf(const FString& Name)
	{
		static const FLine Plain = { TEXT("Plain"), PALE, HONEY, PALE, V3(.2, .36, .37), V3(.22, .38, .39), V3(.2, .36, .37), V3(.44, .24, .18), V3(.62, .57, .47), TEXT("square"), TEXT("slab"), TEXT("slab"), TEXT("stool"), .045, .16, false, false, false, false };
		static const FLine Old = { TEXT("Old Town"), MID, DARK, HONEY, V3(.44, .24, .18), V3(.47, .26, .19), V3(.3, .15, .11), V3(.3, .33, .2), V3(.3, .24, .34), TEXT("turned"), TEXT("rolled"), TEXT("panel"), TEXT("trunk"), .055, .2, false, true, true, false };
		static const FLine Work = { TEXT("Workshop"), DARK, MID, MID, IRON, V3(.62, .57, .46), V3(.66, .61, .5), V3(.1, .14, .24), V3(.44, .24, .18), TEXT("iron"), TEXT("iron"), TEXT("rails"), TEXT("crate"), .07, .14, false, false, false, true };
		static const FLine Curve = { TEXT("Curve"), HONEY, PALE, HONEY, V3(.3, .38, .52), V3(.32, .4, .54), V3(.3, .38, .52), V3(.52, .4, .2), V3(.2, .36, .37), TEXT("taper"), TEXT("tub"), TEXT("arch"), TEXT("pouffe"), .05, .2, true, false, false, false };
		return Name == TEXT("oldtown") ? Old : Name == TEXT("workshop") ? Work : Name == TEXT("curve") ? Curve : Plain;
	}

	// the recorder (as BrixMall's: the slice's mesh and the lot's hooks)
	struct FRec
	{
		FBrixMall& M;
		explicit FRec(FBrixMall& InM) : M(InM) {}
		FBrixMallCall& New(FBrixMallCall::EKind K) { FBrixMallCall& c = M.Calls.AddDefaulted_GetRef(); c.Kind = K; return c; }
		void Quad(const V3& p0, const V3& p1, const V3& p2, const V3& p3, const V3& c, int32 f, const V3& hint, bool uv = false)
		{
			FBrixMallCall& q = New(FBrixMallCall::Quad);
			for (const V3* p : { &p0, &p1, &p2, &p3 }) q.A.Append({ p->X, p->Y, p->Z });
			q.A.Append({ hint.X, hint.Y, hint.Z, 1, uv ? 1.0 : 0.0 });
			q.Col = c; q.bCol = true; q.Mat = f;
		}
		void Flat(double x0, double z0, double x1, double z1, double y, const V3& c, int32 f) { Quad(V3(x0, y, z0), V3(x0, y, z1), V3(x1, y, z1), V3(x1, y, z0), c, f, V3(0, 1, 0)); }
		// Mesh.box() with its `o`: each face key in the order JavaScript lists an object's number keys (ascending); a face with
		// no colour is left out
		struct FFace { int32 K; bool bOn; V3 C; int32 F; };
		void Box(double x0, double y0, double z0, double x1, double y1, double z1, const V3& c, int32 f, TArray<FFace> Faces = {}, bool bHasO = false)
		{
			FBrixMallCall& q = New(FBrixMallCall::Box);
			q.A.Append({ x0, y0, z0, x1, y1, z1 }); q.Col = c; q.bCol = true; q.Mat = f;
			Faces.StableSort([](const FFace& a, const FFace& b) { return a.K < b.K; });
			for (const FFace& Fc : Faces) { TArray<double, TInlineAllocator<5>>& F = q.Faces.AddDefaulted_GetRef(); F.Add(Fc.K); if (Fc.bOn) F.Append({ Fc.C.X, Fc.C.Y, Fc.C.Z, (double)Fc.F }); }
			(void)bHasO;
		}
		void YBox(double cx, double y0, double cz, double sx, double sy, double sz, double yaw, const V3& c, int32 f) { FBrixMallCall& q = New(FBrixMallCall::YBox); q.A.Append({ cx, y0, cz, sx, sy, sz, yaw }); q.Col = c; q.bCol = true; q.Mat = f; }
		void OBox(const FBrixM4& m, double cx, double cy, double cz, double sx, double sy, double sz, const V3& c, int32 f)
		{
			FBrixMallCall& q = New(FBrixMallCall::OBox);
			for (int32 i = 0; i < 16; ++i) q.A.Add(m.M[i]);
			q.A.Append({ cx, cy, cz, sx, sy, sz }); q.Col = c; q.bCol = true; q.Mat = f;
		}
		void Solid(double x0, double y0, double z0, double x1, double y1, double z1) { New(FBrixMallCall::Solid).A.Append({ x0, y0, z0, x1, y1, z1 }); }
		void Use(double x, double y, double z, const FString& Kind, const FString& Label, double yaw) { FBrixMallCall& q = New(FBrixMallCall::Use); q.A.Append({ x, y, z, yaw }); q.Text = Kind + TEXT("|") + Label; }
		void Spot(double x, double z, double pose, double yaw, double h) { New(FBrixMallCall::Spot).A.Append({ x, z, pose, yaw, h }); }
		void Lamp(double x, double y, double z, double s) { New(FBrixMallCall::Lamp).A.Append({ x, y, z, s }); M.Lamps.Add(FVector4d(x, y, z, s)); }
		void Light(double x, double y, double z, double a, double b) { New(FBrixMallCall::Light).A.Append({ x, y, z, a, b }); }
		void Occl(double a, double b, double c, double d, double e) { New(FBrixMallCall::Occl).A.Append({ a, b, c, d, e }); }
		void Name(const FString& Text, const V3& ctr, const V3& n, double h) { FBrixMallCall& q = New(FBrixMallCall::Name); q.A.Append({ ctr.X, ctr.Y, ctr.Z, n.X, n.Y, n.Z, h }); q.Text = Text; }
		void Car(double x, double z, double yaw, double y, const FString& Seed) { FBrixMallCall& q = New(FBrixMallCall::Car); q.A.Append({ x, z, yaw, y }); q.Text = Seed; }
		void Piece(const V2& At, double y, const V2& Across, const V2& Back, const FString& Kind, const FString& Line)
		{
			FBrixMallCall& q = New(FBrixMallCall::Piece);
			q.A.Append({ At.X, y, At.Y, Across.X, Across.Y, Back.X, Back.Y });
			q.Text = Kind + TEXT("|") + Line;
		}
	};

	// play.js sideOf(): a wall of a lot
	struct FSide { bool ns = true; double fix = 0, out = 1, a0 = 0, a1 = 0; };
	FSide SideOf(const double R[4], TCHAR s)
	{
		FSide S;
		S.ns = s == 'n' || s == 's';
		S.fix = s == 'n' ? R[1] : s == 's' ? R[3] : s == 'w' ? R[0] : R[2];
		S.out = s == 'n' || s == 'w' ? -1 : 1;
		S.a0 = S.ns ? R[0] : R[1]; S.a1 = S.ns ? R[2] : R[3];
		return S;
	}
	V3 SpAt(const FSide& S, double a, double y, double d) { return S.ns ? V3(a, y, S.fix + S.out * d) : V3(S.fix + S.out * d, y, a); }
	V3 SpN(const FSide& S) { return S.ns ? V3(0, 0, S.out) : V3(S.out, 0, 0); }

	struct FOutletBuild
	{
		const TSharedPtr<FJsonObject> L;
		FRec& o;
		double R[4] = { 0, 0, 0, 0 };
		FString Id, NameS;
		double Hh = 8.5;
		const double F = .2;
		FOutletBuild(const TSharedPtr<FJsonObject>& InL, FRec& InO) : L(InL), o(InO) {}

		// play.js fbox(): a box on a wall face, a along the wall, d out from it; the face against the wall left out unless all
		void FBox(const FSide& S, double a0, double a1, double y0, double y1, double d0, double d1, const V3& c, int32 f, bool under = false, bool all = false)
		{
			const double p0 = S.fix + S.out * d0, p1 = S.fix + S.out * d1, lo = FMath::Min(p0, p1), hi = FMath::Max(p0, p1);
			TArray<FRec::FFace> Fc;
			if (!all) Fc.Add({ S.ns ? (S.out < 0 ? 5 : 4) : (S.out < 0 ? 1 : 0), false, V3::ZeroVector, 0 });
			if (under) Fc.Add({ 2, true, Mul(c, .8), f });
			if (S.ns) o.Box(a0, y0, lo, a1, y1, hi, c, f, Fc); else o.Box(lo, y0, a0, hi, y1, a1, c, f, Fc);
		}
		void Win(const FSide& S, double c, double y, double w, double h, double d, double seed, bool lit)   // SP.win()
		{
			const double a = c - w / 2 - .12, b = c + w / 2 + .12, ya = y - .1, yb = y + h;
			auto P = [&](double aa, double yy) { return SpAt(S, aa, yy, d + .03); };
			o.Quad(P(a, ya), P(b, ya), P(b, yb), P(a, yb), V3(0, 0, seed), lit ? 1 : 4, SpN(S), true);
		}
		void ArchHead(const FSide& S, double ca, double spring, double r, double seed, bool lit)   // play.js archHead()
		{
			auto P = [&](double a, double y, double d) { return S.ns ? V3(a, y, S.fix + S.out * d) : V3(S.fix + S.out * d, y, a); };
			const V3 hint = S.ns ? V3(0, 0, S.out) : V3(S.out, 0, 0);
			const int32 N = 12;
			for (int32 i = 0; i < N; ++i)
			{
				const double t0 = PI_ * i / N, t1 = PI_ * (i + 1) / N;
				o.Quad(P(ca, spring, .03), P(ca + r * std::cos(t0), spring + r * std::sin(t0), .03), P(ca + r * std::cos(t1), spring + r * std::sin(t1), .03), P(ca, spring, .03), V3(.5, .5, seed), lit ? 1 : 4, hint);
			}
			const int32 NS = 9;
			const double half = PI_ * (r + .19) / NS / 2 * .94;
			for (int32 i = 0; i < NS; ++i)
			{
				const double th = PI_ * (i + .5) / NS;
				const bool key = i == (NS >> 1);
				const double rr = r + .19 + (key ? .04 : 0);
				const V3 ctr = P(ca + rr * std::cos(th), spring + rr * std::sin(th), key ? .05 : .04);
				const FBrixM4 m = S.ns ? FBrixM4::T(ctr.X, ctr.Y, ctr.Z) * FBrixM4::Rz(th - PI_ / 2) : (FBrixM4::T(ctr.X, ctr.Y, ctr.Z) * FBrixM4::Ry(-PI_ / 2)) * FBrixM4::Rz(th - PI_ / 2);
				o.OBox(m, 0, 0, 0, half * 2, key ? .48 : .38, key ? .1 : .08, key ? StoneDark : Mul(Stone, i % 2 ? .82 : 1), 0);
			}
		}
		void SpLamp(const FSide& S, double a, double y)   // SP.lamp()
		{
			const V3 p = SpAt(S, a, y, .55);
			o.Box(p.X - .13, y, p.Z - .13, p.X + .13, y + .42, p.Z + .13, BrixSlice::Lantern, 2);
			o.Box(p.X - .17, y + .42, p.Z - .17, p.X + .17, y + .5, p.Z + .17, Metal, 9);
			FBox(S, a - .03, a + .03, y + .46, y + .52, 0, .55, Metal, 9, true);
			const V3 q = SpAt(S, a, y + .2, 1.0);
			o.Light(q.X, q.Y, q.Z, 1, 1);
		}

		// ---- the furniture (kit 17's living room), each piece in its own frame: u across, v from its front to its back
		struct FPlacer
		{
			TFunction<void(double, double, double, double, double, double, const V3&, double, int32)> B;
			double ox = 0, oz = 0, c = 1, s = 0;
			bool sw = false;
			V2 At(double u, double v) const { return V2(ox + u * c - v * s, oz + u * s + v * c); }
			void operator()(double u, double y, double v, double su, double sy, double sv, const V3& col, int32 f = 0, double yaw = 0) const
			{
				const V2 p = At(u, v);
				B(p.X, y, p.Y, sw ? sv : su, sy, sw ? su : sv, col, yaw, f);
			}
		};
		static void Oct(const FPlacer& Q, double u, double y, double v, double d, double h, const V3& col, int32 f = 0) { Q(u, y, v, d * .83, h, d * .83, col, f); Q(u, y, v, d * .83, h, d * .83, col, f, PI_ / 4); }
		static void Leg(const FPlacer& Q, const FLine& Ln, double u, double v, double h, const V3* col = nullptr)
		{
			const V3 c = col ? *col : Ln.Wood;
			if (Ln.Leg == TEXT("iron")) { Q(u, 0, v, .04, h, .04, IRON, 9); return; }
			Q(u, 0, v, .05, h, .05, c);
			if (Ln.Leg == TEXT("turned")) { Q(u, h * .55, v, .08, .08, .08, c); Q(u, 0, v, .07, .06, .07, c); }
		}
		static void Legs(const FPlacer& Q, const FLine& Ln, double w, double d, double h, double inset, const V3* col = nullptr)
		{
			for (const V2 ab : { V2(-1, -1), V2(1, -1), V2(-1, 1), V2(1, 1) }) Leg(Q, Ln, ab.X * (w / 2 - inset), ab.Y * (d / 2 - inset), h, col);
		}
		static void Sofa(const FPlacer& Q, const FLine& Ln, double w)
		{
			const double d = .92, b0 = .12;
			const int32 nc = w > 1.2 ? 2 : 1;
			const double cw = (w - 2 * Ln.Arm) / nc;
			Legs(Q, Ln, w - .1, d - .1, b0 + .02, .05);
			if (Ln.Leg == TEXT("iron")) { Q(0, b0, 0, w, .05, d, IRON, 9); Q(0, b0 + .05, .02, w - .08, .09, d - .08, Ln.Wood); }
			else Q(0, b0, .02, w, .26, d, Ln.Frame);
			for (int32 i = 0; i < nc; ++i)
			{
				const double cu = -w / 2 + Ln.Arm + cw * (i + .5);
				Q(cu, .38, -.06, cw - .03, .14, d - .28, Ln.Cloth); Q(cu, .44, d / 2 - .17, cw - .04, .44, .2, Ln.Cloth);
				if (Ln.bButtons) for (const double bu : { -.18, .18 }) for (const double by : { .62, .8 }) Q(cu + bu, by, d / 2 - .28, .05, .05, .03, Ln.ClothD);
			}
			Q(0, b0 + .1, d / 2 - .06, w, .62, .12, Ln.Frame);
			for (const double s : { -1.0, 1.0 })
			{
				const double au = s * (w / 2 - Ln.Arm / 2);
				if (Ln.Arms == TEXT("rolled")) { Q(au, b0, 0, Ln.Arm, .44, d - .02, Ln.Frame); Q(au, b0 + .44, 0, Ln.Arm * 1.15, .14, d, Ln.Frame); }
				else if (Ln.Arms == TEXT("tub")) { Q(au, b0, .02, Ln.Arm * 1.2, .5, d, Ln.Frame); Q(au, b0 + .5, .04, Ln.Arm, .08, d - .12, Ln.Frame); }
				else if (Ln.Arms == TEXT("iron")) { Q(au, b0 + .05, -d / 2 + .05, .04, .55, .04, IRON, 9); Q(au, b0 + .58, 0, Ln.Arm, .06, d, Ln.Wood); Q(au, b0 + .1, .05, Ln.Arm - .03, .36, d - .2, Ln.Cloth); }
				else Q(au, b0, 0, Ln.Arm, .5, d, Ln.Frame);
			}
			Q(-w / 2 + Ln.Arm + .22, .5, d / 2 - .32, .34, .32, .12, Ln.Accent);
		}
		static void Coffee(const FPlacer& Q, const FLine& Ln)
		{
			const double h = .42;
			if (Ln.bRound) { Q(0, 0, 0, .3, h - .05, .3, Ln.Wood); Oct(Q, 0, h - .05, 0, .9, .05, Ln.Wood); }
			else { Legs(Q, Ln, 1.1, .6, h - Ln.Top, .06); Q(0, h - Ln.Top, 0, 1.1, Ln.Top, .6, Ln.Wood); if (Ln.Leg == TEXT("iron")) Q(0, .1, 0, 1.0, .04, .5, Ln.Wood); }
			Q(.18, h, .05, .26, .05, .2, V3(.44, .24, .18)); Q(-.2, h, -.05, .09, .09, .09, V3(.66, .61, .5));
		}
		static void Bed(const FPlacer& Q, const FLine& Ln)
		{
			const double w = 1.6, d = 2.1, h0 = .18;
			if (Ln.Leg == TEXT("iron")) { Legs(Q, Ln, w, d, h0, .04); Q(0, h0, 0, w, .05, d, IRON, 9); }
			else { Legs(Q, Ln, w, d, h0, .06); Q(0, h0, 0, w, .16, d, Ln.Wood); }
			Q(0, h0 + .14, 0, w - .06, .2, d - .06, V3(.78, .74, .66)); Q(0, h0 + .33, -.22, w - .02, .1, d - .5, Ln.Accent);
			for (const double s : { -1.0, 1.0 }) Q(s * .36, h0 + .34, d / 2 - .3, .6, .14, .34, V3(.85, .82, .76));
			const double hv = d / 2 + .03;
			if (Ln.Head == TEXT("arch")) { Q(0, 0, hv, w + .06, .95, .12, Ln.Cloth); Q(0, .95, hv, w * .8, .2, .12, Ln.Cloth); Q(0, 1.15, hv, w * .5, .12, .12, Ln.Cloth); }
			else if (Ln.Head == TEXT("panel"))
			{
				for (const double s : { -1.0, 1.0 }) { Q(s * w / 2, 0, hv, .07, 1.15, .07, Ln.Wood); Q(s * w / 2, 1.13, hv, .12, .12, .12, Ln.Wood); }
				Q(0, .5, hv, w - .04, .6, .06, Ln.Wood); Q(0, .6, hv - .035, w - .3, .42, .012, Ln.WoodD);
			}
			else if (Ln.Head == TEXT("rails"))
			{
				for (const double s : { -1.0, 1.0 }) Q(s * w / 2, 0, hv, .045, 1.05, .045, IRON, 9);
				for (const double y : { .62, 1.0 }) Q(0, y, hv, w, .04, .04, IRON, 9);
				for (int32 i = 0; i < 6; ++i) Q(-w / 2 + w * (i + .5) / 6, .62, hv, .03, .38, .03, IRON, 9);
			}
			else Q(0, .17, hv, w + .04, .9, .07, Ln.Wood);
		}
		static void Bedside(const FPlacer& Q, const FLine& Ln)
		{
			Legs(Q, Ln, .48, .42, .12, .04); Q(0, .12, 0, .48, .43, .42, Ln.Wood);
			Q(0, .3, -.215, .44, .2, .02, Ln.bPanels ? Ln.WoodD : Ln.Wood); Q(0, .38, -.24, .06, .05, .05, Ln.Knob);
			Q(.05, .55, .02, .14, .2, .14, Ln.Knob); Q(.05, .75, .02, .3, .2, .3, V3(.95, .82, .55), 2);
		}
		static void Wardrobe(const FPlacer& Q, const FLine& Ln)
		{
			const double w = 1.1, d = .6, h = 2.0, p = .12;
			if (Ln.Leg == TEXT("turned") || Ln.Leg == TEXT("taper")) Legs(Q, Ln, w, d, p + .02, .06); else Q(0, 0, .02, w - .04, p, d - .04, Ln.WoodD);
			if (Ln.bRound) { Q(0, p, 0, w, h - p - .25, d, Ln.Wood); Q(0, h - .25, 0, w * .82, .15, d, Ln.Wood); Q(0, h - .1, 0, w * .5, .1, d, Ln.Wood); Q(0, p + .2, -d / 2 - .005, .02, h - p - .6, .02, Ln.WoodD); }
			else
			{
				Q(0, p, 0, w, h - p, d, Ln.Wood);
				for (const double s : { -1.0, 1.0 })
				{
					Q(s * w / 4, p + .04, -d / 2 - .012, w / 2 - .03, h - p - .12, .025, Ln.Wood);
					if (Ln.bPanels) Q(s * w / 4, p + .13, -d / 2 - .03, w / 2 - .17, h - p - .3, .012, Ln.WoodD);
					if (Ln.bStraps) for (const double y : { p + .16, h - .2 }) Q(s * w / 4, y, -d / 2 - .03, w / 2 - .08, .04, .012, IRON, 9);
				}
				if (Ln.bPanels) Q(0, h, 0, w + .1, .08, d + .08, Ln.Wood);
			}
			for (const double s : { -1.0, 1.0 }) Q(s * .07, p + .9, -d / 2 - .05, .05, .07, .05, Ln.bStraps ? IRON : Ln.Knob);
		}
		static void Bookcase(const FPlacer& Q, const FLine& Ln, const FString& seed)
		{
			const double w = .9, d = .34, h = 1.8;
			const bool iron = Ln.Leg == TEXT("iron");
			const V3 side = iron ? IRON : Ln.Wood;
			for (const double s : { -1.0, 1.0 })
			{
				if (iron) { Q(s * w / 2, 0, -d / 2 + .02, .035, h, .035, IRON, 9); Q(s * w / 2, 0, d / 2 - .02, .035, h, .035, IRON, 9); }
				else Q(s * (w / 2 - .025), 0, 0, .05, h, d, side);
			}
			if (!iron) Q(0, 0, d / 2 - .01, w - .06, h, .02, Ln.WoodD);
			const double Ys[] = { .04, .48, .92, 1.36, h - .05 };
			const V3 Picks[] = { Terracotta, Teal, Cream, Ochre, Blue, Olive, Plum };
			for (int32 i = 0; i < 5; ++i)
			{
				const double y = Ys[i];
				Q(0, y, 0, w - .02, .045, d - .02, Ln.Wood);
				if (y > h - .1) continue;
				double u = -w / 2 + .07;
				const double end = w / 2 - .07 - (i % 2 ? .25 : 0);
				for (int32 k = 0; u < end - .04; ++k)
				{
					const double bw = .035 + .035 * Hs({ seed, J(i), J(k) }), bh = .2 + .1 * Hs({ seed, J(i), J(k), TEXT("h") });
					Q(u + bw / 2, y + .045, 0, bw, bh, .2, Picks[FMath::Min(6, (int32)FMath::FloorToDouble(Hs({ seed, J(i), J(k), TEXT("c") }) * 7))]);
					u += bw + .006;
				}
			}
		}
		static void FloorLamp(const FPlacer& Q, const FLine& Ln)
		{
			const bool iron = Ln.Leg == TEXT("iron");
			Q(0, 0, 0, .34, .05, .34, iron ? IRON : Ln.Wood, iron ? 9 : 19); Q(0, .05, 0, .045, 1.35, .045, iron ? IRON : Ln.Wood, iron ? 9 : 19);
			if (Ln.bRound) Oct(Q, 0, 1.38, 0, .44, .34, V3(.95, .82, .55), 2); else Q(0, 1.38, 0, .4, .3, .4, V3(.95, .82, .55), 2);
		}
		static void Rug(const FPlacer& Q, const FLine& Ln) { if (Ln.bRound) Oct(Q, 0, .005, 0, 2.0, .012, Ln.Rug); else Q(0, .005, 0, 2.0, .012, 1.4, Ln.Rug); }
		static void Extra(const FPlacer& Q, const FLine& Ln)
		{
			if (Ln.Extra == TEXT("crate")) { Q(0, 0, 0, .6, .44, .45, Ln.Wood); for (const double y : { .06, .3 }) Q(0, y, -.228, .58, .07, .012, Ln.WoodD); }
			else if (Ln.Extra == TEXT("pouffe")) { Oct(Q, 0, 0, 0, .6, .3, Ln.Accent); Oct(Q, 0, .3, 0, .48, .1, Ln.Accent); }
			else if (Ln.Extra == TEXT("trunk")) { Q(0, 0, 0, .8, .44, .45, Ln.Wood); Q(0, .44, 0, .78, .1, .4, Ln.Wood); for (const double u : { -.25, .25 }) Q(u, 0, 0, .05, .56, .47, IRON, 9); }
			else { Legs(Q, Ln, .36, .36, .4, .05); Q(0, .4, 0, .38, .05, .38, Ln.Wood); }
		}

		bool Build(TCHAR Side, const TSharedPtr<FJsonObject>& Block, const TMap<FString, FVector2D>& CarSizes, FString& Err);
		void Inside0();
		void Inside(bool bVis);
		void Carpark(const TSharedPtr<FJsonObject>& P, const FString& BlockId, const TMap<FString, FVector2D>& CarSizes);
		TArray<V2> Lanterns() const
		{
			TArray<V2> out;
			for (double x = R[0] + 6; x < R[2] - 2; x += 12) for (const double z : { R[1] + 4, R[1] + 17.5, R[1] + 29 }) out.Add(V2(x, z));
			return out;
		}
	};

	// furniture.js SIZE (what a kit 18 piece takes up, for walking into it: across, front to back, height) and SEAT (what can
	// be sat or lain on: the use, the height of the seat, its words)
	const FVector3d* KitSize(const FString& Kind)
	{
		static const TMap<FString, FVector3d> S = {
			{ TEXT("l_tv"), { 1.3, .45, 1.1 } }, { TEXT("k17:sofa"), { 2.0, .92, 1.0 } }, { TEXT("k17:armchair"), { .95, .92, 1.0 } }, { TEXT("k17:coffee"), { 1.1, .6, .45 } }, { TEXT("k17:bed"), { 1.6, 2.2, .6 } },
			{ TEXT("k17:bedside"), { .48, .42, .6 } }, { TEXT("k17:wardrobe"), { 1.1, .6, 2.1 } }, { TEXT("k17:bookcase"), { .9, .34, 1.8 } }, { TEXT("k17:floor_lamp"), { .34, .34, 1.7 } },
			{ TEXT("k17:dining"), { 1.4, 1.7, .95 } }, { TEXT("l_fireplace"), { 1.7, .6, 1.2 } }, { TEXT("l_sidetable"), { .5, .5, .6 } }, { TEXT("l_piano"), { 1.45, .58, 1.25 } }, { TEXT("o_floorplant"), { .4, .4, 1.0 } },
			{ TEXT("h_console"), { 1.0, .34, .8 } }, { TEXT("h_shoebench"), { 1.0, .36, .5 } }, { TEXT("h_coatstand"), { .42, .42, 1.8 } }, { TEXT("h_umbrella"), { .3, .3, .6 } },
			{ TEXT("k_counter"), { 1.84, .64, .9 } }, { TEXT("k_sink"), { 1.14, .64, .9 } }, { TEXT("k_stove"), { .75, .62, .9 } }, { TEXT("k_fridge"), { .68, .66, 1.75 } }, { TEXT("k_potshelf"), { 1.0, .3, 1.1 } },
			{ TEXT("b_bath"), { 1.76, .84, .65 } }, { TEXT("b_toilet"), { .44, .56, .8 } }, { TEXT("b_basin"), { .62, .46, .9 } }, { TEXT("b_basket"), { .5, .5, .6 } },
			{ TEXT("r_chest"), { .95, .5, 1.05 } }, { TEXT("r_single"), { .99, 2.1, .6 } }, { TEXT("s_desk"), { 1.35, .68, .75 } }, { TEXT("s_chair"), { .48, .46, .95 } }, { TEXT("g_bistro"), { 1.7, .64, .9 } },
			{ TEXT("o_vases"), { .7, .32, 1.3 } }, { TEXT("o_globe"), { .42, .42, 1.1 } }, { TEXT("o_fruit"), { .5, .5, .75 } }, { TEXT("o_bust"), { .42, .42, 1.55 } }, { TEXT("b_rail"), { .76, .32, .92 } },
			{ TEXT("d_dresser"), { 1.24, .5, 2.1 } }, { TEXT("d_sideboard"), { 1.6, .5, .9 } }, { TEXT("d_bench"), { 1.44, .44, .5 } }, { TEXT("d_display"), { .96, .45, 1.9 } }, { TEXT("d_barstool"), { .42, .42, .8 } },
			{ TEXT("s_shelving"), { 1.04, .4, 1.9 } }, { TEXT("s_filing"), { .48, .62, 1.4 } }, { TEXT("c_bunk"), { 1.06, 2.1, 1.8 } }, { TEXT("c_cot"), { .76, 1.31, 1.0 } }, { TEXT("c_toyshelf"), { 1.1, .36, .85 } },
			{ TEXT("c_table"), { 1.4, .7, .55 } }, { TEXT("c_horse"), { 1.15, .4, .9 } }, { TEXT("c_toychest"), { .9, .52, .56 } }, { TEXT("g_bench"), { 1.58, .6, .95 } }, { TEXT("g_planter"), { 1.2, .42, .6 } },
			{ TEXT("g_parasol"), { .52, .52, 2.4 } }, { TEXT("g_lounger"), { .68, 1.9, .8 } }, { TEXT("g_plantstand"), { 1.0, .36, 1.2 } }, { TEXT("p_counter"), { 1.86, .68, 1.0 } }, { TEXT("p_gondola"), { 1.7, .8, 1.6 } },
			{ TEXT("p_rail"), { 1.48, .5, 1.55 } }, { TEXT("p_table"), { 1.2, .8, .8 } }, { TEXT("p_form"), { .45, .45, 1.65 } }, { TEXT("p_aboard"), { .62, .45, 1.0 } }, { TEXT("f_counter"), { 1.9, .75, 1.1 } },
			{ TEXT("f_machine"), { .8, .55, 1.5 } }, { TEXT("f_banquette"), { 1.8, .66, 1.15 } }, { TEXT("f_table"), { .7, .7, .76 } }, { TEXT("f_cakestand"), { .6, .6, 1.4 } }, { TEXT("r_blanket"), { 1.13, .48, .53 } },
			{ TEXT("r_vanity"), { 1.1, 1.5, 1.55 } }, { TEXT("r_screen"), { 1.5, .5, 1.8 } }, { TEXT("r_cheval"), { .8, .5, 1.6 } }, { TEXT("l_records"), { 1.12, .47, .75 } }, { TEXT("l_magrack"), { .5, .32, .5 } } };
		return S.Find(Kind);
	}
	bool KitSeat(const FString& Kind, FString& Use, double& Up, FString& Label)
	{
		struct FSeat { const TCHAR* Use; double Up; const TCHAR* Label; };
		static const TMap<FString, FSeat> S = {
			{ TEXT("k17:sofa"), { TEXT("sit"), .45, TEXT("Sit on the sofa") } }, { TEXT("k17:armchair"), { TEXT("sit"), .45, TEXT("Sit in the armchair") } }, { TEXT("k17:bed"), { TEXT("lie"), .55, TEXT("Lie on the bed") } },
			{ TEXT("k17:dining"), { TEXT("sit"), .49, TEXT("Sit at the table") } }, { TEXT("s_chair"), { TEXT("sit"), .52, TEXT("Sit at the desk") } }, { TEXT("r_single"), { TEXT("lie"), .55, TEXT("Lie on the bed") } },
			{ TEXT("h_shoebench"), { TEXT("sit"), .48, TEXT("Sit on the bench") } }, { TEXT("l_piano"), { TEXT("sit"), .48, TEXT("Sit at the piano") } }, { TEXT("g_bistro"), { TEXT("sit"), .47, TEXT("Sit at the bistro table") } },
			{ TEXT("d_bench"), { TEXT("sit"), .47, TEXT("Sit on the bench") } }, { TEXT("d_barstool"), { TEXT("sit"), .77, TEXT("Sit on the stool") } }, { TEXT("c_bunk"), { TEXT("lie"), .42, TEXT("Lie on the bunk") } },
			{ TEXT("c_table"), { TEXT("sit"), .3, TEXT("Sit at the little table") } }, { TEXT("g_bench"), { TEXT("sit"), .47, TEXT("Sit on the bench") } }, { TEXT("g_lounger"), { TEXT("lie"), .42, TEXT("Lie on the lounger") } },
			{ TEXT("f_banquette"), { TEXT("sit"), .47, TEXT("Sit on the banquette") } }, { TEXT("r_vanity"), { TEXT("sit"), .45, TEXT("Sit at the dressing table") } }, { TEXT("r_blanket"), { TEXT("sit"), .5, TEXT("Sit on the blanket box") } } };
		const FSeat* F = S.Find(Kind);
		if (!F) return false;
		Use = F->Use; Up = F->Up; Label = F->Label;
		return true;
	}

	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* F, double Else = 0) { double v = Else; if (O) O->TryGetNumberField(F, v); return v; }
	const TArray<TSharedPtr<FJsonValue>>& Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { static const TArray<TSharedPtr<FJsonValue>> None; const TArray<TSharedPtr<FJsonValue>>* A; return O && O->TryGetArrayField(F, A) ? *A : None; }
	TArray<double> Nums(const TSharedPtr<FJsonValue>& V) { TArray<double> out; for (const TSharedPtr<FJsonValue>& q : V->AsArray()) if (q->Type == EJson::Number) out.Add(q->AsNumber()); return out; }

	bool FOutletBuild::Build(TCHAR Side, const TSharedPtr<FJsonObject>& Block, const TMap<FString, FVector2D>& CarSizes, FString& Err)
	{
		Id = L->GetStringField(TEXT("id"));
		L->TryGetStringField(TEXT("name"), NameS);
		const TArray<TSharedPtr<FJsonValue>>& Rc = Arr(L, TEXT("rect"));
		for (int32 i = 0; i < 4; ++i) R[i] = Rc[i]->AsNumber();
		Hh = Num(L, TEXT("hall_height"), 0); if (!Hh) Hh = 8.5;
		const double x0 = R[0], z0 = R[1], x1 = R[2], z1 = R[3], H = Hh, t = .5, dx = Arr(L, TEXT("door"))[0]->AsNumber(), dw = 6, dh = 4.4;
		const V3 wallC(.68, .63, .55);
		auto wall = [&](double ax0, double az0, double ax1, double az1, int32 inner)
		{
			struct FY { double y0, y1; V3 c; int32 f; };
			for (const FY& Y : { FY{ 0, 1.6, Stone, 6 }, FY{ 1.6, H, wallC, 0 } })
			{
				o.Box(ax0, Y.y0, az0, ax1, Y.y1, az1, Y.c, Y.f, { { inner, true, INW, 3 }, { 3, Y.y1 == H, StoneDark, 0 } });
				o.Solid(ax0, Y.y0, az0, ax1, Y.y1, az1); o.Occl(ax0, az0, ax1, az1, Y.y1);
			}
		};
		wall(x0, z0, dx - dw / 2, z0 + t, 5); wall(dx + dw / 2, z0, x1, z0 + t, 5);
		o.Box(dx - dw / 2, dh, z0, dx + dw / 2, H, z0 + t, wallC, 0, { { 5, true, INW, 3 }, { 2, true, StoneDark, 9 }, { 3, true, StoneDark, 0 } }); o.Solid(dx - dw / 2, dh, z0, dx + dw / 2, H, z0 + t);
		wall(x0, z1 - t, x1, z1, 4); wall(x0, z0 + t, x0 + t, z1 - t, 1); wall(x1 - t, z0 + t, x1, z1 - t, 0);
		for (const FVector4d& q : { FVector4d(x0, z0, x1, z0 + .4), FVector4d(x0, z1 - .4, x1, z1), FVector4d(x0, z0, x0 + .4, z1), FVector4d(x1 - .4, z0, x1, z1) })
			o.Box(q.X - .05, H, q.Y - .05, q.Z + .05, H + .9, q.W + .05, StoneDark, 0);
		o.Flat(x0 + t, z0 + t, x1 - t, z1 - t, F, FLOORC, 3);
		for (double x = x0 + 4; x < x1 - 1; x += 4) o.Flat(x - .03, z0 + t, x + .03, z1 - t, F + .002, V3(.48, .45, .4), 3);
		for (double z = z0 + 4; z < z1 - 1; z += 4) o.Flat(x0 + t, z - .03, x1 - t, z + .03, F + .002, V3(.48, .45, .4), 3);
		TArray<double> tx;
		for (double x = x0 + 3; x < x1 - 1; x += 6) tx.Add(x);
		const double sk0 = z0 + 11, sk1 = z1 - 11;
		auto roof = [&](double a, double b, double c, double d) { o.Box(a, H, b, c, H + .3, d, V3(.30, .28, .3), 9, { { 2, true, Mix(INW, V3(1, 1, 1), .15), 3 } }); o.Solid(a, H, b, c, H + .3, d); };
		roof(x0, z0, x1, sk0); roof(x0, sk1, x1, z1);
		double xa = x0;
		for (int32 k = 0; k + 1 < tx.Num(); ++k)
		{
			const double a = tx[k] + .9, b = tx[k + 1] - .9;
			roof(xa, sk0, a, sk1); xa = b;
			for (const FVector4d& q : { FVector4d(a, sk0 - .15, b, sk0), FVector4d(a, sk1, b, sk1 + .15), FVector4d(a - .15, sk0, a, sk1), FVector4d(b, sk0, b + .15, sk1) })
				o.Box(q.X, H + .3, q.Y, q.Z, H + .55, q.W, StoneDark, 0);
		}
		roof(xa, sk0, x1, sk1);
		for (const double x : tx)
		{
			o.Box(x - .16, H - .5, z0 + t, x + .16, H, z1 - t, WOOD, 19, { { 2, true, WOOD, 19 } });
			o.Box(x - .05, H - 1.9, z0 + t, x + .05, H - 1.84, z1 - t, IRON, 9, { { 2, true, IRON, 9 } });
			for (const double z : { z0 + 6, (z0 + z1) / 2, z1 - 6 }) o.Box(x - .06, H - 1.9, z - .06, x + .06, H - .5, z + .06, WOOD, 19);
		}
		// outside: tall arched windows, the sign band over the door, the door's surround, lanterns
		for (const TCHAR s : { 'n', 'w', 'e' })
		{
			const FSide S = SideOf(R, s);
			for (double a = S.a0 + 4; a < S.a1 - 3; a += 6)
			{
				if (s == 'n' && std::fabs(a - dx) < dw / 2 + 2.5) continue;
				Win(S, a, 2.0, 2.2, 3.4, 0, .96 + .03 * Hs({ Id, FString(1, &s), J(a) }), true);
				ArchHead(S, a, 5.4, 1.1, .97, true);
			}
		}
		const FSide S = SideOf(R, 'n');
		FBox(S, dx - 15, dx + 15, 5.4, 7.2, 0, .12, Teal, 0, true);
		o.Name(NameS, SpAt(S, dx, 6.3, .14), SpN(S), 1.05);
		for (const double s : { -1.0, 1.0 }) FBox(S, dx + s * (dw / 2 + .35) - .35, dx + s * (dw / 2 + .35) + .35, 0, dh + .4, 0, .25, StoneDark, 6);
		FBox(S, dx - dw / 2 - .7, dx + dw / 2 + .7, dh, dh + .5, 0, .25, StoneDark, 6, true);
		for (const double s : { -1.0, 1.0 }) SpLamp(S, dx + s * (dw / 2 + 1.4), 3.0);
		const FSide SS = SideOf(R, 's');
		for (const double a : { x0 + 12, x0 + 24, x1 - 12 })
		{
			FBox(SS, a - 2.1, a + 2.1, 0, 4.4, 0, .25, StoneDark, 6); FBox(SS, a - 1.8, a + 1.8, 0, 4.1, .25, .3, Metal, 9);
			for (double y = .3; y < 4.1; y += .35) FBox(SS, a - 1.8, a + 1.8, y, y + .04, .3, .32, V3(.12, .12, .14), 9);
			SpLamp(SS, a, 4.7);
		}
		Inside0();
		Inside(false);   // play.js furnish() at load: the places, the uses, what is in the way
		Inside(true);    // and near the camera: the furniture
		for (const TSharedPtr<FJsonValue>& P : Arr(Block, TEXT("props")))
		{
			FString K;
			if (P->AsObject()->TryGetStringField(TEXT("kind"), K) && K == TEXT("carpark")) Carpark(P->AsObject(), Block->GetStringField(TEXT("id")), CarSizes);
		}
		(void)Side;
		return true;
	}

	void FOutletBuild::Inside0()
	{
		const double x0 = R[0], z0 = R[1], x1 = R[2], H = Hh;
		auto box = [&](double a, double y0, double b, double c, double y1, double d, const V3& col, int32 f = 3, bool all = false) { o.Box(a, y0, b, c, y1, d, col, f); if (!all) o.Solid(a, y0, b, c, y1, d); };
		const V3 RED(.44, .24, .18), PAINT(.66, .61, .5);
		struct FRoute { double a, b, c, d; FString w; };
		TArray<FRoute> route;
		for (const TSharedPtr<FJsonValue>& V : Arr(L, TEXT("route"))) { const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray(); route.Add({ A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber(), A.Num() > 4 ? A[4]->AsString() : FString() }); }
		if (!route.Num()) route.Add({ x0 + 3, z0 + 5.2, x1 - 14, z0 + 6.6, TEXT("e") });
		for (int32 k = 0; k < route.Num(); ++k) o.Flat(route[k].a, route[k].b, route[k].c, route[k].d, F + .004 + .0005 * k, RED, 3);
		for (const FRoute& Rt : route)
		{
			const bool along = Rt.w == TEXT("e") || Rt.w == TEXT("w");
			const double s = Rt.w == TEXT("e") || Rt.w == TEXT("s") ? 1 : -1, m = along ? (Rt.b + Rt.d) / 2 : (Rt.a + Rt.c) / 2, half = (along ? Rt.d - Rt.b : Rt.c - Rt.a) / 2 - .3;
			const double lo = (along ? Rt.a : Rt.b) + 1.2, hi = (along ? Rt.c : Rt.d) - 1.8;
			for (double t = lo; t < hi; t += 3)
			{
				const double t0 = s > 0 ? t : t + .6, t1 = s > 0 ? t + .6 : t;
				auto P = [&](double p, double q) { return along ? V3(p, F + .008, q) : V3(q, F + .008, p); };
				o.Quad(P(t0, m - half), P(t1, m), P(t0, m + half), P(t0, m + half), PAINT, 3, V3(0, 1, 0));
			}
		}
		for (const TSharedPtr<FJsonValue>& SV : Arr(L, TEXT("sets")))
		{
			const TSharedPtr<FJsonObject> St = SV->AsObject();
			const TArray<double> Rr = Nums(St->GetField<EJson::None>(TEXT("rect")));
			const double a = Rr[0], b = Rr[1], c = Rr[2], d = Rr[3], h = 2.8, cx = (a + c) / 2;
			FString Open, LineN, SetName;
			St->TryGetStringField(TEXT("open"), Open); St->TryGetStringField(TEXT("line"), LineN); St->TryGetStringField(TEXT("name"), SetName);
			const FLine& Ln = LineOf(LineN);
			const bool south = Open == TEXT("s");
			o.Box(a, F, b, c, F + .03, d, V3(.4, .27, .17), 15, { { 2, false, V3::ZeroVector, 0 } });
			if (south) { box(a - .15, F, b - .15, c + .15, F + h, b, INW); o.Box(a, F, b, c, F + .14, b + .03, WOOD, 19); }
			else { box(a - .15, F, d, c + .15, F + h, d + .15, INW); o.Box(a, F, d - .03, c, F + .14, d, WOOD, 19); }
			box(a - .15, F, b, a, F + h, d, INW); box(c, F, b, c + .15, F + h, d, INW);
			const double bz = south ? b : d, n = south ? 1 : -1;
			const FString label = SetName.IsEmpty() ? Ln.Label : SetName + TEXT(" · ") + Ln.Label;
			o.Box(cx - 1.7, F + 3.05, FMath::Min(bz - .02 * n, bz - .08 * n), cx + 1.7, F + 3.65, FMath::Max(bz - .02 * n, bz - .08 * n), Teal, 3);
			o.Name(label, V3(cx, F + 3.35, bz), V3(0, 0, n), .4);
			const double fz = south ? d + .9 : b - .9;
			const V2 tags[2] = { south ? V2(c - 1.4, fz) : V2(a + 1.4, fz), south ? V2(a + 1.0, fz) : V2(c - 1.0, fz) };
			for (const V2& uv : tags)
			{
				const double u = uv.X, v = uv.Y;
				box(u - .02, F, v - .02, u + .02, F + 1.0, v + .02, IRON, 9, true);
				o.Box(u - .16, F + .98, FMath::Min(v, v - .03 * -n), u + .16, F + 1.2, FMath::Max(v, v - .03 * -n), V3(.66, .61, .5), 3);
				o.Box(u - .16, F + 1.14, FMath::Min(v - .03 * -n, v - .035 * -n), u + .16, F + 1.19, FMath::Max(v - .03 * -n, v - .035 * -n), Ln.Cloth, 3);
			}
		}
		// staff places by what stands there (the lot's fittings)
		const TSharedPtr<FJsonObject>* Fp;
		TArray<V2> tills, desks, cafe, packing;
		if (L->TryGetObjectField(TEXT("fittings"), Fp))
		{
			for (const TSharedPtr<FJsonValue>& q : Arr(*Fp, TEXT("tills"))) { const TArray<double> A = Nums(q); tills.Add(V2(A[0], A[1])); }
			for (const TSharedPtr<FJsonValue>& q : Arr(*Fp, TEXT("desks"))) { const TArray<double> A = Nums(q); desks.Add(V2(A[0], A[1])); }
			for (const TSharedPtr<FJsonValue>& q : Arr(*Fp, TEXT("cafe"))) { const TArray<double> A = Nums(q); cafe.Add(V2(A[0], A[1])); }
			for (const TSharedPtr<FJsonValue>& q : Arr(*Fp, TEXT("packing"))) { const TArray<double> A = Nums(q); packing.Add(V2(A[0], A[1])); }
		}
		TArray<V2> tc = tills; tc.Append(cafe);
		for (const V2& q : tc)
		{
			const double x = q.X, z = q.Y;
			const bool bCafe = cafe.ContainsByPredicate([&](const V2& c) { return c.X == x && c.Y == z; });
			const double hw = bCafe ? 1.7 : 1.1;
			box(x - hw, F, z - 1.2, x + hw, F + .95, z - .6, bCafe ? Brown : WOOD, 19);
			o.Box(x - hw - .05, F + .95, z - 1.25, x + hw + .05, F + 1.0, z - .55, bCafe ? Cream : Teal, 3);
			o.Box(x - .2, F + 1.0, z - 1.05, x + .2, F + 1.25, z - .8, bCafe ? IRON : Charcoal, 9);
		}
		for (const V2& q : desks) { box(q.X - .65, F, q.Y - 1.15, q.X + .65, F + .76, q.Y - .5, WOOD, 19); o.Box(q.X - .15, F + .76, q.Y - 1.0, q.X + .15, F + .78, q.Y - .78, V3(.88, .86, .8), 3); }
		for (const V2& q : packing) box(q.X - 1.2, F, q.Y - 1.3, q.X + 1.2, F + .9, q.Y - .5, WOOD, 19);
		// the warehouse: three rows of tall timber racks with flat-pack boxes in the company's band
		TArray<double> rows;
		for (const TSharedPtr<FJsonValue>& v : Arr(L, TEXT("rack_rows"))) rows.Add(v->AsNumber());
		if (!rows.Num()) rows = { z0 + 25.5, z0 + 31.5, z0 + 37 };
		for (const double rz : rows)
			for (const V2 ac : { V2(x0 + 3, x0 + 20), V2(x0 + 24, x0 + 42), V2(x0 + 46, x1 - 3) })
			{
				const double a = ac.X, c = ac.Y;
				for (double x = a; x <= c + .01; x += (c - a) / 3) box(x - .06, F, rz - .7, x + .06, F + 5, rz + .7, WOOD, 19);
				const double Ys[] = { .1, 1.8, 3.5 };
				for (int32 si = 0; si < 3; ++si)
				{
					const double y = Ys[si];
					o.Box(a, F + y, rz - .7, c, F + y + .07, rz + .7, WOOD, 19);
					double x = a + .1;
					for (int32 k = 0; x < c - .5; ++k)
					{
						const double bw = .5 + .6 * Hs({ Id, J(rz), J(a), J(si), J(k) }), bh = .3 + .9 * Hs({ Id, J(rz), J(si), J(k), TEXT("h") });
						if (x + bw > c - .1) break;
						o.Box(x, F + y + .07, rz - .55, x + bw, F + y + .07 + bh, rz + .55, CARD, 3); o.Box(x, F + y + .07 + bh * .62, rz - .56, x + bw, F + y + .07 + bh * .62 + .08, rz - .55, Teal, 3);
						x += bw + .06;
					}
				}
				o.Solid(a, F, rz - .7, c, F + 5, rz + .7);
			}
		for (const V2& q : Lanterns())
		{
			const double x = q.X, z = q.Y;
			o.Box(x - .02, F + H - 2.8, z - .02, x + .02, H - .5, z + .02, IRON, 9); o.Box(x - .22, F + H - 3.3, z - .22, x + .22, F + H - 2.8, z + .22, BrixSlice::Lantern, 2); o.Box(x - .26, F + H - 2.8, z - .26, x + .26, F + H - 2.72, z + .26, IRON, 9);
		}
	}

	void FOutletBuild::Inside(bool bVis)
	{
		const double H = Hh, z0 = R[1], north = YawOf(0, -1), south = YawOf(0, 1);
		FPlacer Base;
		Base.B = [&](double cx, double y0, double cz, double sx, double sy, double sz, const V3& c, double yaw, int32 f)
		{
			if (!bVis) return;
			if (f == 2) o.Lamp(cx, F + y0 + sy / 2, cz, FMath::Min(.5, .9 * FMath::Max3(sx, sy, sz)));
			o.YBox(cx, F + y0, cz, sx, sy, sz, yaw, c, f ? f : 19);
		};
		auto B = [&](double cx, double y0, double cz, double sx, double sy, double sz, const V3& c, double yaw = 0, int32 f = 0) { Base.B(cx, y0, cz, sx, sy, sz, c, yaw, f); };
		auto hard = [&](double ax0, double ay0, double az0, double ax1, double ay1, double az1) { if (!bVis) o.Solid(FMath::Min(ax0, ax1), ay0, FMath::Min(az0, az1), FMath::Max(ax0, ax1), ay1, FMath::Max(az0, az1)); };
		auto spot = [&](double x, double z, double pose, double yaw, double h) { if (!bVis) o.Spot(x, z, pose, yaw, h); };
		auto use = [&](double x, double y, double z, const TCHAR* K, const TCHAR* Lb, double yaw) { if (!bVis) o.Use(x, y, z, K, Lb, yaw); };
		struct FSet { double a, b, c, d; bool bOpen, bSouth; FString Line; TSharedPtr<FJsonObject> O; };
		TArray<FSet> sets;
		for (const TSharedPtr<FJsonValue>& SV : Arr(L, TEXT("sets")))
		{
			const TSharedPtr<FJsonObject> St = SV->AsObject();
			const TArray<double> Rr = Nums(St->GetField<EJson::None>(TEXT("rect")));
			FString Open, LineN;
			const bool bOpen = St->TryGetStringField(TEXT("open"), Open) && !Open.IsEmpty();
			St->TryGetStringField(TEXT("line"), LineN);
			sets.Add({ Rr[0], Rr[1], Rr[2], Rr[3], bOpen, Open == TEXT("s"), LineN, St });
		}
		// kit 17's living room in each set: the same places in every line (outlet.js LAYOUT), turned round for a set open to the south
		struct FLay { const TCHAR* Kind; double u, v; int32 rot; };
		static const FLay LAYOUT[] = { { TEXT("rug"), -.9, .5, 0 }, { TEXT("sofa"), -.9, 1.65, 0 }, { TEXT("chair"), .55, .45, 3 }, { TEXT("coffee"), -.9, .5, 0 }, { TEXT("lamp"), -2.4, 1.85, 0 },
			{ TEXT("case"), -2.95, -.2, 1 }, { TEXT("bed"), 2.05, 1.1, 0 }, { TEXT("side"), .95, 1.95, 0 }, { TEXT("ward"), 2.85, -1.3, 3 }, { TEXT("extra"), 2.05, -.4, 0 } };
		static const TMap<FString, FVector3d> SIZE = { { TEXT("sofa"), FVector3d(2.0, .92, 1.0) }, { TEXT("chair"), FVector3d(.95, .92, 1.0) }, { TEXT("coffee"), FVector3d(1.1, .6, .45) }, { TEXT("bed"), FVector3d(1.6, 2.2, .6) },
			{ TEXT("side"), FVector3d(.48, .42, .6) }, { TEXT("ward"), FVector3d(1.1, .6, 2.1) }, { TEXT("case"), FVector3d(.9, .34, 1.8) }, { TEXT("extra"), FVector3d(.8, .45, .5) } };
		for (int32 k = 0; k < sets.Num(); ++k)
		{
			const FSet& St = sets[k];
			const FLine& Ln = LineOf(St.Line);
			const FString LineKey = St.Line == TEXT("oldtown") || St.Line == TEXT("workshop") || St.Line == TEXT("curve") ? St.Line : FString(TEXT("plain"));
			if (St.O->HasField(TEXT("pieces")))   // a kit 18 room (furnishRoom()): each piece [kind, x, y, yaw] in Blender's room, x across, y to the back wall
			{
				const double cx = (St.a + St.c) / 2, cz = (St.b + St.d) / 2;
				auto W = [&](double X, double Y) { return St.bSouth ? V2(cx + X, cz - Y) : V2(cx - X, cz + Y); };
				for (const TSharedPtr<FJsonValue>& PV : St.O->GetArrayField(TEXT("pieces")))
				{
					const TArray<TSharedPtr<FJsonValue>>& Pc = PV->AsArray();
					const FString Kind = Pc[0]->AsString();
					const double px = Pc[1]->AsNumber(), py = Pc[2]->AsNumber(), th = Pc[3]->AsNumber(), co = std::cos(th), si = std::sin(th);
					if (bVis) { const V2 p0 = W(px, py); o.Piece(p0, F, W(px + co, py + si) - p0, W(px - si, py + co) - p0, Kind, LineKey); }
					if (const FVector3d* sz = KitSize(Kind))
					{
						const double ac = std::fabs(co), as = std::fabs(si), hx = (ac * sz->X + as * sz->Y) / 2, hz = (as * sz->X + ac * sz->Y) / 2;
						const V2 p = W(px, py);
						hard(p.X - hx, F, p.Y - hz, p.X + hx, F + FMath::Max(sz->Z, .9), p.Y + hz);
					}
					FString SeatUse, SeatLabel;
					double SeatUp = 0;
					if (KitSeat(Kind, SeatUse, SeatUp, SeatLabel))
					{
						const V2 p = W(px + .1 * si, py - .1 * co), f = St.bSouth ? V2(si, co) : V2(-si, -co);
						use(p.X, F + SeatUp, p.Y, *SeatUse, *SeatLabel, YawOf(f.X, f.Y));
					}
				}
				continue;
			}
			const double ox = (St.a + St.c) / 2, oz = (St.b + St.d) / 2;
			const FString seed = Id + J(k);
			for (const FLay& Ly : LAYOUT)
			{
				const double u = St.bSouth ? -Ly.u : Ly.u, v = St.bSouth ? -Ly.v : Ly.v;
				const int32 rot = St.bSouth ? (Ly.rot + 2) % 4 : Ly.rot;
				FPlacer Q = Base;
				Q.ox = ox + u; Q.oz = oz + v;
				const int32 Cs[4] = { 1, 0, -1, 0 }, Ss[4] = { 0, 1, 0, -1 };
				Q.c = Cs[rot]; Q.s = Ss[rot]; Q.sw = rot % 2 == 1;
				const FString Kind = Ly.Kind;
				static const TMap<FString, FString> K17 = { { TEXT("rug"), TEXT("k17:rug") }, { TEXT("sofa"), TEXT("k17:sofa") }, { TEXT("chair"), TEXT("k17:armchair") }, { TEXT("coffee"), TEXT("k17:coffee") },
					{ TEXT("lamp"), TEXT("k17:floor_lamp") }, { TEXT("case"), TEXT("k17:bookcase") }, { TEXT("bed"), TEXT("k17:bed") }, { TEXT("side"), TEXT("k17:bedside") }, { TEXT("ward"), TEXT("k17:wardrobe") } };
				if (const FString* Kit = K17.Find(Kind)) { if (bVis) { const V2 p0 = Q.At(0, 0); o.Piece(p0, F, Q.At(1, 0) - p0, Q.At(0, 1) - p0, *Kit, LineKey); } }   // the kit's own piece (ADR-0050)
				else Extra(Q, Ln);   // the set's stool, trunk, crate or pouffe: as the slice draws it
				(void)seed;
				if (const FVector3d* sz = SIZE.Find(Kind))
				{
					const V2 p = Q.At(-sz->X / 2, -sz->Y / 2), q = Q.At(sz->X / 2, sz->Y / 2);
					hard(FMath::Min(p.X, q.X), F, FMath::Min(p.Y, q.Y), FMath::Max(p.X, q.X), F + sz->Z, FMath::Max(p.Y, q.Y));
				}
				const V2 Dirs[4] = { V2(0, -1), V2(1, 0), V2(0, 1), V2(-1, 0) };
				const double face = YawOf(Dirs[rot].X, Dirs[rot].Y);
				if (Kind == TEXT("sofa") || Kind == TEXT("chair")) { const V2 p = Q.At(0, -.1); use(p.X, F + .45, p.Y, TEXT("sit"), Kind == TEXT("sofa") ? TEXT("Try the sofa") : TEXT("Try the armchair"), face); }
				if (Kind == TEXT("bed")) { const V2 p = Q.At(0, 0); use(p.X, F + .62, p.Y, TEXT("lie"), TEXT("Try the bed"), 0); }
			}
		}
		const TSharedPtr<FJsonObject>* Ob;
		if (!L->TryGetObjectField(TEXT("objects"), Ob)) return;
		for (const TSharedPtr<FJsonValue>& q : Arr(*Ob, TEXT("work"))) { const TArray<double> A = Nums(q); spot(A[0], A[1], 0, north, F); }
		const bool anyOpen = sets.ContainsByPredicate([](const FSet& s) { return s.bOpen; });
		for (const TSharedPtr<FJsonValue>& q : Arr(*Ob, TEXT("customer")))
		{
			const TArray<double> A = Nums(q);
			const double x = A[0], z = A[1];
			const FSet* st = sets.FindByPredicate([&](const FSet& s) { return x > s.a - .5 && x < s.c + .5 && z > s.b - 1.5 && z < s.d + 1.5; });
			spot(x, z, 0, st ? (st->bSouth ? north : south) : sets.Num() && anyOpen ? north : (z < z0 + 10 ? south : north), F);
		}
		TArray<V2> seats, cafe;
		for (const TSharedPtr<FJsonValue>& q : Arr(*Ob, TEXT("seat"))) { const TArray<double> A = Nums(q); seats.Add(V2(A[0], A[1])); }
		const V3 seatC(.66, .61, .5);
		for (const V2& s : seats)
		{
			const double x = s.X, z = s.Y;
			TArray<V2> others = seats.FilterByPredicate([&](const V2& q) { return q.X != x || q.Y != z; });
			others.StableSort([&](const V2& p, const V2& q) { return Hyp(p.X - x, p.Y - z) - Hyp(q.X - x, q.Y - z) < 0; });
			const bool pair = others.Num() && Hyp(others[0].X - x, others[0].Y - z) < 2.2;
			const double tx = pair ? (x + others[0].X) / 2 : x, tz = pair ? (z + others[0].Y) / 2 : z - .65;
			const double fy = YawOf(tx - x, tz - z), fx = std::sin(fy), fz = -std::cos(fy);
			B(x, .4, z, .44, .07, .44, seatC); B(x - fx * .2, .47, z - fz * .2, .44, .45, .06, seatC, -fy);
			for (const V2 uv : { V2(-.18, -.18), V2(.18, -.18), V2(-.18, .18), V2(.18, .18) }) B(x + uv.X, 0, z + uv.Y, .04, .4, .04, IRON, 0, 9);
			if (!cafe.ContainsByPredicate([&](const V2& p) { return Hyp(p.X - tx, p.Y - tz) < .3; }))
			{
				cafe.Add(V2(tx, tz));
				B(tx, .7, tz, .6, .04, .6, WOOD, PI_ / 4); B(tx, 0, tz, .08, .7, .08, IRON, 0, 9); hard(tx - .3, F, tz - .3, tx + .3, F + .74, tz + .3);
			}
			spot(x, z, 2, fy, F + .45); use(x, F + .45, z, TEXT("sit"), TEXT("Sit down"), fy);
		}
		if (bVis) for (const V2& q : Lanterns()) o.Lamp(q.X, F + H - 3.1, q.Y, 1.1);
	}

	void FOutletBuild::Carpark(const TSharedPtr<FJsonObject>& P, const FString& BlockId, const TMap<FString, FVector2D>& CarSizes)
	{
		const TArray<double> Pos = Nums(P->GetField<EJson::None>(TEXT("pos"))), Size = Nums(P->GetField<EJson::None>(TEXT("size")));
		const double cx = Pos[0], cz = Pos[1], w = Size[0], d = Size[1], y = .12 + .006, a0 = cx - w / 2, c0 = cz - d / 2;
		const int32 rows = (int32)Num(P, TEXT("rows"), 0) ? (int32)Num(P, TEXT("rows"), 0) : 2;
		o.Flat(a0, c0, a0 + w, c0 + d, y, V3(.27, .26, .27), 9);
		const double bay = 2.6, deep = 5;
		const int32 n = (int32)FMath::FloorToDouble((w - 2) / bay);
		struct FBay { double x, z, face; };
		TArray<FBay> bays;
		for (int32 r = 0; r < rows; ++r)
		{
			const double zs = rows <= 2 ? (r == 0 ? c0 + .5 : c0 + d - .5 - deep) : c0 + .5 + FMath::FloorToDouble(r / 2.0) * (2 * deep + 6) + (r % 2) * deep;
			const double face = rows <= 2 ? (r == 0 ? 1 : -1) : (r % 2 ? -1 : 1);
			if (zs + deep > c0 + d) break;
			for (int32 k = 0; k <= n; ++k) o.Flat(a0 + 1 + k * bay - .05, zs, a0 + 1 + k * bay + .05, zs + deep, y + .002, V3(.66, .61, .5), 9);
			for (int32 k = 0; k < n; ++k) bays.Add({ a0 + 1 + (k + .5) * bay, zs + deep / 2, face });
		}
		o.Flat(a0 + 1, c0 + d / 2 - .06, a0 + w - 1, c0 + d / 2 + .06, y + .002, V3(.66, .61, .5), 9);
		const int32 want = FMath::Min((int32)Num(P, TEXT("cars"), 0), bays.Num());
		TArray<TPair<double, int32>> used;
		for (int32 k = 0; k < bays.Num(); ++k) used.Add({ Hs({ BlockId, TEXT("park"), J(k) }), k });
		used.StableSort([](const TPair<double, int32>& a, const TPair<double, int32>& b) { return a.Key < b.Key; });
		const V3 Picks[] = { Terracotta, Teal, Blue, Ochre, Olive, Plum, Cream };
		for (int32 k = 0; k < want; ++k)
		{
			const FBay& Bq = bays[used[k].Value];
			const double x = Bq.x, z = Bq.z, yaw = Bq.face > 0 ? PI_ / 2 : -PI_ / 2;
			const bool full = k < 16;
			V2 dims(3.7, 1.7);
			if (full)
			{
				const FString seed = TEXT("park") + Fixed1(x) + Fixed1(z);
				const double hm = Hs({ seed, TEXT("model") });
				const FString Model = hm < .4 ? TEXT("small") : hm < .72 ? TEXT("estate") : TEXT("pickup");
				if (const FVector2D* Sz = CarSizes.Find(Model)) dims = *Sz;
				o.Car(x, z, yaw, y, seed);
			}
			else
			{
				const V3 col = Picks[FMath::Min(6, (int32)FMath::FloorToDouble(Hs({ BlockId, TEXT("car"), J(k) }) * 7))], roof = Cream;
				o.Box(x - .85, y + .25, z - 1.85, x + .85, y + .95, z + 1.85, col, 9);
				o.Box(x - .75, y + .95, z - (Bq.face > 0 ? .6 : 1.1), x + .75, y + 1.45, z + (Bq.face > 0 ? 1.1 : .6), roof, 9);
				for (const double u : { -.7, .7 }) for (const double v : { -1.2, 1.2 }) o.Box(x + u - .12, y, z + v - .32, x + u + .12, y + .5, z + v + .32, Charcoal, 9);
			}
			o.Solid(x - dims.Y / 2, 0, z - dims.X / 2, x + dims.Y / 2, 1.5, z + dims.X / 2);
		}
	}
}

bool BuildBrixOutlet(const FJsonObject& Lot, const FJsonObject& Block, const TMap<FString, FVector2D>& CarSizes, FBrixMall& Out, FString& OutError)
{
	const TSharedPtr<FJsonObject> L = MakeShared<FJsonObject>(Lot), B = MakeShared<FJsonObject>(Block);
	Out = FBrixMall();
	FRec o(Out);
	FOutletBuild Bd(L, o);
	const TArray<TSharedPtr<FJsonValue>>& Rc = Arr(L, TEXT("rect")), & Dr = Arr(L, TEXT("door"));
	if (Rc.Num() < 4 || Dr.Num() < 2) { OutError = TEXT("the outlet has no rect or door"); return false; }
	const double x0 = Rc[0]->AsNumber(), z0 = Rc[1]->AsNumber(), x1 = Rc[2]->AsNumber(), z1 = Rc[3]->AsNumber(), dx = Dr[0]->AsNumber(), dz = Dr[1]->AsNumber();
	const TCHAR Side = std::fabs(dz - z0) < .01 ? 'n' : std::fabs(dz - z1) < .01 ? 's' : std::fabs(dx - x0) < .01 ? 'w' : 'e';   // play.js building()
	if (!Bd.Build(Side, B, CarSizes, OutError)) return false;
	Out.LotId = Bd.Id; Out.Name = Bd.NameS;
	Out.X0 = x0; Out.Z0 = z0; Out.X1 = x1; Out.Z1 = z1;
	Out.H = Bd.Hh; Out.StoreyHeight = Bd.Hh; Out.F = Bd.F; Out.Levels = 1;
	return true;
}

bool LoadBrixOutlet(const FString& CityDir, const FString& CarModels, FBrixMall& Out, FString& OutError)
{
	TMap<FString, FVector2D> Sizes;
	FString MText;
	TSharedPtr<FJsonObject> MJ;
	const TSharedPtr<FJsonObject>* Models = nullptr;
	if (FFileHelper::LoadFileToString(MText, *CarModels) && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(MText), MJ) && MJ && MJ->TryGetObjectField(TEXT("models"), Models))
		for (const TPair<FString, TSharedPtr<FJsonValue>>& M : (*Models)->Values)
		{
			const TArray<double> Bx = Nums(M.Value->AsObject()->GetField<EJson::None>(TEXT("box")));
			if (Bx.Num() >= 4) Sizes.Add(M.Key, FVector2D(Bx[1] - Bx[0], Bx[3] - Bx[2]));
		}
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(CityDir, TEXT("city.json")))) { OutError = TEXT("no city.json in ") + CityDir; return false; }
	TSharedPtr<FJsonObject> City;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), City) || !City) { OutError = TEXT("city.json is not JSON"); return false; }
	for (const TSharedPtr<FJsonValue>& BV : City->GetArrayField(TEXT("blocks")))
	{
		FString File, BText;
		if (!BV->AsObject()->TryGetStringField(TEXT("file"), File) || !FFileHelper::LoadFileToString(BText, *FPaths::Combine(CityDir, File))) continue;
		if (!BText.Contains(TEXT("\"furniture\""))) continue;
		TSharedPtr<FJsonObject> Blk;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BText), Blk) || !Blk) continue;
		for (const TSharedPtr<FJsonValue>& LV : Blk->GetArrayField(TEXT("lots")))
		{
			FString K;
			if (LV->AsObject()->TryGetStringField(TEXT("kind"), K) && K == TEXT("furniture")) return BuildBrixOutlet(*LV->AsObject(), *Blk, Sizes, Out, OutError);
		}
	}
	OutError = TEXT("no furniture outlet in the block files");
	return false;
}
