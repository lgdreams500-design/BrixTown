#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "BrixCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
struct FInputActionValue;

// The player in third person (CORE-001: walk, run, jump). A grey-box stand-in body until the option B people are
// imported. The keys are those of the browser slice (W A S D, mouse, Shift, Space), made in code so the project needs
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
	UPROPERTY(Config) FLinearColor BodyColour = FLinearColor(0.12f, 0.3f, 0.55f);
	UPROPERTY(Config) FLinearColor HeadColour = FLinearColor(0.8f, 0.62f, 0.5f);

protected:
	virtual void BeginPlay() override;
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
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Body;
	UPROPERTY(VisibleAnywhere) TObjectPtr<UStaticMeshComponent> Head;
	UPROPERTY() TObjectPtr<UMaterialInterface> Greybox;

	UPROPERTY(Transient) TObjectPtr<UInputMappingContext> Keys;
	UPROPERTY(Transient) TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> LookAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> JumpAction;
	UPROPERTY(Transient) TObjectPtr<UInputAction> RunAction;
};
