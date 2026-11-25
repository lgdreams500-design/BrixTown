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
		const bool bOk = FFileHelper::LoadFileToArray(File, *FPaths::Combine(Dir, KV.Value.File)) && ReadBrixWav(File, Pcm, Rate, Ch)
			&& Ch == 1 && FMath::IsNearlyEqual((double)Pcm.Num() / (2.0 * Rate), KV.Value.Seconds, 1e-3);
		if (!bOk && ++Bad <= 5) AddError(KV.Key + TEXT(": not a 16-bit mono WAV of the index's length (is Git LFS installed?)"));
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
	AddInfo(FString::Printf(TEXT("%d recordings, %.1f s in all"), Clips.Num(), Seconds));
	// a WAV that is not one is refused
	TArray<uint8> Junk, Pcm;
	Junk.Init(7, 64);
	int32 Rate = 0, Ch = 0;
	TestFalse(TEXT("junk is not a WAV"), ReadBrixWav(Junk, Pcm, Rate, Ch));
	return true;
}

#endif
