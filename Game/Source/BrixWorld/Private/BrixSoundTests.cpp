// SND-003, SND-007, SND-009, SND-017: the town's sound decided as the slice's sound.js decides it, and its recordings.
#include "BrixSound.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	FString FromProject(const TCHAR* Rel) { return FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Rel)); }

	bool LoadRules(FAutomationTestBase& T, FBrixSoundRules& R)
	{
		FString Error;
		return T.TestTrue(TEXT("World/rules/sound.json loads: ") + Error, LoadBrixSoundRules(FromProject(TEXT("../World/rules/sound.json")), R, Error));
	}

	// Tests/values/sound_city_01.json: sound.js's own functions in scripted moments (Tools/unreal/sound_values.py)
	TSharedPtr<FJsonObject> LoadValues(FAutomationTestBase& T)
	{
		FString Text;
		TSharedPtr<FJsonObject> O;
		if (!T.TestTrue(TEXT("the test values are there"), FFileHelper::LoadFileToString(Text, *FromProject(TEXT("../Tests/values/sound_city_01.json"))))) return nullptr;
		if (!T.TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O) && O.IsValid())) return nullptr;
		return O;
	}

	FBrixHeardPerson Person(const TSharedPtr<FJsonObject>& O)
	{
		FBrixHeardPerson P;
		P.Id = (int32)O->GetNumberField(TEXT("i"));
		P.Pos = FVector2D(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("z")));
		P.bWalking = O->GetBoolField(TEXT("walking"));
		P.Pose = (int32)O->GetNumberField(TEXT("pose"));
		P.Phase = O->GetNumberField(TEXT("ph"));
		P.bInside = O->GetBoolField(TEXT("inside"));
		P.bRun = O->GetBoolField(TEXT("run"));
		FString Role;
		O->TryGetStringField(TEXT("role"), Role);
		P.bChild = Role == TEXT("child");
		O->TryGetStringField(TEXT("kind"), P.LotKind);   // null: on no lot
		O->TryGetStringField(TEXT("act"), P.Act);
		P.Role = Role;
		O->TryGetStringField(TEXT("shout"), P.Shout);    // null: no shout
		return P;
	}
}

// People's movements (SND-017, SND-003): residents walking near the listener, children, runners, one indoors, a crowd of
// eight in step, people sitting down, lying down and getting up near and far, run through sound.js people() (its audio
// swapped for a recorder) and through the port. Every frame must give the same steps (surface, loudness, pitch, reach,
// in the same order, at most six a frame) and the same pose sounds.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND017_PeopleSoundAsTheSlice, "BrixTown.World.SND017_people_sound_as_the_slice_hears_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND017_PeopleSoundAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	const TArray<TSharedPtr<FJsonValue>>& Frames = V->GetObjectField(TEXT("cases"))->GetArrayField(TEXT("people"));
	const TArray<TSharedPtr<FJsonValue>>& Want = V->GetObjectField(TEXT("trace"))->GetArrayField(TEXT("people"));
	if (!TestEqual(TEXT("a trace for every frame"), Want.Num(), Frames.Num())) return false;
	FBrixPeopleSounds S;
	int32 Steps = 0, Poses = 0, Wrong = 0, Child = 0, Grass = 0, Wood = 0, Run = 0;
	for (int32 f = 0; f < Frames.Num(); ++f)
	{
		const TSharedPtr<FJsonObject> F = Frames[f]->AsObject();
		TArray<FBrixHeardPerson> People;
		for (const TSharedPtr<FJsonValue>& P : F->GetArrayField(TEXT("residents"))) People.Add(Person(P->AsObject()));
		TArray<FBrixSoundEvent> Got;
		S.Frame(R, People, FVector2D::ZeroVector, F->GetNumberField(TEXT("t")), Got);
		const TArray<TSharedPtr<FJsonValue>>& W = Want[f]->AsArray();
		bool bSame = W.Num() == Got.Num();
		for (int32 k = 0; bSame && k < W.Num(); ++k)
		{
			const TArray<TSharedPtr<FJsonValue>>& E = W[k]->AsArray();
			const FBrixSoundEvent& G = Got[k];
			if (E[0]->AsString() == TEXT("step"))
			{
				const FString Surface = E[3]->AsString();
				const bool bChild = E[6]->AsNumber() != 1;
				bSame = G.Sound == TEXT("step_") + Surface + (bChild ? TEXT("_child") : TEXT("_adult")) && FMath::IsNearlyEqual(G.Pos.X, E[1]->AsNumber(), 1e-5)
					&& FMath::IsNearlyEqual(G.Pos.Y, E[2]->AsNumber(), 1e-5) && FMath::IsNearlyEqual(G.Gain, E[4]->AsNumber(), 1e-6)
					&& FMath::IsNearlyEqual(G.Far, E[5]->AsNumber(), 1e-9) && FMath::IsNearlyEqual(G.Pitch, E[6]->AsNumber(), 1e-6);
				++Steps; Child += bChild; Grass += Surface == TEXT("grass"); Wood += Surface == TEXT("wood"); Run += G.Gain > R.ResidentStepGain + 1e-9;
			}
			else
			{
				bSame = G.Sound == TEXT("person_") + E[1]->AsString() && FMath::IsNearlyEqual(G.Pos.X, E[2]->AsNumber(), 1e-5)
					&& FMath::IsNearlyEqual(G.Pos.Y, E[3]->AsNumber(), 1e-5) && FMath::IsNearlyEqual(G.Far, E[4]->AsNumber(), 1e-9);
				++Poses;
			}
		}
		if (!bSame && ++Wrong <= 5)
		{
			FString Mine;
			for (const FBrixSoundEvent& G : Got) Mine += FString::Printf(TEXT("[%s %.3f,%.3f g%.4f] "), *G.Sound, G.Pos.X, G.Pos.Y, G.Gain);
			AddError(FString::Printf(TEXT("frame %d: sound.js plays %d sounds, the port %d: %s"), f, W.Num(), Got.Num(), *Mine));
		}
	}
	AddInfo(FString::Printf(TEXT("%d frames: %d steps (%d a child's, %d on grass, %d on wood, %d running), %d pose sounds"), Frames.Num(), Steps, Child, Grass, Wood, Run, Poses));
	TestTrue(TEXT("the scripted moments have steps of every kind and pose sounds"), Steps > 50 && Child > 0 && Grass > 0 && Wood > 0 && Run > 0 && Poses >= 4);
	TestEqual(TEXT("frames that differ from sound.js"), Wrong, 0);
	return true;
}

// Where a sound is heard (SND-007): place() of sound.js at distances near and far, ahead and to the side, for the
// default reach and others; the same loudness and pan.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND007_HeardFromWhereItIs, "BrixTown.World.SND007_sounds_are_heard_from_where_they_are",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND007_HeardFromWhereItIs::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	const TArray<TSharedPtr<FJsonValue>>& Cases = V->GetObjectField(TEXT("cases"))->GetArrayField(TEXT("place"));
	const TArray<TSharedPtr<FJsonValue>>& Want = V->GetObjectField(TEXT("trace"))->GetArrayField(TEXT("place"));
	int32 Wrong = 0;
	for (int32 k = 0; k < Cases.Num(); ++k)
	{
		const TArray<TSharedPtr<FJsonValue>>& C = Cases[k]->AsArray();
		const double Far = C[2]->IsNull() ? 0 : C[2]->AsNumber();
		const FVector2D G = BrixHear(R, FVector2D::ZeroVector, C[3]->AsNumber(), FVector2D(C[0]->AsNumber(), C[1]->AsNumber()), Far);
		const TArray<TSharedPtr<FJsonValue>>& W = Want[k]->AsArray();
		if ((!FMath::IsNearlyEqual(G.X, W[0]->AsNumber(), 2e-6) || !FMath::IsNearlyEqual(G.Y, W[1]->AsNumber(), 2e-6)) && ++Wrong <= 5)
			AddError(FString::Printf(TEXT("(%g, %g) reach %g: sound.js %g pan %g, the port %g pan %g"), C[0]->AsNumber(), C[1]->AsNumber(), Far, W[0]->AsNumber(), W[1]->AsNumber(), G.X, G.Y));
	}
	TestEqual(TEXT("places heard otherwise than sound.js hears them"), Wrong, 0);
	return true;
}

