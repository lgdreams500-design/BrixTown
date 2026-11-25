#include "BrixBuyMenu.h"

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
