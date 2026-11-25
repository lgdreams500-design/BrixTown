#pragma once

#include "CoreMinimal.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Styling/CoreStyle.h"
#include "Widgets/Images/SImage.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Text/STextBlock.h"

// "Lamplight" (ADR-0049, accepted by the owner 2026-11-25): how the game's own screens look in the Unreal build: the game menu, the counters and the buy
// menu. Cards of warm dark stone with soft round corners and a thicker lower edge (W3), cream lettering, the page's colour
// from the town's palette as its lintel and its marks (U1), one lantern-orange main action (U6, W6), the town dimmed and
// warmed behind, not hidden (U4). The colours are data (settings.json menu.lamplight); the sizes are here, in one place, so every screen keeps them.
namespace BrixLook
{
	inline FLinearColor Srgb(uint8 R, uint8 G, uint8 B, uint8 A = 255) { return FLinearColor::FromSRGBColor(FColor(R, G, B, A)); }
	// the colours, from settings.json menu.lamplight (SetPalette at start); these are the accepted values, should it be missing
	inline TMap<FString, FLinearColor>& Palette() { static TMap<FString, FLinearColor> P; return P; }
	inline FLinearColor From(const TCHAR* Key, const FLinearColor& Else) { const FLinearColor* C = Palette().Find(Key); return C ? *C : Else; }
	inline void SetPalette(const TMap<FString, FColor>& Data) { Palette().Reset(); for (const auto& C : Data) Palette().Add(C.Key, FLinearColor::FromSRGBColor(C.Value)); }
	inline FLinearColor Stone() { return From(TEXT("stone"), Srgb(43, 39, 35)); }             // a card
	inline FLinearColor StoneDeep() { return From(TEXT("stone_deep"), Srgb(30, 27, 24)); }    // its lower edge, a well inside it
	inline FLinearColor StoneLift() { return From(TEXT("stone_lift"), Srgb(56, 51, 45)); }    // a row picked, a field
	inline FLinearColor Cream() { return From(TEXT("cream"), Srgb(239, 228, 204)); }
	inline FLinearColor Dim() { return From(TEXT("dim"), Srgb(169, 157, 134)); }
	inline FLinearColor Ink() { return From(TEXT("ink"), Srgb(28, 27, 33)); }
	inline FLinearColor Lantern() { return From(TEXT("lantern"), Srgb(224, 150, 78)); }
	inline FLinearColor Paper() { return From(TEXT("paper"), Srgb(233, 220, 190)); }          // a price tag
	inline FLinearColor ShortTag() { return From(TEXT("short_tag"), Srgb(122, 66, 52)); }     // a price the purse cannot meet
	inline FLinearColor Good() { return From(TEXT("good"), Srgb(159, 209, 160)); }
	inline FLinearColor Bad() { return From(TEXT("bad"), Srgb(233, 144, 122)); }
	inline FLinearColor Veil() { return From(TEXT("veil"), Srgb(20, 15, 12, 150)); }          // over the town while the menu is open

