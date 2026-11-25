#include "BrixFurnish.h"

#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixInside.h"

namespace
{
	// play.js C (the slice's palette) and ROOM_WOOD
	const FVector3f Cream(.66f, .61f, .50f), Ochre(.52f, .40f, .20f), Terracotta(.44f, .24f, .18f), Teal(.20f, .36f, .37f), Blue(.30f, .38f, .52f),
		Olive(.30f, .33f, .20f), Plum(.30f, .24f, .34f), Charcoal(.09f, .09f, .11f), Metal(.17f, .18f, .21f), Wood(.36f, .24f, .15f);

	// play.js ROOM_STYLE: what each kind of room's chairs, counters and lamps are, what stands on its counters
	enum class ETool : uint8 { None, Espresso, Till, Vise, Ledger };
	struct FStyle { TArray<FVector3f> Fabric; TOptional<FVector3f> Chair, Counter, Top; ETool Tool = ETool::None; TOptional<FVector3f> Lamp; };
	const FStyle& StyleOf(const FString& Sp, const FString& Kind)
	{
		static const TMap<FString, FStyle> Styles = {
			{ TEXT("home"), { { Olive, Plum, FVector3f(.55f, .35f, .24f), Blue }, {}, {}, {}, ETool::None, Cream } },
			{ TEXT("food"), { { Cream }, FVector3f(.42f, .3f, .2f), FVector3f(.3f, .2f, .13f), FVector3f(.62f, .58f, .52f), ETool::Espresso, Ochre } },
			{ TEXT("shop"), { { Cream }, {}, FVector3f(.32f, .22f, .14f), Teal, ETool::Till, Cream } },
			{ TEXT("work"), { { Blue, Olive }, {}, {}, {}, ETool::Ledger, Teal } },
			{ TEXT("office"), { { Blue, Charcoal }, {}, {}, {}, ETool::Ledger, Teal } },
			{ TEXT("guild"), { { Terracotta }, {}, {}, {}, ETool::Ledger, Ochre } },
			{ TEXT("hall"), { { Terracotta }, {}, {}, {}, ETool::Ledger, Cream } },
			{ TEXT("works"), { { Charcoal }, {}, FVector3f(.36f, .26f, .18f), FVector3f(.42f, .3f, .2f), ETool::Vise, Metal } },
			{ TEXT("health"), { { Teal }, FVector3f(.6f, .62f, .62f), FVector3f(.8f, .8f, .76f), Teal, ETool::Ledger, Cream } },
			{ TEXT("hospital"), { { Teal }, FVector3f(.6f, .62f, .62f), FVector3f(.8f, .8f, .76f), Teal, ETool::Ledger, Cream } },
			{ TEXT("utility"), { { Charcoal }, {}, {}, {}, ETool::Ledger, Metal } },
			{ TEXT("mall"), { { Cream }, {}, FVector3f(.32f, .22f, .14f), Ochre, ETool::Till, Ochre } },
			{ TEXT("flats"), { { Olive }, {}, {}, {}, ETool::Ledger, Cream } },
			{ TEXT("bank"), { { Teal }, FVector3f(.42f, .3f, .2f), FVector3f(.3f, .2f, .13f), FVector3f(.72f, .7f, .64f), ETool::Ledger, Cream } },
			{ TEXT("library"), { { Olive }, FVector3f(.36f, .24f, .15f), FVector3f(.3f, .2f, .13f), FVector3f(.24f, .34f, .26f), ETool::Ledger, Olive } },
			{ TEXT("museum"), { { Terracotta }, {}, FVector3f(.3f, .2f, .13f), FVector3f(.62f, .58f, .52f), ETool::Ledger, Cream } },
			{ TEXT("cinema"), { { FVector3f(.42f, .1f, .1f) }, {}, FVector3f(.42f, .1f, .1f), FVector3f(.62f, .5f, .3f), ETool::Till, FVector3f(.42f, .1f, .1f) } },
			{ TEXT("dealer"), { { Charcoal }, {}, FVector3f(.32f, .34f, .36f), FVector3f(.8f, .8f, .78f), ETool::Ledger, Cream } },
		};
		if (const FStyle* S = Styles.Find(Sp)) return *S;
		if (const FStyle* S = Styles.Find(Kind)) return *S;
		return Styles[TEXT("work")];
	}
	template <typename T> const T& Pick(const TArray<T>& List, double R) { return List[FMath::Min(List.Num() - 1, (int32)(R * List.Num()))]; }
	FString JsNum(double V) { FString S = FString::SanitizeFloat(V); if (S.EndsWith(TEXT(".0"))) S.LeftChopInline(2); return S; }   // as JS writes a number
	double YawOf(double Dx, double Dz) { return FMath::Atan2(Dx, -Dz); }   // play.js yawOf
	FVector2D Facing(double Yaw) { return FVector2D(FMath::Sin(Yaw), -FMath::Cos(Yaw)); }   // the way a yaw faces
}

