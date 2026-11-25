#include "BrixMall.h"

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

	// ---- JavaScript's own arithmetic where it matters for the same numbers
	double Hyp(double A, double B)   // Math.hypot of two (V8: scaled by the larger, Kahan summed)
	{
		const double a = std::fabs(A), b = std::fabs(B), mx = FMath::Max(a, b);
		if (mx == 0) return 0;
		double sum = 0, comp = 0;
		for (const double v : { a, b })
		{
			const double n = v / mx, summand = n * n - comp, pre = sum + summand;
			comp = (pre - sum) - summand;
			sum = pre;
		}
		return std::sqrt(sum) * mx;
	}
	double Hyp3(double A, double B, double C)
	{
		const double a = std::fabs(A), b = std::fabs(B), c = std::fabs(C), mx = FMath::Max3(a, b, c);
		if (mx == 0) return 0;
		double sum = 0, comp = 0;
		for (const double v : { a, b, c })
		{
			const double n = v / mx, summand = n * n - comp, pre = sum + summand;
			comp = (pre - sum) - summand;
			sum = pre;
		}
		return std::sqrt(sum) * mx;
	}
	double Clamp(double v, double a, double b) { return FMath::Max(a, FMath::Min(b, v)); }
	V3 Mul(const V3& c, double k) { return V3(c.X * k, c.Y * k, c.Z * k); }
	V3 Mix(const V3& a, const V3& b, double f) { return V3(a.X + (b.X - a.X) * f, a.Y + (b.Y - a.Y) * f, a.Z + (b.Z - a.Z) * f); }
	V3 Lit(double s) { return V3(.3, .4, s); }
	double ToFixed2(double v) { char B[64]; std::snprintf(B, sizeof(B), "%.2f", v); return std::strtod(B, nullptr); }
	double H(std::initializer_list<FString> P) { return Hash(P); }
	double HashOf(const TArray<FString>& Parts)   // play.js hash() of parts gathered at run time (BrixSliceHash's own steps)
	{
		uint32 h = 2166136261u;
		for (const FString& S : Parts)
		{
			for (TCHAR Ch : S) { h ^= (uint32)Ch; h *= 16777619u; }
			h ^= 124u; h *= 16777619u;
		}
		h ^= h >> 15; h *= 2246822519u; h ^= h >> 13; h *= 3266489917u; h ^= h >> 16;
		return (double)h / 4294967296.0;
	}
	FString J(double v) { return Js(v); }
	FString J(int32 v) { return FString::FromInt(v); }

	using Poly = TArray<V2>;
	bool InPoly(const Poly& p, double x, double z)
	{
		bool h = false;
		for (int32 i = 0, j = p.Num() - 1; i < p.Num(); j = i++)
		{
			const V2& a = p[i]; const V2& b = p[j];
			if ((a.Y > z) != (b.Y > z) && x < a.X + (z - a.Y) * (b.X - a.X) / (b.Y - a.Y)) h = !h;
		}
		return h;
	}
	double EdgeDist(const Poly& p, double x, double z)
	{
		double d = 1e9;
		for (int32 i = 0, j = p.Num() - 1; i < p.Num(); j = i++)
		{
			const V2& a = p[j]; const V2& b = p[i];
			const double ex = b.X - a.X, ez = b.Y - a.Y, den = ex * ex + ez * ez;
			const double t = Clamp(((x - a.X) * ex + (z - a.Y) * ez) / (den != 0 ? den : 1), 0, 1);
			d = FMath::Min(d, Hyp(x - a.X - ex * t, z - a.Y - ez * t));
		}
		return d;
	}
	FVector4d BoxOf(const Poly& p)
	{
		double a = 1e9, b = 1e9, c = -1e9, d = -1e9;
		for (const V2& q : p) { a = FMath::Min(a, q.X); b = FMath::Min(b, q.Y); c = FMath::Max(c, q.X); d = FMath::Max(d, q.Y); }
		return FVector4d(a, b, c, d);
	}
	double Area(const Poly& p)
	{
		double s = 0;
		for (int32 i = 0; i < p.Num(); ++i) { const V2& a = p[i]; const V2& b = p[(i + 1) % p.Num()]; s += a.X * b.Y - b.X * a.Y; }
		return s / 2;
	}
	TArray<V2> Outward(const Poly& p)   // at each corner of a closed line, the way out of it
	{
		const int32 n = p.Num();
		const double sg = Area(p) > 0 ? 1 : -1;
		TArray<V2> out;
		for (int32 i = 0; i < n; ++i)
		{
			const V2& a = p[(i + n - 1) % n]; const V2& b = p[(i + 1) % n];
			const double dx = b.X - a.X, dz = b.Y - a.Y, Lh = Hyp(dx, dz), L = Lh != 0 ? Lh : 1;
			out.Add(V2(dz / L * sg, -dx / L * sg));
		}
		return out;
	}

	// ---- the lot's data
	double Num(const TSharedPtr<FJsonObject>& O, const TCHAR* F, double Else = 0) { double v = Else; if (O) O->TryGetNumberField(F, v); return v; }
	FString Str(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { FString s; if (O) O->TryGetStringField(F, s); return s; }
	bool Has(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { return O && O->HasField(F) && !O->GetField<EJson::None>(F)->IsNull(); }
	V2 Pt2(const TSharedPtr<FJsonValue>& V) { const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray(); return V2(A[0]->AsNumber(), A[1]->AsNumber()); }
	V2 Pt2F(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { const TArray<TSharedPtr<FJsonValue>>* A; return O && O->TryGetArrayField(F, A) ? V2((*A)[0]->AsNumber(), (*A)[1]->AsNumber()) : V2::ZeroVector; }
	Poly Line(const TSharedPtr<FJsonValue>& V) { Poly p; for (const TSharedPtr<FJsonValue>& q : V->AsArray()) p.Add(Pt2(q)); return p; }
	TArray<Poly> Lines(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { TArray<Poly> out; const TArray<TSharedPtr<FJsonValue>>* A; if (O && O->TryGetArrayField(F, A)) for (const TSharedPtr<FJsonValue>& v : *A) out.Add(Line(v)); return out; }
	const TArray<TSharedPtr<FJsonValue>>& Arr(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { static const TArray<TSharedPtr<FJsonValue>> None; const TArray<TSharedPtr<FJsonValue>>* A; return O && O->TryGetArrayField(F, A) ? *A : None; }
	TArray<FString> Strs(const TSharedPtr<FJsonObject>& O, const TCHAR* F) { TArray<FString> out; for (const TSharedPtr<FJsonValue>& v : Arr(O, F)) out.Add(v->AsString()); return out; }

	struct FUnit { FString Id, Name, Kind, Outlet; double Rect[4] = { 0, 0, 0, 0 }; double Angle = 0; TArray<V2> Stand; bool bStand = false; };
	struct FFront { FString Unit; V2 A, B; };
	struct FWallLine { FString Kind; Poly Line; };
	struct FLevel { TArray<FUnit> Units; TArray<Poly> Walks, Voids, Bridges, Rails; TArray<FFront> Fronts; TArray<FWallLine> Walls; };
	struct FHall { FString Unit; V2 Pos; double R = 0; int32 Dome = 0; };
	struct FRamp { int32 From = 0, To = 0; V2 A, B; double Width = 0; };
	struct FLift { V2 Pos; double R = 0, Door = 0; };
	struct FTreeAt { double X = 0, Z = 0; FString Sp; };
	struct FGarden { V2 Pos; double R0 = 0, R1 = 0; TArray<FTreeAt> Trees; bool bLow = false; };
	struct FPlace { int32 Level = 0; V2 Pos; double Yaw = 0; };
	struct FSpec { FString Label, Trade, Sign, Floor; TArray<FString> Walls, Middle, Window; };
	struct FRide { FString Kind; V2 Pos; double R = 0; int32 Horses = 0; };
	struct FPlanter { int32 Level = 0; V2 Pos; double R = 0; FString Tree; };
	struct FEntrance { FString Side; V2 At; double Width = 0; };
	struct FDome { double Rx = 0, Ry = 0, Rz = 0; V2 Pos; double Centre = 0; bool bCentre = false; double Cells = 0, Base = 0; FString Color; };
	struct FArch { FString Side; V2 At; double Width = 0, Height = 0; };

	struct FLotData
	{
		FString Id;
		double Rect[4] = { 0, 0, 0, 0 };
		double H = 18, St = 6;
		int32 Floors = 3;
		TArray<V2> Work;
		TArray<FLevel> Levels;
		TArray<FEntrance> Entrances;
		TArray<FHall> Halls;
		TArray<FRamp> Ramps;
		TArray<FLift> Lifts;
		TArray<FPlace> Fountains;   // (Pos, Yaw as r)
		TArray<FGarden> Gardens;
		TArray<FPlace> Benches;
		TArray<FPlanter> Planters;
		TArray<Poly> Openings;
		TMap<FString, FSpec> Outlets;
		bool bDepth = false;
		double Least = 4.2, Most = 9;
		TArray<FRide> Rides;
		TArray<TArray<FPlace>> Staff;
		TArray<FPlace> Customers, Seats;
		// the shell
		TArray<FVector4d> Skin;
		TArray<FString> SkinColors;
		double Roll = 0, Belly = 0, Bands = 0;
		TArray<FDome> Domes;
		bool bRoofLight = false;
		FDome RoofLight;
		TArray<FArch> Arches;
	};

	FDome ReadDome(const TSharedPtr<FJsonObject>& D)
	{
		FDome d;
		const TArray<TSharedPtr<FJsonValue>>& R = Arr(D, TEXT("radii"));
		d.Rx = R[0]->AsNumber(); d.Ry = R[1]->AsNumber(); d.Rz = R[2]->AsNumber();
		d.Pos = Pt2F(D, TEXT("pos"));
		d.bCentre = Has(D, TEXT("centre")); d.Centre = Num(D, TEXT("centre"));
		d.Cells = Num(D, TEXT("cells")); d.Base = Num(D, TEXT("base")); d.Color = Str(D, TEXT("color"));
		return d;
	}
	FPlace ReadPlace(const TSharedPtr<FJsonValue>& V) { const TSharedPtr<FJsonObject> O = V->AsObject(); FPlace p; p.Level = (int32)Num(O, TEXT("level")); p.Pos = Pt2F(O, TEXT("pos")); p.Yaw = Num(O, TEXT("yaw")); return p; }

	bool Read(const FJsonObject& LotObj, FLotData& L, FString& Err)
	{
		const TSharedPtr<FJsonObject> Lot = MakeShared<FJsonObject>(LotObj);
		const TSharedPtr<FJsonObject>* PP, * SP;
		if (!Lot->TryGetObjectField(TEXT("plan"), PP) || !Lot->TryGetObjectField(TEXT("shell"), SP)) { Err = TEXT("no plan or shell"); return false; }
		const TSharedPtr<FJsonObject> P = *PP, S = *SP;
		L.Id = Str(Lot, TEXT("id"));
		const TArray<TSharedPtr<FJsonValue>>& R = Arr(Lot, TEXT("rect"));
		for (int32 i = 0; i < 4; ++i) L.Rect[i] = R[i]->AsNumber();
		L.Floors = (int32)Num(Lot, TEXT("floors"), 0); if (!L.Floors) L.Floors = 3;
		L.St = Num(P, TEXT("storey_height"), 0); if (!L.St) L.St = 6;
		L.H = L.St * L.Floors;
		const TSharedPtr<FJsonObject>* Ob;
		if (Lot->TryGetObjectField(TEXT("objects"), Ob)) for (const TSharedPtr<FJsonValue>& w : Arr(*Ob, TEXT("work"))) L.Work.Add(Pt2(w));
		for (const TSharedPtr<FJsonValue>& LvV : Arr(P, TEXT("levels")))
		{
			const TSharedPtr<FJsonObject> Lv = LvV->AsObject();
			FLevel& V = L.Levels.AddDefaulted_GetRef();
			for (const TSharedPtr<FJsonValue>& UV : Arr(Lv, TEXT("units")))
			{
				const TSharedPtr<FJsonObject> U = UV->AsObject();
				FUnit& u = V.Units.AddDefaulted_GetRef();
				u.Id = Str(U, TEXT("id")); u.Name = Str(U, TEXT("name")); u.Kind = Str(U, TEXT("kind")); u.Outlet = Str(U, TEXT("outlet"));
				const TArray<TSharedPtr<FJsonValue>>& Rc = Arr(U, TEXT("rect"));
				for (int32 i = 0; i < 4 && i < Rc.Num(); ++i) u.Rect[i] = Rc[i]->AsNumber();
				u.Angle = Num(U, TEXT("angle"));
				u.bStand = Has(U, TEXT("stand"));
				for (const TSharedPtr<FJsonValue>& s : Arr(U, TEXT("stand"))) u.Stand.Add(Pt2(s));
			}
			V.Walks = Lines(Lv, TEXT("walks")); V.Voids = Lines(Lv, TEXT("voids")); V.Bridges = Lines(Lv, TEXT("bridges")); V.Rails = Lines(Lv, TEXT("rails"));
			for (const TSharedPtr<FJsonValue>& FV : Arr(Lv, TEXT("fronts"))) { const TSharedPtr<FJsonObject> Fo = FV->AsObject(); V.Fronts.Add({ Str(Fo, TEXT("unit")), Pt2F(Fo, TEXT("a")), Pt2F(Fo, TEXT("b")) }); }
			for (const TSharedPtr<FJsonValue>& WV : Arr(Lv, TEXT("walls"))) { const TSharedPtr<FJsonObject> Wo = WV->AsObject(); FWallLine w; w.Kind = Str(Wo, TEXT("kind")); for (const TSharedPtr<FJsonValue>& q : Arr(Wo, TEXT("line"))) w.Line.Add(Pt2(q)); V.Walls.Add(w); }
		}
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("entrances"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); L.Entrances.Add({ Str(O, TEXT("side")), Pt2F(O, TEXT("at")), Num(O, TEXT("width")) }); }
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("halls"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); L.Halls.Add({ Str(O, TEXT("unit")), Pt2F(O, TEXT("pos")), Num(O, TEXT("r")), (int32)Num(O, TEXT("dome")) }); }
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("ramps"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); L.Ramps.Add({ (int32)Num(O, TEXT("from")), (int32)Num(O, TEXT("to")), Pt2F(O, TEXT("a")), Pt2F(O, TEXT("b")), Num(O, TEXT("width")) }); }
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("lifts"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); L.Lifts.Add({ Pt2F(O, TEXT("pos")), Num(O, TEXT("r")), Num(O, TEXT("door")) }); }
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("fountains"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); FPlace f; f.Pos = Pt2F(O, TEXT("pos")); f.Yaw = Num(O, TEXT("r")); L.Fountains.Add(f); }
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("gardens")))
		{
			const TSharedPtr<FJsonObject> O = v->AsObject();
			FGarden g; g.Pos = Pt2F(O, TEXT("pos")); g.R0 = Arr(O, TEXT("r"))[0]->AsNumber(); g.R1 = Arr(O, TEXT("r"))[1]->AsNumber();
			bool b = false; g.bLow = O->TryGetBoolField(TEXT("low"), b) && b;
			for (const TSharedPtr<FJsonValue>& t : Arr(O, TEXT("trees"))) { const TArray<TSharedPtr<FJsonValue>>& T = t->AsArray(); g.Trees.Add({ T[0]->AsNumber(), T[1]->AsNumber(), T[2]->AsString() }); }
			L.Gardens.Add(g);
		}
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("benches"))) L.Benches.Add(ReadPlace(v));
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("planters"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); L.Planters.Add({ (int32)Num(O, TEXT("level")), Pt2F(O, TEXT("pos")), Num(O, TEXT("r")), Str(O, TEXT("tree")) }); }
		const TSharedPtr<FJsonObject>* Rf;
		if (P->TryGetObjectField(TEXT("roof"), Rf)) L.Openings = Lines(*Rf, TEXT("openings"));
		const TSharedPtr<FJsonObject>* Os;
		if (P->TryGetObjectField(TEXT("outlets"), Os))
			for (const TPair<FString, TSharedPtr<FJsonValue>>& K : (*Os)->Values)
			{
				const TSharedPtr<FJsonObject> O = K.Value->AsObject();
				FSpec s; s.Label = Str(O, TEXT("label")); s.Trade = Str(O, TEXT("trade")); s.Sign = Str(O, TEXT("sign")); s.Floor = Str(O, TEXT("floor"));
				s.Walls = Strs(O, TEXT("walls")); s.Middle = Strs(O, TEXT("middle")); s.Window = Strs(O, TEXT("window"));
				L.Outlets.Add(K.Key, s);
			}
		const TSharedPtr<FJsonObject>* Dp;
		if (P->TryGetObjectField(TEXT("outlet_depth"), Dp)) { L.bDepth = true; L.Least = Num(*Dp, TEXT("least")); L.Most = Num(*Dp, TEXT("most")); }
		for (const TSharedPtr<FJsonValue>& v : Arr(P, TEXT("rides"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); L.Rides.Add({ Str(O, TEXT("kind")), Pt2F(O, TEXT("pos")), Num(O, TEXT("r")), (int32)Num(O, TEXT("horses")) }); }
		const TSharedPtr<FJsonObject>* St;
		if (P->TryGetObjectField(TEXT("staff"), St))
			for (const TPair<FString, TSharedPtr<FJsonValue>>& K : (*St)->Values) { TArray<FPlace>& A = L.Staff.AddDefaulted_GetRef(); for (const TSharedPtr<FJsonValue>& q : K.Value->AsArray()) A.Add(ReadPlace(q)); }
		const TSharedPtr<FJsonObject>* Pl;
		if (P->TryGetObjectField(TEXT("places"), Pl))
		{
			for (const TSharedPtr<FJsonValue>& q : Arr(*Pl, TEXT("customer"))) L.Customers.Add(ReadPlace(q));
			for (const TSharedPtr<FJsonValue>& q : Arr(*Pl, TEXT("seat"))) L.Seats.Add(ReadPlace(q));
		}
		const TSharedPtr<FJsonObject>* Sk;
		if (S->TryGetObjectField(TEXT("skin"), Sk))
		{
			for (const TSharedPtr<FJsonValue>& q : Arr(*Sk, TEXT("points"))) { const TArray<TSharedPtr<FJsonValue>>& A = q->AsArray(); L.Skin.Add(FVector4d(A[0]->AsNumber(), A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber())); }
			L.SkinColors = Strs(*Sk, TEXT("colors")); L.Roll = Num(*Sk, TEXT("roll")); L.Belly = Num(*Sk, TEXT("belly")); L.Bands = Num(*Sk, TEXT("bands"));
		}
		for (const TSharedPtr<FJsonValue>& d : Arr(S, TEXT("domes"))) L.Domes.Add(ReadDome(d->AsObject()));
		const TSharedPtr<FJsonObject>* RL;
		if (S->TryGetObjectField(TEXT("roof_light"), RL)) { L.bRoofLight = true; L.RoofLight = ReadDome(*RL); }
		for (const TSharedPtr<FJsonValue>& v : Arr(S, TEXT("arches"))) { const TSharedPtr<FJsonObject> O = v->AsObject(); L.Arches.Add({ Str(O, TEXT("side")), Pt2F(O, TEXT("at")), Num(O, TEXT("width")), Num(O, TEXT("height")) }); }
		if (L.Levels.Num() < 2) { Err = TEXT("a court needs two levels or more"); return false; }
		return true;
	}

	// ---- the recorder: the slice's mesh and the lot's hooks
	struct FOut
	{
		FBrixMall& M;
		FOut(FBrixMall& InM) : M(InM) {}
		FBrixMallCall& New(FBrixMallCall::EKind K) { FBrixMallCall& c = M.Calls.AddDefaulted_GetRef(); c.Kind = K; return c; }
		void Quad(const V3& p0, const V3& p1, const V3& p2, const V3& p3, const V3& c, int32 f, const V3* hint = nullptr, bool uv = false)
		{
			FBrixMallCall& q = New(FBrixMallCall::Quad);
			for (const V3* p : { &p0, &p1, &p2, &p3 }) q.A.Append({ p->X, p->Y, p->Z });
			if (hint) q.A.Append({ hint->X, hint->Y, hint->Z, 1 }); else q.A.Append({ 0, 0, 0, 0 });
			q.A.Add(uv ? 1 : 0);
			q.Col = c; q.bCol = true; q.Mat = f;
		}
		void QuadH(const V3& p0, const V3& p1, const V3& p2, const V3& p3, const V3& c, int32 f, const V3& hint, bool uv = false) { Quad(p0, p1, p2, p3, c, f, &hint, uv); }
		void QuadN(const V3 (&p)[4], const V3 (&n)[4], const V3 (&cs)[4], int32 f)
		{
			FBrixMallCall& q = New(FBrixMallCall::QuadN);
			for (int32 i = 0; i < 4; ++i) q.A.Append({ p[i].X, p[i].Y, p[i].Z });
			for (int32 i = 0; i < 4; ++i) q.A.Append({ n[i].X, n[i].Y, n[i].Z });
			for (int32 i = 0; i < 4; ++i) q.A.Append({ cs[i].X, cs[i].Y, cs[i].Z });
			q.Mat = f;
		}
		void Box(double x0, double y0, double z0, double x1, double y1, double z1, const V3& c, int32 f, int32 Face = -1, const V3* FCol = nullptr, int32 FMat = 0)
		{
			FBrixMallCall& q = New(FBrixMallCall::Box);
			q.A.Append({ x0, y0, z0, x1, y1, z1 }); q.Col = c; q.bCol = true; q.Mat = f;
			if (Face >= 0) { TArray<double, TInlineAllocator<5>>& F = q.Faces.AddDefaulted_GetRef(); F.Add(Face); if (FCol) F.Append({ FCol->X, FCol->Y, FCol->Z, (double)FMat }); }
		}
		void YBox(double cx, double y0, double cz, double sx, double sy, double sz, double yaw, const V3& c, int32 f)
		{
			FBrixMallCall& q = New(FBrixMallCall::YBox);
			q.A.Append({ cx, y0, cz, sx, sy, sz, yaw }); q.Col = c; q.bCol = true; q.Mat = f;
		}
		void Solid(double x0, double y0, double z0, double x1, double y1, double z1) { New(FBrixMallCall::Solid).A.Append({ x0, y0, z0, x1, y1, z1 }); }
		void Use(double x, double y, double z, const FString& Kind, const FString& Label, double yaw, const V3* to = nullptr)
		{
			FBrixMallCall& q = New(FBrixMallCall::Use);
			q.A.Append({ x, y, z, yaw });
			if (to) q.A.Append({ to->X, to->Y, to->Z });
			q.Text = Kind + TEXT("|") + Label;
		}
		void Spot(double x, double z, double pose, double yaw, double h) { New(FBrixMallCall::Spot).A.Append({ x, z, pose, yaw, h }); }
		void Light(double x, double y, double z, double a, double b) { New(FBrixMallCall::Light).A.Append({ x, y, z, a, b }); }
		void Occl(double a, double b, double c, double d, double e, double f) { New(FBrixMallCall::Occl).A.Append({ a, b, c, d, e, f }); }
		void Name(const FString& Text, const V3& ctr, const V3& n, double h) { FBrixMallCall& q = New(FBrixMallCall::Name); q.A.Append({ ctr.X, ctr.Y, ctr.Z, n.X, n.Y, n.Z, h }); q.Text = Text; }
	};

	// a wall from a to b that faces `into` (court.js frame())
	struct FFrame
	{
		double L = 1, mx = 0, mz = 0;
		V2 A, t, n;
		V3 At(double u, double y, double d) const { return V3(A.X + t.X * u + n.X * d, y, A.Y + t.Y * u + n.Y * d); }
	};
	FFrame MakeFrame(const V2& a, const V2& b, TFunctionRef<bool(double, double)> Into)
	{
		FFrame S;
		const double Lh = Hyp(b.X - a.X, b.Y - a.Y);
		S.L = Lh != 0 ? Lh : 1; S.A = a;
		S.t = V2((b.X - a.X) / S.L, (b.Y - a.Y) / S.L);
		S.mx = (a.X + b.X) / 2; S.mz = (a.Y + b.Y) / 2;
		S.n = V2(-S.t.Y, S.t.X);
		if (!Into(S.mx + S.n.X * .8, S.mz + S.n.Y * .8)) S.n = -S.n;
		return S;
	}
	FFrame MakeFrameP(const V2& a, const V2& b, const V2& into)
	{
		FFrame S;
		const double Lh = Hyp(b.X - a.X, b.Y - a.Y);
		S.L = Lh != 0 ? Lh : 1; S.A = a;
		S.t = V2((b.X - a.X) / S.L, (b.Y - a.Y) / S.L);
		S.mx = (a.X + b.X) / 2; S.mz = (a.Y + b.Y) / 2;
		S.n = V2(-S.t.Y, S.t.X);
		if (!((into.X - S.mx) * S.n.X + (into.Y - S.mz) * S.n.Y > 0)) S.n = -S.n;
		return S;
	}

	const TCHAR* const TONES[] = { TEXT("ochre"), TEXT("cream"), TEXT("terracotta"), TEXT("teal"), TEXT("plaster") };
	int32 Tone(const FString& Name) { for (int32 i = 0; i < 5; ++i) if (Name == TONES[i]) return i; return 0; }
	const V3 PLASTER0(.68, .63, .55);
	V3 ShellTone(int32 i) { const V3 T[] = { Ochre, Cream, Terracotta, Teal, PLASTER0 }; return T[FMath::Clamp(i, 0, 4)]; }

	// ---- the mall's builder: the slice's court.js, shell.js and outlets.js on one lot
	struct FBuild
	{
		const FLotData& L;
		FOut& o;
		FBrixMall& M;
		const double T = .5, G = 2, DOOR_H = 5.2, RAIL = 1.1, STAIR_V = .7, F = .2;
		V3 Wall, Inside = V3(.74, .7, .62), FloorIn = V3(.74, .71, .64);
		V3 cream, plaster, floorC, shopCeil, dark, wood, fascia, ceilC, leafC = V3(.27, .37, .18);
		FBuild(const FLotData& InL, FOut& InO, FBrixMall& InM) : L(InL), o(InO), M(InM)
		{
			const V3 PL[] = { V3(.68, .63, .55), V3(.70, .58, .46), V3(.64, .6, .53), V3(.7, .6, .5), V3(.66, .56, .46), V3(.68, .63, .55) };
			Wall = PL[FMath::Min(5, (int32)FMath::FloorToDouble(H({ L.Id, TEXT("mall") }) * 6))];
			cream = Mul(Cream, 1.2); plaster = PLASTER0; floorC = Mul(FloorIn, 1.1); shopCeil = Mul(plaster, 1.14); dark = StoneDark; wood = Wood;
			fascia = Mul(Brown, .74); ceilC = Mul(Brown, .98);
		}

		// ---- shared shapes (court.js draw()'s helpers)
		void Flat(const V2& a, const V2& b, const V2& c, const V2& d, double y, const V3& col, int32 f, double up)
		{
			o.QuadH(V3(a.X, y, a.Y), V3(b.X, y, b.Y), V3(c.X, y, c.Y), V3(d.X, y, d.Y), col, f, V3(0, up, 0));
		}
		void WallQ(const FFrame& S, double u0, double u1, double y0, double y1, double d, const V3& col, int32 f, bool uv = false)
		{
			o.QuadH(S.At(u0, y0, d), S.At(u1, y0, d), S.At(u1, y1, d), S.At(u0, y1, d), col, f, V3(S.n.X, 0, S.n.Y), uv);
		}
		void BoxQ(const FFrame& S, double u0, double u1, double y0, double y1, double d0, double d1, const V3& col, int32 f)
		{
			WallQ(S, u0, u1, y0, y1, d1, col, f);
			o.QuadH(S.At(u0, y1, d0), S.At(u1, y1, d0), S.At(u1, y1, d1), S.At(u0, y1, d1), col, f, V3(0, 1, 0));
			o.QuadH(S.At(u0, y0, d0), S.At(u0, y0, d1), S.At(u0, y1, d1), S.At(u0, y1, d0), col, f, V3(-S.t.X, 0, -S.t.Y));
			o.QuadH(S.At(u1, y0, d0), S.At(u1, y0, d1), S.At(u1, y1, d1), S.At(u1, y1, d0), col, f, V3(S.t.X, 0, S.t.Y));
		}
		void SolidAlong(const V2& a, const V2& b, double y0, double y1, double r)
		{
			const double Ln = Hyp(b.X - a.X, b.Y - a.Y);
			const int32 n = FMath::Max(1, (int32)std::ceil(Ln / 1.1));
			for (int32 i = 0; i < n; ++i)
			{
				const double f = (i + .5) / n, x = a.X + (b.X - a.X) * f, z = a.Y + (b.Y - a.Y) * f;
				o.Solid(x - r, y0, z - r, x + r, y1, z + r);
			}
		}
		void Lantern(double x, double y, double z, double top, double s)
		{
			o.Box(x - .04, y + s, z - .04, x + .04, top, z + .04, Metal, 19);
			o.Box(x - s / 2, y, z - s / 2, x + s / 2, y + s, z + s / 2, BrixSlice::Lantern, 2, 2, &BrixSlice::Lantern, 2);
			M.Lamps.Add(FVector4d(x, y - .1, z, 30));
		}
		void Prism(const Poly& pts, double y0, double y1, const V3& colS, const V3& colT, int32 fS, int32 fT)
		{
			const TArray<V2> out = Outward(pts);
			const int32 n = pts.Num();
			double sx = 0, sz = 0;
			for (const V2& p : pts) sx += p.X;
			for (const V2& p : pts) sz += p.Y;
			const double cx = sx / n, cz = sz / n;
			for (int32 i = 0; i < n; ++i)
			{
				const V2& a = pts[i]; const V2& b = pts[(i + 1) % n];
				o.QuadH(V3(a.X, y0, a.Y), V3(b.X, y0, b.Y), V3(b.X, y1, b.Y), V3(a.X, y1, a.Y), colS, fS, V3(out[i].X + out[(i + 1) % n].X, 0, out[i].Y + out[(i + 1) % n].Y));
				o.QuadH(V3(cx, y1, cz), V3(a.X, y1, a.Y), V3(b.X, y1, b.Y), V3(cx, y1, cz), colT, fT, V3(0, 1, 0));
			}
		}
		static Poly Round(double x, double z, double rx, double rz, int32 n)
		{
			Poly p;
			for (int32 i = 0; i < n; ++i) p.Add(V2(x + rx * std::cos(6.2832 * i / n), z + rz * std::sin(6.2832 * i / n)));
			return p;
		}
		void Blob(double x, double y, double z, double r, const V3& col, const FString& seed)   // a tree's crown: a faceted ball, each face its own tone
		{
			const int32 A = 7, B = 4;
			auto q = [&](int32 ii, int32 jj)
			{
				const double ph = 6.2832 * ii / A + jj * .4, th = PI_ * jj / B, rr = r * (.86 + .28 * H({ seed, J(ii % A), J(jj) }));
				return V3(x + rr * std::sin(th) * std::cos(ph), y + r * .85 * std::cos(th), z + rr * std::sin(th) * std::sin(ph));
			};
			for (int32 j = 0; j < B; ++j)
				for (int32 i = 0; i < A; ++i)
				{
					const V3 p0 = q(i, j), p1 = q(i + 1, j), p2 = q(i + 1, j + 1), p3 = q(i, j + 1);
					const V3 hc((p0.X + p2.X) / 2 - x, (p0.Y + p2.Y) / 2 - y, (p0.Z + p2.Z) / 2 - z);
					const V3 tone = Mul(col, .8 + .4 * H({ seed, J(i), J(j), TEXT("c") }));
					if (j) o.QuadH(p0, p1, p2, p3, tone, 19, hc); else o.QuadH(p3, p2, p1, p0, tone, 19, hc);
				}
		}
		void Tree(double x, double z, double y, const FString& species, const FString& seed, double size = 0)
		{
			const double k = size != 0 ? size : 1;
			const bool birch = species == TEXT("birch");
			const double h = (birch ? 5.2 : 4.2) * k, r = (birch ? 2.1 : 2.9) * k;
			const V3 leaf = birch ? V3(.33, .4, .2) : V3(.24, .33, .17);
			o.Box(x - .16, y, z - .16, x + .16, y + h, z + .16, birch ? V3(.6, .58, .52) : Brown, 19);
			o.Solid(x - .2, y, z - .2, x + .2, y + h, z + .2);
			Blob(x, y + h + r * .35, z, r, leaf, seed);
			Blob(x + r * .5, y + h - .3, z + r * .3, r * .62, leaf, seed + TEXT("b"));
			Blob(x - r * .45, y + h - .1, z - r * .35, r * .58, leaf, seed + TEXT("c"));
		}

		// ---- what is floor on level k (court.js region())
		struct FRegion
		{
			const FBuild* B = nullptr;
			int32 k = 0;
			TArray<TPair<const Poly*, FVector4d>> walks, voids;
			bool Walk(double x, double z) const
			{
				for (const TPair<const Poly*, FVector4d>& w : walks) { const FVector4d& b = w.Value; if (x > b.X && x < b.Z && z > b.Y && z < b.W && InPoly(*w.Key, x, z)) return true; }
				return false;
			}
			bool Hole(double x, double z, double r) const
			{
				for (const TPair<const Poly*, FVector4d>& v : voids)
				{
					const FVector4d& b = v.Value;
					if (x > b.X - r && x < b.Z + r && z > b.Y - r && z < b.W + r && (InPoly(*v.Key, x, z) || (r > 0 && EdgeDist(*v.Key, x, z) < r))) return true;
				}
				return false;
			}
			bool InHall(double x, double z, double r) const { for (const FHall& h : B->L.Halls) if (Hyp(x - h.Pos.X, z - h.Pos.Y) < h.R + r) return true; return false; }
			bool Cell(double x, double z) const
			{
				return !Hole(x, z, 1.4) && !(k && InHall(x, z, 1.0)) && (Walk(x, z) || (!(k && InHall(x, z, .5)) && (Walk(x - 1, z - 1) || Walk(x + 1, z - 1) || Walk(x - 1, z + 1) || Walk(x + 1, z + 1))));
			}
			bool On(double x, double z) const { return Walk(x, z) && !Hole(x, z, 0); }
		};
		FRegion Region(int32 k) const
		{
			FRegion R; R.B = this; R.k = k;
			for (const Poly& p : L.Levels[k].Walks) R.walks.Add({ &p, BoxOf(p) });
			for (const Poly& p : L.Levels[k].Voids) R.voids.Add({ &p, BoxOf(p) });
			return R;
		}
		void Raster(TFunctionRef<bool(double, double)> Cell, TFunctionRef<void(double, double, double, double)> Emit) const   // runs of 2 m squares along x, row by row
		{
			const double x0 = L.Rect[0], z0 = L.Rect[1], x1 = L.Rect[2], z1 = L.Rect[3];
			for (double z = z0; z < z1 - .01; z += G)
			{
				double run = 0; bool bRun = false;
				for (double x = x0; x <= x1 + .01; x += G)
				{
					const bool on = x < x1 - .01 && Cell(x + G / 2, z + G / 2);
					if (on && !bRun) { run = x; bRun = true; }
					if (!on && bRun)
					{
						for (double a = run, e = FMath::Min(x, x1); a < e - .01; a += 6) Emit(a, z, FMath::Min(a + 6, e), FMath::Min(z + G, z1));
						bRun = false;
					}
				}
			}
		}

		// ---- the hall's own wall (court.js hall())
		void Hall()
		{
			const double x0 = L.Rect[0], z0 = L.Rect[1], x1 = L.Rect[2], z1 = L.Rect[3], Hh = L.H, st = L.St, t = .3;
			struct FSide { const TCHAR* s; double fix, a0, a1, out; };
			const FSide Sides[] = { { TEXT("n"), z0, x0, x1, -1 }, { TEXT("s"), z1, x0, x1, 1 }, { TEXT("w"), x0, z0, z1, -1 }, { TEXT("e"), x1, z0, z1, 1 } };
			for (const FSide& Sd : Sides)
			{
				const FString s = Sd.s;
				const double fix = Sd.fix, a0 = Sd.a0, a1 = Sd.a1, out = Sd.out;
				const bool ns = s == TEXT("n") || s == TEXT("s");
				const FEntrance* e = L.Entrances.FindByPredicate([&](const FEntrance& q) { return q.Side == s; });
				const double c = e ? (ns ? e->At.X : e->At.Y) : 0, hw = e ? e->Width / 2 : 0;
				const FArch* arch = L.Arches.FindByPredicate([&](const FArch& q) { return q.Side == s; });
				auto at = [&](double u, double y, double d) { return ns ? V3(u, y, fix + d * out) : V3(fix + d * out, y, u); };
				auto dir = [&](double d) { return ns ? V3(0, 0, d * out) : V3(d * out, 0, 0); };
				auto face = [&](double u0, double u1, double y0, double y1, double d, const V3& col, int32 f, double way, bool uv)
				{
					o.QuadH(at(u0, y0, d), at(u1, y0, d), at(u1, y1, d), at(u0, y1, d), col, f, dir(way), uv);
				};
				auto piece = [&](double u0, double u1, double y0, double y1)
				{
					if (u1 - u0 < .01) return;
					if (y0 < .6) face(u0, u1, y0, .6, .3, Stone, 6, 1, false);
					face(u0, u1, FMath::Max(y0, .6), y1, .2, Wall, 0, 1, false);
					face(u0, u1, FMath::Max(y0, F), y1, -.2, Inside, 3, -1, false);
					if (ns) o.Solid(u0, y0, fix - t, u1, y1, fix + t); else o.Solid(fix - t, y0, u0, fix + t, y1, u1);
				};
				if (!e) { piece(a0, a1, 0, Hh); continue; }
				piece(a0, c - hw, 0, Hh); piece(c + hw, a1, 0, Hh);
				const double kz = Tone(L.SkinColors.Num() ? L.SkinColors[0] : FString()) + 8 * Tone(L.SkinColors.Num() > 1 ? L.SkinColors[1] : (L.SkinColors.Num() ? L.SkinColors[0] : FString()));
				const V3 glow(.3, .4, .97);
				const double SPRING = 3.3;
				const int32 N = 14;
				const double ew = e->Width;
				auto openTop = [&](double u) { return SPRING + (DOOR_H + .1 - SPRING) * std::sqrt(FMath::Max(0.0, 1 - 4 * (u - c) * (u - c) / (ew * ew))); };
				TArray<V3> lift;   // the skin where it is lifted over this arch: along the wall, out from it, how high
				if (arch)
				{
					for (const FVector4d& q : L.Skin)
					{
						if (!(q.Z > .02)) continue;
						const V3 p = ns ? V3(q.X, (q.Y - fix) * out, q.Z) : V3(q.Y, (q.X - fix) * out, q.Z);
						if (p.Y > 0 && std::fabs(p.X - c) < arch->Width / 2 + 6) lift.Add(p);
					}
					lift.StableSort([](const V3& p, const V3& q) { return p.X < q.X; });
				}
				auto roofAt = [&](double u)
				{
					const int32 n = lift.Num();
					if (!n) return Hh;
					if (u <= lift[0].X) return lift[0].Z;
					if (u >= lift[n - 1].X) return lift[n - 1].Z;
					for (int32 i = 1; i < n; ++i)
						if (u <= lift[i].X) { const V3& p = lift[i - 1]; const V3& q = lift[i]; const double dd = q.X - p.X; return p.Z + (q.Z - p.Z) * (u - p.X) / (dd != 0 ? dd : 1); }
					return Hh;
				};
				auto up = [&](double u) { return FMath::Min(Hh, roofAt(u)) + .05; };
				auto band = [&](double y) { return V3(y / 64, 0, kz); };
				for (int32 i = 0; i < N; ++i)
				{
					const double u0 = c - hw + ew * i / N, u1 = c - hw + ew * (i + 1) / N, y0 = openTop(u0), y1 = openTop(u1), r0 = up(u0), r1 = up(u1);
					o.QuadH(at(u0, y0, -.2), at(u1, y1, -.2), at(u1, Hh, -.2), at(u0, Hh, -.2), Inside, 3, dir(-1));
					const V3 d1 = dir(1);
					o.QuadN({ at(u0, y0, .3), at(u1, y1, .3), at(u1, r1, .3), at(u0, r0, .3) }, { d1, d1, d1, d1 }, { band(y0), band(y1), band(r1), band(r0) }, 26);
					o.QuadH(at(u0, y0, -.2), at(u1, y1, -.2), at(u1, y1, .3), at(u0, y0, .3), StoneDark, 3, V3(0, -1, 0));
					o.QuadH(at(u0, y0, .33), at(u1, y1, .33), at(u1, y1 + .22, .33), at(u0, y0 + .22, .33), glow, 1, dir(1));
					const double ym = FMath::Min(y0, y1);
					if (ns) o.Solid(u0, ym, fix - t, u1, Hh, fix + t); else o.Solid(fix - t, ym, u0, fix + t, Hh, u1);
				}
				for (const double sg : { -1.0, 1.0 })
				{
					const double u = c + sg * hw;
					const V3 inw = ns ? V3(-sg, 0, 0) : V3(0, 0, -sg);
					o.QuadH(at(u, 0, -.2), at(u, 0, .3), at(u, SPRING, .3), at(u, SPRING, -.2), StoneDark, 3, inw);
					o.QuadH(at(u - sg * .02, F, .06), at(u - sg * .02, F, .28), at(u - sg * .02, SPRING, .28), at(u - sg * .02, SPRING, .06), glow, 1, inw);
				}
				o.QuadH(at(c - hw, F, -.2), at(c + hw, F, -.2), at(c + hw, F, .32), at(c - hw, F, .32), FloorIn, 16, V3(0, 1, 0));
				if (lift.Num() > 2)
					for (const double sg : { -1.0, 1.0 })   // the two ribbon walls: from the opening's side straight out, then round to where the arch comes down
					{
						const V3 end = sg < 0 ? lift[0] : lift.Last();
						const int32 Ms = 18;
						TArray<V3> pts;
						for (int32 i = 0; i <= Ms; ++i)
						{
							const double a = PI_ / 2 * i / Ms, u = c + sg * hw + (end.X - c - sg * hw) * (1 - std::cos(a));
							pts.Add(V3(u, .3 + (end.Y - .3) * std::sin(a), FMath::Max(.05, up(u))));
						}
						TArray<V3> nrm;
						for (int32 i = 0; i <= Ms; ++i)
						{
							const V3& a = pts[FMath::Max(0, i - 1)]; const V3& b = pts[FMath::Min(Ms, i + 1)];
							const double Lh = Hyp(b.X - a.X, b.Y - a.Y), Ln = Lh != 0 ? Lh : 1;
							double nu = -(b.Y - a.Y) / Ln, nd = (b.X - a.X) / Ln;
							if (sg < 0) { nu = -nu; nd = -nd; }
							nrm.Add(ns ? V3(nu, 0, nd * out) : V3(nd * out, 0, nu));
						}
						for (int32 i = 0; i < Ms; ++i)
						{
							const V3& p = pts[i]; const V3& q = pts[i + 1];
							o.QuadN({ at(p.X, 0, p.Y), at(q.X, 0, q.Y), at(q.X, q.Z, q.Y), at(p.X, p.Z, p.Y) }, { nrm[i], nrm[i + 1], nrm[i + 1], nrm[i] }, { band(0), band(0), band(q.Z), band(p.Z) }, 26);
							const int32 k2 = FMath::Max(1, (int32)std::ceil(Hyp(q.X - p.X, q.Y - p.Y) / 1.0));
							for (int32 j = 0; j < k2; ++j)
							{
								const double f = (j + .5) / k2, top = p.Z + (q.Z - p.Z) * f;
								const V3 w = at(p.X + (q.X - p.X) * f, 0, p.Y + (q.Y - p.Y) * f);
								if (top > .4) o.Solid(w.X - .6, 0, w.Z - .6, w.X + .6, top, w.Z + .6);
							}
						}
					}
				for (int32 fl = 1; fl < L.Floors; ++fl)
					for (const double sg : { -1.0, 1.0 }) face(c + sg * 3.4 - 2.6, c + sg * 3.4 + 2.6, fl * st + 1, fl * st + 4.4, -.23, V3(0, 0, .97), 1, -1, true);
			}
		}

		// ---- the shell (shell.js)
		void Skin()
		{
			const int32 SEC = 12, n = L.Skin.Num();
			const double x0 = L.Rect[0], z0 = L.Rect[1], x1 = L.Rect[2], z1 = L.Rect[3];
			const FString c0 = L.SkinColors.Num() ? L.SkinColors[0] : FString(), c1 = L.SkinColors.Num() > 1 ? L.SkinColors[1] : c0;
			const double kz = Tone(c0) + 8 * Tone(c1);
			const V3 base = ShellTone(Tone(c0));
			TArray<TArray<V3>> grid; TArray<V2> hall;
			for (int32 i = 0; i < n; ++i)
			{
				const double x = L.Skin[i].X, z = L.Skin[i].Y, B = L.Skin[i].Z, Tp = L.Skin[i].W;
				const double qx = Clamp(x, x0, x1), qz = Clamp(z, z0, z1), ch = Hyp(qx - x, qz - z), c = ch != 0 ? ch : 1, ex = (qx - x) / c, ez = (qz - z) / c;
				const double h = Tp - B, roll = FMath::Min3(L.Roll, .75 * c, .8 * h), L1 = h - roll, L2 = roll * PI_ / 2;
				TArray<V3>& row = grid.AddDefaulted_GetRef();
				for (int32 j = 0; j <= SEC; ++j)
				{
					const double a = (double)j / SEC * (L1 + L2);
					double y, off;
					if (a <= L1) { const double s = std::sin(PI_ * a / L1); y = B + a; off = -L.Belly * s * s * FMath::Min(1.0, h / 12); }
					else { const double th = (a - L1) / roll; y = B + L1 + roll * std::sin(th); off = roll * (1 - std::cos(th)); }
					row.Add(V3(x + ex * off, y, z + ez * off));
				}
				hall.Add(V2(qx, qz));
			}
			auto norm = [](const V3& v) { const double l = Hyp3(v.X, v.Y, v.Z), d = l != 0 ? l : 1; return V3(v.X / d, v.Y / d, v.Z / d); };
			auto cross = [](const V3& a, const V3& b) { return V3(a.Y * b.Z - a.Z * b.Y, a.Z * b.X - a.X * b.Z, a.X * b.Y - a.Y * b.X); };
			TArray<TArray<V3>> nrm;
			for (int32 i = 0; i < n; ++i)
			{
				TArray<V3>& rn = nrm.AddDefaulted_GetRef();
				for (int32 j = 0; j <= SEC; ++j)
				{
					const V3 v = norm(cross(grid[i][FMath::Min(SEC, j + 1)] - grid[i][FMath::Max(0, j - 1)], grid[(i + 1) % n][j] - grid[(i + n - 1) % n][j]));
					rn.Add(v.X * (L.Skin[i].X - hall[i].X) + v.Z * (L.Skin[i].Y - hall[i].Y) + v.Y * 4 < 0 ? -v : v);
				}
			}
			const V3 soffit = Mul(base, .78), lid = Mul(base, .86);
			const double Ht = L.H + .35;
			for (int32 i = 0; i < n; ++i)
			{
				const int32 i1 = (i + 1) % n;
				const FVector4d& A = L.Skin[i]; const FVector4d& Bp = L.Skin[i1];
				const V2& qa = hall[i]; const V2& qb = hall[i1];
				for (int32 j = 0; j < SEC; ++j)
				{
					const V3 ca((double)j / SEC * L.Bands / 64, 0, kz), cb((double)(j + 1) / SEC * L.Bands / 64, 0, kz);
					o.QuadN({ grid[i][j], grid[i1][j], grid[i1][j + 1], grid[i][j + 1] }, { nrm[i][j], nrm[i1][j], nrm[i1][j + 1], nrm[i][j + 1] }, { ca, ca, cb, cb }, 26);
					o.QuadN({ grid[i][j], grid[i1][j], grid[i1][j + 1], grid[i][j + 1] }, { -nrm[i][j], -nrm[i1][j], -nrm[i1][j + 1], -nrm[i][j + 1] }, { soffit, soffit, soffit, soffit }, 0);
				}
				o.QuadH(grid[i][SEC], grid[i1][SEC], V3(qb.X, Ht, qb.Y), V3(qa.X, Ht, qa.Y), lid, 0, V3(0, 1, 0));
				o.QuadH(grid[i][SEC], grid[i1][SEC], V3(qb.X, Ht, qb.Y), V3(qa.X, Ht, qa.Y), soffit, 0, V3(0, -1, 0));
				if (A.Z > .02 || Bp.Z > .02)
				{
					const V3 p0(A.X, A.Z, A.Y), p1(Bp.X, Bp.Z, Bp.Y), p2(qb.X, Bp.Z, qb.Y), p3(qa.X, A.Z, qa.Y);
					o.Quad(p0, p1, p2, p3, soffit, 0); o.Quad(p3, p2, p1, p0, soffit, 0);
				}
				const double len = Hyp(Bp.X - A.X, Bp.Y - A.Y);
				const int32 k = FMath::Max(1, (int32)std::ceil(len / 1.2));
				for (int32 s = 0; s < k; ++s)
				{
					const double f = (s + .5) / k, x = A.X + (Bp.X - A.X) * f, z = A.Y + (Bp.Y - A.Y) * f, b = A.Z + (Bp.Z - A.Z) * f, tp = A.W + (Bp.W - A.W) * f;
					if (b > 2.6) continue;
					o.Solid(x - .7, 0, z - .7, x + .7, tp, z + .7);
					if (b > .02)
					{
						const double qx = Clamp(x, x0, x1), qz = Clamp(z, z0, z1), d = Hyp(qx - x, qz - z);
						for (double r = 1.2; r < d; r += 1.2) o.Solid(x + (qx - x) * r / d - .7, 0, z + (qz - z) * r / d - .7, x + (qx - x) * r / d + .7, tp, z + (qz - z) * r / d + .7);
					}
				}
			}
			double mnx = 1e300, mnz = 1e300, mxx = -1e300, mxz = -1e300;
			for (const FVector4d& p : L.Skin) { mnx = FMath::Min(mnx, p.X); mnz = FMath::Min(mnz, p.Y); mxx = FMath::Max(mxx, p.X); mxz = FMath::Max(mxz, p.Y); }
			o.Occl(mnx + 3, mnz + 3, mxx - 3, mxz - 3, L.H + 1, 0);
		}
		void Egg(const FDome& d, double base, bool inside)   // shell.js egg()
		{
			const double rx = d.Rx, ry = d.Ry, rz = d.Rz, px = d.Pos.X, pz = d.Pos.Y, cy = d.bCentre ? d.Centre : base, kz = d.Cells + 1000 * Tone(d.Color);
			const double end = std::acos(Clamp((base - cy) / ry, -1, 1));
			const int32 RINGS = FMath::Max(10, (int32)JsRound(30 * end / PI_ * 1.6)), SEG = 80;
			const double cell = PI_ * (rx + rz) / d.Cells;
			TArray<V3> rows;
			double arc = 0;
			for (int32 r = 0; r <= RINGS; ++r)
			{
				const double a = end * r / RINGS, sp = std::sin(a), cp = std::cos(a);
				if (r) { const double a0 = end * (r - 1) / RINGS; arc += Hyp((rx + rz) / 2 * (sp - std::sin(a0)), ry * (cp - std::cos(a0))); }
				rows.Add(V3(sp, cp, arc / cell / d.Cells));
			}
			for (int32 r = 0; r < RINGS; ++r)
				for (int32 s = 0; s < SEG; ++s)
				{
					V3 P[4], N[4], K[4];
					const int32 RR[4][2] = { { r + 1, s }, { r + 1, s + 1 }, { r, s + 1 }, { r, s } };
					for (int32 q = 0; q < 4; ++q)
					{
						const double sp = rows[RR[q][0]].X, cp = rows[RR[q][0]].Y, v = rows[RR[q][0]].Z, ss = RR[q][1];
						const double ph = 2 * PI_ * ss / SEG, c = std::cos(ph), sn = std::sin(ph);
						const V3 nv(sp * c / rx, cp / ry, sp * sn / rz);
						const double l = Hyp3(nv.X, nv.Y, nv.Z), dl = l != 0 ? l : 1;
						const V3 nn(nv.X / dl, nv.Y / dl, nv.Z / dl);
						P[q] = V3(px + rx * sp * c, cy + ry * cp, pz + rz * sp * sn); N[q] = inside ? -nn : nn; K[q] = V3(ss / SEG, v, kz);
					}
					o.QuadN(P, N, K, inside ? 27 : 25);
				}
		}
		void ShellLanterns()
		{
			for (const FArch& a : L.Arches)
			{
				const V2 out = a.Side == TEXT("n") ? V2(0, -1) : a.Side == TEXT("s") ? V2(0, 1) : a.Side == TEXT("w") ? V2(-1, 0) : V2(1, 0);
				const V2 along(std::fabs(out.Y), std::fabs(out.X));
				const FEntrance* way = L.Entrances.FindByPredicate([&](const FEntrance& q) { return q.Side == a.Side; });
				const double k = way ? .3 * way->Width / a.Width : .27;
				for (const double f : { -k, 0.0, k })
				{
					const double x = a.At.X + out.X * 7 + along.X * f * a.Width, z = a.At.Y + out.Y * 7 + along.Y * f * a.Width, top = a.Height * std::sqrt(1 - 4 * f * f), y = FMath::Max(4.2, top - 2.6);
					o.Box(x - .05, y + .7, z - .05, x + .05, top, z + .05, Metal, 9);
					o.Box(x - .34, y, z - .34, x + .34, y + .7, z + .34, BrixSlice::Lantern, 2);
					o.Box(x - .42, y + .7, z - .42, x + .42, y + .82, z + .42, Metal, 9);
					o.Light(x, y - .2, z, 2.4, 1);
				}
			}
		}

		void Front(const FFrame& S, const FUnit& u, double fy, double top, int32 k, bool staffed, const FSpec* InSpec);
		void Ride(const FRide& q);
		void Horse(double x, double z, double y, double yaw, const V3& col, const FString& label);
		void Draw();
	};

	// ---- outlets.js
	const V3 SIGN_terracotta(.5, .25, .19), SIGN_ochre(.56, .43, .2), SIGN_teal(.2, .37, .38), SIGN_blue(.27, .36, .52), SIGN_olive(.33, .37, .2), SIGN_plum(.35, .25, .38);
	V3 SignOf(const FString& s)
	{
		if (s == TEXT("terracotta")) return SIGN_terracotta;
		if (s == TEXT("teal")) return SIGN_teal;
		if (s == TEXT("blue")) return SIGN_blue;
		if (s == TEXT("olive")) return SIGN_olive;
		if (s == TEXT("plum")) return SIGN_plum;
		return SIGN_ochre;
	}
	const V3 GOODS[] = { V3(.56, .26, .2), V3(.6, .46, .2), V3(.2, .4, .42), V3(.28, .38, .58), V3(.36, .42, .22), V3(.4, .27, .42), V3(.78, .72, .6), V3(.7, .36, .3), V3(.24, .24, .27), V3(.8, .62, .3) };
	const int32 NGOODS = 10;
	const V3 SCREEN(.3, .4, .9995), GLOW(.3, .4, .97), DARK(.14, .14, .16), BROWN(.5, .36, .22), LEAF(.27, .37, .18);

	void FBuild::Front(const FFrame& S, const FUnit& u, double fy, double top, int32 k, bool staffed, const FSpec* InSpec)
	{
		FSpec Fallback;
		Fallback.Label = TEXT("Shop"); Fallback.Sign = TEXT("ochre"); Fallback.Floor = TEXT("tiles");
		Fallback.Walls = { TEXT("shelves:boxes"), TEXT("shelves:boxes"), TEXT("shelves:boxes") }; Fallback.Middle = { TEXT("table:boxes") }; Fallback.Window = { TEXT("stack:boxes"), TEXT("plants") };
		const FSpec& spec = InSpec ? *InSpec : Fallback;
		const V3 sign = SignOf(spec.Sign);
		const FString seed = u.Id;
		const double Lf = S.L;
		const double th = std::atan2(-S.t.Y, S.t.X), turn = (-S.n.X * std::sin(th) - S.n.Y * std::cos(th)) > 0 ? 1 : -1;
		const double least = L.bDepth ? L.Least : 4.2, most = L.bDepth ? L.Most : 9;
		const double D = FMath::Max(least, FMath::Min(most, u.Rect[3] - u.Rect[1] - .2));
		const double a0 = .9, a1 = Lf - .9, mid = Lf / 2, door = FMath::Min(3.4, (a1 - a0) * .28);
		const bool food = spec.Trade == TEXT("food");
		{
			const V3 p0 = S.At(0, 0, 0);
			FBrixOutletRoom& Rm = M.Rooms.AddDefaulted_GetRef();
			Rm.Id = u.Id; Rm.Name = u.Name.IsEmpty() ? spec.Label : u.Name; Rm.Kind = u.Outlet; Rm.Level = k; Rm.Fy = fy; Rm.X = p0.X; Rm.Z = p0.Z; Rm.T = S.t; Rm.N = S.n; Rm.A0 = a0; Rm.A1 = a1; Rm.D = D;
		}
		auto P = [&](double a, double y, double v) { return S.At(a, y, -v); };
		auto xz = [&](double a, double v) { const V3 p = P(a, 0, v); return V2(p.X, p.Z); };
		auto good = [&](std::initializer_list<FString> q)
		{
			TArray<FString> Parts; Parts.Add(seed); Parts.Append(q.begin(), (int32)q.size());
			return GOODS[(int32)FMath::FloorToDouble(HashOf(Parts) * NGOODS) % NGOODS];
		};
		auto rnd = [&](std::initializer_list<FString> q) { TArray<FString> Parts; Parts.Add(seed); Parts.Append(q.begin(), (int32)q.size()); return HashOf(Parts); };
		auto bx = [&](double a, double v, double y0, double sa, double sy, double sv, const V3& col, int32 f = 19, double yaw = 0)
		{
			const V3 p = P(a, 0, v);
			o.YBox(p.X, y0, p.Z, sa, sy, sv, th - turn * yaw, col, f);
		};
		auto big = [&](double a, double v, double y, double r, const V3& col, const FString& key) { const V3 p = P(a, 0, v); Blob(p.X, y, p.Z, r, col, seed + key); };
		auto ball = [&](double a, double v, double y, double r, const V3& col, const FString& key)
		{
			const V3 p = P(a, 0, v);
			auto q = [&](int32 i, int32 j) { const double g = 1.0472 * i + j * .5, tt = j == 0 ? 0 : j == 1 ? 1.05 : j == 2 ? 2.1 : 3.1416; return V3(p.X + r * std::sin(tt) * std::cos(g), y + r * std::cos(tt), p.Z + r * std::sin(tt) * std::sin(g)); };
			for (int32 j = 0; j < 3; ++j)
				for (int32 i = 0; i < 6; ++i)
				{
					const V3 c = Mul(col, .85 + .3 * rnd({ key, J(i), J(j) })), A = q(i, j), B = q(i + 1, j), Cc = q(i + 1, j + 1), Dd = q(i, j + 1);
					const V3 n((A.X + Cc.X) / 2 - p.X, (A.Y + Cc.Y) / 2 - y, (A.Z + Cc.Z) / 2 - p.Z);
					if (j) o.QuadH(A, B, Cc, Dd, c, 19, n); else o.QuadH(Dd, Cc, B, A, c, 19, n);
				}
		};
		auto disc = [&](double a, double v, double y0, double y1, double r, const V3& colS, const V3* colT = nullptr, int32 n = 10)
		{
			const V3 p = P(a, 0, v);
			Prism(Round(p.X, p.Z, r, r, n), y0, y1, colS, colT ? *colT : colS, 19, 19);
		};
		auto block = [&](double a, double v, double r, double h) { const V3 p = P(a, 0, v); o.Solid(p.X - r, fy, p.Z - r, p.X + r, fy + h, p.Z + r); };
		auto quad = [&](const V3& p0, const V3& p1, const V3& p2, const V3& p3, const V3& col, int32 f, const V3& n) { o.QuadH(p0, p1, p2, p3, col, f, n); };
		const V3 along(S.t.X, 0, S.t.Y), face(S.n.X, 0, S.n.Y);
		struct FW { double a0, v0, da, dv, na, nv, len; bool side; };
		auto wb = [&](const FW& W, double q, double r, double y0, double len, double sy, double dep, const V3& col, int32 f = 19)
		{
			const double a = W.a0 + W.da * q + W.na * r, v = W.v0 + W.dv * q + W.nv * r;
			if (W.side) bx(a, v, y0, dep, sy, len, col, f); else bx(a, v, y0, len, sy, dep, col, f);
		};
		auto wp = [&](const FW& W, double q, double r) { return V2(W.a0 + W.da * q + W.na * r, W.v0 + W.dv * q + W.nv * r); };

		// ---- the room: floor, walls, ceiling and its light panels
		const V3 wallC(.95, .92, .85), backC = Mix(sign, V3(.92, .88, .78), .5), ceilCol(.93, .91, .85);
		const bool boards = spec.Floor == TEXT("boards");
		const V3 flC = boards ? Mul(Wood, 1.25) : Mul(floorC, .97);
		const int32 flM = boards ? 15 : 16;
		quad(P(a0, fy + .004, 0), P(a1, fy + .004, 0), P(a1, fy + .004, D), P(a0, fy + .004, D), flC, flM, V3(0, 1, 0));
		quad(P(a0, fy, 0), P(a0, fy, D), P(a0, fy + 3.5, D), P(a0, fy + 3.5, 0), wallC, 3, along);
		quad(P(a1, fy, 0), P(a1, fy, D), P(a1, fy + 3.5, D), P(a1, fy + 3.5, 0), wallC, 3, Mul(along, -1));
		quad(P(a0, fy, D), P(a1, fy, D), P(a1, fy + 3.5, D), P(a0, fy + 3.5, D), backC, 3, face);
		quad(P(a0, fy + 3.5, 0), P(a1, fy + 3.5, 0), P(a1, fy + 3.5, D), P(a0, fy + 3.5, D), ceilCol, 3, V3(0, -1, 0));
		quad(P(a0 + .4, fy + 3.12, D - .03), P(a1 - .4, fy + 3.12, D - .03), P(a1 - .4, fy + 3.3, D - .03), P(a0 + .4, fy + 3.3, D - .03), GLOW, 1, face);
		for (const double v : { D * .32, D * .72 })
			quad(P(mid - (a1 - a0) * .3, fy + 3.49, v - .3), P(mid + (a1 - a0) * .3, fy + 3.49, v - .3), P(mid + (a1 - a0) * .3, fy + 3.49, v + .3), P(mid - (a1 - a0) * .3, fy + 3.49, v + .3), SCREEN, 1, V3(0, -1, 0));
		{ const V3 p = P(mid, 0, D * .45); M.Lamps.Add(FVector4d(p.X, fy + 3.3, p.Z, -26)); }
		SolidAlong(xz(a0, D), xz(a1, D), fy, top, .45);
		for (const double a : { a0, a1 }) SolidAlong(xz(a, .5), xz(a, D), fy, top, .45);
		if (k) for (double a = a0 + .7; a < a1; a += 1.4) for (double v = .7; v < D; v += 1.4) { const V3 p = P(a, 0, v); o.Solid(p.X - .95, fy - T, p.Z - .95, p.X + .95, fy, p.Z + .95); }

		// ---- the front: piers, the sign band and the name on it, posts, a display plinth either side of the way in
		BoxQ(S, 0, .9, fy, top, 0, .18, plaster, 3); BoxQ(S, Lf - .9, Lf, fy, top, 0, .18, plaster, 3);
		SolidAlong(xz(0, 0), xz(.9, 0), fy, top, .45); SolidAlong(xz(Lf - .9, 0), xz(Lf, 0), fy, top, .45);
		WallQ(S, a0, a1, fy + 3.5, fy + 4.5, .12, sign, 3); WallQ(S, a0, a1, fy + 4.5, top, .02, plaster, 3); WallQ(S, a0, a1, fy + 3.42, fy + 3.5, .12, Mul(sign, .6), 3);
		quad(S.At(a0, fy + 3.42, 0), S.At(a1, fy + 3.42, 0), S.At(a1, fy + 3.42, .12), S.At(a0, fy + 3.42, .12), dark, 3, V3(0, -1, 0));
		quad(S.At(a0, fy + 3.5, 0), S.At(a1, fy + 3.5, 0), S.At(a1, fy + 3.5, -.02), S.At(a0, fy + 3.5, -.02), dark, 3, V3(0, -1, 0));
		o.Name(u.Name.IsEmpty() ? spec.Label : u.Name, S.At(mid, fy + 4.0, .15), face, .5);
		const double zones[2][2] = { { a0 + .12, mid - door / 2 - .15 }, { mid + door / 2 + .15, a1 - .12 } };

		// ---- what stands in it
		TFunction<void(const FW&, double, double, double, double, const FString&, const FString&, int32)> items;
		items = [&](const FW& W, double q0, double q1, double r, double y, const FString& what, const FString& key, int32 cap)
		{
			const double len = q1 - q0;
			if (len < .3) return;
			if (what == TEXT("books"))
			{
				const int32 n = FMath::Min(cap ? cap : 40, FMath::Max(3, (int32)JsRound(len / .34)));
				for (int32 i = 0; i < n; ++i) wb(W, q0 + (i + .5) * len / n, r, y, len / n * .9, .3 + .14 * rnd({ key, J(i) }), .22, good({ key, J(i), TEXT("c") }));
				return;
			}
			double step = .7;
			static const TMap<FString, double> Steps = { { TEXT("folded"), .8 }, { TEXT("boxes"), .72 }, { TEXT("shoes"), .7 }, { TEXT("balls"), .6 }, { TEXT("pots"), .66 }, { TEXT("jars"), .5 },
				{ TEXT("gadgets"), .7 }, { TEXT("plush"), .7 }, { TEXT("bread"), .62 }, { TEXT("bottles"), .38 }, { TEXT("lamps"), .8 } };
			if (const double* s = Steps.Find(what)) step = *s;
			const int32 n = FMath::Min(cap ? cap : 12, FMath::Max(1, (int32)FMath::FloorToDouble(len / step)));
			for (int32 i = 0; i < n; ++i)
			{
				const double q = q0 + (i + .5) * len / n, h = rnd({ key, J(i), TEXT("h") });
				const V3 c = good({ key, J(i), TEXT("c") });
				if (what == TEXT("folded")) wb(W, q, r, y, .56, .14 + .2 * h, .34, c);
				else if (what == TEXT("boxes")) wb(W, q, r, y, .42 + .14 * h, .3 + .18 * rnd({ key, J(i), TEXT("g") }), .3, Mul(c, 1.25));
				else if (what == TEXT("shoes")) { wb(W, q - .09, r, y, .13, .13, .32, c); wb(W, q + .09, r, y, .13, .13, .32, c); }
				else if (what == TEXT("balls")) { const V2 av = wp(W, q, r); ball(av.X, av.Y, y + .16, .17, Mul(c, 1.2), key + J(i)); }
				else if (what == TEXT("plush")) { const V2 av = wp(W, q, r); ball(av.X, av.Y, y + .17, .19, BROWN, key + J(i)); ball(av.X, av.Y, y + .42, .13, BROWN, key + J(i) + TEXT("h")); }
				else if (what == TEXT("pots")) wb(W, q, r, y, .3 + .12 * h, .22 + .24 * h, .28, Mul(c, .9));
				else if (what == TEXT("jars")) wb(W, q, r, y, .24, .28 + .12 * h, .22, Mul(c, 1.1));
				else if (what == TEXT("gadgets")) { wb(W, q, r, y, .42, .03, .3, DARK); wb(W, q, r - .1, y + .03, .42, .28, .03, SCREEN, 1); }
				else if (what == TEXT("bread")) wb(W, q, r, y, .42, .16 + .08 * h, .24, V3(.58 + .1 * h, .4, .2));
				else if (what == TEXT("bottles")) wb(W, q, r, y, .14, .36 + .1 * h, .14, Mul(c, 1.1));
				else if (what == TEXT("lamps")) { wb(W, q, r, y, .1, .3, .1, DARK); wb(W, q, r, y + .3, .3, .22, .3, V3(.86, .78, .6)); }
			}
		};
		auto guitar = [&](const FW& W, double q, double r, double y, const FString& key)
		{
			const V3 Cs[] = { V3(.5, .25, .12), V3(.62, .42, .18), DARK, V3(.5, .14, .12), V3(.75, .7, .6), V3(.2, .32, .4) };
			const V3 c = Cs[(int32)FMath::FloorToDouble(rnd({ key, TEXT("g") }) * 6) % 6];
			wb(W, q, r, y, .5, .58, .11, c); wb(W, q, r, y + .52, .37, .38, .11, c); wb(W, q, r, y + .84, .08, .9, .06, V3(.32, .22, .13)); wb(W, q, r, y + 1.72, .14, .22, .06, V3(.16, .14, .12)); wb(W, q, r + .065, y + .4, .14, .14, .02, V3(.08, .07, .07));
		};
		auto thing = [&](double a, double v, double y, const FString& what, const FString& key)   // one thing on a plinth
		{
			if (what == TEXT("bear")) { big(a, v, y + .45, .5, BROWN, key); big(a, v, y + 1.05, .34, BROWN, key + TEXT("h")); ball(a - .24, v, y + 1.36, .12, BROWN, key + TEXT("l")); ball(a + .24, v, y + 1.36, .12, BROWN, key + TEXT("r")); ball(a, v - .26, y + .98, .13, V3(.76, .66, .5), key + TEXT("m")); }
			else if (what == TEXT("blocks")) { int32 n = 0; for (int32 row = 0; row < 3; ++row) for (int32 i = 0; i < 3 - row; ++i) { const V3 g = good({ key, J(n++) }); bx(a + (i - (2 - row) / 2.0) * .34, v, y + row * .3, .3, .3, .3, Mul(g, 1.3)); } }
			else if (what == TEXT("vase")) { disc(a, v, y, y + .55, .2, V3(.2, .37, .38)); ball(a, v, y + .85, .32, Mul(good({ key }), 1.25), key); }
			else if (what == TEXT("cone")) { bx(a, v, y, .22, .45, .22, V3(.72, .56, .3)); ball(a, v, y + .62, .27, V3(.86, .7, .72), key); ball(a, v, y + .94, .21, V3(.8, .84, .7), key + TEXT("b")); }
			else if (what == TEXT("plant")) { const V3 soil(.2, .15, .1); disc(a, v, y, y + .4, .26, SIGN_terracotta, &soil); big(a, v, y + .9, .52, LEAF, key); }
			else if (what == TEXT("rocket")) { disc(a, v, y, y + 1.2, .3, V3(.75, .72, .66)); ball(a, v, y + 1.32, .32, V3(.6, .2, .18), key); for (const double s : { -1.0, 1.0 }) bx(a + s * .36, v, y, .1, .42, .3, V3(.6, .2, .18)); }
			else if (what == TEXT("cake")) { disc(a, v, y, y + .25, .4, V3(.85, .78, .66)); disc(a, v, y + .25, y + .45, .28, V3(.8, .6, .62)); ball(a, v, y + .54, .09, V3(.6, .15, .15), key); }
		};
		auto split = [](const FString& kind, FString& name, FString& what) { if (!kind.Split(TEXT(":"), &name, &what)) { name = kind; what = FString(); } };
		auto inWindow = [&](double z0, double z1, const FString& kind, const FString& key)   // on the display plinth of one side of the front
		{
			FString name, what; split(kind, name, what);
			const double len = z1 - z0, c = (z0 + z1) / 2, y = fy + .4;
			const FW W = { z0, .6, 1, 0, 0, 1, 0, false };
			if (name == TEXT("mannequins"))
			{
				const int32 n = FMath::Max(1, FMath::Min(4, (int32)FMath::FloorToDouble(len / 1.1)));
				for (int32 i = 0; i < n; ++i)
				{
					const double a = z0 + (i + .5) * len / n;
					const V3 g = good({ key, J(i) });
					bx(a, .6, y, .26, .8, .18, V3(.2, .2, .24)); bx(a, .6, y + .8, .42, .62, .22, g); bx(a, .6, y + 1.45, .18, .22, .18, V3(.8, .72, .6));
					for (const double s : { -1.0, 1.0 }) bx(a + s * .27, .6, y + .86, .09, .54, .12, g);
				}
			}
			else if (name == TEXT("stack"))
			{
				for (int32 row = 0; row < 3; ++row) items(W, .25 + row * .28, len - .25 - row * .28, 0, y + row * .3, what == TEXT("shoes") ? FString(TEXT("boxes")) : what, key + J(row), 6);
				if (what == TEXT("shoes")) items(W, .3, len - .3, -.3, y, TEXT("shoes"), key + TEXT("s"), 6);
			}
			else if (name == TEXT("plinth")) thing(c, .6, y, what, key);
			else if (name == TEXT("screen")) { const double w = FMath::Min(2.2, len - .5); bx(c, .6, y + .1, w, w * .58, .08, V3(.1, .1, .12)); bx(c, .6 - .05, y + .16, w - .12, w * .58 - .12, .02, SCREEN, 1); }
			else if (name == TEXT("guitarstand")) { const int32 n = FMath::Max(1, FMath::Min(3, (int32)FMath::FloorToDouble(len / .8))); for (int32 i = 0; i < n; ++i) guitar(W, (i + .5) * len / n, 0, y + .05, key + J(i)); }
			else { const int32 n = FMath::Max(1, FMath::Min(3, (int32)FMath::FloorToDouble(len / 1.2))); for (int32 i = 0; i < n; ++i) thing(z0 + (i + .5) * len / n, .6, y, TEXT("plant"), key + J(i)); }
		};
		for (int32 i = 0; i < 2; ++i)
		{
			const double zA = zones[i][0], zB = zones[i][1], len = zB - zA, c = (zA + zB) / 2;
			if (len < 1.2) continue;
			bx(c, .6, fy, len, .4, .95, V3(.7, .66, .58)); SolidAlong(xz(zA + .3, .6), xz(zB - .3, .6), fy, fy + 1.3, .5);
			for (const double a : { zA, zB }) bx(a, .08, fy, .09, 3.42, .09, Metal);
			bx(c, .08, fy + 2.75, len, .07, .07, Metal);
			inWindow(zA, zB, spec.Window.IsValidIndex(i) && !spec.Window[i].IsEmpty() ? spec.Window[i] : FString(TEXT("plants")), TEXT("w") + J(i));
		}

		auto onWall = [&](const FW& W, const FString& kind, const FString& key)   // what stands against one wall
		{
			FString name, what; split(kind, name, what);
			const double len = W.len;
			if (len < 1.2) return;
			double deep = .55;
			if (name == TEXT("shelves"))
			{
				wb(W, len / 2, .03, fy, len, 2.5, .06, Mul(Wood, 1.5)); wb(W, len / 2, .27, fy, len, .28, .5, Mul(Wood, 1.1)); wb(W, len / 2, .27, fy + 2.45, len, .06, .52, Mul(Wood, 1.1));
				for (int32 s = 0; s < 3; ++s)
				{
					const double y = fy + .28 + s * .68;
					if (s) wb(W, len / 2, .27, y - .05, len, .05, .5, Wood);
					const bool rnd2 = what == TEXT("balls") || what == TEXT("plush");
					items(W, .15, len - .15, .27, y, rnd2 && s != 1 ? FString(TEXT("boxes")) : what, key + J(s), rnd2 ? 6 : 0);
				}
			}
			else if (name == TEXT("rail"))
			{
				for (const double q : { .15, len - .15 }) wb(W, q, .32, fy, .06, 1.62, .06, Metal);
				wb(W, len / 2, .32, fy + 1.58, len - .2, .05, .05, Metal);
				const int32 n = FMath::Min(14, (int32)FMath::FloorToDouble((len - .5) / .36));
				for (int32 i = 0; i < n; ++i) { const bool tall = rnd({ key, J(i), TEXT("t") }) > .7; wb(W, .4 + i * (len - .8) / FMath::Max(1, n - 1), .32, fy + (tall ? .55 : .8), .24, tall ? 1.0 : .75, .5, good({ key, J(i) })); }
				wb(W, len / 2, .22, fy + 1.95, len, .05, .4, Wood); items(W, .2, len - .2, .22, fy + 2.0, TEXT("folded"), key + TEXT("top"), 5);
			}
			else if (name == TEXT("guitars"))
			{
				wb(W, len / 2, .02, fy + .6, len, 2.3, .04, Mul(Wood, 1.4));
				const int32 n = FMath::Max(1, (int32)FMath::FloorToDouble(len / .95));
				for (int32 i = 0; i < n; ++i) guitar(W, (i + .5) * len / n, .1, fy + .78, key + J(i));
				wb(W, len / 2, .3, fy, len, .55, .55, DARK); wb(W, len / 2, .585, fy + .08, len - .3, .4, .02, V3(.3, .28, .25));
			}
			else if (name == TEXT("screens"))
			{
				const int32 n = FMath::Max(1, (int32)FMath::FloorToDouble(len / 1.5));
				for (int32 i = 0; i < n; ++i) for (const double y : { .95, 1.85 }) { const double q = (i + .5) * len / n; wb(W, q, .06, fy + y, 1.3, .78, .08, V3(.1, .1, .12)); wb(W, q, .11, fy + y + .05, 1.2, .68, .02, SCREEN, 1); }
				wb(W, len / 2, .25, fy, len, .6, .45, DARK);
			}
			else if (name == TEXT("mirror"))
			{
				const double w = FMath::Min(1.6, len * .5);
				wb(W, len / 2, .04, fy + .3, w, 1.9, .06, Wood); wb(W, len / 2, .08, fy + .38, w - .16, 1.74, .02, V3(.62, .68, .72));
				for (const double q : { len * .16, len * .84 }) wb(W, q, .25, fy, FMath::Min(1.6, len * .26), .9, .45, Mul(Wood, .9));
				deep = .5;
			}
			else if (name == TEXT("rackets"))
			{
				const int32 n = FMath::Max(1, (int32)FMath::FloorToDouble(len / .6));
				wb(W, len / 2, .02, fy + .8, len, 1.7, .04, Mul(Wood, .7));
				for (int32 i = 0; i < n; ++i) { const double q = (i + .5) * len / n; const V3 c = good({ key, J(i) }); wb(W, q, .08, fy + 1.0, .05, .5, .04, DARK); wb(W, q, .08, fy + 1.5, .3, .4, .03, c); wb(W, q, .1, fy + 1.55, .22, .3, .02, V3(.8, .78, .7)); }
				wb(W, len / 2, .25, fy, len, .7, .45, Mul(Wood, .9)); items(W, .15, len - .15, .25, fy + .7, TEXT("balls"), key + TEXT("b"), 6);
			}
			else if (name == TEXT("cabinets"))
			{
				const int32 n = FMath::Max(1, (int32)FMath::FloorToDouble(len / 1.05));
				for (int32 i = 0; i < n; ++i)
				{
					const double q = (i + .5) * len / n;
					const V3 c = Mul(good({ key, J(i) }), .8);
					wb(W, q, .4, fy, .85, 1.85, .78, c); wb(W, q, .8, fy + 1.1, .62, .46, .03, SCREEN, 1); wb(W, q, .8, fy + 1.62, .75, .2, .03, GLOW, 1); wb(W, q, .9, fy + .92, .85, .07, .28, DARK);
				}
				deep = .95;
			}
			else if (name == TEXT("posters"))
			{
				const int32 n = FMath::Max(1, (int32)FMath::FloorToDouble(len / 1.5));
				for (int32 i = 0; i < n; ++i) { const double q = (i + .5) * len / n; wb(W, q, .03, fy + 1.1, 1.05, 1.45, .04, Mul(Wood, .8)); wb(W, q, .06, fy + 1.17, .93, 1.31, .02, good({ key, J(i) })); wb(W, q, .08, fy + 1.6, .6, .5, .02, Mul(good({ key, J(i), TEXT("b") }), 1.3)); }
				deep = .1;
			}
			else if (name == TEXT("servery"))
			{
				wb(W, len / 2, .3, fy, len, .9, .5, Mul(Wood, .85));
				for (int32 i = 0; i < 3; ++i) { const double q = (i + .5) * len / 3; wb(W, q, .05, fy + 2.15, len / 3 - .3, .9, .05, DARK); wb(W, q, .09, fy + 2.2, len / 3 - .5, .8, .02, GLOW, 1); for (int32 b = 0; b < 3; ++b) wb(W, q, .11, fy + 2.36 + b * .2, len / 3 - .9, .06, .02, Mul(sign, .8)); }
				if (!staffed)
				{
					wb(W, len / 2, 1.6, fy, len - 1.2, 1.0, .7, Wood); wb(W, len / 2, 1.6, fy + 1.0, len - 1.1, .05, .8, V3(.78, .72, .6)); wb(W, len * .32, 1.6, fy + 1.05, len * .36, .32, .5, GLOW, 1); wb(W, len * .74, 1.55, fy + 1.05, .6, .5, .45, V3(.22, .22, .25));
					const V2 a = wp(W, .8, 1.6), b = wp(W, len - .8, 1.6);
					SolidAlong(xz(a.X, a.Y), xz(b.X, b.Y), fy, fy + 1.05, .45);
				}
			}
			if (deep > .2) { const V2 a = wp(W, .2, deep / 2), b = wp(W, len - .2, deep / 2); SolidAlong(xz(a.X, a.Y), xz(b.X, b.Y), fy, fy + 2.2, deep / 2 + .1); }
		};
		auto island = [&](double a, double v, const FString& kind, const FString& key)   // what stands in the middle of the floor
		{
			FString name, what; split(kind, name, what);
			const FW W = { a, v, 1, 0, 0, 1, 0, false };
			if (name == TEXT("table"))
			{
				wb(W, 0, 0, fy + .74, 2.0, .06, 1.0, Wood);
				for (const double s : { -1.0, 1.0 }) for (const double r : { -1.0, 1.0 }) wb(W, s * .9, r * .4, fy, .07, .74, .07, Mul(Wood, .8));
				items(W, -.9, .9, -.22, fy + .8, what, key, 5); items(W, -.9, .9, .24, fy + .8, what, key + TEXT("b"), 5); block(a - .5, v, .6, 1); block(a + .5, v, .6, 1);
			}
			else if (name == TEXT("roundrack"))
			{
				wb(W, 0, 0, fy, .07, 1.6, .07, Metal);
				for (int32 i = 0; i < 10; ++i) { const double g = 6.2832 * i / 10; bx(a + std::cos(g) * .5, v + std::sin(g) * .5, fy + .72, .46, .8, .12, good({ key, J(i) }), 19, g); }
				block(a, v, .7, 1.5);
			}
			else if (name == TEXT("bin"))
			{
				const V3 soil(.2, .15, .1); disc(a, v, fy, fy + .62, .78, Mul(Wood, .9), &soil, 12);
				for (int32 i = 0; i < 14; ++i) { const double g = 6.2832 * rnd({ key, J(i) }), r = .6 * std::sqrt(rnd({ key, J(i), TEXT("r") })); bx(a + r * std::cos(g), v + r * std::sin(g), fy + .56, .22, .22, .22, Mul(good({ key, J(i), TEXT("c") }), 1.3), 19, g); }
				block(a, v, .75, .7);
			}
			else if (name == TEXT("bench"))
			{
				wb(W, 0, 0, fy, 1.6, .4, .45, Wood); wb(W, 0, 0, fy + .4, 1.5, .08, .4, good({ key })); block(a - .4, v, .45, .48); block(a + .4, v, .45, .48);
				const V3 p = P(a, 0, v); o.Use(p.X, fy + .49, p.Z, TEXT("sit"), TEXT("Sit down"), std::atan2(S.n.X, -S.n.Y));
			}
			else if (name == TEXT("amps"))
			{
				for (const double q : { -.5, .5 }) { wb(W, q, 0, fy, .8, .62, .42, DARK); wb(W, q, 0, fy + .62, .7, .5, .4, DARK); wb(W, q, -.22, fy + .08, .66, .46, .02, V3(.3, .28, .25)); wb(W, q, -.21, fy + .68, .58, .36, .02, V3(.3, .28, .25)); }
				block(a - .5, v, .5, 1.1); block(a + .5, v, .5, 1.1);
			}
			else if (name == TEXT("drums"))
			{
				const V3 red(.5, .14, .12), skin(.8, .76, .66), brass(.7, .58, .25);
				wb(W, 0, .1, fy, .62, .62, .4, red); disc(a - .55, v - .15, fy, fy + .5, .28, red, &skin); disc(a + .5, v - .15, fy + .45, fy + .6, .2, red, &skin); disc(a + .15, v - .45, fy + .5, fy + .62, .17, red, &skin);
				for (const V2 qr : { V2(-.85, .25), V2(.85, .3) }) { wb(W, qr.X, qr.Y, fy, .04, 1.25, .04, Metal); disc(a + qr.X, v + qr.Y, fy + 1.25, fy + 1.28, .27, brass); }
				block(a, v, .9, 1);
			}
			else if (name == TEXT("keyboard"))
			{
				wb(W, 0, 0, fy + .82, 1.3, .08, .36, DARK); wb(W, 0, -.08, fy + .9, 1.2, .02, .14, V3(.86, .84, .78));
				for (const double s : { -1.0, 1.0 }) wb(W, s * .45, 0, fy, .06, .82, .3, Metal);
				block(a, v, .6, 1);
			}
			else if (name == TEXT("glasscase"))
			{
				wb(W, 0, 0, fy, 1.7, .75, .65, Mul(Wood, .85)); wb(W, 0, 0, fy + .75, 1.7, .28, .65, GLOW, 1); wb(W, 0, 0, fy + 1.03, 1.74, .03, .69, V3(.75, .7, .6));
				block(a - .45, v, .5, 1.1); block(a + .45, v, .5, 1.1);
			}
			else if (name == TEXT("cafetable"))
			{
				disc(a, v, fy + .7, fy + .75, .46, Wood); wb(W, 0, 0, fy, .08, .7, .08, Metal);
				const V3 c = good({ key });
				for (const V2 qr : { V2(.8, 0), V2(-.8, 0), V2(0, .8), V2(0, -.8) })
				{
					wb(W, qr.X, qr.Y, fy, .36, .45, .36, c);
					if (qr.X != 0) wb(W, qr.X * 1.24, qr.Y, fy + .45, .06, .42, .36, c); else wb(W, qr.X, qr.Y * 1.24, fy + .45, .36, .42, .06, c);
				}
				block(a, v, .55, .76);
			}
			else if (name == TEXT("hockey"))
			{
				wb(W, 0, 0, fy, 2.0, .8, 1.0, V3(.6, .2, .18)); wb(W, 0, 0, fy + .8, 1.9, .03, .9, V3(.84, .86, .9)); wb(W, 0, 0, fy + .83, .04, .01, .9, V3(.6, .2, .18));
				block(a - .5, v, .6, .9); block(a + .5, v, .6, .9);
			}
			else if (name == TEXT("claw"))
			{
				const V3 c(.5, .2, .4);
				wb(W, 0, 0, fy, .9, .85, .9, c); wb(W, 0, 0, fy + .85, .84, .95, .84, SCREEN, 1); wb(W, 0, 0, fy + 1.8, .9, .16, .9, c); block(a, v, .55, 2);
			}
			else if (name == TEXT("buckets"))
			{
				for (int32 i = 0; i < 5; ++i) { const double q = (i - 2) * .5, r = (i % 2) * .34 - .17; disc(a + q, v + r, fy, fy + .4, .2, V3(.3, .32, .34)); ball(a + q, v + r, fy + .66, .27, Mul(good({ key, J(i) }), 1.3), key + J(i)); }
				block(a - .6, v, .55, .6); block(a + .6, v, .55, .6);
			}
			else if (name == TEXT("plinth")) { wb(W, 0, 0, fy, 1.0, .35, 1.0, V3(.7, .66, .58)); thing(a, v, fy + .35, what, key); block(a, v, .55, 1.2); }
		};
		const TArray<FString>& walls = spec.Walls;
		const double v0 = 1.35;
		auto wallOf = [&](int32 i) { return walls.IsValidIndex(i) && !walls[i].IsEmpty() ? walls[i] : FString(TEXT("posters")); };
		onWall({ a0, v0, 0, 1, 1, 0, D - v0 - .15, true }, wallOf(0), TEXT("L"));
		onWall({ a0 + .75, D, 1, 0, 0, -1, a1 - a0 - 1.5, false }, wallOf(1), TEXT("B"));
		onWall({ a1, v0, 0, 1, -1, 0, D - v0 - .15, true }, wallOf(2), TEXT("R"));
		if (staffed)   // its one keeper's counter (CITY-105): 2.2 m in, with a till; a lit case on it where food is sold
		{
			const double half = FMath::Min(mid - 2.4, food ? 2.3 : 1.6);
			BoxQ(S, mid - half, mid + half, fy, fy + 1.0, -2.9, -2.2, wood, 19); SolidAlong(xz(mid - half, 2.55), xz(mid + half, 2.55), fy, fy + 1.0, .4);
			bx(mid + half - .5, 2.55, fy + 1.0, .4, .3, .34, DARK); bx(mid + half - .5, 2.47, fy + 1.3, .34, .22, .03, SCREEN, 1);
			if (food) bx(mid - half + .9, 2.55, fy + 1.0, 1.6, .34, .5, GLOW, 1);
		}
		if (D >= 6)
		{
			const bool servery = walls.IsValidIndex(1) && walls[1] == TEXT("servery");
			const double vA = staffed ? 5.0 : FMath::Max(3.4, D * .44), vB = FMath::Min(D - 2.0, vA + 2.4);
			const double slots[3][2] = { { .27, vA }, { .73, vA }, { .5, servery || staffed ? vA : vB } };
			for (int32 i = 0; i < FMath::Min(3, spec.Middle.Num()); ++i) island(a0 + (a1 - a0) * slots[i][0], slots[i][1], spec.Middle[i], TEXT("m") + J(i));
		}
	}

	void FBuild::Horse(double x, double z, double y, double yaw, const V3& col, const FString& label)   // facing (sin yaw, -cos yaw)
	{
		const double fx = std::sin(yaw), fz = -std::cos(yaw), th = std::atan2(-fz, fx);
		auto at = [&](double d, double s) { return V2(x + fx * d - fz * s, z + fz * d + fx * s); };
		auto b = [&](double d, double s, double y0, double sx, double sy, double sz, const V3& c) { const V2 p = at(d, s); o.YBox(p.X, y0, p.Y, sx, sy, sz, th, c, 19); };
		b(0, 0, y, .95, .36, .3, col); b(.52, 0, y + .28, .2, .46, .2, col); b(.68, 0, y + .6, .36, .2, .18, col); b(-.5, 0, y + .1, .1, .34, .1, Mul(col, .6));
		for (const double d : { -.3, .32 }) for (const double s : { -.1, .1 }) b(d, s, y - .42, .09, .44, .08, Mul(col, .85));
		b(0, 0, y + .36, .34, .06, .34, V3(.5, .16, .14));
		o.Use(x, y + .42, z, TEXT("sit"), label, yaw);
	}

	void FBuild::Ride(const FRide& q)
	{
		const double x = q.Pos.X, z = q.Pos.Y;
		const FString key = q.Kind + J(x) + J(z);
		const V3 creamR(.8, .74, .6), red(.56, .22, .18);
		if (q.Kind == TEXT("carousel"))
		{
			const double r = q.R;
			const int32 n = q.Horses ? q.Horses : 10;
			Prism(Round(x, z, r, r, 28), F, F + .35, Mul(Wood, .9), Mul(Wood, 1.2), 19, 15); Prism(Round(x, z, r + .12, r + .12, 28), F, F + .16, Stone, Stone, 3, 3);
			Prism(Round(x, z, 1.15, 1.15, 14), F + .35, F + 3.7, creamR, creamR, 3, 3); Prism(Round(x, z, 1.19, 1.19, 14), F + 2.9, F + 3.12, GLOW, creamR, 1, 3);
			for (int32 i = 0; i < 6; ++i) { const double f = i / 6.0, rr = (r + .35) * (1 - f) + .5 * f; Prism(Round(x, z, rr, rr, 28), F + 3.7 + i * .3, F + 4.0 + i * .3, i % 2 ? creamR : red, i % 2 ? creamR : red, 19, 19); }
			Blob(x, F + 5.75, z, .38, V3(.7, .58, .25), key);
			for (int32 i = 0; i < 16; ++i) { const double a = 6.2832 * i / 16, bxp = x + (r + .3) * std::cos(a), bzp = z + (r + .3) * std::sin(a); o.Box(bxp - .08, F + 3.5, bzp - .08, bxp + .08, F + 3.68, bzp + .08, BrixSlice::Lantern, 2); }
			for (int32 i = 0; i < n; ++i)
			{
				const double a = 6.2832 * i / n, hx = x + (r - 1.5) * std::cos(a), hz = z + (r - 1.5) * std::sin(a), yaw = std::atan2(-std::sin(a), -std::cos(a)), y = F + 1.15 + (i % 2) * .28;
				o.Box(hx - .035, F + .35, hz - .035, hx + .035, F + 3.7, hz + .035, V3(.7, .58, .25), 19);
				Horse(hx, hz, y, yaw, GOODS[(i * 3) % NGOODS], TEXT("Sit on the carousel"));
			}
			for (int32 i = 0; i < 12; ++i) { const double a = 6.2832 * i / 12; o.Solid(x + r * .62 * std::cos(a) - r * .4, F, z + r * .62 * std::sin(a) - r * .4, x + r * .62 * std::cos(a) + r * .4, F + .35, z + r * .62 * std::sin(a) + r * .4); }
			o.Solid(x - 1.1, F, z - 1.1, x + 1.1, F + 3.7, z + 1.1);
			return;
		}
		o.Box(x - .8, F, z - .5, x + .8, F + .28, z + .5, V3(.3, .32, .36), 19); o.Box(x + .95, F, z - .12, x + 1.19, F + .95, z + .12, red, 19); o.Box(x + .97, F + .95, z - .1, x + 1.17, F + 1.05, z + .1, GLOW, 1);
		o.Solid(x - .8, F, z - .5, x + .8, F + .28, z + .5);
		if (q.Kind == TEXT("pony")) Horse(x, z, F + .95, PI_, V3(.78, .7, .56), TEXT("Sit on the pony"));
		else if (q.Kind == TEXT("rocket"))
		{
			Prism(Round(x, z, .34, .34, 10), F + .28, F + 1.5, V3(.78, .75, .68), V3(.78, .75, .68), 19, 19); Blob(x, F + 1.62, z, .36, red, key);
			for (const double s : { -1.0, 1.0 }) o.Box(x + s * .36 - .05, F + .28, z - .18, x + s * .36 + .05, F + .75, z + .18, red, 19);
			o.Use(x, F + .9, z + .42, TEXT("sit"), TEXT("Sit on the rocket"), PI_);
		}
		else
		{
			o.Box(x - .62, F + .4, z - .34, x + .62, F + .78, z + .34, red, 19); o.Box(x - .28, F + .78, z - .3, x + .3, F + 1.08, z + .3, creamR, 19);
			for (const double sx : { -.42, .42 }) for (const double sz : { -.36, .36 }) o.Box(x + sx - .14, F + .28, z + sz - .05, x + sx + .14, F + .56, z + sz + .05, DARK, 19);
			o.Use(x, F + .8, z, TEXT("sit"), TEXT("Sit in the car"), PI_ / 2);
		}
	}

	void FBuild::Draw()
	{
		const double st = L.St, Hh = L.H;
		const V3 lit97 = Lit(.97);
		TArray<FRegion> regions;
		for (int32 k = 0; k < L.Levels.Num(); ++k) regions.Add(Region(k));
		for (int32 k = 0; k < L.Levels.Num(); ++k)
		{
			const FLevel& Lv = L.Levels[k];
			const double fy = k ? k * st : F;
			const FRegion& R = regions[k];
			const double top = fy + st - (k ? 0 : F) - T;
			// ---- floors, their undersides, the smooth strip and the parapet along each void
			Raster([&](double x, double z) { return R.Cell(x, z); }, [&](double xa, double za, double xb, double zb)
			{
				Flat(V2(xa, za), V2(xa, zb), V2(xb, zb), V2(xb, za), fy, floorC, 16, 1);
				if (k) { Flat(V2(xa, za), V2(xa, zb), V2(xb, zb), V2(xb, za), fy - T, ceilC, 3, -1); o.Solid(xa, fy - T, za, xb, fy, zb); }
			});
			if (k)
				for (const Poly& w : Lv.Walks)
				{
					if (w.Num() != 4) continue;
					const double la = Hyp(w[1].X - w[0].X, w[1].Y - w[0].Y), lb = Hyp(w[3].X - w[0].X, w[3].Y - w[0].Y);
					const int32 na = (int32)std::ceil(la / 1.2), nb = (int32)std::ceil(lb / 1.2);
					auto at = [&](double s, double v) { return V2(w[0].X + (w[1].X - w[0].X) * s + (w[3].X - w[0].X) * v, w[0].Y + (w[1].Y - w[0].Y) * s + (w[3].Y - w[0].Y) * v); };
					for (int32 i = 0; i < na; ++i)
						for (int32 j = 0; j < nb; ++j)
						{
							const V2 c = at((i + .5) / na, (j + .5) / nb);
							bool nearHall = false;
							for (const FHall& h : L.Halls) if (Hyp(c.X - h.Pos.X, c.Y - h.Pos.Y) < h.R + 2.4) { nearHall = true; break; }
							if (R.Hole(c.X, c.Y, 0) || !nearHall) continue;
							const V2 p0 = at((double)i / na, (double)j / nb), p1 = at((double)(i + 1) / na, (double)j / nb), p2 = at((double)(i + 1) / na, (double)(j + 1) / nb), p3 = at((double)i / na, (double)(j + 1) / nb);
							Flat(p0, p1, p2, p3, fy, floorC, 16, 1); Flat(p0, p1, p2, p3, fy - T, ceilC, 3, -1); o.Solid(c.X - .8, fy - T, c.Y - .8, c.X + .8, fy, c.Y + .8);
						}
				}
			for (const Poly& v : Lv.Voids)
			{
				const TArray<V2> out = Outward(v);
				const int32 n = v.Num();
				const double Wd = 2.9;
				for (int32 i = 0; i < n; ++i)
				{
					const int32 j = (i + 1) % n;
					const V2& a = v[i]; const V2& b = v[j];
					const V2 a2(a.X + out[i].X * Wd, a.Y + out[i].Y * Wd), b2(b.X + out[j].X * Wd, b.Y + out[j].Y * Wd);
					Flat(a, b, b2, a2, fy + .006, floorC, 16, 1); Flat(a, b, b2, a2, fy - T, ceilC, 3, -1);
					for (const V2 dw : { V2(1.2, .2), V2(2.4, .12) })
					{
						auto p = [](const V2& q, const V2& oo, double dd) { return V2(q.X + oo.X * dd, q.Y + oo.Y * dd); };
						Flat(p(a, out[i], dw.X), p(b, out[j], dw.X), p(b, out[j], dw.X + dw.Y), p(a, out[i], dw.X + dw.Y), fy - T - .012, lit97, 1, -1);
					}
					for (const double f : { .17, .5, .83 })
						for (const double r : { .4, 1.3, 2.2, 3.1 })
						{
							const double x = a.X + (b.X - a.X) * f + (out[i].X + out[j].X) / 2 * r, z = a.Y + (b.Y - a.Y) * f + (out[i].Y + out[j].Y) / 2 * r;
							o.Solid(x - .95, fy - T, z - .95, x + .95, fy, z + .95);
						}
				}
			}
			auto inBridge = [&](double x, double z)
			{
				for (const Poly& b : Lv.Bridges) { const FVector4d q = BoxOf(b); if (x > q.X - .2 && x < q.Z + .2 && z > q.Y - .2 && z < q.W + .2) return true; }
				return false;
			};
			// a ribbon from under the slab to hand height, a line of light under it
			auto parapet = [&](const FFrame& S, bool endA, bool endB, const V2* nA, const V2* nB)
			{
				const double y0 = fy - T - .4, y1 = fy + RAIL;
				const V3 back(-S.n.X, 0, -S.n.Y);
				auto bk = [&](int32 e, double y)
				{
					const V2* nE = e ? nB : nA;
					const V3 p = S.At(e ? S.L : 0, y, 0);
					if (!nE) return S.At(e ? S.L : 0, y, -.3);
					const double q = .3 / FMath::Max(.5, nE->X * S.n.X + nE->Y * S.n.Y);
					return V3(p.X - nE->X * q, y, p.Z - nE->Y * q);
				};
				const double yf = fy + .3;
				o.QuadH(bk(0, y0), bk(1, y0), bk(1, fy - T), bk(0, fy - T), fascia, 3, back);
				for (int32 w = 0; w < 2; ++w)
				{
					const bool on = w ? endB : endA;
					const int32 e = w;
					const double sg = w ? 1 : -1;
					if (!on) continue;
					o.QuadH(S.At(e * S.L, y0, 0), bk(e, y0), bk(e, yf), S.At(e * S.L, yf, 0), fascia, 3, V3(S.t.X * sg, 0, S.t.Y * sg));
					o.QuadH(S.At(e * S.L, yf, 0), bk(e, yf), bk(e, y1), S.At(e * S.L, y1, 0), cream, 3, V3(S.t.X * sg, 0, S.t.Y * sg));
				}
				WallQ(S, 0, S.L, y0, yf, 0, fascia, 3); WallQ(S, 0, S.L, yf, y1, 0, cream, 3); WallQ(S, 0, S.L, y0 + .06, y0 + .3, .03, lit97, 1);
				o.QuadH(S.At(0, y1, .04), S.At(S.L, y1, .04), bk(1, y1), bk(0, y1), wood, 19, V3(0, 1, 0)); WallQ(S, 0, S.L, y1 - .07, y1, .04, wood, 19);
				o.QuadH(bk(0, fy), bk(1, fy), bk(1, y1), bk(0, y1), plaster, 3, back);
				o.QuadH(S.At(0, y0, 0), S.At(S.L, y0, 0), bk(1, y0), bk(0, y0), fascia, 3, V3(0, -1, 0));
				SolidAlong(V2(S.mx - S.t.X * S.L / 2 - S.n.X * .15, S.mz - S.t.Y * S.L / 2 - S.n.Y * .15), V2(S.mx + S.t.X * S.L / 2 - S.n.X * .15, S.mz + S.t.Y * S.L / 2 - S.n.Y * .15), fy, fy + RAIL, .3);
			};
			auto toVoid = [&](double x, double z) { return !R.On(x, z); };
			for (const Poly& rl : Lv.Rails)
			{
				const int32 n = rl.Num() - 1;
				const bool shut = rl[0].X == rl[n].X && rl[0].Y == rl[n].Y;
				TArray<V2> sn; TArray<bool> gone;
				for (int32 i = 0; i < n; ++i) { sn.Add(MakeFrame(rl[i], rl[i + 1], toVoid).n); gone.Add(inBridge((rl[i].X + rl[i + 1].X) / 2, (rl[i].Y + rl[i + 1].Y) / 2)); }
				auto joint = [&](int32 i, int32 j, V2& Out)
				{
					if (j < 0 || j >= n) { if (!shut) return false; j = (j + n) % n; }
					const double x = sn[i].X + sn[j].X, z = sn[i].Y + sn[j].Y, q = Hyp(x, z);
					if (gone[j] || q < .2) return false;
					Out = V2(x / q, z / q);
					return true;
				};
				for (int32 i = 0; i < n; ++i)
				{
					if (gone[i]) continue;
					V2 a, b;
					const bool hA = joint(i, i - 1, a), hB = joint(i, i + 1, b);
					parapet(MakeFrame(rl[i], rl[i + 1], toVoid), !hA, !hB, hA ? &a : nullptr, hB ? &b : nullptr);
				}
				if (shut && n > 40)   // plants in troughs on the parapet, trailing down over the fascia
				{
					double run = 3 + 5 * H({ L.Id, J(k), TEXT("green") });
					for (int32 i = 0; i < n; ++i)
					{
						const V2& a = rl[i]; const V2& b = rl[i + 1];
						const double len = Hyp(b.X - a.X, b.Y - a.Y);
						run -= len;
						if (run > 0 || gone[i]) continue;
						run = 6.5 + 5 * H({ L.Id, J(k), J(i), TEXT("gap") });
						const double x = (a.X + b.X) / 2 + sn[i].X * .05, z = (a.Y + b.Y) / 2 + sn[i].Y * .05, tx = (b.X - a.X) / len, tz = (b.Y - a.Y) / len, bg = H({ L.Id, J(k), J(i), TEXT("big") });
						for (const double u : { -.7, 0.0, .7 }) Blob(x + tx * u, fy + RAIL + .22, z + tz * u, .5 + .22 * H({ L.Id, J(k), J(i), J(u) }), leafC, L.Id + J(k) + TEXT(",") + J(i) + J(u));
						for (int32 s = 0; s < 6; ++s)
						{
							const double u = -1.1 + 2.2 * H({ L.Id, J(k), J(i), J(s), TEXT("u") }), drop = .5 + (1.3 + bg * 1.4) * H({ L.Id, J(k), J(i), J(s), TEXT("d") });
							const double px = x + tx * u + sn[i].X * .14, pz = z + tz * u + sn[i].Y * .14;
							o.Box(px - .09, fy + RAIL - drop, pz - .09, px + .09, fy + RAIL + .1, pz + .09, Mul(leafC, .8 + .4 * H({ L.Id, J(k), J(i), J(s), TEXT("c") })), 19);
						}
					}
				}
			}
			for (const Poly& br : Lv.Bridges)   // a bridge: its slab, and a parapet along each side, open where a moving stair lands
			{
				const FVector4d q = BoxOf(br);
				Flat(V2(q.X, q.Y), V2(q.X, q.W), V2(q.Z, q.W), V2(q.Z, q.Y), fy + .008, floorC, 16, 1); Flat(V2(q.X, q.Y), V2(q.X, q.W), V2(q.Z, q.W), V2(q.Z, q.Y), fy - T, ceilC, 3, -1); o.Solid(q.X, fy - T, q.Y, q.Z, fy, q.W);
				for (const V2 zsg : { V2(q.Y, -1), V2(q.W, 1) })
				{
					const double zs = zsg.X, sg = zsg.Y;
					bool bGap = false; double g0 = 0, g1 = 0;
					for (const FRamp& r : L.Ramps)
					{
						if (r.To == k && std::fabs(r.B.Y - zs) < .6) { bGap = true; g0 = r.B.X; g1 = r.Width; break; }
						if (r.From == k && std::fabs(r.A.Y - zs) < .6) { bGap = true; g0 = r.A.X; g1 = r.Width; break; }
					}
					TArray<V2> parts;
					if (bGap) { parts.Add(V2(q.X, g0 - g1 / 2 - .1)); parts.Add(V2(g0 + g1 / 2 + .1, q.Z)); } else parts.Add(V2(q.X, q.Z));
					for (const V2& uu : parts) parapet(MakeFrameP(V2(uu.X, zs), V2(uu.Y, zs), V2((uu.X + uu.Y) / 2, zs + sg * 4)), true, true, nullptr, nullptr);
				}
			}
			// ---- walls and shop fronts
			TSet<FString> open;
			TMap<FString, const FUnit*> units;
			for (const FUnit& u : Lv.Units)
			{
				units.Add(u.Id, &u);
				if (!u.Outlet.IsEmpty() && u.bStand)
				{
					bool any = false;
					for (const V2& w : L.Work) { for (const V2& s : u.Stand) if (std::fabs(s.X - w.X) < .3 && std::fabs(s.Y - w.Y) < .3) { any = true; break; } if (any) break; }
					if (any) open.Add(u.Id);
				}
			}
			for (const FFront& fr : Lv.Fronts)
			{
				const FFrame S = MakeFrame(fr.A, fr.B, [&](double x, double z) { return R.Walk(x, z); });
				const FUnit* const* up = units.Find(fr.Unit);
				if (!up) continue;
				const FUnit& u = **up;
				Front(S, u, fy, top, k, open.Contains(u.Id), u.Outlet.IsEmpty() ? nullptr : L.Outlets.Find(u.Outlet));
			}
			for (const FWallLine& w : Lv.Walls)
				for (int32 i = 0; i + 1 < w.Line.Num(); ++i)
				{
					const V2& a = w.Line[i]; const V2& b = w.Line[i + 1];
					const bool drum = w.Kind == TEXT("drum");
					const FHall* hh = nullptr;
					if (drum) { hh = &L.Halls[0]; for (const FHall& h : L.Halls) if (Hyp(h.Pos.X - a.X, h.Pos.Y - a.Y) < Hyp(hh->Pos.X - a.X, hh->Pos.Y - a.Y)) hh = &h; }
					const FFrame S = drum ? MakeFrameP(a, b, hh->Pos) : MakeFrame(a, b, [&](double x, double z) { return R.Walk(x, z); });
					const double y1 = drum ? fy + st - (k ? 0 : F) : top;
					WallQ(S, 0, S.L, fy - (drum && k ? T : 0), y1, 0, drum ? cream : plaster, 3);
					if (drum) { WallQ(S, 0, S.L, fy + 4.3, fy + 4.42, .03, lit97, 1); if (!k) WallQ(S, 0, S.L, fy, fy + .5, .05, dark, 3); }
					else if (i % 3 == 1 && S.L > 1.7) WallQ(S, .15, S.L - .15, fy + 1.0, fy + 3.1, .04, V3(0, 0, .96 + .03 * H({ L.Id, J(k), J(i), J(a.X) })), 1, true);
					SolidAlong(a, b, fy, y1, .45);
				}
		}

		// ---- the roof: solid round the court's opening and the egg halls' eyes, with a rim under the roof light
		TArray<TPair<const Poly*, FVector4d>> eyes;
		for (const Poly& e : L.Openings) eyes.Add({ &e, BoxOf(e) });
		const double RIM = 1.5;
		Raster([&](double x, double z)
		{
			for (const TPair<const Poly*, FVector4d>& e : eyes) { const FVector4d& b = e.Value; if (x > b.X - 1.5 && x < b.Z + 1.5 && z > b.Y - 1.5 && z < b.W + 1.5 && (InPoly(*e.Key, x, z) || EdgeDist(*e.Key, x, z) < 1.4)) return false; }
			for (const FHall& h : L.Halls) if (Hyp(x - h.Pos.X, z - h.Pos.Y) < h.R + 1.2) return false;
			return true;
		}, [&](double xa, double za, double xb, double zb)
		{
			Flat(V2(xa, za), V2(xa, zb), V2(xb, zb), V2(xb, za), Hh, Mul(cream, .86), 0, 1); Flat(V2(xa, za), V2(xa, zb), V2(xb, zb), V2(xb, za), Hh - T, ceilC, 3, -1);
		});
		for (const TPair<const Poly*, FVector4d>& eb : eyes)
		{
			const Poly& e = *eb.Key; const FVector4d& hb = eb.Value;
			const TArray<V2> out = Outward(e);
			const int32 n = e.Num();
			const V2 mid((hb.X + hb.Z) / 2, (hb.Y + hb.W) / 2);
			for (int32 i = 0; i < n; ++i)
			{
				const int32 j = (i + 1) % n;
				const V2& a = e[i]; const V2& b = e[j];
				const V2 a2(a.X + out[i].X * 2.9, a.Y + out[i].Y * 2.9), b2(b.X + out[j].X * 2.9, b.Y + out[j].Y * 2.9), a3(a.X + out[i].X * .35, a.Y + out[i].Y * .35), b3(b.X + out[j].X * .35, b.Y + out[j].Y * .35);
				Flat(a, b, b2, a2, Hh - T - .004, ceilC, 3, -1); Flat(a, b, b2, a2, Hh + .004, Mul(cream, .86), 0, 1);
				const FFrame S = MakeFrameP(a, b, mid);
				WallQ(S, 0, S.L, Hh - T - RIM, Hh + .3, 0, fascia, 3); WallQ(S, 0, S.L, Hh - T - RIM + .12, Hh - T - RIM + .36, .03, lit97, 1);
				o.QuadH(V3(a3.X, Hh - T - RIM, a3.Y), V3(b3.X, Hh - T - RIM, b3.Y), V3(b3.X, Hh - T, b3.Y), V3(a3.X, Hh - T, a3.Y), fascia, 3, V3(-S.n.X, 0, -S.n.Y));
				o.QuadH(V3(a.X, Hh - T - RIM, a.Y), V3(b.X, Hh - T - RIM, b.Y), V3(b3.X, Hh - T - RIM, b3.Y), V3(a3.X, Hh - T - RIM, a3.Y), fascia, 3, V3(0, -1, 0));
			}
		}

		// ---- moving stairs: a slope with a parapet each side, walked up as thirty low steps
		for (const FRamp& r : L.Ramps)
		{
			const double ya = r.From ? r.From * st : F, yb = r.To * st, dx = r.B.X - r.A.X, dz = r.B.Y - r.A.Y, Lr = Hyp(dx, dz);
			const V2 t(dx / Lr, dz / Lr), s(-t.Y, t.X);
			const double hw = r.Width / 2;
			auto pt = [&](double f, double side, double y) { return V3(r.A.X + dx * f + s.X * side, ya + (yb - ya) * f + y, r.A.Y + dz * f + s.Y * side); };
			const double Ls = Hyp(Lr, yb - ya), MID = .15;
			const V3 upN(0, 1, 0);
			for (const double sg : { -1.0, 1.0 })
			{
				const double s0 = sg > 0 ? MID : -hw + .3, s1 = sg > 0 ? hw - .3 : -MID;
				o.QuadN({ pt(0, s0, .01), pt(0, s1, .01), pt(1, s1, .01), pt(1, s0, .01) }, { upN, upN, upN, upN }, { V3(0, sg, s0), V3(0, sg, s1), V3(Ls, sg, s1), V3(Ls, sg, s0) }, 28);
				o.QuadH(pt(0, sg * MID, 0), pt(1, sg * MID, 0), pt(1, sg * MID, RAIL - .2), pt(0, sg * MID, RAIL - .2), plaster, 3, V3(s.X * sg, 0, s.Y * sg));
			}
			o.QuadH(pt(0, -MID, RAIL - .2), pt(0, MID, RAIL - .2), pt(1, MID, RAIL - .2), pt(1, -MID, RAIL - .2), cream, 3, V3(0, 1, 0));
			for (const double f : { 0.0, 1.0 }) o.QuadH(pt(f, -MID, 0), pt(f, MID, 0), pt(f, MID, RAIL - .2), pt(f, -MID, RAIL - .2), cream, 3, V3(t.X * (f != 0 ? 1 : -1), 0, t.Y * (f != 0 ? 1 : -1)));
			o.QuadH(pt(0, -hw, -T), pt(0, hw, -T), pt(1, hw, -T), pt(1, -hw, -T), ceilC, 3, V3(0, -1, 0));
			FBrixMovingStair& mv = M.Moving.AddDefaulted_GetRef();
			mv.A = r.A; mv.T = t; mv.S = s; mv.Lr = Lr; mv.Ya = ya; mv.Yb = yb; mv.Hw = hw; mv.V = STAIR_V * Lr / Ls;
			for (const double sg : { -1.0, 1.0 })
			{
				o.QuadH(pt(0, sg * hw, -T), pt(1, sg * hw, -T), pt(1, sg * hw, RAIL), pt(0, sg * hw, RAIL), fascia, 3, V3(s.X * sg, 0, s.Y * sg));
				o.QuadH(pt(0, sg * (hw + .02), -T + .04), pt(1, sg * (hw + .02), -T + .04), pt(1, sg * (hw + .02), -T + .2), pt(0, sg * (hw + .02), -T + .2), lit97, 1, V3(s.X * sg, 0, s.Y * sg));
				for (const double f : { 0.0, 1.0 }) o.QuadH(pt(f, sg * hw, -T), pt(f, sg * (hw - .3), -T), pt(f, sg * (hw - .3), RAIL), pt(f, sg * hw, RAIL), fascia, 3, V3(t.X * (f != 0 ? 1 : -1), 0, t.Y * (f != 0 ? 1 : -1)));
				o.QuadH(pt(0, sg * (hw - .3), 0), pt(1, sg * (hw - .3), 0), pt(1, sg * (hw - .3), RAIL), pt(0, sg * (hw - .3), RAIL), plaster, 3, V3(-s.X * sg, 0, -s.Y * sg));
				o.QuadH(pt(0, sg * hw, RAIL), pt(1, sg * hw, RAIL), pt(1, sg * (hw - .3), RAIL), pt(0, sg * (hw - .3), RAIL), wood, 19, V3(0, 1, 0));
			}
			const int32 N = 30;
			for (int32 i = 0; i < N; ++i)
			{
				const V3 p0 = pt((double)i / N, 0, 0), p1 = pt((double)(i + 1) / N, 0, 0);
				const double y1 = ya + (yb - ya) * (i + 1) / N, ex = std::fabs(s.X) * hw, ez = std::fabs(s.Y) * hw;
				o.Solid(FMath::Min(p0.X, p1.X) - ex, y1 - T, FMath::Min(p0.Z, p1.Z) - ez, FMath::Max(p0.X, p1.X) + ex, y1, FMath::Max(p0.Z, p1.Z) + ez);
				for (const double sg : { -1.0, 1.0 }) { const V3 q = pt((i + .5) / N, sg * (hw - .15), 0); o.Solid(q.X - .25, y1 - T, q.Z - .25, q.X + .25, y1 + RAIL, q.Z + .25); }
				{ const V3 q = pt((i + .5) / N, 0, 0); o.Solid(q.X - .24, y1 - T, q.Z - .24, q.X + .24, y1 + RAIL - .2, q.Z + .24); }
			}
		}

		// ---- lifts: a round shaft through every floor, a door on each; using it takes you up or down a floor
		for (const FLift& q : L.Lifts)
		{
			const double x = q.Pos.X, z = q.Pos.Y, r = q.R, yaw = q.Door * PI_ / 180, fx = std::sin(yaw), fz = -std::cos(yaw), rx = -fz, rz = fx;
			const int32 n = L.Levels.Num();
			Prism(Round(x, z, r, r, 24), F, Hh - T, cream, cream, 3, 3); Prism(Round(x, z, r + .08, r + .08, 24), F, F + .5, dark, dark, 3, 3);
			for (int32 i = 0; i < 10; ++i) { const double a = 6.2832 * i / 10; o.Solid(x + (r - .5) * std::cos(a) - .5, F, z + (r - .5) * std::sin(a) - .5, x + (r - .5) * std::cos(a) + .5, Hh, z + (r - .5) * std::sin(a) + .5); }
			auto fyOf = [&](int32 k) { return k ? k * st : F; };
			auto to = [&](int32 k) { return V3(x + fx * (r + 1.3), fyOf(k), z + fz * (r + 1.3)); };
			auto floorName = [&](int32 k) { return k ? TEXT("floor ") + J(k) : FString(TEXT("the ground floor")); };
			for (int32 k = 0; k < n; ++k)
			{
				const double fy = fyOf(k);
				const V2 c(x + fx * (r + .17), z + fz * (r + .17));
				const FFrame S = MakeFrameP(V2(c.X - rx * .8, c.Y - rz * .8), V2(c.X + rx * .8, c.Y + rz * .8), V2(x + fx * 9, z + fz * 9));
				o.YBox(x + fx * (r - .12), fy, z + fz * (r - .12), 2.2, 2.75, .56, -yaw, plaster, 3);
				WallQ(S, .05, .78, fy + .03, fy + 2.25, 0, Mul(Metal, 1.5), 19); WallQ(S, .82, 1.55, fy + .03, fy + 2.25, 0, Mul(Metal, 1.5), 19);
				WallQ(S, .05, 1.55, fy + 2.32, fy + 2.56, 0, lit97, 1);
				Prism(Round(x, z, r + .03, r + .03, 24), fy + 4.3, fy + 4.42, lit97, cream, 1, 3);
				const double ux = x + fx * (r + .9), uz = z + fz * (r + .9), back = yaw + PI_;
				if (k < n - 1) { const V3 tt = to(k + 1); o.Use(ux - rx * .35, fy, uz - rz * .35, TEXT("lift"), TEXT("Lift: up to floor ") + J(k + 1), back, &tt); }
				if (k > 0) { const V3 tt = to(k - 1); o.Use(ux + rx * .35, fy, uz + rz * .35, TEXT("lift"), TEXT("Lift: down to ") + floorName(k - 1), back, &tt); }
				if (k == 0 && n > 2) { const V3 tt = to(n - 1); o.Use(ux + rx * .35, fy, uz + rz * .35, TEXT("lift"), TEXT("Lift: up to floor ") + J(n - 1), back, &tt); }
				if (k == n - 1 && n > 2) { const V3 tt = to(0); o.Use(ux - rx * .35, fy, uz - rz * .35, TEXT("lift"), TEXT("Lift: down to the ground floor"), back, &tt); }
			}
		}

		// ---- the egg halls: the egg seen from inside, lanterns hanging in a ring, a stall where each of its staff stands, a tree in the middle
		TMap<FString, const FUnit*> anchors;
		for (const FUnit& u : L.Levels[0].Units) if (u.Kind == TEXT("anchor")) anchors.Add(u.Id, &u);
		for (const FHall& h : L.Halls)
		{
			const double px = h.Pos.X, pz = h.Pos.Y;
			if (L.Domes.IsValidIndex(h.Dome)) Egg(L.Domes[h.Dome], Hh - .6, true);
			for (int32 i = 0; i < 6; ++i) { const double a = 6.2832 * (i + .5) / 6; Lantern(px + h.R * .62 * std::cos(a), 6.4 + (i % 2) * 1.6, pz + h.R * .62 * std::sin(a), Hh + 6, .7); }
			if (const FUnit* const* au = anchors.Find(h.Unit))
				for (const V2& s : (*au)->Stand)
				{
					const double dx = px - s.X, dz = pz - s.Y, dh = Hyp(dx, dz), d = dh != 0 ? dh : 1, yaw = std::atan2(dx, -dz), cx = s.X + dx / d * 1.0, cz = s.Y + dz / d * 1.0;
					o.YBox(cx, F, cz, 2.8, 1.0, .8, -yaw, wood, 19); o.YBox(cx, F + 1.0, cz, 3.0, .08, 1.0, -yaw, Cream, 19); o.Solid(cx - .9, F, cz - .9, cx + .9, F + 1.0, cz + .9);
				}
			Prism(Round(px, pz, 3.2, 3.2, 16), F, F + .7, Stone, V3(.2, .15, .11), 3, 19); SolidAlong(V2(px - 2.4, pz), V2(px + 2.4, pz), F, F + .7, 2.4); Tree(px, pz, F + .7, TEXT("linden"), h.Unit);
		}
		if (L.bRoofLight) Egg(L.RoofLight, L.RoofLight.Base, true);

		// ---- the court floor: the fountain, the gardens and their trees, benches, kiosks, lanterns under the galleries
		for (const FPlace& f : L.Fountains)
		{
			const double x = f.Pos.X, z = f.Pos.Y, r = f.Yaw;
			Prism(Round(x, z, r, r, 28), F, F + .75, Stone, Stone, 3, 3); Prism(Round(x, z, r - .6, r - .6, 28), F + .75, F + .78, Water, Mul(Water, 1.5), 19, 19);
			Prism(Round(x, z, 1.1, 1.1, 12), F + .5, F + 2.6, Stone, Stone, 3, 3); Prism(Round(x, z, 2.6, 2.6, 16), F + 2.6, F + 2.9, Stone, Water, 3, 19); Prism(Round(x, z, .35, .35, 8), F + 2.9, F + 4.6, V3(.62, .7, .74), V3(.62, .7, .74), 19, 19);
			for (int32 i = 0; i < 20; ++i) { const double a = 6.2832 * i / 20; o.Solid(x + r * std::cos(a) - .8, F, z + r * std::sin(a) - .8, x + r * std::cos(a) + .8, F + .75, z + r * std::sin(a) + .8); }
		}
		for (const FGarden& g : L.Gardens)
		{
			const double x = g.Pos.X, z = g.Pos.Y;
			Prism(Round(x, z, g.R0, g.R1, 28), F, F + .42, Stone, V3(.25, .33, .2), 3, 19);
			for (int32 i = 0; i < g.Trees.Num(); ++i) Tree(g.Trees[i].X, g.Trees[i].Z, F + .42, g.Trees[i].Sp, J(x) + TEXT(",") + J(z) + J(i));
			if (g.bLow)
				for (int32 i = 0; i < 16; ++i)
				{
					const double a = 6.2832 * H({ J(x), J(z), J(i), TEXT("a") }), d = std::sqrt(H({ J(x), J(z), J(i), TEXT("d") })) * .8;
					Blob(x + g.R0 * d * std::cos(a), F + .42 + .35, z + g.R1 * d * std::sin(a), .7 + .5 * H({ J(x), J(z), J(i), TEXT("r") }), leafC, TEXT("low") + J(i));
				}
			for (int32 i = 0; i < 28; ++i) { const double a = 6.2832 * i / 28; o.Solid(x + (g.R0 - .5) * std::cos(a) - .6, F, z + (g.R1 - .5) * std::sin(a) - .6, x + (g.R0 - .5) * std::cos(a) + .6, F + (g.bLow ? 1.2 : .42), z + (g.R1 - .5) * std::sin(a) + .6); }
		}
		for (const FPlanter& q : L.Planters)
		{
			const double x = q.Pos.X, z = q.Pos.Y, y = q.Level * st;
			Prism(Round(x, z, q.R, q.R, 16), y, y + .6, fascia, V3(.2, .15, .11), 3, 19); Prism(Round(x, z, q.R + .12, q.R + .12, 16), y + .5, y + .62, wood, wood, 19, 19);
			for (int32 i = 0; i < 8; ++i) { const double a = 6.2832 * i / 8; o.Solid(x + q.R * .6 * std::cos(a) - .75, y, z + q.R * .6 * std::sin(a) - .75, x + q.R * .6 * std::cos(a) + .75, y + 1.0, z + q.R * .6 * std::sin(a) + .75); }
			Tree(x, z, y + .6, q.Tree, TEXT("pl") + J(x) + J(z), .5); Blob(x + .9, y + .85, z - .5, .55, leafC, TEXT("pa") + J(x) + J(z)); Blob(x - .8, y + .8, z + .7, .5, leafC, TEXT("pb") + J(x) + J(z));
		}
		for (const FRide& q : L.Rides) Ride(q);
		for (const FPlace& b : L.Benches)
		{
			const double yaw = b.Yaw * PI_ / 180, x = b.Pos.X, z = b.Pos.Y, fx = std::sin(yaw), fz = -std::cos(yaw);
			o.YBox(x, F + .3, z, 1.9, .16, .55, -yaw, wood, 19); o.YBox(x - fx * .3, F + .46, z - fz * .3, 1.9, .5, .1, -yaw, wood, 19);
			for (const double sgn : { -.8, .8 }) o.YBox(x + fz * sgn * -1, F, z + fx * sgn, .12, .3, .5, -yaw, Metal, 19);
			o.Solid(x - .6, F, z - .6, x + .6, F + .46, z + .6); o.Use(x, F + .47, z, TEXT("sit"), TEXT("Sit on the bench"), yaw);
			for (const double sd : { -.45, .45 }) o.Spot(ToFixed2(x + std::cos(yaw) * sd), ToFixed2(z + std::sin(yaw) * sd), 2, yaw, F + .46);
		}
		const V3 ACC[] = { Ochre, Terracotta, Teal, Blue, Olive, Plum };
		for (const FUnit& u : L.Levels[0].Units)
		{
			if (u.Kind != TEXT("kiosk")) continue;
			const double a = u.Rect[0], b = u.Rect[1], c = u.Rect[2], d = u.Rect[3];
			const V3 acc = ACC[(int32)FMath::FloorToDouble(H({ u.Id, TEXT("k") }) * 6)];
			const V3 acc7 = Mul(acc, .7);
			o.Box(a, F, b, c, F + 1.05, d, wood, 19); o.Box(a - .3, F + 2.7, b - .3, c + .3, F + 2.95, d + .3, acc, 19, 2, &acc7, 19);
			for (const V2 xz : { V2(a, b), V2(c, b), V2(a, d), V2(c, d) }) o.Box(xz.X - .08, F + 1.05, xz.Y - .08, xz.X + .08, F + 2.7, xz.Y + .08, Metal, 19);
			o.Box((a + c) / 2 - .2, F + 2.2, (b + d) / 2 - .2, (a + c) / 2 + .2, F + 2.6, (b + d) / 2 + .2, BrixSlice::Lantern, 2, 2, &BrixSlice::Lantern, 2); o.Solid(a, F, b, c, F + 1.05, d);
		}
		const double under = st - T;
		for (const FEntrance& e : L.Entrances)
		{
			const bool ns = e.Side == TEXT("n") || e.Side == TEXT("s");
			const double sg = e.Side == TEXT("n") || e.Side == TEXT("w") ? 1 : -1;
			for (double d = 10; d < 70; d += 15) Lantern(e.At.X + (ns ? 0 : sg * d), F + 3.9, e.At.Y + (ns ? sg * d : 0), F + under, .55);
		}
		if (L.Levels[1].Voids.Num())
		{
			const Poly& v = L.Levels[1].Voids[0];
			const TArray<V2> out = Outward(v);
			for (int32 i = 0; i < v.Num(); i += 11) Lantern(v[i].X + out[i].X * 7.5, F + 3.9, v[i].Y + out[i].Y * 7.5, F + under, .55);
		}

		// ---- where the staff stand (CITY-105), and the visitors' places (WLD-068, CITY-106)
		for (int32 k = 0; k < L.Levels.Num(); ++k)
			for (const FUnit& u : L.Levels[k].Units)
			{
				if (k && u.Outlet.IsEmpty()) continue;
				for (const V2& s : u.Stand)
				{
					const FHall* h = u.Kind == TEXT("anchor") ? L.Halls.FindByPredicate([&](const FHall& q) { return q.Unit == u.Id; }) : nullptr;
					o.Spot(s.X, s.Y, 0, h ? std::atan2(h->Pos.X - s.X, -(h->Pos.Y - s.Y)) : u.Angle * PI_ / 180, k ? k * st : F);
				}
			}
		for (const TArray<FPlace>& role : L.Staff) for (const FPlace& q : role) o.Spot(q.Pos.X, q.Pos.Y, 0, q.Yaw * PI_ / 180, q.Level ? q.Level * st : F);
		for (const FPlace& q : L.Customers) o.Spot(q.Pos.X, q.Pos.Y, 0, q.Yaw * PI_ / 180, q.Level ? q.Level * st : F);
		for (const FPlace& q : L.Seats) o.Spot(q.Pos.X, q.Pos.Y, 2, q.Yaw * PI_ / 180, (q.Level ? q.Level * st : F) + .45);
	}
}

