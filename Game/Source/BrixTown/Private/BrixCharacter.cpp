#include "BrixCharacter.h"
#include "Framework/Application/SlateApplication.h"
#include "BrixMallActor.h"

#include "BrixCarsActor.h"
#include "BrixCity.h"
#include "BrixGameMode.h"
#include "BrixLifeActor.h"
#include "BrixMapActor.h"
#include "BrixLook.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Text/STextBlock.h"
#include "BrixPeople.h"
#include "BrixRooms.h"
#include "BrixTalkActor.h"
#include "BrixUnits.h"
#include "BrixWeatherActor.h"
#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/CapsuleComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
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
#include "BrixTubesActor.h"
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
	Person->LightingChannels.bChannel1 = true;   // a hall's light for people (ABrixMall::PeopleLight)
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
	BrixAddClimbSignature(Rules);   // up a slide's ladder or a climbing frame (dressing.js climbSig(), WLD-041)
	if (!LoadBrixBoards(FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::ProjectDir(), TEXT("../World/rules/boards.json"))), BoardRules, Error))
		UE_LOG(LogTemp, Error, TEXT("Brix Town: no skateboards (%s)"), *Error);
	Boards.R = &BoardRules;
	if ((FParse::Param(FCommandLine::Get(), TEXT("BrixBoards")) || FCommandLine::Get() && FCString::Strifind(FCommandLine::Get(), TEXT("-BrixShot="))) && !FParse::Param(FCommandLine::Get(), TEXT("BrixNoBoards")))
		Boards.Give(BoardRules.AutoOwned, BoardRules.AutoEquipped);   // CITY-100: a test owns every kind, the electric one in use (the slice's ?auto=1)
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
	CounterAction = NewObject<UInputAction>(this, TEXT("Counter"));
	WalletAction = NewObject<UInputAction>(this, TEXT("Wallet"));
	QuestsAction = NewObject<UInputAction>(this, TEXT("Quests"));
	BoardAction = NewObject<UInputAction>(this, TEXT("Board"));
	WeatherAction = NewObject<UInputAction>(this, TEXT("Weather"));
	SkipMinutesAction = NewObject<UInputAction>(this, TEXT("SkipMinutes"));
	SkipHourAction = NewObject<UInputAction>(this, TEXT("SkipHour"));
	SkipWeatherAction = NewObject<UInputAction>(this, TEXT("SkipWeather"));
	PageNames = { TEXT("me"), TEXT("belongings"), TEXT("property"), TEXT("wardrobe"), TEXT("map"), TEXT("help") };   // settings.json actions
	for (const FString& N : PageNames) PageActions.Add(NewObject<UInputAction>(this, *(TEXT("Page_") + N)));
	for (UInputAction* A : PageActions) A->bTriggerWhenPaused = true;   // the menu stops the clock; its keys still work
	LeaveAction->bTriggerWhenPaused = true;
	WalletAction->bTriggerWhenPaused = true;
	QuestsAction->bTriggerWhenPaused = true;
	for (const TCHAR* N : { TEXT("ChooseLeft"), TEXT("ChooseRight"), TEXT("ChooseUp"), TEXT("ChooseDown"), TEXT("ChooseEnter") }) ChooseActions.Add(NewObject<UInputAction>(this, N));
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
	Keys->MapKey(CounterAction, EKeys::B);
	Keys->MapKey(WalletAction, EKeys::I);
	Keys->MapKey(QuestsAction, EKeys::J);
	Keys->MapKey(BoardAction, EKeys::Y);   // settings.json: board, Y
	Keys->MapKey(WeatherAction, EKeys::K);   // settings.json: weather, K
	Keys->MapKey(SkipMinutesAction, EKeys::Z);   // settings.json: minutes, Z (WLD-073)
	Keys->MapKey(SkipHourAction, EKeys::T);      // settings.json: hour, T
	Keys->MapKey(SkipWeatherAction, EKeys::X);   // settings.json: next_weather, X
	const FKey PageKeys[6] = { EKeys::C, EKeys::G, EKeys::P, EKeys::U, EKeys::M, EKeys::H };
	for (int32 k = 0; k < 6; ++k) Keys->MapKey(PageActions[k], PageKeys[k]);
	Keys->MapKey(ChooseActions[0], EKeys::Left);
	Keys->MapKey(ChooseActions[1], EKeys::Right);
	Keys->MapKey(ChooseActions[2], EKeys::Up);
	Keys->MapKey(ChooseActions[3], EKeys::Down);
	Keys->MapKey(ChooseActions[4], EKeys::Enter);
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
		In->BindAction(CounterAction, ETriggerEvent::Started, this, &ABrixCharacter::CounterKey);
		In->BindAction(WalletAction, ETriggerEvent::Started, this, &ABrixCharacter::WalletKey);
		In->BindAction(QuestsAction, ETriggerEvent::Started, this, &ABrixCharacter::QuestsKey);
		In->BindAction(BoardAction, ETriggerEvent::Started, this, &ABrixCharacter::BoardKey);
		In->BindAction(WeatherAction, ETriggerEvent::Started, this, &ABrixCharacter::WeatherKey);
		In->BindAction(SkipMinutesAction, ETriggerEvent::Started, this, &ABrixCharacter::SkipMinutesKey);
		In->BindAction(SkipHourAction, ETriggerEvent::Started, this, &ABrixCharacter::SkipHourKey);
		In->BindAction(SkipWeatherAction, ETriggerEvent::Started, this, &ABrixCharacter::SkipWeatherKey);
		for (UInputAction* A : PageActions) In->BindAction(A, ETriggerEvent::Started, this, &ABrixCharacter::MenuPageKey);
		for (UInputAction* A : ChooseActions) In->BindAction(A, ETriggerEvent::Started, this, &ABrixCharacter::ChooseKey);
		for (UInputAction* A : AnswerActions) In->BindAction(A, ETriggerEvent::Started, this, &ABrixCharacter::AnswerKey);
	}
}

