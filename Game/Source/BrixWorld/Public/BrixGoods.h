#pragma once

#include "CoreMinimal.h"

// The mall's goods for the home as they were made in Blender (FURN-065, UI-020, UI-021): Tools/blender/mall_goods_31.py
// writes Prototype/play/goods_models.js, copied to Game/Data/goods_models.json by Tools/worldgen/build_play.py. Each good
// is made twice: near (lod 0, for a home and the buy menu) and far (lod 1, the lighter one). Points are millimetres, u across,
// y up, v front to back; normals x100; a colour of the palette to each point. The two colours a model leaves open (accent0,
// accent1) are chosen by a seed, so two guitars differ (goods.js put(), buymenu.js modelMesh()).

struct BRIXWORLD_API FBrixGoodsModel
{
	struct FLod
	{
		TArray<int32> P, N;   // three numbers a point: mm; normals x100
		TArray<int32> C;      // a palette index a point
		TArray<int32> I;      // three points a triangle
	};
	FString Name;
	FVector3d Size = FVector3d::ZeroVector;   // across, front to back, high (metres): what outlets.js reads to set it out
	FLod Lods[2];
};

struct BRIXWORLD_API FBrixGoodsModels
{
	TArray<FVector3f> Palette;   // linear
	int32 A0 = -1, A1 = -1;      // the palette's two open colours
	TMap<FString, FBrixGoodsModel> Models;
	const FBrixGoodsModel* Find(const FString& Piece) const { return Models.Find(Piece); }
	// The colour of each palette entry for a seed: accent0 and accent1 picked from goods.js ACCENTS by hash(seed, "a0"/"a1")
	TArray<FVector3f> Colours(const FString& Seed) const;
};

BRIXWORLD_API bool LoadBrixGoodsModels(const FString& Path, FBrixGoodsModels& Out, FString& OutError);

// goods.js put(): the model's triangles standing at (X, Y, Z) of the slice's frame (x east, y up, z south), turned by Yaw
// (its +v, its back, runs towards (sin yaw, cos yaw)), in the colours of Seed, lod 0 near or 1 far. Three corners a
// triangle are appended to P, N (each corner's own normal) and C (linear colour).
BRIXWORLD_API void BrixGoodPut(const FBrixGoodsModels& Models, const FBrixGoodsModel& M, double X, double Y, double Z, double Yaw, const FString& Seed, int32 Lod,
	TArray<FVector3d>& P, TArray<FVector3f>& N, TArray<FVector3f>& C);