bool BrixMallCan(const FJsonObject& Lot)
{
	const TSharedPtr<FJsonObject>* P, * S;
	if (!Lot.TryGetObjectField(TEXT("plan"), P) || !Lot.TryGetObjectField(TEXT("shell"), S)) return false;
	FString Concept, Style;
	const TArray<TSharedPtr<FJsonValue>>* E, * Pts;
	const TSharedPtr<FJsonObject>* Skin;
	return (*P)->TryGetStringField(TEXT("concept"), Concept) && Concept == TEXT("court") && (*P)->TryGetArrayField(TEXT("entrances"), E)
		&& (*S)->TryGetStringField(TEXT("style"), Style) && Style == TEXT("ribbon") && (*S)->TryGetObjectField(TEXT("skin"), Skin) && (*Skin)->TryGetArrayField(TEXT("points"), Pts) && Pts->Num() > 8;
}

bool BuildBrixMall(const FJsonObject& Lot, FBrixMall& Out, FString& OutError)
{
	if (!BrixMallCan(Lot)) { OutError = TEXT("not a mall with its inside built"); return false; }
	FLotData L;
	if (!Read(Lot, L, OutError)) return false;
	Out = FBrixMall();
	Out.LotId = L.Id;
	Lot.TryGetStringField(TEXT("name"), Out.Name);
	Out.X0 = L.Rect[0]; Out.Z0 = L.Rect[1]; Out.X1 = L.Rect[2]; Out.Z1 = L.Rect[3];
	Out.H = L.H; Out.StoreyHeight = L.St; Out.Levels = L.Levels.Num();
	Out.Calls.Reserve(200000);
	FOut o(Out);
	FBuild B(L, o, Out);
	Out.F = B.F;
	B.Hall();
	B.Skin();
	for (const FDome& d : L.Domes) B.Egg(d, 0, false);
	if (L.bRoofLight) B.Egg(L.RoofLight, L.RoofLight.Base, false);
	B.ShellLanterns();
	B.Draw();
	return true;
}

