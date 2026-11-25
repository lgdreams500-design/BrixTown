#include "BrixRoofs.h"

#include "BrixCity.h"

double FBrixRoofMap::TopAt(const FVector2D& P) const
{
	const int32 x = FMath::FloorToInt((P.X - Origin.X) / Cell), y = FMath::FloorToInt((P.Y - Origin.Y) / Cell);
	if (x < 0 || y < 0 || x >= W || y >= H) return 0;
	return Px[y * W + x] * Step;
}

FBrixRoofMap BrixRoofMap(const FBrixCity& City, double Cell, double Step, TFunctionRef<double(const FBrixLot&)> TopOf)
{
	FBrixRoofMap M;
	M.Cell = Cell;
	M.Step = Step;
	struct FIn { FVector2D C, Half; double Cos, Sin; uint8 V; };
	TArray<FIn> In;
	FBox2D All(ForceInit);
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			if (L.IsOpen()) continue;
			const double Top = TopOf(L);
			if (Top <= 0) continue;
			const double A = FMath::DegreesToRadians(L.AngleDeg);
			In.Add({ L.Centre(), L.Size() / 2, FMath::Cos(A), FMath::Sin(A), (uint8)FMath::Clamp(FMath::FloorToInt(Top / Step), 1, 255) });
			const double R = L.Size().Size() / 2;
			All += FBox2D(L.Centre() - FVector2D(R), L.Centre() + FVector2D(R));
		}
	if (!In.Num()) return M;
	M.Origin = FVector2D(FMath::FloorToDouble(All.Min.X / Cell) - 1, FMath::FloorToDouble(All.Min.Y / Cell) - 1) * Cell;
	M.W = FMath::CeilToInt((All.Max.X - M.Origin.X) / Cell) + 2;
	M.H = FMath::CeilToInt((All.Max.Y - M.Origin.Y) / Cell) + 2;
	M.Px.SetNumZeroed(M.W * M.H);
	for (const FIn& R : In)   // each cell whose middle is inside the footprint, as ABrixWeather::Roofed() tests a point
	{
		const double Reach = R.Half.Size();
		const int32 x0 = FMath::Max(0, FMath::FloorToInt((R.C.X - Reach - M.Origin.X) / Cell)), x1 = FMath::Min(M.W - 1, FMath::CeilToInt((R.C.X + Reach - M.Origin.X) / Cell));
		const int32 y0 = FMath::Max(0, FMath::FloorToInt((R.C.Y - Reach - M.Origin.Y) / Cell)), y1 = FMath::Min(M.H - 1, FMath::CeilToInt((R.C.Y + Reach - M.Origin.Y) / Cell));
		for (int32 y = y0; y <= y1; ++y)
			for (int32 x = x0; x <= x1; ++x)
			{
				const FVector2D D = M.Origin + FVector2D(x + .5, y + .5) * Cell - R.C, Lc(D.X * R.Cos + D.Y * R.Sin, -D.X * R.Sin + D.Y * R.Cos);
				if (FMath::Abs(Lc.X) <= R.Half.X && FMath::Abs(Lc.Y) <= R.Half.Y) { uint8& V = M.Px[y * M.W + x]; V = FMath::Max(V, R.V); }
			}
	}
	return M;
}
