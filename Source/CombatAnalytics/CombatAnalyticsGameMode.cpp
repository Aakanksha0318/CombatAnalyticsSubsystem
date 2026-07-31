// Copyright Epic Games, Inc. All Rights Reserved.

#include "CombatAnalyticsGameMode.h"
#include "CombatAnalyticsCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACombatAnalyticsGameMode::ACombatAnalyticsGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