// The player's steps and landing (SND-003; sound.js frame()): a step at each half of the walk while moving on the ground,
// louder running, none while standing or in the air; a thud on landing from a fall faster than 3 m/s, louder from higher.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND003_ThePlayersSteps, "BrixTown.World.SND003_the_players_steps_and_landings",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND003_ThePlayersSteps::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	TestEqual(TEXT("a step indoors is on wood"), BrixStepSurface(true, TEXT("garden")), FString(TEXT("wood")));
	TestEqual(TEXT("a step in a garden is on grass"), BrixStepSurface(false, TEXT("garden")), FString(TEXT("grass")));
	TestEqual(TEXT("a step at the market is on stone"), BrixStepSurface(false, TEXT("market")), FString(TEXT("stone")));
	TestEqual(TEXT("a step in the street is on stone"), BrixStepSurface(false, TEXT("")), FString(TEXT("stone")));
	FBrixPlayerSounds P;
	TArray<FBrixSoundEvent> E;
	double Ph = 0;
	for (int32 f = 0; f < 130; ++f) { Ph = FMath::Fmod(Ph + 1.0 / 60, 1.0); P.Frame(R, true, true, Ph, false, 0, TEXT("stone"), E); }   // a little over two cycles, a cycle a second
	TestEqual(TEXT("walking two cycles: four steps"), E.Num(), 4);
	TestTrue(TEXT("on stone, at the player's level"), E.Num() > 0 && E[0].Sound == TEXT("step_stone_adult") && FMath::IsNearlyEqual(E[0].Gain, R.PlayerStepGain) && E[0].bHere);
	E.Reset();
	for (int32 f = 0; f < 60; ++f) { Ph = FMath::Fmod(Ph + 1.0 / 60, 1.0); P.Frame(R, true, true, Ph, true, 0, TEXT("grass"), E); }
	TestTrue(TEXT("running on grass: louder"), E.Num() == 2 && E[0].Sound == TEXT("step_grass_adult") && FMath::IsNearlyEqual(E[0].Gain, R.PlayerStepGain * 1.4));
	E.Reset();
	for (int32 f = 0; f < 60; ++f) { Ph = FMath::Fmod(Ph + 1.0 / 60, 1.0); P.Frame(R, true, false, Ph, false, -2, TEXT("stone"), E); }
	TestEqual(TEXT("in the air: no steps"), E.Num(), 0);
	P.Frame(R, false, false, Ph, false, -7.2, TEXT("stone"), E);
	P.Frame(R, false, true, Ph, false, 0, TEXT("stone"), E);
	TestTrue(TEXT("landing at 7.2 m/s thuds at the land level times 1.2"), E.Num() == 1 && E[0].Sound == TEXT("act_land") && FMath::IsNearlyEqual(E[0].Gain, R.LandGain * 1.2));
	E.Reset();
	P.Frame(R, false, false, Ph, false, -2.5, TEXT("stone"), E);
	P.Frame(R, false, true, Ph, false, 0, TEXT("stone"), E);
	TestEqual(TEXT("a small hop lands quietly"), E.Num(), 0);
	P.Frame(R, false, true, Ph, false, 0, TEXT("stone"), E);
	P.Frame(R, false, false, Ph, false, 4.8, TEXT("stone"), E);   // Space: up at the slice's 4.8 m/s
	TestTrue(TEXT("a jump is heard (SND-009)"), E.Num() == 1 && E[0].Sound == TEXT("act_jump") && E[0].bHere);
	return true;
}

// Engines (SND-002) and the player's car (SND-009): eight cars around the listener, one coming into reach, one idling,
// one faster than the top note, vans among them, and the player getting in, waiting for the engine to start, speeding
// up, braking hard and being stopped short, run through sound.js engines() and the port: each frame the same cars get the
// six voices, nearest first, with the same pitch, tyres and level; the player's car the same pitch, level and skid, and
// the bump at the same moment.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND002_EnginesAsTheSlice, "BrixTown.World.SND002_engines_sound_as_the_slice_hears_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND002_EnginesAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	const TArray<TSharedPtr<FJsonValue>>& Frames = V->GetObjectField(TEXT("cases"))->GetArrayField(TEXT("engines"));
	const TArray<TSharedPtr<FJsonValue>>& Want = V->GetObjectField(TEXT("trace"))->GetArrayField(TEXT("engines"));
	if (!TestEqual(TEXT("a trace for every frame"), Want.Num(), Frames.Num())) return false;
	FBrixEngineSounds S;
	int32 Wrong = 0, Voices = 0, Bumps = 0, Skids = 0;
	auto Near = [](double A, const TSharedPtr<FJsonValue>& B) { return FMath::IsNearlyEqual(A, B->AsNumber(), 2e-6); };
	for (int32 f = 0; f < Frames.Num(); ++f)
	{
		const TSharedPtr<FJsonObject> F = Frames[f]->AsObject();
		TArray<FBrixCarHeard> Cars;
		for (const TSharedPtr<FJsonValue>& C : F->GetArrayField(TEXT("cars")))
		{
			const TSharedPtr<FJsonObject> O = C->AsObject();
			Cars.Add({ FVector2D(O->GetNumberField(TEXT("x")), O->GetNumberField(TEXT("z"))), O->GetNumberField(TEXT("v")), O->GetBoolField(TEXT("van")) });
		}
		FBrixDriveHeard D;
		const TSharedPtr<FJsonObject>* DO = nullptr;
		const bool bDrive = F->TryGetObjectField(TEXT("drive"), DO);
		if (bDrive) { D.V = (*DO)->GetNumberField(TEXT("v")); D.bGas = (*DO)->GetBoolField(TEXT("gas")); D.bBrake = (*DO)->GetBoolField(TEXT("brake")); }
		TArray<FBrixSoundEvent> Got;
		S.Frame(R, Cars, FVector2D::ZeroVector, bDrive ? &D : nullptr, F->GetNumberField(TEXT("t")), Got);
		const TSharedPtr<FJsonObject> W = Want[f]->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& Pool = W->GetArrayField(TEXT("pool"));
		bool bSame = Pool.Num() == S.Pool.Num();
		for (int32 k = 0; bSame && k < Pool.Num(); ++k)
		{
			const FBrixEngineVoice& G = S.Pool[k];
			if (Pool[k]->IsNull()) { bSame = !G.bOn; continue; }
			const TArray<TSharedPtr<FJsonValue>>& E = Pool[k]->AsArray();
			bSame = G.bOn && Near(G.Hz, E[0]) && Near(G.Tyres, E[1]) && Near(G.Gain, E[2]) && Near(G.Far, E[3]);
			++Voices;
		}
		const TArray<TSharedPtr<FJsonValue>>& M = W->GetArrayField(TEXT("mine"));
		bSame = bSame && Near(S.Mine.Hz, M[0]) && Near(S.Mine.Tyres, M[1]) && Near(S.Mine.Gain, M[2]) && Near(S.Skid, M[3]);
		const TArray<TSharedPtr<FJsonValue>>& Ev = W->GetArrayField(TEXT("events"));
		bSame = bSame && Ev.Num() == Got.Num();
		for (int32 k = 0; bSame && k < Ev.Num(); ++k) bSame = Got[k].Sound == TEXT("car_bump") && FMath::IsNearlyEqual(Ev[k]->AsArray()[4]->AsNumber(), R.BumpGain);
		Bumps += Got.Num();
		Skids += S.Skid > 0;
		if (!bSame && ++Wrong <= 5)
			AddError(FString::Printf(TEXT("frame %d differs from sound.js: the player's car %.4f Hz, level %.4f, skid %.3f; %d sounds"), f, S.Mine.Hz, S.Mine.Gain, S.Skid, Got.Num()));
	}
	AddInfo(FString::Printf(TEXT("%d frames: %d engine voices, %d frames skidding, %d bumps"), Frames.Num(), Voices, Skids, Bumps));
	TestTrue(TEXT("the scripted moments have voices, a skid and a bump"), Voices > 500 && Skids > 0 && Bumps == 1);
	TestEqual(TEXT("frames that differ from sound.js"), Wrong, 0);
	return true;
}

