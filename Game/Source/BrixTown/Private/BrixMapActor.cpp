#include "BrixMapActor.h"

#include "BrixCharacter.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixDesignerActor.h"
#include "BrixGameMode.h"
#include "BrixLifeActor.h"
#include "BrixLook.h"
#include "BrixPeople.h"
#include "BrixTalkActor.h"
#include "BrixUnits.h"
#include "Camera/PlayerCameraManager.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/Texture2D.h"
#include "Fonts/FontMeasure.h"
#include "Framework/Application/SlateApplication.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Misc/Paths.h"
#include "Rendering/DrawElements.h"
#include "Rendering/SlateRenderer.h"
#include "Widgets/SLeafWidget.h"

using BrixUnits::CmPerM;

namespace
{
	// A widget that paints with a function and passes the mouse and keys to others (the maps' canvases).
	class SBrixMapCanvas : public SLeafWidget
	{
	public:
		SLATE_BEGIN_ARGS(SBrixMapCanvas) {}
		SLATE_END_ARGS()
		void Construct(const FArguments&) {}
		TFunction<int32(const FGeometry&, FSlateWindowElementList&, int32)> Painter;
		TFunction<void(const FGeometry&, const FPointerEvent&, int32)> Mouse;   // 0 down, 1 up, 2 move, 3 wheel
		TFunction<bool(const FKeyEvent&)> Key;
		virtual FVector2D ComputeDesiredSize(float) const override { return FVector2D(16, 16); }
		virtual int32 OnPaint(const FPaintArgs&, const FGeometry& G, const FSlateRect&, FSlateWindowElementList& Out, int32 Layer, const FWidgetStyle&, bool) const override
		{
			return Painter ? Painter(G, Out, Layer) : Layer;
		}
		virtual FReply OnMouseButtonDown(const FGeometry& G, const FPointerEvent& E) override { if (!Mouse) return FReply::Unhandled(); Mouse(G, E, 0); return FReply::Handled().CaptureMouse(AsShared()).SetUserFocus(AsShared()); }
		virtual FReply OnMouseButtonUp(const FGeometry& G, const FPointerEvent& E) override { if (!Mouse) return FReply::Unhandled(); Mouse(G, E, 1); return FReply::Handled().ReleaseMouseCapture(); }
		virtual FReply OnMouseMove(const FGeometry& G, const FPointerEvent& E) override { if (!Mouse) return FReply::Unhandled(); Mouse(G, E, 2); return FReply::Handled(); }
		virtual FReply OnMouseWheel(const FGeometry& G, const FPointerEvent& E) override { if (!Mouse) return FReply::Unhandled(); Mouse(G, E, 3); return FReply::Handled(); }
		virtual bool SupportsKeyboardFocus() const override { return !!Key; }
		virtual FReply OnKeyDown(const FGeometry&, const FKeyEvent& E) override { return Key && Key(E) ? FReply::Handled() : FReply::Unhandled(); }
	};

	// triangles in one colour (or textured), in the widget's own units
	struct FMesh
	{
		TArray<FSlateVertex> V;
		TArray<SlateIndex> I;
		FSlateRenderTransform T;
		void Vert(const FVector2f& P, const FColor& C, const FVector2f& UV = FVector2f(.5f, .5f)) { V.Add(FSlateVertex::Make<ESlateVertexRounding::Disabled>(T, P, UV, C)); }
		void Fan(const TArray<FVector2f>& P, const FColor& C)   // a convex shape, or one that fans from its first point
		{
			const int32 B = V.Num();
			for (const FVector2f& Q : P) Vert(Q, C);
			for (int32 k = 1; k + 1 < P.Num(); ++k) I.Append({ (SlateIndex)B, (SlateIndex)(B + k), (SlateIndex)(B + k + 1) });
		}
		void Quad(const FVector2f& A, const FVector2f& B, const FVector2f& C, const FVector2f& D, const FColor& Col) { Fan({ A, B, C, D }, Col); }
		void Seg(const FVector2f& A, const FVector2f& B, float W, const FColor& C)
		{
			FVector2f N = B - A;
			N = FVector2f(-N.Y, N.X).GetSafeNormal() * (W / 2);
			Quad(A + N, B + N, B - N, A - N, C);
		}
		void Outline(const TArray<FVector2f>& P, float W, const FColor& C) { for (int32 k = 0; k < P.Num(); ++k) Seg(P[k], P[(k + 1) % P.Num()], W, C); }
		static TArray<FVector2f> Circle(const FVector2f& O, float R, int32 Seg = 24)
		{
			TArray<FVector2f> P;
			for (int32 k = 0; k < Seg; ++k) { const float A = k * UE_TWO_PI / Seg; P.Add(O + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R); }
			return P;
		}
		void Ring(const FVector2f& O, float R, float W, const FColor& C, int32 Seg = 48)
		{
			for (int32 k = 0; k < Seg; ++k)
			{
				const float A0 = k * UE_TWO_PI / Seg, A1 = (k + 1) * UE_TWO_PI / Seg;
				const FVector2f D0(FMath::Cos(A0), FMath::Sin(A0)), D1(FMath::Cos(A1), FMath::Sin(A1));
				Quad(O + D0 * (R - W / 2), O + D1 * (R - W / 2), O + D1 * (R + W / 2), O + D0 * (R + W / 2), C);
			}
		}
		void Disc(const FVector2f& O, float R, const FColor& In, const FColor& Rim, int32 Seg = 32)   // shaded from its middle to its rim
		{
			const int32 B = V.Num();
			Vert(O, In);
			for (int32 k = 0; k < Seg; ++k) { const float A = k * UE_TWO_PI / Seg; Vert(O + FVector2f(FMath::Cos(A), FMath::Sin(A)) * R, Rim); }
			for (int32 k = 0; k < Seg; ++k) I.Append({ (SlateIndex)B, (SlateIndex)(B + 1 + k), (SlateIndex)(B + 1 + (k + 1) % Seg) });
		}
		void Band(const FVector2f& O, float R0, float R1, const FColor& C0, const FColor& C1, int32 Seg = 32)   // a ring shaded across
		{
			const int32 B = V.Num();
			for (int32 k = 0; k < Seg; ++k)
			{
				const FVector2f D(FMath::Cos(k * UE_TWO_PI / Seg), FMath::Sin(k * UE_TWO_PI / Seg));
				Vert(O + D * R0, C0);
				Vert(O + D * R1, C1);
			}
			for (int32 k = 0; k < Seg; ++k)
			{
				const SlateIndex a = (SlateIndex)(B + 2 * k), b = (SlateIndex)(B + 2 * k + 1), c = (SlateIndex)(B + 2 * ((k + 1) % Seg)), d = (SlateIndex)(B + 2 * ((k + 1) % Seg) + 1);
				I.Append({ a, b, d, a, d, c });
			}
		}
		int32 Draw(FSlateWindowElementList& Out, int32 Layer, const FSlateBrush* Brush = nullptr)
		{
			if (!I.Num() || !FSlateApplication::IsInitialized()) return Layer;
			const FSlateResourceHandle H = FSlateApplication::Get().GetRenderer()->GetResourceHandle(Brush ? *Brush : *FCoreStyle::Get().GetBrush("WhiteBrush"));
			FSlateDrawElement::MakeCustomVerts(Out, Layer, H, V, I, nullptr, 0, 0);
			V.Reset();
			I.Reset();
			return Layer + 1;
		}
	};

