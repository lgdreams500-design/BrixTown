#include "BrixMap.h"

#include "BrixEconomy.h"
#include "BrixQuests.h"
#include "Dom/JsonObject.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool ReadMapJson(const FString& Path, TSharedPtr<FJsonObject>& Out, FString& OutError)
	{
		FString Text;
		if (!FFileHelper::LoadFileToString(Text, *Path)) { OutError = FString::Printf(TEXT("Cannot read %s"), *Path); return false; }
		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(Text);
		if (!FJsonSerializer::Deserialize(Reader, Out) || !Out.IsValid()) { OutError = FString::Printf(TEXT("%s is not valid JSON"), *Path); return false; }
		return true;
	}

	FVector2D Pt(const TSharedPtr<FJsonValue>& V)
	{
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (!V.IsValid() || !V->TryGetArray(A) || A->Num() < 2) return FVector2D::ZeroVector;
		return FVector2D((*A)[0]->AsNumber(), (*A)[1]->AsNumber());
	}

	FVector2D PtOf(const TSharedPtr<FJsonObject>& O, const TCHAR* Field)
	{
		const TSharedPtr<FJsonValue> V = O->TryGetField(Field);
		return Pt(V);
	}

	bool ReadLine(const TSharedPtr<FJsonObject>& O, FBrixMapLine& Out)
	{
		const TArray<TSharedPtr<FJsonValue>>* P = nullptr;
		if (!O.IsValid() || !O->TryGetArrayField(TEXT("points"), P)) return false;
		for (const TSharedPtr<FJsonValue>& V : *P) Out.Points.Add(Pt(V));
		double W = 1;
		O->TryGetNumberField(TEXT("width"), W);
		Out.Width = (float)W;
		return Out.Points.Num() > 1;
	}

	FColor Hex(const TCHAR* S) { return FColor::FromHex(S); }

	const TMap<FString, FColor>& KindCss()
	{
		static const TMap<FString, FColor> M = {
			{ TEXT("school"), Hex(TEXT("d8b74a")) }, { TEXT("fire"), Hex(TEXT("b8443a")) }, { TEXT("police"), Hex(TEXT("4a5a9a")) },
			{ TEXT("home"), Hex(TEXT("b59a7c")) }, { TEXT("shop"), Hex(TEXT("c79a45")) }, { TEXT("food"), Hex(TEXT("b0614a")) },
			{ TEXT("work"), Hex(TEXT("6f86ab")) }, { TEXT("health"), Hex(TEXT("d9d3c2")) }, { TEXT("utility"), Hex(TEXT("4b4b55")) },
			{ TEXT("garden"), Hex(TEXT("6f8650")) }, { TEXT("market"), Hex(TEXT("d8c9a3")) }, { TEXT("stop"), Hex(TEXT("6fa3a2")) },
			{ TEXT("bank"), Hex(TEXT("8c7a3c")) }, { TEXT("dealer"), Hex(TEXT("5a7f8f")) }, { TEXT("library"), Hex(TEXT("7a5c8f")) },
			{ TEXT("museum"), Hex(TEXT("a08a6a")) }, { TEXT("furniture"), Hex(TEXT("a8744a")) }, { TEXT("cinema"), Hex(TEXT("8f3c4a")) } };
		return M;
	}
	const TMap<FString, FColor>& MapGreen()   // play.js MAP_GREEN: the open ground (WLD-056)
	{
		static const TMap<FString, FColor> M = { { TEXT("woods"), Hex(TEXT("4f6b3a")) }, { TEXT("common"), Hex(TEXT("5d7343")) }, { TEXT("orchard"), Hex(TEXT("5a6e3f")) },
			{ TEXT("fields"), Hex(TEXT("66703f")) }, { TEXT("plots"), Hex(TEXT("6c7449")) }, { TEXT("meadow"), Hex(TEXT("5d7343")) } };
		return M;
	}
	const TMap<FString, FColor>& MapCrop()
	{
		static const TMap<FString, FColor> M = { { TEXT("wheat"), Hex(TEXT("a8924a")) }, { TEXT("barley"), Hex(TEXT("9a8d55")) }, { TEXT("furrow"), Hex(TEXT("5a4632")) },
			{ TEXT("pasture"), Hex(TEXT("617a3e")) }, { TEXT("cabbage"), Hex(TEXT("4c6a3e")) }, { TEXT("rough"), Hex(TEXT("76804d")) } };
		return M;
	}

	// A small painter for the map, in its own pixels: shapes filled where a pixel's middle is inside; lines and discs with a
	// pixel of soft edge, as a canvas draws them.
	struct FPainter
	{
		int32 N;
		TArray<FColor>& P;
		void Blend(int32 X, int32 Y, const FColor& C, float A)
		{
			if (X < 0 || Y < 0 || X >= N || Y >= N || A <= 0.f) return;
			FColor& D = P[Y * N + X];
			if (A >= 1.f) { D = C; return; }
			D = FColor((uint8)FMath::RoundToInt(D.R + (C.R - D.R) * A), (uint8)FMath::RoundToInt(D.G + (C.G - D.G) * A), (uint8)FMath::RoundToInt(D.B + (C.B - D.B) * A), 255);
		}
		void Rect(double X0, double Y0, double X1, double Y1, const FColor& C)
		{
			const int32 A = FMath::Max(0, FMath::CeilToInt(X0 - .5)), B = FMath::Min(N - 1, FMath::FloorToInt(X1 - .5));
			const int32 Cy = FMath::Max(0, FMath::CeilToInt(Y0 - .5)), Dy = FMath::Min(N - 1, FMath::FloorToInt(Y1 - .5));
			for (int32 y = Cy; y <= Dy; ++y) for (int32 x = A; x <= B; ++x) P[y * N + x] = C;
		}
		void Poly(const TArray<FVector2D>& Q, const FColor& C)   // even-odd, by scanlines through the pixels' middles
		{
			if (Q.Num() < 3) return;
			double Y0 = Q[0].Y, Y1 = Q[0].Y;
			for (const FVector2D& V : Q) { Y0 = FMath::Min(Y0, V.Y); Y1 = FMath::Max(Y1, V.Y); }
			TArray<double> Xs;
			for (int32 y = FMath::Max(0, FMath::FloorToInt(Y0)); y <= FMath::Min(N - 1, FMath::CeilToInt(Y1)); ++y)
			{
				const double Yc = y + .5;
				Xs.Reset();
				for (int32 i = 0, j = Q.Num() - 1; i < Q.Num(); j = i++)
				{
					const FVector2D& A = Q[i]; const FVector2D& B = Q[j];
					if ((A.Y > Yc) != (B.Y > Yc)) Xs.Add(A.X + (Yc - A.Y) / (B.Y - A.Y) * (B.X - A.X));
				}
				Xs.Sort();
				for (int32 k = 0; k + 1 < Xs.Num(); k += 2)
					for (int32 x = FMath::Max(0, FMath::CeilToInt(Xs[k] - .5)); x <= FMath::Min(N - 1, FMath::FloorToInt(Xs[k + 1] - .5)); ++x) P[y * N + x] = C;
			}
		}
		void Seg(const FVector2D& A, const FVector2D& B, double W, const FColor& C)   // round ends
		{
			const double R = W / 2;
			const int32 X0 = FMath::Max(0, FMath::FloorToInt(FMath::Min(A.X, B.X) - R - 1)), X1 = FMath::Min(N - 1, FMath::CeilToInt(FMath::Max(A.X, B.X) + R + 1));
			const int32 Y0 = FMath::Max(0, FMath::FloorToInt(FMath::Min(A.Y, B.Y) - R - 1)), Y1 = FMath::Min(N - 1, FMath::CeilToInt(FMath::Max(A.Y, B.Y) + R + 1));
			const FVector2D V = B - A;
			const double L2 = FMath::Max(1e-9, V.SizeSquared());
			for (int32 y = Y0; y <= Y1; ++y)
				for (int32 x = X0; x <= X1; ++x)
				{
					const FVector2D Q(x + .5, y + .5);
					const double T = FMath::Clamp(FVector2D::DotProduct(Q - A, V) / L2, 0.0, 1.0);
					const double D = FVector2D::Distance(Q, A + V * T);
					const float Cov = (float)FMath::Clamp(R - D + .5, 0.0, 1.0);
					if (Cov > 0.f) { FColor& Px = P[y * N + x]; if (Px != C) Blend(x, y, C, Cov); }
				}
		}
		void Line(const TArray<FVector2D>& Q, double W, const FColor& C, bool bClosed = false)
		{
			for (int32 i = 1; i < Q.Num(); ++i) Seg(Q[i - 1], Q[i], W, C);
			if (bClosed && Q.Num() > 2) Seg(Q.Last(), Q[0], W, C);
		}
		void Disc(const FVector2D& O, double R, const FColor& C)
		{
			for (int32 y = FMath::Max(0, FMath::FloorToInt(O.Y - R - 1)); y <= FMath::Min(N - 1, FMath::CeilToInt(O.Y + R + 1)); ++y)
				for (int32 x = FMath::Max(0, FMath::FloorToInt(O.X - R - 1)); x <= FMath::Min(N - 1, FMath::CeilToInt(O.X + R + 1)); ++x)
					Blend(x, y, C, (float)FMath::Clamp(R - FVector2D::Distance(FVector2D(x + .5, y + .5), O) + .5, 0.0, 1.0));
		}
		TArray<FVector2D> Ellipse(const FVector2D& O, const FVector2D& R) const
		{
			TArray<FVector2D> Q;
			for (int32 k = 0; k < 48; ++k) { const double A = k * UE_DOUBLE_TWO_PI / 48; Q.Add(O + FVector2D(FMath::Cos(A) * R.X, FMath::Sin(A) * R.Y)); }
			return Q;
		}
	};
}

