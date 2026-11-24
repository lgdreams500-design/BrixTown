#pragma once

#include "CoreMinimal.h"

// From the data's frame (metres; x east, z south, y up) to Unreal's (centimetres; X east, Y south, Z up). Both are the
// same turn of hand once up is swapped, so a plan read from above is not mirrored.
namespace BrixUnits
{
	constexpr float CmPerM = 100.f;
	inline FVector ToWorld(const FVector2D& XZ, float Up) { return FVector(XZ.X, XZ.Y, Up) * CmPerM; }
}