void ABrixCharacter::Move(const FInputActionValue& Value)
{
	const FVector2D V = Value.Get<FVector2D>();
	if (IsPlaying()) { if (!V.IsNearlyZero()) bPlayOff = true; return; }   // moving gets off what you play on
	if (bSeated) { if (!V.IsNearlyZero()) bSeatOff = true; return; }      // and up from a seat
	if (bDriving) { DriveIn = V; return; }   // W/S the speed, A/D the steering
	if (!Controller || bTalking || bPanel) return;   // a talk or a counter holds the player still (conversations.js open())
	if (Boards.Riding()) { BoardIn = V; return; }    // on a board there is no walk: W goes, S brakes, A and D turn
	const FRotator Yaw(0.f, Controller->GetControlRotation().Yaw, 0.f);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::X), V.Y);
	AddMovementInput(FRotationMatrix(Yaw).GetUnitAxis(EAxis::Y), V.X);
}

void ABrixCharacter::Look(const FInputActionValue& Value)
{
	if (bTalking || bPanel || (bDesigning && bDesignKeys)) return;
	const FVector2D V = Value.Get<FVector2D>() * LookScale;
	if (!V.IsNearlyZero(.01)) LookedAt = GetWorld()->GetTimeSeconds();
	AddControllerYawInput(V.X);
	AddControllerPitchInput(bInvertLook ? V.Y : -V.Y);
}

void ABrixCharacter::StartRun() { GetCharacterMovement()->MaxWalkSpeed = RunSpeed * CmPerM; }
void ABrixCharacter::StopRun() { GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * CmPerM; }

bool ABrixCharacter::CameraFree() const { return GetWorld()->GetTimeSeconds() - LookedAt > LookHoldSeconds; }

void ABrixCharacter::RideCamera(float DeltaSeconds)
{
	if (RideArm <= 0.f) return;
	const FVector From = Boom->GetComponentLocation();
	const FVector To = From - GetControlRotation().Vector() * RideArm + FVector(0, 0, Boom->SocketOffset.Z);
	FHitResult Hit;
	FCollisionQueryParams Q(TEXT("BrixRideCamera"), false, this);
	float Want = RideArm;
	if (GetWorld()->SweepSingleByChannel(Hit, From, To, FQuat::Identity, ECC_Camera, FCollisionShape::MakeSphere(12.f), Q))
	{
		RideBlocked += DeltaSeconds;
		if (RideBlocked >= RideBlockSeconds) Want = FMath::Max(150.f, RideArm * Hit.Time - 20.f);
		else Want = Boom->TargetArmLength;   // not yet: stay
	}
	else RideBlocked = 0.f;
	Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, Want, DeltaSeconds, Want < Boom->TargetArmLength ? 8.f : 2.f);
}

void ABrixCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);
	if (!Rules.Roles.Num()) return;
	if (bDriving) RideCamera(DeltaSeconds);
	{
		// the camera pushed in against the player by a wall looks out through an undrawn body, not from inside one
		const bool bSeen = bDesigning || FVector::Dist(Camera->GetComponentLocation(), GetActorLocation() + FVector(0, 0, 60)) > HideBodyWithin * CmPerM;
		if (Person->IsVisible() != bSeen) Person->SetVisibility(bSeen);
	}
	if (const ABrixGameMode* M = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))   // wet clothes out in the rain (WLD-037)
	{
		const ABrixWeather* Wx = M->Weather.Get();
		const bool bIn = bDesigning || bDriving || (Wx && Wx->Roofed(FVector2D(GetActorLocation()) / CmPerM));   // indoors or in a vehicle: dry
		const float W = !bIn && M->PeopleActor() ? M->PeopleActor()->WetClothes() : 0.f;
		if (FMath::Abs(W - PersonWet) > .005f) { Person->SetCustomPrimitiveDataFloat(1, W); PersonWet = W; }
	}
	if (bDesigning)   // the designer: standing, breathing and blinking while the clock stands still; the camera close in front
	{
		const FBrixLook& L = Rules.Player;
		const float S = (float)L.S, D = DesignM * S * CmPerM;
		Boom->TargetArmLength = D;
		const float Half = FMath::DegreesToRadians(Camera->FieldOfView * .5f);
		Boom->SocketOffset = FVector(0, -D * DesignShare * FMath::Tan(Half), (.95f * S + .18f) * CmPerM - 85.f);   // the free part's middle
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
			PC->SetControlRotation(FRotator(-4.0, GetActorRotation().Yaw + 180.0 + DesignSpin, 0));
		FBrixDrawOptions O;
		O.Seconds = GetWorld()->GetRealTimeSeconds();
		O.Lod = 0;
		Scratch.Reset();
		DrawBrixPerson(Rules, L, FVector::ZeroVector, 0.0, O, Scratch);
		Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
		if (GetWorld()->IsPaused())   // a paused world does not move the camera itself
			if (APlayerController* PC = Cast<APlayerController>(GetController())) if (PC->PlayerCameraManager) PC->PlayerCameraManager->UpdateCamera(DeltaSeconds);
		return;
	}
	if (bTubeRiding || TubeBackLeft > 0) { TubeTick(DeltaSeconds); if (bTubeRiding) return; }   // in a glass tube (CITY-109)
	if (IsPlaying())   // on a swing, seesaw, slide, sandpit or climbing frame: where the go puts you (dressing.js playerAt())
	{
		ABrixPeople* Pp = People();
		if (!Pp) { PlayUse = INDEX_NONE; return; }
		const FBrixPlayPose P = Pp->PlayArea().PlayerAt(PlayUse, PlayT0, GetWorld()->GetTimeSeconds());
		if (bPlayOff || P.bDone) { PlayEnd = FVector2D(P.X, P.Z); StopPlay(P.bDone); return; }
		SetActorLocationAndRotation(FVector(P.X * CmPerM, P.Z * CmPerM, P.Y * CmPerM + 85.f), FRotator(0, FMath::RadiansToDegrees(P.Yaw) - 90.0, 0));   // (the body is drawn turned a quarter in the actor)
		FBrixDrawOptions O;
		O.Pose = P.Pose;
		O.Phase = P.Phase;
		O.bStraddle = P.bStraddle;
		if (P.bClimb) O.Sig = TEXT("Climb");
		O.Arms[0] = P.Arms[0]; O.Arms[1] = P.Arms[1];
		O.Seconds = GetWorld()->GetTimeSeconds();
		O.Lod = 0;
		Scratch.Reset();
		DrawBrixPerson(Rules, Rules.Player, FVector::ZeroVector, 0.0, O, Scratch);
		Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
		return;
	}
	if (bSeated)   // on a sofa, a chair, a bench or a bed: the body sits (or lies) on its top, facing as the piece does
	{
		if (bSeatOff) { StopSeat(); return; }
		SetActorLocationAndRotation(FVector(SeatAt.X * CmPerM, SeatAt.Y * CmPerM, SeatAt.Z * CmPerM + 85.f), FRotator(0, FMath::RadiansToDegrees(SeatYaw) - 90.0, 0));   // (the body is drawn turned a quarter in the actor)
		FBrixDrawOptions O;
		O.Pose = bSeatLie ? 3 : 2;
		O.Seconds = GetWorld()->GetTimeSeconds();
		O.Lod = 0;
		Scratch.Reset();
		DrawBrixPerson(Rules, Rules.Player, FVector::ZeroVector, 0.0, O, Scratch);
		Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
		return;
	}
	if (bDriving)   // on a bus or the tram: carried where it goes, sitting near the front (cars.js ride()) or on the tram's bench
		if (ABrixCars* C = Cars())
			if (C->Driving().bRide)
			{
				FVector2D Seat;
				double Yaw = 0;
				if (!C->RideSeat(Seat, Yaw)) { TalkKey(); return; }   // the bus has left service: everybody off
				SetActorLocationAndRotation(FVector(Seat.X * CmPerM, Seat.Y * CmPerM, C->RideUp() * CmPerM + 85.f), FRotator(0, FMath::RadiansToDegrees(Yaw), 0));
				if (C->Driving().bMoving && CameraFree())
					if (APlayerController* PC = Cast<APlayerController>(GetController()))
					{
						FRotator R = PC->GetControlRotation();
						R.Yaw += FMath::FindDeltaAngleDegrees(R.Yaw, FMath::RadiansToDegrees(Yaw)) * FMath::Min(1.0, DeltaSeconds * 1.5);
						PC->SetControlRotation(R);
					}
				FBrixDrawOptions O;
				O.Pose = 2;
				O.Seconds = GetWorld()->GetTimeSeconds();
				O.Lod = 0;
				Scratch.Reset();
				DrawBrixPerson(Rules, Rules.Player, FVector::ZeroVector, 0.0, O, Scratch);
				Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
				return;
			}
	if (bDriving)   // at the wheel: the car moves, the player sits in it, the camera swings round behind (cars.js drive())
		if (ABrixCars* C = Cars())
		{
			if (!TestHold.IsZero()) DriveIn = TestHold;
			C->Step(DeltaSeconds, DriveIn.Y, DriveIn.X, bHandbrake);
			DriveUsed = DriveIn;
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
				if (FMath::Abs(V) > .5 && CameraFree())
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
	if (Boards.Riding()) { StepBoard(DeltaSeconds); return; }
	// the slice's stepPlayer(): the phase runs on with the distance covered, by the Run signature when running
	const double Speed = GetVelocity().Size2D() / CmPerM;
	FBrixLook L = Rules.Player;
	if (Speed > (WalkSpeed + RunSpeed) * 0.5) L.SigName = TEXT("Run");
	Phase = FMath::Fmod(Phase + Speed * DeltaSeconds / BrixStepLength(Rules.Sig(L.SigName), L.S), 1.0);
	if (!bTalking && !bDesigning)   // nearer in a room
	{
		const ABrixGameMode* GM = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
		const bool bInRoom = GM && GM->RoomAt(GetActorLocation()) != INDEX_NONE;
		const float Arm = (bInRoom ? FMath::Min(CameraDistance, IndoorCameraDistance) : CameraDistance) * CmPerM;
		if (!FMath::IsNearlyEqual(Boom->TargetArmLength, Arm, .5f)) Boom->TargetArmLength = FMath::FInterpTo(Boom->TargetArmLength, Arm, DeltaSeconds, 5.f);
	}
	if (bTalking)   // the camera eases between your shoulder and your face
		if (APlayerController* PC = Cast<APlayerController>(GetController()))
		{
			FRotator R = PC->GetControlRotation();
			R.Yaw += FMath::FindDeltaAngleDegrees(R.Yaw, TalkYaw + (bAnswering ? AnswerCameraYaw : TalkCameraYaw)) * FMath::Min(1.0, DeltaSeconds * 3.0);
			R.Pitch += FMath::FindDeltaAngleDegrees(R.Pitch, bAnswering ? -4.0 : -7.0) * FMath::Min(1.0, DeltaSeconds * 3.0);
			PC->SetControlRotation(R);
		}
	FBrixDrawOptions O;
	O.Pose = Speed > 0.1 ? 1 : 0;
	O.Phase = Phase;
	if (bTalking) O.Face = TalkFace;
	else if (const APlayerController* PC = Cast<APlayerController>(GetController()))   // the head turned part of the way to where the camera looks (play.js; ANIM-005)
	{
		const FRotator R = PC->GetControlRotation();
		O.Look = BrixLookAlong(FMath::DegreesToRadians(FMath::FindDeltaAngleDegrees(GetActorRotation().Yaw, R.Yaw)), FMath::DegreesToRadians(FRotator::NormalizeAxis(R.Pitch)));
	}
	O.Seconds = GetWorld()->GetTimeSeconds();
	O.Lod = 0;
	O.Board = Boards.Kind();   // the board in use, carried in the right hand (CITY-099)
	O.bCarry = true;
	Scratch.Reset();
	DrawBrixPerson(Rules, L, FVector::ZeroVector, 0.0, O, Scratch);
	Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
}

ABrixPeople* ABrixCharacter::People() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->PeopleActor() : nullptr;
}

void ABrixCharacter::StartPlay(int32 Use)
{
	ABrixPeople* Pp = People();
	if (!Pp || !Pp->PlayArea().PlayerUses.IsValidIndex(Use)) return;
	PlayUse = Use;
	PlayT0 = GetWorld()->GetTimeSeconds();
	bPlayOff = false;
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the player has a go (%s)"), *Pp->PlayArea().PlayerUses[Use].Label);
}