	FColor Css(const TCHAR* Hex, float A = 1.f) { FColor C = FColor::FromHex(Hex); C.A = (uint8)FMath::RoundToInt(A * 255); return C; }

	// the arrow of the player, pointing the way they look (play.js: the same on both maps)
	void Arrow(FMesh& M, const FVector2f& O, double Yaw, float S)
	{
		const float c = (float)FMath::Cos(Yaw), s = (float)FMath::Sin(Yaw);
		auto R = [&](float X, float Y) { return O + FVector2f(X * c - Y * s, X * s + Y * c) * S; };
		const FVector2f Tip = R(0, -13), Rt = R(9, 10), In = R(0, 5), Lt = R(-9, 10);
		const FColor Ink = Css(TEXT("17171c")), Fill = Css(TEXT("f0a35a"));
		M.Seg(Tip, Rt, 3 * S, Ink); M.Seg(Rt, In, 3 * S, Ink); M.Seg(In, Lt, 3 * S, Ink); M.Seg(Lt, Tip, 3 * S, Ink);
		M.Fan({ Tip, Rt, In }, Fill);
		M.Fan({ Tip, In, Lt }, Fill);
	}

	void Dot(FMesh& M, const FVector2f& O, float R, const FColor& Fill, float S)
	{
		M.Fan(FMesh::Circle(O, R + S), Css(TEXT("17171c")));
		M.Fan(FMesh::Circle(O, R - S), Fill);
	}

	FColor MkA(FColor C, float A) { C.A = (uint8)FMath::Clamp(FMath::RoundToInt(C.A * A), 0, 255); return C; }
	FColor MkMix(const FColor& A, const FColor& B, float T) { return FColor((uint8)FMath::Lerp((float)A.R, (float)B.R, T), (uint8)FMath::Lerp((float)A.G, (float)B.G, T), (uint8)FMath::Lerp((float)A.B, (float)B.B, T), (uint8)FMath::Lerp((float)A.A, (float)B.A, T)); }

	// a sign of World/rules/markers.json, stamped: round-capped strokes on a 24-unit square centred on O, Box pixels across
	void MkSign(FMesh& M, const FBrixMarkerSign& S, const FVector2f& O, float Box, float Stroke, const FColor& C)
	{
		const float K = Box / 24.f, W = Stroke * K;
		auto P = [&](const FVector2f& Q) { return O + (Q - FVector2f(12, 12)) * K; };
		for (int32 l = 0; l < S.Lines.Num(); ++l)
		{
			const TArray<FVector2f>& L = S.Lines[l];
			const int32 N = L.Num(), Segs = S.Closed.IsValidIndex(l) && S.Closed[l] ? N : N - 1;
			for (int32 k = 0; k < Segs; ++k) M.Seg(P(L[k]), P(L[(k + 1) % N]), W, C);
			for (const FVector2f& Q : L) M.Fan(FMesh::Circle(P(Q), W / 2, 8), C);   // round joints and caps
		}
		for (const FVector3f& D : S.Dots) M.Fan(FMesh::Circle(P(FVector2f(D.X, D.Y)), D.Z * K, 12), C);
	}

	// the brass token (ADR-0051, look test 37 option A): halo, the coin's edge below it, its shaded face, the ring in the
	// kind's colour, a light rim, the sign stamped dark; a new thing's halo is larger and a ring pulses out from it. Behind
	// a wall only its outline and sign, faint.
	void MkToken(FMesh& M, const FBrixMarkerRules& R, const FString& Kind, const FVector2f& O, float Rad, float Alpha, float Pulse, bool bBehind, bool bHalo = true)
	{
		const FBrixMarkerKind* K = R.Kind(Kind);
		const FBrixMarkerSign* S = R.SignOf(Kind);
		if (!K || !S || Alpha <= 0.01f) return;
		const FColor Cream = Css(TEXT("efe4cc"));
		if (bBehind)
		{
			const float A = Alpha * R.BehindAlpha;
			M.Ring(O, Rad, FMath::Max(1.5f, Rad * .1f), MkA(Cream, A), 40);
			MkSign(M, *S, O, Rad * 2 * R.SignSize * .5f * 1.9f, R.SignStroke, MkA(Cream, A));
			return;
		}
		if (bHalo)
		{
			const float Hs = K->bNew ? FMath::Lerp(R.HaloSize, R.HaloNewSize, .5f + .5f * Pulse) : R.HaloSize;
			const float Ha = (K->bNew ? R.HaloNewAlpha : R.HaloAlpha) * Alpha;
			M.Disc(O, Rad * Hs * .45f, MkA(R.Halo, Ha), MkA(R.Halo, Ha * .35f), 32);
			M.Band(O, Rad * Hs * .45f, Rad * Hs, MkA(R.Halo, Ha * .35f), MkA(R.Halo, 0.f), 32);
			if (K->bNew) M.Ring(O, Rad * FMath::Lerp(1.12f, R.PulseRing, Pulse), Rad * .08f, MkA(Css(TEXT("ffcf8a")), .55f * (1.f - Pulse) * Alpha), 48);
		}
		M.Fan(FMesh::Circle(O + FVector2f(0, Rad * R.EdgeDepth), Rad, 40), MkA(R.Edge, Alpha));
		const FVector2f Lit = O + FVector2f(-.22f, -.28f) * Rad;   // lit from above, a little from the left
		M.Disc(O, Rad * .72f, MkA(MkMix(R.FaceLight, R.Face, .35f), Alpha), MkA(R.Face, Alpha), 40);
		M.Band(O, Rad * .72f, Rad, MkA(R.Face, Alpha), MkA(R.FaceDark, Alpha), 40);
		M.Disc(Lit, Rad * .42f, MkA(R.FaceLight, Alpha * .8f), MkA(R.FaceLight, 0.f), 24);
		M.Ring(O, Rad * .84f, Rad * R.RingWidth, MkA(K->Colour, Alpha), 48);
		M.Ring(O, Rad * .97f, FMath::Max(1.f, Rad * .06f), MkA(R.Rim, .8f * Alpha), 48);
		MkSign(M, *S, O + FVector2f(0, Rad * .03f), Rad * R.SignSize, R.SignStroke, MkA(R.Stamp, Alpha));
	}

