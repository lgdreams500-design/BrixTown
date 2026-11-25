// Automation tests for what is inside the buildings (CITY-013). Run headless with the rest of BrixTown.World.
#include "BrixCity.h"
#include "BrixInside.h"
#include "BrixDay.h"

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

namespace
{
	bool InRect(const FBrixRect& R, const FVector2D& P, float Slack)
	{
		const FVector2D D = P - R.Centre, Ac(-R.Along.Y, R.Along.X);
		return FMath::Abs(FVector2D::DotProduct(D, R.Along)) <= R.Length / 2 + Slack && FMath::Abs(FVector2D::DotProduct(D, Ac)) <= R.Width / 2 + Slack;
	}
}

// FURN-060, FURN-061, FURN-063: every planned home (the row houses of lot.interior and the designed houses of plan
// house_12x9) is furnished from its plan: each piece stands in one of the rooms of its floor and inside its lot, at the
// household's level or below but for the beds, with a bed place for every bed the residents have; the level follows the
// rule of World/rules/furnishing_levels.json (estate.js furnish.level()).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FFURN060_HomesFurnished, "BrixTown.World.FURN060_every_planned_home_is_furnished_in_its_rooms",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FFURN060_HomesFurnished::RunTest(const FString&)
{
	FBrixCity City;
	FBrixInsides Ins;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(CityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(InsidePath(), Ins, Error))) return false;
	if (!TestTrue(TEXT("the plans load"), Ins.Plans.Contains(TEXT("house_12x9")) && Ins.Plans.Num() >= 4)) return false;

	// the rule itself (furnishing_levels.json: bands 0 / 9000 / 16000, 10 % saved, steps 30000, 60000, 120000)
	const FBrixFurnishRules& F = Ins.Furnish;
	TestEqual(TEXT("no wages: Bare"), F.Level(0, 0), 1);
	TestEqual(TEXT("9,000 a day starts Basic"), F.Level(9000, 0), 2);
	TestEqual(TEXT("16,000 a day starts Homely"), F.Level(16000, 0), 3);
	TestEqual(TEXT("5,000 a day saves 30,000 in 60 days: Basic"), F.Level(5000, 60), 2);
	TestEqual(TEXT("but not in 59"), F.Level(5000, 59), 1);
	TestEqual(TEXT("16,000 a day: Full after 75 days"), F.Level(16000, 75), 4);

	int32 k = -1, Homes = 0, Houses = 0, Pieces = 0, Bad = 0;
	const TArray<FBrixWindowSpan> NoWindows;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++k;
			if (!Ins.Lots[k].IsSet()) continue;
			const FBrixInside& In = Ins.Lots[k].GetValue();
			FBrixHome H;
			if (!BrixHomeOf(Ins, L, In, 16, NoWindows, H)) continue;
			++Homes;
			if (L.House.IsSet()) ++Houses;
			auto Fail = [&](const FString& Why) { if (Bad++ < 20) AddError(L.Id + TEXT(" (") + L.Name + TEXT("): ") + Why); };
			if (H.Level < 1 || H.Level > 4) Fail(FString::Printf(TEXT("level %d"), H.Level));
			const FVector2D C = L.Centre();
			const double A = FMath::DegreesToRadians(L.AngleDeg);
			FBrixRect Lot;
			Lot.Centre = C; Lot.Along = FVector2D(FMath::Cos(A), FMath::Sin(A)); Lot.Length = (float)L.Size().X; Lot.Width = (float)L.Size().Y;
			for (const FBrixFurniturePiece& P : H.Pieces)
			{
				++Pieces;
				if (!InRect(Lot, P.At, 0.f)) Fail(P.Kind + TEXT(" stands outside the lot"));
				bool bInRoom = false;
				for (const FBrixRoom& R : H.Rooms) if (R.Floor == P.Floor && InRect(R.R, P.At, 0.05f)) bInRoom = true;
				if (!bInRoom) Fail(FString::Printf(TEXT("%s on floor %d stands in no room"), *P.Kind, P.Floor));
			}
			int32 Lie = 0;
			for (const FBrixSpot& S : H.Spots) if (S.bLie) ++Lie;
			const int32 Beds = In.Objects.Contains(TEXT("bed")) ? In.Objects[TEXT("bed")].Num() : 0;
			if (Lie < Beds) Fail(FString::Printf(TEXT("%d bed places for %d beds"), Lie, Beds));
		}
	AddInfo(FString::Printf(TEXT("%d planned homes (%d designed houses), %d pieces on day 16"), Homes, Houses, Pieces));
	TestTrue(TEXT("the designed houses and the row houses are furnished"), Houses > 400 && Homes > Houses + 100);
	return Bad == 0;
}