void ABrixCharacter::StopPlay(bool bAtEnd)
{
	if (!IsPlaying()) return;
	ABrixPeople* Pp = People();
	const int32 U = PlayUse;
	PlayUse = INDEX_NONE;
	bPlayOff = false;
	if (!Pp || !Pp->PlayArea().PlayerUses.IsValidIndex(U))   // the piece is gone: still back on your feet
	{
		SetActorEnableCollision(true);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		return;
	}
	const FVector2D Off = bAtEnd ? PlayEnd : Pp->PlayArea().PlayerOff(U);   // off at the slide's bottom, or beside the piece
	SetActorLocation(FVector(Off.X * CmPerM, Off.Y * CmPerM, Pp->PlayArea().PlayerUses[U].Y * CmPerM + 95.f), false, nullptr, ETeleportType::TeleportPhysics);
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

const ABrixMall* ABrixCharacter::SeatHere(int32& OutSeat, double& OutMetres) const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	const ABrixMall* Best = nullptr;
	OutSeat = INDEX_NONE;
	OutMetres = 1e9;
	if (!Mode || bDriving || bSeated || IsPlaying()) return nullptr;
	for (const ABrixMall* Hall : { Mode->Mall.Get(), Mode->OutletHall.Get() })
	{
		double D = 0;
		const int32 i = Hall ? Hall->SeatNear(GetActorLocation(), D) : INDEX_NONE;
		if (i != INDEX_NONE && D < OutMetres) { OutMetres = D; OutSeat = i; Best = Hall; }
	}
	return Best;
}

bool ABrixCharacter::SeatOffer(FString& OutLabel, double& OutMetres) const
{
	int32 Se = INDEX_NONE;
	const ABrixMall* Hall = SeatHere(Se, OutMetres);
	if (!Hall) return false;
	OutLabel = Hall->Seats()[Se].Label;
	return true;
}

void ABrixCharacter::StartSeat(const FVector& At, double Yaw, bool bLie, const FString& Label)
{
	bSeated = true;
	bSeatLie = bLie;
	bSeatOff = false;
	SeatAt = At;
	SeatYaw = Yaw;
	SeatFrom = GetActorLocation();
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	UE_LOG(LogTemp, Display, TEXT("Brix Town: the player %s (%s) at %.2f, %.2f, %.2f m up"), bLie ? TEXT("lies down") : TEXT("sits down"), *Label, At.X, At.Y, At.Z);
}

void ABrixCharacter::StopSeat()
{
	if (!bSeated) return;
	bSeated = bSeatOff = false;
	SetActorLocation(SeatFrom, false, nullptr, ETeleportType::TeleportPhysics);   // up, where they stood
	SetActorEnableCollision(true);
	GetCharacterMovement()->SetMovementMode(MOVE_Walking);
}

void ABrixCharacter::TestSeat(const FString& LabelPart)
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	for (const ABrixMall* Hall : { Mode ? Mode->Mall.Get() : nullptr, Mode ? Mode->OutletHall.Get() : nullptr })
		for (int32 i = 0; Hall && i < Hall->Seats().Num(); ++i)
		{
			const ABrixMall::FSeat& X = Hall->Seats()[i];
			if (!X.Label.Contains(LabelPart)) continue;
			// stand a step in front of it, facing it, as the player would come to it
			const FVector2D Fwd(FMath::Sin(X.Yaw), -FMath::Cos(X.Yaw));
			SetActorLocation(FVector((X.At.X + Fwd.X * .8) * CmPerM, (X.At.Y + Fwd.Y * .8) * CmPerM, (X.At.Z - .45) * CmPerM + 95.f), false, nullptr, ETeleportType::TeleportPhysics);
			if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->SetControlRotation(FRotator(-18.0, FMath::RadiansToDegrees(FMath::Atan2(-Fwd.Y, -Fwd.X)) + 35.0, 0));
			StartSeat(X.At, X.Yaw, X.bLie, X.Label);
			return;
		}
	UE_LOG(LogTemp, Warning, TEXT("Brix Town: no seat whose offer says %s"), *LabelPart);
}

void ABrixCharacter::TestPlay(int32 Use)
{
	ABrixPeople* Pp = People();
	if (!Pp || !Pp->PlayArea().PlayerUses.IsValidIndex(Use)) { UE_LOG(LogTemp, Warning, TEXT("Brix Town: no play place %d"), Use); return; }
	const FBrixPlay::FPlayerUse& U = Pp->PlayArea().PlayerUses[Use];
	SetActorLocation(FVector(U.X * CmPerM, U.Z * CmPerM, U.Y * CmPerM + 95.f), false, nullptr, ETeleportType::TeleportPhysics);
	TalkKey();   // as the player would, at the place
}

ABrixTalk* ABrixCharacter::Talk() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->Talk : nullptr;
}