bool LoadBrixMapExtras(const FString& CityDir, const FBrixCity& City, FBrixMapExtras& Out, FString& OutError)
{
	Out = FBrixMapExtras();
	TSharedPtr<FJsonObject> Root;
	if (!ReadMapJson(FPaths::Combine(CityDir, TEXT("city.json")), Root, OutError)) return false;
	Out.Wall = FIntRect(0, 0, City.Columns, City.Rows);
	const TSharedPtr<FJsonObject>* Wall = nullptr;
	const TArray<TSharedPtr<FJsonValue>>* Cells = nullptr;
	if (Root->TryGetObjectField(TEXT("wall"), Wall) && (*Wall)->TryGetArrayField(TEXT("cells"), Cells) && Cells->Num() == 4)
		Out.Wall = FIntRect((int32)(*Cells)[0]->AsNumber(), (int32)(*Cells)[1]->AsNumber(), (int32)(*Cells)[2]->AsNumber(), (int32)(*Cells)[3]->AsNumber());
	TSharedPtr<FJsonObject> Roads;   // the road network (CITY-043): the links out to the new areas and the gates
	if (FPaths::FileExists(FPaths::Combine(CityDir, TEXT("roads.json"))) && ReadMapJson(FPaths::Combine(CityDir, TEXT("roads.json")), Roads, OutError))
	{
		TMap<FString, FVector2D> Junctions;
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (Roads->TryGetArrayField(TEXT("junctions"), A)) for (const TSharedPtr<FJsonValue>& V : *A) Junctions.Add(V->AsObject()->GetStringField(TEXT("id")), PtOf(V->AsObject(), TEXT("pos")));
		if (Roads->TryGetArrayField(TEXT("links"), A)) for (const TSharedPtr<FJsonValue>& V : *A) { FBrixMapLine L; if (ReadLine(V->AsObject(), L)) Out.Links.Add(MoveTemp(L)); }
		if (Roads->TryGetArrayField(TEXT("gates"), A))
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> G = V->AsObject();
				const FVector2D* J = Junctions.Find(G->GetStringField(TEXT("junction")));
				if (J) Out.Gates.Add({ *J, *J + PtOf(G, TEXT("dir")) * G->GetNumberField(TEXT("length")) });
			}
	}
	const TArray<TSharedPtr<FJsonValue>>* List = nullptr;
	if (!Root->TryGetArrayField(TEXT("blocks"), List)) { OutError = TEXT("city.json lists no blocks"); return false; }
	for (const TSharedPtr<FJsonValue>& E : *List)
	{
		TSharedPtr<FJsonObject> B;
		if (!ReadMapJson(FPaths::Combine(CityDir, E->AsObject()->GetStringField(TEXT("file"))), B, OutError)) return false;
		FBrixMapBlock& M = Out.Blocks.AddDefaulted_GetRef();
		const TArray<TSharedPtr<FJsonValue>>* A = nullptr;
		if (B->TryGetArrayField(TEXT("lanes"), A)) for (const TSharedPtr<FJsonValue>& V : *A) { FBrixMapLine L; if (ReadLine(V->AsObject(), L)) M.Lanes.Add(MoveTemp(L)); }
		if (B->TryGetArrayField(TEXT("tracks"), A)) for (const TSharedPtr<FJsonValue>& V : *A) { FBrixMapLine L; if (ReadLine(V->AsObject(), L)) M.Tracks.Add(MoveTemp(L)); }
		if (B->TryGetArrayField(TEXT("paths"), A))
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TArray<TSharedPtr<FJsonValue>>& Two = V->AsArray();
				if (Two.Num() >= 2) M.Paths.Add({ Pt(Two[0]), Pt(Two[1]) });
			}
		if (B->TryGetArrayField(TEXT("props"), A))
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> P = V->AsObject();
				FString Kind, Shape, Along;
				P->TryGetStringField(TEXT("kind"), Kind);
				P->TryGetStringField(TEXT("shape"), Shape);
				P->TryGetStringField(TEXT("along"), Along);
				const FVector2D Pos = PtOf(P, TEXT("pos"));
				double Len = 0;
				P->TryGetNumberField(TEXT("len"), Len);
				const FVector2D Half = Along == TEXT("x") ? FVector2D(Len / 2, 0) : FVector2D(0, Len / 2);
				if (Kind == TEXT("crop")) { FString C; P->TryGetStringField(TEXT("crop"), C); M.Crops.Add({ Pos, PtOf(P, TEXT("size")), C }); }
				else if (Kind == TEXT("pond")) M.Ponds.Add({ Pos, (float)P->GetNumberField(TEXT("r")) });
				else if (Kind == TEXT("fence")) M.Fences.Add({ Pos - Half, Pos + Half });
				else if (Kind == TEXT("board")) M.Boards.Add(Pos);
				if (Shape == TEXT("hedge")) M.Hedges.Add({ Pos - Half, Pos + Half });
			}
		if (B->TryGetArrayField(TEXT("lots"), A))
			for (const TSharedPtr<FJsonValue>& V : *A)
			{
				const TSharedPtr<FJsonObject> L = V->AsObject();
				const TSharedPtr<FJsonObject>* Sh = nullptr;
				const TSharedPtr<FJsonObject>* Skin = nullptr;
				if (!L->TryGetObjectField(TEXT("shell"), Sh) || !(*Sh)->TryGetObjectField(TEXT("skin"), Skin)) continue;
				FBrixMapShell S;
				const TArray<TSharedPtr<FJsonValue>>* Q = nullptr;
				if ((*Skin)->TryGetArrayField(TEXT("points"), Q)) for (const TSharedPtr<FJsonValue>& W : *Q) S.Skin.Add(Pt(W));
				auto Egg = [](const TSharedPtr<FJsonObject>& D) { const TArray<TSharedPtr<FJsonValue>>& R = D->GetArrayField(TEXT("radii")); return TPair<FVector2D, FVector2D>(PtOf(D, TEXT("pos")), FVector2D(R[0]->AsNumber(), R.Num() > 2 ? R[2]->AsNumber() : R[0]->AsNumber())); };
				const TSharedPtr<FJsonObject>* RL = nullptr;
				if ((*Sh)->TryGetObjectField(TEXT("roof_light"), RL)) S.RoofLight = Egg(*RL);
				if ((*Sh)->TryGetArrayField(TEXT("domes"), Q)) for (const TSharedPtr<FJsonValue>& W : *Q) S.Domes.Add(Egg(W->AsObject()));
				if ((*Sh)->TryGetArrayField(TEXT("arches"), Q))
					for (const TSharedPtr<FJsonValue>& W : *Q)
					{
						FBrixMapShell::FArch Ar;
						W->AsObject()->TryGetStringField(TEXT("side"), Ar.Side);
						Ar.At = PtOf(W->AsObject(), TEXT("at"));
						double Wd = 4;
						W->AsObject()->TryGetNumberField(TEXT("width"), Wd);
						Ar.Width = (float)Wd;
						S.Arches.Add(Ar);
					}
				M.Shells.Add(L->GetStringField(TEXT("id")), MoveTemp(S));
			}
	}
	if (Out.Blocks.Num() != City.Blocks.Num()) { OutError = TEXT("the map's blocks are not the city's"); return false; }
	return true;
}