bool LoadBrixMall(const FString& CityDir, FBrixMall& Out, FString& OutError)
{
	FString Text;
	if (!FFileHelper::LoadFileToString(Text, *FPaths::Combine(CityDir, TEXT("city.json")))) { OutError = TEXT("no city.json in ") + CityDir; return false; }
	TSharedPtr<FJsonObject> City;
	if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), City) || !City) { OutError = TEXT("city.json is not JSON"); return false; }
	for (const TSharedPtr<FJsonValue>& BV : City->GetArrayField(TEXT("blocks")))
	{
		FString File;
		if (!BV->AsObject()->TryGetStringField(TEXT("file"), File)) continue;
		FString BText;
		if (!FFileHelper::LoadFileToString(BText, *FPaths::Combine(CityDir, File))) continue;
		if (!BText.Contains(TEXT("\"court\""))) continue;   // (only a block with a court mall needs reading through)
		TSharedPtr<FJsonObject> Blk;
		if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BText), Blk) || !Blk) continue;
		for (const TSharedPtr<FJsonValue>& LV : Blk->GetArrayField(TEXT("lots")))
			if (BrixMallCan(*LV->AsObject())) return BuildBrixMall(*LV->AsObject(), Out, OutError);
	}
	OutError = TEXT("no mall with its inside built in the block files");
	return false;
}

