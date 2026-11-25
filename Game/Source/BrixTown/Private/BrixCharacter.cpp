#include "BrixCharacter.h"

#include "BrixCarsActor.h"
#include "BrixGameMode.h"
#include "BrixPeople.h"
#include "BrixTalkActor.h"
#include "BrixUnits.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Misc/Paths.h"
#include "ProceduralMeshComponent.h"
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

	PrimaryActorTick.bCanEverTick = true;
	Person = CreateDefaultSubobject<UProceduralMeshComponent>(TEXT("Person"));
	Person->SetupAttachment(RootComponent);
	Person->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	// feet at the capsule's bottom; the body is drawn facing the slice's yaw 0 (Unreal's -Y), turned to face the actor's +X
	Person->SetRelativeLocationAndRotation(FVector(0, 0, -85), FRotator(0, 90, 0));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> PeopleMat(TEXT("/Game/People/M_People.M_People"));
	BodyMaterial = PeopleMat.Object;
}

void ABrixCharacter::BeginPlay()
{
	Super::BeginPlay();
	GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * CmPerM;
	Boom->TargetArmLength = CameraDistance * CmPerM;
	Boom->SocketOffset = FVector(0, 0, CameraHeight * CmPerM);
	FString Error;
	if (!LoadBrixPeopleRules(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("Data/people.json"))), Rules, Error))
		UE_LOG(LogTemp, Error, TEXT("Brix Town: %s"), *Error);
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
	TalkAction = NewObject<UInputAction>(this, TEXT("Talk"));
	LeaveAction = NewObject<UInputAction>(this, TEXT("Leave"));
	for (int32 n = 0; n < 9; ++n) AnswerActions.Add(NewObject<UInputAction>(this, *FString::Printf(TEXT("Answer%d"), n + 1)));

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
	Keys->MapKey(TalkAction, EKeys::E);
	Keys->MapKey(LeaveAction, EKeys::Escape);
	const FKey Digits[9] = { EKeys::One, EKeys::Two, EKeys::Three, EKeys::Four, EKeys::Five, EKeys::Six, EKeys::Seven, EKeys::Eight, EKeys::Nine };
	const FKey Pad[9] = { EKeys::NumPadOne, EKeys::NumPadTwo, EKeys::NumPadThree, EKeys::NumPadFour, EKeys::NumPadFive, EKeys::NumPadSix, EKeys::NumPadSeven, EKeys::NumPadEight, EKeys::NumPadNine };
	for (int32 n = 0; n < 9; ++n) { Keys->MapKey(AnswerActions[n], Digits[n]); Keys->MapKey(AnswerActions[n], Pad[n]); }
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
		In->BindAction(JumpAction, ETriggerEvent::Started, this, &ABrixCharacter::JumpKey);
		In->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABrixCharacter::JumpEnd);
		In->BindAction(RunAction, ETriggerEvent::Started, this, &ABrixCharacter::StartRun);
		In->BindAction(RunAction, ETriggerEvent::Completed, this, &ABrixCharacter::StopRun);
		In->BindAction(TalkAction, ETriggerEvent::Started, this, &ABrixCharacter::TalkKey);
		In->BindAction(LeaveAction, ETriggerEvent::Started, this, &ABrixCharacter::LeaveKey);
		for (UInputAction* A : AnswerActions) In->BindAction(A, ETriggerEvent::Started, this, &ABrixCharacter::AnswerKey);
	}
}

void ABrixCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (bDriving) { DriveIn = V; return; }   // W/S the speed, A/D the steering
	if (!Controller || bTalking) return;   // a talk holds the player still (conversations.js open())
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void ABrixCharacter::Look(const FInputActionValue& Value)
{
	if (bTalking) return;
	const FVector2D V = Value.Get<FVector2D>();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(-V.Y);
}

void ABrixCharacter::StartRun() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed * CmPerM; }
void ABrixCharacter::StopRun() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * CmPerM; }

void ABrixCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules.Roles.Num()) return;
	if (bDriving)   // at the wheel: the car moves, the player sits in it, the camera swings round behind (cars.js drive())
		if (ABrixCars* C = Cars())
		{
			if (!TestHold.IsZero()) DriveIn = TestHold;
			C->Step(DeltaSeconds, DriveIn.Y, DriveIn.X, bHandbrake);
			DriveIn = FVector2D::ZeroVector;
			const FBrixLook& L = Rules.Player;
			FVector2D Seat;
			double Yaw = 0;
			float Up = 0, Floor = 0;
			const float Head = (L.Cap ? 1.06f : L.Hair == TEXT("afro") ? 1.12f : L.Hair == TEXT("bun") ? 1.06f : 1.04f) * (float)L.S;
			if (C->DriverSeat(Seat, Yaw, Up, Floor, Head))
			{
				SetActorLocationAndRotation(FVector(Seat.X * CmPerM, Seat.Y * CmPerM, Up * CmPerM + 85.f), FRotator(0, FMath::RadiansToDegrees(Yaw), 0));
				const double V = C->Driving().V;
				if (FMath::Abs(V) > .5)
					if (APlayerController* PC = Cast<APlayerController>(GetController()))
					{
						FRotator R = PC->GetControlRotation();
						const double Want = FMath::RadiansToDegrees(Yaw) + (V < 0 ? 180.0 : 0.0);
						R.Yaw += FMath::FindDeltaAngleDegrees(R.Yaw, Want) * FMath::Min(1.0, DeltaSeconds * 2.2);
						PC->SetControlRotation(R);
					}
				FBrixDrawOptions O;
				O.Pose = 2;
				O.Floor = Floor;
				O.bDriving = true;
				O.Seconds = GetWorld()->GetTimeSeconds();
				O.Lod = 0;
				Scratch.Reset();
				DrawBrixPerson(Rules, L, FVector::ZeroVector, 0.0, O, Scratch);
				Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
			}
			return;
		}
	// the slice's stepPlayer(): the phase runs on with the distance covered, by the Run signature when running
	const double Speed = GetVelocity().Size2D() / CmPerM;
	FBrixLook L = Rules.Player;
	if (Speed > (WalkSpeed + RunSpeed) * 0.5) L.SigName = TEXT("Run");
	Phase = FMath::Fmod(Phase + Speed * DeltaSeconds / BrixStepLength(Rules.Sig(L.SigName), L.S), 1.0);
	FBrixDrawOptions O;
	O.Pose = Speed > 0.1 ? 1 : 0;
	O.Phase = Phase;
	O.Seconds = GetWorld()->GetTimeSeconds();
	O.Lod = 0;
	Scratch.Reset();
	DrawBrixPerson(Rules, L, FVector::ZeroVector, 0.0, O, Scratch);
	Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
}

ABrixTalk* ABrixCharacter::Talk() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->Talk : nullptr;
}

void ABrixCharacter::TalkKey()   // E: talk, get in a car or get out of it, whichever is nearest (play.js interact())
{
	ABrixCars* C = Cars();
	ABrixTalk* T = Talk();
	if (bDriving && C)
	{
		FVector2D Spot;
		double Yaw = 0;
		C->Leave(Spot, Yaw);
		bDriving = false;
		SetActorLocationAndRotation(FVector(Spot.X * CmPerM, Spot.Y * CmPerM, 95.f), FRotator(0, FMath::RadiansToDegrees(Yaw), 0));
		SetActorEnableCollision(true);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Boom->TargetArmLength = CameraDistance * CmPerM;
		return;
	}
	if (T && T->IsTalking()) return;
	const FVector2D Me(GetActorLocation().X / CmPerM, GetActorLocation().Y / CmPerM);
	double CarD = 1e9;
	const int32 Car = C ? C->TargetCar(Me, CarD) : -1;
	const double TalkD = T && T->CandidateDistance() >= 0 ? T->CandidateDistance() : 1e9;
	if (Car >= 0 && CarD <= TalkD)
	{
		if (!C->Enter(Car)) return;
		bDriving = true;
		SetActorEnableCollision(false);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		Boom->TargetArmLength = FMath::Max(Boom->TargetArmLength, 8.5f * CmPerM);   // back a way, to see the car
		return;
	}
	if (T) T->Open();
}

ABrixCars* ABrixCharacter::Cars() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->Cars : nullptr;
}

void ABrixCharacter::JumpKey()
{
	if (bDriving) { bHandbrake = true; return; }
	if (!bTalking) Jump();
}

void ABrixCharacter::JumpEnd()
{
	bHandbrake = false;
	StopJumping();
}

void ABrixCharacter::AnswerKey(const FInputActionInstance& Instance)
{
	ABrixTalk* T = Talk();
	if (!T || !T->IsTalking()) return;
	const int32 n = AnswerActions.IndexOfByKey(Instance.GetSourceAction());
	if (n >= 0) T->Choose(n);
}

void ABrixCharacter::LeaveKey()
{
	if (ABrixTalk* T = Talk()) T->Close();
}

void ABrixCharacter::BeginTalk(const FVector& Them, float CameraM)
{
	bTalking = true;
	GetCharacterMovement()->StopMovementImmediately();
	const FVector To = Them - GetActorLocation();
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
	SetActorRotation(FRotator(0, Yaw, 0));
	ArmBefore = Boom->TargetArmLength;
	Boom->TargetArmLength = FMath::Min(ArmBefore, CameraM * CmPerM);   // come in closer so the face can be read
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->SetControlRotation(FRotator(-7.0, Yaw + 26.0, 0));   // look past the shoulder (the slice: yaw - .45, pitch -.12)
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
}

void ABrixCharacter::EndTalk()
{
	bTalking = false;
	if (ArmBefore > 0.f) Boom->TargetArmLength = ArmBefore;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}