// The tram (SND-004): a tram pulling away, running, stopping and leaving again, one passing the other way, one standing
// at a stop out of hearing, run through sound.js trams() and the port: each frame the same two trams are heard with the
// same level, whine pitch and share and rumble, and the bell rings at the same moments, as loud.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND004_TheTramAsTheSlice, "BrixTown.World.SND004_the_tram_sounds_as_the_slice_hears_it",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND004_TheTramAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	const TArray<TSharedPtr<FJsonValue>>& Frames = V->GetObjectField(TEXT("cases"))->GetArrayField(TEXT("trams"));
	const TArray<TSharedPtr<FJsonValue>>& Want = V->GetObjectField(TEXT("trace"))->GetArrayField(TEXT("trams"));
	if (!TestEqual(TEXT("a trace for every frame"), Want.Num(), Frames.Num())) return false;
	FBrixTramSounds S;
	int32 Wrong = 0, Bells = 0, Whining = 0;
	auto Near = [](double A, const TSharedPtr<FJsonValue>& B) { return FMath::IsNearlyEqual(A, B->AsNumber(), 2e-6); };
	for (int32 f = 0; f < Frames.Num(); ++f)
	{
		const TSharedPtr<FJsonObject> F = Frames[f]->AsObject();
		const double Z = F->GetNumberField(TEXT("tz"));
		TArray<FBrixTramHeard> Trams;
		for (const TSharedPtr<FJsonValue>& T : F->GetArrayField(TEXT("list")))
		{
			const TArray<TSharedPtr<FJsonValue>>& A = T->AsArray();
			Trams.Add({ A[0]->AsNumber(), A[1]->AsBool(), A[2]->AsNumber() });
		}
		TArray<FBrixSoundEvent> Got;
		S.Frame(R, Trams, Z, FVector2D::ZeroVector, F->GetNumberField(TEXT("t")), Got);
		Got.RemoveAll([&](const FBrixSoundEvent& E) { return BrixHear(R, FVector2D::ZeroVector, 0, E.Pos, E.Far).X < .004; });   // too faint to start (once())
		const TSharedPtr<FJsonObject> W = Want[f]->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& Vo = W->GetArrayField(TEXT("voices"));
		bool bSame = Vo.Num() == 2;
		for (int32 k = 0; bSame && k < 2; ++k)
		{
			const FBrixTramVoice& G = S.Voices[k];
			if (Vo[k]->IsNull()) { bSame = !G.bOn; continue; }
			const TArray<TSharedPtr<FJsonValue>>& E = Vo[k]->AsArray();
			bSame = G.bOn && Near(G.Gain, E[0]) && Near(G.Far, E[1]) && Near(G.WhineHz, E[2]) && Near(G.Whine, E[3]) && Near(G.Rumble, E[4]);
			Whining += G.Whine > 0;
		}
		const TArray<TSharedPtr<FJsonValue>>& Ev = W->GetArrayField(TEXT("events"));
		bSame = bSame && Ev.Num() == Got.Num();
		for (int32 k = 0; bSame && k < Ev.Num(); ++k)
			bSame = Got[k].Sound == TEXT("tram_bell") && Near(BrixHear(R, FVector2D::ZeroVector, 0, Got[k].Pos, Got[k].Far).X, Ev[k]->AsArray()[5]);
		Bells += Got.Num();
		if (!bSame && ++Wrong <= 5)
			AddError(FString::Printf(TEXT("frame %d differs from sound.js: level %.4f whine %.2f Hz %.4f rumble %.4f; %d bells"), f, S.Voices[0].Gain, S.Voices[0].WhineHz, S.Voices[0].Whine, S.Voices[0].Rumble, Got.Num()));
	}
	AddInfo(FString::Printf(TEXT("%d frames: %d bells, %d voice-frames whining"), Frames.Num(), Bells, Whining));
	TestTrue(TEXT("the scripted moments ring the bell as it stops and leaves"), Bells == 2 && Whining > 50);
	TestEqual(TEXT("frames that differ from sound.js"), Wrong, 0);
	return true;
}