void ABrixCharacter::TalkKey()   // E: talk, get in a car or get out of it, whichever is nearest (play.js interact())
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	if (bTubeRiding) return;   // the rider has no say during a ride (CITY-111)
	if (IsPlaying()) { StopPlay(); return; }
	if (bSeated) { StopSeat(); return; }
	ABrixCars* C = Cars();
	ABrixTalk* T = Talk();
	if (bDriving && C)
	{
		if (!C->CanGetOff()) return;   // off a bus only where it stands at a stop
		FVector2D Spot;
		double Yaw = 0;
		C->Leave(Spot, Yaw);
		bDriving = false;
		Boom->bDoCollisionTest = true;
		SetActorLocationAndRotation(FVector(Spot.X * CmPerM, Spot.Y * CmPerM, 95.f), FRotator(0, FMath::RadiansToDegrees(Yaw), 0));
		SetActorEnableCollision(true);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Boom->TargetArmLength = CameraDistance * CmPerM;
		return;
	}
	if (bPanel) return;   // a counter, the menu or the map is open: E started a drive or a talk under it, with the camera held
	if (T && T->IsTalking()) return;
	if (ABrixTubes* Tb = Tubes())   // in a tube stop's booth: ride to the stop chosen (CITY-111)
	{
		const FString From = Tb->StopAt(FVector2D(GetActorLocation()) / CmPerM);
		if (!From.IsEmpty()) { StartTube(Tb->Choice(From, TubePickAt == From ? TubePick : 0)); return; }
	}
	if (const ABrixGameMode* M = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))   // a tower's lift call within reach (WLD-051)
		if (M->RoomsActor() && M->RoomsActor()->UseLift(this)) return;
	const FVector2D Me(GetActorLocation().X / CmPerM, GetActorLocation().Y / CmPerM);
	double CarD = 1e9;
	const int32 Car = C ? C->TargetCar(Me, CarD) : -1;
	const double TalkD = T && T->CandidateDistance() >= 0 ? T->CandidateDistance() : 1e9;
	double PlayD = 1e9;
	const int32 PlayAt = !bDriving && !Boards.Riding() && People() ? People()->PlayTarget(GetActorLocation(), PlayD) : INDEX_NONE;
	if (PlayAt != INDEX_NONE && PlayD <= TalkD && (Car < 0 || PlayD <= CarD)) { StartPlay(PlayAt); return; }   // a go on the playground (WLD-041)
	if (!Boards.Riding())   // a seat or a bed to try (FURN-022): nearer than anyone to talk to
	{
		int32 Se = INDEX_NONE;
		double SeatD = 1e9;
		if (const ABrixMall* Hall = SeatHere(Se, SeatD)) if (SeatD <= TalkD) { const ABrixMall::FSeat& X = Hall->Seats()[Se]; StartSeat(X.At, X.Yaw, X.bLie, X.Label); return; }
	}
	if (Car >= 0 && CarD <= TalkD)
	{
		if (!C->Enter(Car)) return;
		bDriving = true;
		Boom->bDoCollisionTest = false;   // 8.5 m behind a car the arm met every lamp post and corner passed, and the camera jumped in and out (RideCamera)
		SetActorEnableCollision(false);
		GetCharacterMovement()->StopMovementImmediately();
		GetCharacterMovement()->DisableMovement();
		Boom->TargetArmLength = FMath::Max(Boom->TargetArmLength, (ABrixCars::IsTram(Car) ? 17.f : C->IsBus(Car) ? 15.f : 8.5f) * CmPerM);   // back a way, to see the car, the bus or the tram (a bus is 11 m long: from 11 m behind a seat at its front the camera sat over its roof; further back and flatter it is behind the bus and under the trees' crowns, which do not stop it)
		RideArm = Boom->TargetArmLength;
		RideBlocked = 0.f;
		if (APlayerController* PC = Cast<APlayerController>(GetController()))   // looking along the car, a little down on it
			PC->SetControlRotation(FRotator(ABrixCars::IsTram(Car) || C->IsBus(Car) ? -5.0 : -12.0, FMath::RadiansToDegrees(C->Driving().A), 0));
		return;
	}
	if (T) T->Open();
}

void ABrixCharacter::WeatherKey()   // K: the next kind of weather to look at, for the whole day (play.js wxTry)
{
	if ((bDesigning && bDesignKeys) || bTalking || bPanel) return;
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	ABrixWeather* Wx = Mode ? Mode->Weather.Get() : nullptr;
	if (!Wx || !Wx->bOn) return;
	const FString Now = Wx->TryNext();
	if (ABrixLife* L = Life()) L->Toast(Now.IsEmpty() ? FString(TEXT("The day's own weather")) : TEXT("Trying ") + Now);
}

void ABrixCharacter::SkipKey(const FString& Step)   // WLD-073, WLD-074: ask the server to move the clock
{
	if ((bDesigning && bDesignKeys) || bPanel) return;
	ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode) return;   // only the server has the game mode; there is no network play yet
	TArray<FString> Busy;
	if (bTalking) Busy.Add(TEXT("conversation"));
	if (bTubeRiding) Busy.Add(TEXT("ride"));
	const FString Why = Mode->SkipTime(Step, Cast<APlayerController>(GetController()), Busy);
	ABrixLife* L = Life();
	if (!L) return;
	if (Why.IsEmpty())
	{
		const int32 M = (int32)FMath::Fmod(Mode->ClockMinutes(), 1440.0);
		L->Toast(FString::Printf(TEXT("%s: now %02d:%02d"), Step == TEXT("weather") ? TEXT("To the next weather") : (Step == TEXT("1h") ? TEXT("An hour on") : TEXT("10 minutes on")), M / 60, M % 60));
	}
	else if (Why == TEXT("not_host")) L->Toast(TEXT("Only the host can skip time"));
	else if (Why.StartsWith(TEXT("busy:"))) L->Toast(TEXT("Not now"));
}

ABrixCars* ABrixCharacter::Cars() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->Cars : nullptr;
}

void ABrixCharacter::JumpKey()
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	if (bDriving) { bHandbrake = true; return; }
	if (IsPlaying()) { bPlayOff = true; return; }
	if (bSeated) { bSeatOff = true; return; }
	if (Boards.Riding()) return;   // no jumping off a board: Y steps off it
	if (!bTalking) Jump();
}

void ABrixCharacter::JumpEnd()
{
	bHandbrake = false;
	StopJumping();
}

void ABrixCharacter::AnswerKey(const FInputActionInstance& Instance)
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	ABrixTalk* T = Talk();
	if (!T || !T->IsTalking()) return;
	const int32 n = AnswerActions.IndexOfByKey(Instance.GetSourceAction());
	if (n >= 0) T->Choose(n);
}

void ABrixCharacter::LeaveKey()
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this))) if (Mode->Map && Mode->Map->IsOpen()) { Mode->Map->Close(); return; }   // Esc closes the big map first (play.js)
	if (ABrixLife* L = Life()) if (L->IsOpen()) { L->Close(); return; }
	if (ABrixTalk* T = Talk()) if (T->IsTalking()) { T->Close(); return; }
	if (bDriving) return;
	if (ABrixLife* L = Life()) L->Menu(FString());   // Esc with nothing open: the menu's home (UI-005)
}

ABrixLife* ABrixCharacter::Life() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode ? Mode->Life : nullptr;
}

void ABrixCharacter::CounterKey()
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	if (bDriving || bTalking) return;
	if (ABrixLife* L = Life()) L->CounterKey();
}

void ABrixCharacter::WalletKey()
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	if (bDriving || bTalking) return;
	if (ABrixLife* L = Life()) L->WalletKey();
}