FColor BrixMapKindColour(const FString& Kind)
{
	const FColor* C = KindCss().Find(Kind);
	return C ? *C : Hex(TEXT("999999"));
}

FColor FBrixMapImage::At(const FVector2D& W) const
{
	const FVector2D P = ToPx(W);
	const int32 X = FMath::FloorToInt(P.X), Y = FMath::FloorToInt(P.Y);
	return X >= 0 && Y >= 0 && X < N && Y < N ? Px[Y * N + X] : FColor::Transparent;
}

// play.js drawBase(), step by step: the ground, Old Town's streets, the roads out, each block (its lanes, or its open
// ground with crops, ponds, paths, tracks, fences, hedges and plots for sale, or its paving), each lot in its kind's colour
// (turned by its angle, or a sculpted shell's own outline with its domes, roof light and ways in), every tree, the tram line.
FBrixMapImage BrixDrawMap(const FBrixCity& City, const FBrixMapExtras& X, int32 N, double TreeRadius)
{
	FBrixMapImage Img;
	Img.N = N;
	const double Pitch = City.Pitch, SW = City.StreetWidth, CityM = FMath::Max(City.Columns, City.Rows) * Pitch;
	Img.K = N / (CityM + 40);
	Img.O = 20;
	const double K = Img.K;
	Img.Px.Init(Hex(TEXT("3a3d2c")), N * N);
	FPainter G{ N, Img.Px };
	auto Px = [&](const FVector2D& W) { return Img.ToPx(W); };
	auto Pts = [&](const TArray<FVector2D>& W) { TArray<FVector2D> Q; for (const FVector2D& V : W) Q.Add(Px(V)); return Q; };
	auto LineW = [&](double W) { return FMath::Max(1.0, W * K); };
	const FColor Road = Hex(TEXT("4b4745"));
	const double OX = X.Wall.Min.X * Pitch, OZ = X.Wall.Min.Y * Pitch, OX1 = X.Wall.Max.X * Pitch, OZ1 = X.Wall.Max.Y * Pitch;
	{
		const FVector2D A = Px(FVector2D(OX - SW / 2, OZ - SW / 2)), B = Px(FVector2D(OX1 + SW / 2, OZ1 + SW / 2));
		G.Rect(A.X, A.Y, B.X, B.Y, Road);
	}
	for (const FBrixMapLine& L : X.Links) G.Line(Pts(L.Points), LineW(L.Width), Road);
	for (const auto& Gt : X.Gates) G.Seg(Px(Gt.Key), Px(Gt.Value), LineW(7), Road);
	for (int32 b = 0; b < City.Blocks.Num(); ++b)
	{
		const FBrixBlock& B = City.Blocks[b];
		const FBrixMapBlock& M = X.Blocks[b];
		const FColor* Green = MapGreen().Find(B.Template);
		if (M.Lanes.Num()) { for (const FBrixMapLine& L : M.Lanes) G.Line(Pts(L.Points), LineW(L.Width), Road); }
		else if (Green)
		{
			const FVector2D A = Px(B.Origin), Z = Px(B.Origin + B.Extent);
			G.Rect(A.X, A.Y, Z.X, Z.Y, *Green);
			for (const FBrixMapBlock::FCrop& C : M.Crops)
			{
				const FColor* Cc = MapCrop().Find(C.Crop);
				const FVector2D P0 = Px(C.Pos - C.Size / 2), P1 = Px(C.Pos + C.Size / 2);
				G.Rect(P0.X, P0.Y, P1.X, P1.Y, Cc ? *Cc : MapCrop()[TEXT("rough")]);
			}
			for (const auto& Pd : M.Ponds) G.Disc(Px(Pd.Key), Pd.Value * K, Hex(TEXT("4f6f7a")));
			for (const auto& Pa : M.Paths) G.Seg(Px(Pa.Key), Px(Pa.Value), LineW(2.5), Hex(TEXT("a3967a")));
			for (const FBrixMapLine& T : M.Tracks) G.Line(Pts(T.Points), LineW(T.Width), Hex(TEXT("8a7d64")));
			for (const auto& F : M.Fences) G.Seg(Px(F.Key), Px(F.Value), LineW(1), Hex(TEXT("7a5f43")));
			for (const auto& H : M.Hedges) G.Seg(Px(H.Key), Px(H.Value), LineW(2), Hex(TEXT("33461f")));
			for (const FVector2D& Bd : M.Boards) { const FVector2D P0 = Px(Bd - FVector2D(3, 3)), P1 = Px(Bd + FVector2D(3, 3)); G.Rect(P0.X, P0.Y, P1.X, P1.Y, Hex(TEXT("f0c35a"))); }
		}
		else
		{
			const FVector2D A = Px(B.Origin - FVector2D(2, 2)), Z = Px(B.Origin + B.Extent + FVector2D(2, 2));
			G.Rect(A.X, A.Y, Z.X, Z.Y, B.Template == TEXT("park") ? Hex(TEXT("5d7343")) : Hex(TEXT("77705f")));
		}
		for (const FBrixLot& L : B.Lots)
		{
			const FColor C = BrixMapKindColour(L.Kind);
			if (const FBrixMapShell* S = M.Shells.Find(L.Id); S && S->Skin.Num() > 2)
			{
				const FColor Edge = Hex(TEXT("5e5647"));
				const TArray<FVector2D> Q = Pts(S->Skin);
				G.Poly(Q, C);
				G.Line(Q, FMath::Max(1.0, 1.2 * K), Edge, true);
				auto Egg = [&](const TPair<FVector2D, FVector2D>& E, const FColor& Fill) { const TArray<FVector2D> Q2 = G.Ellipse(Px(E.Key), E.Value * K); G.Poly(Q2, Fill); G.Line(Q2, FMath::Max(1.0, 1.2 * K), Edge, true); };
				if (S->RoofLight) Egg(*S->RoofLight, Hex(TEXT("cfc5a8")));
				for (const auto& D : S->Domes) Egg(D, Hex(TEXT("b08f48")));
				for (const FBrixMapShell::FArch& Ar : S->Arches)   // a light mark at each way in, on the outline nearest the arch
				{
					const bool bNs = Ar.Side == TEXT("n") || Ar.Side == TEXT("s");
					const double Out = Ar.Side == TEXT("n") || Ar.Side == TEXT("w") ? -1 : 1;
					const FVector2D Want = Ar.At + (bNs ? FVector2D(0, Out * 14) : FVector2D(Out * 14, 0));
					int32 Best = 0;
					for (int32 n = 1; n < S->Skin.Num(); ++n) if (FVector2D::Distance(S->Skin[n], Want) < FVector2D::Distance(S->Skin[Best], Want)) Best = n;
					const FVector2D Q0 = S->Skin[Best], H = bNs ? FVector2D(Ar.Width / 2, 0) : FVector2D(0, Ar.Width / 2);
					G.Seg(Px(Q0 - H), Px(Q0 + H), FMath::Max(1.5, 2.5 * K), Hex(TEXT("f3e7c6")));
				}
				continue;
			}
			const FVector2D Ctr = L.Centre(), Hf = L.Size() / 2;
			const double A = FMath::DegreesToRadians((double)L.AngleDeg), Ca = FMath::Cos(A), Sa = FMath::Sin(A);
			TArray<FVector2D> Q;
			for (const FVector2D& D : { FVector2D(-Hf.X, -Hf.Y), FVector2D(Hf.X, -Hf.Y), FVector2D(Hf.X, Hf.Y), FVector2D(-Hf.X, Hf.Y) })
				Q.Add(Px(Ctr + FVector2D(D.X * Ca - D.Y * Sa, D.X * Sa + D.Y * Ca)));   // canvas rotate(): clockwise on the map, x east, z south
			G.Poly(Q, C);
		}
	}
	const FColor Tree = Hex(TEXT("2f4524"));   // every tree, so a wood reads as a wood and an orchard as rows
	for (const FBrixBlock& B : City.Blocks) for (const FBrixTree& T : B.Trees) G.Disc(Px(T.Pos), FMath::Max(.9, TreeRadius * K * .8), Tree);
	G.Seg(Px(FVector2D(OX - 8, City.Tram.Z)), Px(FVector2D(OX1 + 8, City.Tram.Z)), 1.5, Hex(TEXT("2a2a30")));
	return Img;
}

