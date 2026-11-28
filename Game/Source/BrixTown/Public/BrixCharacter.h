#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BrixBody.h"
#include "BrixBoards.h"
#include "BrixCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
class UProceduralMeshComponent;
struct FInputActionValue;
struct FInputActionInstance;

// The player in third person (CORE-001: walk, run, jump). Their body is the slice's player (people.js player(), from
// Game/Data/people.json) at full detail, built each frame by BrixBody as the residents are (ART-004): walking by their
// movement signature, running by the slice's Run (ANIM-002), standing still when not moving. The keys are those of the browser slice (W A S D, mouse, Shift, Space), made in code so the project needs
// no input assets yet.
UCLASS(Config = Game)
class BRIXTOWN_API ABrixCharacter : public ACharacter
{
	GENERATED_BODY()

public:
	ABrixCharacter();

	UPROPERTY(Config) float WalkSpeed = 1.4f;        // m/s
	UPROPERTY(Config) float RunSpeed = 4.5f;         // m/s
	UPROPERTY(Config) float CameraDistance = 4.f;    // m behind the player
	UPROPERTY(Config) float CameraHeight = 0.6f;     // m above the shoulders' pivot
	// In a room the camera comes in to IndoorCameraDistance: at the street's 4 m it was against a wall or the ceiling at
	// almost every turn, jammed into the player and not following the mouse. The body is not drawn while the camera is
	// within HideBodyWithin (m) of it, and the mouse has the camera for LookHoldSeconds before a car, a bus or a board
	// brings it round behind again (pulled back every frame, it could not be turned while moving).
	UPROPERTY(Config) float IndoorCameraDistance = 2.8f;
	UPROPERTY(Config) float HideBodyWithin = 0.55f;
	UPROPERTY(Config) float LookHoldSeconds = 1.5f;
	UPROPERTY(Config) float RideBlockSeconds = 0.15f;
	UPROPERTY(Config) float AnswerCameraYaw = 160.f;  // degrees round from looking where the player faces, while they answer
	UPROPERTY(Config) float TalkCameraYaw = 26.f;     // and while the resident speaks: past the shoulder (the slice's .45 rad)
	UPROPERTY(Config) float DesignCameraM = 2.7f;    // m from the character in the designer, times their height / 1.72 (life.js camera())

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
public:
	// A talk begins: stand still facing them, the camera in close and looking past the shoulder, the mouse free for the
	// answers (conversations.js open()); and ends (close()).
	void BeginTalk(const FVector& Them, float CameraM);
	void EndTalk();
	// in a talk: the player's own face (ABrixTalk blends it), drawn on the body; while they answer the camera comes round
	// to look at them past the resident (a reverse shot), and goes back over the shoulder when the resident speaks
	void SetTalkFace(const FBrixFace* F) { TalkFace = F; }
	void SetAnswering(bool bOn) { bAnswering = bOn; }
	// A counter or the money page opens (life.js open()): the player stands, the mouse is free; and closes.
	void BeginPanel();
	void EndPanel();
	// the settings that move the camera and the look (UI-009): metres behind, degrees of view, the mouse's speed, up-down turned
	void SetView(float CameraM, float FovDeg, float LookSpeed, bool bInvertY);
	// for test shots: press E as a player would, and hold W/S and A/D (x steer, y throttle)
	void TestPress() { TalkKey(); }
	// Glass travel tubes (CITY-109 to CITY-111): on a ride; the stop E would ride to from the booth stood in (empty if
	// none); the arrow keys choose another; a test shot's ride to a stop, as if chosen and E pressed
	bool IsTubeRiding() const { return bTubeRiding; }
	FString TubeOffer() const;
	void TestTube(const FString& To);
	FVector2D TestHold = FVector2D::ZeroVector;
	double WalkPhase() const { return Phase; }   // the walk's cycle, 0..1: a step at each half (the sound's steps, SND-003)
	FVector2D DriveKeys() const { return DriveUsed; }   // this frame's steer and throttle while driving (the engine's sound, SND-009)
	bool Handbrake() const { return bHandbrake; }
	// Skateboards (CITY-098 to CITY-101; play.js boardKey(), stepBoard()): the boards owned, the one in use carried in the
	// right hand or ridden. A test shot (or -BrixBoards) owns every kind with the electric board in use, as the slice's ?auto=1,
	// unless -BrixNoBoards (a new game's none, to buy at the skate store: CITY-102, CITY-103);
	// -BrixBoard=<kind> gets on that one a third into the shot's wait (with -BrixHold=steer,throttle held)
	FBrixBoardRules BoardRules;
	FBrixBoards Boards;
	void BoardKey();                 // Y: get on the board in use, and off it again, picking it up
	void WeatherKey();               // K: try the next kind of weather (WLD-037; the slice's K)
	void SkipMinutesKey() { SkipKey(TEXT("10m")); }   // Z: skip 10 minutes (WLD-073; settings.json minutes)
	void SkipHourKey() { SkipKey(TEXT("1h")); }       // T: skip an hour (settings.json hour, the slice's T)
	void SkipWeatherKey() { SkipKey(TEXT("weather")); }   // X: skip to the next change of weather (settings.json next_weather)
	void SkipKey(const FString& Step);
	void TestBoard(const FString& Kind);
	// the boards owned and the one in use set anew (a game loaded): off any board ridden first (CITY-100)
	void SetBoards(const TArray<FString>& Own, const FString& Use);
	// the character's look (CHR-001): the designed character's (ABrixDesigner), drawn from the next frame
	void SetLook(const FBrixLook& L) { Rules.Player = L; }
	const FBrixLook& CurrentLook() const { return Rules.Player; }
	// while the character designer is open (life.js camera()): the camera close in front, turned by SpinDeg, the character
	// framed in the part of the screen PanelShare (of its width, from the left) leaves free; the body drawn as time passes
	void SetDesignView(bool bOn, double SpinDeg, float PanelShare, bool bTakeKeys = true, float CameraM = 0.f);   // CameraM: metres off (times height / 1.72), 0 for DesignCameraM   // bTakeKeys: the game's keys do nothing meanwhile
	bool IsDesigning() const { return bDesigning; }
	// On the playground (WLD-041; play.js interact(), stepPlayer(), offPlay()): E at a swing seat, seesaw end, slide's ladder,
	// sandpit or climbing frame has a go there; E, Space or moving gets off, beside it (a slide's go ends at its bottom)
	bool IsPlaying() const { return PlayUse != INDEX_NONE; }
	// Sitting or lying on a piece of furniture that has a place for it (play.js interact(), sit and lie; FURN-022, CITY-030):
	// E at the place sits the player on it, facing as the piece does; E, Space or moving gets up, where they stood
	bool IsSeated() const { return bSeated; }
	bool SeatOffer(FString& OutLabel, double& OutMetres) const;   // what E offers within reach, in the mall or the furniture outlet
	void TestSeat(const FString& LabelPart);                     // for a test shot: at the first seat whose offer has these words, and E
	void StartPlay(int32 Use);
	void StopPlay(bool bAtEnd = false);
	void TestPlay(int32 Use);   // -BrixPlay=<n>: a test shot has a go at the n-th place
protected:
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

private:
	void MakeInput();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartRun();
	void StopRun();
	void TalkKey();                  // E: talk to the resident the player is next to (CITY-007)
	void AnswerKey(const FInputActionInstance& Instance);   // 1 to 9: an answer
	void LeaveKey();                 // Esc: leave the talk, or close a counter or the money page
	void CounterKey();               // B: the counter here (life.js counter())
	void WalletKey();                // I: the money page
	void QuestsKey();                // J: the quest log
	void MenuPageKey(const FInputActionInstance& Instance);   // C, G, P, U, M, H: a page of the menu
	void ChooseKey(const FInputActionInstance& Instance);   // the arrows and Enter at a shop's counter (UI-020)
	class ABrixLife* Life() const;
	void JumpKey();                  // Space: a jump on foot, the handbrake in a car
	void JumpEnd();
	void StepBoard(float Dt);         // riding: the speed along the heading, A and D turn, the camera comes round behind
	class ABrixCars* Cars() const;
	class ABrixTalk* Talk() const;

