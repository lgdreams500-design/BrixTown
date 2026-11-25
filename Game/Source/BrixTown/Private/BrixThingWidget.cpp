#include "BrixThingWidget.h"

#include "BrixLook.h"

#include "Engine/StaticMesh.h"
#include "StaticMeshResources.h"

#include "Framework/Application/SlateApplication.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Styling/CoreStyle.h"

TOptional<FBrixThingMesh> BrixThingFromStaticMesh(const UStaticMesh* Mesh)
{
	const FStaticMeshRenderData* Rd = Mesh ? Mesh->GetRenderData() : nullptr;
	if (!Rd || !Rd->LODResources.Num()) return {};
	const FStaticMeshLODResources& L = Rd->LODResources[Rd->LODResources.Num() - 1];   // the coarsest make: a cell is small
	const FPositionVertexBuffer& Pos = L.VertexBuffers.PositionVertexBuffer;
	const FStaticMeshVertexBuffer& Tan = L.VertexBuffers.StaticMeshVertexBuffer;
	const FColorVertexBuffer& Col = L.VertexBuffers.ColorVertexBuffer;
	// the corners are kept readable in the editor's builds; a packaged game keeps them only for meshes marked so
	if (!Pos.GetNumVertices() || !Pos.GetAllowCPUAccess() || !L.IndexBuffer.GetAllowCPUAccess() || Tan.GetNumVertices() != Pos.GetNumVertices() || !Tan.GetAllowCPUAccess()) return {};
	const bool bColours = Col.GetNumVertices() == Pos.GetNumVertices() && Col.GetAllowCPUAccess();
	const FIndexArrayView Ix = L.IndexBuffer.GetArrayView();
	FBrixThingMesh M;
	M.P.Reserve(Ix.Num()); M.N.Reserve(Ix.Num()); M.C.Reserve(Ix.Num());
	for (int32 t = 0; t + 2 < Ix.Num(); t += 3)
		for (int32 k = 0; k < 3; ++k)
		{
			const uint32 i = Ix[t + k];
			if (i >= Pos.GetNumVertices()) return {};
			// the kit's piece in Unreal: x across, its front toward +y, z up, centimetres
			const FVector3f P = Pos.VertexPosition(i);
			const FVector4f N = Tan.VertexTangentZ(i);
			M.P.Add(FVector3f(P.X, P.Z, P.Y) * .01f);
			M.N.Add(FVector3f(N.X, N.Z, N.Y));
			const FColor C = bColours ? Col.VertexColor(i) : FColor(115, 97, 77);
			M.C.Add(FVector3f(C.R / 255.f, C.G / 255.f, C.B / 255.f));   // as M_Furniture reads it: the colour itself
		}
	if (!M.P.Num()) return {};
	FBrixBuyMenuRules::Finish(M);
	return M;
}

FBrixThingMesh BrixThingBox(const FVector3f& Size, const FVector3f& Colour)
{
	FBrixThingMesh M;
	M.bStandIn = true;
	const float hx = Size.X / 2, hz = Size.Y / 2, h = Size.Z;
	const FVector3f Lo[4] = { { -hx, 0, -hz }, { hx, 0, -hz }, { hx, 0, hz }, { -hx, 0, hz } }, Hi[4] = { { -hx, h, -hz }, { hx, h, -hz }, { hx, h, hz }, { -hx, h, hz } };
	auto Face = [&](const FVector3f& a, const FVector3f& b, const FVector3f& c, const FVector3f& d, const FVector3f& n)
	{
		for (const FVector3f& p : { a, b, c, a, c, d }) { M.P.Add(p); M.N.Add(n); M.C.Add(Colour); }
	};
	Face(Hi[0], Hi[1], Hi[2], Hi[3], FVector3f(0, 1, 0));
	Face(Lo[0], Lo[1], Lo[2], Lo[3], FVector3f(0, -1, 0));
	Face(Lo[0], Lo[1], Hi[1], Hi[0], FVector3f(0, 0, -1));
	Face(Lo[1], Lo[2], Hi[2], Hi[1], FVector3f(1, 0, 0));
	Face(Lo[2], Lo[3], Hi[3], Hi[2], FVector3f(0, 0, 1));
	Face(Lo[3], Lo[0], Hi[0], Hi[3], FVector3f(-1, 0, 0));
	FBrixBuyMenuRules::Finish(M);
	return M;
}

void SBrixThing::Construct(const FArguments& A)
{
	Mesh = A._Mesh;
	Rules = A._Rules;
	bSpin = A._Spin;
	bGrey = A._Grey;
	bDrag = A._Drag;
	Size = A._Size;
	Yaw = A._Yaw.IsValid() ? A._Yaw : MakeShared<double>(Rules ? FMath::DegreesToRadians(Rules->StartDeg) : 0.0);
	SetClipping(EWidgetClipping::ClipToBounds);
}

