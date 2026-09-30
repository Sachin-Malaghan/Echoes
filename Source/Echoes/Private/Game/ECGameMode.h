// ECHOES: wires the controller and HUD; there is no pawn - Subject 7 lives in the 2D sim. (CLAUDE.md: Architecture)
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "ECGameMode.generated.h"

UCLASS()
class AECGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	AECGameMode();
	virtual void RestartPlayer(AController* NewPlayer) override {}
};
