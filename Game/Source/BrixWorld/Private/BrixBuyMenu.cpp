#include "BrixBuyMenu.h"
#include "BrixBody.h"

#include "Dom/JsonObject.h"

namespace
{
	FLinearColor Rgb(const TArray<TSharedPtr<FJsonValue>>& A)
	{
		return A.Num() >= 3 ? FLinearColor((float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber()) : FLinearColor::Gray;
	}
	float NumOr(const TSharedPtr<FJsonObject>& O, const TCHAR* F, float D) { double V = 0; return O && O->TryGetNumberField(F, V) ? (float)V : D; }
}

bool LoadBrixBuyMenu(const TSharedPtr<FJsonObject>& J, FBrixBuyMenuRules& Out, FString& OutError)
{
	Out = FBrixBuyMenuRules();
	const TSharedPtr<FJsonObject>* BM = nullptr;
	if (!J || !J->TryGetObjectField(TEXT("buy_menu"), BM)) { OutError = TEXT("no buy menu in life.json"); return false; }
	const TSharedPtr<FJsonObject> B = (*BM)->GetObjectField(TEXT("rules"));
	Out.NarrowPx = (int32)NumOr(B, TEXT("narrow_px"), 560);
	const TSharedPtr<FJsonObject> W = B->GetObjectField(TEXT("wide")), P = B->GetObjectField(TEXT("picture"));
	Out.WidePanelPx = (int32)NumOr(W, TEXT("panel_px"), 960); Out.CellPx = (int32)NumOr(W, TEXT("cell_px"), 112); Out.StagePx = (int32)NumOr(W, TEXT("stage_px"), 96);
	Out.ShowPx = (int32)NumOr(W, TEXT("show_px"), 320); Out.ShowStagePx = (int32)NumOr(W, TEXT("show_stage_px"), 250); Out.ListPx = (int32)NumOr(W, TEXT("list_px"), 470);
	Out.FovDeg = NumOr(P, TEXT("fov_deg"), 24); Out.PitchDeg = NumOr(P, TEXT("pitch_deg"), 17); Out.StartDeg = NumOr(P, TEXT("start_deg"), -34);
	Out.TurnS = NumOr(P, TEXT("turn_s"), 16); Out.DragDegPx = NumOr(P, TEXT("drag_deg_px"), .7f); Out.StandInFill = NumOr(P, TEXT("stand_in_fill"), .8f);
	for (const auto& G : B->GetObjectField(TEXT("goes"))->Values)
	{
		const TSharedPtr<FJsonObject> O = G.Value->AsObject();
		Out.Goes.Add(FString(G.Key.ToView()), { O->GetStringField(TEXT("name")), O->GetStringField(TEXT("colour")), O->GetStringField(TEXT("to")) });
	}
	for (const auto& C : B->GetObjectField(TEXT("colours"))->Values) Out.Colours.Add(FString(C.Key.ToView()), Rgb(C.Value->AsArray()));
	for (const auto& C : B->GetObjectField(TEXT("bands"))->Values) Out.Bands.Add(FString(C.Key.ToView()), C.Value->AsString());
	for (const auto& S : B->GetObjectField(TEXT("shapes"))->Values)
	{
		TArray<FBrixBuyMenuRules::FBox>& L = Out.Shapes.Add(FString(S.Key.ToView()));
		for (const auto& V : S.Value->AsArray())
		{
			const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
			if (A.Num() < 7) continue;
			FBrixBuyMenuRules::FBox X{ (float)A[0]->AsNumber(), (float)A[1]->AsNumber(), (float)A[2]->AsNumber(), (float)A[3]->AsNumber(), (float)A[4]->AsNumber(), (float)A[5]->AsNumber(), A[6]->AsString() };
			if (A.Num() > 7) X.Shade = (float)A[7]->AsNumber();
			if (A.Num() > 8) X.TurnDeg = (float)A[8]->AsNumber();
			L.Add(X);
		}
	}
	const TSharedPtr<FJsonObject> SI = B->GetObjectField(TEXT("stand_in"));
	for (const auto& G : SI->GetObjectField(TEXT("goes"))->Values) Out.StandInGoes.Add(FString(G.Key.ToView()), G.Value->AsString());
	for (const auto& G : SI->GetObjectField(TEXT("things"))->Values) Out.StandInThings.Add(FString(G.Key.ToView()), G.Value->AsString());
	for (const auto& V : SI->GetArrayField(TEXT("wear")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
		Out.StandInWear.Add({ A[0]->AsString(), A[1]->AsString(), A[2]->IsNull() ? FString() : A[2]->AsString() });
	}
	const TSharedPtr<FJsonObject>* St = nullptr;
	if (J->TryGetObjectField(TEXT("settings"), St))
		for (const auto& C : (*St)->GetObjectField(TEXT("menu"))->GetObjectField(TEXT("colours"))->Values)
		{
			const TArray<TSharedPtr<FJsonValue>>& A = C.Value->AsArray();
			if (A.Num() == 2) Out.BoardColours.Add(FString(C.Key.ToView()), { FColor::FromHex(A[0]->AsString()), FColor::FromHex(A[1]->AsString()) });
		}
	for (const auto& C : J->GetObjectField(TEXT("palette"))->Values) Out.Palette.Add(FString(C.Key.ToView()), Rgb(C.Value->AsArray()));
	for (const auto& V : J->GetArrayField(TEXT("pieces"))) Out.Pieces.Add(V->AsString());
	for (const auto& V : J->GetObjectField(TEXT("economy"))->GetObjectField(TEXT("items"))->Values) Out.Items.Add(FString(V.Key.ToView()));
	for (const auto& V : J->GetObjectField(TEXT("goods"))->GetObjectField(TEXT("goods"))->Values)
	{
		const TSharedPtr<FJsonObject> O = V.Value->AsObject();
		FBrixBuyMenuRules::FGood G;
		G.Goes = O->GetStringField(TEXT("goes"));
		O->TryGetStringField(TEXT("piece"), G.Piece);
		const TSharedPtr<FJsonObject>* Parts = nullptr;
		if (O->TryGetObjectField(TEXT("parts"), Parts))
			for (const auto& Pt : (*Parts)->Values)
				if (!Pt.Value->IsNull()) { FString S; if (Pt.Value->TryGetString(S)) G.Parts.Add(FString(Pt.Key.ToView()), S); else if (Pt.Value->Type == EJson::Boolean && Pt.Value->AsBool()) G.Parts.Add(FString(Pt.Key.ToView()), TEXT("true")); }
		Out.Goods.Add(FString(V.Key.ToView()), G);
	}
	const TSharedPtr<FJsonObject>* Ic = nullptr;
	if ((*BM)->TryGetObjectField(TEXT("icons"), Ic))
		for (const auto& I : (*Ic)->Values)
		{
			TArray<FBrixIconPath>& L = Out.Icons.Add(FString(I.Key.ToView()));
			for (const auto& V : I.Value->AsArray())
			{
				const TArray<TSharedPtr<FJsonValue>>& A = V->AsArray();
				if (A.Num() >= 2) L.Add({ A[0]->AsString(), (float)A[1]->AsNumber() });
			}
		}
	if (!Out.Shapes.Num() || !Out.Goes.Num()) { OutError = TEXT("the buy menu's rules are empty"); return false; }
	return true;
}

// world.buy_menu_shows(), line for line
FBrixShows FBrixBuyMenuRules::Shows(const FString& Thing) const
{
	FBrixShows Out;
	const FGood* G = Goods.Find(Thing);
	if (Pieces.Contains(Thing) && !G && !Items.Contains(Thing)) { Out.Kind = TEXT("piece"); Out.Name = Thing; return Out; }
	if (G && G->Goes == TEXT("home")) { Out.Kind = TEXT("piece"); Out.Name = G->Piece; return Out; }
	if (const FString* S = StandInThings.Find(Thing)) { Out.Kind = TEXT("shape"); Out.Name = *S; return Out; }
	if (G && G->Goes == TEXT("wear"))
	{
		for (const FWear& Wr : StandInWear)
			if (const FString* V = G->Parts.Find(Wr.Part))
			{
				if (V->IsEmpty()) continue;
				Out.Kind = TEXT("shape");
				Out.Name = Wr.Shape;
				if (!Wr.ColourPart.IsEmpty()) if (const FString* C = G->Parts.Find(Wr.ColourPart)) Out.Colour = *C;
				return Out;
			}
		return Out;
	}
	if (G || Items.Contains(Thing))
	{
		Out.Kind = TEXT("shape");
		if (const FString* S = StandInGoes.Find(G ? G->Goes : FString(TEXT("carry")))) Out.Name = *S;
		return Out;
	}
	return Out;
}

// buymenu.js boxMesh() with shapeMesh()'s boxes: each box turned about its own middle, six faces of two triangles
FBrixThingMesh FBrixBuyMenuRules::ShapeMesh(const FString& Shape, const FLinearColor& Band, const FLinearColor& Own) const
{
	FBrixThingMesh M;
	M.bStandIn = true;
	const TArray<FBox>* Boxes = Shapes.Find(Shape);
	if (!Boxes) return M;
	for (const FBox& X : *Boxes)
	{
		const FLinearColor* Base = X.Colour == TEXT("band") ? &Band : X.Colour == TEXT("own") ? &Own : Colours.Find(X.Colour);
		if (!Base || !(X.Su > 0) || !(X.Sy > 0) || !(X.Sv > 0)) continue;
		const FVector3f Col(Base->R * X.Shade, Base->G * X.Shade, Base->B * X.Shade);
		const float Yaw = -FMath::DegreesToRadians(X.TurnDeg), c = FMath::Cos(Yaw), s = FMath::Sin(Yaw), hx = X.Su / 2, hz = X.Sv / 2;
		auto Pt = [&](float du, float dy, float dv) { return FVector3f(X.U + c * du - s * dv, X.Y + dy, -(X.V + s * du + c * dv)); };
		auto Nm = [&](float nu, float nv) { return FVector3f(c * nu - s * nv, 0, -(s * nu + c * nv)); };
		auto Face = [&](const FVector3f& p0, const FVector3f& p1, const FVector3f& p2, const FVector3f& p3, const FVector3f& n)
		{
			for (const FVector3f& p : { p0, p1, p2, p0, p2, p3 }) { M.P.Add(p); M.N.Add(n); M.C.Add(Col); }
		};
		const float K[4][2] = { { -hx, -hz }, { hx, -hz }, { hx, hz }, { -hx, hz } };
		FVector3f Lo[4], Hi[4];
		for (int32 k = 0; k < 4; ++k) { Lo[k] = Pt(K[k][0], 0, K[k][1]); Hi[k] = Pt(K[k][0], X.Sy, K[k][1]); }
		Face(Hi[0], Hi[1], Hi[2], Hi[3], FVector3f(0, 1, 0));
		Face(Lo[0], Lo[1], Lo[2], Lo[3], FVector3f(0, -1, 0));
		Face(Lo[0], Lo[1], Hi[1], Hi[0], Nm(0, -1));
		Face(Lo[1], Lo[2], Hi[2], Hi[1], Nm(1, 0));
		Face(Lo[2], Lo[3], Hi[3], Hi[2], Nm(0, 1));
		Face(Lo[3], Lo[0], Hi[0], Hi[3], Nm(-1, 0));
	}
	if (!M.P.Num()) return M;
	M.Lo = FVector3f(1e9f); M.Hi = FVector3f(-1e9f);
	for (const FVector3f& p : M.P) { M.Lo = FVector3f::Min(M.Lo, p); M.Hi = FVector3f::Max(M.Hi, p); }
	M.Cen = (M.Lo + M.Hi) * .5f;
	float R = 0, Foot = 0;
	for (const FVector3f& p : M.P) { R = FMath::Max(R, (p - M.Cen).Size()); Foot = FMath::Max(Foot, FMath::Sqrt(FMath::Square(p.X - M.Cen.X) + FMath::Square(p.Z - M.Cen.Z))); }
	M.R = FMath::Max(R, .01f);
	M.Foot = Foot;
	return M;
}

TOptional<FBrixThingMesh> FBrixBuyMenuRules::MeshFor(const FString& Thing, const FString& BandColour) const
{
	const FBrixShows S = Shows(Thing);
	if (S.Kind != TEXT("shape") || S.Name.IsEmpty()) return {};
	const FLinearColor* Band = Colours.Find(BandColour);
	const FLinearColor* Own = S.Colour ? Palette.Find(*S.Colour) : nullptr;
	FBrixThingMesh M = ShapeMesh(S.Name, Band ? *Band : Colours.FindRef(TEXT("ochre")), Own ? *Own : Colours.FindRef(TEXT("cream")));
	if (!M.P.Num()) return {};
	return M;
}

// ---------------------------------------------------------------- the marks in the dots and chips (UI-020)
TArray<TArray<FVector2f>> BrixIconOutlines(const FString& D)
{
	TArray<TArray<FVector2f>> Out;
	TCHAR Cmd = 0;
	FVector2f At(0, 0), Start(0, 0);
	int32 i = 0;
	const int32 N = D.Len();
	auto Skip = [&]() { while (i < N && (FChar::IsWhitespace(D[i]) || D[i] == TEXT(','))) ++i; };
	auto Num = [&]() -> float
	{
		Skip();
		const int32 a = i;
		if (i < N && (D[i] == TEXT('-') || D[i] == TEXT('+'))) ++i;
		while (i < N && (FChar::IsDigit(D[i]) || D[i] == TEXT('.'))) ++i;
		return FCString::Atof(*D.Mid(a, i - a));
	};
	auto Line = [&](FVector2f P) { if (!Out.Num()) Out.AddDefaulted_GetRef().Add(At); Out.Last().Add(P); At = P; };
	while (true)
	{
		Skip();
		if (i >= N) break;
		if (FChar::IsAlpha(D[i])) Cmd = D[i++];
		else if (Cmd == TEXT('M')) Cmd = TEXT('L');   // numbers after a move's first point are lines
		switch (Cmd)
		{
		case TEXT('M'): { const float x = Num(), y = Num(); At = Start = FVector2f(x, y); Out.AddDefaulted_GetRef().Add(At); break; }
		case TEXT('L'): { const float x = Num(), y = Num(); Line(FVector2f(x, y)); break; }
		case TEXT('H'): Line(FVector2f(Num(), At.Y)); break;
		case TEXT('V'): Line(FVector2f(At.X, Num())); break;
		case TEXT('Q'):
		{
			const float cx = Num(), cy = Num(), x = Num(), y = Num();
			const FVector2f P0 = At, C(cx, cy), P1(x, y);
			for (int32 k = 1; k <= 16; ++k)
			{
				const float t = k / 16.f, u = 1 - t;
				Line(P0 * (u * u) + C * (2 * u * t) + P1 * (t * t));
			}
			break;
		}
		case TEXT('Z'): case TEXT('z'): if (Out.Num() && Out.Last().Num() && Out.Last().Last() != Start) Out.Last().Add(Start); At = Start; break;
		default: ++i; break;   // a command the marks do not use: skipped
		}
	}
	return Out;
}

TArray<float> BrixIconCoverage(const TArray<FBrixIconPath>& Paths, int32 Px)
{
	TArray<float> Out;
	Out.SetNumZeroed(Px * Px);
	struct FShape { TArray<TArray<FVector2f>> Rings; float Half; };
	TArray<FShape> Shapes;
	for (const FBrixIconPath& P : Paths) Shapes.Add({ BrixIconOutlines(P.D), P.Stroke * .5f });
	const int32 SS = 4;
	for (int32 y = 0; y < Px; ++y)
		for (int32 x = 0; x < Px; ++x)
		{
			int32 Hit = 0;
			for (int32 sy = 0; sy < SS; ++sy)
				for (int32 sx = 0; sx < SS; ++sx)
				{
					const FVector2f Q((x + (sx + .5f) / SS) * 16.f / Px, (y + (sy + .5f) / SS) * 16.f / Px);
					bool bIn = false;
					for (const FShape& S : Shapes)
					{
						if (S.Half <= 0)   // a fill, by the nonzero rule (each ring closed back to its start)
						{
							int32 Wind = 0;
							for (const TArray<FVector2f>& R : S.Rings)
								for (int32 k = 0; k < R.Num(); ++k)
								{
									const FVector2f A = R[k], B = R[(k + 1) % R.Num()];
									if (A.Y <= Q.Y) { if (B.Y > Q.Y && (B.X - A.X) * (Q.Y - A.Y) - (Q.X - A.X) * (B.Y - A.Y) > 0) ++Wind; }
									else if (B.Y <= Q.Y && (B.X - A.X) * (Q.Y - A.Y) - (Q.X - A.X) * (B.Y - A.Y) < 0) --Wind;
								}
							bIn = Wind != 0;
						}
						else   // a stroke: within half its width of a straight piece, square across its ends
							for (const TArray<FVector2f>& R : S.Rings)
							{
								for (int32 k = 0; k + 1 < R.Num() && !bIn; ++k)
								{
									const FVector2f A = R[k], B = R[k + 1], AB = B - A;
									const float L2 = AB.SizeSquared();
									if (L2 < 1e-8f) continue;
									const float t = FVector2f::DotProduct(Q - A, AB) / L2;
									if (t < 0 || t > 1) continue;
									bIn = FVector2f::DistSquared(Q, A + AB * t) <= S.Half * S.Half;
								}
								if (bIn) break;
							}
						if (bIn) break;
					}
					Hit += bIn;
				}
			Out[y * Px + x] = Hit / (float)(SS * SS);
		}
	return Out;
}

FBrixThingMesh BrixThingFromBody(const FBrixBodyMesh& B)
{
	FBrixThingMesh M;
	for (int32 t = 0; t + 2 < B.Index.Num(); t += 3)
	{
		const int32 a = B.Index[t], b = B.Index[t + 1], c = B.Index[t + 2];
		if (!B.Pos.IsValidIndex(a) || !B.Pos.IsValidIndex(b) || !B.Pos.IsValidIndex(c)) continue;
		FVector3f N = FVector3f::CrossProduct(B.Pos[b] - B.Pos[a], B.Pos[c] - B.Pos[a]);
		if (N.SizeSquared() < 1e-14f) continue;   // a sliver at a pole
		N.Normalize();
		if (FVector3f::DotProduct(N, B.Normal[a] + B.Normal[b] + B.Normal[c]) < 0) N = -N;
		const FVector3f Col = (B.Colour[a] + B.Colour[b] + B.Colour[c]) / 3.f;
		for (const int32 k : { a, b, c }) { M.P.Add(B.Pos[k]); M.N.Add(N); M.C.Add(Col); }
	}
	if (!M.P.Num()) return M;
	M.Lo = FVector3f(1e9f); M.Hi = FVector3f(-1e9f);
	for (const FVector3f& p : M.P) { M.Lo = FVector3f::Min(M.Lo, p); M.Hi = FVector3f::Max(M.Hi, p); }
	M.Cen = (M.Lo + M.Hi) * .5f;
	float R = 0, Foot = 0;
	for (const FVector3f& p : M.P) { R = FMath::Max(R, (p - M.Cen).Size()); Foot = FMath::Max(Foot, FMath::Sqrt(FMath::Square(p.X - M.Cen.X) + FMath::Square(p.Z - M.Cen.Z))); }
	M.R = FMath::Max(R, .01f);
	M.Foot = Foot;
	return M;
}