bool BrixMallCarry(const FBrixMall& M, FVector3d& P, double Dt)
{
	for (const FBrixMovingStair& r : M.Moving)
	{
		const double qx = P.X - r.A.X, qz = P.Z - r.A.Y, f = (qx * r.T.X + qz * r.T.Y) / r.Lr, side = qx * r.S.X + qz * r.S.Y;
		if (f < 0 || f > 1 || std::fabs(side) > r.Hw || std::fabs(P.Y - (r.Ya + (r.Yb - r.Ya) * f)) > .9) continue;
		const double d = (side > 0 ? 1 : -1) * r.V * Dt;
		P.X += r.T.X * d; P.Z += r.T.Y * d;
		return true;
	}
	return false;
}

double BrixMallStepUnder(const FBrixMall& M, const FVector3d& P)
{
	for (const FBrixMovingStair& r : M.Moving)
	{
		const double qx = P.X - r.A.X, qz = P.Z - r.A.Y, f = (qx * r.T.X + qz * r.T.Y) / r.Lr, side = qx * r.S.X + qz * r.S.Y;
		if (f < 0 || f > 1 || std::fabs(side) > r.Hw || std::fabs(P.Y - (r.Ya + (r.Yb - r.Ya) * f)) > .9) continue;
		const int32 i = FMath::Min(29, (int32)FMath::FloorToDouble(f * 30));
		return r.Ya + (r.Yb - r.Ya) * (i + 1) / 30;
	}
	return -1;
}

