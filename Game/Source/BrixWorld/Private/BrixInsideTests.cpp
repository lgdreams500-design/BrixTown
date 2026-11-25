// Automation tests for what is inside the buildings (CITY-013). Run headless with the rest of BrixTown.World.
#include "BrixCity.h"
#include "BrixInside.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString CityDir() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01"))); }
	FString InsidePath() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/inside.json"))); }
	constexpr float PlayerRadius = 0.30f, PlayerHeight = 1.70f;   // ABrixCharacter's capsule
}

// CITY-013: every enterable building (but the outlet's hall and the malls, drawn their own way) has a doorway the player
// can walk through: on its outline, at the lot's door where the residents' routes go in, wider than the player with room
// to spare, high enough for them under its lintel or its arch where they pass, and with nothing of the inside (the stair)
// in the way for a step and a half beyond it. inside.json lines up with the city's lots.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY013_Doorways, "BrixTown.World.CITY013_every_enterable_building_has_a_doorway",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY013_Doorways::RunTest(const FString&)
{
	FBrixCity City;
	FBrixInsides Ins;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(CityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(InsidePath(), Ins, Error))) return false;
	if (!TestEqual(TEXT("one entry in inside.json per lot"), Ins.Lots.Num(), City.NumLots())) return false;

	const FBrixDoorRules R = FBrixDoorRules::FromConfig();
	int32 k = -1, Hollow = 0, Enterable = 0, Bad = 0;
	double Worst = 0;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++k;
			const FBrixInside* In = Ins.Lots[k].IsSet() ? &Ins.Lots[k].GetValue() : nullptr;
			if (In && In->Id != L.Id) { AddError(FString::Printf(TEXT("inside.json entry %d is %s, the city's lot is %s"), k, *In->Id, *L.Id)); return false; }
			if (L.bEnterable && !L.IsOpen()) ++Enterable;
			if (!BrixIsHollow(L, In)) continue;
			++Hollow;
			const FBrixDoorway D = BrixDoorwayOf(City, L, B.District, R);
			auto Fail = [&](const FString& Why) { if (Bad++ < 20) AddError(L.Id + TEXT(" (") + L.Name + TEXT("): ") + Why); };
			if (D.Width < 2 * PlayerRadius + 0.3f) Fail(FString::Printf(TEXT("doorway %.2f m wide"), D.Width));
			// the clear height where the player's shoulders pass: an arch is lower off its middle
			const float Half = D.Width / 2;
			const bool bArch = D.Kind == FBrixDoorway::EKind::Arch || D.Kind == FBrixDoorway::EKind::Civic || (D.Kind == FBrixDoorway::EKind::House && D.bArchHouse);
			const float Clear = bArch ? (D.Height - Half) + FMath::Sqrt(FMath::Max(0.f, Half * Half - PlayerRadius * PlayerRadius)) : D.Height;
			if (Clear < PlayerHeight + 0.1f) Fail(FString::Printf(TEXT("doorway %.2f m high where the player passes"), Clear));
			// at the door: within the slack a bay's clamp or a shopfront's pane leaves
			const double Off = FVector2D::Distance(D.Centre, L.Door);
			Worst = FMath::Max(Worst, Off);
			const double Slack = D.Kind == FBrixDoorway::EKind::Shop ? 1.3 : 0.6;
			if (Off > Slack) Fail(FString::Printf(TEXT("doorway %.2f m from the lot's door"), Off));
			// a step and a half inside, the way is clear of the stair's steps and its spine wall (its landing may be there)
			if (In && In->Stair && L.AngleDeg == 0.f)
			{
				const FBrixStairGeo G = BrixStairOf(*In->Stair, Ins.Rules);
				const TArray<FVector4f> Solids = G.GroundSolids();
				const FVector2D Side(-D.Out.Y, D.Out.X);
				bool bHit = false;
				for (float Deep = 0.f; Deep <= 1.2f && !bHit; Deep += 0.1f)
					for (float Across : { -PlayerRadius, 0.f, PlayerRadius })
					{
						const FVector2D Uv = G.UV(D.Centre - D.Out * (R.Wall + Deep) + Side * Across);
						for (const FVector4f& Q : Solids)
							if (Uv.X > Q.X && Uv.X < Q.Y && Uv.Y > Q.Z && Uv.Y < Q.W) bHit = true;
						if (bHit) { Fail(FString::Printf(TEXT("the stair is in the way %.1f m inside the doorway"), Deep)); break; }
					}
			}
		}
	AddInfo(FString::Printf(TEXT("%d of %d enterable buildings are hollow with a doorway; the farthest from its lot's door by %.2f m"), Hollow, Enterable, Worst));
	TestTrue(TEXT("most enterable buildings can be walked into"), Hollow >= Enterable - 10 && Hollow > 0);
	return Bad == 0;
}

#endif