double SBrixThing::YawNow() const
{
	const double Now = FPlatformTime::Seconds();
	if (bSpin && !bHeld && !bDragging && Rules && Last >= 0) *Yaw += FMath::Min(.05, Now - Last) * UE_DOUBLE_TWO_PI / FMath::Max(1.f, Rules->TurnS);
	Last = Now;
	return *Yaw;
}

FReply SBrixThing::OnMouseButtonDown(const FGeometry&, const FPointerEvent& E)
{
	if (!bDrag || E.GetEffectingButton() != EKeys::LeftMouseButton) return FReply::Unhandled();
	bDragging = bHeld = true;
	DragX = E.GetScreenSpacePosition().X;
	DragYaw = *Yaw;
	return FReply::Handled().CaptureMouse(SharedThis(this));
}

FReply SBrixThing::OnMouseButtonUp(const FGeometry&, const FPointerEvent&)
{
	if (!bDragging) return FReply::Unhandled();
	bDragging = false;
	return FReply::Handled().ReleaseMouseCapture();
}

FReply SBrixThing::OnMouseMove(const FGeometry& G, const FPointerEvent& E)
{
	if (!bDragging || !Rules) return FReply::Unhandled();
	*Yaw = DragYaw + (E.GetScreenSpacePosition().X - DragX) / FMath::Max(.01f, G.Scale) * FMath::DegreesToRadians(Rules->DragDegPx);
	return FReply::Handled();
}