// The town's ambience (SND-001; sound.js frame()): the street quiet and dark at night, louder and brighter by day, eased
// through 80 minutes either side of dawn and dusk; crickets from 21:00 to 05:00 out of doors only; the nearest fountain;
// birds by day at the rules' rate, more than twice as many by a garden, none at night or while driving.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND001_TheTownsAmbience, "BrixTown.World.SND001_the_town_sounds_by_time_of_day",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND001_TheTownsAmbience::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	TestEqual(TEXT("birds from 05:30"), R.BirdsFrom, 330.0);
	TestEqual(TEXT("crickets from 21:00"), R.CricketsFrom, 1260.0);
	TestTrue(TEXT("within: 06:00 is in 05:30 to 20:30"), BrixWithin(360, 330, 1230));
	TestFalse(TEXT("within: 20:30 is not"), BrixWithin(1230, 330, 1230));
	TestTrue(TEXT("within: 02:00 is in 21:00 to 05:00"), BrixWithin(120, 1260, 300));
	TestFalse(TEXT("within: 12:00 is not"), BrixWithin(720, 1260, 300));
	// sunrise 6, sunset 20: dark until 05:20, half at 06:00, full by 06:40, half again at 20:00
	TestEqual(TEXT("night before 05:20"), BrixStreetDay(300, 6, 20), 0.0);
	TestTrue(TEXT("half day at 06:00"), FMath::IsNearlyEqual(BrixStreetDay(360, 6, 20), .5, 1e-9));
	TestEqual(TEXT("full day at noon"), BrixStreetDay(720, 6, 20), 1.0);
	TestTrue(TEXT("half day at 20:00"), FMath::IsNearlyEqual(BrixStreetDay(1200, 6, 20), .5, 1e-9));
	TestTrue(TEXT("a quarter in: smooth step"), FMath::IsNearlyEqual(BrixStreetDay(340, 6, 20), .25 * .25 * (3 - .5), 1e-9));
	FBrixAmbience A;
	FRandomStream Rng(7);
	TArray<FBrixSoundEvent> E;
	const TArray<FVector2D> Fountains = { FVector2D(30, 0), FVector2D(-8, 4) }, Gardens = { FVector2D(200, 200) };
	A.Frame(R, 720, 6, 20, false, false, FVector2D::ZeroVector, Fountains, Gardens, 1.0 / 60, Rng, E);
	TestTrue(TEXT("noon: the street at its day level, all the day's recording"), FMath::IsNearlyEqual(A.Street, R.StreetDay) && A.StreetDay == 1.0);
	TestEqual(TEXT("noon: no crickets"), A.Crickets, 0.0);
	TestTrue(TEXT("the nearest fountain"), A.Fountain.bOn && A.Fountain.Pos == FVector2D(-8, 4) && A.Fountain.Gain == R.FountainGain);
	A.Frame(R, 120, 6, 20, false, false, FVector2D::ZeroVector, Fountains, Gardens, 1.0 / 60, Rng, E);
	TestTrue(TEXT("02:00: the street at its night level, crickets"), FMath::IsNearlyEqual(A.Street, R.StreetNight) && A.StreetDay == 0.0 && A.Crickets == R.CricketsGain);
	A.Frame(R, 120, 6, 20, true, false, FVector2D::ZeroVector, Fountains, Gardens, 1.0 / 60, Rng, E);
	TestTrue(TEXT("indoors: no crickets, the room's tone"), A.Crickets == 0 && A.Room == R.RoomGain);
	auto Birds = [&](double Minute, const FVector2D& Here, bool bDriving)
	{
		FBrixAmbience B;
		TArray<FBrixSoundEvent> Ev;
		for (int32 f = 0; f < 600 * 60; ++f) B.Frame(R, Minute, 6, 20, false, bDriving, Here, Fountains, Gardens, 1.0 / 60, Rng, Ev);   // ten minutes
		for (const FBrixSoundEvent& X : Ev) if (X.Sound != TEXT("bird") || FVector2D::Distance(X.Pos, Here) < 6 - 1e-6 || FVector2D::Distance(X.Pos, Here) > 28 + 1e-6) return -1;
		return Ev.Num() / 10;
	};
	const int32 Street = Birds(600, FVector2D::ZeroVector, false), Garden = Birds(600, FVector2D(210, 200), false);
	AddInfo(FString::Printf(TEXT("birds a minute: %d in the street, %d by a garden"), Street, Garden));
	TestTrue(TEXT("birds in the street at about the rules' rate, 6 to 28 m away"), Street >= R.BirdsPerMinute * .8 && Street <= R.BirdsPerMinute * 1.2);
	TestTrue(TEXT("by a garden, at its rate"), Garden >= R.GardenBirdsPerMinute * .8 && Garden <= R.GardenBirdsPerMinute * 1.2);
	TestEqual(TEXT("no birds at night"), Birds(60, FVector2D::ZeroVector, false), 0);
	TestEqual(TEXT("no birds while driving"), Birds(600, FVector2D::ZeroVector, true), 0);
	return true;
}

// Money coming in (SND-019, SND-020): which note each coin rings. sound.js reward() traced for coins taken, i of n, for
// rewards of 1 to 25 coins, and the bank's interest coins: the port picks the recording of the same note; a coin a step,
// and a big reward's coins spread over the steps so its last rings at the top.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND019_RewardsRingAsTheSlice, "BrixTown.World.SND019_coins_ring_as_the_slice_rings_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND019_RewardsRingAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	int32 Wrong = 0, Takes = 0, Interest = 0;
	for (const TSharedPtr<FJsonValue>& E : V->GetObjectField(TEXT("trace"))->GetArrayField(TEXT("rewards")))
	{
		const TArray<TSharedPtr<FJsonValue>>& A = E->AsArray();
		const FString What = A[0]->AsString();
		const int32 I = (int32)A[1]->AsNumber(), N = (int32)A[2]->AsNumber(), Midi = (int32)A[3]->AsNumber();
		const bool bTake = What == TEXT("coin_take");
		if (!bTake && I >= R.InterestCoins) continue;   // past the coins the bank pays out: not recorded
		const FString Name = BrixRewardSound(R, What, I, N);
		const int32 K = FCString::Atoi(*Name.Mid(Name.Find(TEXT("_"), ESearchCase::CaseSensitive, ESearchDir::FromEnd) + 1));
		const int32 Got = bTake ? BrixRewardMidi(R.RewardRoot, R.RewardScale, K) : BrixRewardMidi(R.InterestRoot, R.InterestScale, K);
		(bTake ? Takes : Interest)++;
		if (Got != Midi && ++Wrong <= 5) AddError(FString::Printf(TEXT("%s %d of %d: sound.js rings %d, the port %s (%d)"), *What, I, N, Midi, *Name, Got));
		if (bTake && I == N - 1) TestEqual(FString::Printf(TEXT("the last of %d coins a step a coin, up to the top"), N), K, FMath::Min(R.TopStep, N - 1));
	}
	AddInfo(FString::Printf(TEXT("%d coins taken, %d interest coins"), Takes, Interest));
	TestTrue(TEXT("the trace has coins"), Takes > 50 && Interest == R.InterestCoins);
	TestEqual(TEXT("coins ringing another note than sound.js's"), Wrong, 0);
	return true;
}

// The music (SND-010 to SND-012): the score's mood follows the clock (morning from 05:00 ... night, and night again before
// 05:00), and goes under a talk and under nearer music; a café plays in its hours within its reach, clear inside it, the
// nearer of two wins; the market's busker only on market days (Wednesday and Saturday); the radio steps through its music
// stations (and the podcasts) and back to off.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND010_TheMusicAsTheSlice, "BrixTown.World.SND010_the_score_changes_through_the_day",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND010_TheMusicAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	if (!TestTrue(TEXT("the score has its four moods"), R.Moods.Num() == 4)) return false;
	TestEqual(TEXT("04:00 is still night"), BrixMoodAt(R, 240), R.Moods.Last().Name);
	TestEqual(TEXT("05:00 is the first mood"), BrixMoodAt(R, R.Moods[0].From), R.Moods[0].Name);
	for (int32 k = 0; k < R.Moods.Num(); ++k) TestEqual(TEXT("a minute into ") + R.Moods[k].Name, BrixMoodAt(R, R.Moods[k].From + 1), R.Moods[k].Name);
	TestTrue(TEXT("the score's level alone"), FMath::IsNearlyEqual(BrixScoreLevel(R, false, 0), R.ScoreGain));
	TestTrue(TEXT("under a talk"), FMath::IsNearlyEqual(BrixScoreLevel(R, true, 0), R.ScoreGain * R.TalkDuck));
	TestTrue(TEXT("under music near (or the radio)"), FMath::IsNearlyEqual(BrixScoreLevel(R, false, 1), R.ScoreGain * R.SourceDuck));
	TestTrue(TEXT("under faint music, a little"), FMath::IsNearlyEqual(BrixScoreLevel(R, false, .2), R.ScoreGain * (1 - (1 - R.SourceDuck) * .5)));
	// places (SND-011)
	const FBrixSoundRules::FPlace* Food = R.Places.Find(TEXT("food"));
	const FBrixSoundRules::FPlace* Market = R.Places.Find(TEXT("market"));
	if (!TestTrue(TEXT("cafés and the market play"), Food && Market && Market->bMarketDaysOnly)) return false;
	const TArray<FBrixMusicLot> Lots = {
		{ 3, TEXT("food"), FVector2D(10, 0), FVector2D(20, 10), FVector2D(10, 5) },
		{ 8, TEXT("food"), FVector2D(-30, 0), FVector2D(-20, 10), FVector2D(-20, 5) },
		{ 9, TEXT("home"), FVector2D(0, -5), FVector2D(5, 0), FVector2D(2, 0) },
		{ 12, TEXT("market"), FVector2D(0, 20), FVector2D(30, 40), FVector2D(15, 20) } };
	const double Noon = 720;
	FBrixMusicPlace P = BrixMusicPlace(R, Lots, FVector2D(0, 5), INDEX_NONE, Noon, false);
	TestTrue(TEXT("the nearer café, from its door, muffled"), P.Lot == 3 && P.Style == Food->Style && P.Pos == FVector2D(10, 5) && !P.bInside);
	P = BrixMusicPlace(R, Lots, FVector2D(-25, 5), 8, Noon, false);
	TestTrue(TEXT("inside a café: its music, from where the listener is"), P.Lot == 8 && P.bInside && P.Pos == FVector2D(-25, 5));
	P = BrixMusicPlace(R, Lots, FVector2D(0, 5), INDEX_NONE, Food->To + 1, false);
	TestEqual(TEXT("a café after its hours plays nothing"), P.Lot, -1);
	P = BrixMusicPlace(R, Lots, FVector2D(0, -Food->Far - 5), INDEX_NONE, Noon, false);
	TestEqual(TEXT("beyond its reach, nothing"), P.Lot, -1);
	P = BrixMusicPlace(R, Lots, FVector2D(15, 30), INDEX_NONE, Noon, false);
	TestTrue(TEXT("the market on another day: the café"), P.Lot == 3);
	P = BrixMusicPlace(R, Lots, FVector2D(15, 30), INDEX_NONE, Noon, true);
	TestTrue(TEXT("the market on a market day: the busker, in the open air"), P.Lot == 12 && P.Style == Market->Style && P.bOpenAir);
	const FString Cal = FromProject(TEXT("../World/rules/calendar.json"));
	TestTrue(TEXT("Wednesday is a market day"), BrixIsMarketDay(Cal, TEXT("Wed")));
	TestFalse(TEXT("Monday is not"), BrixIsMarketDay(Cal, TEXT("Mon")));
	// the radio (SND-012): its music stations, then off
	int32 S = 0, Music = 0;
	TArray<int32> Seen;
	do { S = BrixNextStation(R, S); Seen.Add(S); } while (S != 0 && Seen.Num() < 20);
	for (int32 k : Seen) if (k > 0 && !R.Radio[k - 1].Style.IsEmpty()) ++Music;   // and the podcasts (SND014_the_radio_has_podcasts)
	TestTrue(TEXT("at least three stations, and back to off"), Music >= 3 && Seen.Last() == 0);
	return true;
}