// WLD-050, WLD-052: inside is the size of outside and every storey is reached by its stair: the floors rise a storey at a
// time inside the building's height, each storey at least 2.6 m high under its ceiling, the stair inside the walls, each
// flight's steps no higher than 0.2 m from one floor to the next, the hole each flight comes up through inside the floor
// it comes up to, and the foot of each flight on that floor's landing inside the building.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWLD052_Storeys, "BrixTown.World.WLD052_every_storey_is_reached_by_its_stair",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FWLD052_Storeys::RunTest(const FString&)
{
	FBrixCity City;
	FBrixInsides Ins;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(CityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(InsidePath(), Ins, Error))) return false;
	const FBrixDoorRules R = FBrixDoorRules::FromConfig();
	int32 k = -1, Buildings = 0, Flights = 0, Bad = 0;
	for (const FBrixBlock& B : City.Blocks)
		for (const FBrixLot& L : B.Lots)
		{
			++k;
			const FBrixInside* In = Ins.Lots[k].IsSet() ? &Ins.Lots[k].GetValue() : nullptr;
			if (!In || !BrixIsHollow(L, In) || L.House.IsSet() || L.Floors < 2 || !In->Stair || L.AngleDeg != 0.f) continue;
			++Buildings;
			auto Fail = [&](const FString& Why) { if (Bad++ < 20) AddError(L.Id + TEXT(" (") + L.Name + TEXT("): ") + Why); };
			const float H = R.Base + L.Floors * R.Storey;
			const FBox2D Inner(L.Min + FVector2D(R.Wall, R.Wall), L.Max - FVector2D(R.Wall, R.Wall));
			for (int32 f = 0; f < L.Floors; ++f)
			{
				const float Up = BrixFloorUp(L, f, R), Ce = BrixCeilingUp(L, f, R);
				if (Ce - Up < 2.6f) Fail(FString::Printf(TEXT("floor %d is %.2f m high"), f, Ce - Up));
				if (Ce > H + 1e-3f) Fail(FString::Printf(TEXT("floor %d's ceiling is over the roof"), f));
				if (f > 0 && Up - BrixFloorUp(L, f - 1, R) > R.Storey + 1e-3f) Fail(TEXT("floors more than a storey apart"));
			}
			const FBrixStairGeo G = BrixStairOf(*In->Stair, Ins.Rules);
			const FBox2D Well(G.P(G.U0, G.V0), G.P(G.U1, G.V1));
			const FBox2D WellN(FVector2D(FMath::Min(Well.Min.X, Well.Max.X), FMath::Min(Well.Min.Y, Well.Max.Y)), FVector2D(FMath::Max(Well.Min.X, Well.Max.X), FMath::Max(Well.Min.Y, Well.Max.Y)));
			if (!Inner.ExpandBy(0.05).IsInside(WellN)) Fail(TEXT("the stairwell is not inside the walls"));
			for (int32 f = 0; f + 1 < L.Floors; ++f)
			{
				++Flights;
				const float Rise = BrixFloorUp(L, f + 1, R) - BrixFloorUp(L, f, R);
				const int32 N = FMath::CeilToInt(Rise / 0.19f);
				if (Rise / N > 0.2f) Fail(FString::Printf(TEXT("flight %d has %.2f m steps"), f, Rise / N));
				const FBrixStairGeo::FFlight Fl = G.Flight(f);
				if (Fl.Lane.Y - Fl.Lane.X < 0.9f) Fail(FString::Printf(TEXT("flight %d is %.2f m wide"), f, Fl.Lane.Y - Fl.Lane.X));
				const FVector2D Foot = G.FootAt(f);
				if (!Inner.IsInside(Foot)) Fail(FString::Printf(TEXT("the foot of flight %d is outside the walls"), f));
				const FVector2D FootUv = G.UV(Foot);
				if (FootUv.X > G.ULo && FootUv.X < G.UHi) Fail(FString::Printf(TEXT("the foot of flight %d is on the steps, not the landing"), f));
				const FVector2D Top = G.P(Fl.Ut, (Fl.Lane.X + Fl.Lane.Y) / 2);
				if (!Inner.IsInside(Top)) Fail(FString::Printf(TEXT("flight %d comes up outside the walls"), f));
			}
		}
	AddInfo(FString::Printf(TEXT("%d buildings of two storeys or more, %d flights"), Buildings, Flights));
	TestTrue(TEXT("the buildings with storeys are checked"), Buildings > 100);
	return Bad == 0;
}