int32 SBrixThing::OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const
{
	// the stage: buymenu.css .bm-stage, #24242b
	FSlateDrawElement::MakeBox(Out, Layer, G.ToPaintGeometry(), BrixLook::Round(FLinearColor::White, 10.f), ESlateDrawEffect::None, BrixLook::Srgb(38, 34, 30));   // the plinth (Lamplight, ADR-0049)
	if (!Mesh || !Rules || !Mesh->Num() || !FSlateApplication::IsInitialized()) return Layer;
	const FVector2f Sz = FVector2f(G.GetLocalSize());
	const double W = Sz.X, H = Sz.Y;
	if (W < 4 || H < 4) return Layer;
	const double Fov = FMath::DegreesToRadians(Rules->FovDeg), Pitch = FMath::DegreesToRadians(Rules->PitchDeg), Zoom = Mesh->bStandIn ? Rules->StandInFill : 1.0;
	const double Asp = W / H, Half = FMath::Min(Fov / 2, FMath::Atan(FMath::Tan(Fov / 2) * Asp)), Dist = Mesh->R / FMath::Sin(Half) * 1.06 / Zoom, F = 1 / FMath::Tan(Fov / 2);
	const double cp = FMath::Cos(Pitch), sp = FMath::Sin(Pitch), Yw = YawNow(), cy = FMath::Cos(Yw), sy = FMath::Sin(Yw);
	const FSlateRenderTransform& T = G.GetAccumulatedRenderTransform();
	const FSlateResourceHandle Handle = FSlateApplication::Get().GetRenderer()->GetResourceHandle(*FCoreStyle::Get().GetBrush("WhiteBrush"));
	TArray<FSlateVertex> V;
	TArray<SlateIndex> I;
	// a pool of lantern light on the plinth, under where the thing stands
	{
		const FVector2f C((float)(W / 2), (float)(H * .86));
		const int32 Seg = 32, B0 = V.Num();
		V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, C, FVector2f(.5f, .5f), FColor(240, 163, 90, 40)));
		for (int32 k = 0; k < Seg; ++k)
		{
			const float A = k * UE_TWO_PI / Seg;
			V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, C + FVector2f(FMath::Cos(A) * (float)W * .5f, FMath::Sin(A) * (float)H * .4f), FVector2f(.5f, .5f), FColor(240, 163, 90, 0)));
		}
		for (int32 k = 0; k < Seg; ++k) I.Append({ (SlateIndex)B0, (SlateIndex)(B0 + 1 + k), (SlateIndex)(B0 + 1 + (k + 1) % Seg) });
	}
	// the shade it stands in: a soft dark ellipse under its foot (paint())
	{
		const double Gy = Mesh->Lo.Y - Mesh->Cen.Y, Ey = cp * Gy, Ez = sp * Gy - Dist, Px = H / 2 * F / -Ez;
		const FVector2f C((float)(W / 2), (float)(H / 2 - Ey * Px));
		const float Rx = (float)(Mesh->Foot * Px * 1.25), Ry = (float)(Mesh->Foot * Px * FMath::Max(.16, sp) * 1.25);
		const int32 S0 = V.Num();
		V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, C, FVector2f(.5f, .5f), FColor(0, 0, 0, 128)));
		const int32 Seg = 28;
		for (int32 r = 0; r < 2; ++r)
			for (int32 k = 0; k < Seg; ++k)
			{
				const float A = k * UE_TWO_PI / Seg, S = r == 0 ? .7f : 1.f;
				V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, C + FVector2f(FMath::Cos(A) * Rx * S, FMath::Sin(A) * Ry * S), FVector2f(.5f, .5f), FColor(0, 0, 0, r == 0 ? 56 : 0)));
			}
		for (int32 k = 0; k < Seg; ++k)
		{
			const int32 a = S0 + 1 + k, b = S0 + 1 + (k + 1) % Seg, c = S0 + 1 + Seg + k, d = S0 + 1 + Seg + (k + 1) % Seg;
			I.Append({ (SlateIndex)S0, (SlateIndex)a, (SlateIndex)b, (SlateIndex)a, (SlateIndex)c, (SlateIndex)d, (SlateIndex)a, (SlateIndex)d, (SlateIndex)b });
		}
	}
	// the triangles: turned, seen from the raised eye, the far ones first; lit at each corner as the slice's shader lights them
	const FVector3f L = FVector3f(-.5f, .62f, .6f).GetSafeNormal();
	const float Lo = Mesh->Lo.Y, Hh = FMath::Max(.02f, Mesh->Hi.Y - Mesh->Lo.Y);
	struct FTri { double Depth; FVector2f S[3]; FColor C[3]; };
	TArray<FTri> Tris;
	Tris.Reserve(Mesh->Num());
	for (int32 t = 0; t < Mesh->Num(); ++t)
	{
		FTri Tr;
		FVector3d Qs[3];
		// each corner's own normal, turned (a model's are smooth; a box's three are its face's)
		FVector3d Nk[3];
		for (int32 k = 0; k < 3; ++k) { const FVector3f& N0 = Mesh->N[t * 3 + k]; Nk[k] = FVector3d(cy * N0.X + sy * N0.Z, N0.Y, -sy * N0.X + cy * N0.Z); }
		const FVector3d Nn = Nk[0] + Nk[1] + Nk[2];
		double Wsum = 0;
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector3f P = Mesh->P[t * 3 + k] - Mesh->Cen;
			const FVector3d R(cy * P.X + sy * P.Z, P.Y, -sy * P.X + cy * P.Z);
			Qs[k] = FVector3d(R.X, cp * R.Y - sp * R.Z, sp * R.Y + cp * R.Z);
			const double Wc = FMath::Max(1e-3, Dist - Qs[k].Z);
			Tr.S[k] = FVector2f((float)(W / 2 * (1 + F / Asp * Qs[k].X / Wc)), (float)(H / 2 * (1 - F * Qs[k].Y / Wc)));
			Wsum += Wc;
		}
		// facing away from the eye: not drawn
		const FVector3d Nv(Nn.X, cp * Nn.Y - sp * Nn.Z, sp * Nn.Y + cp * Nn.Z), Mid = (Qs[0] + Qs[1] + Qs[2]) / 3;
		if (FVector3d::DotProduct(Nv, FVector3d(0, 0, Dist) - Mid) <= 0) continue;
		Tr.Depth = Wsum / 3;
		for (int32 k = 0; k < 3; ++k)
		{
			const FVector3d Nu = Nk[k].GetSafeNormal();
			const float D = FMath::Max(0.f, (float)Nu.X * L.X + (float)Nu.Y * L.Y + (float)Nu.Z * L.Z), Up = (float)Nu.Y * .5f + .5f;
			const FVector3f Light = FVector3f(1.f, .86f, .68f) * D * 1.5f + FVector3f(.42f, .55f, .82f) * FMath::Lerp(.2f, .62f, Up) + FVector3f(.5f, .44f, .38f) * (1 - Up) * .4f + FVector3f(.1f);
			const FVector3f& Col = Mesh->C[t * 3 + k];
			const float Foot = FMath::Lerp(.7f, 1.f, FMath::SmoothStep(0.f, .22f, (Mesh->P[t * 3 + k].Y - Lo) / Hh));
			FVector3f Cc(1 - FMath::Exp(-Col.X * Light.X * Foot * 1.25f), 1 - FMath::Exp(-Col.Y * Light.Y * Foot * 1.25f), 1 - FMath::Exp(-Col.Z * Light.Z * Foot * 1.25f));
			Cc = FVector3f(FMath::Pow(Cc.X, 1 / 2.2f), FMath::Pow(Cc.Y, 1 / 2.2f), FMath::Pow(Cc.Z, 1 / 2.2f));
			if (bGrey) { const float Y = Cc.X * .3f + Cc.Y * .59f + Cc.Z * .11f; Cc = FMath::Lerp(Cc, FVector3f(Y), .85f) * .55f; }
			Tr.C[k] = FColor((uint8)FMath::Clamp(Cc.X * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(Cc.Y * 255.f, 0.f, 255.f), (uint8)FMath::Clamp(Cc.Z * 255.f, 0.f, 255.f), 255);
		}
		Tris.Add(Tr);
	}
	Tris.Sort([](const FTri& A, const FTri& B) { return A.Depth > B.Depth; });
	for (const FTri& Tr : Tris)
		for (int32 k = 0; k < 3; ++k)
		{
			I.Add((SlateIndex)V.Num());
			V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, Tr.S[k], FVector2f(.5f, .5f), Tr.C[k]));
		}
	FSlateDrawElement::MakeCustomVerts(Out, Layer + 1, Handle, V, I, nullptr, 0, 0);
	return Layer + 1;
}