void ABrixCharacter::ChooseKey(const FInputActionInstance& Instance)
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	ABrixLife* L = Life();
	const int32 n = ChooseActions.IndexOfByKey(Instance.GetSourceAction());
	if ((!L || !L->IsShop()) && !bTubeRiding && (n == 0 || n == 1))   // in a tube booth: the arrows choose the stop to ride to
		if (ABrixTubes* Tb = Tubes())
		{
			const FString From = Tb->StopAt(FVector2D(GetActorLocation()) / CmPerM);
			if (!From.IsEmpty())
			{
				if (TubePickAt != From) { TubePickAt = From; TubePick = 0; }
				TubePick += n == 0 ? -1 : 1;
				return;
			}
		}
	if (!L || !L->IsShop()) return;
	if (n == 4) L->EnterKey();
	else if (n >= 0) L->ArrowKey(n == 0 ? -1 : n == 1 ? 1 : 0, n == 2 ? -1 : n == 3 ? 1 : 0);
}

void ABrixCharacter::MenuPageKey(const FInputActionInstance& Instance)
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	if (bDriving || bTalking) return;
	const int32 n = PageActions.IndexOfByKey(Instance.GetSourceAction());
	if (ABrixLife* L = Life()) if (PageNames.IsValidIndex(n)) L->MenuKey(PageNames[n]);   // M: the menu's Map page is the big map (ABrixLife::Menu)
}

void ABrixCharacter::SetView(float CameraM, float FovDeg, float LookSpeed, bool bInvertY)
{
	CameraDistance = CameraM;
	if (!bDriving && !bTalking) Boom->TargetArmLength = CameraM * CmPerM;
	FVector2D Vp(16, 9);
	if (GEngine && GEngine->GameViewport) GEngine->GameViewport->GetViewportSize(Vp);
	const float Aspect = Vp.Y > 0 ? (float)(Vp.X / Vp.Y) : 16.f / 9.f;   // the slice's degrees are up and down, the engine's across
	Camera->SetFieldOfView(FMath::RadiansToDegrees(2.f * FMath::Atan(FMath::Tan(FMath::DegreesToRadians(FovDeg) / 2.f) * Aspect)));
	LookScale = LookSpeed;
	bInvertLook = bInvertY;
}

void ABrixCharacter::QuestsKey()
{
	if (bDesigning && bDesignKeys) return;   // the designer has the keys
	if (bDriving || bTalking) return;
	if (ABrixLife* L = Life()) L->QuestsKey();
}

void ABrixCharacter::SetDesignView(bool bOn, double SpinDeg, float PanelShare, bool bTakeKeys, float CameraM)
{
	DesignM = CameraM > 0.f ? CameraM : DesignCameraM;
	bDesignKeys = bTakeKeys;
	DesignSpin = SpinDeg;
	DesignShare = PanelShare;
	if (bOn == bDesigning) return;
	bDesigning = bOn;
	SetTickableWhenPaused(bOn);   // the game is paused while designing; the character and the camera's arm are not
	Boom->SetTickableWhenPaused(bOn);
	Boom->bDoCollisionTest = !bOn;
	if (bOn) { ArmBefore = Boom->TargetArmLength; BoomOffsetBefore = Boom->SocketOffset; return; }
	Boom->TargetArmLength = ArmBefore > 0.f ? ArmBefore : CameraDistance * CmPerM;
	Boom->SocketOffset = BoomOffsetBefore;
	if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->SetControlRotation(FRotator(-12.0, GetActorRotation().Yaw, 0));
}

void ABrixCharacter::BeginPanel()
{
	bPanel = true;
	GetCharacterMovement()->StopMovementImmediately();
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = true;
		FInputModeGameAndUI Mode;
		Mode.SetHideCursorDuringCapture(false);
		PC->SetInputMode(Mode);
	}
}

void ABrixCharacter::EndPanel()
{
	bPanel = false;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
		if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();   // the mouse turns the camera again without a click
	}
}

void ABrixCharacter::BeginTalk(const FVector& Them, float CameraM)
{
	bTalking = true;
	GetCharacterMovement()->StopMovementImmediately();
	const FVector To = Them - GetActorLocation();
	const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(To.Y, To.X));
	SetActorRotation(FRotator(0, Yaw, 0));
	TalkYaw = Yaw;
	bAnswering = false;
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
	bAnswering = false;
	TalkFace = nullptr;
	if (ArmBefore > 0.f) Boom->TargetArmLength = ArmBefore;
	if (APlayerController* PC = Cast<APlayerController>(GetController()))
	{
		PC->bShowMouseCursor = false;
		PC->SetInputMode(FInputModeGameOnly());
	}
}

// ---- skateboards (CITY-098 to CITY-101; play.js boardKey(), stepBoard(), hud())

void ABrixCharacter::BoardKey()   // Y: from the hand onto the board, and back; not in a car, at a talk, a counter or the designer
{
	if (bDriving || bTalking || bPanel || bDesigning) return;
	const TOptional<bool> On = Boards.Toggle();
	ABrixLife* L = Life();
	if (!On) { if (L) L->Toast(TEXT("You have no skateboard yet. ") + L->BoardsSoldWhere() + TEXT(" sell them.")); return; }
	const FBrixBoardKind* K = Boards.Kind();
	BoardBlend = 0;
	BoardMeant = 0;
	BoardYaw = GetActorRotation().Yaw;
	BoardIn = FVector2D::ZeroVector;
	Phase = 0;
	GetCharacterMovement()->bOrientRotationToMovement = !*On;
	GetCharacterMovement()->MaxWalkSpeed = (*On ? BoardRules.Kind(BoardRules.Order.Last())->TopKmh / 3.6 + 1 : WalkSpeed) * CmPerM;
	if (L) L->Toast(*On ? K->Name + TEXT(": W to ") + (K->bPush ? TEXT("push") : TEXT("go")) + TEXT(", S to brake, Y to pick it up.") : K->Name + TEXT(" in your hand."));
}

void ABrixCharacter::SetBoards(const TArray<FString>& Own, const FString& Use)
{
	if (Boards.Riding())   // off the board first, as Y steps off it
	{
		GetCharacterMovement()->bOrientRotationToMovement = true;
		GetCharacterMovement()->MaxWalkSpeed = WalkSpeed * CmPerM;
	}
	Boards.Give(Own, Use);
}

void ABrixCharacter::TestBoard(const FString& Kind)
{
	if (!Boards.Own.Contains(Kind)) Boards.Give(BoardRules.AutoOwned, Kind);
	Boards.Use(Kind);
	if (!Boards.Riding()) BoardKey();
}

