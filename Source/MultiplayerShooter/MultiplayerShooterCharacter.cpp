// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerShooterCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "Kismet/GameplayStatics.h"
#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"

//////////////////////////////////////////////////////////////////////////
// AMultiplayerShooterCharacter

AMultiplayerShooterCharacter::AMultiplayerShooterCharacter():
	// Initialise .h variables (Construct delegates which bind action functions to callback functions)
	CreateSessionCompleteDelegate(FOnCreateSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnCreateSessionComplete)),
	FindSessionsCompleteDelegate(FOnFindSessionsCompleteDelegate::CreateUObject(this, &ThisClass::OnFindSessionsComplete)),
	JoinSessionCompleteDelegate(FOnJoinSessionCompleteDelegate::CreateUObject(this, &ThisClass::OnJoinSessionComplete))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// set our turn rate for input
	TurnRateGamepad = 50.f;

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)

	// Find the Steam subsystem
	// IOnlineSubsystem* OnlineSubsystem = IOnlineSubsystem::Get();
	// if(OnlineSubsystem)
	// {
	// 	OnlineSessionInterface = OnlineSubsystem->GetSessionInterface();
	// 	if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Blue, FString::Printf(TEXT("Found subsystem : %s"), *OnlineSubsystem->GetSubsystemName().ToString()));}
	// }
}

/***
 * Called whend pressing the G Key
 ***/
void AMultiplayerShooterCharacter::CreateGameSession()
{
	if(!OnlineSessionInterface.IsValid()) // Not just 'OnlineSessionInterface' because it's a shared pointer
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("OnlineSessionInterface is Invalid.")));}
		return;
	}

	// Before to create a new session, we need to delete a session with the same name, if she exists
	FNamedOnlineSession* ExistingSession = OnlineSessionInterface->GetNamedSession(NAME_GameSession);
	if(ExistingSession)
	{
		OnlineSessionInterface->DestroySession(NAME_GameSession);
	}

	// The interface has the list of "delegates", which are the objects reacting to events and triggering callback functions
	OnlineSessionInterface->AddOnCreateSessionCompleteDelegate_Handle(CreateSessionCompleteDelegate);

	TSharedPtr<FOnlineSessionSettings> SessionSettings = MakeShareable(new FOnlineSessionSettings()); // MakeShareable allows to init SharedPointer, we give it a constructor as a parameter
	SessionSettings->bIsLANMatch = false;
	SessionSettings->NumPublicConnections = 4;
	SessionSettings->bAllowJoinInProgress = true;
	SessionSettings->bAllowJoinViaPresence = true; // Allows Steam to let players of the closest region join the server
	SessionSettings->bShouldAdvertise = true; // Allows Steam to advertise the session
	SessionSettings->bUsesPresence = true; // Allows Steam to search players which belongs to the region of the server in priority
	SessionSettings->bUseLobbiesIfAvailable = true; 
	SessionSettings->Set(FName("MatchType"), FString("FreeForAll"), EOnlineDataAdvertisementType::ViaOnlineServiceAndPing);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	OnlineSessionInterface->CreateSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, *SessionSettings);
}

/*
* OnCreateSessionComplete Callback
*/
void AMultiplayerShooterCharacter::OnCreateSessionComplete(FName SessionName, bool bWasSuccessfull)
{
	if(bWasSuccessfull)
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Green, FString::Printf(TEXT("Created session : %s"), *SessionName.ToString()));}
		UWorld* World = GetWorld();
		if(World)
		{
			World->ServerTravel(FString("/Game/ThirdPerson/Maps/Lobby?listen"));
		}
			
	}
	else
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("Failed to create session.")));}
	}

}

/*
* JoinGameSession Action
* First function call by the player when he wants to join the lobby
*/
void AMultiplayerShooterCharacter::JoinGameSession()
{
	if(!OnlineSessionInterface.IsValid())
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("OnlineSessionInterface is Invalid.")));}
		return;
	}

	// The interface has the list of "delegates", which are the objects reacting to events and triggering callback functions
	OnlineSessionInterface->AddOnFindSessionsCompleteDelegate_Handle(FindSessionsCompleteDelegate);

	//Find Game Sessions
	SessionSearch = MakeShareable(new FOnlineSessionSearch());
	SessionSearch->MaxSearchResults = 10000;
	SessionSearch->bIsLanQuery = false;
	SessionSearch->QuerySettings.Set(SEARCH_PRESENCE, true, EOnlineComparisonOp::Equals);

	const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
	OnlineSessionInterface->FindSessions(*LocalPlayer->GetPreferredUniqueNetId(), SessionSearch.ToSharedRef());
	if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("FindSessions called.")));}

}

