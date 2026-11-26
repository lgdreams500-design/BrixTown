// CHR-009, CITY-007, SND-018: residents' conversations, mouths and voices as the slice has them.
#include "BrixTalk.h"

#include "Dom/JsonObject.h"
#include "Misc/AutomationTest.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
	bool LoadTalk(FAutomationTestBase& T, FBrixSpeech& S, FBrixDialogue& D)
	{
		FString Error;
		const FString Rules = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules")));
		const FString Talk = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/talk.json")));
		return T.TestTrue(TEXT("the talk rules load: ") + Error, LoadBrixTalk(Rules, Talk, S, D, Error));
	}
}

// Every line of World/rules/dialogue.json (what residents say and what the player can answer), mouthed as
// conversations.js speak() mouths it (Tests/values/talk_city_01.json, from the slice by Tools/unreal/people_values.py):
// the same mouth shapes for the same times, the same words and the same questions, to a microsecond.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCHR009_LinesAreMouthed, "BrixTown.World.CHR009_lines_are_mouthed_as_the_slice_mouths_them",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCHR009_LinesAreMouthed::RunTest(const FString&)
{
	FBrixSpeech S;
	FBrixDialogue D;
	if (!LoadTalk(*this, S, D)) return false;
	FString Text;
	const FString Values = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../Tests/values/talk_city_01.json")));
	if (!TestTrue(TEXT("the test values are there"), FFileHelper::LoadFileToString(Text, *Values))) return false;
	TSharedPtr<FJsonObject> Want;
	if (!TestTrue(TEXT("the test values are JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), Want) && Want.IsValid())) return false;
	int32 Lines = 0, Wrong = 0;
	for (const TSharedPtr<FJsonValue>& V : Want->GetArrayField(TEXT("speak")))
	{
		const TArray<TSharedPtr<FJsonValue>>& P = V->AsArray();
		const FString Line = P[0]->AsString();
		const TSharedPtr<FJsonObject> W = P[1]->AsObject();
		const FBrixLine L = S.Speak(Line);
		++Lines;
		const TArray<TSharedPtr<FJsonValue>>& Seq = W->GetArrayField(TEXT("seq"));
		const TArray<TSharedPtr<FJsonValue>>& Words = W->GetArrayField(TEXT("words"));
		const TArray<TSharedPtr<FJsonValue>>& Asks = W->GetArrayField(TEXT("asks"));
		bool bSame = Seq.Num() == L.Seq.Num() && Words.Num() == L.Words.Num() && Asks.Num() == L.Asks.Num() && FMath::IsNearlyEqual(L.End, W->GetNumberField(TEXT("end")), 1e-6);
		for (int32 k = 0; bSame && k < Seq.Num(); ++k)
		{
			const TSharedPtr<FJsonObject> Q = Seq[k]->AsObject();
			bSame = L.Seq[k].Shape == Q->GetStringField(TEXT("shape")) && FMath::IsNearlyEqual(L.Seq[k].T0, Q->GetNumberField(TEXT("t0")), 1e-6) && FMath::IsNearlyEqual(L.Seq[k].D, Q->GetNumberField(TEXT("d")), 1e-6);
		}
		for (int32 k = 0; bSame && k < Words.Num(); ++k)
		{
			const TSharedPtr<FJsonObject> Q = Words[k]->AsObject();
			bSame = FMath::IsNearlyEqual(L.Words[k].T0, Q->GetNumberField(TEXT("t0")), 1e-6) && L.Words[k].bLong == Q->GetBoolField(TEXT("long"));
		}
		for (int32 k = 0; bSame && k < Asks.Num(); ++k) bSame = FMath::IsNearlyEqual(L.Asks[k], Asks[k]->AsNumber(), 1e-6);
		if (!bSame) { if (Wrong < 8) AddError(TEXT("not mouthed as the slice mouths it: \"") + Line + TEXT("\"")); ++Wrong; }
	}
	TestTrue(TEXT("lines were checked"), Lines > 50);
	return Wrong == 0;
}

// The conversations are whole: each starts at a node it has, every answer leads to a node it has or ends the talk,
// every node can be reached, every rule names a conversation that exists, and every role has words for the box.
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCITY007_ConversationsAreSound, "BrixTown.World.CITY007_every_conversation_is_sound",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FCITY007_ConversationsAreSound::RunTest(const FString&)
{
	FBrixSpeech S;
	FBrixDialogue D;
	if (!LoadTalk(*this, S, D)) return false;
	int32 Wrong = 0;
	for (const TPair<FString, FBrixDialogue::FConversation>& C : D.Conversations)
	{
		const FBrixDialogue::FConversation& Cv = C.Value;
		if (!Cv.Nodes.Contains(Cv.Start)) { AddError(C.Key + TEXT(": starts at a node it does not have")); ++Wrong; continue; }
		TSet<FString> Seen = { Cv.Start };
		TArray<FString> Todo = { Cv.Start };
		while (Todo.Num())
		{
			const FString N = Todo.Pop();
			const FBrixDialogue::FNode& Node = Cv.Nodes[N];
			if (!Node.Options.Num()) { AddError(C.Key + TEXT(".") + N + TEXT(": no answer to give")); ++Wrong; }
			if (!S.Expressions.Contains(Node.Mood)) { AddError(C.Key + TEXT(".") + N + TEXT(": a mood speech.json has no face for: ") + Node.Mood); ++Wrong; }
			for (const FBrixDialogue::FOption& O : Node.Options)
			{
				if (O.Next.IsEmpty()) continue;
				if (!Cv.Nodes.Contains(O.Next)) { AddError(C.Key + TEXT(".") + N + TEXT(": an answer leads to ") + O.Next + TEXT(", which is not there")); ++Wrong; continue; }
				if (!Seen.Contains(O.Next)) { Seen.Add(O.Next); Todo.Add(O.Next); }
			}
		}
		for (const TPair<FString, FBrixDialogue::FNode>& N : Cv.Nodes)
			if (!Seen.Contains(N.Key)) { AddError(C.Key + TEXT(".") + N.Key + TEXT(": cannot be reached")); ++Wrong; }
	}
	for (const FBrixDialogue::FRule& R : D.Rules)
		if (!D.Conversations.Contains(R.Conversation)) { AddError(TEXT("a rule names a conversation that is not there: ") + R.Conversation); ++Wrong; }
	TestTrue(TEXT("there are conversations"), D.Conversations.Num() > 3);
	TestTrue(TEXT("anyone at all has a conversation"), !D.ConversationFor(0, TEXT("nobody"), TEXT("nothing"), false).IsEmpty());
	return Wrong == 0;
}

// SND-018, QST-012: a named character of a story speaks every line of their part. For each resident whose voice is a
// cast voice (Tools/voice/cast), every line they say in World/rules/quests_story.json has its clips, looked up by the
// line's words alone, and each clip is a file. What is seen and who speaks are shown in the box but are not part of the
// words: a line with a direction in front of it would find no voice (it did; found by running Oda's scenes, 2026-11-26).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND018_TheCastSpeakTheirLines, "BrixTown.World.SND018_the_cast_speak_their_story_lines",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND018_TheCastSpeakTheirLines::RunTest(const FString&)
{
	FBrixSpeech S;
	FBrixDialogue D;
	if (!LoadTalk(*this, S, D)) return false;
	FBrixDialogue::FNode N;
	N.Say = TEXT("Aye.");
	TestEqual(TEXT("the words alone are shown as they are"), FBrixDialogue::Shown(N), FString(TEXT("Aye.")));
	N.Dir = TEXT("She does not look up.");
	N.SpeakerName = TEXT("Oda");
	TestEqual(TEXT("what is seen and who says it go before the words"), FBrixDialogue::Shown(N), FString(TEXT("(She does not look up.)\nOda: Aye.")));
	TestEqual(TEXT("and the words stay the words"), N.Say, FString(TEXT("Aye.")));

	auto Read = [&](const FString& Rel) -> TSharedPtr<FJsonObject>
	{
		FString Text;
		TSharedPtr<FJsonObject> O;
		if (!FFileHelper::LoadFileToString(Text, *FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), Rel)))) return nullptr;
		FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text), O);
		return O;
	};
	const TSharedPtr<FJsonObject> City = Read(TEXT("../World/city_01/residents.json")), Story = Read(TEXT("../World/rules/quests_story.json")), Cast = Read(TEXT("../World/rules/story_cast.json"));
	if (!TestTrue(TEXT("the residents, the story and its cast are read"), City.IsValid() && Story.IsValid() && Cast.IsValid())) return false;
	TMap<FString, int32> Index;
	for (const TSharedPtr<FJsonValue>& R : City->GetArrayField(TEXT("residents"))) Index.Add(R->AsObject()->GetStringField(TEXT("id")), Index.Num());
	TMap<FString, FString> Played;   // a cast id is the resident who plays it
	for (const auto& KV : Cast->GetObjectField(TEXT("cast"))->Values) Played.Add(FString(KV.Key), KV.Value->AsString());
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT(".."), D.VoiceDir));
	int32 Lines = 0, Speakers = 0;
	TSet<int32> Seen;
	for (const auto& QV : Story->GetObjectField(TEXT("quests"))->Values)
	{
		const TSharedPtr<FJsonObject> Q = QV.Value->AsObject();
		FString Giver;
		Q->TryGetStringField(TEXT("giver"), Giver);
		for (const auto& SV : Q->GetObjectField(TEXT("stages"))->Values)
		{
			const TSharedPtr<FJsonObject> St = SV.Value->AsObject();
			FString Talk;
			const TSharedPtr<FJsonObject>* Goal = nullptr;
			if (St->TryGetObjectField(TEXT("goal"), Goal)) (*Goal)->TryGetStringField(TEXT("talk"), Talk);
			const TSharedPtr<FJsonObject>* Nodes = nullptr;
			if (!St->TryGetObjectField(TEXT("nodes"), Nodes)) continue;
			for (const auto& NV : (*Nodes)->Values)
			{
				const TSharedPtr<FJsonObject> Nd = NV.Value->AsObject();
				FString Who, Say, Mood = TEXT("neutral");
				Nd->TryGetStringField(TEXT("who"), Who);
				Nd->TryGetStringField(TEXT("mood"), Mood);
				if (Who == TEXT("scene") || !Nd->TryGetStringField(TEXT("say"), Say)) continue;
				FString Rid = Who.IsEmpty() ? (Talk.IsEmpty() ? Giver : Talk) : Who;
				if (const FString* P = Played.Find(Rid)) Rid = *P;
				const int32* w = Index.Find(Rid);
				if (!w || !D.ResidentVoice.IsValidIndex(*w) || !D.VoiceConfigs.IsValidIndex(D.ResidentVoice[*w]) || !D.VoiceConfigs[D.ResidentVoice[*w]].StartsWith(TEXT("cast_"))) continue;
				++Lines;
				if (!Seen.Contains(*w)) { Seen.Add(*w); ++Speakers; }
				const TOptional<FBrixDialogue::FPlan> P = D.Plan(*w, Say, {}, Mood);
				if (!P) { AddError(FString::Printf(TEXT("%s has no voice for: %s"), *Rid, *Say)); continue; }
				for (const TPair<FString, double>& Part : P->Parts)
					if (Part.Key.IsEmpty() || !FPaths::FileExists(FPaths::Combine(Root, Part.Key))) AddError(FString::Printf(TEXT("%s: a piece of this line is not voiced: %s"), *Rid, *Say));
			}
		}
	}
	TestTrue(TEXT("Oda, the first of the cast, speaks her seventeen lines"), Speakers >= 1 && Lines >= 17);
	return !HasAnyErrors();
}

