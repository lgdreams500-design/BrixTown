#include "BrixCharacter.h"

#include "BrixUnits.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/StaticMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "UObject/ConstructorHelpers.h"

using BrixUnits::CmPerM;

ABrixCharacter::ABrixCharacter()
{
	GetCapsuleComponent()->InitCapsuleSize(30.f, 85.f);   // a 1.7 m person
	bUseControllerRotationYaw = false;
	GetCharacterMovement()->bOrientRotationToMovement = true;
	GetCharacterMovement()->RotationRate = FRotator(0.f, 540.f, 0.f);
	GetCharacterMovement()->JumpZVelocity = 420.f;

	Boom = CreateDefaultSubobject<USpringArmComponent>(TEXT("Boom"));
	Boom->SetupAttachment(RootComponent);
	Boom->bUsePawnControlRotation = true;
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera"));
	Camera->SetupAttachment(Boom, USpringArmComponent::SocketName);

	static ConstructorHelpers::FObjectFinder<UStaticMesh> CylinderMesh(TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
	static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(TEXT("/Engine/BasicShapes/Sphere.Sphere"));
	Body = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Body"));
	Body->SetupAttachment(RootComponent);
	Body->SetStaticMesh(CylinderMesh.Object);
	Body->SetRelativeLocation(FVector(0, 0, -20));        // feet at the capsule's bottom (-85), shoulders at +45
	Body->SetRelativeScale3D(FVector(0.42f, 0.32f, 1.3f));
	Body->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Head = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("Head"));
	Head->SetupAttachment(RootComponent);
	Head->SetStaticMesh(SphereMesh.Object);
	Head->SetRelativeLocation(FVector(0, 0, 62));
	Head->SetRelativeScale3D(FVector(0.27f));
	Head->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> GreyboxMat(TEXT("/Game/Greybox/M_Greybox.M_Greybox"));
	Greybox = GreyboxMat.Object;
}

void ABrixCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * CmPerM;
	Boom->TargetArmLength = CameraDistance * CmPerM;
	Boom->SocketOffset = FVector(0, 0, CameraHeight * CmPerM);
	for (auto [Part, Colour] : { TPair<UStaticMeshComponent*, FLinearColor>(Body, BodyColour), TPair<UStaticMeshComponent*, FLinearColor>(Head, HeadColour) })
	{
		if (UMaterialInstanceDynamic* Mat = Part->CreateDynamicMaterialInstance(0, Greybox)) Mat->SetVectorParameterValue(TEXT("Color"), Colour);
	}
}

void ABrixCharacter::MakeInput()
{
	if (Keys) return;
	Keys = NewObject<UInputMappingContext>(this, TEXT("BrixKeys"));
	MoveAction = NewObject<UInputAction>(this, TEXT("Move"));
	MoveAction->ValueType = EInputActionValueType::Axis2D;
	LookAction = NewObject<UInputAction>(this, TEXT("Look"));
	LookAction->ValueType = EInputActionValueType::Axis2D;
	JumpAction = NewObject<UInputAction>(this, TEXT("Jump"));
	RunAction = NewObject<UInputAction>(this, TEXT("Run"));

	// Move is (right, forward): D and A on x, W and S on y.
	auto Map = [this](UInputAction* Action, FKey Key, bool bToY, bool bNegate)
	{
		FEnhancedActionKeyMapping& M = Keys->MapKey(Action, Key);
		if (bToY)
		{
			UInputModifierSwizzleAxis* Swap = NewObject<UInputModifierSwizzleAxis>(Keys);
			Swap->Order = EInputAxisSwizzle::YXZ;
			M.Modifiers.Add(Swap);
		}
		if (bNegate) M.Modifiers.Add(NewObject<UInputModifierNegate>(Keys));
	};
	Map(MoveAction, EKeys::W, true, false);
	Map(MoveAction, EKeys::S, true, true);
	Map(MoveAction, EKeys::D, false, false);
	Map(MoveAction, EKeys::A, false, true);
	Keys->MapKey(LookAction, EKeys::Mouse2D);
	Keys->MapKey(JumpAction, EKeys::SpaceBar);
	Keys->MapKey(RunAction, EKeys::LeftShift);
}

void ABrixCharacter::PawnClientRestart()
{
	Super::PawnClientRestart();
	MakeInput();
	if (const APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Input = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PC->GetLocalPlayer()))
		{
			Input->AddMappingContext(Keys, 0);
		}
	}
}

void ABrixCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
	Super::SetupPlayerInputComponent(PlayerInputComponent);
	MakeInput();
	if (UEnhancedInputComponent* In = Cast<UEnhancedInputComponent>(PlayerInputComponent))
	{
		In->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABrixCharacter::Move);
		In->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABrixCharacter::Look);
		In->BindAction(JumpAction, ETriggerEvent::Started, this, &ACharacter::Jump);
		In->BindAction(JumpAction, ETriggerEvent::Completed, this, &ACharacter::StopJumping);
		In->BindAction(RunAction, ETriggerEvent::Started, this, &ABrixCharacter::StartRun);
		In->BindAction(RunAction, ETriggerEvent::Completed, this, &ABrixCharacter::StopRun);
	}
}

void ABrixCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (!Controller) return;
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void ABrixCharacter::Look(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(-V.Y);
}

void ABrixCharacter::StartRun() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed * CmPerM; }
void ABrixCharacter::StopRun() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * CmPerM; }
