// Copyright Epic Games, Inc. All Rights Reserved.

#include "MultiplayerShooterGameMode.h"
#include "MultiplayerShooterCharacter.h"
#include "UObject/ConstructorHelpers.h"

AMultiplayerShooterGameMode::AMultiplayerShooterGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