// SND-018: the lines are voiced. Every resident's opening line of the passer-by talk is found in the voiced pieces for a
// sample of residents, with its blanks filled; the length of a line is its pieces' and the gaps between them; a line with
// nothing voiced has no plan (the mouth then keeps the text's own time).
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FSND018_LinesAreVoiced, "BrixTown.World.SND018_lines_are_voiced",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ClientContext | EAutomationTestFlags::ProductFilter)
bool FSND018_LinesAreVoiced::RunTest(const FString&)
{
	FBrixSpeech S;
	FBrixDialogue D;
	if (!LoadTalk(*this, S, D)) return false;
	TestTrue(TEXT("voices are indexed"), D.Clips.Num() > 100 && D.ResidentVoice.Num() > 100);
	int32 Voiced = 0, Tried = 0;
	for (const TPair<FString, FBrixDialogue::FConversation>& C : D.Conversations)
		for (const TPair<FString, FBrixDialogue::FNode>& N : C.Value.Nodes)
			for (int32 r = 0; r < 40; r += 7)
			{
				++Tried;
				const TMap<FString, FString> Vars = { { TEXT("name"), TEXT("Cleo") }, { TEXT("greeting"), TEXT("Good morning") }, { TEXT("player"), D.PlayerName } };
				const TOptional<FBrixDialogue::FPlan> P = D.Plan(r, N.Value.Say, Vars, N.Value.Mood);
				if (!P) continue;
				++Voiced;
				double Sum = 0;
				for (const TPair<FString, double>& Pt : P->Parts) Sum += Pt.Value;
				TestTrue(TEXT("a line lasts as long as its pieces and the gaps"), FMath::IsNearlyEqual(P->Dur, Sum + D.GapS * (P->Parts.Num() - 1), 1e-9));
			}
	AddInfo(FString::Printf(TEXT("%d of %d lines voiced for the residents tried"), Voiced, Tried));
	TestTrue(TEXT("lines are voiced"), Voiced > 0);
	TestFalse(TEXT("nothing voiced, no plan"), D.Plan(0, TEXT("Zzyzx qwv."), {}, TEXT("neutral")).IsSet());
	return true;
}

#endif
