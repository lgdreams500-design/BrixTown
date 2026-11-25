#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class FJsonValue;
struct FBrixLot;
struct FBrixBlock;

// The building designer's rules (docs/specs/domains/BUILDER.md BUILD-002, BUILD-004 to BUILD-007, BUILD-016 to BUILD-019;
// F-BLD-DESIGN): a port of the slice's builder.js, itself a port of Tools/citysim/builder.py and world.py's built(),
// allowed() and design_problems(), rule for rule and message for message. A design is the lot's data document
// (inside.json "dsg", world.designs()), kept here as JSON as the slice keeps it, naming every choice by its kit id;
// BrixBuilt() turns it into the colours and sizes the town is built with. BUILD005_remodels_match_the_python runs
// builder.check_script() (inside.json builder.checks) and compares every result with Python's.

// The kit, the planning rules and the stone and brick colours (inside.json "builder": World/rules/building_kit.json,
// planning.json and surfaces.json's patterns).
struct BRIXWORLD_API FBrixBuilderRules
{
	TSharedPtr<FJsonObject> Kit, Planning, Patterns, Checks;
	bool IsSet() const { return Kit.IsValid() && Planning.IsValid() && Patterns.IsValid(); }
};
BRIXWORLD_API bool LoadBrixBuilderRules(const TSharedPtr<FJsonObject>& Builder, FBrixBuilderRules& Out, FString& OutError);

// What the designer needs of the lot: its footprint, storeys and where it stands (the slice's lot, l.block)
struct BRIXWORLD_API FBrixDesignLot
{
	FString Id, Kind, Name, District, Template;
	FVector2D Min = FVector2D::ZeroVector, Max = FVector2D::ZeroVector;
	int32 Floors = 0;
	TArray<FString> Tags;
};

// A design built (world.built()): per facade, in the kit's order n, s, e, w
struct BRIXWORLD_API FBrixBuiltLook
{
	struct FFacade { FVector3f Plaster = FVector3f(0.6f); FString Pattern; TOptional<FVector3f> Stone; bool bStoneToEaves = false, bBrickToEaves = false; };
	FFacade Walls[4];
	FString Side;                                         // the front (shell.side)
	FVector3f Roof = FVector3f(0.44f, 0.24f, 0.18f), Shutter = FVector3f(0.5f), Frame = FVector3f(0.3f), Awning = FVector3f(0.3f);
	bool bShutters = true;
	float WinWidth = 1.1f, WinGap = 3.2f, WinTall = 1.6f;
	TArray<int32> FloorFinish;                            // per storey: the slice's material (15 boards, 16 stone ...)
	TArray<FVector3f> FloorColour, WallColour;
	static int32 FacadeIndex(const FString& F);           // n 0, s 1, e 2, w 3
};

// One option of a part, as builder.options() offers it: an id (a string, a bool or null), a name, maybe a colour
struct BRIXWORLD_API FBrixDesignOption
{
	TSharedPtr<FJsonValue> Id;
	FString Name;
	TOptional<FVector3f> Rgb;
	int32 Mat = 0;                                        // a floor finish's material
	TArray<TSharedPtr<FJsonValue>> Colors;                // a stone's colours (an index, or null for the town's own)
	bool bBrickToEaves = false;
};
using FBrixDesignChanges = TArray<TPair<FString, TSharedPtr<FJsonValue>>>;   // dotted path ('*' for every facade) -> value

struct BRIXWORLD_API FBrixCostLine { FString What; double Qty = 0; int64 Chips = 0; };

namespace BrixBuilder
{
	BRIXWORLD_API extern const TCHAR* const Facades[4];   // n, s, e, w
	BRIXWORLD_API FString FacadeName(const FString& F);   // north, south, east, west
	BRIXWORLD_API TSharedPtr<FJsonObject> Clone(const TSharedPtr<FJsonObject>& D);
	BRIXWORLD_API FString Json(const TSharedPtr<FJsonValue>& V);   // as JSON.stringify writes it (null for none)
	BRIXWORLD_API bool Same(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B);
	BRIXWORLD_API TSharedPtr<FJsonValue> Get(const TSharedPtr<FJsonObject>& D, const FString& Path);
	BRIXWORLD_API TArray<FString> Changed(const TSharedPtr<FJsonObject>& A, const TSharedPtr<FJsonObject>& B);   // sorted
	BRIXWORLD_API TSharedPtr<FJsonObject> Remodel(const TSharedPtr<FJsonObject>& Old, const FBrixDesignChanges& Changes);
	BRIXWORLD_API FBrixBuiltLook Built(const FBrixBuilderRules& R, const TSharedPtr<FJsonObject>& D);
	BRIXWORLD_API TSharedPtr<FJsonObject> BuiltFacadeJson(const FBrixBuilderRules& R, const TSharedPtr<FJsonObject>& D, const FString& F);   // for the checks
	BRIXWORLD_API bool Allowed(const TSharedPtr<FJsonObject>& Item, const TSharedPtr<FJsonObject>& D);
	BRIXWORLD_API TArray<FString> Problems(const FBrixBuilderRules& R, const TSharedPtr<FJsonObject>& D);
	BRIXWORLD_API int64 Cost(const FBrixBuilderRules& R, const FBrixDesignLot& L, const TSharedPtr<FJsonObject>& Old, const TSharedPtr<FJsonObject>& New, TArray<FBrixCostLine>& OutLines);
	// what the designer may offer on this building, by part (BUILD-017), in the slice's order of parts
	BRIXWORLD_API TArray<TPair<FString, TArray<FBrixDesignOption>>> Options(const FBrixBuilderRules& R, const FBrixDesignLot& L, const TSharedPtr<FJsonObject>& D);
	// [] when the remodel may go ahead, else what is wrong (BUILD-004, BUILD-006, BUILD-007). Owner: the whole building, or
	// the storeys held; Funds: unset not to check the money
	BRIXWORLD_API TArray<FString> Check(const FBrixBuilderRules& R, const FBrixDesignLot& L, const TSharedPtr<FJsonObject>& Old, const TSharedPtr<FJsonObject>& New,
		bool bWhole, const TArray<int32>& Floors, TOptional<int64> Funds);
	BRIXWORLD_API bool Civic(const FBrixBuilderRules& R, const FBrixDesignLot& L);
	BRIXWORLD_API FBrixDesignLot LotOf(const FBrixLot& L, const FBrixBlock& B);
}
