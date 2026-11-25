#pragma once

#include "CoreMinimal.h"
#include "BrixMall.h"

class FJsonObject;

// The furniture outlet outside the East Gate as the slice builds it (FURN-020, FURN-021; F-FURN-STORE): a port of
// Prototype/play/outlet.js and the play.js helpers it draws with (sideOf, fbox, SP.at, SP.n, SP.win, SP.lamp, archHead, the
// lettering), giving the same calls with the same numbers. Outlet.build(): the long hall of stone and plaster under timber
// trusses and skylights, its arched windows, sign band, door and loading doors; inside0(): the painted walking route, the
// sixteen room sets' walls with their name boards and price tags, the tills, the sales desks, the cafe counter, the packing
// tables, the warehouse's racks of flat-pack boxes, the lanterns; furnish()'s Outlet.inside(), as play.js runs it at load
// (the staff's, customers' and cafe's places, what can be used, what is in the way) and then near the camera (each set's
// furniture in its line, the cafe's chairs and tables, the lamps); and Outlet.carpark() for the block's car park, its cars
// the Soft box models (a call of kind Car: x, z, yaw, y; Text: the seed its model and paints come from). What is where is
// data: the lot of kind "furniture" and its block (World/city_01/blocks/b_14_8.json). Checked by
// FURN020_the_outlet_is_built_as_the_slice_builds_it (Tools/unreal/outlet_values.py runs outlet.js in headless Edge).
// The calls go into an FBrixMall (one level, no outlets of its own), so ABrixMall lays them as it lays Eastgate.
BRIXWORLD_API bool BuildBrixOutlet(const FJsonObject& Lot, const FJsonObject& Block, const TMap<FString, FVector2D>& CarSizes, FBrixMall& Out, FString& OutError);
// The block files' furniture outlet (the lot of kind "furniture"), built; CarModels is Game/Data/car_models.json (each model's
// length and width, for a parked car's place). False when there is none.
BRIXWORLD_API bool LoadBrixOutlet(const FString& CityDir, const FString& CarModels, FBrixMall& Out, FString& OutError);
