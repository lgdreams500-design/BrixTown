#pragma once

#include "CoreMinimal.h"

class FJsonObject;
struct FBrixMapTarget;
struct FBrixQuestRules;
class FBrixQuests;

// The markers over heads and over places (QST-005, ADR-0051; look test 37 "Brass token"): a brass coin with a sign
// stamped in it, one sign for each kind of thing to do (so it reads without colour), ringed in the kind's colour, in a
// warm halo. Far off only a spark; nearer the token, gently bobbing; near enough to act, a caption with the key; behind
// a wall a faint outline; off the screen an arrow at its edge. New things (a quest offered, one to hand in) glow
// brighter and pulse; things under way are steady and dimmer. Everything here is data, World/rules/markers.json, checked
// by world.marker_problems() and BrixMarkerProblems(); BrixTown paints it.
struct BRIXWORLD_API FBrixMarkerSign
{
	TArray<TArray<FVector2f>> Lines;     // on a 24-unit square, y down
	TArray<bool> Closed;
	TArray<FVector3f> Dots;              // x, y, r
};

struct BRIXWORLD_API FBrixMarkerKind
{
	FString Sign, Words;
	FColor Colour = FColor::White;
	bool bNew = false;
};

struct BRIXWORLD_API FBrixMarkerRules
{
	TMap<FString, FBrixMarkerKind> Kinds;
	FString HaveWords = TEXT("{item} sold here");   // the caption over a shop with something a quest needs
	int32 HaveShops = 3;                             // how many of the nearest such shops are marked
	TMap<FString, FString> PlaceKinds;   // lot kind -> marker kind, for a place a quest sends you to
	TMap<FString, FBrixMarkerSign> Signs;
	// the token
	FColor Face, FaceLight, FaceDark, Edge, Rim, Stamp, Halo;
	float HaloAlpha = .55f, HaloNewAlpha = .75f, HaloSize = 2.4f, HaloNewSize = 3.1f;
	float RingWidth = .16f, EdgeDepth = .14f, SignSize = 1.05f, SignStroke = 2.6f;
	// how it behaves
	float TokenPx = 26, TokenNearPx = 34, NearM = 6, SparkPx = 4, TokenM = 45, SparkM = 160, FadeM = 10;
	float PlaceM = 30, PlaceUpM = 4.2f, HeadUpM = .42f, BobM = .06f, BobS = 2.4f, PulseS = 1.8f, PulseRing = 1.45f;
	float BehindAlpha = .45f, EdgePx = 46, MapTokenPx = 15, MiniTokenPx = 13;
	bool bEdgeArrows = true;
	TSharedPtr<FJsonObject> Json;        // the file as read, so a broken copy can be checked

	const FBrixMarkerKind* Kind(const FString& K) const { return Kinds.Find(K); }
	const FBrixMarkerSign* SignOf(const FString& K) const { const FBrixMarkerKind* M = Kinds.Find(K); return M ? Signs.Find(M->Sign) : nullptr; }
	// the caption's words: "{name}" filled in
	FString Words(const FString& K, const FString& Name) const;
	// how strong a token is at a distance: 0 beyond spark_m, a spark (Spark) between token_m and spark_m, the token fading
	// in over fade_m inside token_m; Size its pixels (token_px growing to token_near_px inside near_m)
	struct FLook { float Alpha = 0, Size = 0; bool bSpark = false; };
	FLook At(double Metres, bool bPlace) const;
	// the pulse of a new thing (0..1, one beat every pulse_s) and the bob (metres) at a time in seconds
	float Pulse(double Seconds) const;
	float Bob(double Seconds, int32 Seed) const;
};

BRIXWORLD_API bool ParseBrixMarkers(const TSharedPtr<FJsonObject>& Json, FBrixMarkerRules& Out, FString& OutError);
BRIXWORLD_API bool LoadBrixMarkers(const FString& Path, FBrixMarkerRules& Out, FString& OutError);
BRIXWORLD_API TArray<FString> BrixMarkerProblems(const TSharedPtr<FJsonObject>& Json, const TSet<FString>& LotKinds);

// The kind of marker for what a quest or a job wants (BrixQuestTargets() gives "offer", "talk", "handin", "job", "go",
// "bank" or "stall"): a place a quest sends you to shows what it is (a shop's basket, a café's cup) when its lot kind is
// one of place_kinds.
// Of the shops marked for a thing to get, only the HaveShops nearest the player (Where gives a lot's middle); the nearest
// of each thing comes first. Everything else is kept as it is.
BRIXWORLD_API void BrixKeepNearestShops(TArray<FBrixMapTarget>& Targets, const FBrixMarkerRules& R, const FVector2D& Player, TFunctionRef<FVector2D(int32)> Where);
BRIXWORLD_API FString BrixMarkerKindFor(const FBrixMapTarget& T, const FBrixMarkerRules& R, const FString& LotKind);
