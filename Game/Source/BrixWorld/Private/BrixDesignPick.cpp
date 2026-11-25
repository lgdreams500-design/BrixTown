#include "BrixDesignPick.h"

namespace
{
	double Fixed2(double V) { return FCString::Atod(*FString::Printf(TEXT("%.2f"), V)); }
	double GridUp(double Grid, double V) { return Fixed2(FMath::CeilToDouble(V / Grid - 1e-6) * Grid); }
	double GridDown(double Grid, double V) { return Fixed2(FMath::FloorToDouble(V / Grid + 1e-6) * Grid); }

	// where a ray goes into a box lying along the axes (the slab test): how far along the ray, or none
	TOptional<double> IntoBox(const FVector3d& O, const FVector3d& D, const FVector3d& Lo, const FVector3d& Hi)
	{
		double T0 = 0, T1 = 1e9;
		for (int32 k = 0; k < 3; ++k)
		{
			if (FMath::Abs(D[k]) < 1e-9) { if (O[k] < Lo[k] || O[k] > Hi[k]) return {}; continue; }
			double A = (Lo[k] - O[k]) / D[k], B = (Hi[k] - O[k]) / D[k];
			if (A > B) Swap(A, B);
			T0 = FMath::Max(T0, A);
			T1 = FMath::Min(T1, B);
		}
		if (T0 > T1) return {};
		return T0;
	}
}

namespace BrixDesignPick
{
	TOptional<FVector2D> FloorHit(const FBrixRay& Ray, double FloorY)
	{
		if (Ray.D.Y > -1e-4) return {};
		const double T = (FloorY - Ray.O.Y) / Ray.D.Y;
		if (!(T > 0)) return {};
		return FVector2D(Ray.O.X + Ray.D.X * T, Ray.O.Z + Ray.D.Z * T);
	}

	int32 PickPiece(const FBrixLayout& Lay, double FloorY, const FBrixRay& Ray)   // play.js pickPiece()
	{
		int32 Best = INDEX_NONE;
		double Bt = 1e9;
		for (int32 i = 0; i < Lay.Num(); ++i)
		{
			const FBrixLayoutPiece& P = Lay[i];
			const double Y = FMath::DegreesToRadians(P.Yaw), c = FMath::Cos(Y), sn = FMath::Sin(Y), Ox = Ray.O.X - P.X, Oz = Ray.O.Z - P.Z;
			// into the piece's own frame
			const FVector3d O(c * Ox - sn * Oz, Ray.O.Y - FloorY, sn * Ox + c * Oz), D(c * Ray.D.X - sn * Ray.D.Z, Ray.D.Y, sn * Ray.D.X + c * Ray.D.Z);
			const TOptional<double> T = IntoBox(O, D, FVector3d(-P.W / 2, 0, -P.D / 2), FVector3d(P.W / 2, FMath::Max(P.H, .05), P.D / 2));
			if (!T) continue;
			const double Tt = *T + (P.bRug ? 50 : 0);   // a rug only when nothing stands there
			if (Tt < Bt) { Bt = Tt; Best = i; }
		}
		return Best;
	}

	int32 PickWall(const FBrixRoomPlanRules& R, const FBrixPlan& Plan, double FloorY, double CutM, const FBrixRay& Ray)   // play.js pickWall()
	{
		int32 Best = INDEX_NONE;
		double Bt = 1e9;
		for (int32 i = 0; i < Plan.Num(); ++i)
		{
			const FVector4f B = BrixRoomPlan::Box(R, Plan[i]);
			const TOptional<double> T = IntoBox(Ray.O, Ray.D, FVector3d(B.X, FloorY, B.Y), FVector3d(B.Z, FloorY + CutM, B.W));
			if (T && *T < Bt) { Bt = *T; Best = i; }
		}
		return Best;
	}

	FVector2D GrabPiece(const FBrixLayoutPiece& P, const FVector2D& Hit) { return FVector2D(P.X - Hit.X, P.Z - Hit.Y); }

	FVector2D DragPiece(const FBrixFurnishRulesD& F, const FVector2D& Hit, const FVector2D& Grab)
	{
		return FVector2D(BrixFurnishPlan::Snap(F, Hit.X + Grab.X), BrixFurnishPlan::Snap(F, Hit.Y + Grab.Y));
	}

	FWallGrab GrabWall(const FBrixPlanWall& W, const FVector2D& Hit)
	{
		const bool bX = W.Ax == TEXT("x");
		return { W.Fix - (bX ? Hit.Y : Hit.X), W.A0 - (bX ? Hit.X : Hit.Y) };
	}