/*
* OnFindSessionComplete() Callback binds to FindSessions()
*/
void AMultiplayerShooterCharacter::OnFindSessionsComplete(bool bWasSuccessfull)
{
	if(!OnlineSessionInterface.IsValid())
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("OnlineSessionInterface is Invalid.")));}
		return;
	}

	// if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("%b"), bWasSuccessfull));}

	if(bWasSuccessfull)
	{
		FString MatchType;
		for(auto Result : SessionSearch->SearchResults)
		{
			Result.Session.SessionSettings.Get(FName("MatchType"), MatchType); // MatchType is an OutParameter
			if (MatchType == "FreeForAll")
			{
				if(GEngine)
				{
					GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Green, FString::Printf(TEXT("My User Id : %s."), *Result.GetSessionIdStr()));
					GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Green, FString::Printf(TEXT("My User Name : %s."), *Result.Session.OwningUserName));
					GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Green, FString::Printf(TEXT("Joining Match type : %s."), *MatchType));
				}
				
				const ULocalPlayer* LocalPlayer = GetWorld()->GetFirstLocalPlayerFromController();
				OnlineSessionInterface->AddOnJoinSessionCompleteDelegate_Handle(JoinSessionCompleteDelegate);
				OnlineSessionInterface->JoinSession(*LocalPlayer->GetPreferredUniqueNetId(), NAME_GameSession, Result);

			}
		}		
	}
	else
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("No sessions found.")));}
	}
}

/*
* OnFindSessionComplete() Callback binds to JoinSession()
*/
void AMultiplayerShooterCharacter::OnJoinSessionComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result)
{
	if(!OnlineSessionInterface.IsValid())
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Red, FString::Printf(TEXT("OnlineSessionInterface is Invalid.")));}
		return;
	}

	FString Address;
	if(OnlineSessionInterface->GetResolvedConnectString(NAME_GameSession, Address))
	{
		if(GEngine){GEngine->AddOnScreenDebugMessage(-1, 10, FColor::Green, FString::Printf(TEXT("Connect String is %s."), *Address));}
	}
	
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if(PlayerController)
	{
		PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
	}
}


//////////////////////////////////////////////////////////////////////////
// Input

void AMultiplayerShooterCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up gameplay key bindings
	check(PlayerInputComponent);
	PlayerInputComponent->BindAction("Jump", IE_Pressed, this, &ACharacter::Jump);
	PlayerInputComponent->BindAction("Jump", IE_Released, this, &ACharacter::StopJumping);

	PlayerInputComponent->BindAxis("Move Forward / Backward", this, &AMultiplayerShooterCharacter::MoveForward);
	PlayerInputComponent->BindAxis("Move Right / Left", this, &AMultiplayerShooterCharacter::MoveRight);

	// We have 2 versions of the rotation bindings to handle different kinds of devices differently
	// "turn" handles devices that provide an absolute delta, such as a mouse.
	// "turnrate" is for devices that we choose to treat as a rate of change, such as an analog joystick
	PlayerInputComponent->BindAxis("Turn Right / Left Mouse", this, &APawn::AddControllerYawInput);
	PlayerInputComponent->BindAxis("Turn Right / Left Gamepad", this, &AMultiplayerShooterCharacter::TurnAtRate);
	PlayerInputComponent->BindAxis("Look Up / Down Mouse", this, &APawn::AddControllerPitchInput);
	PlayerInputComponent->BindAxis("Look Up / Down Gamepad", this, &AMultiplayerShooterCharacter::LookUpAtRate);

	// handle touch devices
	PlayerInputComponent->BindTouch(IE_Pressed, this, &AMultiplayerShooterCharacter::TouchStarted);
	PlayerInputComponent->BindTouch(IE_Released, this, &AMultiplayerShooterCharacter::TouchStopped);
}

void AMultiplayerShooterCharacter::TouchStarted(ETouchIndex::Type FingerIndex, FVector Location)
{
	Jump();
}

void AMultiplayerShooterCharacter::TouchStopped(ETouchIndex::Type FingerIndex, FVector Location)
{
	StopJumping();
}

void AMultiplayerShooterCharacter::TurnAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerYawInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMultiplayerShooterCharacter::LookUpAtRate(float Rate)
{
	// calculate delta for this frame from the rate information
	AddControllerPitchInput(Rate * TurnRateGamepad * GetWorld()->GetDeltaSeconds());
}

void AMultiplayerShooterCharacter::MoveForward(float Value)
{
	if ((Controller != nullptr) && (Value != 0.0f))
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
		AddMovementInput(Direction, Value);
	}
}

void AMultiplayerShooterCharacter::MoveRight(float Value)
{
	if ( (Controller != nullptr) && (Value != 0.0f) )
	{
		// find out which way is right
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);
	
		// get right vector 
		const FVector Direction = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		// add movement in that direction
		AddMovementInput(Direction, Value);
	}
}


void AMultiplayerShooterCharacter::OpenLobby()
{
	UWorld* World = GetWorld();
	if(World)
	{
		// Found on "Copy File Path" menu on the map 
		// E:/MyGames/MultiplayerShooter/Content/ == Game
		// "?"" is for option, which is equal to listen in this case
		World->ServerTravel("/Game/ThirdPerson/Maps/Lobby?listen");
	}
}

void AMultiplayerShooterCharacter::CallOpenLevel(const FString &Address)
{
	UGameplayStatics::OpenLevel(this, *Address);
}

void AMultiplayerShooterCharacter::CallClientTravel(const FString &Address)
{
	APlayerController* PlayerController = GetGameInstance()->GetFirstLocalPlayerController();
	if(PlayerController)
	{
		PlayerController->ClientTravel(Address, ETravelType::TRAVEL_Absolute);
	}
}
