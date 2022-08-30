// Fill out your copyright notice in the Description page of Project Settings.


#include "LobbyGameMode.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/GameState.h"

void ALobbyGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    if(GameState)
    {
        // To access to a TObjectPtr like this variable GameState, we need to call Get() function.
        int32 NumberOfPlayer = GameState.Get()->PlayerArray.Num();
        // The key "1" (first param) replace a text with the same value on the screen
        if(GEngine){GEngine->AddOnScreenDebugMessage(1, 3, FColor::Blue, FString::Printf(TEXT("Number of players in the map : %d"), NumberOfPlayer));}

        // Get player name
        APlayerState* PlayerState = NewPlayer->GetPlayerState<APlayerState>();
        if(PlayerState)
        {
            if(GEngine){GEngine->AddOnScreenDebugMessage(1, 3, FColor::Blue, FString::Printf(TEXT("%s has joinded the game !"), *PlayerState->GetPlayerName()));}
        }
    }
}


void ALobbyGameMode::Logout(AController* Exiting)
{
    Super::Logout(Exiting);
    if(GameState)
    {
        // To access to a TObjectPtr like this variable GameState, we need to call Get() function.
        int32 NumberOfPlayer = GameState.Get()->PlayerArray.Num();
        // The key "1" (first param) replace a text with the same value on the screen
        if(GEngine){GEngine->AddOnScreenDebugMessage(1, 3, FColor::Blue, FString::Printf(TEXT("Number of players in the map : %d"), NumberOfPlayer - 1));}

        // Get player name
        APlayerState* PlayerState = Exiting->GetPlayerState<APlayerState>();
        if(PlayerState)
        {
            if(GEngine){GEngine->AddOnScreenDebugMessage(1, 3, FColor::Blue, FString::Printf(TEXT("%s has leaved the game !"), *PlayerState->GetPlayerName()));}
        }
    }
}