	TArray<double> WallStops(const FBrixPlan& Plan, const FBrixPlanWall& W, double Fix, double Lo, double Hi)   // life.js wallStops()
	{
		TArray<double> Out = { Lo, Hi };
		for (const FBrixPlanWall& Q : Plan) if (Q.Id != W.Id && Q.Ax != W.Ax && Q.A0 - .01 <= Fix && Fix <= Q.A1 + .01) Out.Add(Q.Fix);
		Out.Sort();
		return Out;
	}

	FBrixPlanWall DragWall(const FBrixRoomPlanRules& R, const FBrixPlan& Plan, const FBrixPlanWall& W, const FVector4f& Room, const FVector2D& Hit, const FWallGrab& G)   // life.js pointer(), grab.wall
	{
		const bool bX = W.Ax == TEXT("x");
		const double Flo = bX ? Room.Y : Room.X, Fhi = bX ? Room.W : Room.Z, Lo = bX ? Room.X : Room.Y, Hi = bX ? Room.Z : Room.W;
		const double Fix = BrixRoomPlan::Gridded(R, FMath::Clamp((bX ? Hit.Y : Hit.X) + G.Off, Flo + .2, Fhi - .2)), Len = W.A1 - W.A0;
		double A0 = BrixRoomPlan::Gridded(R, FMath::Clamp((bX ? Hit.X : Hit.Y) + G.OffA, Lo, Hi - Len));
		const double Reach = .3;
		const TArray<double> Stops = WallStops(Plan, W, Fix, Lo, Hi);
		const double* S0 = Stops.FindByPredicate([&](double C) { return FMath::Abs(A0 - C) < Reach; });
		const double* S1 = Stops.FindByPredicate([&](double C) { return FMath::Abs(A0 + Len - C) < Reach; });
		if (S0) A0 = GridUp(R.Grid, *S0);
		else if (S1) A0 = GridDown(R.Grid, *S1) - Len;
		A0 = BrixRoomPlan::Gridded(R, FMath::Clamp(A0, GridUp(R.Grid, Lo), GridDown(R.Grid, Hi) - Len));
		FBrixPlanWall Out = W;
		Out.Fix = Fix;
		Out.A0 = A0;
		Out.A1 = BrixRoomPlan::Gridded(R, A0 + Len);
		if (W.Door) Out.Door = BrixRoomPlan::Gridded(R, *W.Door + (A0 - W.A0));
		return Out;
	}

	TArray<FMarkBox> Marks(const FBrixLayoutPiece& P, double FloorY, bool bPicked)   // play.js designMarks()
	{
		TArray<FMarkBox> Out;
		const double Y = FMath::DegreesToRadians(P.Yaw), c = FMath::Cos(Y), sn = FMath::Sin(Y), e = .07, w = P.W + 2 * e, d = P.D + 2 * e, y = FloorY + .012, t = bPicked ? .05 : .03;
		auto At = [&](double u, double v) { return FVector2D(P.X + c * u + sn * v, P.Z - sn * u + c * v); };
		for (const double Sg : { -1.0, 1.0 })
		{
			const FVector2D A = At(0, Sg * d / 2), B = At(Sg * w / 2, 0);
			Out.Add({ FVector3d(A.X, y, A.Y), FVector3d(w + t, .02, t), P.Yaw });
			Out.Add({ FVector3d(B.X, y, B.Y), FVector3d(t, .02, d + t), P.Yaw });
		}
		if (bPicked)
			for (const FVector2D& S : { FVector2D(-1, -1), FVector2D(1, -1), FVector2D(-1, 1), FVector2D(1, 1) })
			{
				const FVector2D Q = At(S.X * w / 2, S.Y * d / 2);
				Out.Add({ FVector3d(Q.X, y, Q.Y), FVector3d(.09, FMath::Min(.5, P.H + .15), .09), P.Yaw });
			}
		return Out;
	}

	FBrixLayoutPiece WallAsPiece(const FBrixRoomPlanRules& R, const FBrixPlanWall& W)
	{
		const FVector4f B = BrixRoomPlan::Box(R, W);
		FBrixLayoutPiece P;
		P.Id = W.Id;
		P.X = (B.X + B.Z) / 2.0; P.Z = (B.Y + B.W) / 2.0; P.W = B.Z - B.X; P.D = B.W - B.Y; P.H = 1.2; P.Yaw = 0;
		return P;
	}
}
