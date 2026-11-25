#pragma once

#include "CoreMinimal.h"
#include "BrixFurnishPlan.h"

class FJsonObject;
class FBrixEconomy;

// Buying furniture at the furniture outlet (FURN-022 to FURN-029; F-FURN-STORE): a port of Tools/citysim/furniture.py (and
// of the slice's store.js), rule for rule and message for message. Rules: World/rules/furniture.json, carried in
// Game/Data/life.json (furniture.rules) with furniture.check_script() and what Python gives for it, which
// FURN025_the_store_matches_the_python runs. Nothing is ticked: a delivery is its booking, and where the van is, what has
// been carried in and whether a piece is put together are worked out from the booking and the clock.

struct BRIXWORLD_API FBrixStoreRules
{
	FString CompanyId, CompanyName, Account;
	int64 Float = 0, InstantFee = 0, LeastFee = 0;
	int32 WagesBp = 0, PriceLevelBp = 10000, MostBoxes = 6, FirstDayAfter = 1, DaysAhead = 7, FeeBp = 0, LeastMin = 10;
	double PackFromM3 = 0.3, PacksTo = 6, VanMPerMin = 400, CarryMin = 4, MinPerM3 = 30;
	FVector3d Box = FVector3d(1.2, 0.5, 0.18);
	TArray<TPair<TOptional<double>, int64>> FeeByM3;     // up to this many cubic metres (none: any size), the fee
	TArray<FIntPoint> Slots;                             // each a day's minutes from and to
	bool Load(const TSharedPtr<FJsonObject>& Rules);
};

struct BRIXWORLD_API FBrixStorePiece
{
	FString Id, Kind, Line, Name;
	double W = 1, D = 1, H = 1;
	bool bRug = false, bBoxed = false, bFitted = false;
	TOptional<double> Ready;                             // put together from this minute of the game (day * 1440 + minute)
	TOptional<FVector> At;                               // where the player marked it to stand: x, z, yaw
	FString StoodLot;                                    // set down on a home's floor: its lot and storey (the game's, not the rules')
	int32 StoodFloor = -1;
};

struct BRIXWORLD_API FBrixStoreOrder
{
	FString Id, Unit;
	int32 Day = 0, Slot = 0, Landed = 0;
	TArray<FBrixStorePiece> Pieces;
};

struct BRIXWORLD_API FBrixBasketItem
{
	FString Kind, Line, How = TEXT("instant"), Unit;      // How: "instant" or "delivery"
	int32 Day = 0, Slot = 0;
	bool bAssembled = false;
	TOptional<FVector> At;
};

struct BRIXWORLD_API FBrixStoreTag
{
	FString Name, Line;
	int64 Price = 0, AssemblyFee = 0;
	FVector3f Size = FVector3f(1);
	TArray<FString> OtherLines;
	bool bFlatPacked = false;
	int32 Boxes = 0, AssemblyMin = 0;
};

struct BRIXWORLD_API FBrixVan
{
	FString Phase = TEXT("booked");                       // booked, out, at the door, back, done
	double F = 0;
	int32 Landed = 0;
};

class BRIXWORLD_API FBrixStore
{
public:
	FBrixStore(const FBrixStoreRules& InR, const FBrixFurnishRulesD& InF, FBrixEconomy& InE) : R(InR), F(InF), E(InE) {}
	FBrixStoreRules R;
	FBrixFurnishRulesD F;
	FBrixEconomy& E;
	TFunction<bool(const FString&)> Holds;               // whether the player owns or rents a unit now
	TFunction<double(const FString&)> RouteM;            // metres by road from the outlet to that unit's building
	int32 N = 0, Settled = -1;
	TArray<FBrixStorePiece> Owned;                       // bought and with the belongings
	TArray<FBrixStoreOrder> Orders;
	TArray<TPair<FString, TArray<FBrixStorePiece>>> Arrived;   // unit -> the pieces set down there, units in the order they first had one
	struct FNote { int32 Day = 0; double Minute = 0; FString Words; };
	TArray<FNote> Notes;
	bool bCatalogue = false;
	FString Why;                                          // why the last rule refused

	bool Sells(const FString& Kind) const;
	int64 Price(const FString& Kind) const;
	bool FlatPacked(const FString& Kind) const;
	int32 Boxes(const FString& Kind, FVector3d* OutStack = nullptr) const;
	bool Tag(const FString& Kind, const FString& Line, FBrixStoreTag& Out);
	int64 DeliveryFee(const TArray<FString>& Kinds) const;
	int64 AssemblyFee(const FString& Kind) const;
	int32 AssemblyMin(const FString& Kind) const;
	TArray<FIntPoint> SlotsNow() const;                  // (day, slot) that can be booked now, soonest first
	// what a basket costs, line by line; false (and Why) when it cannot be bought
	bool Quote(const TArray<FBrixBasketItem>& Basket, bool bAtHome, int64& OutTotal, TArray<FBrixFurnLine>& OutLines);
	// FURN-025, FURN-026: one ledger transaction for the whole basket, or nothing
	bool Checkout(const TArray<FBrixBasketItem>& Basket, bool bAtHome, int64& OutTotal, TArray<FString>& OutIds);
	FBrixVan Van(const FBrixStoreOrder& O, int32 Day, double Minute) const;   // FURN-027
	TArray<FString> Advance(int32 Day, double Minute);   // FURN-028: the pieces that landed just now
	TArray<FString> OnItsWay() const;
	void MoveOut(const FString& Unit);
	FBrixStorePiece* Find(const FString& Id, FString* OutWhere = nullptr);
	bool IsBoxed(const FString& Id, int32 Day, double Minute);
	bool AsPlaced(const FString& Id, int32 Day, double Minute, FBrixLayoutPiece& Out, bool& bOutBoxed);
	bool Assemble(const FString& Id, int32 Day, double Minute, int32& OutMinutes);   // FURN-029
	bool Fitter(const FString& Id, int32 Day, int32 Slot, int64& OutFee);
	void Found(int32 Day = 0, double Minute = 0);        // FURN-024: the company's float when the town begins
	void Settle(int32 Day);                               // and its takings paid on to wages and stock at each day's end
	void RemoveArrived(const FString& Unit, const FString& Id);
	TArray<FBrixStorePiece>* ArrivedAt(const FString& Unit, bool bAdd = false);
	TSharedPtr<FJsonObject> Save() const;                 // QST-008
	void Load(const TSharedPtr<FJsonObject>& S);

private:
	bool No(const FString& Words) { Why = Words; return false; }
	double Vol(const FString& Kind) const;
	struct FLegs { double Drive = 0, Leave = 0, Back = 0; TArray<double> Down; };
	FLegs Legs(const FBrixStoreOrder& O) const;
};

// FURN-028 (furniture.set_down): where a delivered piece stands: where it was marked if nothing is wrong with it there,
// else the free spot on the grid nearest the way in; false when there is no room on the floor.
BRIXWORLD_API bool BrixStoreSetDown(const FBrixFurnishRulesD& F, const FVector4f& Room, const TArray<FVector4f>& Keep, double Ceiling, const FBrixLayout& Layout,
	const FBrixLayoutPiece& Piece, const TOptional<FVector>& At, const FVector2D& Entry, FBrixLayoutPiece& Out);
