#pragma once

#include "CoreMinimal.h"
#include "BrixBody.h"

// Talking to residents (CITY-007), their faces as they speak (CHR-007, CHR-009) and their voices (SND-018): a port of the
// slice's conversations.js and voices.js. The conversations are data (World/rules/dialogue.json), how a line is mouthed
// and how faces look in each mood too (World/rules/speech.json); what the slice keeps in code (the words for what someone
// is doing, the index of voiced pieces) comes from Game/Data/talk.json, written by Tools/unreal/people_values.py.
// CHR009_lines_are_mouthed_as_the_slice_mouths_them checks Speak() against conversations.js speak().

// A line as the mouth says it (conversations.js speak()): mouth shapes in time, the words (for nods), the questions.
struct BRIXWORLD_API FBrixLine
{
	struct FShape { FString Shape; double T0 = 0, D = 0; };
	struct FWord { double T0 = 0; bool bLong = false; };
	TArray<FShape> Seq;
	TArray<FWord> Words;
	TArray<double> Asks;          // when each question mark ends
	double End = 0;
};

struct BRIXWORLD_API FBrixSpeech
{
	struct FExpression { TOptional<FString> MouthShape; FBrixFace::FMouth Mouth; double Curve = 0, BrowL[2] = { 0, 0 }, BrowR[2] = { 0, 0 }, Lid = 0, Squint = 0, Open = 1, Gaze[2] = { 0, 0 }; };
	double DrawWidth = 1, DrawOpening = 1;
	TMap<FString, FBrixFace::FMouth> Shapes;   // as speech.json has them (not yet scaled for drawing)
	TMap<FString, FExpression> Expressions;
	double Fps = 12, FrameQuick = 1, FrameNormal = 2, FrameStressed = 3;
	TArray<FString> StressedShapes;
	int32 StressedWordLetters = 4;
	TMap<FString, double> Pauses;
	TMap<FString, FString> EndShape;           // the mouth after a line, by mood ("default" for the rest)
	double BlendMouth = 18, BlendExpression = 5;
	FString Vowels;
	TMap<FString, FString> Digraphs, Letters;  // a letter with no shape is left out
	double LineStartBrow = 0, LineStartSeconds = 1, QuestionBrow = 0, QuestionSeconds = 1, Nod = 0, NodSeconds = 1;
	int32 AccentWordLetters = 6;

	FBrixLine Speak(const FString& Text) const;
	FBrixFace::FMouth Shape(const FString& Name) const;   // scaled for drawing (conversations.js SHAPE())
	// What the face should be T seconds into Line, said in Mood (conversations.js faceTarget()).
	FBrixFace Target(const FBrixLine& Line, const FString& Mood, double T) const;
	// The face blended towards Target over Dt seconds (blendFace()).
	void Blend(FBrixFace& Face, const FBrixFace& Target, double Dt) const;
	FBrixFace Rest() const;
};

struct BRIXWORLD_API FBrixDialogue
{
	struct FOption
	{
		FString Text; FString Next; TArray<FString> If, Do;   // Next empty: the talk ends
		TArray<FString> Need, Act;   // a quest's: what the player must have to say it, and what saying it does (BrixQuests)
		FString Quest, Job;          // the quest line it belongs to; "take" or "hand" for a job (life.js _q, _job)
	};
	struct FNode { FString Say, Mood = TEXT("neutral"); TArray<FOption> Options; FString Who, Dir; int32 Speaker = -1; FString SpeakerName; };   // Who: another resident who says it, or "scene" for a line nobody says; Dir: what is seen, never spoken (QST-012)
	// Speaker, SpeakerName: the resident (index, name) who says a line that is not the one talked to's (QST-012).
	// What is written in the talk box for a line: what is seen above it in brackets, and the name of somebody else who
	// says it, before the words. Say itself stays the words alone: they are what is voiced and mouthed (SND-018, CHR-009).
	static FString Shown(const FNode& N) { return (N.Dir.IsEmpty() ? FString() : TEXT("(") + N.Dir + TEXT(")\n")) + (N.SpeakerName.IsEmpty() ? FString() : N.SpeakerName + TEXT(": ")) + N.Say; }
	struct FConversation { FString Title, Start; TMap<FString, FNode> Nodes; TMap<FString, FString> Vars; };
	struct FRule { FString Conversation; TArray<FString> Roles, Acts; TOptional<bool> bWalking; };
	TMap<FString, FConversation> Conversations;
	TArray<FRule> Rules;
	TMap<FString, FString> Residents;          // a resident's own conversation, by resident index
	TMap<FString, FString> RoleWords;
	TMap<FString, FString> Doing, Going;       // play.js DOING, GOING: what someone is doing at an act, or on their way to
	FString PlayerName = TEXT("you");
	// the voiced pieces (audio/voices/index.js)
	FString VoiceDir;                          // relative to the repository
	TArray<FString> VoiceConfigs;
	TArray<int32> ResidentVoice;               // into VoiceConfigs, by resident
	TMap<FString, int32> PlayerVoices;
	FString PlayerVoice;
	double GapS = .06, BlankGapS = .35, Volume = .9;
	TMap<FString, TPair<FString, double>> Clips;   // "config|mood|text" -> file, seconds

	// The conversation for a resident: their own, else the first rule their role, act and walking match (conversationFor()).
	FString ConversationFor(int32 Resident, const FString& Role, const FString& Act, bool bWalking) const;
	static FString Fill(const FString& Text, const TMap<FString, FString>& Vars);
	static bool Allowed(const FOption& O, const TSet<FString>& Flags);   // its 'if': each flag set, or "!flag" not set
	// The pieces of a line as Who (a resident's index, or -1 the player) says it (voices.js plan()): a file or a gap each,
	// and the whole length; none when no piece of it is voiced.
	struct FPlan { TArray<TPair<FString, double>> Parts; double Dur = 0; };
	TOptional<FPlan> Plan(int32 Who, const FString& Template, const TMap<FString, FString>& Vars, const FString& Mood) const;
};

// Reads World/rules/speech.json, World/rules/dialogue.json and Game/Data/talk.json. On failure returns false and says why.
BRIXWORLD_API bool LoadBrixTalk(const FString& RulesDir, const FString& TalkPath, FBrixSpeech& OutSpeech, FBrixDialogue& OutDialogue, FString& OutError);