	// far off: a spark of the kind's colour in a little light
	void MkSpark(FMesh& M, const FBrixMarkerRules& R, const FString& Kind, const FVector2f& O, float Px, float Alpha)
	{
		const FBrixMarkerKind* K = R.Kind(Kind);
		if (!K || Alpha <= 0.01f) return;
		M.Disc(O, Px * 4.f, MkA(R.Halo, .6f * Alpha), MkA(R.Halo, 0.f), 20);
		M.Fan(FMesh::Circle(O, Px * 1.25f, 14), MkA(K->Colour, Alpha));
		M.Fan(FMesh::Circle(O, Px * .7f, 12), MkA(Css(TEXT("ffd59a")), Alpha));
	}

	// the tokens over the street, far ones first, then the captions of the near ones over them (ADR-0051)
	int32 MkPaintTokens(const ABrixMap& Map, const FGeometry& G, FSlateWindowElementList& Out, int32 Layer, FMesh& M, const FVector2f& Size)
	{
		const FBrixMarkerRules& R = Map.Markers;
		if (!Map.Tokens.Num() || !GEngine || !GEngine->GameViewport) return Layer;
		const float Sc = Size.Y / 1080.f;
		const double Now = FPlatformTime::Seconds();
		FVector2D Vp(1, 1);
		GEngine->GameViewport->GetViewportSize(Vp);
		const APlayerController* PC = UGameplayStatics::GetPlayerController(&Map, 0);
		struct FCap { FVector2f At; FString Key, Text; float Alpha; };
		TArray<FCap> Caps;
		for (const ABrixMap::FToken& T : Map.Tokens)
		{
			const float Pulse = R.Pulse(Now + T.Seed * .37);
			const FVector W = T.World + FVector(0, 0, R.Bob(Now, T.Seed) * CmPerM);
			FVector2D S;
			const bool bAhead = Map.ProjectToScreen(W, S);
			const FVector2f At = FVector2f(S / Vp) * Size;
			const float Edge = R.EdgePx * Sc;
			if (!bAhead || At.X < 0 || At.Y < 0 || At.X > Size.X || At.Y > Size.Y)
			{
				if (!T.bEdge || !PC || !PC->PlayerCameraManager) continue;
				FVector2f D = At - Size / 2;
				if (!bAhead)   // behind the camera: which way to turn
				{
					const FRotationMatrix Rm(PC->PlayerCameraManager->GetCameraRotation());
					const FVector V = W - PC->PlayerCameraManager->GetCameraLocation();
					D = FVector2f((float)FVector::DotProduct(V, Rm.GetScaledAxis(EAxis::Y)), (float)-FVector::DotProduct(V, Rm.GetScaledAxis(EAxis::Z)));
					if (FMath::Abs(D.X) < 1.f) D.X = 1.f;
					D.Y = FMath::Max(D.Y, FMath::Abs(D.X) * .3f);   // low on the screen, so it reads as behind you
				}
				if (D.IsNearlyZero()) D = FVector2f(0, 1);
				D.Normalize();
				const FVector2f Half = Size / 2 - FVector2f(Edge, Edge);
				const float Tx = FMath::Abs(D.X) > 1e-4f ? Half.X / FMath::Abs(D.X) : 1e9f, Ty = FMath::Abs(D.Y) > 1e-4f ? Half.Y / FMath::Abs(D.Y) : 1e9f;
				const FVector2f P = Size / 2 + D * FMath::Min(Tx, Ty);
				const float Rr = R.TokenPx * Sc * .38f;
				MkToken(M, R, T.Kind, P, Rr, FMath::Max(.7f, T.Alpha), Pulse, false);
				const FVector2f Nn(-D.Y, D.X);
				M.Fan({ P + D * (Rr * 1.95f), P + D * (Rr * 1.3f) + Nn * (Rr * .6f), P + D * (Rr * 1.3f) - Nn * (Rr * .6f) }, Css(TEXT("e0964e")));
				continue;
			}
			if (T.bSpark) { MkSpark(M, R, T.Kind, At, R.SparkPx * Sc, T.Alpha); continue; }
			const float Rad = T.Size * Sc * .5f;
			MkToken(M, R, T.Kind, At, Rad, T.Alpha, Pulse, T.bBehind);
			if (!T.Caption.IsEmpty()) Caps.Add({ At - FVector2f(0, Rad * 1.25f + 6 * Sc), T.Key, T.Caption, T.Alpha });
		}
		Layer = M.Draw(Out, Layer);
		if (!Caps.Num()) return Layer;
		const TSharedRef<FSlateFontMeasure> Ms = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
		const FSlateFontInfo F = BrixLook::Font(11), Fk = BrixLook::Font(10, true);
		for (const FCap& C : Caps)   // a small Lamplight card: dark stone on a darker edge, the key on a paper cap
		{
			const FVector2f Tz = FVector2f(Ms->Measure(C.Text, F));
			const float Hh = 28.f, Kw = C.Key.IsEmpty() ? 0.f : 26.f, Wd = Tz.X + Kw + 22.f;
			const FVector2D O(C.At.X - Wd / 2, C.At.Y - Hh);
			auto Box = [&](const FVector2D& At2, const FVector2D& Sz, const FLinearColor& Fill, float Rr, int32 L)
			{
				FSlateDrawElement::MakeBox(Out, L, G.ToPaintGeometry(Sz, FSlateLayoutTransform(At2)), BrixLook::Round(FLinearColor::White, Rr), ESlateDrawEffect::None, Fill.CopyWithNewOpacity(Fill.A * C.Alpha));
			};
			Box(O + FVector2D(0, 3), FVector2D(Wd, Hh), BrixLook::StoneDeep(), 9.f, Layer);
			Box(O, FVector2D(Wd, Hh), BrixLook::Stone().CopyWithNewOpacity(.96f), 9.f, Layer + 1);
			if (!C.Key.IsEmpty())
			{
				Box(O + FVector2D(7, 4), FVector2D(20, 20), BrixLook::Paper(), 5.f, Layer + 2);
				const FVector2f Kz = FVector2f(Ms->Measure(C.Key, Fk));
				FSlateDrawElement::MakeText(Out, Layer + 3, G.ToPaintGeometry(FVector2D(Kz), FSlateLayoutTransform(O + FVector2D(17 - Kz.X / 2, 14 - Kz.Y / 2))), C.Key, Fk, ESlateDrawEffect::None, BrixLook::Ink().CopyWithNewOpacity(C.Alpha));
			}
			FSlateDrawElement::MakeText(Out, Layer + 3, G.ToPaintGeometry(FVector2D(Tz), FSlateLayoutTransform(O + FVector2D(11 + Kw, Hh / 2 - Tz.Y / 2))), C.Text, F, ESlateDrawEffect::None, BrixLook::Cream().CopyWithNewOpacity(C.Alpha));
		}
		return Layer + 4;
	}

