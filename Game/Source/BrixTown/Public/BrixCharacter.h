#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BrixBody.h"
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

protected:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaSeconds) override;
public:
	// A talk begins: stand still facing them, the camera in close and looking past the shoulder, the mouse free for the
	// answers (conversations.js open()); and ends (close()).
	void BeginTalk(const FVector& Them, float CameraM);
	void EndTalk();
	// A counter or the money page opens (life.js open()): the player stands, the mouse is free; and closes.
	void BeginPanel();
	void EndPanel();
	// the settings that move the camera and the look (UI-009): metres behind, degrees of view, the mouse's speed, up-down turned
	void SetView(float CameraM, float FovDeg, float LookSpeed, bool bInvertY);
	// for test shots: press E as a player would, and hold W/S and A/D (x steer, y throttle)
	void TestPress() { TalkKey(); }
	FVector2D TestHold = FVector2D::ZeroVector;
	double WalkPhase() const { return Phase; }   // the walk's cycle, 0..1: a step at each half (the sound's steps, SND-003)
	FVector2D DriveKeys() const { return DriveUsed; }   // this frame's steer and throttle while driving (the engine's sound, SND-009)
	bool Handbrake() const { return bHandbrake; }
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
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> PageActions;
	TArray<FString> PageNames;
	float LookScale = 1.f;
	bool bInvertLook = false;
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> ChooseActions;   // left, right, up, down, Enter
	UPROPERTY(Transient) TArray<TObjectPtr<UInputAction>> AnswerActions;
	bool bTalking = false;
	bool bPanel = false;
	bool bDriving = false, bHandbrake = false;
	FVector2D DriveIn = FVector2D::ZeroVector;   // this frame's W/S and A/D while driving
	FVector2D DriveUsed = FVector2D::ZeroVector; // what the car was given this frame (DriveIn is cleared once used)
	float ArmBefore = 0.f;
};