bool BrixFurnishFloor(const FBrixInsides& Ins, const FBrixLot& L, const FBrixInside& In, int32 Floor, FBrixFurnished& Out)
{
	Out = FBrixFurnished();
	if (!BrixIsHollow(L, &In) || L.House.IsSet() || !In.Interior.IsEmpty()) return false;   // planned homes: BrixHomes
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
		for (int32 i = 0; i < All->Num(); ++i) if (Lv ? (Lv->IsValidIndex(i) && (*Lv)[i] == Floor) : Floor == 0) P.Add((*All)[i]);
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
	const FVector3f Fab = Pick(R.Fabric, BrixSliceHash({ L.Id, TEXT("fab") }));
	const int32 N0 = Out.Boxes.Num();

	for (const FVector2D& P : Places(TEXT("bed")))   // a framed bed with a headboard, blanket and pillow, a nightstand and its lamp
	{
		const double x = P.X, z = P.Y;
		const FVector3f Bl = Pick(TArray<FVector3f>{ Cream, Blue, Olive, Plum, Terracotta }, BrixSliceHash({ L.Id, JsNum(x), JsNum(z) }));
		B(x, 0, z, 1.0, .32, 2.0, Wood); B(x, .32, z, .94, .2, 1.92, FVector3f(.78f, .74f, .66f)); B(x, .5, z - .3, .98, .1, 1.36, Bl); B(x, .52, z + .68, .62, .14, .34, FVector3f(.85f, .82f, .76f));
		B(x, 0, z + 1.02, 1.06, .98, .08, Wood); B(x, 0, z - 1.02, 1.06, .5, .07, Wood);
		if (BrixSliceHash({ L.Id, JsNum(x), JsNum(z), TEXT("ns") }) < .7)
		{
			const double sx = x + (BrixSliceHash({ L.Id, JsNum(z), TEXT("s") }) < .5 ? -.82 : .82);
			B(sx, 0, z + .75, .42, .5, .38, Wood); B(sx, .5, z + .75, .12, .22, .12, FVector3f(.9f, .8f, .6f), 0, true);
		}
		Hard(x - .5, 0, z - 1, x + .5, .5, z + 1);
		Spot(x, z, 3, 0, .62);
	}
	{
		const TArray<FVector2D> Rooms = Places(TEXT("room"));
		for (int32 k = 0; k < Rooms.Num(); ++k)
		{
			const double Yaw = FMath::FloorToDouble(BrixSliceHash({ L.Id, JsNum(k), TEXT("r") }) * 4) * PI / 2;
			Chair(Rooms[k].X, Rooms[k].Y, Yaw, Fab, Kind == TEXT("home"));
			Spot(Rooms[k].X, Rooms[k].Y, 2, Yaw, .45);
		}
	}
	for (const FVector2D& P : Places(TEXT("chair"))) { Chair(P.X, P.Y, toDoor, R.Chair.Get(Cream), false); Spot(P.X, P.Y, 2, toDoor, .45); }
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
				Spot(x, z, 2, fromDoor, .45);
				continue;
			}
			const double sgn = k % 2 ? -1 : 1;
			Chair(x, z, YawOf(sgn, 0), R.Chair.Get(Wood), false);
			Table(x + sgn * .7, z, .64, .74, Kind == TEXT("food"));
			if (Kind == TEXT("food") || Sp == TEXT("mall")) Chair(x + sgn * 1.4, z, YawOf(-sgn, 0), R.Chair.Get(Wood), false);   // a table for two
			Hard(x + sgn * .7 - .31, 0, z - .31, x + sgn * .7 + .31, .74, z + .31);
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
		Hard(cx - sx / 2, 0, cz - sz / 2, cx + sx / 2, h, cz + sz / 2);
		Spot(x, z, desk ? 2 : 0, toDoor, desk ? .45 : 0);
	}
	for (const FVector2D& P : Places(TEXT("customer"))) Spot(P.X, P.Y, 0, fromDoor, 0);
	return Out.Boxes.Num() > N0;
}
