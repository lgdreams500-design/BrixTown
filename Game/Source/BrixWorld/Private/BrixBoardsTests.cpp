// CITY-098 to CITY-101: the skateboards of the Unreal build are the slice's. World/rules/boards.json holds the kinds; the
// slice's own boards.js (step, give, toggle, use, slow) and people.js (board(), ride(), draw() with a board), run in headless
// Edge by Tools/unreal/people_values.py, give Tests/values/boards_city_01.json, and the C++ ports must give the same.
#include "BrixBoards.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString BoardsPath() { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/boards.json"))); }
	TSharedPtr<FJsonObject> ReadValues(FAutomationTestBase& T, const TCHAR* Rel)
	{
		FString Text;
		TSharedPtr<FJsonObject> J;
		const FString P = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Rel));
		if (!T.TestTrue(TEXT("the test values are there: ") + P, FFileHelper::LoadFileToString(Text, *P))) return nullptr;
		if (!T.TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), J) && J.IsValid())) return nullptr;
		return J;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY098_BoardKinds, "BrixTown.World.CITY098_board_kinds_and_speeds",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY098_BoardKinds::RunTest(const FString&)
{
	FBrixBoardRules R;
	FString Error;
	if (!TestTrue(TEXT("boards.json loads: ") + Error, LoadBrixBoards(BoardsPath(), R, Error))) return false;
	TestEqual(TEXT("three kinds, up the store"), FString::Join(R.Order, TEXT(",")), FString(TEXT("manual,electric,neon_boost")));
	TestEqual(TEXT("the manual board's top speed (the owner's)"), R.Kind(TEXT("manual"))->TopKmh, 50.0);
	TestEqual(TEXT("the electric board's"), R.Kind(TEXT("electric"))->TopKmh, 100.0);
	TestEqual(TEXT("the Neon Boost's"), R.Kind(TEXT("neon_boost"))->TopKmh, 250.0);
	for (int32 k = 1; k < R.Order.Num(); ++k) TestTrue(TEXT("each kind up the store is faster than the one before"), R.Kind(R.Order[k])->TopKmh > R.Kind(R.Order[k - 1])->TopKmh);
	for (const FString& K : R.Order)
	{
		const FBrixBoardKind* B = R.Kind(K);
		TestTrue(K + TEXT(": only the manual board is pushed by a foot, the others are powered"), B->bPush == (K == TEXT("manual")) && B->bPowered == !B->bPush);
		TestTrue(K + TEXT(": it gets to its speed, glides and brakes"), B->AccelKmhS > 0 && B->GlideKmhS > 0 && B->BrakeKmhS > B->GlideKmhS);
		TestTrue(K + TEXT(": its size (look test 35)"), B->DeckM > .5 && B->WidthM > .15 && B->TopM > B->WheelM && B->WheelbaseM < B->DeckM);
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY100_AutoOwnsAll, "BrixTown.World.CITY100_auto_owns_all_and_rides_electric",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY100_AutoOwnsAll::RunTest(const FString&)
{
	FBrixBoardRules R;
	FString Error;
	if (!TestTrue(TEXT("boards.json loads: ") + Error, LoadBrixBoards(BoardsPath(), R, Error))) return false;
	const TSharedPtr<FJsonObject> J = ReadValues(*this, TEXT("../Tests/values/boards_city_01.json"));
	if (!J) return false;
	FBrixBoards B;
	B.R = &R;
	B.Give({}, FString());
	TestFalse(TEXT("a new game has no board (until the skate store)"), B.Kind() != nullptr || B.Toggle().IsSet());
	TestTrue(TEXT("and boards.js gives none either"), J->GetObjectField(TEXT("none"))->HasTypedField<EJson::Null>(TEXT("kind")));
	B.Give(R.AutoOwned, R.AutoEquipped);
	const TSharedPtr<FJsonObject> A = J->GetObjectField(TEXT("auto"));
	TArray<FString> Want;
	for (const TSharedPtr<FJsonValue>& V : A->GetArrayField(TEXT("owned"))) Want.Add(V->AsString());
	TestEqual(TEXT("a test owns every kind, as boards.js give()"), FString::Join(B.Own, TEXT(",")), FString::Join(Want, TEXT(",")));
	TestEqual(TEXT("with the electric board in use"), B.InUse, A->GetStringField(TEXT("kind")));
	TestFalse(TEXT("in the hand, not ridden"), B.Riding());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY099_BoardsAsTheSlice, "BrixTown.World.CITY099_boards_are_ridden_and_drawn_as_the_slice_does",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY099_BoardsAsTheSlice::RunTest(const FString&)
{
	FBrixBoardRules R;
	FString Error;
	if (!TestTrue(TEXT("boards.json loads: ") + Error, LoadBrixBoards(BoardsPath(), R, Error))) return false;
	const TSharedPtr<FJsonObject> J = ReadValues(*this, TEXT("../Tests/values/boards_city_01.json"));
	const TSharedPtr<FJsonObject> P = ReadValues(*this, TEXT("../Tests/values/people_city_01.json"));
	if (!J || !P) return false;
	// the speeds through a ride of each kind: W, let go, S, indoors, in a mall, a knock
	int32 Steps = 0;
	for (const TSharedPtr<FJsonValue>& RV : J->GetArrayField(TEXT("rides")))
	{
		const TSharedPtr<FJsonObject> Ride = RV->AsObject();
		const FString K = Ride->GetStringField(TEXT("kind"));
		FBrixBoards B;
		B.R = &R;
		B.Give(R.AutoOwned, K);
		B.Toggle();
		double Worst = 0;
		for (const TSharedPtr<FJsonValue>& SV : Ride->GetArrayField(TEXT("steps")))
		{
			const TArray<TSharedPtr<FJsonValue>>& S = SV->AsArray();
			const double Cap = S[3]->AsNumber();
			if (Cap == -2) B.Slow(.4);
			else B.Step(S[0]->AsNumber(), S[1]->AsNumber() > .5, S[2]->AsNumber() > .5, Cap);
			Worst = FMath::Max(Worst, FMath::Abs(B.V - S[4]->AsNumber()));
			++Steps;
		}
		TestTrue(FString::Printf(TEXT("%s: every speed as boards.js gives it (worst %.2g m/s)"), *K, Worst), Worst < 1e-9);
	}
	TestTrue(TEXT("rides were checked"), Steps > 200);
	{   // another kind made the one in use keeps no more than its top speed; Y picks the board up and stops
		FBrixBoards B;
		B.R = &R;
		B.Give(R.AutoOwned, TEXT("neon_boost"));
		B.Toggle();
		for (int32 i = 0; i < 200; ++i) B.Step(1 / 20.0, true, false);
		const TArray<TSharedPtr<FJsonValue>>& U = J->GetArrayField(TEXT("use"));
		TestTrue(TEXT("the Neon Boost's top speed"), FMath::IsNearlyEqual(B.V, U[0]->AsNumber(), 1e-9));
		B.Use(TEXT("manual"));
		TestTrue(TEXT("made the manual board, no faster than it goes"), FMath::IsNearlyEqual(B.V, U[1]->AsNumber(), 1e-9) && B.InUse == U[2]->AsString());
		const TOptional<bool> On = B.Toggle();
		TestTrue(TEXT("Y again: in the hand"), On.IsSet() && *On == J->GetArrayField(TEXT("toggled"))[0]->AsBool() && B.V == 0);
	}
	// the riders and the boards carried, drawn as people.js draws them (after its other bodies, as people_values.py drew them)
	FBrixPeopleRules Rules;
	if (!TestTrue(TEXT("the people's tables load: ") + Error, LoadBrixPeopleRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/people.json"))), Rules, Error))) return false;
	TArray<FBrixLook> Looks;
	for (const TSharedPtr<FJsonValue>& V : P->GetArrayField(TEXT("looks")))
	{
		const TSharedPtr<FJsonObject> W = V->AsObject();
		Looks.Add(BrixLookOf(Rules, (int32)W->GetNumberField(TEXT("i")), W->GetStringField(TEXT("role")), W->GetStringField(TEXT("sigName"))));
	}
	ResetBrixWalkHeights();
	FBrixBodyMesh Mesh;
	TArray<FBrixFarBox> Boxes;
	for (const TSharedPtr<FJsonValue>& V : P->GetArrayField(TEXT("bodies")))   // the walks' hip heights are kept as people.js kept them
	{
		const TSharedPtr<FJsonObject> B = V->AsObject();
		if ((int32)B->GetNumberField(TEXT("pose")) != 1) continue;
		FBrixDrawOptions O;
		O.Pose = 1;
		O.Lod = (int32)B->GetNumberField(TEXT("lod"));
		B->TryGetStringField(TEXT("sig"), O.Sig);
		Mesh.Reset();
		Boxes.Reset();
		DrawBrixPerson(Rules, Looks[(int32)B->GetNumberField(TEXT("look"))], FVector::ZeroVector, 0, O, Mesh, &Boxes);
	}
	int32 Bodies = 0, Wrong = 0;
	for (const TSharedPtr<FJsonValue>& V : J->GetArrayField(TEXT("bodies")))
	{
		const TSharedPtr<FJsonObject> B = V->AsObject();
		FBrixDrawOptions O;
		O.Pose = (int32)B->GetNumberField(TEXT("pose"));
		O.Phase = B->GetNumberField(TEXT("ph"));
		O.Seconds = B->GetNumberField(TEXT("t"));
		O.Lod = (int32)B->GetNumberField(TEXT("lod"));
		O.FaceFade = B->GetNumberField(TEXT("ff"));
		O.Board = R.Kind(B->GetStringField(TEXT("board")));
		O.bCarry = B->GetBoolField(TEXT("carry"));
		O.RidePush = B->GetNumberField(TEXT("push"));
		const int32 k = (int32)B->GetNumberField(TEXT("look"));
		Mesh.Reset();
		DrawBrixPerson(Rules, Looks[k], FVector(B->GetNumberField(TEXT("x")), B->GetNumberField(TEXT("y")), B->GetNumberField(TEXT("z"))), B->GetNumberField(TEXT("yaw")), O, Mesh);
		const FString What = FString::Printf(TEXT("look %d, %s %s, pose %d, lod %d, push %.2f"), k, *B->GetStringField(TEXT("board")), O.bCarry ? TEXT("carried") : TEXT("ridden"), O.Pose, O.Lod, O.RidePush);
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
			Worst = FMath::Max(Worst, (double)FVector3f::Distance(Mesh.Pos[Mesh.Index[(int32)S[0]->AsNumber()]], FVector3f((float)S[1]->AsNumber(), (float)S[2]->AsNumber(), (float)S[3]->AsNumber())));
		}
		if (Worst > 1e-3) { AddError(FString::Printf(TEXT("%s: a vertex %.4f m from where people.js puts it"), *What, Worst)); ++Wrong; }
	}
	AddInfo(FString::Printf(TEXT("%d riders and boards carried, %d wrong"), Bodies, Wrong));
	return TestTrue(TEXT("every kind carried and ridden was checked"), Bodies >= 36 && Wrong == 0);
}

#endif
