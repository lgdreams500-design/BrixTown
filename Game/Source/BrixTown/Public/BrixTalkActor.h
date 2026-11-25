#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "BrixPeople.h"
#include "BrixTalk.h"
#include "BrixTalkActor.generated.h"

class UAudioComponent;
class SWidget;
struct FBrixDay;
struct FBrixCity;

// Talking to residents (CITY-007), as the slice's conversations.js does it: E near a resident opens the talk, the
// resident stops and turns to the player, the box at the foot of the screen shows who they are and what they say, 1 to 9
// (or a click) answers, Esc leaves. The resident says each line aloud when it is voiced (SND-018, the slice's pieces of
// Kokoro speech, decoded here at run time) and their face follows it: mouth shapes from the line's text stretched to the
// voice's length, brows and nods on stressed words and questions, the mood's expression (CHR-007, CHR-009). The player's
// answers are said in the player's voice. Afterwards the resident runs late by the time the talk took (CITY-022).
// The quests, jobs and counters that the slice's life.js adds to talks come from ABrixLife (Conversation(), Allowed(),
// Chose(), TalkEnded()), as conversations.js asks Life for them.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixTalk : public AActor
{
	GENERATED_BODY()

public:
	const FBrixDialogue& Words() const { return Dialogue; }   // the words for what residents are doing (the names over them)
	ABrixTalk();
	void Start(const FBrixCity& City, TSharedPtr<FBrixDay> InDay, ABrixPeople* InPeople);
	virtual void Tick(float DeltaSeconds) override;
	virtual void EndPlay(const EEndPlayReason::Type Reason) override;

	bool IsTalking() const { return Res >= 0; }
	double CandidateDistance() const;   // metres to the resident who could be talked to, or -1
	void Open();                        // with the resident the player is next to, if any
	void Choose(int32 N);               // the N-th answer (0 first)
	void Close();
	void SetPlayerName(const FString& Name) { Dialogue.PlayerName = Name; }   // {player} (CHR-001)
	bool IsAnswering() const { return bAnswering; }
	int32 Answered = 0;                  // answers chosen so far (the sound of choosing, SND-009)
	const FString& Offer() const { return PromptText; }   // what E offers now ("Talk to ...", "Drive this car"), or empty

	UPROPERTY(Config) float CloseCamera = 2.6f;   // m: the camera comes in this close so the face can be read
	UPROPERTY(Config) float AnswerPause = 0.35f;  // s after the player's voiced answer before the resident's next line

private:
	void Show(const FString& Node);
	TMap<FString, FString> Vars() const;
	class ABrixLife* Life() const;
	void SayAloud(int32 Who, const FString& Template, const FString& Mood, double& OutDur);
	void BuildUI();
	void Refresh();

	FBrixSpeech Speech;
	FBrixDialogue Dialogue;
	TSharedPtr<FBrixDay> Day;
	UPROPERTY() TObjectPtr<ABrixPeople> People;
	UPROPERTY() TObjectPtr<UAudioComponent> Voice;
	TArray<FString> LotNames;
	FString CityName;
	TMap<int32, TSet<FString>> Flags;    // per resident: what has been said (an answer's 'do')
	TMap<FString, TArray<uint8>> Decoded;   // a voiced piece's sound, decoded once
	int32 VoiceRate = 24000, VoiceChannels = 1;

	// the talk under way (conversations.js s)
	int32 Res = -1;
	ABrixPeople::FCandidate At;          // where they were when the talk began (held there)
	FString ConvId, NodeId, Mood = TEXT("neutral"), Say;
	FBrixDialogue::FConversation Conv;  // the talk as Life gives it: the resident's own, with quest lines and work added
	TArray<int32> Opts;                  // the node's answers the player may give
	FBrixLine Line;
	FBrixFace Face;
	FBrixFace PlayerFace;                // the player's own face: it says their answer, then listens (CHR-009)
	FBrixLine PlayerLine, Quiet;
	double PT0 = 0, PK = 1;
	bool bAnswering = false;
	double T0 = 0, K = 1, Minute0 = 0, NextAt = -1;
	FString NextNode;
	bool bNextPending = false;
	float SavedArm = 0.f;

	TSharedPtr<SWidget> Box, Prompt;
	TSharedPtr<class SVerticalBox> OptionList;
	FString PromptText;
};