	FColor DotColour(int32 Kind) { return Kind == 2 ? Css(TEXT("f4e9d0")) : Kind ? Css(TEXT("f0c35a")) : Css(TEXT("9fd1c4")); }

	FSlateFontInfo TagFont(int32 Size, float Outline)
	{
		FSlateFontInfo F = BrixLook::Font(Size, true);
		F.OutlineSettings.OutlineSize = FMath::Max(1, FMath::RoundToInt(Outline));
		F.OutlineSettings.OutlineColor = FLinearColor::FromSRGBColor(FColor(23, 23, 28, 217));
		return F;
	}

	void Words(FSlateWindowElementList& Out, int32 Layer, const FGeometry& G, const FString& S, const FVector2f& At, const FSlateFontInfo& F, const FLinearColor& C, float HAlign = .5f)
	{
		const FVector2f Sz = FVector2f(FSlateApplication::Get().GetRenderer()->GetFontMeasureService()->Measure(S, F));
		FSlateDrawElement::MakeText(Out, Layer, G.ToPaintGeometry(FVector2D(Sz), FSlateLayoutTransform(FVector2D(At.X - Sz.X * HAlign, At.Y - Sz.Y * .8f))), S, F, ESlateDrawEffect::None, C);
	}

	const TSet<FString>& ShopKinds()   // named on the big map when asked (UI-012)
	{
		static const TSet<FString> S = { TEXT("shop"), TEXT("food"), TEXT("bank"), TEXT("dealer"), TEXT("library"), TEXT("museum"), TEXT("cinema"), TEXT("furniture"), TEXT("health") };
		return S;
	}
}

ABrixMap::ABrixMap()
{
	PrimaryActorTick.bCanEverTick = true;
	PrimaryActorTick.bTickEvenWhenPaused = true;
}