TArray<TPair<FColor, FString>> BrixMapLegend()
{
	TArray<TPair<FColor, FString>> Out;
	const TPair<const TCHAR*, const TCHAR*> Words[] = { { TEXT("school"), TEXT("Schools") }, { TEXT("fire"), TEXT("Fire station") }, { TEXT("police"), TEXT("Police station") },
		{ TEXT("home"), TEXT("Homes") }, { TEXT("shop"), TEXT("Shops") }, { TEXT("food"), TEXT("Cafés") }, { TEXT("work"), TEXT("Workplaces") },
		{ TEXT("health"), TEXT("Clinics and hospital") }, { TEXT("utility"), TEXT("Utilities") }, { TEXT("garden"), TEXT("Gardens and parks") },
		{ TEXT("market"), TEXT("Market Square") }, { TEXT("stop"), TEXT("Tram stops") }, { TEXT("bank"), TEXT("Bank") }, { TEXT("dealer"), TEXT("Car showroom") },
		{ TEXT("library"), TEXT("Library") }, { TEXT("museum"), TEXT("Museum") }, { TEXT("cinema"), TEXT("Cinema") }, { TEXT("furniture"), TEXT("Furniture outlet") } };
	for (const auto& W : Words) Out.Add({ BrixMapKindColour(W.Key), W.Value });
	Out.Add({ Hex(TEXT("4f6b3a")), TEXT("Woods") });
	Out.Add({ Hex(TEXT("5d7343")), TEXT("Commons and greens") });
	Out.Add({ Hex(TEXT("66703f")), TEXT("Farmland") });
	Out.Add({ Hex(TEXT("76804d")), TEXT("Building plots") });
	Out.Add({ Hex(TEXT("f0c35a")), TEXT("Plot for sale") });
	Out.Add({ Hex(TEXT("17171c")), TEXT("Residents") });
	Out.Add({ Hex(TEXT("f0a35a")), TEXT("You, facing the way you look") });
	return Out;
}

