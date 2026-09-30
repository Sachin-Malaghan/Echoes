// ECHOES: game mode. (CLAUDE.md: Architecture)
#include "Game/ECGameMode.h"

#include "Game/ECHUD.h"
#include "Game/ECPlayerController.h"

AECGameMode::AECGameMode()
{
	PlayerControllerClass = AECPlayerController::StaticClass();
	HUDClass = AECHUD::StaticClass();
	DefaultPawnClass = nullptr;
}