void ABrixMap::Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, const FString& CityDir)
{
	Day = InDay;
	{
		FString MarkError;
		if (!LoadBrixMarkers(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/markers.json"))), Markers, MarkError))
			UE_LOG(LogTemp, Error, TEXT("Brix Town: the markers did not load: %s"), *MarkError);
	}
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) Lots.Add(&L);
	FBrixMapExtras X;
	FString Error;
	if (!LoadBrixMapExtras(CityDir, City, X, Error)) UE_LOG(LogTemp, Error, TEXT("Brix Town: the map's data did not load: %s"), *Error);
	const double T0 = FPlatformTime::Seconds();
	Image = BrixDrawMap(City, X, MapPixels, TreeRadius);
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the map drawn, %d px square, in %.2f s"), Image.N, FPlatformTime::Seconds() - T0);
	Texture = UTexture2D::CreateTransient(Image.N, Image.N, PF_B8G8R8A8, TEXT("BrixMap"));
	if (Texture)
	{
		Texture->SRGB = true;
		Texture->AddressX = TA_Clamp;
		Texture->AddressY = TA_Clamp;
		Texture->Filter = TF_Bilinear;
		Texture->NeverStream = true;
		void* Data = Texture->GetPlatformData()->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
		FMemory::Memcpy(Data, Image.Px.GetData(), Image.Px.Num() * sizeof(FColor));
		Texture->GetPlatformData()->Mips[0].BulkData.Unlock();
		Texture->UpdateResource();
		Brush.SetResourceObject(Texture);
		Brush.ImageSize = FVector2D(Image.N, Image.N);
		Brush.DrawAs = ESlateBrushDrawType::Image;
	}
	if (!GEngine || !GEngine->GameViewport) return;
	const double MetresPerMap = 900.0 / (Image.K * 900.0 / Image.N);   // 900 map units over the whole picture

	// ---- the minimap and the words over the street (play.js drawMaps(), hud())
	TSharedRef<SBrixMapCanvas> H = SNew(SBrixMapCanvas);
	H->SetVisibility(EVisibility::HitTestInvisible);
	H->Painter = [this](const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) -> int32
	{
		const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
		if (bOpen || !Mode || (Mode->Life && Mode->Life->IsMenu()) || (Mode->Designer && Mode->Designer->IsOpen())) return Layer;   // not over the menu or the character designer (play.js: body.designing)
		FMesh M{ {}, {}, G.GetAccumulatedRenderTransform() };
		const FVector2f Size = FVector2f(G.GetLocalSize());
		Layer = MkPaintTokens(*this, G, Out, Layer, M, Size);
		// the words over the street, nearest last (drawn over the far ones)
		for (const FTag& T : Tags)
		{
			FVector2D S;
			if (!ProjectToScreen(T.World, S)) continue;
			FVector2D Vp(1, 1);
			GEngine->GameViewport->GetViewportSize(Vp);
			const FVector2f At = FVector2f(S / Vp) * Size;
			Words(Out, Layer, G, T.Text, At, TagFont(T.Size * 3 / 4, 3.f), FLinearColor::FromSRGBColor(FColor(230, 220, 195)).CopyWithNewOpacity(T.Alpha));
		}
		++Layer;
		if (!bMini) return Layer;
		// the round map: the 230 m round the player, with the people, the marks and the arrow (S: the slice's 408 units across)
		const float Px = MiniSize, U = Px / 408.f, Z = 408.f / MiniSpan;
		const FVector2f C(Size.X - 18.f - Px / 2, 16.f + Px / 2);
		M.Fan(FMesh::Circle(C, Px / 2, 64), Css(TEXT("24242b")));
		Layer = M.Draw(Out, Layer);
		{
			const int32 Seg = 64;
			const int32 B = M.V.Num();
			auto UV = [&](const FVector2f& Off) { const FVector2D W = Player + FVector2D(Off) / U / Z; const FVector2D P = Image.ToPx(W) / Image.N; return FVector2f(P); };
			M.Vert(C, FColor::White, UV(FVector2f::ZeroVector));
			for (int32 k = 0; k < Seg; ++k)
			{
				const float A = k * UE_TWO_PI / Seg;
				const FVector2f Off = FVector2f(FMath::Cos(A), FMath::Sin(A)) * (Px / 2);
				M.Vert(C + Off, FColor::White, UV(Off));
			}
			for (int32 k = 0; k < Seg; ++k) M.I.Append({ (SlateIndex)B, (SlateIndex)(B + 1 + k), (SlateIndex)(B + 1 + (k + 1) % Seg) });
			Layer = M.Draw(Out, Layer, &Brush);
		}
		const float R2 = FMath::Square(Px / 2 - 2);
		if (bPeople)
			for (const FVector2D& F : Folk)
			{
				const FVector2f O = C + FVector2f(F - Player) * Z * U;
				if ((O - C).SizeSquared() > R2) continue;
				M.Quad(O + FVector2f(-2, -2) * U, O + FVector2f(2, -2) * U, O + FVector2f(2, 2) * U, O + FVector2f(-2, 2) * U, Css(TEXT("17171c")));
			}
		for (const FDot& D : Dots)
		{
			FVector2f O = FVector2f(D.Pos - Player) * Z;
			O = FVector2f(FMath::Clamp(O.X, -192.f, 192.f), FMath::Clamp(O.Y, -192.f, 192.f));   // the edge of the canvas (12 in from 408)
			if (O.SizeSquared() > FMath::Square(190.f)) O = O.GetSafeNormal() * 190.f;   // kept within the round
			if (D.Marker.IsEmpty()) Dot(M, C + O * U, 7 * U, DotColour(D.Kind), U);
			else MkToken(M, Markers, D.Marker, C + O * U, Markers.MiniTokenPx * .5f * (MiniSize / 204.f), 1.f, 0.f, false, false);
		}
		Arrow(M, C, Yaw, U);
		M.Ring(C, Px / 2, 2.f, Css(TEXT("d9cfb6"), .55f), 64);
		return M.Draw(Out, Layer);
	};
	Hud = H;
	GEngine->GameViewport->AddViewportWidgetContent(H, 14);

	// ---- the big map (UI-018): the town in a square of 84% of the screen, the legend beside it
	TSharedRef<SBrixMapCanvas> B = SNew(SBrixMapCanvas);
	B->SetVisibility(EVisibility::Collapsed);
	struct FBox { FVector2f O; float S = 1; };
	auto Square = [](const FVector2f& Size) { const float S = FMath::Min(Size.X, Size.Y) * .84f; const float Lw = 270.f; return FBox{ FVector2f((Size.X - S - 26 - Lw) / 2, (Size.Y - S) / 2), S }; };
	TSharedPtr<FVector2D> DragAt = MakeShared<FVector2D>(-1, -1), DragC = MakeShared<FVector2D>(0, 0);
	TSharedPtr<bool> Moved = MakeShared<bool>(false);
	B->Painter = [this, Square, MetresPerMap](const FGeometry& G, FSlateWindowElementList& Out, int32 Layer) -> int32
	{
		FMesh M{ {}, {}, G.GetAccumulatedRenderTransform() };
		const FVector2f Size = FVector2f(G.GetLocalSize());
		M.Quad(FVector2f(0, 0), FVector2f(Size.X, 0), Size, FVector2f(0, Size.Y), FColor(23, 23, 28, 209));
		const FBox Q = Square(Size);
		const float U = Q.S / 900.f, Zm = (float)View.Z;
		M.Quad(Q.O, Q.O + FVector2f(Q.S, 0), Q.O + FVector2f(Q.S, Q.S), Q.O + FVector2f(0, Q.S), Css(TEXT("3a3d2c")));
		Layer = M.Draw(Out, Layer);
		{   // the part of the town in view
			const float H = 450.f / Zm;
			const FVector2f A((float)(View.Cx - H) / 900.f, (float)(View.Cy - H) / 900.f), Bv((float)(View.Cx + H) / 900.f, (float)(View.Cy + H) / 900.f);
			M.Vert(Q.O, FColor::White, A);
			M.Vert(Q.O + FVector2f(Q.S, 0), FColor::White, FVector2f(Bv.X, A.Y));
			M.Vert(Q.O + FVector2f(Q.S, Q.S), FColor::White, Bv);
			M.Vert(Q.O + FVector2f(0, Q.S), FColor::White, FVector2f(A.X, Bv.Y));
			M.I.Append({ 0, 1, 2, 0, 2, 3 });
			Layer = M.Draw(Out, Layer, &Brush);
		}
		const double K9 = 900.0 / MetresPerMap;   // map units a metre
		auto X = [&](const FVector2D& W) { const FVector2D P9 = (W + FVector2D(Image.O, Image.O)) * K9; return Q.O + FVector2f(View.ScreenAt(P9)) * U; };
		auto In = [&](const FVector2f& P, float Pad) { return P.X > Q.O.X - Pad && P.Y > Q.O.Y - Pad && P.X < Q.O.X + Q.S + Pad && P.Y < Q.O.Y + Q.S + Pad; };
		if (bMine)   // the player's own buildings and the places they rent, outlined in the lantern's orange (play.js drawMaps(), map_mine)
			for (const int32 n : Mine)
			{
				if (!Lots.IsValidIndex(n)) continue;
				const FBrixLot* L = Lots[n];
				const FVector2D Ctr = L->Centre();
				const double PxPerM = FMath::Max(1e-3, (double)(X(Ctr + FVector2D(1, 0)) - X(Ctr)).Size());
				const FVector2D Hf = L->Size() / 2 + FVector2D(2.0 * U / PxPerM, 2.0 * U / PxPerM);   // 2 px out, as the slice's strokeRect
				const double A = FMath::DegreesToRadians((double)L->AngleDeg), Ca = FMath::Cos(A), Sa = FMath::Sin(A);
				TArray<FVector2f> Qd;
				for (const FVector2D& D : { FVector2D(-Hf.X, -Hf.Y), FVector2D(Hf.X, -Hf.Y), FVector2D(Hf.X, Hf.Y), FVector2D(-Hf.X, Hf.Y) })
					Qd.Add(X(Ctr + FVector2D(D.X * Ca - D.Y * Sa, D.X * Sa + D.Y * Ca)));
				if (In(Qd[0], 40) || In(Qd[2], 40)) M.Outline(Qd, 3.f * U, Css(TEXT("f0a35a")));
			}
		if (bShops)   // each name once, none over another (more of them fit as you zoom in)
		{
			const FSlateFontInfo F = TagFont(FMath::Max(6, FMath::RoundToInt(9 * U * .75f)), 2.f * U);
			TArray<FSlateRect> Put;
			TSet<FString> Seen;
			const TSharedRef<FSlateFontMeasure> Measure = FSlateApplication::Get().GetRenderer()->GetFontMeasureService();
			for (const FBrixLot* L : Lots)
			{
				if (!ShopKinds().Contains(L->Kind) || L->Name.IsEmpty() || Seen.Contains(L->Name)) continue;
				const FVector2f P = X(L->Centre());
				if (!In(P, -10)) continue;
				const FVector2f Sz = FVector2f(Measure->Measure(L->Name, F));
				const FSlateRect R(P.X - Sz.X / 2 - 2, P.Y - Sz.Y, P.X + Sz.X / 2 + 2, P.Y + 2);
				if (Put.ContainsByPredicate([&](const FSlateRect& O) { return FSlateRect::DoRectanglesIntersect(O, R); })) continue;
				Put.Add(R);
				Seen.Add(L->Name);
				Words(Out, Layer + 2, G, L->Name, P, F, FLinearColor::FromSRGBColor(Css(TEXT("f4e9d0"))));
			}
		}
		if (bPeople)
		{
			const float S = FMath::Min(5.f, 1.5f * FMath::Sqrt(Zm)) * U;
			for (const FVector2D& F : Folk) { const FVector2f P = X(F); if (In(P, -S)) M.Quad(P - FVector2f(S, S), P + FVector2f(S, -S), P + FVector2f(S, S), P + FVector2f(-S, S), Css(TEXT("17171c"))); }
		}
		for (const FDot& D : Dots)
		{
			const FVector2f P = X(D.Pos);
			if (!In(P, -4 * U)) continue;
			if (!D.Marker.IsEmpty()) { MkToken(M, Markers, D.Marker, P, Markers.MapTokenPx * .5f * FMath::Max(.75f, U), 1.f, 0.f, false, false); continue; }
			if (D.Kind == 2)   // the mark: a diamond
			{
				const TArray<FVector2f> Dm = { P + FVector2f(0, -11) * U, P + FVector2f(8, 0) * U, P + FVector2f(0, 11) * U, P + FVector2f(-8, 0) * U };
				M.Fan({ P + FVector2f(0, -13) * U, P + FVector2f(10, 0) * U, P + FVector2f(0, 13) * U, P + FVector2f(-10, 0) * U }, Css(TEXT("17171c")));
				M.Fan(Dm, Css(TEXT("f4e9d0")));
				continue;
			}
			Dot(M, P, 8 * U, DotColour(D.Kind), U);
		}
		const FVector2f Me = X(Player);
		if (In(Me, 0)) Arrow(M, Me, Yaw, U);
		M.Outline({ Q.O, Q.O + FVector2f(Q.S, 0), Q.O + FVector2f(Q.S, Q.S), Q.O + FVector2f(0, Q.S) }, 2.f, Css(TEXT("d9cfb6"), .5f));
		Layer = M.Draw(Out, Layer);
		// the legend beside it
		const FSlateFontInfo F = BrixLook::Font(11);
		float Y = Q.O.Y + 14;
		for (const auto& L : BrixMapLegend())
		{
			const FVector2f A(Q.O.X + Q.S + 26, Y);
			M.Quad(A + FVector2f(0, -10), A + FVector2f(12, -10), A + FVector2f(12, 2), A + FVector2f(0, 2), L.Key);
			Words(Out, Layer + 1, G, L.Value, A + FVector2f(21, 0), F, BrixLook::Cream(), 0.f);
			Y += FMath::Min(28.f, Q.S / 26);
		}
		Words(Out, Layer + 1, G, View.Z > 1.01 ? FString::Printf(TEXT("x%.1f   wheel: zoom   drag: move   0: whole town   click: mark"), View.Z) : FString(TEXT("wheel: zoom   drag: move   click: mark   M or Esc: close")),
			Q.O + FVector2f(10, Q.S - 10), BrixLook::Font(10, true), FLinearColor::FromSRGBColor(FColor(244, 233, 208, 217)), 0.f);
		return M.Draw(Out, Layer) + 2;
	};
	B->Mouse = [this, Square, MetresPerMap, DragAt, DragC, Moved](const FGeometry& G, const FPointerEvent& E, int32 What)
	{
		const FBox Q = Square(FVector2f(G.GetLocalSize()));
		const FVector2D L = G.AbsoluteToLocal(E.GetScreenSpacePosition());
		const FVector2D At = (L - FVector2D(Q.O)) / Q.S * 900.0;   // canvas units
		if (What == 3) { View.Zoom(E.GetWheelDelta() > 0 ? 1.25 : 1 / 1.25, At); return; }
		if (What == 0)
		{
			if (E.GetEffectingButton() == EKeys::MiddleMouseButton) { View.Whole(); return; }
			*DragAt = At; *DragC = FVector2D(View.Cx, View.Cy); *Moved = false;
			return;
		}
		if (What == 2)
		{
			if (DragAt->X < -0.5 || !E.IsMouseButtonDown(EKeys::LeftMouseButton)) return;
			const FVector2D D = At - *DragAt;
			if (D.Size() > 4) *Moved = true;
			View.Cx = DragC->X - D.X / View.Z;
			View.Cy = DragC->Y - D.Y / View.Z;
			View.Clamp();
			return;
		}
		if (What == 1 && E.GetEffectingButton() == EKeys::LeftMouseButton && DragAt->X > -0.5)   // a click (not the end of a drag) marks the place
		{
			const bool bClick = !*Moved && At.X >= 0 && At.Y >= 0 && At.X <= 900 && At.Y <= 900;
			*DragAt = FVector2D(-1, -1);
			if (!bClick) return;
			const FVector2D W = View.MapAt(At) * (MetresPerMap / 900.0) - FVector2D(Image.O, Image.O);
			MarkAt(W);
		}
	};
	B->Key = [this](const FKeyEvent& E) -> bool
	{
		const FKey K = E.GetKey();
		if (K == EKeys::Escape || K == EKeys::M) { Close(); return true; }
		if (K == EKeys::Equals || K == EKeys::Add) { View.Zoom(1.25, FVector2D(450, 450)); return true; }
		if (K == EKeys::Hyphen || K == EKeys::Subtract) { View.Zoom(1 / 1.25, FVector2D(450, 450)); return true; }
		if (K == EKeys::Zero || K == EKeys::NumPadZero) { View.Whole(); return true; }
		return false;
	};
	Big = B;
	GEngine->GameViewport->AddViewportWidgetContent(B, 28);
}

void ABrixMap::EndPlay(const EEndPlayReason::Type Reason)
{
	if (GEngine && GEngine->GameViewport)
	{
		if (Hud) GEngine->GameViewport->RemoveViewportWidgetContent(Hud.ToSharedRef());
		if (Big) GEngine->GameViewport->RemoveViewportWidgetContent(Big.ToSharedRef());
	}
	Super::EndPlay(Reason);
}

void ABrixMap::Open()
{
	if (bOpen || !Big) return;
	bOpen = true;
	Big->SetVisibility(EVisibility::Visible);
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->BeginPanel();
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocus(Big, EFocusCause::SetDirectly);
}

void ABrixMap::Close()
{
	if (!bOpen) return;
	bOpen = false;
	Big->SetVisibility(EVisibility::Collapsed);
	if (ABrixCharacter* Me = Cast<ABrixCharacter>(UGameplayStatics::GetPlayerCharacter(this, 0))) Me->EndPanel();
	if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
}

const FBrixLot* ABrixMap::LotAt(const FVector2D& P) const
{
	for (const FBrixLot* L : Lots)
	{
		const double A = -FMath::DegreesToRadians((double)L->AngleDeg);
		const FVector2D D = P - L->Centre(), R(D.X * FMath::Cos(A) - D.Y * FMath::Sin(A), D.X * FMath::Sin(A) + D.Y * FMath::Cos(A)), H = L->Size() / 2;
		if (FMath::Abs(R.X) <= H.X && FMath::Abs(R.Y) <= H.Y) return L;
	}
	return nullptr;
}

FString ABrixMap::MarkAt(const FVector2D& At, const FString& Name)
{
	const FBrixLot* L = LotAt(At);
	const FString Said = Mark.MarkAt(At, Name.IsEmpty() ? (L ? L->Name : FString()) : Name, !Name.IsEmpty());
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this))) if (Mode->Life) Mode->Life->Toast(Said);
	return Said;
}