// Outside is muffled indoors (SND-007): the rules' muffling, and the room's tone indoors, no crickets.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND007_OutsideIsMuffledIndoors, "BrixTown.World.SND007_outside_is_muffled_indoors",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND007_OutsideIsMuffledIndoors::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	TestTrue(TEXT("a muffling filter well below the street's brightness, and quieter"), R.MuffleHz > 100 && R.MuffleHz < 2000 && R.MuffleGain > 0 && R.MuffleGain < 1);
	FBrixAmbience A;
	FRandomStream Rng(3);
	TArray<FBrixSoundEvent> E;
	A.Frame(R, 1380, 6, 20, true, false, FVector2D::ZeroVector, {}, {}, .02, Rng, E);
	TestTrue(TEXT("indoors at night: the room's tone, no crickets"), A.Room == R.RoomGain && A.Crickets == 0);
	A.Frame(R, 1380, 6, 20, false, false, FVector2D::ZeroVector, {}, {}, .02, Rng, E);
	TestTrue(TEXT("out of doors: crickets, no room"), A.Room == 0 && A.Crickets == R.CricketsGain);
	TestEqual(TEXT("a step indoors is on wood"), BrixStepSurface(true, TEXT("")), FString(TEXT("wood")));
	return true;
}

// People's voices (SND-016, SND-017): sound.js hashOf() for a range of numbers, and voiceHz() for residents of four roles
// (children higher, elders lower, the rest in the voices' range): the same pitch for the same resident, to a millionth.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND016_VoicesAsTheSlice, "BrixTown.World.SND016_each_resident_speaks_in_their_own_pitch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND016_VoicesAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	const TSharedPtr<FJsonObject> C = V->GetObjectField(TEXT("cases")), W = V->GetObjectField(TEXT("trace"));
	int32 Wrong = 0;
	const TArray<TSharedPtr<FJsonValue>>& Hc = C->GetArrayField(TEXT("hashes")), &Hw = W->GetArrayField(TEXT("hash"));
	for (int32 k = 0; k < Hc.Num(); ++k) if (!FMath::IsNearlyEqual(BrixHashOf((int64)Hc[k]->AsNumber()), Hw[k]->AsNumber(), 2e-6) && ++Wrong <= 3) AddError(FString::Printf(TEXT("hashOf(%d)"), (int32)Hc[k]->AsNumber()));
	const TArray<TSharedPtr<FJsonValue>>& Vc = C->GetArrayField(TEXT("voices")), &Vw = W->GetArrayField(TEXT("voice"));
	for (int32 k = 0; k < Vc.Num(); ++k)
	{
		const int32 I = (int32)Vc[k]->AsArray()[0]->AsNumber();
		const FString Role = Vc[k]->AsArray()[1]->AsString();
		if (!FMath::IsNearlyEqual(BrixVoiceHz(R, I, Role), Vw[k]->AsNumber(), 2e-5) && ++Wrong <= 6) AddError(FString::Printf(TEXT("voiceHz(%d, %s): sound.js %g, the port %g"), I, *Role, Vw[k]->AsNumber(), BrixVoiceHz(R, I, Role)));
	}
	TestEqual(TEXT("pitches unlike sound.js's"), Wrong, 0);
	TestTrue(TEXT("a child's voice is higher than an elder's"), BrixVoiceHz(R, 5, TEXT("child")) > BrixVoiceHz(R, 5, TEXT("elder")));
	// a line voiced (voice()): every syllable at the same time, pitch, shape and length as sound.js
	FBrixSpeech Sp;
	FBrixDialogue Dl;
	FString Error;
	if (!TestTrue(TEXT("the talk rules load"), LoadBrixTalk(FromProject(TEXT("../World/rules")), FromProject(TEXT("Data/talk.json")), Sp, Dl, Error))) return false;
	const TArray<TSharedPtr<FJsonValue>>& Lc = C->GetArrayField(TEXT("lines")), &Lw = W->GetArrayField(TEXT("lines"));
	int32 Syl = 0;
	for (int32 k = 0; k < Lc.Num(); ++k)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = Lc[k]->AsArray();
		TArray<FBrixSyllable> Got;
		BrixVoiceLine(R, Sp.Speak(A[0]->AsString()), .02, A[1]->AsNumber(), A[2]->AsNumber(), A[3]->AsNumber(), Got);
		const TArray<TSharedPtr<FJsonValue>>& Want = Lw[k]->AsArray();
		bool bSame = Want.Num() == Got.Num();
		for (int32 j = 0; bSame && j < Want.Num(); ++j)
		{
			const TArray<TSharedPtr<FJsonValue>>& E = Want[j]->AsArray();
			bSame = FMath::IsNearlyEqual(Got[j].At, E[0]->AsNumber(), 2e-6) && FMath::IsNearlyEqual(Got[j].Hz, E[1]->AsNumber(), 2e-5) && Got[j].Shape == E[2]->AsString()
				&& FMath::IsNearlyEqual(Got[j].Len, E[3]->AsNumber(), 2e-6) && FMath::IsNearlyEqual(Got[j].Gain, E[4]->AsNumber(), 2e-6);
		}
		Syl += Got.Num();
		if (!bSame) AddError(FString::Printf(TEXT("\"%s\": sound.js voices %d syllables, the port %d"), *A[0]->AsString(), Want.Num(), Got.Num()));
	}
	AddInfo(FString::Printf(TEXT("%d pitches, %d syllables of %d lines"), Vc.Num(), Syl, Lc.Num()));
	return true;
}