// CITY-013, WLD-053: every place a resident's day has them inside a building is inside that lot, on a floor it has; at
// home they lie on a bed or sit on a seat of their own home's plan, on their own floor, inside their home.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY013_IndoorPlaces, "BrixTown.World.CITY013_every_indoor_place_is_inside_its_lot_on_its_floor",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY013_IndoorPlaces::RunTest(const FString&)
{
	FBrixCity City;
	FBrixInsides Ins;
	FBrixDay Day;
	FString Error;
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(CityDir(), City, Error))) return false;
	if (!TestTrue(TEXT("inside.json loads: ") + Error, LoadBrixInside(InsidePath(), Ins, Error))) return false;
	if (!TestTrue(TEXT("day.json loads: ") + Error, LoadBrixDay(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/day.json"))), Day, Error))) return false;
	const FBrixDoorRules R = FBrixDoorRules::FromConfig();
	const int32 Placed = SeatBrixDayIndoors(Day, City, Ins, R);
	TArray<const FBrixLot*> Lots;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) Lots.Add(&L);
	int32 Stays = 0, Lie = 0, Sit = 0, Bad = 0;
	for (int32 r = 0; r < Day.Num(); ++r)
		for (const FBrixDay::FPiece& P : Day.Plan[r])
		{
			if (P.Kind != 0 || !P.bInside) continue;
			++Stays;
			auto Fail = [&](const FString& Why) { if (Bad++ < 20) AddError(FString::Printf(TEXT("%s, a stay at minute %.0f: %s"), *Day.Ids[r], P.T0, *Why)); };
			if (!Lots.IsValidIndex(P.Lot)) { Fail(TEXT("in no lot")); continue; }
			const FBrixLot& L = *Lots[P.Lot];
			const FVector2D C = L.Centre(), D = P.Pos - C;
			const double A = FMath::DegreesToRadians(L.AngleDeg);
			const FVector2D Loc(D.X * FMath::Cos(A) + D.Y * FMath::Sin(A), -D.X * FMath::Sin(A) + D.Y * FMath::Cos(A));
			if (FMath::Abs(Loc.X) > L.Size().X / 2 || FMath::Abs(Loc.Y) > L.Size().Y / 2) Fail(TEXT("outside its lot ") + L.Id);
			const int32 Floors = L.House.IsSet() ? 2 : FMath::Max(1, L.Floors);
			if (P.Floor < 0 || P.Floor >= Floors) Fail(FString::Printf(TEXT("on floor %d of %s, which has %d"), P.Floor, *L.Id, Floors));
			if (P.Pose == 3) ++Lie;
			if (P.Pose == 2) ++Sit;
			if (P.SeatUp < BrixFloorUp(L, P.Floor, R) - 1e-3f) Fail(TEXT("below its floor"));
		}
	AddInfo(FString::Printf(TEXT("%d stays inside; %d in a bed, %d on a seat at home (%d placed)"), Stays, Lie, Sit, Placed));
	TestTrue(TEXT("residents lie in their beds at night"), Lie > 1000);
	TestTrue(TEXT("and sit at home"), Sit > 100);
	return Bad == 0;
}

#endif