bool ABrixMap::ProjectToScreen(const FVector& World, FVector2D& Out) const
{
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	if (!PC || !PC->PlayerCameraManager) return false;
	const FVector Cam = PC->PlayerCameraManager->GetCameraLocation(), Fwd = PC->PlayerCameraManager->GetCameraRotation().Vector();
	if (FVector::DotProduct(World - Cam, Fwd) < 30.f) return false;   // behind the camera, or at it
	return PC->ProjectWorldLocationToScreen(World, Out, false);
}

void ABrixMap::Settings()
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode || !Mode->Life || !Mode->Life->Settings.Settings.Num()) return;
	const FBrixSettings& S = Mode->Life->Settings;
	bMini = S.On(TEXT("display"), TEXT("minimap"));
	bNames = S.On(TEXT("display"), TEXT("names"));
	bPeople = S.On(TEXT("map"), TEXT("map_people"));
	bQuests = S.On(TEXT("map"), TEXT("map_quests"));
	bShops = S.On(TEXT("map"), TEXT("map_shops"));
	bWaypoint = S.On(TEXT("map"), TEXT("map_waypoint"));
	bMine = S.On(TEXT("map"), TEXT("map_mine"));
	Mine = Mode->Life->MyLots();
}

void ABrixMap::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	Settings();
	Gather(DeltaSeconds);
}