// A talk's line that has no voice yet (SND-018: few lines are voiced while the dialogue is not final) is murmured as the
// slice murmurs it (sound.js line(), from conversations.js show()): a dozen lines of World/rules/dialogue.json, each said by
// another resident somewhere else, heard from where they stand out to as far, every syllable at the same moment, pitch,
// mouth shape, length and level.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND018_UnvoicedLinesAreMurmured, "BrixTown.World.SND018_unvoiced_lines_are_murmured",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND018_UnvoicedLinesAreMurmured::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	const TArray<TSharedPtr<FJsonValue>>* Mc = nullptr, * Mw = nullptr;
	if (!TestTrue(TEXT("the values have murmured lines (run Tools/unreal/sound_values.py --values-only)"), V->GetObjectField(TEXT("cases"))->TryGetArrayField(TEXT("murmurs"), Mc)
		&& V->GetObjectField(TEXT("trace"))->TryGetArrayField(TEXT("murmurs"), Mw) && Mc->Num() == Mw->Num() && Mc->Num() > 5)) return false;
	FBrixSpeech Sp;
	FBrixDialogue Dl;
	FString Error;
	if (!TestTrue(TEXT("the talk rules load"), LoadBrixTalk(FromProject(TEXT("../World/rules")), FromProject(TEXT("Data/talk.json")), Sp, Dl, Error))) return false;
	int32 Wrong = 0, Syl = 0;
	for (int32 k = 0; k < Mc->Num(); ++k)
	{
		const TArray<TSharedPtr<FJsonValue>>& A = (*Mc)[k]->AsArray();
		const FVector2D At(A[2]->AsNumber(), A[3]->AsNumber());
		const FBrixVoiceOut Got = BrixMurmur(R, Sp.Speak(A[0]->AsString()), (int32)A[1]->AsNumber(), At);
		const TSharedPtr<FJsonObject> E = (*Mw)[k]->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& Far = E->GetArrayField(TEXT("far")), &Want = E->GetArrayField(TEXT("syllables"));
		bool bSame = Got.Pos.Equals(FVector2D(Far[0]->AsNumber(), Far[1]->AsNumber()), 1e-6) && Got.Far == Far[2]->AsNumber() && Want.Num() == Got.Syllables.Num();
		for (int32 j = 0; bSame && j < Want.Num(); ++j)
		{
			const TArray<TSharedPtr<FJsonValue>>& X = Want[j]->AsArray();
			const FBrixSyllable& G = Got.Syllables[j];
			bSame = FMath::IsNearlyEqual(G.At, X[0]->AsNumber(), 2e-6) && FMath::IsNearlyEqual(G.Hz, X[1]->AsNumber(), 2e-5) && G.Shape == X[2]->AsString()
				&& FMath::IsNearlyEqual(G.Len, X[3]->AsNumber(), 2e-6) && FMath::IsNearlyEqual(G.Gain, X[4]->AsNumber(), 2e-6);
		}
		Syl += Got.Syllables.Num();
		if (!bSame && ++Wrong <= 4) AddError(FString::Printf(TEXT("\"%s\" from resident %d: sound.js murmurs %d syllables, the port %d (or not the same)"), *A[0]->AsString(), (int32)A[1]->AsNumber(), Want.Num(), Got.Syllables.Num()));
	}
	TestTrue(TEXT("every line is murmured"), Syl > Mc->Num() * 3);
	AddInfo(FString::Printf(TEXT("%d syllables of %d lines"), Syl, Mc->Num()));
	return Wrong == 0;
}

// Who talks together (SND-006, SND-016): a group of three eating, two reading (a quiet act: no chatter), a chain of four
// waiting (one group, each within reach of the next), a pair lying down (silent), six pairs further off (only the five
// nearest groups are heard), a walker passing, and a resident who shouts at the player's car twice. Every frame the same
// groups talk, as many murmur, as many are gathered, the crowd hums as loud, and the shouts are voiced at the same
// moments in the same voice, as in sound.js people().
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND016_TalkAsTheSlice, "BrixTown.World.SND016_talk_is_turns_and_quiet_places_stay_quiet",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND016_TalkAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	FBrixSpeech Sp;
	FBrixDialogue Dl;
	FString Error;
	if (!TestTrue(TEXT("the talk rules load"), LoadBrixTalk(FromProject(TEXT("../World/rules")), FromProject(TEXT("Data/talk.json")), Sp, Dl, Error))) return false;
	const TArray<TSharedPtr<FJsonValue>>& Frames = V->GetObjectField(TEXT("cases"))->GetArrayField(TEXT("groups"));
	const TArray<TSharedPtr<FJsonValue>>& Want = V->GetObjectField(TEXT("trace"))->GetArrayField(TEXT("groups"));
	FBrixPeopleTalk T;
	FRandomStream Rng(11);
	int32 Wrong = 0, Shouts = 0, MaxChat = 0;
	for (int32 f = 0; f < Frames.Num(); ++f)
	{
		const TSharedPtr<FJsonObject> F = Frames[f]->AsObject();
		TArray<FBrixHeardPerson> People;
		for (const TSharedPtr<FJsonValue>& P : F->GetArrayField(TEXT("residents"))) People.Add(Person(P->AsObject()));
		TArray<FBrixSoundEvent> Sounds;
		TArray<FBrixVoiceOut> Voices;
		T.Frame(R, People, FVector2D::ZeroVector, F->GetNumberField(TEXT("t")), F->GetNumberField(TEXT("dt")), false, &Sp, Rng, Sounds, Voices);
		const TSharedPtr<FJsonObject> W = Want[f]->AsObject();
		TArray<const FBrixVoiceOut*> Shout;
		for (const FBrixVoiceOut& Vo : Voices) if (Vo.Far == 40) Shout.Add(&Vo);
		const TArray<TSharedPtr<FJsonValue>>& Ws = W->GetArrayField(TEXT("shouts"));
		bool bSame = T.Chat == (int32)W->GetNumberField(TEXT("chat")) && T.Murmur == (int32)W->GetNumberField(TEXT("murmur")) && T.Crowd == (int32)W->GetNumberField(TEXT("crowd"))
			&& FMath::IsNearlyEqual(T.CrowdBed, W->GetNumberField(TEXT("bed")), 2e-6) && Shout.Num() == Ws.Num();
		for (int32 k = 0; bSame && k < Ws.Num(); ++k)
		{
			const TArray<TSharedPtr<FJsonValue>>& E = Ws[k]->AsArray();
			bSame = Shout[k]->Syllables.Num() > 0 && FMath::IsNearlyEqual(Shout[k]->Syllables[0].Gain, E[1]->AsNumber(), 1e-6)
				&& FMath::IsNearlyEqual(BrixVoiceHz(R, 140, TEXT("commuter")) * 1.2, E[0]->AsNumber(), 2e-5);
		}
		Shouts += Shout.Num();
		MaxChat = FMath::Max(MaxChat, T.Chat);
		if (!bSame && ++Wrong <= 5)
			AddError(FString::Printf(TEXT("frame %d: sound.js %d groups, %d murmuring, %d gathered, %d shouts; the port %d, %d, %d, %d"), f, (int32)W->GetNumberField(TEXT("chat")),
				(int32)W->GetNumberField(TEXT("murmur")), (int32)W->GetNumberField(TEXT("crowd")), Ws.Num(), T.Chat, T.Murmur, T.Crowd, Shout.Num()));
	}
	AddInfo(FString::Printf(TEXT("%d frames: up to %d groups talking, %d shouts"), Frames.Num(), MaxChat, Shouts));
	TestTrue(TEXT("the groups are capped and the shout comes twice"), MaxChat == R.ChatGroups && Shouts == 2);
	TestEqual(TEXT("frames that differ from sound.js"), Wrong, 0);
	return true;
}