	UPROPERTY(VisibleAnywhere) TObjectPtr<USpringArmComponent> Boom;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UProceduralMeshComponent> Person;
	UPROPERTY() TObjectPtr<UMaterialInterface> BodyMaterial;
	FBrixPeopleRules Rules;
	FBrixBodyMesh Scratch;
	double Phase = 0;                // the walk's cycle (the slice's pl.ph): metres walked / the signature's step length
	int32 Verts = 0;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Keys;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RunAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> TalkAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LeaveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> CounterAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> WalletAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> QuestsAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> BoardAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> WeatherAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SkipMinutesAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SkipHourAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> SkipWeatherAction;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> PageActions;
	TArray<FString> PageNames;
	float LookScale = 1.f;
	bool bInvertLook = false;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> ChooseActions;   // left, right, up, down, Enter
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> AnswerActions;
	bool bTalking = false;
	bool bPanel = false;
	bool bDriving = false, bHandbrake = false;
	// a glass tube ride (CITY-109, CITY-110): the server's ride, when it began, the camera's place and turn as the ride took
	// it over and as it gave it back, and which stop the booth's arrows have chosen
	bool bTubeRiding = false;
	bool bBoothProbeOff = false;               // the boom's probe turned off while standing in a tube booth
	TSharedPtr<struct FBrixRide> TubeRide;
	double TubeT0 = 0, TubeBackLeft = 0;
	FVector TubeCamFrom = FVector::ZeroVector, TubeCamLast = FVector::ZeroVector;
	FQuat TubeCamFromRot = FQuat::Identity, TubeCamLastRot = FQuat::Identity;
	FTransform TubeCamRel;
	int32 TubePick = 0;
	FString TubePickAt;
	UPROPERTY(EditAnywhere) float TubeCamBlendSeconds = .8f;   // from the everyday camera onto the ride's, and back
	bool StartTube(const FString& To);
	void TubeTick(float DeltaSeconds);
	class ABrixTubes* Tubes() const;
	float PersonWet = -1.f;          // how wet the player's clothes are shown (WLD-037)
	FVector2D DriveIn = FVector2D::ZeroVector;   // this frame's W/S and A/D while driving
	FVector2D DriveUsed = FVector2D::ZeroVector; // what the car was given this frame (DriveIn is cleared once used)
	float ArmBefore = 0.f;
	double LookedAt = -100;          // when the mouse last turned the camera
	bool CameraFree() const;         // the mouse has not turned it for LookHoldSeconds
	// Behind a vehicle the arm is the character's own work: it comes in only for something that has stood in its way for
	// RideBlockSeconds (a wall, not a lamp post passed), quickly, and goes out again slowly.
	void RideCamera(float DeltaSeconds);
	float RideArm = 0.f, RideBlocked = 0.f;
	FVector2D BoardIn = FVector2D::ZeroVector;   // this frame's A/D and W/S on a board
	double BoardYaw = 0, BoardBlend = 0, BoardMeant = 0;   // the heading (Unreal degrees), into the push (0 to 1), the metres last meant to go
	FVector BoardFrom = FVector::ZeroVector;
	TSharedPtr<class SWidget> BoardBar;
	bool bDesigning = false, bDesignKeys = true;
	const FBrixFace* TalkFace = nullptr;
	bool bAnswering = false;
	double TalkYaw = 0;
	double DesignSpin = 0;
	float DesignM = 0.f;
	float DesignShare = 0.f;
	FVector BoomOffsetBefore = FVector::ZeroVector;
	class ABrixPeople* People() const;
	int32 PlayUse = INDEX_NONE;
	bool bSeated = false, bSeatLie = false, bSeatOff = false;
	FVector SeatAt = FVector::ZeroVector, SeatFrom = FVector::ZeroVector;   // the seat's top (x, z, up; metres); where the player stood (cm)
	double SeatYaw = 0;
	const class ABrixMall* SeatHere(int32& OutSeat, double& OutMetres) const;
	void StartSeat(const FVector& At, double Yaw, bool bLie, const FString& Label);
	void StopSeat();
	double PlayT0 = 0;
	bool bPlayOff = false;           // moving or Space while on it: off at the next frame
	FVector2D PlayEnd = FVector2D::ZeroVector;
};