// Riding: the board's speed (boards.js) along the rider's heading. A and D turn the rider, and the camera swings round behind
// them as behind a car, more slowly the faster the board goes; W speeds it up the way they face, S brakes. Only W speeds it up:
// let go and it glides. A manual board speeds up only while a foot pushes it (W held: the push goes round), and the push stops
// the moment W is let go; the powered boards go by themselves and the rider just stands. A knock against a wall or a car
// loses the speed into it. Slower indoors (indoor_top_kmh), less so in a mall.
void ABrixCharacter::StepBoard(float Dt)
{
	const FBrixBoardKind* K = Boards.Kind();
	if (!TestHold.IsZero()) BoardIn = TestHold;
	const double F = BoardIn.Y, S = BoardIn.X;
	BoardIn = FVector2D::ZeroVector;
	// what the last frame's move came to: running into something loses the speed into it
	const FVector Here = GetActorLocation();
	if (BoardMeant > .001)
	{
		const double Went = FVector::Dist2D(Here, BoardFrom) / CmPerM;
		if (Went < BoardMeant * .7) Boards.Slow(FMath::Max(0.0, Went / BoardMeant));
	}
	const bool bGo = F > 0, bBack = F < 0;
	const double V0 = Boards.V;
	if (S != 0) BoardYaw += FMath::RadiansToDegrees(S * BoardRules.TurnRate / (1 + V0 / 15) * Dt);   // D turns right, A left
	if (APlayerController* PC = CameraFree() ? Cast<APlayerController>(GetController()) : nullptr)   // the camera comes round behind
	{
		FRotator R = PC->GetControlRotation();
		R.Yaw += FMath::FindDeltaAngleDegrees(R.Yaw, BoardYaw) * FMath::Min(1.0, Dt * BoardRules.CamFollow);
		PC->SetControlRotation(R);
	}
	const bool bPushing = K->bPush && bGo;
	BoardBlend += ((bPushing ? 1.0 : 0.0) - BoardBlend) * FMath::Min(1.0, Dt * (bPushing ? 7.0 : 14.0));   // into the push, and straight back to standing
	Phase = bPushing ? FMath::Fmod(Phase + BoardRules.PushPerS * Dt, 1.0) : 0.0;
	double Cap = -1;
	if (const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this)))
	{
		const int32 Room = Mode->RoomAt(Here);
		if (Room != INDEX_NONE)
		{
			Cap = BoardRules.IndoorTopKmh;
			int32 n = 0;
			if (const FBrixCity* C = Mode->CityData()) for (const FBrixBlock& B : C->Blocks) for (const FBrixLot& L : B.Lots) if (n++ == Room && L.Tags.Contains(TEXT("mall"))) Cap = BoardRules.MallTopKmh;
		}
	}
	const double V = Boards.Step(Dt, bGo, bBack, Cap);
	const FVector Dir = FRotator(0, BoardYaw, 0).Vector();
	GetCharacterMovement()->RequestDirectMove(Dir * V * CmPerM, false);
	BoardFrom = Here;
	BoardMeant = V * Dt;
	FRotator Me = GetActorRotation();
	Me.Yaw += FMath::FindDeltaAngleDegrees(Me.Yaw, BoardYaw) * FMath::Min(1.0, Dt * 12.0);
	SetActorRotation(Me);
	FBrixDrawOptions O;
	O.Pose = 5;
	O.Board = K;
	O.RidePush = BoardBlend;
	O.Phase = Phase;
	O.Seconds = GetWorld()->GetTimeSeconds();
	O.Lod = 0;
	Scratch.Reset();
	DrawBrixPerson(Rules, Rules.Player, FVector::ZeroVector, 0.0, O, Scratch);
	Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
	if (!BoardBar && GEngine && GEngine->GameViewport)   // which board and how fast (play.js #boardbar)
	{
		BoardBar = SNew(SBox).HAlign(HAlign_Center).VAlign(VAlign_Bottom).Padding(FMargin(0, 0, 0, 140))
			.Visibility_Lambda([this]() { return Boards.Riding() && !bPanel && !bTalking ? EVisibility::HitTestInvisible : EVisibility::Collapsed; })
			[
				SNew(SBorder).BorderImage(BrixLook::Round(FLinearColor::White, 6.f)).BorderBackgroundColor(FLinearColor::FromSRGBColor(FColor(23, 23, 28, 153))).Padding(FMargin(12, 5))
				[ SNew(STextBlock).Font(BrixLook::Font(11)).ColorAndOpacity(BrixLook::Cream()).Text_Lambda([this]()
				{
					const FBrixBoardKind* B = Boards.Kind();
					return FText::FromString(B ? FString::Printf(TEXT("%s \u00B7 %d of %d km/h"), *B->Name, (int32)FMath::RoundHalfFromZero(Boards.V * 3.6), (int32)B->TopKmh) : FString());
				}) ]
			];
		GEngine->GameViewport->AddViewportWidgetContent(BoardBar.ToSharedRef(), 16);
	}
}

// Glass travel tubes (CITY-109 to CITY-111) ----------------------------------------------------------------------------

ABrixTubes* ABrixCharacter::Tubes() const
{
	const ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	return Mode && Mode->Tubes && Mode->Tubes->IsReady() ? Mode->Tubes.Get() : nullptr;
}

FString ABrixCharacter::TubeOffer() const
{
	const ABrixTubes* Tb = Tubes();
	if (!Tb || bTubeRiding || bDriving || bSeated || IsPlaying()) return FString();
	const FString From = Tb->StopAt(FVector2D(GetActorLocation()) / CmPerM);
	if (From.IsEmpty()) return FString();
	const FString To = Tb->Choice(From, TubePickAt == From ? TubePick : 0);
	const FBrixTubeStop* S = Tb->Net().Stops.Find(To);
	return S ? FString::Printf(TEXT("Ride the tube to %s   (arrows: another stop)"), *S->Name) : FString();
}

void ABrixCharacter::TestTube(const FString& To)
{
	if (!StartTube(To)) UE_LOG(LogTemp, Warning, TEXT("Brix Town: -BrixTube: no ride to %s"), *To);
}