void FBrixMapView::Clamp()
{
	const double H = Size / 2 / Z;
	Cx = FMath::Clamp(Cx, H, Size - H);
	Cy = FMath::Clamp(Cy, H, Size - H);
}

FVector2D FBrixMapView::MapAt(const FVector2D& S) const { return FVector2D((S.X - Size / 2) / Z + Cx, (S.Y - Size / 2) / Z + Cy); }
FVector2D FBrixMapView::ScreenAt(const FVector2D& M) const { return FVector2D((M.X - Cx) * Z + Size / 2, (M.Y - Cy) * Z + Size / 2); }

void FBrixMapView::Zoom(double F, const FVector2D& At)
{
	const FVector2D M = MapAt(At);
	Z = FMath::Clamp(Z * F, MinZ, MaxZ);
	Cx = M.X - (At.X - Size / 2) / Z;
	Cy = M.Y - (At.Y - Size / 2) / Z;
	Clamp();
}

FString FBrixMark::MarkAt(const FVector2D& At, const FString& PlaceName, bool bNamed)
{
	if (bSet && FVector2D::Distance(Pos, At) < 6 && !bNamed) { bSet = false; return TEXT("Mark cleared."); }
	bSet = true;
	Pos = At;
	Name = PlaceName.IsEmpty() ? FString(TEXT("Your mark")) : PlaceName;
	return TEXT("Marked: ") + Name + TEXT(".");
}