	// a rounded brush, made once per colour, radius and outline and kept for the game's life
	inline const FSlateBrush* Round(const FLinearColor& Fill, float Radius, const FLinearColor& Line = FLinearColor::Transparent, float LineWidth = 0.f)
	{
		static TMap<FString, TUniquePtr<FSlateRoundedBoxBrush>> Made;
		const FString Key = FString::Printf(TEXT("%s|%.1f|%s|%.1f"), *Fill.ToString(), Radius, *Line.ToString(), LineWidth);
		TUniquePtr<FSlateRoundedBoxBrush>& B = Made.FindOrAdd(Key);
		if (!B) B = MakeUnique<FSlateRoundedBoxBrush>(Fill, Radius, Line, LineWidth);
		return B.Get();
	}
	inline FSlateFontInfo Font(int32 Size, bool bBold = false) { return FCoreStyle::GetDefaultFontStyle(bBold ? "Bold" : "Regular", Size); }
	inline TSharedRef<STextBlock> Text(const FString& S, int32 Size = 14, const FLinearColor& C = Cream(), bool bBold = false, bool bWrap = true)
	{
		return SNew(STextBlock).Text(FText::FromString(S)).Font(Font(Size, bBold)).ColorAndOpacity(C).AutoWrapText(bWrap);
	}
	inline TSharedRef<SWidget> Caps(const FString& S, const FLinearColor& C = Dim())   // a small heading in spaced capitals
	{
		return SNew(STextBlock).Text(FText::FromString(S.ToUpper())).Font(Font(10, true)).ColorAndOpacity(C);
	}
	// A chunky card: the face on a darker lower edge a few pixels deep (W3)
	inline TSharedRef<SWidget> Card(TSharedRef<SWidget> Content, const FMargin& Pad = FMargin(18, 14), const FLinearColor& Face = Stone(), float Radius = 14.f)
	{
		return SNew(SBorder).BorderImage(Round(StoneDeep(), Radius)).Padding(FMargin(0, 0, 0, 4))
			[ SNew(SBorder).BorderImage(Round(Face, Radius, Srgb(255, 236, 200, 18), 1.f)).Padding(Pad)[ Content ] ];
	}
	// A button: the main one filled with lantern (or the page's colour) on a darker edge; the others outlined
	inline TSharedRef<SWidget> Button(const FString& Label, TFunction<void()> OnClick, bool bMain = true, bool bEnabled = true, const FLinearColor& Fill = Lantern(), const FLinearColor& Letters = Ink(), int32 Size = 12)
	{
		const FLinearColor Edge = Fill * .72f;
		TSharedRef<SWidget> Face = bMain
			? (TSharedRef<SWidget>)(SNew(SBorder).BorderImage(Round(bEnabled ? FLinearColor(Edge.R, Edge.G, Edge.B, 1) : Srgb(70, 64, 56), 10.f)).Padding(FMargin(0, 0, 0, 3))
				[ SNew(SBorder).BorderImage(Round(bEnabled ? Fill : Srgb(96, 88, 77), 10.f)).Padding(FMargin(14, 6)).HAlign(HAlign_Center)
					[ SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(Size, true)).ColorAndOpacity(bEnabled ? Letters : Srgb(40, 37, 33)) ] ])
			: (TSharedRef<SWidget>)(SNew(SBorder).BorderImage(Round(FLinearColor::Transparent, 10.f, bEnabled ? Srgb(239, 228, 204, 90) : Srgb(239, 228, 204, 40), 1.5f)).Padding(FMargin(14, 6, 14, 7)).HAlign(HAlign_Center)
				[ SNew(STextBlock).Text(FText::FromString(Label)).Font(Font(Size, true)).ColorAndOpacity(bEnabled ? Cream() : Dim()) ]);
		return SNew(SButton).ButtonStyle(&FCoreStyle::Get().GetWidgetStyle<FButtonStyle>("NoBorder")).IsFocusable(false).IsEnabled(bEnabled).ContentPadding(0)
			.OnClicked_Lambda([OnClick]() { OnClick(); return FReply::Handled(); })[ Face ];
	}
	// A price tag of paper, as tied to the goods in a shop window
	inline TSharedRef<SWidget> Tag(const FString& S, bool bShort = false, int32 Size = 12)
	{
		return SNew(SBorder).BorderImage(Round(bShort ? ShortTag() : Paper(), 6.f)).Padding(FMargin(7, 2, 8, 3))
			[ SNew(STextBlock).Text(FText::FromString(S)).Font(Font(Size, true)).ColorAndOpacity(bShort ? Srgb(251, 243, 224) : Ink()) ];
	}
	// A small rounded mark in a colour, with a letter on it
	inline TSharedRef<SWidget> Tile(const FLinearColor& Fill, const FString& Letter, const FLinearColor& Letters, float Size = 26.f)
	{
		return SNew(SBox).WidthOverride(Size).HeightOverride(Size)
			[ SNew(SBorder).BorderImage(Round(Fill, Size * .3f)).HAlign(HAlign_Center).VAlign(VAlign_Center).Padding(0)
				[ SNew(STextBlock).Text(FText::FromString(Letter)).Font(Font((int32)(Size * .46f), true)).ColorAndOpacity(Letters) ] ];
	}
	inline TSharedRef<SWidget> Rule() { return SNew(SBorder).BorderImage(Round(Srgb(239, 228, 204, 22), 0.f)).Padding(0)[ SNew(SBox).HeightOverride(1.f) ]; }
}
