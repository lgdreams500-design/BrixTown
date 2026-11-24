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
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
	virtual void PawnClientRestart() override;

private:
	void MakeInput();
	void Move(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void StartRun();
	void StopRun();

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
};