const FBrixOutletRoom* BrixOutletAt(const FBrixMall& M, double X, double Y, double Z)
{
	for (const FBrixOutletRoom& r : M.Rooms)
	{
		if (Y < r.Fy - .6 || Y > r.Fy + 3.3) continue;
		const double dx = X - r.X, dz = Z - r.Z, a = dx * r.T.X + dz * r.T.Y, v = -(dx * r.N.X + dz * r.N.Y);
		if (a >= r.A0 && a <= r.A1 && v >= -.2 && v <= r.D) return &r;
	}
	return nullptr;
}

FVector3d BrixMallShellTone(int32 Tone) { return ShellTone(Tone); }

double BrixMallChecksum(const FBrixMallCall& r)
{
	double s = (r.Kind + 1) * 1000.0;
	for (int32 j = 0; j < r.A.Num(); ++j) s += r.A[j] * (1 + (j % 5) * .1);
	if (r.bCol) s += r.Col.X * 3 + r.Col.Y * 5 + r.Col.Z * 7;
	s += r.Mat * 11;
	for (const TArray<double, TInlineAllocator<5>>& q : r.Faces) { s += q[0] * 13; if (q.Num() > 1) s += q[1] * 3 + q[2] * 5 + q[3] * 7 + q[4] * 11; else s += 17; }
	if (!r.Text.IsEmpty()) s += r.Text.Len() * 19;
	return s;
}
