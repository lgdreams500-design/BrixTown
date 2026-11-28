// CITY-108 to CITY-112: the glass travel tubes (Tools/citysim/test_tubes.py), on the town's own network.
#include "BrixTubes.h"

#include "Misc/AutomationTest.h"
#include "Misc/Paths.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	const double FPS = 60;

	bool LoadTown(FAutomationTestBase& Test, FBrixTubeNet& Net, FBrixTubeRules& R)
	{
		const FString World = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World")));
		FString E1, E2;
		const bool bNet = LoadBrixTubeNet(FPaths::Combine(World, TEXT("city_01/tubes.json")), Net, E1);
		const bool bRules = LoadBrixTubeRules(FPaths::Combine(World, TEXT("rules/tubes.json")), R, E2);
		return Test.TestTrue(TEXT("tubes.json loads: ") + E1, bNet) && Test.TestTrue(TEXT("rules/tubes.json loads: ") + E2, bRules);
	}

	TArray<double> Frames(const FBrixRide& W, const FBrixTubeRules& R)
	{
		TArray<double> F;
		const int32 N = (int32)((W.Duration + R.FollowS + 1.0) * FPS);
		for (int32 i = 0; i <= N; ++i) F.Add(i / FPS);
		return F;
	}

	// a few rides across the town: the first stop to each other, and the last to the first
	TArray<FBrixRide> SomeRides(const FBrixTubeNet& Net, const FBrixTubeRules& R)
	{
		TArray<FBrixRide> Out;
		for (int32 i = 1; i < Net.StopOrder.Num(); ++i) Out.Add(BrixMakeRide(Net, R, Net.StopOrder[0], Net.StopOrder[i]));
		Out.Add(BrixMakeRide(Net, R, Net.StopOrder.Last(), Net.StopOrder[0]));
		return Out;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY108_Net, "BrixTown.World.CITY108_the_towns_tubes_are_sound_and_reach_every_stop",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY108_Net::RunTest(const FString&)
{
	FBrixTubeNet Net;
	FBrixTubeRules R;
	if (!LoadTown(*this, Net, R)) return false;
	TestTrue(TEXT("the town has stops"), Net.StopOrder.Num() >= 2);
	const TArray<FString> P = BrixTubeProblems(Net, R);
	TestEqual(TEXT("no problems: ") + FString::Join(P, TEXT("; ")), P.Num(), 0);
	for (const FString& A : Net.StopOrder)                  // CITY-112: from any stop to any other, never through a stop
		for (const FString& B : Net.StopOrder)
		{
			if (A == B) continue;
			const TArray<FString> Way = BrixTubeRoute(Net, A, B);
			if (!TestTrue(FString::Printf(TEXT("%s reaches %s"), *A, *B), Way.Num() >= 2)) continue;
			for (int32 i = 1; i + 1 < Way.Num(); ++i) TestFalse(TEXT("passes a stop: ") + Way[i], Net.Stops.Contains(Way[i]));
		}
	FBrixTubeNet Low = Net;                                 // a tube too low is caught
	Low.Tubes[0].Line[Low.Tubes[0].Line.Num() / 2].Z = R.ClearanceM - 1;
	TestTrue(TEXT("a tube too low is caught"), BrixTubeProblems(Low, R).Num() > 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY109_Ride, "BrixTown.World.CITY109_a_ride_goes_floor_tube_floor_without_a_jump",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY109_Ride::RunTest(const FString&)
{
	FBrixTubeNet Net;
	FBrixTubeRules R;
	if (!LoadTown(*this, Net, R)) return false;
	TestTrue(TEXT("the tube fits a rider"), R.TubeInnerM >= 0.63 && R.TubeInnerM <= 0.8);
	for (const FBrixRide& W : SomeRides(Net, R))
	{
		if (!TestTrue(TEXT("a ride"), W.IsValid())) continue;
		const FBrixTubeStop& B = Net.Stops[W.Nodes.Last()];
		TestTrue(TEXT("starts on the floor"), W.At(0).Equals(W.FloorA));
		TestTrue(TEXT("drawn in first"), W.Phase(0.1) == EBrixRidePhase::DrawIn);
		TestTrue(TEXT("up to the mouth"), FMath::IsNearlyEqual(W.At(R.DrawInS).Z, R.DrawInHeightM, 1e-6));
		const FVector End = W.At(W.Duration);
		TestTrue(TEXT("set down on the far booth's floor"), FMath::IsNearlyEqual(End.Z, R.FloorM));
		TestTrue(TEXT("towards the door"), FMath::IsNearlyEqual(FVector2D::Distance(FVector2D(End), B.Pos), R.PopOutArcM, 1e-6));
		const TArray<double> F = Frames(W, R);
		double Level = 0;
		for (int32 i = 1; i < F.Num(); ++i)
		{
			const double Step = FVector::Dist(W.At(F[i - 1]), W.At(F[i]));
			if (Step > R.SpeedMps / FPS + 1e-6) { AddError(FString::Printf(TEXT("%s: a jump of %.2f m at %.2f s"), *W.Nodes.Last(), Step, F[i])); break; }
			if (FMath::Abs(W.Lean(F[i]) - W.Lean(F[i - 1])) >= 12) { AddError(TEXT("flipped, not turned")); break; }
			const FVector P = W.At(F[i] - 0.1), Q = W.At(F[i] + 0.1);
			if (W.Phase(F[i]) == EBrixRidePhase::Run && FMath::Abs(Q.Z - P.Z) < 0.01 && FVector::Dist(P, Q) > 1) Level = FMath::Max(Level, W.Lean(F[i]));
		}
		TestTrue(TEXT("lying along the level tube"), Level > 85);
		auto Speed = [&W](double S) { return FVector::Dist(W.At(S - .5 / FPS), W.At(S + .5 / FPS)) * FPS; };
		for (const double T : { W.T0, W.T1 })                  // on into the tube at the speed they came up at, and out of it the same
			TestTrue(FString::Printf(TEXT("%s: no stop at %.2f s"), *W.Nodes.Last(), T), FMath::Abs(Speed(T - 1 / FPS) - Speed(T + 1 / FPS)) <= .15 * Speed(T - 1 / FPS) + 1);
		TestTrue(TEXT("upright going up"), W.Lean(W.T0 * 0.5) < 5);
		TestEqual(TEXT("on their feet at the end"), W.Lean(W.Duration), 0.0);
		const double Mid = W.T0 + W.TRun * 0.37;                // worked out from time: asked twice, the same
		TestTrue(TEXT("the same every time"), W.At(Mid).Equals(W.At(Mid), 0) && W.At(W.T0 + 0.5).Equals(BrixMakeRide(Net, R, W.Nodes[0], W.Nodes.Last()).At(W.T0 + 0.5), 0));
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY110_Camera, "BrixTown.World.CITY110_the_camera_follows_the_rider_without_a_cut",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY110_Camera::RunTest(const FString&)
{
	FBrixTubeNet Net;
	FBrixTubeRules R;
	if (!LoadTown(*this, Net, R)) return false;
	const double Far = R.SpeedMps * R.FollowS + R.BehindM + R.AboveM;
	for (const FBrixRide& W : SomeRides(Net, R))
	{
		if (!W.IsValid()) continue;
		const TArray<double> F = Frames(W, R);
		for (int32 i = 1; i < F.Num(); ++i)
		{
			if (FVector::Dist(W.Camera(F[i - 1]), W.Camera(F[i])) > R.MaxJumpM) { AddError(FString::Printf(TEXT("a cut at %.2f s"), F[i])); break; }
			if (FVector::Dist(W.Camera(F[i]), W.At(F[i])) >= Far) { AddError(TEXT("the rider left behind")); break; }
		}
		for (const double S : { 0.0, F.Last() })               // beside the booth at both ends, at the rider's height
		{
			const FVector C = W.Camera(S), P = W.At(S);
			TestTrue(TEXT("beside the booth"), FMath::IsNearlyEqual(FVector2D::Distance(FVector2D(C), FVector2D(P)), R.BehindM, .5));
			TestTrue(TEXT("over the rider"), FMath::IsNearlyEqual(C.Z, P.Z + R.AboveM, .5));
		}
		for (const double S : F)
			if (FVector::Dist(W.Camera(S), W.At(S)) <= R.AboveM)   // never nearer than it stands over them
			 { AddError(TEXT("the camera in the rider")); break; }
	}
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY111_Ask, "BrixTown.World.CITY111_the_server_checks_the_stop_and_the_way",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY111_Ask::RunTest(const FString&)
{
	FBrixTubeNet Net;
	FBrixTubeRules R;
	if (!LoadTown(*this, Net, R)) return false;
	const FBrixTubeStop& A = Net.Stops[Net.StopOrder[0]];
	const FString B = Net.StopOrder[1];
	FBrixRide W;
	TestEqual(TEXT("standing in a stop, a stop chosen: allowed"), BrixAskRide(Net, R, A.Pos + FVector2D(0.3, 0), false, B, W), FString());
	TestTrue(TEXT("and the ride is the server's own"), W.IsValid() && W.Nodes[0] == A.Id && W.Nodes.Last() == B);
	TestEqual(TEXT("out in the street: refused"), BrixAskRide(Net, R, A.Pos + FVector2D(R.BoothR + 0.5, 0), false, B, W), FString(TEXT("not_in_a_stop")));
	TestFalse(TEXT("refused: no ride"), W.IsValid());
	TestEqual(TEXT("already riding: refused"), BrixAskRide(Net, R, A.Pos, true, B, W), FString(TEXT("riding")));
	TestEqual(TEXT("a stop that is not there: refused"), BrixAskRide(Net, R, A.Pos, false, TEXT("nowhere"), W), FString(TEXT("unknown_stop")));
	TestEqual(TEXT("the stop you stand in: refused"), BrixAskRide(Net, R, A.Pos, false, A.Id, W), FString(TEXT("same_stop")));
	FBrixTubeNet Cut = Net;                                  // a stop no tube reaches
	FBrixTubeStop Lone;
	Lone.Id = TEXT("lone");
	Lone.Pos = FVector2D(-5000, -5000);
	Cut.Stops.Add(Lone.Id, Lone);
	Cut.StopOrder.Add(Lone.Id);
	TestEqual(TEXT("no way there: refused"), BrixAskRide(Cut, R, A.Pos, false, Lone.Id, W), FString(TEXT("no_way")));
	return true;
}

#endif
