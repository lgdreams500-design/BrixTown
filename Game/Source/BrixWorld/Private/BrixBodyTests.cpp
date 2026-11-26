// ART-004, ANIM-001, ANIM-002: the people of the Unreal build are the slice's. Run headless with the other tests:
//   UnrealEditor-Cmd.exe Game/BrixTown.uproject -ExecCmds="Automation RunTests BrixTown.World; Quit" -unattended -nullrhi
#include "BrixBody.h"
#include "BrixCity.h"
#include "BrixDay.h"
#include "BrixPlay.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FVector3f Col(const TSharedPtr<FJsonObject>& O, const TCHAR* Key)
	{
		const TArray<TSharedPtr<FJsonValue>>* A;
		if (!O->TryGetArrayField(Key, A) || A->Num() < 3) return FVector3f(-1);
		return FVector3f((float)(*A)[0]->AsNumber(), (float)(*A)[1]->AsNumber(), (float)(*A)[2]->AsNumber());
	}
}

// QST-014, ADR-0059: Noor (r0262) wears her approved look (World/rules/cast_looks.json, from baker_look_47.py's N), not her
// job's stock clothes: an olive dress to the shin with short sleeves and bare legs under it, a cream bib apron, brown shoes,
// grey hair up in a bun with locks along the hairline, and age at the brow, the eyes and the mouth. Everyone else is as the
// slice draws them (ART004): the new parts are drawn only for a look with age or a dress.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FQST014_NoorsLook, "BrixTown.World.QST014_noor_wears_her_approved_look",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FQST014_NoorsLook::RunTest(const FString&)
{
	FBrixPeopleRules Rules;
	FBrixDay Day;
	FString Error;
	if (!TestTrue(TEXT("the people's tables load: ") + Error, LoadBrixPeopleRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/people.json"))), Rules, Error))) return false;
	if (!TestTrue(TEXT("the day loads: ") + Error, LoadBrixDay(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/day.json"))), Day, Error))) return false;
	TArray<FBrixLook> Looks, Stock;
	for (int32 i = 0; i < Day.Num(); ++i)
	{
		const FString RoleName = Day.Roles.IsValidIndex(Day.Role[i]) ? Day.Roles[Day.Role[i]] : FString();
		Looks.Add(BrixLookOf(Rules, i, Day.Wear.IsValidIndex(i) && !Day.Wear[i].IsEmpty() ? Day.Wear[i] : RoleName, TEXT("Easy")));
	}
	Stock = Looks;
	int32 Cast = 0;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/cast_looks.json")));
	if (!TestTrue(TEXT("cast_looks.json applies: ") + Error, BrixApplyCastLooks(Path, Rules, Day.Ids, Looks, Cast, Error))) return false;
	TestEqual(TEXT("one resident has a look of their own"), Cast, 1);
	const int32 N = Day.Ids.IndexOfByKey(FString(TEXT("r0262")));
	if (!TestTrue(TEXT("Noor is r0262"), N != INDEX_NONE && Day.Names.IsValidIndex(N) && Day.Names[N] == TEXT("Noor"))) return false;
	const FBrixLook& L = Looks[N];
	const FVector3f* Olive = Rules.Palette.Find(TEXT("olive"));
	const FVector3f* Cream = Rules.Palette.Find(TEXT("cream"));
	TestTrue(TEXT("an olive dress to the shin, bare legs"), L.Garment == TEXT("dress") && L.Legs == TEXT("bare") && Olive && L.Top.Equals(*Olive) && FMath::IsNearlyEqual(L.DressLen, .40));
	TestTrue(TEXT("a cream apron, no cap"), L.Apron.IsSet() && Cream && L.Apron->Equals(*Cream) && !L.Cap.IsSet());
	TestTrue(TEXT("grey hair in a bun"), L.Hair == TEXT("bun") && L.HairC.Equals(FVector3f(.27f, .26f, .27f)));
	TestTrue(TEXT("1.62 m, sturdy, 58"), FMath::IsNearlyEqual(L.S, 1.62 / 1.72) && L.W > 1.1 && FMath::IsNearlyEqual(L.Age, .45));
	for (int32 i = 0; i < Looks.Num(); ++i)
		if (i != N && (Looks[i].Age != 0 || Looks[i].DressLen != 0 || Looks[i].Garment != Stock[i].Garment)) { AddError(FString::Printf(TEXT("%s's look changed"), *Day.Ids[i])); break; }
	// drawn standing at full detail: the dress reaches to the shin (the lowest point of its colour about 0.4 m up), the legs
	// below it are bare, and age adds the creases and the hairline locks to the stock look's body
	FBrixDrawOptions O;
	O.Lod = 0;
	FBrixBodyMesh Mine, Young;
	DrawBrixPerson(Rules, L, FVector::ZeroVector, 0, O, Mine);
	FBrixLook Y = L;
	Y.Age = 0;
	DrawBrixPerson(Rules, Y, FVector::ZeroVector, 0, O, Young);
	double Hem = 1e9, SkinLow = 1e9;
	for (int32 v = 0; v < Mine.Pos.Num(); ++v)
	{
		if (Mine.Colour[v].Equals(L.Top)) Hem = FMath::Min(Hem, (double)Mine.Pos[v].Y);
		if (Mine.Colour[v].Equals(L.Skin)) SkinLow = FMath::Min(SkinLow, (double)Mine.Pos[v].Y);
	}
	AddInfo(FString::Printf(TEXT("Noor: the dress's hem %.2f m up, skin from %.2f m; %d vertices, %d at age 0"), Hem, SkinLow, Mine.Pos.Num(), Young.Pos.Num()));
	TestTrue(TEXT("the dress's hem is at the shin (0.3 to 0.5 m up)"), Hem > .3 && Hem < .5);
	TestTrue(TEXT("bare legs under it"), SkinLow < .15);
	TestTrue(TEXT("age adds creases and locks"), Mine.Pos.Num() > Young.Pos.Num() + 100);
	return true;
}

// Prototype/play/people.js, run by Tools/unreal/people_values.py, gives the looks of a set of residents and the bodies it
// draws for them walking and standing at every distance (Tests/values/people_city_01.json). The C++ port gives the same
// look (sizes, colours, clothes, hair, walk) and the same body: as many vertices, and every sampled vertex within 1 mm of
// where people.js puts it (the slice stores them as 32-bit floats); the far figures' boxes the same.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FART004_PeopleAreTheSlices, "BrixTown.World.ART004_people_are_drawn_as_the_slice_draws_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FART004_PeopleAreTheSlices::RunTest(const FString&)
{
	FBrixPeopleRules Rules;
	FString Error, Text;
	const FString Path = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/people.json")));
	if (!TestTrue(TEXT("the people's tables load: ") + Error, LoadBrixPeopleRules(Path, Rules, Error))) return false;
	BrixAddClimbSignature(Rules);   // children up a ladder (dressing.js climbSig(), which people_values.py cuts from dressing.js)
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/people_city_01.json")));
	if (!TestTrue(TEXT("the test values are there: ") + Values, FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;

	TArray<FBrixLook> Looks;
	int32 Wrong = 0;
	auto Near = [](const FVector3f& A, const FVector3f& B) { return A.Equals(B, 1e-4f); };
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("looks")))
	{
		const TSharedPtr<FJsonObject> W = V->AsObject();
		const int32 Id = (int32)W->GetNumberField(TEXT("i"));
		const FBrixLook L = BrixLookOf(Rules, Id, W->GetStringField(TEXT("role")), W->GetStringField(TEXT("sigName")));
		Looks.Add(L);
		const FString Who = FString::Printf(TEXT("%d (%s)"), Id, *W->GetStringField(TEXT("role")));
		auto Opt = [&](const TCHAR* Key, const TOptional<FVector3f>& Got) { const FVector3f E = Col(W, Key); return E.X < 0 ? !Got.IsSet() : Got.IsSet() && Near(*Got, E); };
		bool bBag = false, bTash = false;
		W->TryGetBoolField(TEXT("bag"), bBag);
		W->TryGetBoolField(TEXT("tash"), bTash);
		const bool bSame = FMath::IsNearlyEqual(L.S, W->GetNumberField(TEXT("S")), 1e-9) && FMath::IsNearlyEqual(L.W, W->GetNumberField(TEXT("W")), 1e-9)
			&& Near(L.Skin, Col(W, TEXT("skin"))) && Near(L.HairC, Col(W, TEXT("hairC"))) && Near(L.Eyes, Col(W, TEXT("eyes")))
			&& Near(L.Top, Col(W, TEXT("top"))) && Near(L.Bottom, Col(W, TEXT("bottom"))) && Near(L.Shoes, Col(W, TEXT("shoes")))
			&& Opt(TEXT("apron"), L.Apron) && Opt(TEXT("cap"), L.Cap) && Opt(TEXT("scarf"), L.Scarf)
			&& L.Hair == W->GetStringField(TEXT("hair")) && L.Garment == W->GetStringField(TEXT("g")) && L.Legs == W->GetStringField(TEXT("legs"))
			&& L.bBag == bBag && L.bTash == bTash && L.SigName == W->GetStringField(TEXT("sig")) && L.Seed == (int32)W->GetNumberField(TEXT("seed"));
		if (!bSame) { AddError(TEXT("resident ") + Who + TEXT(": not the look people.js gives them")); ++Wrong; }
	}

	ResetBrixWalkHeights();   // drawn in the order people_values.py drew them
	int32 Bodies = 0;
	FBrixBodyMesh Mesh;
	TArray<FBrixFarBox> Boxes;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("bodies")))
	{
		const TSharedPtr<FJsonObject> B = V->AsObject();
		const int32 k = (int32)B->GetNumberField(TEXT("look"));
		FBrixDrawOptions O;
		O.Pose = (int32)B->GetNumberField(TEXT("pose"));
		O.Phase = B->GetNumberField(TEXT("ph"));
		O.Seconds = B->GetNumberField(TEXT("t"));
		O.Lod = (int32)B->GetNumberField(TEXT("lod"));
		O.FaceFade = B->GetNumberField(TEXT("ff"));
		double Floor = 0;
		if (B->TryGetNumberField(TEXT("floor"), Floor)) O.Floor = Floor;
		B->TryGetBoolField(TEXT("driver"), O.bDriving);
		B->TryGetBoolField(TEXT("straddle"), O.bStraddle);   // astride a seesaw (dressing.js)
		B->TryGetBoolField(TEXT("sleep"), O.bSleep);         // asleep in bed: the eyes shut
		// a gesture, another walk, a hood or an umbrella (people.js o.mod, o.sig, o.hood, o.umb: dodge.js, smash.js, wet.js)
		B->TryGetStringField(TEXT("sig"), O.Sig);
		const TArray<TSharedPtr<FJsonValue>>* Arr = nullptr;
		if (B->TryGetArrayField(TEXT("arms"), Arr))
			for (int32 a = 0; a < 2 && a < Arr->Num(); ++a)
			{
				const TArray<TSharedPtr<FJsonValue>>* G = nullptr;
				if ((*Arr)[a]->TryGetArray(G) && G->Num() >= 4) O.Arms[a] = FVector4d((*G)[0]->AsNumber(), (*G)[1]->AsNumber(), (*G)[2]->AsNumber(), (*G)[3]->AsNumber());
			}
		if (B->TryGetArrayField(TEXT("chest"), Arr) && Arr->Num() >= 2) O.Chest = FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber());
		if (B->TryGetArrayField(TEXT("look_at"), Arr) && Arr->Num() >= 2) O.Look = FVector2D((*Arr)[0]->AsNumber(), (*Arr)[1]->AsNumber());   // the head and eyes turned (ANIM-005)
		const FVector3f HoodC = Col(B, TEXT("hood")), UmbC = Col(B, TEXT("umb"));
		if (HoodC.X >= 0) O.Hood = HoodC;
		if (UmbC.X >= 0) O.Umbrella = UmbC;
		FBrixFace Face;
		const TSharedPtr<FJsonObject>* FaceObj = nullptr;
		if (B->TryGetObjectField(TEXT("face"), FaceObj))   // a talking face (CHR-007)
		{
			const TSharedPtr<FJsonObject>& Fo = *FaceObj;
			const TSharedPtr<FJsonObject> Mo = Fo->GetObjectField(TEXT("mouth"));
			Face.Mouth.Width = Mo->GetNumberField(TEXT("width")); Face.Mouth.Opening = Mo->GetNumberField(TEXT("opening"));
			Face.Mouth.Round = Mo->GetNumberField(TEXT("round")); Face.Mouth.Teeth = (int32)Mo->GetNumberField(TEXT("teeth"));
			Mo->TryGetBoolField(TEXT("tongue"), Face.Mouth.bTongue);
			Face.Curve = Fo->GetNumberField(TEXT("curve")); Face.Lid = Fo->GetNumberField(TEXT("lid"));
			Face.Squint = Fo->GetNumberField(TEXT("squint")); Face.Open = Fo->GetNumberField(TEXT("open")); Face.Nod = Fo->GetNumberField(TEXT("nod"));
			auto Pair = [&](const TCHAR* K, double Out[2]) { const TArray<TSharedPtr<FJsonValue>>& A = Fo->GetArrayField(K); Out[0] = A[0]->AsNumber(); Out[1] = A[1]->AsNumber(); };
			Pair(TEXT("brow_l"), Face.BrowL); Pair(TEXT("brow_r"), Face.BrowR); Pair(TEXT("gaze"), Face.Gaze);
			O.Face = &Face;
		}
		Mesh.Reset();
		Boxes.Reset();
		DrawBrixPerson(Rules, Looks[k], FVector(B->GetNumberField(TEXT("x")), B->GetNumberField(TEXT("y")), B->GetNumberField(TEXT("z"))), B->GetNumberField(TEXT("yaw")), O, Mesh, &Boxes);
		const FString What = FString::Printf(TEXT("look %d, pose %d, lod %d%s%s%s%s%s%s%s"), k, O.Pose, O.Lod, O.Arms[0] || O.Arms[1] ? TEXT(", a gesture") : TEXT(""),
			O.Sig.IsEmpty() ? TEXT("") : *(TEXT(", walking ") + O.Sig), O.Hood ? TEXT(", a hood") : TEXT(""), O.Umbrella ? TEXT(", an umbrella") : TEXT(""),
			O.Look.IsZero() ? TEXT("") : TEXT(", looking aside"), O.bSleep ? TEXT(", asleep") : TEXT(""), O.bStraddle ? TEXT(", astride") : TEXT(""));
		++Bodies;
		if (Mesh.Index.Num() != (int32)B->GetNumberField(TEXT("vertices")))
		{
			AddError(FString::Printf(TEXT("%s: %d vertices, people.js draws %d"), *What, Mesh.Index.Num(), (int32)B->GetNumberField(TEXT("vertices"))));
			++Wrong;
			continue;
		}
		double Worst = 0;
		for (const TSharedPtr<FJsonValue>& SV : B->GetArrayField(TEXT("samples")))
		{
			const TArray<TSharedPtr<FJsonValue>>& S = SV->AsArray();
			const FVector3f E((float)S[1]->AsNumber(), (float)S[2]->AsNumber(), (float)S[3]->AsNumber());
			Worst = FMath::Max(Worst, (double)FVector3f::Distance(Mesh.Pos[Mesh.Index[(int32)S[0]->AsNumber()]], E));   // the k-th vertex of people.js's triangle list
		}
		if (Worst > 1e-3) { AddError(FString::Printf(TEXT("%s: a vertex %.4f m from where people.js puts it"), *What, Worst)); ++Wrong; }
		const TArray<TSharedPtr<FJsonValue>>& WB = B->GetArrayField(TEXT("boxes"));
		bool bBoxes = WB.Num() == Boxes.Num();
		for (int32 i = 0; bBoxes && i < WB.Num(); ++i)
		{
			const TArray<TSharedPtr<FJsonValue>>& X = WB[i]->AsArray();
			auto F = [&](int32 j) { return (float)X[j]->AsNumber(); };
			bBoxes = Boxes[i].Min.Equals(FVector3f(F(0), F(1), F(2)), 1e-4f) && Boxes[i].Max.Equals(FVector3f(F(3), F(4), F(5)), 1e-4f) && Near(Boxes[i].Colour, FVector3f(F(6), F(7), F(8)));
		}
		if (!bBoxes) { AddError(What + TEXT(": not the far figure people.js draws")); ++Wrong; }
	}
	TestTrue(TEXT("looks and bodies were checked"), Looks.Num() > 10 && Bodies > 40);
	return Wrong == 0;
}


// CITY-017: residents use a garden's benches. The day's stays at a garden's bench seats and fountain rims are found as the
// slice finds them (play.js resident(), openLot()) and sat on: each such stay is inside its garden (so the lots are read in
// the slice's order), on a seat a bench's height up, facing along a street's axes towards the garden's middle (a bench) or
// out of the fountain (a rim); and some of the day's stays are seated.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY017_ResidentsSitOnGardenBenches, "BrixTown.World.CITY017_residents_sit_on_garden_benches",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY017_ResidentsSitOnGardenBenches::RunTest(const FString&)
{
	FBrixCity City;
	FBrixDay Day;
	FString Error;
	const FString CityDir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/city_01")));
	if (!TestTrue(TEXT("the city loads: ") + Error, LoadBrixCity(CityDir, City, Error))) return false;
	if (!TestTrue(TEXT("the day loads: ") + Error, LoadBrixDay(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/day.json"))), Day, Error))) return false;
	const int32 Seated = SeatBrixDay(Day, City);
	TArray<const FBrixLot*> Lots;
	for (const FBrixBlock& B : City.Blocks) for (const FBrixLot& L : B.Lots) Lots.Add(&L);
	int32 Wrong = 0, Checked = 0;
	for (int32 i = 0; i < Day.Num(); ++i)
		for (const FBrixDay::FPiece& P : Day.Plan[i])
		{
			if (P.Kind != 0 || P.Pose != 2) continue;
			++Checked;
			const FBrixLot* L = Lots.IsValidIndex(P.Lot) ? Lots[P.Lot] : nullptr;
			const bool bIn = L && L->Kind == TEXT("garden") && P.Pos.X >= L->Min.X - .5 && P.Pos.X <= L->Max.X + .5 && P.Pos.Y >= L->Min.Y - .5 && P.Pos.Y <= L->Max.Y + .5;
			const bool bUp = P.SeatUp > .5f && P.SeatUp < .7f;
			const FVector2D ToMid = L ? L->Centre() - P.Pos : FVector2D::ZeroVector;
			const bool bFace = FMath::IsNearlyEqual(P.Facing.Size(), 1.0, 1e-3) && (FVector2D::DotProduct(P.Facing, ToMid) > 0 || FVector2D::DotProduct(P.Facing, -ToMid) > .99 * ToMid.Size());
			if (!bIn || !bUp || !bFace)
			{
				if (Wrong < 10) AddError(FString::Printf(TEXT("%s: a seat at (%.1f, %.1f) %s"), *Day.Ids[i], P.Pos.X, P.Pos.Y, !bIn ? TEXT("not in its garden") : !bUp ? TEXT("not a seat's height up") : TEXT("facing neither the middle nor out of the rim")));
				++Wrong;
			}
		}
	TestEqual(TEXT("every seat counted once"), Checked, Seated);
	TestTrue(TEXT("some of the day's stays are on a garden seat"), Seated > 20);
	return Wrong == 0;
}

// ANIM-005: someone near turns to look at the player, the eyes leading and the head part of the way (play.js): straight
// ahead of them they look back at once, more the nearer the player (fully within 3.2 m, not past 5.5 m), never past 0.85
// radians or at someone well behind them; and the player's head turns part of the way to where the camera looks.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FANIM005_PeopleLookAtThePlayer, "BrixTown.World.ANIM005_people_look_at_whoever_comes_near",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FANIM005_PeopleLookAtThePlayer::RunTest(const FString&)
{
	// facing north (yaw 0, -z): the player 2 m away to the north-east is to their right, so the head turns right (yaw -)
	const FVector2D Right = BrixGlanceAt(1.4142, -1.4142, 0), Left = BrixGlanceAt(-1.4142, -1.4142, 0);
	TestTrue(TEXT("the head turns right to someone on the right"), Right.X < 0 && FMath::IsNearlyEqual(Right.X, -UE_DOUBLE_PI / 4 * .6, 1e-9));
	TestTrue(TEXT("and left to someone on the left"), FMath::IsNearlyEqual(Left.X, -Right.X, 1e-12));
	TestEqual(TEXT("straight ahead the head stays"), BrixGlanceAt(0, -2, 0).X, 0.0);
	TestEqual(TEXT("not past 5.5 m"), BrixGlanceAt(5.6, 0, 1.5708).Size(), 0.0);
	TestTrue(TEXT("less at 4.5 m than at 3 m"), FMath::Abs(BrixGlanceAt(3.18, -3.18, 0).X) < FMath::Abs(BrixGlanceAt(2.12, -2.12, 0).X));
	TestEqual(TEXT("not at someone behind"), BrixGlanceAt(0, 2, 0).Size(), 0.0);
	TestTrue(TEXT("never past 0.85 radians"), FMath::IsNearlyEqual(FMath::Abs(BrixGlanceAt(2, 0.3, 0).X), .85, 1e-9));
	const FVector2D Cam = BrixLookAlong(.5, -.3);   // the camera a little right and looking down
	TestTrue(TEXT("the player's head turns towards where the camera looks"), FMath::IsNearlyEqual(Cam.X, -.35, 1e-9) && FMath::IsNearlyEqual(Cam.Y, .15, 1e-9));
	TestTrue(TEXT("part of the way at most"), FMath::IsNearlyEqual(BrixLookAlong(3, 0).X, -.7, 1e-9) && FMath::IsNearlyEqual(BrixLookAlong(0, 2).Y, -.25, 1e-9));
	return true;
}

#endif