void ABrixMap::Gather(float Dt)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (!Mode || !Me || !Day) return;
	const FVector At = Me->GetActorLocation();
	Player = FVector2D(At.X, At.Y) / CmPerM;
	const float Feet = (float)(At.Z / CmPerM - .85);
	if (PC->PlayerCameraManager) Yaw = FMath::DegreesToRadians(PC->PlayerCameraManager->GetCameraRotation().Yaw + 90.0);   // the slice's: 0 north, clockwise
	const double Minute = Mode->ClockMinute();
	FolkLeft -= Dt;
	if (FolkLeft <= 0.f)   // where everyone out of doors is, for the dots
	{
		FolkLeft = FolkEvery;
		Folk.Reset();
		for (int32 i = 0; i < Day->Num(); ++i) { const TOptional<FBrixWhere> W = Day->Where(i, Minute); if (W && !W->bInside) Folk.Add(W->Pos); }
	}
	TargetLeft -= Dt;
	const FBrixQuests* Q = Mode->Life ? Mode->Life->Quests() : nullptr;
	if (TargetLeft <= 0.f && Q && Mode->Life->Economy())   // what the quests want (life.js targets(), twice a second)
	{
		TargetLeft = .5f;
		Targets = BrixQuestTargets(*Q, Mode->Life->QuestLines(), *Mode->Life->Economy());
		BrixKeepNearestShops(Targets, Markers, Player, [this](int32 N) { return Lots.IsValidIndex(N) ? Lots[N]->Centre() : FVector2D::ZeroVector; });
		Marked.Reset();
		for (const FBrixMapTarget& T : Targets) if (T.Resident >= 0 && !Marked.Contains(T.Resident)) Marked.Add(T.Resident, BrixMarkerKindFor(T, Markers, FString()));
	}
	if (bWaypoint && Mark.Reached(Player) && Mode->Life) Mode->Life->Toast(TEXT("You are there: ") + Mark.Name + TEXT("."));
	Dots.Reset();
	if (Mark.bSet && bWaypoint) Dots.Add({ Mark.Pos, 2, TEXT("mark") });
	auto KindOf = [&](const FBrixMapTarget& T) { return BrixMarkerKindFor(T, Markers, Lots.IsValidIndex(T.Lot) ? Lots[T.Lot]->Kind : FString()); };
	if (bQuests)
		for (const FBrixMapTarget& T : Targets)
		{
			if (T.Resident >= 0) { const TOptional<FBrixWhere> W = Day->Where(T.Resident, Minute); if (W) Dots.Add({ W->Pos, T.bOffer ? 0 : 1, KindOf(T) }); }
			else if (Lots.IsValidIndex(T.Lot)) Dots.Add({ Lots[T.Lot]->Centre(), 1, KindOf(T) });
			else if (T.Lot < 0) Dots.Add({ T.Pos, 1, KindOf(T) });
		}
	// the words over the street (play.js hud(), life.js mark(), waypointTag())
	Tags.Reset();
	const bool bBusy = (Mode->Talk && Mode->Talk->IsTalking());
	if (Mode->PeopleActor() && !bBusy)
		for (const ABrixPeople::FHeard& H : Mode->PeopleActor()->Heard)
		{
			const double D = FVector2D::Distance(H.Pos, Player);
			const auto Pos = [&](double Up) { return FVector(H.Pos.X * CmPerM, H.Pos.Y * CmPerM, (H.Up + Up * H.S) * CmPerM); };
			if (!bNames || D >= NameReach || FMath::Abs(H.Up - Feet) >= NameRise || !Day->Names.IsValidIndex(H.Resident)) continue;
			const TOptional<FBrixWhere> W = Day->Where(H.Resident, Minute);
			const FString Act = W && Day->Acts.IsValidIndex(W->Act) ? Day->Acts[W->Act] : FString();
			const FString Job = Day->Careers.IsValidIndex(H.Resident) && !Day->Careers[H.Resident].IsEmpty() ? Day->Careers[H.Resident] : Day->Roles.IsValidIndex(Day->Role[H.Resident]) ? Day->Roles[Day->Role[H.Resident]] : FString();
			const FString* What = Mode->Talk ? (W && W->bWalking ? Mode->Talk->Words().Going.Find(Act) : Mode->Talk->Words().Doing.Find(Act)) : nullptr;
			const FString Doing = What ? *What : W && W->bWalking ? FString(TEXT("walking")) : Act;
			Tags.Add({ Pos(H.Pose == 2 ? 1.0 : H.Pose == 3 ? .55 : 1.88), Day->Names[H.Resident] + TEXT(", ") + Job + TEXT(" · ") + Doing, FMath::Clamp((float)(NameReach - D) / 3.f, 0.f, 1.f), 14 });
		}
	if (Mode->Life && !bBusy)   // PROP-023: a board over the door of every building near with something on the market (life.js boards())
		for (int32 n = 0; n < Lots.Num(); ++n)
		{
			const FBrixLot* L = Lots[n];
			if (L->Floors <= 0) continue;
			const double D = FVector2D::Distance(L->Door, Player);
			if (D > BoardReach) continue;
			const FString T = Mode->Life->BoardText(n);
			if (!T.IsEmpty()) Tags.Add({ FVector(L->Door.X * CmPerM, L->Door.Y * CmPerM, 3.1 * CmPerM), TEXT("▣ ") + T, FMath::Min(1.f, (float)(BoardReach - D) / 8.f), 14 });
		}
	GatherTokens(Feet, Minute, bBusy, KindOf);
	Tags.Sort([this](const FTag& A, const FTag& B) { return FVector2D::DistSquared(FVector2D(A.World) / CmPerM, Player) > FVector2D::DistSquared(FVector2D(B.World) / CmPerM, Player); });
}