bool ABrixCharacter::StartTube(const FString& To)
{
	ABrixGameMode* Mode = Cast<ABrixGameMode>(UGameplayStatics::GetGameMode(this));
	if (!Mode) return false;   // only the server has the game mode; there is no network play yet
	TSharedPtr<FBrixRide> W = MakeShared<FBrixRide>();
	const FString Why = Mode->RideTube(this, bTubeRiding, To, *W);
	if (!Why.IsEmpty())
	{
		if (ABrixLife* L = Life()) L->Toast(Why == TEXT("no_way") ? TEXT("No tube goes there from here") : TEXT("You can only ride from inside a tube stop"));
		return false;
	}
	TubeRide = W;
	TubeT0 = GetWorld()->GetTimeSeconds();
	bTubeRiding = true;
	TubeBackLeft = 0;
	TubePick = 0;
	// the camera leaves the boom: from here the ride places it, starting from where it is (CITY-110: no cut)
	TubeCamRel = Camera->GetRelativeTransform();
	TubeCamFrom = Camera->GetComponentLocation();
	TubeCamFromRot = Camera->GetComponentQuat();
	Camera->SetUsingAbsoluteLocation(true);
	Camera->SetUsingAbsoluteRotation(true);
	Camera->SetWorldLocationAndRotation(TubeCamFrom, TubeCamFromRot);
	Boom->bDoCollisionTest = false;
	SetActorEnableCollision(false);
	GetCharacterMovement()->StopMovementImmediately();
	GetCharacterMovement()->DisableMovement();
	return true;
}

void ABrixCharacter::TubeTick(float DeltaSeconds)
{
	auto Smooth = [](double F) { F = FMath::Clamp(F, 0.0, 1.0); return F * F * (3 - 2 * F); };
	const double Half = .95;   // the capsule's middle over the feet, standing (as the stops and seats put the player)
	if (bTubeRiding && TubeRide)
	{
		const FBrixRide& W = *TubeRide;
		const double S = GetWorld()->GetTimeSeconds() - TubeT0;
		// the rider: feet where the ride has them, upright going up or down and lying along the level tube head first,
		// turned on their side (the owner, 2026-11-28: "the player will rotate laterally when going horizontal")
		const FVector Feet = W.At(S), Up = W.Heading(S);
		const double Lean = W.Lean(S);
		const FVector Mid = Feet + Up * Half * FMath::Cos(FMath::DegreesToRadians(Lean));
		const FVector Base = S < (W.T0 + W.T1) / 2 ? FVector(-W.FaceA, 0) : FVector(W.FaceB, 0);   // into the first booth, out of the last
		const FVector Flat = FVector(Up.X, Up.Y, 0).GetSafeNormal();
		const FVector Side = Flat.IsNearlyZero() ? Base : FVector::CrossProduct(Flat, FVector::UpVector);
		FVector Fwd = FMath::Lerp(Base, Side, FMath::Clamp(Lean / 80.0, 0.0, 1.0));
		Fwd = (Fwd - Up * FVector::DotProduct(Fwd, Up)).GetSafeNormal();
		if (Fwd.IsNearlyZero()) Fwd = FVector::CrossProduct(Up, FVector::RightVector).GetSafeNormal();
		SetActorLocationAndRotation(Mid * CmPerM, FRotationMatrix::MakeFromZX(Up, Fwd).Rotator(), false, nullptr, ETeleportType::TeleportPhysics);
		// the camera: the ride's (FBrixRide::Camera), eased in from where the everyday camera was, looking at the rider
		const double K = Smooth(S / TubeCamBlendSeconds);
		const FVector C = W.Camera(S) * CmPerM + (TubeCamFrom - W.Camera(0) * CmPerM) * (1 - K);
		const FQuat Look = FRotationMatrix::MakeFromX(Mid * CmPerM - C).ToQuat();
		Camera->SetWorldLocationAndRotation(C, FQuat::Slerp(TubeCamFromRot, Look, K));
		FBrixDrawOptions O;
		O.Seconds = GetWorld()->GetTimeSeconds();
		O.Lod = 0;
		Scratch.Reset();
		DrawBrixPerson(Rules, Rules.Player, FVector::ZeroVector, 0.0, O, Scratch);
		Verts = BrixBodyToMesh(Person, Scratch, Verts, false, BodyMaterial);
		if (S < W.Duration + W.R.FollowS) return;
		// set down in the far booth, facing out of its door, the everyday camera behind again
		bTubeRiding = false;
		const double Yaw = FMath::RadiansToDegrees(FMath::Atan2(W.FaceB.Y, W.FaceB.X));
		SetActorLocationAndRotation((W.OutB + FVector(0, 0, Half + .05)) * CmPerM, FRotator(0, Yaw, 0), false, nullptr, ETeleportType::TeleportPhysics);
		SetActorEnableCollision(true);
		GetCharacterMovement()->SetMovementMode(MOVE_Walking);
		Boom->bDoCollisionTest = true;
		const FVector Toward = (W.OutB + FVector(0, 0, Half)) * CmPerM - C;   // the everyday camera taken up from where the ride left it, not swung round
		if (APlayerController* PC = Cast<APlayerController>(GetController())) PC->SetControlRotation(FRotator(-10, FMath::RadiansToDegrees(FMath::Atan2(Toward.Y, Toward.X)), 0));
		TubeCamLast = C;
		TubeCamLastRot = Camera->GetComponentQuat();
		TubeBackLeft = TubeCamBlendSeconds;
		return;
	}
	// back to the boom: where it would put the camera, eased into from where the ride left it
	TubeBackLeft = FMath::Max(0.0, TubeBackLeft - DeltaSeconds);
	const APlayerController* PC = Cast<APlayerController>(GetController());
	const FRotator R = PC ? PC->GetControlRotation() : GetActorRotation();
	const FVector Pivot = Boom->GetComponentLocation() + Boom->TargetOffset;
	const FVector Natural = Pivot - R.Vector() * Boom->TargetArmLength + FRotationMatrix(R).TransformVector(Boom->SocketOffset) + TubeCamRel.GetLocation();
	const double F = Smooth(1 - TubeBackLeft / TubeCamBlendSeconds);
	Camera->SetWorldLocationAndRotation(FMath::Lerp(TubeCamLast, Natural, F), FQuat::Slerp(TubeCamLastRot, R.Quaternion(), F));
	if (TubeBackLeft <= 0)
	{
		Camera->SetUsingAbsoluteLocation(false);
		Camera->SetUsingAbsoluteRotation(false);
		Camera->SetRelativeTransform(TubeCamRel);
	}
}