// What people do (SND-015): ten minutes of residents at rest near the listener, each doing something with a sound (a café
// worker, a seller, someone eating, two asleep, a child on the slide, a reader, a maker, a clerk and a courier, one more
// out of reach). The port makes the same sounds for the same residents as sound.js people() (only the eight nearest busy
// ones are heard; only some sleepers snore, by their hash), each about as often, and the seller and the child call out.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND015_DoingsAsTheSlice, "BrixTown.World.SND015_what_people_do_makes_a_sound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND015_DoingsAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	const TSharedPtr<FJsonObject> Acts = V->GetObjectField(TEXT("cases"))->GetObjectField(TEXT("acts"));
	const TSharedPtr<FJsonObject> Want = V->GetObjectField(TEXT("trace"))->GetObjectField(TEXT("acts"));
	TArray<FBrixHeardPerson> People;
	for (const TSharedPtr<FJsonValue>& P : Acts->GetArrayField(TEXT("residents"))) People.Add(Person(P->AsObject()));
	const double Dt = Acts->GetNumberField(TEXT("dt"));
	FBrixPeopleTalk T;
	FRandomStream Rng(5);
	TMap<FString, int32> Got;
	int32 Calls = 0;
	for (int32 f = 0; f < (int32)Acts->GetNumberField(TEXT("frames")); ++f)
	{
		TArray<FBrixSoundEvent> Sounds;
		TArray<FBrixVoiceOut> Voices;
		T.Frame(R, People, FVector2D::ZeroVector, 1000 + f * Dt, Dt, false, nullptr, Rng, Sounds, Voices);
		for (const FBrixSoundEvent& E : Sounds)
			for (const FBrixHeardPerson& P : People) if (P.Pos == E.Pos) Got.FindOrAdd(FString::FromInt(P.Id) + TEXT("/") + E.Sound.Mid(5))++;
		for (const FBrixVoiceOut& Vo : Voices) if (Vo.Syllables.Num() && FMath::IsNearlyEqual(Vo.Syllables[0].Gain, R.CallGain)) ++Calls;
	}
	TMap<FString, int32> Js;
	int32 JsCalls = 0;
	for (const auto& KV : Want->Values)
	{
		if (FString(KV.Key) == TEXT("calls")) { JsCalls = (int32)KV.Value->AsNumber(); continue; }
		for (const auto& S : KV.Value->AsObject()->Values) Js.Add(FString(KV.Key) + TEXT("/") + FString(S.Key), (int32)S.Value->AsNumber());
	}
	TArray<FString> Keys;
	Js.GetKeys(Keys);
	for (const auto& KV : Got) Keys.AddUnique(KV.Key);
	Keys.Sort();
	for (const FString& K : Keys)
	{
		const int32 A = Js.FindRef(K), B = Got.FindRef(K);
		AddInfo(FString::Printf(TEXT("%s: sound.js %d, the port %d"), *K, A, B));
		TestTrue(TEXT("made by both, about as often: ") + K, A > 0 && B > 0 && FMath::Abs(A - B) <= FMath::Max(6, (int32)(.25 * A)));
	}
	TestTrue(FString::Printf(TEXT("calls: sound.js %d, the port %d"), JsCalls, Calls), JsCalls > 0 && FMath::Abs(JsCalls - Calls) <= FMath::Max(6, (int32)(.25 * JsCalls)));
	TestTrue(TEXT("seven residents make sounds, and the snore is one sleeper's only"), Keys.Num() >= 8 && Js.Contains(TEXT("16/snore")) && !Js.Contains(TEXT("4/snore")));
	return true;
}

// Podcasts on the radio (SND-014): for both shows, at moments through the day, the same part is on air as sound.js
// podcast() puts there (by the world clock), with the same caption, the same host's voice and the same point in the line.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND014_PodcastsAsTheSlice, "BrixTown.World.SND014_the_radio_has_podcasts",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND014_PodcastsAsTheSlice::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	const TSharedPtr<FJsonObject> V = LoadValues(*this);
	if (!V) return false;
	FBrixSpeech Sp;
	FBrixDialogue Dl;
	FString Error;
	if (!TestTrue(TEXT("the talk rules load"), LoadBrixTalk(FromProject(TEXT("../World/rules")), FromProject(TEXT("Data/talk.json")), Sp, Dl, Error))) return false;
	FString Cal;
	FFileHelper::LoadFileToString(Cal, *FromProject(TEXT("../World/rules/calendar.json")));
	TSharedPtr<FJsonObject> CO;
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Cal), CO);
	const double Pace = CO.IsValid() ? CO->GetObjectField(TEXT("calendar"))->GetNumberField(TEXT("real_seconds_per_minute")) : 10;
	const TArray<TSharedPtr<FJsonValue>>& Cases = V->GetObjectField(TEXT("cases"))->GetArrayField(TEXT("podcasts"));
	const TArray<TSharedPtr<FJsonValue>>& Want = V->GetObjectField(TEXT("trace"))->GetArrayField(TEXT("podcasts"));
	int32 Wrong = 0, Lines = 0;
	for (int32 k = 0; k < Cases.Num(); ++k)
	{
		const FString Show = Cases[k]->AsArray()[0]->AsString();
		const double M = Cases[k]->AsArray()[1]->AsNumber();
		const FBrixOnAir A = BrixPodcastAt(R, Sp, Show, M, Pace);
		const TSharedPtr<FJsonObject> W = Want[k]->AsObject();
		const TArray<TSharedPtr<FJsonValue>>& Cap = W->GetArrayField(TEXT("caption"));
		bool bSame = A.Key == W->GetStringField(TEXT("key")) && A.Head == Cap[0]->AsString() && A.Text == Cap[1]->AsString();
		const TArray<TSharedPtr<FJsonValue>>* Said = nullptr;
		if (W->TryGetArrayField(TEXT("said"), Said))
		{
			++Lines;
			bSame = bSame && !A.bIntro && FMath::IsNearlyEqual(A.Pitch, (*Said)[0]->AsNumber(), 1e-6) && FMath::IsNearlyEqual(R.PodVoiceGain, (*Said)[1]->AsNumber(), 1e-6)
				&& FMath::IsNearlyEqual(A.From, (*Said)[2]->AsNumber(), 2e-5) && A.Line.Seq.Num() == (int32)(*Said)[3]->AsNumber();
		}
		if (!bSame && ++Wrong <= 5) AddError(FString::Printf(TEXT("%s at %.2f: sound.js %s \"%s\", the port %s \"%s\""), *Show, M, *W->GetStringField(TEXT("key")), *Cap[1]->AsString(), *A.Key, *A.Text));
	}
	AddInfo(FString::Printf(TEXT("%d moments, %d of them a host speaking"), Cases.Num(), Lines));
	TestTrue(TEXT("hosts speak and the intros play"), Lines > Cases.Num() / 2 && Lines < Cases.Num());
	TestEqual(TEXT("moments unlike sound.js's"), Wrong, 0);
	int32 S = 0, Pods = 0;
	do { S = BrixNextStation(R, S); if (S > 0 && !R.Radio[S - 1].Podcast.IsEmpty()) ++Pods; } while (S != 0);
	TestEqual(TEXT("the radio steps through both podcasts"), Pods, R.Shows.Num());
	return true;
}