void ABrixMap::GatherTokens(float Feet, double Minute, bool bBusy, TFunctionRef<FString(const FBrixMapTarget&)> KindOf)   // ADR-0051
{
	Tokens.Reset();
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const APlayerController* PC = UGameplayStatics::GetPlayerController(this, 0);
	const APawn* Me = PC ? PC->GetPawn() : nullptr;
	if (bBusy || !Mode || !PC || !Day || !Markers.Kinds.Num()) return;
	const UWorld* World = GetWorld();
	const FVector Cam = PC->PlayerCameraManager ? PC->PlayerCameraManager->GetCameraLocation() : FVector(Player.X * CmPerM, Player.Y * CmPerM, (Feet + 1.6) * CmPerM);
	auto Add = [&](const FVector& Wp, const FString& Kind, bool bPlace, bool bPerson, const FString& Name, int32 Seed) -> FToken*
	{
		const double D = FVector2D::Distance(FVector2D(Wp.X, Wp.Y) / CmPerM, Player);
		const FBrixMarkerRules::FLook L = Markers.At(D, bPlace);
		if (L.Alpha <= 0.f) return nullptr;
		FToken T;
		T.World = Wp; T.Kind = Kind; T.Alpha = L.Alpha; T.Size = L.Size; T.bSpark = L.bSpark; T.Seed = Seed; T.Dist = D; T.bEdge = Markers.bEdgeArrows;
		if (!L.bSpark && World)   // behind a wall: a faint outline
		{
			FHitResult Hit;
			FCollisionQueryParams Qp(SCENE_QUERY_STAT(BrixMarker), false, Me);
			T.bBehind = World->LineTraceSingleByChannel(Hit, Cam, Wp, ECC_Visibility, Qp) && Hit.Distance < FVector::Distance(Cam, Wp) - 150.f;
			if (bPerson && D < Markers.NearM && !T.bBehind) { T.Key = TEXT("E"); T.Caption = Markers.Words(Kind, Name); }
			else if (!bPerson && D < Markers.NearM * 2) T.Caption = Markers.Words(Kind, Name);
		}
		return &Tokens.Add_GetRef(T);
	};
	TMap<int32, const ABrixPeople::FHeard*> Seen;
	if (Mode->PeopleActor()) for (const ABrixPeople::FHeard& H : Mode->PeopleActor()->Heard) Seen.Add(H.Resident, &H);
	for (const auto& Mk : Marked)
	{
		const FString Name = Day->Names.IsValidIndex(Mk.Key) ? Day->Names[Mk.Key] : FString();
		if (const ABrixPeople::FHeard* const* H = Seen.Find(Mk.Key))
		{
			const double Up = (*H)->Pose == 2 ? 1.25 : (*H)->Pose == 3 ? .8 : 2.12;
			Add(FVector((*H)->Pos.X * CmPerM, (*H)->Pos.Y * CmPerM, ((*H)->Up + Up * (*H)->S + Markers.HeadUpM) * CmPerM), Mk.Value, false, true, Name, Mk.Key);
		}
		else if (const TOptional<FBrixWhere> W = Day->Where(Mk.Key, Minute); W && !W->bInside)   // not drawn yet: a spark where they are
			Add(FVector(W->Pos.X * CmPerM, W->Pos.Y * CmPerM, (Feet + 2.12 + Markers.HeadUpM) * CmPerM), Mk.Value, false, true, Name, Mk.Key);
	}
	TSet<FString> Pointed;   // of the shops for a thing to get, only the nearest has an arrow at the screen's edge
	for (const FBrixMapTarget& T : Targets)
	{
		if (T.Resident >= 0) continue;
		const FVector2D P = Lots.IsValidIndex(T.Lot) ? Lots[T.Lot]->Centre() : T.Pos;
		const FString Name = Lots.IsValidIndex(T.Lot) ? Lots[T.Lot]->Name : FString();
		FToken* Tk = Add(FVector(P.X * CmPerM, P.Y * CmPerM, (Feet + Markers.PlaceUpM) * CmPerM), KindOf(T), true, false, Name, T.Lot >= 0 ? 1000 + T.Lot : 999);
		if (Tk && !T.Item.IsEmpty()) { bool bSeen = false; Pointed.Add(T.Item, &bSeen); Tk->bEdge &= !bSeen; }
		if (Tk && !T.Item.IsEmpty() && !Tk->Caption.IsEmpty())   // what to get there: "Loaf of bread sold here"
		{
			const FBrixEconomy* E = Mode->Life ? Mode->Life->Economy() : nullptr;
			const FBrixEconomyRules::FItem* It = E ? E->Item(T.Item) : nullptr;
			Tk->Caption = Markers.HaveWords.Replace(TEXT("{item}"), *(It ? It->Name : T.Item));
		}
	}
	if (Mark.bSet && bWaypoint)
		if (FToken* T = Add(FVector(Mark.Pos.X * CmPerM, Mark.Pos.Y * CmPerM, (Feet + 2.5) * CmPerM), TEXT("mark"), true, false, Mark.Name, 7))
			if (!T->bSpark) T->Caption = Mark.TagText(Player).Replace(TEXT("◆ "), TEXT(""));
	Tokens.Sort([](const FToken& A, const FToken& B) { return A.Dist > B.Dist; });
}