bool FBrixMark::Reached(const FVector2D& Player)
{
	if (!bSet || FVector2D::Distance(Pos, Player) >= 4) return false;
	bSet = false;
	return true;
}

FString FBrixMark::TagText(const FVector2D& Player) const
{
	const double D = FVector2D::Distance(Pos, Player);
	return TEXT("◆ ") + Name + TEXT(" · ") + (D < 1000 ? FString::Printf(TEXT("%d m"), (int32)FMath::RoundHalfFromZero(D)) : FString::Printf(TEXT("%.1f km"), FMath::RoundHalfFromZero(D / 100) / 10));
}

TArray<FBrixMapTarget> BrixQuestTargets(const FBrixQuests& Q, const FBrixQuestRules& Rules, const FBrixEconomy& E)
{
	TArray<FBrixMapTarget> Out;
	auto Lot = [&](int32 N, const TCHAR* Kind) { FBrixMapTarget T; T.Lot = N; T.Kind = Kind; if (N >= 0) Out.Add(T); };
	auto Who = [&](int32 I, bool bOffer, const TCHAR* Kind) { FBrixMapTarget T; T.Resident = I; T.bOffer = bOffer; T.Kind = Kind; if (I >= 0) Out.Add(T); };
	for (const auto& A : Q.Active)
	{
		const FBrixQuestStage* S = Q.Stage(A.Key);
		if (!S) continue;
		const FBrixGoal& G = S->Goal;
		if (G.Kind == TEXT("talk"))
		{
			// going back: to the line's giver, or to whom its first stage sent you, from a later stage (ADR-0051)
			const FBrixQuestLine* L = Rules.Quests.FindByPredicate([&](const FBrixQuestLine& X) { return X.Id == A.Key; });
			const FBrixQuestStage* First = L ? L->Stages.Find(L->Start) : nullptr;
			const bool bBack = L && A.Value.Stage != L->Start && (G.Value == L->Giver || (First && First->Goal.Kind == TEXT("talk") && First->Goal.Value == G.Value));
			Who(Q.ResidentIndex(G.Value), false, bBack ? TEXT("handin") : TEXT("talk"));
		}
		else if (G.Kind == TEXT("go")) Lot(E.LotIndex.FindRef(G.Value, -1), TEXT("go"));
		else if (G.Kind == TEXT("near")) { FBrixMapTarget T; T.Pos = G.Near; T.Kind = TEXT("go"); Out.Add(T); }
		else if (G.Kind == TEXT("bank")) Lot(E.BankLot, TEXT("bank"));
		else if (G.Kind == TEXT("stall") || G.Kind == TEXT("stall_sold")) Lot(E.MarketLot, TEXT("stall"));
	}
	if (Q.Job)
	{
		const FBrixJob& J = *Q.Job;
		if (J.Kind == TEXT("parcel")) Lot(J.Step == TEXT("pickup") ? J.Frm : J.To, TEXT("job"));
		else if (J.Kind == TEXT("shift")) Lot(J.To, TEXT("job"));
		else Who(J.Giver, false, TEXT("job"));
	}
	for (const FBrixQuestLine& L : Rules.Quests)
		if (!L.Giver.IsEmpty() && Q.Offers(L.Giver).Contains(L.Id)) Who(Q.ResidentIndex(L.Giver), true, TEXT("offer"));
	return Out;
}