// Every sound has its recording (SND-009): the recordings of Game/Data/sounds (Tools/unreal/sound_values.py) are all
// there, read as 16-bit WAV at the index's rate and length, loops among them; every action of sound.json has one, every
// surface's steps for adults and children, and the pose sounds.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND009_EverySoundIsRecorded, "BrixTown.World.SND009_every_sound_has_its_recording",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND009_EverySoundIsRecorded::RunTest(const FString&)
{
	FBrixSoundRules R;
	if (!LoadRules(*this, R)) return false;
	TMap<FString, FBrixClipInfo> Clips;
	TArray<double> EngineHz;
	FString Error;
	const FString Dir = FromProject(TEXT("Data/sounds"));
	if (!TestTrue(TEXT("the index loads: ") + Error, LoadBrixSoundIndex(FPaths::Combine(Dir, TEXT("index.json")), Clips, EngineHz, Error))) return false;
	int32 Bad = 0;
	double Seconds = 0;
	for (const auto& KV : Clips)
	{
		TArray<uint8> File, Pcm;
		int32 Rate = 0, Ch = 0;
		const bool bRead = FFileHelper::LoadFileToArray(File, *FPaths::Combine(Dir, KV.Value.File));
		const bool bOk = KV.Value.File.EndsWith(TEXT(".ogg"))   // Ogg Vorbis (decoded by the engine at run time): its pages begin OggS
			? bRead && File.Num() > 4 && !FMemory::Memcmp(File.GetData(), "OggS", 4)
			: bRead && ReadBrixWav(File, Pcm, Rate, Ch) && Ch == 1 && FMath::IsNearlyEqual((double)Pcm.Num() / (2.0 * Rate), KV.Value.Seconds, 1e-3);
		if (!bOk && ++Bad <= 5) AddError(KV.Key + TEXT(": not a 16-bit mono WAV of the index's length, nor Ogg (is Git LFS installed?)"));
		Seconds += KV.Value.Seconds;
	}
	TestEqual(TEXT("recordings that do not read"), Bad, 0);
	FString Text;
	TSharedPtr<FJsonObject> S;
	FFileHelper::LoadFileToString(Text, *FromProject(TEXT("../World/rules/sound.json")));
	FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), S);
	if (S.IsValid()) for (const auto& KV : S->GetObjectField(TEXT("actions"))->Values) TestTrue(TEXT("the action has a recording: ") + FString(KV.Key), Clips.Contains(TEXT("act_") + FString(KV.Key)));
	for (const FString& Su : R.Surfaces) for (const TCHAR* Who : { TEXT("_adult_0"), TEXT("_child_0") }) TestTrue(TEXT("steps on ") + Su, Clips.Contains(TEXT("step_") + Su + Who));
	for (const FString* P : { &R.PoseSit, &R.PoseStand, &R.PoseLie }) TestTrue(TEXT("a resident's ") + *P, Clips.Contains(TEXT("person_") + *P));
	TestTrue(TEXT("a hard landing"), Clips.Contains(TEXT("act_land")));
	for (const TCHAR* L : { TEXT("engine_lo"), TEXT("engine_hi"), TEXT("tram_hum"), TEXT("tram_whine"), TEXT("tram_rumble"), TEXT("car_skid"),
		TEXT("street_day"), TEXT("street_night"), TEXT("room"), TEXT("crickets"), TEXT("fountain") })
		TestTrue(FString(TEXT("a loop: ")) + L, Clips.Contains(L) && Clips[L].bLoop);
	for (const TCHAR* O : { TEXT("tram_bell"), TEXT("car_bump"), TEXT("car_in"), TEXT("bird_0") }) TestTrue(FString(TEXT("a one-shot: ")) + O, Clips.Contains(O) && !Clips[O].bLoop);
	TestTrue(TEXT("the engine loops' two pitches"), EngineHz.Num() == 2 && EngineHz[0] < EngineHz[1]);
	// the music (SND-010 to SND-012): every mood of the score, every place's style and every music station, as Ogg loops
	for (const FBrixSoundRules::FMood& M : R.Moods) TestTrue(TEXT("the score's ") + M.Name, Clips.Contains(TEXT("music_") + M.Name) && Clips[TEXT("music_") + M.Name].bLoop);
	for (const auto& P : R.Places) TestTrue(TEXT("music at a ") + P.Key, Clips.Contains(TEXT("music_") + P.Value.Style));
	for (const FBrixSoundRules::FStation& St : R.Radio) if (!St.Style.IsEmpty()) TestTrue(TEXT("the station ") + St.Name, Clips.Contains(TEXT("music_") + St.Style));
	// money coming in (SND-019 to SND-021): every moment the widgets call, every note
	for (const TCHAR* W : { TEXT("quest_start"), TEXT("coin_out"), TEXT("coin_land"), TEXT("quest_end"), TEXT("interest_start"), TEXT("interest_end"), TEXT("achievement") })
	{
		const FString Name = BrixRewardSound(R, W, 0, 1);
		TestTrue(TEXT("a reward sound: ") + Name, Clips.Contains(Name) || Clips.Contains(Name + TEXT("_0")));
	}
	for (int32 k = 0; k <= R.TopStep; ++k) TestTrue(TEXT("a coin taken, note ") + FString::FromInt(k), Clips.Contains(FString::Printf(TEXT("reward_coin_take_%d"), k)));
	for (int32 k = 0; k < R.InterestCoins; ++k) TestTrue(TEXT("an interest coin ") + FString::FromInt(k), Clips.Contains(FString::Printf(TEXT("reward_interest_coin_%d"), k)));
	// what people do and say (SND-015, SND-016), the podcasts' intro, the horn
	for (const FBrixSoundRules::FActivity& A : R.Activities) if (!A.Sound.IsEmpty()) TestTrue(TEXT("a doing: ") + A.Sound, Clips.Contains(TEXT("pact_") + A.Sound + TEXT("_0")));
	TArray<double> SylHz, SylLen;
	LoadBrixSyllableIndex(FPaths::Combine(Dir, TEXT("index.json")), SylHz, SylLen);
	TestTrue(TEXT("syllables at several pitches and lengths"), SylHz.Num() >= 2 && SylLen.Num() >= 1);
	for (const FString& Sh : R.VoiceShapes) for (double Hz : SylHz) for (double Len : SylLen)
		TestTrue(TEXT("a syllable: ") + Sh, Clips.Contains(FString::Printf(TEXT("syl_%s_%d_%d"), *Sh, FMath::RoundToInt(Hz), FMath::RoundToInt(Len * 100))));
	for (const TCHAR* N : { TEXT("crowd_bed"), TEXT("car_horn") }) TestTrue(FString(TEXT("a loop: ")) + N, Clips.Contains(N) && Clips[N].bLoop);
	TestTrue(TEXT("the podcasts' intro"), Clips.Contains(TEXT("podcast_intro")));
	AddInfo(FString::Printf(TEXT("%d recordings, %.1f s in all"), Clips.Num(), Seconds));
	// a WAV that is not one is refused
	TArray<uint8> Junk, Pcm;
	Junk.Init(7, 64);
	int32 Rate = 0, Ch = 0;
	TestFalse(TEXT("junk is not a WAV"), ReadBrixWav(Junk, Pcm, Rate, Ch));
	return true;
}

#endif
